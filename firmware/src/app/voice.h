#ifndef VOICE_H
#define VOICE_H

#include "app/app_state.h"
#include "storage/device_config.h"

/* Glue between the pure state machine and the hardware: input polling, mic, upload and
 * playback. Events come out of the poll functions as app_event_t, or -1 for none. */

#define VOICE_NO_EVENT (-1)

/* Sets up mic, speaker, playback and uplink and starts the (silent) speaker. cfg must
 * outlive the voice module; its volume_step is the starting volume. */
void voice_init(device_config_t *cfg);

/* Button and encoder, every loop pass in CONNECTING, IDLE, RECORDING, WAITING, PLAYING. */
int voice_poll_input(app_state_t s);

/* Per-state work; each may return a time- or network-driven event. */
int voice_poll(app_state_t s);

/* Carries out the ACT_* effects of a transition. */
void voice_do(unsigned actions, app_state_t from);

/* Flash save of a changed volume once the encoder has been quiet (IDLE only). */
void voice_poll_volume_save(app_state_t s);

#endif
