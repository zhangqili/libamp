/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "file_system.h"
#include "keyboard_def.h"
#include "keyboard_config.h"
#include "driver.h"

#if FILE_SYSTEM_TYPE == FILE_SYSTEM_RAW
void fs_init_dir(void)
{

}

int fs_init(void)
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

void fs_init_dir(void)
{
    lfs_mkdir(&_lfs, "profiles");
    lfs_mkdir(&_lfs, "system");
    lfs_mkdir(&_lfs, "scripts");
}

int fs_init(void)
{
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
