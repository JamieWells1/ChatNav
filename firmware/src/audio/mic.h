#ifndef MIC_H
#define MIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/error.h"
#include "config.h"

/* One DMA block = UPLOAD_CHUNK_MS of audio. */
#define MIC_BLOCK_FRAMES (AUDIO_SAMPLE_RATE * UPLOAD_CHUNK_MS / 1000)

/* Claims the PIO state machine and DMA channels. The mic stays unpowered. */
ReturnCode mic_init(void);

/* Powers the mic (GP9 high), starts I2S and DMA. The first MIC_WARMUP_MS of samples
 * are discarded. */
void mic_start(void);

/* Stops capture and cuts mic power. */
void mic_stop(void);

bool mic_running(void);

/* Copies the next completed block as 16-bit samples into out (which must hold
 * MIC_BLOCK_FRAMES samples). Returns the sample count, or 0 if none is ready. */
size_t mic_poll(int16_t *out, size_t max);

/* Blocks lost because mic_poll was not called in time. */
uint32_t mic_overruns(void);

#endif
