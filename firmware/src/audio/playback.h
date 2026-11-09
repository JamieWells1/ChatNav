#ifndef PLAYBACK_H
#define PLAYBACK_H

#include <stdbool.h>

#include "audio/jitter.h"

/* Reply playback: owns the jitter buffer and attaches it to the speaker. The network
 * receiver writes into it; the speaker DMA fill reads from it. */

void playback_init(void);
jitter_t *playback_buffer(void);

/* Empties the buffer and silences the reply. Queued cues are left alone. */
void playback_reset(void);

/* True once the whole reply was received and has been played out. */
bool playback_finished(void);

#endif
