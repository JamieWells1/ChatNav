#ifndef SPEAKER_H
#define SPEAKER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "audio/cues.h"
#include "common/error.h"

/* Claims the PIO state machine and DMA channels. Output stays idle until speaker_start. */
ReturnCode speaker_init(void);

/* Starts I2S output (silence until samples are queued) / stops it and idles the pins. */
void speaker_start(void);
void speaker_stop(void);
bool speaker_running(void);

/* Queues mono 16-bit samples at AUDIO_SAMPLE_RATE. Returns how many were accepted. */
size_t speaker_write(const int16_t *mono, size_t n);
size_t speaker_free(void);

/* Samples queued but not yet played. */
size_t speaker_queued(void);

/* Volume step in [0, VOLUME_STEPS-1], applied as blocks are filled. */
void speaker_set_volume(int step);
int speaker_get_volume(void);

/* Plays a cue (replaces any cue in progress). Call speaker_poll from the main loop. */
void speaker_play_cue(cue_id_t id);
bool speaker_cue_active(void);
void speaker_poll(void);

/* Blocks that ran dry mid-block while samples were still arriving. */
uint32_t speaker_underruns(void);

#endif
