/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef FILE_STREAM_H_
#define FILE_STREAM_H_

#include "stddef.h"
#include "stdbool.h"
#include "keyboard_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LFS_ENABLE
#include "lfs.h"
#endif

enum FileOpenFlags {
    // open flags
    FS_O_RDONLY = 1,         // Open a file as read only
    FS_O_WRONLY = 2,         // Open a file as write only
    FS_O_RDWR   = 3,         // Open a file as read and write
    FS_O_CREAT  = 0x0100,    // Create a file if it does not exist
    FS_O_EXCL   = 0x0200,    // Fail if a file already exists
    FS_O_TRUNC  = 0x0400,    // Truncate the existing file to zero size
    FS_O_APPEND = 0x0800,    // Move to end of file on every write
    FS_F_DIRTY   = 0x010000, // File does not match storage
    FS_F_WRITING = 0x020000, // File has been written since last flush
    FS_F_READING = 0x040000, // File has been read since last flush
    FS_F_ERRED   = 0x080000, // An error occurred during write
    FS_F_INLINE  = 0x100000, // Currently inlined in directory entry
};

// File seek flags
enum FileWhenceFlags {
    FS_SEEK_SET = 0,   // Seek relative to an absolute position
    FS_SEEK_CUR = 1,   // Seek relative to the current file position
    FS_SEEK_END = 2,   // Seek relative to the end of the file
};

#ifdef LFS_ENABLE
typedef lfs_file_t File;
typedef struct lfs_info FileStat;
typedef struct lfs_fsinfo FileSystemStat;
typedef lfs_dir_t Directory;
typedef lfs_soff_t FilePosition;
#else
typedef void File;
typedef void FileStat;
typedef void FileSystemStat;
typedef void Directory;
typedef long FilePosition;
#endif

typedef struct __VolumeStat {
    unsigned long f_bfree;
    unsigned long f_blocks;
    unsigned long f_bsize;
    unsigned long f_frsize;
} VolumeStat;

int fs_init(void);

int fs_open(File * file, const char * name, size_t flags);
int fs_close(File * file);
int fs_unlink(const char * name);
int fs_rename(const char * old, const char * new);
size_t fs_read(File *file, void *ptr, size_t size);
size_t fs_write(File *file, void *ptr, size_t size);
int fs_seek(File *file,  FilePosition offset, int whence);
FilePosition fs_tell(File * file);
int fs_truncate(File * file, FilePosition size);
int fs_sync(File *file);

int fs_stat(const char * file, FileStat*buf);
FilePosition fs_size(File * file);

int fs_statfs(const char * path, FileSystemStat*buf);
int fs_statvfs(const char * file, VolumeStat*buf);
int fs_mkdir(const char * path, size_t mode);
int fs_rmdir(const char * path);
int fs_opendir(Directory *dir, const char *name);
int fs_readdir(Directory *dir, FileStat *buf);
int fs_telldir(Directory *dir);
int fs_seekdir(Directory *dir, FilePosition pos);
int fs_rewinddir(Directory *dir);
int fs_closedir(Directory *dir);

#ifdef __cplusplus
}
#endif

#endif /* FILE_STREAM_H_ */
