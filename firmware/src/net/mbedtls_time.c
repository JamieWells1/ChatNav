#include "mbedtls/platform_time.h"
#include "pico/time.h"

/* mbedTLS wants a millisecond clock for session timestamps; time since boot is enough. */
mbedtls_ms_time_t mbedtls_ms_time(void) {
    return (mbedtls_ms_time_t)to_ms_since_boot(get_absolute_time());
}
