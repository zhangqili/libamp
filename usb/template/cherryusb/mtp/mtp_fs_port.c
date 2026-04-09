#include <stdio.h>
#include <string.h>
#include "mtp_fs_port.h"
#include "file_system.h"

extern lfs_t _lfs; 

#ifndef MTP_DESCRIPTION
#define MTP_DESCRIPTION "Keyboard Flash"
#endif
//--------------------------------------------------------------------+
// 文件描述符池
//--------------------------------------------------------------------+
#define MAX_OPEN_FILES 4
#define MAX_OPEN_DIRS  8

static lfs_file_t mtp_files[MAX_OPEN_FILES];
static bool file_used[MAX_OPEN_FILES] = {false};

static lfs_dir_t mtp_dirs[MAX_OPEN_DIRS];
static bool dir_used[MAX_OPEN_DIRS] = {false};
static struct mtp_dirent current_dirent; // 用于 readdir 返回

//--------------------------------------------------------------------+
// 信息与通知类接口
//--------------------------------------------------------------------+

// 如果文件发生改变，可以在这里触发键盘刷新显示、播放音效等，暂时返回0即可
int usbd_mtp_notify_object_add(const char *path) { return 0; }
int usbd_mtp_notify_object_remove(const char *path) { return 0; }

// 告诉 MTP 电脑端，根目录叫什么
const char *usbd_mtp_fs_root_path(void) { 
    return "/"; 
}

// 告诉 MTP 电脑端，磁盘显示什么名字 (比如在 Windows 我的电脑里显示的盘符名称)
const char *usbd_mtp_fs_description(void) { 
    return MTP_DESCRIPTION; 
}

//--------------------------------------------------------------------+
// 目录操作接口
//--------------------------------------------------------------------+

int usbd_mtp_mkdir(const char *path) { 
    return fs_mkdir(path, 0);
}

int usbd_mtp_rmdir(const char *path) { 
    return fs_rmdir(path); 
}

int usbd_mtp_unlink(const char *path) { 
    return fs_unlink(path); 
}

MTP_DIR *usbd_mtp_opendir(const char *name) {
    for (int i = 0; i < MAX_OPEN_DIRS; i++) {
        if (!dir_used[i]) {
            if (fs_opendir(&mtp_dirs[i], name) >= 0) {
                dir_used[i] = true;
                return (MTP_DIR *)&mtp_dirs[i];
            }
            break;
        }
    }
    return NULL;
}

int usbd_mtp_closedir(MTP_DIR *d) {
    if (!d) return -1;
    for (int i = 0; i < MAX_OPEN_DIRS; i++) {
        if (d == (MTP_DIR *)&mtp_dirs[i] && dir_used[i]) {
            fs_closedir(&mtp_dirs[i]); // 调用 file_stream 的 closedir
            dir_used[i] = false;
            return 0;
        }
    }
    return -1;
}

struct mtp_dirent *usbd_mtp_readdir(MTP_DIR *d) {
    if (!d) return NULL;
    Directory *dir = (Directory *)d;
    FileStat info;

    if (fs_readdir(dir, &info) > 0) {
        strncpy(current_dirent.d_name, info.name, sizeof(current_dirent.d_name) - 1);
        current_dirent.d_name[sizeof(current_dirent.d_name) - 1] = '\0';
        
        current_dirent.d_type = (info.type == LFS_TYPE_DIR) ? 4 : 8; 
        return &current_dirent;
    }
    return NULL; 
}

int usbd_mtp_stat(const char *file, mtp_stat_t *buf) {
    FileStat info;
    int res = fs_stat(file, &info);
    
    if (res >= 0) {
        buf->size = info.size;
        buf->is_dir = (info.type == LFS_TYPE_DIR);
        return 0;
    }
    return -1;
}

int usbd_mtp_statfs(const char *path, struct mtp_statfs *buf) {
    FileSystemStat fs_info;
    VolumeStat vol_info;
    if (fs_statvfs(path, &vol_info) >= 0) {
        buf->f_bsize  = vol_info.f_bsize;
        buf->f_blocks = vol_info.f_blocks;
        buf->f_bfree = vol_info.f_blocks / 2; // TODO: 在 file_stream 中完善真实的剩余容量计算
        return 0;
    }
    if (fs_statfs(path, &fs_info) >= 0) {
        buf->f_bsize  = fs_info.block_size;
        buf->f_blocks = fs_info.block_count;
        buf->f_bfree = buf->f_blocks / 2; // TODO: 在 file_stream 中完善真实的剩余容量计算
        return 0;
    }
    return -1;
}

//--------------------------------------------------------------------+
// 读写操作接口
//--------------------------------------------------------------------+

int usbd_mtp_open(const char *path, uint8_t mode) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!file_used[i]) {
            size_t fs_flags = (mode == 0) ? FS_O_RDONLY : (FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC);
            
            if (fs_open(&mtp_files[i], path, fs_flags) >= 0) {
                file_used[i] = true;
                return i; 
            }
            break;
        }
    }
    return -1;
}

int usbd_mtp_close(int fd) {
    if (fd >= 0 && fd < MAX_OPEN_FILES && file_used[fd]) {
        fs_close(&mtp_files[fd]);
        file_used[fd] = false;
        return 0;
    }
    return -1;
}

int usbd_mtp_read(int fd, void *buf, size_t len) {
    if (fd >= 0 && fd < MAX_OPEN_FILES && file_used[fd]) {
        return fs_read(&mtp_files[fd], buf, len);
    }
    return -1;
}

int usbd_mtp_write(int fd, const void *buf, size_t len) {
    if (fd >= 0 && fd < MAX_OPEN_FILES && file_used[fd]) {
        return fs_write(&mtp_files[fd], (void*)buf, len);
    }
    return -1;
}

int usbd_mtp_rename(const char *oldpath, const char *newpath) {
    return fs_rename(oldpath, newpath);
}

int usbd_mtp_format_store(void) {
    
    return 0;
}

void usbd_mtp_device_reset(void)  {

}