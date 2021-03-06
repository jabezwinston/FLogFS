#include <Arduino.h>

#include <cstring>
#include <cassert>
#include <cstdarg>
#include <cstdio>

#include <flogfs.h>
#include <flogfs_private.h>

#include "sd_raw.h"

#include "flogfs_arduino_sd.h"

extern "C" void debugfln(const char *f, ...);

static sd_raw_t sd;
static flog_block_idx_t open_block = FLOG_BLOCK_IDX_INVALID;
static flog_block_idx_t last_block_erased = FLOG_BLOCK_IDX_INVALID;
static flog_page_index_t open_page{ 0 };
static uint16_t pages_per_block{ 0 };

#ifdef FLOGFS_VERBOSE_LOGGING
#define fslog_trace(f, ...) flash_debug_warn(f, ##__VA_ARGS__)
#else
#define fslog_trace(f, ...)
#endif

static inline uint32_t get_sd_block(flog_block_idx_t block, flog_page_index_t page, flog_sector_idx_t sector) {
    return (block * pages_per_block * FS_SECTORS_PER_PAGE_INTERNAL) +
           (page * FS_SECTORS_PER_PAGE_INTERNAL) +
           (sector);
}

static inline uint32_t get_sd_block_in_open_page(flog_sector_idx_t sector) {
    return get_sd_block(open_block, open_page, sector);
}

flog_result_t flogfs_arduino_sd_open(uint8_t cs, flog_initialize_params_t *params) {
    pages_per_block = params->pages_per_block;

    if (!sd_raw_initialize(&sd, cs)) {
        return FLOG_FAILURE;
    }

    uint32_t number_of_sd_blocks = sd_raw_card_size(&sd);
    uint32_t number_of_fs_blocks = number_of_sd_blocks / (FS_SECTORS_PER_PAGE_INTERNAL * pages_per_block);

    params->number_of_blocks = number_of_fs_blocks;
    return FLOG_RESULT(FLOG_SUCCESS);
}

flog_result_t flogfs_arduino_sd_close() {
    return FLOG_FAILURE;
}

void fs_lock_initialize(fs_lock_t *lock) {
}

void fs_lock(fs_lock_t *lock) {
}

void fs_unlock(fs_lock_t *lock) {
}

flog_result_t flash_initialize() {
    return FLOG_SUCCESS;
}

void flash_lock() {
}

void flash_unlock() {
}

flog_result_t flash_open_page(flog_block_idx_t block, flog_page_index_t page) {
    fslog_trace("flash_open_page(%d, %d)", block, page);
    open_block = block;
    open_page = page;
    if (last_block_erased != block) {
        last_block_erased = ((uint16_t)-1);
    }
    return FLOG_SUCCESS;
}

void flash_close_page() {
    fslog_trace("flash_close_page\n");
}

flog_result_t flash_erase_block(flog_block_idx_t block) {
    auto first_sd_block = get_sd_block(block, 0, 0);
    auto last_sd_block = get_sd_block(block + 1, 0, 0);
    fslog_trace("flash_erase_block(%d, %d -> %d)", block, first_sd_block, last_sd_block);
    last_block_erased = block;
    return FLOG_RESULT(sd_raw_erase(&sd, first_sd_block, last_sd_block));
}

flog_result_t flash_block_is_bad() {
    return FLOG_RESULT(0);
}

void flash_set_bad_block() {
}

void flash_commit() {
}

flog_result_t flash_read_sector(uint8_t *dst, flog_sector_idx_t sector, uint16_t offset, uint16_t n) {
    fslog_trace("flash_read_sector(%p, %d, %d, %d)", dst, sector, offset, n);
    sector = sector % FS_SECTORS_PER_PAGE;
    return FLOG_RESULT(sd_raw_read_data(&sd, get_sd_block_in_open_page(sector), offset, n, dst));
}

flog_result_t flash_read_spare(uint8_t *dst, flog_sector_idx_t sector) {
    fslog_trace("flash_read_spare(%p, %d)", dst, sector);
    sector = sector % FS_SECTORS_PER_PAGE;
    auto sd_block = get_sd_block_in_open_page(FS_SECTORS_PER_PAGE);
    return FLOG_RESULT(sd_raw_read_data(&sd, sd_block, sector * 0x10, sizeof(flog_file_sector_spare_t), dst));
}

void flash_write_sector(uint8_t const *src, flog_sector_idx_t sector, uint16_t offset, uint16_t n) {
    bool preserve_partial_write = last_block_erased != open_block;
    fslog_trace("flash_write_sector(%p, %d, %d, %d, %s)", src, sector, offset, n, preserve_partial_write ? "PRESERVE" : "IGNORE-EXISTING");
    sector = sector % FS_SECTORS_PER_PAGE;
    sd_raw_write_data(&sd, get_sd_block_in_open_page(sector), offset, n, src, preserve_partial_write);
}

void flash_write_spare(uint8_t const *src, flog_sector_idx_t sector) {
    fslog_trace("flash_write_spare(%p, %d)", src, sector);
    sector = sector % FS_SECTORS_PER_PAGE;
    auto sd_block = get_sd_block_in_open_page(FS_SECTORS_PER_PAGE);
    sd_raw_write_data(&sd, sd_block, sector * 0x10, sizeof(flog_file_sector_spare_t), src, true);
}

uint32_t flash_random(uint32_t max) {
    return random(max);
}

void flash_high_level(flog_high_level_event_t hle) {
}

void flash_debug(const char *f, va_list args) {
    constexpr uint16_t DebugLineMax = 256;
    char buffer[DebugLineMax];
    auto w = vsnprintf(buffer, DebugLineMax, f, args);
    debugfln("%s", buffer);
}

void flash_debug_warn(char const *f, ...) {
    #ifdef FLOGFS_DEBUG
    va_list args;
    va_start(args, f);
    flash_debug(f, args);
    va_end(args);
    #endif
}

void flash_debug_error(char const *f, ...) {
    va_list args;
    va_start(args, f);
    flash_debug(f, args);
    va_end(args);
}

void flash_debug_panic() {
    while (true) {
        delay(10);
    }
}
