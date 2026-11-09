/*
 * Adapted from Photon src/middleware/flash_memory.c
 */

#include "storage/flash_memory.h"

#include <stdio.h>
#include <string.h>

#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/stdlib.h"

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define FLASH_LOCK_TIMEOUT_MS 1000

typedef struct {
    const uint8_t *page;
} program_args_t;

static void do_erase(void *param) {
    (void)param;
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
}

static void do_program(void *param) {
    const program_args_t *a = param;
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, a->page, FLASH_PAGE_SIZE * 4);
}

ReturnCode flash_memory_read(device_config_t *cfg) {
    if (!cfg) return RC_ERR_NULL_POINTER;
    memcpy(cfg, (const void *)(XIP_BASE + FLASH_TARGET_OFFSET), sizeof(*cfg));
    cfg_result_t r = device_config_check(cfg);
    if (r == CFG_OK) return RC_OK;
    printf("[FLASH] stored config rejected: %s\n", cfg_result_name(r));
    device_config_defaults(cfg);
    return r == CFG_ERR_CRC ? RC_FLASH_CRC_FAILED : RC_FLASH_MAGIC_INVALID;
}

ReturnCode flash_memory_write(device_config_t *cfg) {
    if (!cfg) return RC_ERR_NULL_POINTER;
    cfg_result_t v = device_config_validate(cfg);
    if (v != CFG_OK) {
        printf("[FLASH] refusing to store config: %s\n", cfg_result_name(v));
        return RC_ERR_INVALID_PARAM;
    }
    device_config_seal(cfg);

    /* Pad to whole pages; the config is far smaller than the 4 pages written. */
    _Static_assert(sizeof(device_config_t) <= FLASH_PAGE_SIZE * 4, "config exceeds 4 pages");
    uint8_t page[FLASH_PAGE_SIZE * 4];
    memset(page, 0xFF, sizeof(page));
    memcpy(page, cfg, sizeof(*cfg));

    program_args_t args = {.page = page};
    int rc = flash_safe_execute(do_program, &args, FLASH_LOCK_TIMEOUT_MS);
    if (rc != PICO_OK) {
        printf("[FLASH] write failed (flash_safe_execute rc=%d)\n", rc);
        return RC_FLASH_WRITE_FAILED;
    }

    device_config_t back;
    if (RC_FAILED(flash_memory_read(&back)) || memcmp(&back, cfg, sizeof(back)) != 0) {
        printf("[FLASH] read-back mismatch after write\n");
        return RC_FLASH_WRITE_FAILED;
    }
    printf("[FLASH] config stored (%u bytes)\n", (unsigned)sizeof(*cfg));
    return RC_OK;
}

ReturnCode flash_memory_erase(void) {
    int rc = flash_safe_execute(do_erase, NULL, FLASH_LOCK_TIMEOUT_MS);
    if (rc != PICO_OK) {
        printf("[FLASH] erase failed (rc=%d)\n", rc);
        return RC_FLASH_ERASE_FAILED;
    }
    printf("[FLASH] config erased\n");
    return RC_OK;
}
