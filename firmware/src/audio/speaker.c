#include "audio/speaker.h"

#include <stdio.h>
#include <string.h>

#include "audio/pcm.h"
#include "audio/volume.h"
#include "config.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "i2s_out.pio.h"

#define BLOCK_FRAMES 256 /* 16 ms per DMA block */
#define RING_SIZE 8192   /* samples, power of two; 512 ms */

static PIO pio;
static uint sm;
static uint offset;
static int dma_ch[2];
static uint32_t out_buf[2][BLOCK_FRAMES];

static int16_t ring[RING_SIZE];
static volatile uint32_t head; /* producer: main loop */
static volatile uint32_t tail; /* consumer: DMA IRQ */

static volatile int volume = VOLUME_STEPS / 2;
static volatile uint32_t underruns;
static cue_gen_t cue;
static bool running;
static bool ready;

static void fill_block(int idx) {
    int16_t tmp[BLOCK_FRAMES];
    size_t got = 0;
    uint32_t t = tail;
    uint32_t h = head;
    while (got < BLOCK_FRAMES && t != h) tmp[got++] = ring[t++ % RING_SIZE];
    tail = t;
    if (got > 0 && got < BLOCK_FRAMES) underruns++;
    memset(tmp + got, 0, (BLOCK_FRAMES - got) * sizeof(tmp[0]));
    pcm_mono_to_stereo(tmp, BLOCK_FRAMES, out_buf[idx], volume);
}

static void on_dma_irq(void) {
    for (int i = 0; i < 2; i++) {
        if (dma_channel_get_irq1_status(dma_ch[i])) {
            dma_channel_acknowledge_irq1(dma_ch[i]);
            fill_block(i);
            dma_channel_set_read_addr(dma_ch[i], out_buf[i], false);
        }
    }
}

static void idle_pins(void) {
    const uint pins[] = {PIN_AMP_BCLK, PIN_AMP_LRC, PIN_AMP_DIN};
    for (int i = 0; i < 3; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
        gpio_put(pins[i], 0);
    }
}

ReturnCode speaker_init(void) {
    idle_pins();
    if (!pio_claim_free_sm_and_add_program_for_gpio_range(&i2s_out_program, &pio, &sm, &offset,
                                                          PIN_AMP_BCLK, 3, true)) {
        printf("[AMP] no free PIO state machine or program space\n");
        return RC_AUDIO_PIO_FAILED;
    }
    for (int i = 0; i < 2; i++) {
        dma_ch[i] = dma_claim_unused_channel(false);
        if (dma_ch[i] < 0) {
            printf("[AMP] no free DMA channel\n");
            return RC_AUDIO_DMA_FAILED;
        }
    }
    for (int i = 0; i < 2; i++) {
        dma_channel_config c = dma_channel_get_default_config(dma_ch[i]);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, false);
        channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
        channel_config_set_chain_to(&c, dma_ch[1 - i]);
        dma_channel_configure(dma_ch[i], &c, &pio->txf[sm], out_buf[i], BLOCK_FRAMES, false);
        dma_channel_set_irq1_enabled(dma_ch[i], true);
    }
    irq_set_exclusive_handler(DMA_IRQ_1, on_dma_irq);
    irq_set_enabled(DMA_IRQ_1, true);
    ready = true;
    printf("[AMP] ready: PIO%u SM%u, DMA %d/%d, ring %d samples, volume %d/%d\n",
           pio_get_index(pio), sm, dma_ch[0], dma_ch[1], RING_SIZE, volume, VOLUME_STEPS - 1);
    return RC_OK;
}

void speaker_start(void) {
    if (!ready || running) return;
    i2s_out_program_init(pio, sm, offset, PIN_AMP_BCLK, PIN_AMP_DIN, AUDIO_SAMPLE_RATE);
    pio_sm_clear_fifos(pio, sm);
    fill_block(0);
    fill_block(1);
    for (int i = 0; i < 2; i++) {
        dma_channel_set_read_addr(dma_ch[i], out_buf[i], false);
        dma_channel_set_trans_count(dma_ch[i], BLOCK_FRAMES, false);
    }
    dma_channel_start(dma_ch[0]);
    pio_sm_set_enabled(pio, sm, true);
    running = true;
    printf("[AMP] started at %d Hz\n", AUDIO_SAMPLE_RATE);
}

void speaker_stop(void) {
    if (!running) return;
    pio_sm_set_enabled(pio, sm, false);
    for (int i = 0; i < 2; i++) dma_channel_abort(dma_ch[i]);
    idle_pins();
    running = false;
    printf("[AMP] stopped, %u underruns\n", (unsigned)underruns);
}

bool speaker_running(void) { return running; }

size_t speaker_free(void) { return RING_SIZE - (head - tail); }

size_t speaker_queued(void) { return head - tail; }

size_t speaker_write(const int16_t *mono, size_t n) {
    size_t room = speaker_free();
    if (n > room) n = room;
    uint32_t h = head;
    for (size_t i = 0; i < n; i++) ring[h++ % RING_SIZE] = mono[i];
    head = h;
    return n;
}

void speaker_set_volume(int step) { volume = volume_clamp_step(step); }

int speaker_get_volume(void) { return volume; }

void speaker_play_cue(cue_id_t id) {
    cue_start(&cue, id);
    printf("[AMP] cue %s (%u samples) at volume %d\n", cue_name(id), (unsigned)cue.total, volume);
}

bool speaker_cue_active(void) { return cue.active; }

void speaker_poll(void) {
    int16_t tmp[128];
    while (cue.active && speaker_free() >= sizeof(tmp) / sizeof(tmp[0])) {
        size_t n = cue_next(&cue, tmp, sizeof(tmp) / sizeof(tmp[0]));
        speaker_write(tmp, n);
    }
}

uint32_t speaker_underruns(void) { return underruns; }
