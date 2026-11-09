#ifndef FLASH_MEMORY_H
#define FLASH_MEMORY_H

#include "common/error.h"
#include "storage/device_config.h"

/* Adapted from Photon flash_memory. One config struct in the last flash sector. */

/* Reads and checks the stored config. Returns RC_FLASH_MAGIC_INVALID when nothing valid is
 * stored (cfg is left as defaults), RC_FLASH_CRC_FAILED when the data is corrupt. */
ReturnCode flash_memory_read(device_config_t *cfg);

/* Validates, seals and stores cfg, then reads it back to confirm. */
ReturnCode flash_memory_write(device_config_t *cfg);

ReturnCode flash_memory_erase(void);

#endif
