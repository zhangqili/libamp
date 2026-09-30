/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef FILE_SYSTEM_H_
#define FILE_SYSTEM_H_

#include "stddef.h"
#include "stdbool.h"
#include "keyboard_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FILE_SYSTEM_RAW   0
#define FILE_SYSTEM_LFS   1
#define FILE_SYSTEM_FILEX 2

#define FS_TYPE_REG 1
#define FS_TYPE_DIR 2

#ifndef FILE_SYSTEM_TYPE
#define FILE_SYSTEM_TYPE FILE_SYSTEM_LFS
#endif

/* Optional middle layer between the file system and the flash driver. It is
 * shared by every FILE_SYSTEM_TYPE that stores data (LFS and FILEX; the RAW
 * backend is a stub that never touches flash).
 *
 * FLASH_LAYER_DIRECT forwards reads, programs and erases straight to
 * flash_read/flash_write/flash_erase, so the partition is used exactly as the
 * geometry describes it.
 *
 * FLASH_LAYER_LEVELX puts LevelX NOR wear leveling underneath: the partition is
 * addressed in 512-byte logical sectors that LevelX remaps across the physical
 * blocks, an erase releases the covered sectors (they read back as 0xFF), and a
 * program that covers part of a sector is read-modify-written. LevelX holds back
 * one erase block for reclaim and keeps header sectors in every block, so the
 * usable area is smaller; the layer asks it for the exact capacity on open.
 *
 * Both implementations are always compiled, so this is an ordinary
 * configuration macro: keyboard_config.h picks the layer, and the linker drops
 * the one that is not referenced. An unknown value is rejected where the layer
 * is implemented. */
#define FLASH_LAYER_DIRECT 1
#define FLASH_LAYER_LEVELX 2
#ifndef FLASH_LAYER
#define FLASH_LAYER FLASH_LAYER_DIRECT
#endif

/* Byte offset of the file system partition inside the flash driver address
 * space. FILEX_FLASH_OFFSET is the historical FileX-only spelling; both stay in
 * sync so existing configurations keep working. */
#if defined(FS_FLASH_OFFSET) && !defined(FILEX_FLASH_OFFSET)
#define FILEX_FLASH_OFFSET FS_FLASH_OFFSET
#elif !defined(FILEX_FLASH_OFFSET)
#define FILEX_FLASH_OFFSET 0
#endif
#ifndef FS_FLASH_OFFSET
#define FS_FLASH_OFFSET FILEX_FLASH_OFFSET
#endif

#if FILE_SYSTEM_TYPE == FILE_SYSTEM_RAW

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

typedef int File;
typedef int FileStat;
typedef int FileSystemStat;
typedef int Directory;
typedef long FilePosition;
#endif

#if FILE_SYSTEM_TYPE == FILE_SYSTEM_LFS
#include "lfs.h"

enum FileOpenFlags {
    // open flags
    FS_O_RDONLY  = LFS_O_RDONLY,  // Open a file as read only
    FS_O_WRONLY  = LFS_O_WRONLY,  // Open a file as write only
    FS_O_RDWR    = LFS_O_RDWR,    // Open a file as read and write
    FS_O_CREAT   = LFS_O_CREAT,   // Create a file if it does not exist
    FS_O_EXCL    = LFS_O_EXCL,    // Fail if a file already exists
    FS_O_TRUNC   = LFS_O_TRUNC,   // Truncate the existing file to zero size
    FS_O_APPEND  = LFS_O_APPEND,  // Move to end of file on every write
    FS_F_DIRTY   = LFS_F_DIRTY,   // File does not match storage
    FS_F_WRITING = LFS_F_WRITING, // File has been written since last flush
    FS_F_READING = LFS_F_READING, // File has been read since last flush
    FS_F_ERRED   = LFS_F_ERRED,   // An error occurred during write
    FS_F_INLINE  = LFS_F_INLINE,  // Currently inlined in directory entry
};

// File seek flags
enum FileWhenceFlags {
    FS_SEEK_SET = LFS_SEEK_SET,   // Seek relative to an absolute position
    FS_SEEK_CUR = LFS_SEEK_CUR,   // Seek relative to the current file position
    FS_SEEK_END = LFS_SEEK_END,   // Seek relative to the end of the file
};

typedef lfs_file_t File;
typedef struct lfs_info FileStat;
typedef struct lfs_fsinfo FileSystemStat;
typedef lfs_dir_t Directory;
typedef lfs_soff_t FilePosition;
#endif

#if FILE_SYSTEM_TYPE == FILE_SYSTEM_FILEX
/* The port's types come from libamp's fx_user.h, which fx_port.h includes
 * before its own type block (and which deliberately leaves out BOOL). */
#include "fx_api.h"
#include <stdint.h>

/* All addresses are byte offsets in the device exposed by flash_*(). */
#ifndef FS_PROG_SIZE
#define FS_PROG_SIZE FS_PROG_SIZE
#endif

/* Portable open flags, kept identical in every backend. FileX cannot express
 * them as a bitmask: fs_open() translates the access mode to one FX_OPEN_FOR_*
 * value and the control flags to dedicated FileX calls. */
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

typedef int32_t FilePosition;
typedef struct {
    FX_FILE handle;
    size_t flags;
    FilePosition position;
    uint32_t generation;
} File;
typedef struct {
    uint8_t type;
    uint32_t size;
    char name[FX_MAX_LONG_NAME_LEN];
} FileStat;
typedef struct {
    uint32_t disk_version;
    uint32_t block_size;
    uint32_t block_count;
    uint32_t name_max;
    uint32_t file_max;
    uint32_t attr_max;
} FileSystemStat;
typedef struct {
    char path[FX_MAXIMUM_PATH];
    FilePosition position;
    uint32_t generation;
    bool open;
} Directory;
#endif

typedef struct __VolumeStat {
    unsigned long f_bfree;
    unsigned long f_blocks;
    unsigned long f_bsize;
    unsigned long f_frsize;
} VolumeStat;

void fs_init_dir(void);
int fs_init(void);
int fs_format(void);

int fs_open(File * file, const char * name, size_t flags);
int fs_close(File * file);
int fs_unlink(const char * name);
int fs_rename(const char * old, const char * new_name);
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

#endif /* FILE_SYSTEM_H_ */
