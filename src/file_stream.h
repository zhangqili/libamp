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

enum FileStreamOpenFlags {
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
enum FileStreamWhenceFlags {
    FS_SEEK_SET = 0,   // Seek relative to an absolute position
    FS_SEEK_CUR = 1,   // Seek relative to the current file position
    FS_SEEK_END = 2,   // Seek relative to the end of the file
};

typedef struct {
#ifdef LFS_ENABLE
    lfs_file_t file;
#endif
} FileStream;
typedef long FileStreamPosition;

int fs_init(void);

int fs_open(FileStream * file, const char * name, size_t type);
int fs_close(FileStream * file);
int fs_remove(const char * name);

size_t fs_read(void *ptr, size_t size_of_elements, size_t number_of_elements, FileStream *a_file);
size_t fs_write(const void *ptr, size_t size_of_elements, size_t number_of_elements, FileStream *a_file);

int fs_seek(FileStream *file,  long int offset, int whence);
void fs_rewind(FileStream *file);

int	fs_getpos(FileStream *__restrict file, FileStreamPosition *__restrict pos);
int	fs_setpos(FileStream * file, const FileStreamPosition * pos);
long fs_tell(FileStream * file);
long fs_size(FileStream * file);

#ifdef __cplusplus
}
#endif

#endif /* FILE_STREAM_H_ */
