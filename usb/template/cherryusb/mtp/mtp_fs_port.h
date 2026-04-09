/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef MTP_FS_PORT_H
#define MTP_FS_PORT_H

#include "usbd_core.h"

typedef void MTP_DIR;

struct mtp_dirent {
    uint8_t d_type;                              /* The type of the file */
    uint8_t d_namlen;                            /* The length of the not including the terminating null file name */
    uint16_t d_reclen;                           /* length of this record */
    char d_name[CONFIG_USBDEV_MTP_MAX_PATHNAME]; /* The null-terminated file name */
};

struct mtp_statfs {
    size_t f_bsize;  /* block size */
    size_t f_blocks; /* total data blocks in file system */
    size_t f_bfree;  /* free blocks in file system */
};

typedef struct {
    uint32_t size;
    bool is_dir;
} mtp_stat_t;


#ifdef __cplusplus
extern "C" {
#endif

int usbd_mtp_notify_object_add(const char *path);
int usbd_mtp_notify_object_remove(const char *path);

const char *usbd_mtp_fs_root_path(void);
const char *usbd_mtp_fs_description(void);

int usbd_mtp_mkdir(const char *path);
int usbd_mtp_rmdir(const char *path);
MTP_DIR *usbd_mtp_opendir(const char *name);
int usbd_mtp_closedir(MTP_DIR *d);
struct mtp_dirent *usbd_mtp_readdir(MTP_DIR *d);

int usbd_mtp_statfs(const char *path, struct mtp_statfs *buf);
int usbd_mtp_stat(const char *file, mtp_stat_t *buf);

int usbd_mtp_open(const char *path, uint8_t mode);
int usbd_mtp_close(int fd);
int usbd_mtp_read(int fd, void *buf, size_t len);
int usbd_mtp_write(int fd, const void *buf, size_t len);

int usbd_mtp_unlink(const char *path);


int usbd_mtp_rename(const char *oldpath, const char *newpath);
int usbd_mtp_format_store(void);
void usbd_mtp_device_reset(void);

#ifdef __cplusplus
}
#endif

#endif //MTP_FS_PORT_H
