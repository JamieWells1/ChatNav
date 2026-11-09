#ifndef WAV_H
#define WAV_H

#include <stdint.h>

#define WAV_HEADER_SIZE 44

/* 16-bit mono PCM WAV header for data_bytes of samples. */
void wav_header(uint8_t out[WAV_HEADER_SIZE], uint32_t sample_rate, uint32_t data_bytes);

#endif
