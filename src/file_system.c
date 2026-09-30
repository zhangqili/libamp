/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "file_system.h"
#include "keyboard_def.h"
#include "keyboard_config.h"
#include "driver.h"
#include <stdint.h>
#include <string.h>

/* ==========================================================================
 * Optional middle layer between the file system and the flash driver.
 *
 * The layer owns the partition as a byte address space with NOR semantics: an
 * erase resets whole blocks, a program only clears bits inside an erased
 * range, and release() is a hint that a range is no longer needed. It is
 * shared by every data-storing FILE_SYSTEM_TYPE, so the same switch selects it
 * for LFS and FileX:
 *
 *   FLASH_LAYER_DIRECT  read/program/erase go straight to the flash driver.
 *   FLASH_LAYER_LEVELX  LevelX NOR wear leveling maps 512-byte logical sectors
 *                       over the physical blocks, keeps one erase block spare
 *                       for reclaim, turns erase into a sector release and
 *                       read-modify-writes programs that cover part of a
 *                       sector.
 * ========================================================================== */
#if FILE_SYSTEM_TYPE != FILE_SYSTEM_RAW

#if FLASH_LAYER == FLASH_LAYER_LEVELX
#include "lx_api.h"
#elif FLASH_LAYER != FLASH_LAYER_DIRECT
#error "FLASH_LAYER must be FLASH_LAYER_DIRECT or FLASH_LAYER_LEVELX"
#endif

#define FLASH_LAYER_SECTOR_SIZE 512
#if FLASH_LAYER == FLASH_LAYER_LEVELX
/* FileX runs in the layer's own addressing unit. With 512-byte sectors its
 * logical sector cache holds eight entries, so the FAT sectors stay resident
 * instead of being rewritten for every data sector. Whole erase blocks would
 * multiply the LevelX churn of those rewrites eightfold. */
#define FLASH_LAYER_FILE_SYSTEM_SECTOR_SIZE FLASH_LAYER_SECTOR_SIZE
#else
/* Direct flash can only rewrite a whole erase block, so FileX uses one. */
#define FLASH_LAYER_FILE_SYSTEM_SECTOR_SIZE FS_BLOCK_SIZE
#endif
/* Physical partition the layer itself may touch. */
#define FLASH_LAYER_FLASH_BYTES ((uint64_t)FS_BLOCK_SIZE * FS_BLOCK_COUNT)

/* Logical capacity handed to the file system, in bytes. LevelX reports the real
 * number once the media is open: it keeps header sectors in every erase block
 * and one spare block for reclaim, so the physical sector count overstates what
 * the file system may use. */
static uint64_t flash_layer_capacity;

_Static_assert(FS_BLOCK_SIZE >= FLASH_LAYER_SECTOR_SIZE &&
               FS_BLOCK_SIZE % FLASH_LAYER_SECTOR_SIZE == 0,
               "flash layer blocks must be a multiple of 512 bytes");
_Static_assert(FS_PROG_SIZE > 0 && FS_BLOCK_SIZE % FS_PROG_SIZE == 0,
               "flash programming pages must divide an erase block");
_Static_assert(FS_FLASH_OFFSET % FS_BLOCK_SIZE == 0,
               "file system partition must start on an erase block boundary");
_Static_assert(FLASH_LAYER_FLASH_BYTES + FS_FLASH_OFFSET <= UINT32_MAX,
               "file system partition exceeds the flash driver address range");
#if FLASH_LAYER == FLASH_LAYER_LEVELX
_Static_assert(FS_BLOCK_COUNT >= 3, "LevelX requires spare erase blocks");
#endif

/* Each backend uses a subset of the layer, so several entry points stay unused
 * in any single configuration. */
#if defined(__GNUC__)
#define FLASH_LAYER_UNUSED __attribute__((unused))
#else
#define FLASH_LAYER_UNUSED
#endif

/* -------------------------------------------------------------------------
 * LevelX implementation
 * ------------------------------------------------------------------------- */
#if FLASH_LAYER == FLASH_LAYER_LEVELX

/* LevelX uses ULONG words in its on-flash metadata. Test this layer with -m32
 * on LP64 hosts so that host images match the embedded target. */
_Static_assert(sizeof(ULONG) == 4, "the LevelX layer requires 32-bit ULONG");

static LX_NOR_FLASH flash_layer_nor;
static ULONG flash_layer_nor_buffer[FLASH_LAYER_SECTOR_SIZE / sizeof(ULONG)];
/* Staging sector for read-modify-write programs; ULONG-aligned for LevelX. */
static ULONG flash_layer_sector[FLASH_LAYER_SECTOR_SIZE / sizeof(ULONG)];
static bool flash_layer_open;
static bool flash_layer_initialized;

/* LevelX performs pointer arithmetic but all accesses go through callbacks.
 * Use a nonzero token base, never a memory-mapped flash address. */
#define FLASH_LAYER_TOKEN_BASE ((uintptr_t)0x1000)
_Static_assert(FLASH_LAYER_FLASH_BYTES <= UINTPTR_MAX - FLASH_LAYER_TOKEN_BASE,
               "LevelX address tokens overflow uintptr_t");

static bool flash_layer_range(ULONG *address, ULONG words, uint32_t *offset, uint32_t *bytes)
{
    uintptr_t token = (uintptr_t)address;
    uint64_t length = (uint64_t)words * sizeof(ULONG);
    if (token < FLASH_LAYER_TOKEN_BASE || token - FLASH_LAYER_TOKEN_BASE > FLASH_LAYER_FLASH_BYTES ||
        length > FLASH_LAYER_FLASH_BYTES - (token - FLASH_LAYER_TOKEN_BASE))
        return false;
    *offset = FS_FLASH_OFFSET + (uint32_t)(token - FLASH_LAYER_TOKEN_BASE);
    *bytes = (uint32_t)length;
    return true;
}

static UINT flash_layer_nor_read(ULONG *address, ULONG *destination, ULONG words)
{
    uint32_t offset, bytes;
    if (!flash_layer_range(address, words, &offset, &bytes))
        return LX_ERROR;
    return flash_read(offset, bytes, (uint8_t *)destination) == 0 ? LX_SUCCESS : LX_ERROR;
}

static UINT flash_layer_nor_write(ULONG *address, ULONG *source, ULONG words)
{
    uint32_t offset, bytes;
    const uint8_t *data = (const uint8_t *)source;
    if (!flash_layer_range(address, words, &offset, &bytes))
        return LX_ERROR;
    while (bytes) {
        uint32_t chunk = FS_PROG_SIZE - offset % FS_PROG_SIZE;
        if (chunk > bytes) chunk = bytes;
        if (flash_write(offset, chunk, data) != 0) return LX_ERROR;
        offset += chunk;
        data += chunk;
        bytes -= chunk;
    }
    return LX_SUCCESS;
}

static UINT flash_layer_nor_erase(ULONG block, ULONG erase_count)
{
    (void)erase_count;
    if (block >= FS_BLOCK_COUNT) return LX_ERROR;
    return flash_erase(FS_FLASH_OFFSET + block * FS_BLOCK_SIZE,
                       FS_BLOCK_SIZE) == 0 ? LX_SUCCESS : LX_ERROR;
}

static UINT flash_layer_nor_erased(ULONG block)
{
    uint8_t buffer[64];
    if (block >= FS_BLOCK_COUNT) return LX_ERROR;
    for (uint32_t i = 0; i < FS_BLOCK_SIZE; i += sizeof(buffer)) {
        if (flash_read(FS_FLASH_OFFSET + block * FS_BLOCK_SIZE + i,
                       sizeof(buffer), buffer) != 0) return LX_ERROR;
        for (size_t j = 0; j < sizeof(buffer); ++j)
            if (buffer[j] != 0xff) return LX_ERROR;
    }
    return LX_SUCCESS;
}

static UINT flash_layer_nor_setup(LX_NOR_FLASH *nor)
{
    nor->lx_nor_flash_base_address = (ULONG *)FLASH_LAYER_TOKEN_BASE;
    nor->lx_nor_flash_total_blocks = FS_BLOCK_COUNT;
    nor->lx_nor_flash_words_per_block = FS_BLOCK_SIZE / sizeof(ULONG);
    nor->lx_nor_flash_driver_read = flash_layer_nor_read;
    nor->lx_nor_flash_driver_write = flash_layer_nor_write;
    nor->lx_nor_flash_driver_block_erase = flash_layer_nor_erase;
    nor->lx_nor_flash_driver_block_erased_verify = flash_layer_nor_erased;
    nor->lx_nor_flash_sector_buffer = flash_layer_nor_buffer;
    return LX_SUCCESS;
}

static int flash_layer_init(void)
{
    if (!flash_layer_initialized) {
        lx_nor_flash_initialize();
        flash_layer_initialized = true;
    }
    if (flash_layer_open) return 0;
    UINT status = lx_nor_flash_open(&flash_layer_nor, "libamp NOR", flash_layer_nor_setup);
    if (status == LX_SUCCESS) {
        flash_layer_open = true;
        /* LevelX holds back one erase block for reclaim and stores header
         * sectors in every block, so only it knows the usable capacity. */
        flash_layer_capacity =
            ((uint64_t)flash_layer_nor.lx_nor_flash_total_physical_sectors -
             flash_layer_nor.lx_nor_flash_physical_sectors_per_block) * FLASH_LAYER_SECTOR_SIZE;
    }
    return status == LX_SUCCESS ? 0 : -1;
}

static FLASH_LAYER_UNUSED int flash_layer_deinit(void)
{
    if (!flash_layer_open) return 0;
    UINT status = lx_nor_flash_close(&flash_layer_nor);
    if (status == LX_SUCCESS) flash_layer_open = false;
    return status == LX_SUCCESS ? 0 : -1;
}

static int flash_layer_read(uint32_t offset, uint32_t size, uint8_t *data)
{
    while (size) {
        uint32_t in_sector = offset % FLASH_LAYER_SECTOR_SIZE;
        uint32_t chunk = FLASH_LAYER_SECTOR_SIZE - in_sector;
        if (chunk > size) chunk = size;
        UINT status = lx_nor_flash_sector_read(&flash_layer_nor,
                                               offset / FLASH_LAYER_SECTOR_SIZE,
                                               flash_layer_sector);
        if (status == LX_SECTOR_NOT_FOUND) {
            /* Released sector that LevelX could not materialize: erased. */
            memset(flash_layer_sector, 0xff, sizeof(flash_layer_sector));
        } else if (status != LX_SUCCESS) {
            return -1;
        }
        memcpy(data, (uint8_t *)flash_layer_sector + in_sector, chunk);
        offset += chunk;
        data += chunk;
        size -= chunk;
    }
    return 0;
}

static int flash_layer_program(uint32_t offset, uint32_t size, const uint8_t *data)
{
    while (size) {
        uint32_t in_sector = offset % FLASH_LAYER_SECTOR_SIZE;
        uint32_t chunk = FLASH_LAYER_SECTOR_SIZE - in_sector;
        if (chunk > size) chunk = size;
        if (chunk != FLASH_LAYER_SECTOR_SIZE) {
            /* LevelX writes whole sectors, so keep the bytes outside the range. */
            UINT status = lx_nor_flash_sector_read(&flash_layer_nor,
                                                   offset / FLASH_LAYER_SECTOR_SIZE,
                                                   flash_layer_sector);
            if (status == LX_SECTOR_NOT_FOUND)
                memset(flash_layer_sector, 0xff, sizeof(flash_layer_sector));
            else if (status != LX_SUCCESS)
                return -1;
        }
        memcpy((uint8_t *)flash_layer_sector + in_sector, data, chunk);
        if (lx_nor_flash_sector_write(&flash_layer_nor, offset / FLASH_LAYER_SECTOR_SIZE,
                                      flash_layer_sector) != LX_SUCCESS)
            return -1;
        offset += chunk;
        data += chunk;
        size -= chunk;
    }
    return 0;
}

/* Erasing drops the LevelX mapping of the covered sectors: they read back as
 * 0xFF and their physical space becomes reclaimable. */
static FLASH_LAYER_UNUSED int flash_layer_erase(uint32_t offset, uint32_t size)
{
    if (offset % FLASH_LAYER_SECTOR_SIZE || size % FLASH_LAYER_SECTOR_SIZE) return -1;
    for (uint32_t i = 0; i < size; i += FLASH_LAYER_SECTOR_SIZE) {
        UINT status = lx_nor_flash_sector_release(&flash_layer_nor,
                                                  (offset + i) / FLASH_LAYER_SECTOR_SIZE);
        if (status != LX_SUCCESS && status != LX_SECTOR_NOT_FOUND) return -1;
    }
    return 0;
}

static FLASH_LAYER_UNUSED int flash_layer_release(uint32_t offset, uint32_t size)
{
    return flash_layer_erase(offset, size);
}

/* Make the partition usable by LevelX again, whatever wrote it last: LevelX's
 * own entry point erases every block that is not already erased and writes the
 * erase counts. LevelX can only append to erased space, so a partition holding
 * foreign data (LittleFS, direct-layer FAT) must be reset through it before
 * fx_media_format() writes. Reopening afterwards rebuilds the geometry, which
 * format() itself does not do. */
static FLASH_LAYER_UNUSED int flash_layer_format(void)
{
    (void)flash_layer_deinit();
    if (lx_nor_flash_format(&flash_layer_nor, "libamp NOR", flash_layer_nor_setup, LX_NULL) != LX_SUCCESS)
        return -1;
    return flash_layer_init();
}

/* -------------------------------------------------------------------------
 * Direct implementation: the partition is used exactly as configured
 * ------------------------------------------------------------------------- */
#else

static FLASH_LAYER_UNUSED int flash_layer_init(void)
{
    flash_layer_capacity = FLASH_LAYER_FLASH_BYTES;
    return 0;
}

static FLASH_LAYER_UNUSED int flash_layer_deinit(void)
{
    return 0;
}

static int flash_layer_read(uint32_t offset, uint32_t size, uint8_t *data)
{
    return flash_read(FS_FLASH_OFFSET + offset, size, data);
}

static int flash_layer_program(uint32_t offset, uint32_t size, const uint8_t *data)
{
    uint32_t address = FS_FLASH_OFFSET + offset;
    while (size) {
        uint32_t chunk = FS_PROG_SIZE - address % FS_PROG_SIZE;
        if (chunk > size) chunk = size;
        if (flash_write(address, chunk, data) != 0) return -1;
        address += chunk;
        data += chunk;
        size -= chunk;
    }
    return 0;
}

static FLASH_LAYER_UNUSED int flash_layer_erase(uint32_t offset, uint32_t size)
{
    if (offset % FS_BLOCK_SIZE || size % FS_BLOCK_SIZE) return -1;
    return flash_erase(FS_FLASH_OFFSET + offset, size);
}

static FLASH_LAYER_UNUSED int flash_layer_release(uint32_t offset, uint32_t size)
{
    /* Nothing to reclaim: only the caller's bookkeeping changes. */
    (void)offset;
    (void)size;
    return 0;
}

static int flash_layer_block_erased(uint32_t block)
{
    uint8_t buffer[64];
    uint32_t base = FS_FLASH_OFFSET + block * FS_BLOCK_SIZE;
    for (uint32_t i = 0; i < FS_BLOCK_SIZE; i += sizeof(buffer)) {
        if (flash_read(base + i, sizeof(buffer), buffer) != 0) return -1;
        for (size_t j = 0; j < sizeof(buffer); ++j)
            if (buffer[j] != 0xff) return -1;
    }
    return 0;
}

static FLASH_LAYER_UNUSED int flash_layer_reset(void)
{
    for (uint32_t block = 0; block < FS_BLOCK_COUNT; ++block) {
        if (flash_layer_erase(block * FS_BLOCK_SIZE, FS_BLOCK_SIZE) != 0 ||
            flash_layer_block_erased(block) != 0) return -1;
    }
    return 0;
}
#endif

/* Replace the contents of an already block-aligned range. Direct flash has to
 * erase first; LevelX rewrites sector by sector, which leaves the previous
 * mapping valid until the new contents are programmed. */
static FLASH_LAYER_UNUSED int flash_layer_rewrite(uint32_t offset, uint32_t size,
                                                 const uint8_t *data)
{
#if FLASH_LAYER == FLASH_LAYER_DIRECT
    if (offset % FS_BLOCK_SIZE || size % FS_BLOCK_SIZE) return -1;
    if (flash_layer_erase(offset, size) != 0) return -1;
#endif
    return flash_layer_program(offset, size, data);
}

#endif /* FILE_SYSTEM_TYPE != FILE_SYSTEM_RAW */

#if FILE_SYSTEM_TYPE == FILE_SYSTEM_RAW
void fs_init_dir(void)
{

}

int fs_init(void)
{
    return -1;
}

int fs_format(void)
{
    return -1;
}

int fs_open(File * file, const char * name, size_t flags)
{
    UNUSED(file);
    UNUSED(name);
    UNUSED(flags);
    return -1;
}

int fs_close(File * file)
{
    UNUSED(file);
    return -1;
}

int fs_unlink(const char * name)
{
    UNUSED(name);
    return -1;
}

int fs_rename(const char * old, const char * new)
{
    UNUSED(old);
    UNUSED(new);
    return -1;
}

size_t fs_read(File *file, void *ptr, size_t size)
{
    UNUSED(ptr);
    UNUSED(size);
    UNUSED(file);
    return 0;
}

size_t fs_write(File *file, void *ptr, size_t size)
{
    UNUSED(ptr);
    UNUSED(size);
    UNUSED(file);
    return 0;
}

int fs_seek(File *file,  FilePosition offset, int whence)
{
    UNUSED(file);
    UNUSED(offset);
    UNUSED(whence);
    return -1;
}

FilePosition fs_tell(File * file)
{
    UNUSED(file);
    return -1L;
}

FilePosition fs_size(File * file)
{
    UNUSED(file);
    return -1L;
}

int fs_truncate(File * file, FilePosition size)
{
    UNUSED(file);
    UNUSED(size);
    return -1;
}

int fs_sync(File *file)
{
    UNUSED(file);
    return -1;
}

int fs_stat(const char * file, FileStat*buf)
{
    UNUSED(file);
    UNUSED(buf);
    return -1;
}

int fs_statfs(const char * path, FileSystemStat*buf)
{
    UNUSED(path);
    UNUSED(buf);
    return -1;
}

int fs_statvfs(const char * file, VolumeStat*buf)
{
    UNUSED(file);
    UNUSED(buf);
    return -1;
}

int fs_mkdir(const char * path, size_t mode)
{
    UNUSED(path);
    UNUSED(mode);
    return -1;
}

int fs_rmdir(const char * path)
{
    UNUSED(path);
    return -1;
}

int fs_opendir(Directory *dir, const char *name)
{
    UNUSED(dir);
    UNUSED(name);
    return -1;
}

int fs_readdir(Directory *dir, FileStat *buf)
{
    UNUSED(dir);
    UNUSED(buf);
    return -1;
}

int fs_telldir(Directory *dir)
{
    UNUSED(dir);
    return -1;
}

int fs_seekdir(Directory *dir, FilePosition pos)
{
    UNUSED(dir);
    UNUSED(pos);
    return -1;
}

int fs_rewinddir(Directory *dir)
{
    UNUSED(dir);
    return -1;
}

int fs_closedir(Directory *dir)
{
    UNUSED(dir);
    return -1;
}

#endif

#if FILE_SYSTEM_TYPE == FILE_SYSTEM_LFS

#ifndef FS_READ_SIZE
#error "FS_READ_SIZE is not defined."
#endif
#ifndef FS_PROG_SIZE
#error "FS_PROG_SIZE is not defined."
#endif
#ifndef FS_BLOCK_SIZE
#error "FS_BLOCK_SIZE is not defined."
#endif
#ifndef FS_BLOCK_COUNT
#error "FS_BLOCK_COUNT is not defined."
#endif
#ifndef FS_CACHE_SIZE
#define FS_CACHE_SIZE 16
#endif
#ifndef FS_LOOKAHEAD_SIZE
#error "FS_READ_SIZE is not defined."
#endif
#ifndef LFS_BLOCK_CYCLES
#error "FS_READ_SIZE is not defined."
#endif

static uint8_t read_buffer[FS_CACHE_SIZE];
static uint8_t prog_buffer[FS_CACHE_SIZE];
static uint8_t lookahead_buffer[FS_CACHE_SIZE];
lfs_t _lfs;

static int lfs_flash_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    return flash_layer_read((uint32_t)(c->block_size * block + off), (uint32_t)size, (uint8_t *)buffer);
}

static int lfs_flash_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    return flash_layer_program((uint32_t)(c->block_size * block + off), (uint32_t)size, (const uint8_t *)buffer);
}

static int lfs_flash_erase(const struct lfs_config *c, lfs_block_t block)
{
    return flash_layer_erase((uint32_t)(block * c->block_size), (uint32_t)c->block_size);
}

static int _sync(const struct lfs_config *c)
{
    UNUSED(c);
    return LFS_ERR_OK;
}
/* block_count is filled in from the flash layer in fs_init(): wear leveling
 * hides part of the partition, so the usable size is only known at runtime. */
static struct lfs_config _lfs_config =
{
    // block device operations
    .read  = lfs_flash_read,
    .prog  = lfs_flash_prog,
    .erase = lfs_flash_erase,
    .sync  = _sync,

    // block device configuration
    .read_size = FS_READ_SIZE,
    .prog_size = FS_PROG_SIZE,
    .block_size = FS_BLOCK_SIZE,
    .block_count = 0,

    .cache_size = FS_CACHE_SIZE,
    .lookahead_size = FS_LOOKAHEAD_SIZE,
    .block_cycles = LFS_BLOCK_CYCLES,

    .read_buffer = read_buffer,
    .prog_buffer = prog_buffer,
    .lookahead_buffer = lookahead_buffer,
};

void fs_init_dir(void)
{
    lfs_mkdir(&_lfs, "profiles");
    lfs_mkdir(&_lfs, "system");
    lfs_mkdir(&_lfs, "scripts");
}

int fs_init(void)
{
    if (flash_layer_init() != 0) {
        /* The layer could not read the media, rebuild the partition once. LevelX
         * resets its own block headers and mapping (flash_layer_format()); the
         * direct layer has no metadata to reset, so it just erases the range. */
#if FLASH_LAYER == FLASH_LAYER_LEVELX
        if (flash_layer_format() != 0)
            return -1;
#else
        if (flash_layer_reset() != 0)
            return -1;
#endif
    }
    _lfs_config.block_count = (lfs_size_t)(flash_layer_capacity / FS_BLOCK_SIZE);
    // mount the filesystem
    int err = lfs_mount(&_lfs, &_lfs_config);
    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err)
    {
        lfs_format(&_lfs, &_lfs_config);
        err = lfs_mount(&_lfs, &_lfs_config);
    }
    fs_init_dir();
    return err;
}

int fs_format(void)
{
    (void)lfs_unmount(&_lfs);
    if (flash_layer_init() != 0)
        return -1;
    _lfs_config.block_count = (lfs_size_t)(flash_layer_capacity / FS_BLOCK_SIZE);
    return lfs_format(&_lfs, &_lfs_config);
}

int fs_open(File * file, const char * name, size_t flags)
{
    if (file == NULL || name == NULL)
        return -1;

    int err = lfs_file_open(&_lfs, file, name, (int)flags);

    return err;
}

int fs_close(File * file)
{
    if (file == NULL)
        return -1;

    int err = lfs_file_close(&_lfs, file);

    return err;
}

int fs_unlink(const char * name)
{
    if (name == NULL)
        return -1;

    return lfs_remove(&_lfs, name);
}

int fs_rename(const char * old, const char * new)
{
    if (old == NULL || new == NULL)
        return -1;

    return lfs_rename(&_lfs, old, new);
}

size_t fs_read(File *file, void *ptr, size_t size)
{
    if (ptr == NULL || file == NULL)
        return -1;
    lfs_ssize_t res = lfs_file_read(&_lfs, file, ptr, size);

    if (res < 0)
    {
        return 0;
    }

    return (size_t)(res);
}

size_t fs_write(File *file, void *ptr, size_t size)
{
    if (ptr == NULL || file == NULL)
        return 0;

    lfs_ssize_t res = lfs_file_write(&_lfs, file, ptr, size);

    if (res < 0)
    {
        return 0;
    }

    return (size_t)(res);
}

int fs_seek(File *file,  FilePosition offset, int whence)
{
    if (file == NULL)
        return -1;

    lfs_soff_t res = lfs_file_seek(&_lfs, file, (lfs_soff_t)offset, whence);

    if (res < 0)
    {
        return (int)res;
    }

    return 0;
}

FilePosition fs_tell(File * file)
{
    if (file == NULL)
        return -1L;

    FilePosition res = lfs_file_tell(&_lfs, file);

    if (res < 0)
    {
        return -1L;
    }
    return (FilePosition)res;
}

FilePosition fs_size(File * file)
{
    if (file == NULL)
        return -1L;

    lfs_soff_t res = lfs_file_size(&_lfs, file);

    if (res < 0)
    {
        return -1L;
    }
    return (long)res;
}

int fs_truncate(File * file, FilePosition size)
{
    if (file == NULL)
        return -1;

    int res = lfs_file_truncate(&_lfs, file, (lfs_soff_t)size);

    return res;
}

int fs_sync(File *file)
{
    if (file == NULL)
        return -1;

    int res = lfs_file_sync(&_lfs, file);

    return res;
}

int fs_stat(const char * file, FileStat*buf)
{
    if (file == NULL || buf == NULL)
        return -1;

    int res = lfs_stat(&_lfs, file, buf);

    return res;
}

int fs_statfs(const char * path, FileSystemStat*buf)
{
    UNUSED(path);
    if (path == NULL || buf == NULL)
        return -1;

    int res = lfs_fs_stat(&_lfs, buf);

    return res;
}

int fs_statvfs(const char * file, VolumeStat*buf)
{
    if (file == NULL || buf == NULL)
        return -1;

    FileSystemStat fs_stat;
    int res = lfs_fs_stat(&_lfs, &fs_stat);

    if (res < 0)
    {
        return res;
    }

    buf->f_bsize = fs_stat.block_size;
    buf->f_blocks = fs_stat.block_count;
    buf->f_frsize = fs_stat.block_size;
    lfs_ssize_t allocated_blocks = lfs_fs_size(&_lfs);
    if (allocated_blocks >= 0) {
        buf->f_bfree = buf->f_blocks - allocated_blocks;
    } else {
        buf->f_bfree = 0;
    }
    return 0;
}

int fs_mkdir(const char * path, size_t mode)
{
    if (path == NULL)
        return -1;
    UNUSED(mode);
    int res = lfs_mkdir(&_lfs, path);

    return res;
}

int fs_rmdir(const char * path)
{
    if (path == NULL)
        return -1;

    int res = lfs_remove(&_lfs, path);

    return res;
}

int fs_opendir(Directory *dir, const char *name)
{
    if (dir == NULL || name == NULL)
        return -1;

    int res = lfs_dir_open(&_lfs, dir, name);

    return res;
}

int fs_readdir(Directory *dir, FileStat *buf)
{
    if (dir == NULL || buf == NULL)
        return -1;

    int res = lfs_dir_read(&_lfs, dir, buf);

    return res;
}

int fs_telldir(Directory *dir)
{
    if (dir == NULL)
        return -1;

    int res = lfs_dir_tell(&_lfs, dir);

    return res;
}

int fs_seekdir(Directory *dir, FilePosition pos)
{
    if (dir == NULL)
        return -1;

    int res = lfs_dir_seek(&_lfs, dir, pos);

    return res;
}

int fs_rewinddir(Directory *dir)
{
    if (dir == NULL)
        return -1;

    int res = lfs_dir_rewind(&_lfs, dir);

    return res;
}

int fs_closedir(Directory *dir)
{
    if (dir == NULL)
        return -1;

    int res = lfs_dir_close(&_lfs, dir);

    return res;
}
#endif

#if FILE_SYSTEM_TYPE == FILE_SYSTEM_FILEX
#include <limits.h>
#include <stdint.h>
#include <string.h>

/* FileX uses the flash layer's FileX sector size, which is a whole erase block
 * on direct flash and the layer's 512-byte unit under LevelX, so the driver
 * never needs a read-modify-write path of its own. */
#define FS_FILEX_SECTOR_SIZE FLASH_LAYER_FILE_SYSTEM_SECTOR_SIZE
_Static_assert(FS_FILEX_SECTOR_SIZE >= 512 && FS_FILEX_SECTOR_SIZE <= 4096 &&
               FS_FILEX_SECTOR_SIZE % 512 == 0 &&
               FS_BLOCK_SIZE % FS_FILEX_SECTOR_SIZE == 0,
               "FileX sectors must be 512..4096 bytes, a multiple of 512, dividing an erase block");

/* FS_O_* is libamp's portable open interface and deliberately keeps the
 * LittleFS bit layout in every backend. FileX is different: the access mode is
 * one FX_OPEN_FOR_* value (not a bitmask) and create/exclusive/truncate/append
 * have no flag at all, so fs_open() extracts the access mode from the two low
 * FS_O_* bits and performs the remaining flags through dedicated FileX calls.
 * The FileX values must never be aliased onto FS_O_*: FX_OPEN_FOR_READ is 0
 * (no access mode at all) and FX_OPEN_FOR_READ_FAST is 2, which collides with
 * FS_O_WRONLY. */
#define FS_MODE_MASK (FS_O_RDONLY | FS_O_WRONLY)
_Static_assert(FS_O_RDWR == FS_MODE_MASK,
               "fs_open() treats FS_O_RDWR as both access-mode bits");
_Static_assert((FS_O_CREAT | FS_O_EXCL | FS_O_TRUNC | FS_O_APPEND) > FS_MODE_MASK,
               "control flags must not overlap the access-mode bits");
_Static_assert(FS_O_RDONLY != FX_OPEN_FOR_READ && FS_O_WRONLY != FX_OPEN_FOR_WRITE,
               "FS_O_* must stay a portable bitmask, not FileX open types");

/* Map the portable access mode onto FileX's single open type. */
static UINT fs_filex_open_type(size_t mode)
{
    return mode == FS_O_RDONLY ? FX_OPEN_FOR_READ : FX_OPEN_FOR_WRITE;
}

static FX_MEDIA fs_media;
static ULONG fs_media_buffer[4096 / sizeof(ULONG)];
#ifdef FX_ENABLE_FAULT_TOLERANT
#include <stdio.h>
/* Journal scratch for fault tolerance, armed by fs_init(). FileX wants at least
 * one FileX sector (4096B on the direct layer, 512B under LevelX) and
 * fx_fault_tolerant.h documents 3072B as the smallest useful size, so take the
 * larger. Compiled out entirely when the option is off. */
#ifndef FX_FAULT_TOLERANT_MINIMAL_BUFFER_SIZE
#define FX_FAULT_TOLERANT_MINIMAL_BUFFER_SIZE 3072U
#endif
#define FILEX_FAULT_TOLERANT_BUFFER_SIZE \
    (((FX_FAULT_TOLERANT_MINIMAL_BUFFER_SIZE) > (FS_FILEX_SECTOR_SIZE)) \
         ? (FX_FAULT_TOLERANT_MINIMAL_BUFFER_SIZE) : (FS_FILEX_SECTOR_SIZE))
static UCHAR fs_fault_tolerant_buffer[FILEX_FAULT_TOLERANT_BUFFER_SIZE];
#endif /* FX_ENABLE_FAULT_TOLERANT */
static bool fs_libraries_initialized;
static bool fs_media_ready;
static uint32_t fs_generation;

static int fs_result(UINT status)
{
    return status == FX_SUCCESS ? 0 : -(int)status;
}


static void fs_media_driver(FX_MEDIA *media)
{
    UINT status = FX_SUCCESS;
    ULONG sector = media->fx_media_driver_logical_sector;
    ULONG count = media->fx_media_driver_sectors;
    UCHAR *buffer = media->fx_media_driver_buffer;
    UINT request = media->fx_media_driver_request;
    media->fx_media_driver_status = FX_IO_ERROR;
    switch (request) {
    case FX_DRIVER_INIT:
        /* Only a wear leveling layer can reclaim the sectors FileX reports as
         * free; without one the hint would be a no-op. */
#if FLASH_LAYER == FLASH_LAYER_LEVELX
        media->fx_media_driver_free_sector_update = FX_TRUE;
#else
        media->fx_media_driver_free_sector_update = FX_FALSE;
#endif
        media->fx_media_driver_write_protect = FX_FALSE;
        fs_media_ready = flash_layer_init() == 0;
        status = fs_media_ready ? FX_SUCCESS : FX_IO_ERROR;
        break;
    case FX_DRIVER_UNINIT:
    case FX_DRIVER_ABORT:
        status = flash_layer_deinit() == 0 ? FX_SUCCESS : FX_IO_ERROR;
        fs_media_ready = false;
        break;
    case FX_DRIVER_FLUSH:
        /* flash_write/erase are synchronous; there is no driver write cache. */
        break;
    case FX_DRIVER_BOOT_READ:
    case FX_DRIVER_BOOT_WRITE:
    case FX_DRIVER_READ:
    case FX_DRIVER_WRITE:
    case FX_DRIVER_RELEASE_SECTORS:
        if (!fs_media_ready) return;
        if (request == FX_DRIVER_BOOT_READ || request == FX_DRIVER_BOOT_WRITE) {
            sector = 0;
            count = 1;
        }
        /* The layer rewrites whole blocks, so a volume in any other geometry
         * fails the mount and fs_init() reformats the partition. The boot
         * sector is requested before the BPB is parsed, hence the exception. */
        if (request != FX_DRIVER_BOOT_READ &&
            media->fx_media_bytes_per_sector != FS_FILEX_SECTOR_SIZE) return;
        ULONG total = (ULONG)(flash_layer_capacity / FS_FILEX_SECTOR_SIZE);
        if (sector > total || count > total - sector) return;
        uint32_t offset = (uint32_t)(sector * FS_FILEX_SECTOR_SIZE);
        uint32_t size = (uint32_t)(count * FS_FILEX_SECTOR_SIZE);
        int result;
        if (request == FX_DRIVER_RELEASE_SECTORS)
            result = flash_layer_release(offset, size);
        else if (request == FX_DRIVER_READ || request == FX_DRIVER_BOOT_READ)
            result = flash_layer_read(offset, size, buffer);
        else
            result = flash_layer_rewrite(offset, size, buffer);
        status = result == 0 ? FX_SUCCESS : FX_IO_ERROR;
        break;
    default:
        return;
    }
    if (status == FX_SUCCESS) media->fx_media_driver_status = FX_SUCCESS;
}

static UINT fs_make_directories(void)
{
    static const char *const paths[] = {"/profiles", "/system", "/scripts"};
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        UINT status = fx_directory_create(&fs_media, (CHAR *)paths[i]);
        if (status == FX_ALREADY_CREATED) {
            UINT attributes;
            status = fx_directory_attributes_read(&fs_media, (CHAR *)paths[i], &attributes);
            if (status == FX_SUCCESS && !(attributes & FX_DIRECTORY)) status = FX_NOT_A_FILE;
        }
        if (status != FX_SUCCESS) return status;
    }
    return fx_media_flush(&fs_media);
}

void fs_init_dir(void)
{
    (void)fs_make_directories();
}

int fs_init(void)
{
    if (!fs_libraries_initialized) {
        fx_system_initialize();
        fs_libraries_initialized = true;
    }
    /* Reinitialization discards unsynced state, just as a remount after reset.
     * All handles from the preceding mount become invalid. */
    if (fs_media.fx_media_id == FX_MEDIA_ID) {
        UINT status = fx_media_abort(&fs_media);
        if (status != FX_SUCCESS) return fs_result(status);
    }
    if (flash_layer_deinit() != 0) return -FX_IO_ERROR;
    fs_media_ready = false;
    ++fs_generation;
    if (!fs_generation) ++fs_generation;
    memset(&fs_media, 0, sizeof(fs_media));
    UINT status = fx_media_open(&fs_media, "libamp", fs_media_driver, NULL,
                                fs_media_buffer, sizeof(fs_media_buffer));
    if (status != FX_SUCCESS) {
        /* Destructive recovery, limited to this partition and to one attempt;
         * existing LittleFS contents are not migrated. LevelX has to reset its
         * own block headers first (flash_layer_format()); the direct layer needs
         * nothing here because every write erases its block. */
#if FLASH_LAYER == FLASH_LAYER_LEVELX
        if (flash_layer_format() != 0) return -FX_IO_ERROR;
#endif
        memset(&fs_media, 0, sizeof(fs_media));
        status = fx_media_format(&fs_media, fs_media_driver, NULL,
                                 (UCHAR *)fs_media_buffer, sizeof(fs_media_buffer),
                                 "LIBAMP", 1, 128, 0,
                                 (ULONG)(flash_layer_capacity / FS_FILEX_SECTOR_SIZE),
                                 FS_FILEX_SECTOR_SIZE, 1, 1, 1);
        if (status != FX_SUCCESS) {
            (void)flash_layer_deinit();
            return fs_result(status);
        }
        memset(&fs_media, 0, sizeof(fs_media));
        status = fx_media_open(&fs_media, "libamp", fs_media_driver, NULL,
                               fs_media_buffer, sizeof(fs_media_buffer));
    }
#ifdef FX_ENABLE_FAULT_TOLERANT
    if (status == FX_SUCCESS) {
        /* Arm fault tolerance for this mount, before the directories below so
         * their creation is journaled too; it is re-armed on every mount because
         * the state lives in RAM. A failure is reported but not fatal: the log
         * needs a free cluster, and a full volume would otherwise become
         * unmountable (hence unfixable). Return fs_result(ft_status) to make it
         * fatal instead. */
        UINT ft_status = fx_fault_tolerant_enable(&fs_media, fs_fault_tolerant_buffer,
                                                  sizeof(fs_fault_tolerant_buffer));
        if (ft_status != FX_SUCCESS) {
            printf("libamp: fault tolerant enable failed (%u), continuing without it\r\n",
                   (unsigned int)ft_status);
        }
    }
#endif
    if (status == FX_SUCCESS) status = fs_make_directories();
    if (status != FX_SUCCESS) {
        if (fs_media.fx_media_id == FX_MEDIA_ID) (void)fx_media_abort(&fs_media);
        (void)flash_layer_deinit();
        fs_media_ready = false;
    }
    return fs_result(status);
}

/* Erase the volume. The media is left formatted but not mounted, holding
 * nothing but an empty root directory; call fs_init() before using it. LevelX
 * can only append to erased space, so its own block headers are reset first
 * (format() closes and reopens the layer); the direct layer needs nothing here
 * because every write of the driver erases the blocks it covers. */
int fs_format(void)
{
    if (fs_media.fx_media_id == FX_MEDIA_ID) (void)fx_media_abort(&fs_media);
    (void)flash_layer_deinit();
    fs_media_ready = false;
    ++fs_generation;
    if (!fs_generation) ++fs_generation;
    memset(&fs_media, 0, sizeof(fs_media));
#if FLASH_LAYER == FLASH_LAYER_LEVELX
    if (flash_layer_format() != 0) return -FX_IO_ERROR;
#else
    if (flash_layer_init() != 0) return -FX_IO_ERROR;
#endif
    UINT status = fx_media_format(&fs_media, fs_media_driver, NULL,
                                  (UCHAR *)fs_media_buffer, sizeof(fs_media_buffer),
                                  "LIBAMP", 1, 128, 0,
                                  (ULONG)(flash_layer_capacity / FS_FILEX_SECTOR_SIZE),
                                  FS_FILEX_SECTOR_SIZE, 1, 1, 1);
    if (status != FX_SUCCESS) {
        (void)flash_layer_deinit();
        return fs_result(status);
    }
    /* Reopening rebuilds the geometry, which format() itself does not do. */
    memset(&fs_media, 0, sizeof(fs_media));
    return 0;
}

/* Keep FileX's default directory at root, even between directory iterations. */
static bool fs_path(const char *name, char path[FX_MAXIMUM_PATH])
{
    if (!name || !*name) return false;
    size_t length = strlen(name);
    size_t prefix = name[0] == '/' ? 0 : 1;
    if (length + prefix >= FX_MAXIMUM_PATH) return false;
    if (prefix) path[0] = '/';
    memcpy(path + prefix, name, length + 1);
    while (length + prefix > 1 && path[length + prefix - 1] == '/')
        path[--length + prefix] = '\0';
    return true;
}

static bool fs_file_valid(const File *file)
{
    return file && fs_media.fx_media_id == FX_MEDIA_ID &&
           file->generation == fs_generation && file->handle.fx_file_id == FX_FILE_ID;
}

int fs_open(File *file, const char *name, size_t flags)
{
    char path[FX_MAXIMUM_PATH];
    size_t mode = flags & FS_MODE_MASK;
    const size_t allowed = FS_O_RDWR | FS_O_CREAT | FS_O_EXCL | FS_O_TRUNC | FS_O_APPEND;
    if (!file || !fs_path(name, path) || !mode || (flags & ~allowed) ||
        (mode == FS_O_RDONLY && (flags & (FS_O_TRUNC | FS_O_APPEND)))) return -FX_INVALID_OPTION;
    /* Do not overwrite an already registered FileX control block. */
    FX_FILE *existing = fs_media.fx_media_opened_file_list;
    for (ULONG i = 0; i < fs_media.fx_media_opened_file_count; ++i) {
        if (existing == &file->handle) return -FX_ACCESS_ERROR;
        existing = existing->fx_file_opened_next;
    }
    UINT status;
    if (flags & FS_O_CREAT) {
        status = fx_file_create(&fs_media, path);
        if (status != FX_SUCCESS && (status != FX_ALREADY_CREATED || (flags & FS_O_EXCL)))
            return fs_result(status);
    }
    memset(file, 0, sizeof(*file));
    status = fx_file_open(&fs_media, &file->handle, path, fs_filex_open_type(mode));
    if (status != FX_SUCCESS) return fs_result(status);
    file->flags = flags;
    file->generation = fs_generation;
    if (file->handle.fx_file_current_file_size > INT32_MAX) {
        (void)fs_close(file);
        return -FX_INVALID_OPTION;
    }
    if (flags & FS_O_TRUNC) {
        status = fx_file_truncate_release(&file->handle, 0);
        if (status != FX_SUCCESS) {
            (void)fs_close(file);
            return fs_result(status);
        }
    }
    if (flags & FS_O_APPEND) file->position = (FilePosition)file->handle.fx_file_current_file_size;
    return 0;
}

int fs_close(File *file)
{
    if (!fs_file_valid(file)) return -FX_NOT_OPEN;
    UINT status = fx_file_close(&file->handle);
    if (status == FX_SUCCESS) {
        file->generation = 0;
        /* LittleFS close persists metadata; FileX close alone leaves FAT and
         * directory updates in the media cache. Preserve the fs_* contract. */
        status = fx_media_flush(&fs_media);
    }
    return fs_result(status);
}

int fs_stat(const char *name, FileStat *buf)
{
    char path[FX_MAXIMUM_PATH];
    if (!buf || !fs_path(name, path)) return -FX_INVALID_NAME;
    if (fs_media.fx_media_id != FX_MEDIA_ID) return -FX_MEDIA_NOT_OPEN;
    memset(buf, 0, sizeof(*buf));
    if (!strcmp(path, "/")) {
        buf->type = FS_TYPE_DIR;
        strcpy(buf->name, "/");
        return 0;
    }
    UINT attr, year, month, day, hour, minute, second;
    ULONG size;
    UINT status = fx_directory_information_get(&fs_media, path, &attr, &size,
                                               &year, &month, &day, &hour, &minute, &second);
    if (status != FX_SUCCESS) return fs_result(status);
    buf->type = attr & FX_DIRECTORY ? FS_TYPE_DIR : FS_TYPE_REG;
    buf->size = buf->type == FS_TYPE_DIR ? 0 : size;
    const char *base = strrchr(path, '/') + 1;
    if (strlen(base) >= sizeof(buf->name)) return -FX_INVALID_NAME;
    strcpy(buf->name, base);
    return 0;
}

int fs_unlink(const char *name)
{
    char path[FX_MAXIMUM_PATH];
    FileStat info;
    if (!fs_path(name, path)) return -FX_INVALID_NAME;
    int status = fs_stat(path, &info);
    if (status < 0) return status;
    return fs_result(info.type == FS_TYPE_DIR ? fx_directory_delete(&fs_media, path) :
                     fx_file_delete(&fs_media, path));
}

int fs_rename(const char *old, const char *new_name)
{
    char source[FX_MAXIMUM_PATH], target[FX_MAXIMUM_PATH];
    FileStat info;
    if (!fs_path(old, source) || !fs_path(new_name, target)) return -FX_INVALID_NAME;
    int status = fs_stat(source, &info);
    if (status < 0) return status;
    if (!strcmp(source, target)) return 0;
    return fs_result(info.type == FS_TYPE_DIR ? fx_directory_rename(&fs_media, source, target) :
                     fx_file_rename(&fs_media, source, target));
}

size_t fs_read(File *file, void *ptr, size_t size)
{
    if (!fs_file_valid(file) || !ptr || (file->flags & FS_MODE_MASK) == FS_O_WRONLY) return 0;
    if ((ULONG64)file->position >= file->handle.fx_file_current_file_size) return 0;
    UINT status = fx_file_seek(&file->handle, (ULONG)file->position);
    ULONG actual = 0;
    if (status == FX_SUCCESS) status = fx_file_read(&file->handle, ptr, size, &actual);
    if (status != FX_SUCCESS && status != FX_END_OF_FILE) return 0;
    file->position += (FilePosition)actual;
    return actual;
}

/* FileX clamps seeks to EOF; zero-fill gaps explicitly to retain fs_* semantics. */
static UINT fs_extend(File *file, ULONG size)
{
    static const UCHAR zeros[128] = {0};
    UINT status = fx_file_seek(&file->handle, (ULONG)file->handle.fx_file_current_file_size);
    while (status == FX_SUCCESS && file->handle.fx_file_current_file_size < size) {
        ULONG remaining = size - (ULONG)file->handle.fx_file_current_file_size;
        ULONG chunk = remaining < sizeof(zeros) ? remaining : sizeof(zeros);
        status = fx_file_write(&file->handle, (VOID *)zeros, chunk);
    }
    return status;
}

size_t fs_write(File *file, void *ptr, size_t size)
{
    if (!fs_file_valid(file) || !ptr || (file->flags & FS_MODE_MASK) == FS_O_RDONLY || !size) return 0;
    if (file->flags & FS_O_APPEND) file->position = (FilePosition)file->handle.fx_file_current_file_size;
    if (size > (size_t)(INT32_MAX - file->position)) return 0;
    if ((ULONG64)file->position > file->handle.fx_file_current_file_size &&
        fs_extend(file, (ULONG)file->position) != FX_SUCCESS) return 0;
    if (fx_file_seek(&file->handle, (ULONG)file->position) != FX_SUCCESS) return 0;
    ULONG64 before = file->handle.fx_file_current_file_offset;
    UINT status = fx_file_write(&file->handle, ptr, size);
    size_t actual = (size_t)(file->handle.fx_file_current_file_offset - before);
    if (status != FX_SUCCESS && actual > size) return 0;
    file->position += (FilePosition)actual;
    return actual;
}

int fs_seek(File *file, FilePosition offset, int whence)
{
    if (!fs_file_valid(file)) return -FX_NOT_OPEN;
    int64_t base;
    switch (whence) {
    case FS_SEEK_SET: base = 0; break;
    case FS_SEEK_CUR: base = file->position; break;
    case FS_SEEK_END: base = (int64_t)file->handle.fx_file_current_file_size; break;
    default: return -FX_INVALID_OPTION;
    }
    int64_t position = base + offset;
    if (position < 0 || position > INT32_MAX) return -FX_INVALID_OPTION;
    file->position = (FilePosition)position;
    return 0;
}

FilePosition fs_tell(File *file)
{
    return fs_file_valid(file) ? file->position : -1;
}

FilePosition fs_size(File *file)
{
    return fs_file_valid(file) ? (FilePosition)file->handle.fx_file_current_file_size : -1;
}

int fs_truncate(File *file, FilePosition size)
{
    if (!fs_file_valid(file)) return -FX_NOT_OPEN;
    if (size < 0 || (file->flags & FS_MODE_MASK) == FS_O_RDONLY) return -FX_ACCESS_ERROR;
    return fs_result((ULONG64)size > file->handle.fx_file_current_file_size ? fs_extend(file, (ULONG)size) :
                     fx_file_truncate_release(&file->handle, (ULONG)size));
}

int fs_sync(File *file)
{
    if (!fs_file_valid(file)) return -FX_NOT_OPEN;
    return fs_result(fx_media_flush(&fs_media));
}

int fs_statfs(const char *path, FileSystemStat *buf)
{
    if (!path || !buf) return -FX_PTR_ERROR;
    if (fs_media.fx_media_id != FX_MEDIA_ID) return -FX_MEDIA_NOT_OPEN;
    memset(buf, 0, sizeof(*buf));
    buf->disk_version = fs_media.fx_media_FAT_type;
    buf->block_size = fs_media.fx_media_sectors_per_cluster * fs_media.fx_media_bytes_per_sector;
    buf->block_count = fs_media.fx_media_total_clusters;
    buf->name_max = FX_MAX_LONG_NAME_LEN - 1;
    buf->file_max = INT32_MAX;
    return 0;
}

int fs_statvfs(const char *path, VolumeStat *buf)
{
    FileSystemStat info;
    if (!buf) return -FX_PTR_ERROR;
    int status = fs_statfs(path, &info);
    if (status < 0) return status;
    ULONG64 available;
    UINT result = fx_media_extended_space_available(&fs_media, &available);
    if (result != FX_SUCCESS) return fs_result(result);
    buf->f_bsize = buf->f_frsize = info.block_size;
    buf->f_blocks = info.block_count;
    buf->f_bfree = (unsigned long)(available / info.block_size);
    return 0;
}

int fs_mkdir(const char *path, size_t mode)
{
    char name[FX_MAXIMUM_PATH];
    (void)mode;
    if (!fs_path(path, name)) return -FX_INVALID_NAME;
    return fs_result(fx_directory_create(&fs_media, name));
}

int fs_rmdir(const char *path)
{
    char name[FX_MAXIMUM_PATH];
    if (!fs_path(path, name)) return -FX_INVALID_NAME;
    return fs_result(fx_directory_delete(&fs_media, name));
}

int fs_opendir(Directory *dir, const char *name)
{
    FileStat info;
    char path[FX_MAXIMUM_PATH];
    if (!dir || !fs_path(name, path)) return -FX_INVALID_NAME;
    int status = fs_stat(path, &info);
    if (status < 0) return status;
    if (info.type != FS_TYPE_DIR) return -FX_INVALID_PATH;
    memset(dir, 0, sizeof(*dir));
    strcpy(dir->path, path);
    dir->generation = fs_generation;
    dir->open = true;
    return 0;
}

static bool fs_dir_valid(const Directory *dir)
{
    return dir && dir->open && dir->generation == fs_generation && fs_media.fx_media_id == FX_MEDIA_ID;
}

int fs_readdir(Directory *dir, FileStat *buf)
{
    if (!fs_dir_valid(dir) || !buf) return -FX_PTR_ERROR;
    memset(buf, 0, sizeof(*buf));
    /* LittleFS exposes dot entries, including in the root directory. */
    if (dir->position < 2) {
        strcpy(buf->name, dir->position == 0 ? "." : "..");
        buf->type = FS_TYPE_DIR;
        ++dir->position;
        return 1;
    }
    UINT status = fx_directory_default_set(&fs_media, dir->path);
    if (status != FX_SUCCESS) return fs_result(status);
    UINT attributes, year, month, day, hour, minute, second;
    ULONG size;
    FilePosition index = 2;
    bool first = true;
    for (;;) {
        status = first ? fx_directory_first_full_entry_find(&fs_media, buf->name, &attributes, &size,
                                  &year, &month, &day, &hour, &minute, &second) :
                         fx_directory_next_full_entry_find(&fs_media, buf->name, &attributes, &size,
                                  &year, &month, &day, &hour, &minute, &second);
        first = false;
        if (status != FX_SUCCESS) break;
        if (!strcmp(buf->name, ".") || !strcmp(buf->name, "..") || (attributes & FX_VOLUME)) continue;
        if (index++ == dir->position) break;
    }
    UINT restore = fx_directory_default_set(&fs_media, "/");
    if (restore != FX_SUCCESS) return fs_result(restore);
    if (status == FX_NO_MORE_ENTRIES) return 0;
    if (status != FX_SUCCESS) return fs_result(status);
    if (dir->position == INT32_MAX) return -FX_INVALID_OPTION;
    buf->type = attributes & FX_DIRECTORY ? FS_TYPE_DIR : FS_TYPE_REG;
    buf->size = buf->type == FS_TYPE_DIR ? 0 : size;
    ++dir->position;
    return 1;
}

int fs_telldir(Directory *dir)
{
    return fs_dir_valid(dir) ? dir->position : -FX_PTR_ERROR;
}

int fs_seekdir(Directory *dir, FilePosition pos)
{
    if (!fs_dir_valid(dir) || pos < 0) return -FX_INVALID_OPTION;
    Directory probe = *dir;
    probe.position = 0;
    FileStat info;
    while (probe.position < pos) {
        int status = fs_readdir(&probe, &info);
        if (status <= 0) return status < 0 ? status : -FX_INVALID_OPTION;
    }
    dir->position = pos;
    return 0;
}

int fs_rewinddir(Directory *dir)
{
    return fs_seekdir(dir, 0);
}

int fs_closedir(Directory *dir)
{
    if (!fs_dir_valid(dir)) return -FX_PTR_ERROR;
    dir->open = false;
    return 0;
}
#endif
