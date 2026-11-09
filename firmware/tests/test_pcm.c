#include <string.h>

#include "audio/pcm.h"
#include "audio/volume.h"
#include "audio/wav.h"
#include "config.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_takes_left_word_top_16_bits(void) {
    int32_t words[6] = {0x12345600, (int32_t)0xDEADBEEF, (int32_t)0xFFFF0000, 0x7777, 0x00010000, 0};
    int16_t out[3];
    TEST_ASSERT_EQUAL_size_t(3, pcm_from_i2s32(words, 3, out, 0));
    TEST_ASSERT_EQUAL_INT16(0x1234, out[0]);
    TEST_ASSERT_EQUAL_INT16(-1, out[1]);
    TEST_ASSERT_EQUAL_INT16(1, out[2]);
}

static void test_sign_extremes(void) {
    int32_t words[4] = {(int32_t)0x80000000, 0, 0x7FFFFFFF, 0};
    int16_t out[2];
    pcm_from_i2s32(words, 2, out, 0);
    TEST_ASSERT_EQUAL_INT16(-32768, out[0]);
    TEST_ASSERT_EQUAL_INT16(32767, out[1]);
}

static void test_gain_shift_scales_and_saturates(void) {
    int32_t words[6] = {0x00100000, 0, (int32_t)0xFFF00000, 0, 0x30000000, 0};
    int16_t out[3];
    pcm_from_i2s32(words, 3, out, 4);
    TEST_ASSERT_EQUAL_INT16(256, out[0]);  /* 0x10 << 4 */
    TEST_ASSERT_EQUAL_INT16(-256, out[1]); /* -16 << 4 */
    TEST_ASSERT_EQUAL_INT16(32767, out[2]); /* 0x3000 << 4 saturates */
    pcm_from_i2s32(words, 3, out, 99);      /* shift clamps to 15 */
    TEST_ASSERT_EQUAL_INT16(32767, out[0]);
}

static void test_mono_to_stereo_duplicates_and_scales(void) {
    int16_t mono[3] = {1000, -1000, 32767};
    uint32_t st[3];
    pcm_mono_to_stereo(mono, 3, st, VOLUME_STEPS - 1);
    TEST_ASSERT_EQUAL_HEX32(0x03E803E8, st[0]);
    TEST_ASSERT_EQUAL_HEX32(0xFC18FC18, st[1]);
    TEST_ASSERT_EQUAL_HEX32(0x7FFF7FFF, st[2]);
    pcm_mono_to_stereo(mono, 3, st, 0);
    TEST_ASSERT_EQUAL_HEX32(0, st[0]);
    TEST_ASSERT_EQUAL_HEX32(0, st[1]);
}

static void test_peak_and_rms(void) {
    int16_t s[4] = {100, -300, 200, -100};
    TEST_ASSERT_EQUAL_INT16(300, pcm_peak(s, 4));
    TEST_ASSERT_EQUAL_UINT32(193, pcm_rms(s, 4));
    int16_t min = -32768;
    TEST_ASSERT_EQUAL_INT16(32767, pcm_peak(&min, 1));
    TEST_ASSERT_EQUAL_UINT32(0, pcm_rms(s, 0));
}

static void test_wav_header(void) {
    uint8_t h[WAV_HEADER_SIZE];
    wav_header(h, 16000, 160000);
    TEST_ASSERT_EQUAL_MEMORY("RIFF", h, 4);
    TEST_ASSERT_EQUAL_MEMORY("WAVEfmt ", h + 8, 8);
    TEST_ASSERT_EQUAL_MEMORY("data", h + 36, 4);
    TEST_ASSERT_EQUAL_UINT8(0x24, h[4]); /* 36 + 160000 = 0x27124 */
    TEST_ASSERT_EQUAL_UINT8(0x80, h[24]);       /* 16000 = 0x3E80 */
    TEST_ASSERT_EQUAL_UINT8(0x3E, h[25]);
    TEST_ASSERT_EQUAL_UINT8(16, h[34]);
    TEST_ASSERT_EQUAL_UINT8(0x00, h[40]);       /* 160000 = 0x27100 */
    TEST_ASSERT_EQUAL_UINT8(0x71, h[41]);
    TEST_ASSERT_EQUAL_UINT8(0x02, h[42]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_takes_left_word_top_16_bits);
    RUN_TEST(test_sign_extremes);
    RUN_TEST(test_gain_shift_scales_and_saturates);
    RUN_TEST(test_mono_to_stereo_duplicates_and_scales);
    RUN_TEST(test_peak_and_rms);
    RUN_TEST(test_wav_header);
    return UNITY_END();
}
