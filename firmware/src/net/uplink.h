#ifndef UPLINK_H
#define UPLINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "net/http_stream.h"
#include "storage/device_config.h"

/* One voice request at a time over the kept-open link: POST /v1/query with the recording
 * streamed as UPLOAD_CHUNK_MS chunks, or GET /v1/query/last. Audio is queued as it is
 * captured and written to TLS as the send buffer allows, so the clip is never held whole.
 * The reply streams into the playback jitter buffer. cfg must outlive the uplink. */

void uplink_init(const device_config_t *cfg);

/* Starts a recording upload: clears the queue, empties the jitter buffer, binds the reply
 * parser. Nothing is sent until uplink_arm. */
void uplink_begin_query(void);

/* Queues captured samples (at most HTTP_REQ_CHUNK_MAX / 2 per call). */
void uplink_feed_pcm(const int16_t *samples, size_t n);

/* Queues the request head so sending can start. Idempotent. Called once the press is
 * past MIN_UTTERANCE_MS, so an accidental tap sends nothing. */
void uplink_arm(void);

/* Ends the request body after the last chunk. */
void uplink_finish(void);

/* Throws away queued audio. Only valid before uplink_arm sent anything. */
void uplink_discard(void);

/* U13: asks for the cached last reply. */
void uplink_begin_replay(void);

/* Forgets the current request; reopens the connection if a reply was still in flight. */
void uplink_abort(void);

/* Writes queued bytes to TLS and re-offers held reply data. Call every loop pass. */
void uplink_poll(void);

typedef struct {
    bool upload_done;     /* body ended and every byte is in the TLS send buffer */
    bool failed;          /* write error or malformed reply */
    bool headers;
    int status;
    bool audio_started;
    bool reply_done;
    uint32_t last_audio_ms;
    uint32_t dropped_chunks;
} uplink_status_t;

uplink_status_t uplink_status(void);

#endif
