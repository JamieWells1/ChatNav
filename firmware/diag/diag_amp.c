/* Diagnostic: amplifier and speaker only. Loops forever: every cue at mid volume,
 * then the press cue at each volume step from quiet to loud. */
#include <stdio.h>

#include "audio/cues.h"
#include "audio/speaker.h"
#include "config.h"
#include "pico/stdlib.h"

static void play_and_wait(cue_id_t id) {
    speaker_play_cue(id);
    while (speaker_cue_active() || speaker_queued() > 0) {
        speaker_poll();
        sleep_ms(2);
    }
    sleep_ms(500);
}

int main(void) {
    stdio_init_all();
    sleep_ms(3000);
    printf("[AMP] diag_amp start\n");
    if (speaker_init() != RC_OK) {
        printf("[AMP] init failed\n");
        while (true) sleep_ms(1000);
    }
    speaker_start();

    while (true) {
        printf("[AMP] pass 1: every cue at volume %d\n", VOLUME_STEPS / 2);
        speaker_set_volume(VOLUME_STEPS / 2);
        for (int id = 0; id < CUE_COUNT; id++) play_and_wait((cue_id_t)id);

        printf("[AMP] pass 2: press cue at each volume step 1..%d\n", VOLUME_STEPS - 1);
        for (int v = 1; v < VOLUME_STEPS; v++) {
            speaker_set_volume(v);
            printf("[AMP] volume %d/%d\n", v, VOLUME_STEPS - 1);
            play_and_wait(CUE_PRESS);
        }
        printf("[AMP] loop done, underruns %lu\n", (unsigned long)speaker_underruns());
        sleep_ms(2000);
    }
}
