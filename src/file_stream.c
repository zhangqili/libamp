/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "file_stream.h"
#include "keyboard_def.h"
#include "keyboard_config.h"
#include "driver.h"

#ifdef LFS_ENABLE
#include "lfs.h"

#ifndef LFS_READ_SIZE
#error "LFS_READ_SIZE is not defined."
#endif
#ifndef LFS_PROG_SIZE
#error "LFS_PROG_SIZE is not defined."
#endif
#ifndef LFS_BLOCK_SIZE
#error "LFS_BLOCK_SIZE is not defined."
#endif
#ifndef LFS_BLOCK_COUNT
#error "LFS_BLOCK_COUNT is not defined."
#endif
#ifndef LFS_CACHE_SIZE
#define LFS_CACHE_SIZE 16
#endif
#ifndef LFS_LOOKAHEAD_SIZE
#error "LFS_READ_SIZE is not defined."
#endif
#ifndef LFS_BLOCK_CYCLES
#error "LFS_READ_SIZE is not defined."
#endif

static uint8_t read_buffer[LFS_CACHE_SIZE];
static uint8_t prog_buffer[LFS_CACHE_SIZE];
static uint8_t lookahead_buffer[LFS_CACHE_SIZE];
static lfs_t _lfs;

static int lfs_flash_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    return flash_read(c->block_size * block + off, size, (uint8_t *)buffer);
}

static int lfs_flash_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    return flash_write(c->block_size * block + off, size, (uint8_t *)buffer);
}

static int lfs_flash_erase(const struct lfs_config *c, lfs_block_t block)
{
    return flash_erase((block * c->block_size), c->block_size);
}

static int _sync(const struct lfs_config *c)
{
    UNUSED(c);
    return LFS_ERR_OK;
}
static const struct lfs_config _lfs_config =
{
    // block device operations
    .read  = lfs_flash_read,
    .prog  = lfs_flash_prog,
    .erase = lfs_flash_erase,
    .sync  = _sync,

    // block device configuration
    .read_size = LFS_READ_SIZE,
    .prog_size = LFS_PROG_SIZE,
    .block_size = LFS_BLOCK_SIZE,
    .block_count = LFS_BLOCK_COUNT,

    .cache_size = LFS_CACHE_SIZE,
    .lookahead_size = LFS_LOOKAHEAD_SIZE,
    .block_cycles = LFS_BLOCK_CYCLES,

    .read_buffer = read_buffer,
    .prog_buffer = prog_buffer,
    .lookahead_buffer = lookahead_buffer,
};
#endif

int fs_init(void)
{
#ifdef LFS_ENABLE
    // mount the filesystem
    int err = lfs_mount(&_lfs, &_lfs_config);
    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err)
    {
        lfs_format(&_lfs, &_lfs_config);
        lfs_mount(&_lfs, &_lfs_config);
    }
    return err;
#endif
}

int fs_open(FileStream *stream, const char *name, size_t type)
{
#ifdef LFS_ENABLE
    if (stream == NULL || name == NULL)
        return -1;

    int err = lfs_file_open(&_lfs, &stream->file, name, (int)type);

    return err;
#else
    UNUSED(stream);
    UNUSED(name);
    UNUSED(type);
    return -1;
#endif
}

int fs_close(FileStream *file)
{
#ifdef LFS_ENABLE
    if (file == NULL)
        return -1;

    int err = lfs_file_close(&_lfs, &file->file);

    return err;
#else
    UNUSED(file);
    return -1;
#endif
}

size_t fs_read(void *ptr, size_t size_of_elements, size_t number_of_elements, FileStream *a_file)
{
#ifdef LFS_ENABLE
    if (ptr == NULL || a_file == NULL)
        return 0;
    if (size_of_elements == 0 || number_of_elements == 0)
        return 0;

    lfs_size_t bytes_to_read = size_of_elements * number_of_elements;

    lfs_ssize_t res = lfs_file_read(&_lfs, &a_file->file, ptr, bytes_to_read);

    if (res < 0)
    {
        return 0;
    }

    return (size_t)(res / size_of_elements);
#else
    UNUSED(ptr);
    UNUSED(size_of_elements);
    UNUSED(number_of_elements);
    UNUSED(a_file);
    return 0;
#endif
}

size_t fs_write(const void *ptr, size_t size_of_elements, size_t number_of_elements, FileStream *a_file)
{
#ifdef LFS_ENABLE
    if (ptr == NULL || a_file == NULL)
        return 0;
    if (size_of_elements == 0 || number_of_elements == 0)
        return 0;

    lfs_size_t bytes_to_write = size_of_elements * number_of_elements;

    lfs_ssize_t res = lfs_file_write(&_lfs, &a_file->file, ptr, bytes_to_write);

    if (res < 0)
    {
        return 0;
    }

    return (size_t)(res / size_of_elements);
#else
    UNUSED(ptr);
    UNUSED(size_of_elements);
    UNUSED(number_of_elements);
    UNUSED(a_file);
    return 0;
#endif
}

int fs_remove(const char * name)
{
#ifdef LFS_ENABLE
    if (name == NULL)
        return -1;

    return lfs_remove(&_lfs, name);
#else
    UNUSED(name);
    return -1;
#endif
}


int fs_seek(FileStream *file, long int offset, int whence)
{
#ifdef LFS_ENABLE
    if (file == NULL)
        return -1;

    lfs_soff_t res = lfs_file_seek(&_lfs, &file->file, (lfs_soff_t)offset, whence);

    if (res < 0)
    {
        return (int)res;
    }

    return 0;
#else
    UNUSED(file);
    UNUSED(offset);
    UNUSED(whence);
    return -1;
#endif
}

void fs_rewind(FileStream *file)
{
#ifdef LFS_ENABLE
    if (file != NULL)
    {
        lfs_file_seek(&_lfs, &file->file, 0, LFS_SEEK_SET);
    }
#else
    UNUSED(file);
#endif
}

int fs_getpos(FileStream *__restrict file, FileStreamPosition *__restrict pos)
{
#ifdef LFS_ENABLE
    if (file == NULL || pos == NULL)
        return -1;

    lfs_soff_t res = lfs_file_tell(&_lfs, &file->file);
    if (res < 0)
    {
        return (int)res;
    }

    *pos = (FileStreamPosition)res;
    return 0;
#else
    UNUSED(file);
    UNUSED(pos);
    return -1;
#endif
}

int	fs_setpos(FileStream * file, const FileStreamPosition * pos)
{
#ifdef LFS_ENABLE
    if (file == NULL || pos == NULL)
        return -1;

    lfs_soff_t res = lfs_file_seek(&_lfs, &file->file, (lfs_soff_t)(*pos), LFS_SEEK_SET);
    if (res < 0)
    {
        return (int)res;
    }

    return 0;
#else
    UNUSED(file);
    UNUSED(pos);
    return -1;
#endif
}

long fs_tell(FileStream *file)
{
#ifdef LFS_ENABLE
    if (file == NULL)
        return -1L;

    lfs_soff_t res = lfs_file_tell(&_lfs, &file->file);

    if (res < 0)
    {
        return -1L;
    }
    return (long)res;
#else
    UNUSED(file);
    return -1L;
#endif
}


long fs_size(FileStream * file)
{
#ifdef LFS_ENABLE
    if (file == NULL)
        return -1L;

    lfs_soff_t res = lfs_file_size(&_lfs, &file->file);

    if (res < 0)
    {
        return -1L;
    }
    return (long)res;
#else
    UNUSED(file);
    return -1L;
#endif
}
