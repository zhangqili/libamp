/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "file_system.h"
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
lfs_t _lfs;

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
const struct lfs_config _lfs_config =
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

void fs_init_dir(void)
{
#ifdef LFS_ENABLE
    lfs_mkdir(&_lfs, "profiles");
    lfs_mkdir(&_lfs, "system");
    lfs_mkdir(&_lfs, "scripts");
#endif
}

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
    fs_init_dir();
    return err;
#endif
}


int fs_open(File * file, const char * name, size_t flags)
{
#ifdef LFS_ENABLE
    if (file == NULL || name == NULL)
        return -1;

    int err = lfs_file_open(&_lfs, file, name, (int)flags);

    return err;
#else
    UNUSED(file);
    UNUSED(name);
    UNUSED(flags);
    return -1;
#endif
}

int fs_close(File * file)
{
#ifdef LFS_ENABLE
    if (file == NULL)
        return -1;

    int err = lfs_file_close(&_lfs, file);

    return err;
#else
    UNUSED(file);
    return -1;
#endif
}

int fs_unlink(const char * name)
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

int fs_rename(const char * old, const char * new)
{
#ifdef LFS_ENABLE
    if (old == NULL || new == NULL)
        return -1;

    return lfs_rename(&_lfs, old, new);
#else
    UNUSED(old);
    UNUSED(new);
    return -1;
#endif
}

size_t fs_read(File *file, void *ptr, size_t size)
{
#ifdef LFS_ENABLE
    if (ptr == NULL || file == NULL)
        return -1;
    lfs_ssize_t res = lfs_file_read(&_lfs, file, ptr, size);

    if (res < 0)
    {
        return 0;
    }

    return (size_t)(res);
#else
    UNUSED(ptr);
    UNUSED(size);
    UNUSED(file);
    return 0;
#endif
}

size_t fs_write(File *file, void *ptr, size_t size)
{
#ifdef LFS_ENABLE
    if (ptr == NULL || file == NULL)
        return 0;

    lfs_ssize_t res = lfs_file_write(&_lfs, file, ptr, size);

    if (res < 0)
    {
        return 0;
    }

    return (size_t)(res);
#else
    UNUSED(ptr);
    UNUSED(size);
    UNUSED(file);
    return 0;
#endif
}

int fs_seek(File *file,  FilePosition offset, int whence)
{
#ifdef LFS_ENABLE
    if (file == NULL)
        return -1;

    lfs_soff_t res = lfs_file_seek(&_lfs, file, (lfs_soff_t)offset, whence);

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

FilePosition fs_tell(File * file)
{
#ifdef LFS_ENABLE
    if (file == NULL)
        return -1L;

    FilePosition res = lfs_file_tell(&_lfs, file);

    if (res < 0)
    {
        return -1L;
    }
    return (FilePosition)res;
#else
    UNUSED(file);
    return -1L;
#endif
}

FilePosition fs_size(File * file)
{
#ifdef LFS_ENABLE
    if (file == NULL)
        return -1L;

    lfs_soff_t res = lfs_file_size(&_lfs, file);

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

int fs_truncate(File * file, FilePosition size)
{
#ifdef LFS_ENABLE
    if (file == NULL)
        return -1;

    int res = lfs_file_truncate(&_lfs, file, (lfs_soff_t)size);

    return res;
#else
    UNUSED(file);
    UNUSED(size);
    return -1;
#endif
}

int fs_sync(File *file)
{
#ifdef LFS_ENABLE
    if (file == NULL)
        return -1;

    int res = lfs_file_sync(&_lfs, file);

    return res;
#else
    UNUSED(file);
    return -1;
#endif
}

int fs_stat(const char * file, FileStat*buf)
{
#ifdef LFS_ENABLE
    if (file == NULL || buf == NULL)
        return -1;

    int res = lfs_stat(&_lfs, file, buf);

    return res;
#else
    UNUSED(file);
    UNUSED(buf);
    return -1;
#endif
}

int fs_statfs(const char * path, FileSystemStat*buf)
{
#ifdef LFS_ENABLE
    UNUSED(path);
    if (path == NULL || buf == NULL)
        return -1;

    int res = lfs_fs_stat(&_lfs, buf);

    return res;
#else
    UNUSED(path);
    UNUSED(buf);
    return -1;
#endif
}

int fs_statvfs(const char * file, VolumeStat*buf)
{
#ifdef LFS_ENABLE
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
#else
    UNUSED(file);
    UNUSED(buf);
    return -1;
#endif
}

int fs_mkdir(const char * path, size_t mode)
{
#ifdef LFS_ENABLE
    if (path == NULL)
        return -1;
    UNUSED(mode);
    int res = lfs_mkdir(&_lfs, path);

    return res;
#else
    UNUSED(path);
    UNUSED(mode);
    return -1;
#endif
}

int fs_rmdir(const char * path)
{
#ifdef LFS_ENABLE
    if (path == NULL)
        return -1;

    int res = lfs_remove(&_lfs, path);

    return res;
#else
    UNUSED(path);
    return -1;
#endif
}

int fs_opendir(Directory *dir, const char *name)
{
#ifdef LFS_ENABLE
    if (dir == NULL || name == NULL)
        return -1;

    int res = lfs_dir_open(&_lfs, dir, name);

    return res;
#else
    UNUSED(dir);
    UNUSED(name);
    return -1;
#endif
}

int fs_readdir(Directory *dir, FileStat *buf)
{
#ifdef LFS_ENABLE
    if (dir == NULL || buf == NULL)
        return -1;

    int res = lfs_dir_read(&_lfs, dir, buf);

    return res;
#else
    UNUSED(dir);
    UNUSED(buf);
    return -1;
#endif
}

int fs_telldir(Directory *dir)
{
#ifdef LFS_ENABLE
    if (dir == NULL)
        return -1;

    int res = lfs_dir_tell(&_lfs, dir);

    return res;
#else
    UNUSED(dir);
    return -1;
#endif
}

int fs_seekdir(Directory *dir, FilePosition pos)
{
#ifdef LFS_ENABLE
    if (dir == NULL)
        return -1;

    int res = lfs_dir_seek(&_lfs, dir, pos);

    return res;
#else
    UNUSED(dir);
    UNUSED(pos);
    return -1;
#endif

}

int fs_rewinddir(Directory *dir)
{
#ifdef LFS_ENABLE
    if (dir == NULL)
        return -1;

    int res = lfs_dir_rewind(&_lfs, dir);

    return res;
#else
    UNUSED(dir);
    return -1;
#endif
}

int fs_closedir(Directory *dir)
{
#ifdef LFS_ENABLE
    if (dir == NULL)
        return -1;

    int res = lfs_dir_close(&_lfs, dir);

    return res;
#else
    UNUSED(dir);
    return -1;
#endif
}
