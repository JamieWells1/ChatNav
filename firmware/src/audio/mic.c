#include "audio/mic.h"

#include <stdio.h>

#include "audio/pcm.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "i2s_in.pio.h"

#define BLOCK_WORDS (MIC_BLOCK_FRAMES * 2) /* left + right word per frame */

static PIO pio;
static uint sm;
static uint offset;
static int dma_ch[2];
static int32_t buf[2][BLOCK_WORDS];

static volatile uint32_t blocks_done;
static uint32_t blocks_read;
static uint32_t overruns;
static uint32_t warmup_left; /* frames still to discard */
static bool running;
static bool ready;

static void on_dma_irq(void) {
    for (int i = 0; i < 2; i++) {
        if (dma_channel_get_irq0_status(dma_ch[i])) {
            dma_channel_acknowledge_irq0(dma_ch[i]);
            dma_channel_set_write_addr(dma_ch[i], buf[i], false);
            blocks_done++;
        }
    }
}

static void idle_pins(void) {
    const uint pins[] = {PIN_MIC_SCK, PIN_MIC_WS};
    for (int i = 0; i < 2; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
        gpio_put(pins[i], 0);
    }
}

ReturnCode mic_init(void) {
    gpio_init(PIN_MIC_VDD);
    gpio_set_dir(PIN_MIC_VDD, GPIO_OUT);
    gpio_put(PIN_MIC_VDD, 0);
    idle_pins();

    if (!pio_claim_free_sm_and_add_program_for_gpio_range(&i2s_in_program, &pio, &sm, &offset,
                                                          PIN_MIC_SD, PIN_MIC_WS - PIN_MIC_SD + 1,
                                                          true)) {
        printf("[MIC] no free PIO state machine or program space\n");
        return RC_AUDIO_PIO_FAILED;
    }
    for (int i = 0; i < 2; i++) {
        dma_ch[i] = dma_claim_unused_channel(false);
        if (dma_ch[i] < 0) {
            printf("[MIC] no free DMA channel\n");
            return RC_AUDIO_DMA_FAILED;
        }
    }
    for (int i = 0; i < 2; i++) {
        dma_channel_config c = dma_channel_get_default_config(dma_ch[i]);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        channel_config_set_read_increment(&c, false);
        channel_config_set_write_increment(&c, true);
        channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));
        channel_config_set_chain_to(&c, dma_ch[1 - i]);
        dma_channel_configure(dma_ch[i], &c, buf[i], &pio->rxf[sm], BLOCK_WORDS, false);
        dma_channel_set_irq0_enabled(dma_ch[i], true);
    }
    irq_set_exclusive_handler(DMA_IRQ_0, on_dma_irq);
    irq_set_enabled(DMA_IRQ_0, true);
    ready = true;
    printf("[MIC] ready: PIO%u SM%u, DMA %d/%d, %u-frame blocks, gain shift %d\n", pio_get_index(pio),
           sm, dma_ch[0], dma_ch[1], (unsigned)MIC_BLOCK_FRAMES, MIC_GAIN_SHIFT);
    return RC_OK;
}

void mic_start(void) {
    if (!ready || running) return;
    gpio_put(PIN_MIC_VDD, 1);
    blocks_done = 0;
    blocks_read = 0;
    warmup_left = (uint32_t)AUDIO_SAMPLE_RATE * MIC_WARMUP_MS / 1000;

    i2s_in_program_init(pio, sm, offset, PIN_MIC_SCK, PIN_MIC_SD, AUDIO_SAMPLE_RATE);
    pio_sm_clear_fifos(pio, sm);
    dma_channel_set_write_addr(dma_ch[0], buf[0], false);
    dma_channel_set_write_addr(dma_ch[1], buf[1], false);
    dma_channel_set_trans_count(dma_ch[0], BLOCK_WORDS, false);
    dma_channel_set_trans_count(dma_ch[1], BLOCK_WORDS, false);
    dma_channel_start(dma_ch[0]);
    pio_sm_set_enabled(pio, sm, true);
    running = true;
    printf("[MIC] started, discarding first %u frames (%d ms warm-up)\n", (unsigned)warmup_left,
           MIC_WARMUP_MS);
}

void mic_stop(void) {
    if (!running) return;
    pio_sm_set_enabled(pio, sm, false);
    for (int i = 0; i < 2; i++) dma_channel_abort(dma_ch[i]);
    idle_pins();
    gpio_put(PIN_MIC_VDD, 0);
    running = false;
    printf("[MIC] stopped, %u blocks captured, %u overruns\n", (unsigned)blocks_done,
           (unsigned)overruns);
}

bool mic_running(void) { return running; }

uint32_t mic_overruns(void) { return overruns; }

size_t mic_poll(int16_t *out, size_t max) {
    if (!running || max < MIC_BLOCK_FRAMES) return 0;
    while (blocks_read != blocks_done) {
        uint32_t lag = blocks_done - blocks_read;
        if (lag > 1) { /* the DMA has lapped the oldest unread block */
            overruns += lag - 1;
            blocks_read = blocks_done - 1;
        }
        int32_t *b = buf[blocks_read & 1];
        blocks_read++;
        size_t skip = 0;
        if (warmup_left > 0) {
            skip = warmup_left < MIC_BLOCK_FRAMES ? warmup_left : MIC_BLOCK_FRAMES;
            warmup_left -= skip;
        }
        if (skip == MIC_BLOCK_FRAMES) continue;
        return pcm_from_i2s32(b + 2 * skip, MIC_BLOCK_FRAMES - skip, out, MIC_GAIN_SHIFT);
    }
    return 0;
}
