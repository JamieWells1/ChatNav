#ifndef CONFIG_H
#define CONFIG_H

/* Pins (brief section 4.2, provisional). */
#define PIN_AMP_BCLK 0
#define PIN_AMP_LRC 1 /* must be BCLK + 1 */
#define PIN_AMP_DIN 2
#define PIN_MIC_SCK 6
#define PIN_MIC_WS 7 /* must be SCK + 1 */
#define PIN_MIC_SD 8
#define PIN_MIC_VDD 9 /* driven high to power the mic */
#define PIN_ENC_VCC 12 /* driven high to power the encoder */
#define PIN_ENC_SW 13 /* active low */
#define PIN_ENC_DT 14
#define PIN_ENC_CLK 15
#define PIN_GPS_TX 20 /* UART1 TX, to module RXD */
#define PIN_GPS_RX 21 /* UART1 RX, from module TXD */
#define GPS_BAUD 9600

/* Device tunables (plan section 14). */
#define AUDIO_SAMPLE_RATE 16000
#define MIC_GAIN_SHIFT 0 /* tune in P1 from T8 recordings */
#define UPLOAD_CHUNK_MS 64
#define MIC_WARMUP_MS 260
#define MIN_UTTERANCE_MS 400
#define MAX_UTTERANCE_S 10
#define PLAYBACK_PREBUFFER_MS 300
#define VOLUME_STEPS 16
#define VOLUME_SAVE_DELAY_S 5
#define SETUP_HOLD_S 10
#define QUERY_TIMEOUT_S 20
#define DEVICE_FIX_BUFFER 1800
#define FIX_UPLOAD_INTERVAL_S 5
#define MIN_SATS 4
#define MAX_HDOP 5.0f

#endif
