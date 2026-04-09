/*
 * The MIT License (MIT)
 * Modified for Real Filesystem API Integration
 */

#include "keyboard.h"
#include "mtp_device.h"
#include <stdio.h>
#include <string.h>
#include "mtp_fs_port.h"

#define DEV_INFO_MANUFACTURER   MANUFACTURER
#define DEV_INFO_MODEL          "Keyboard"
#define DEV_INFO_VERSION        LIBAMP_VERSION_STRING
#define DEV_PROP_FRIENDLY_NAME  PRODUCT

static inline size_t board_usb_get_serial(uint16_t desc_str[], size_t max_chars) {
    const char* serial = SERIAL_NUMBER;
    size_t len = strlen(serial);
    if (len > max_chars) len = max_chars;
    for (size_t i = 0; i < len; i++) desc_str[i] = serial[i];
    return len;
}

static void utf16le_to_utf8(const uint8_t* utf16_bytes, size_t char_count, char* utf8_buf, size_t max_len) {
    size_t out_idx = 0;
    for (size_t i = 0; i < char_count; i++) {
        // 安全拼装：按单字节读取并组合为 16 位，任何架构下都不会卡死
        uint16_t unicode_char = utf16_bytes[i * 2] | (utf16_bytes[i * 2 + 1] << 8);
        if (unicode_char == 0) break; // 遇到字符串结尾符
        
        // 转换为 UTF-8 字节流
        if (unicode_char < 0x0080) {
            if (out_idx + 1 >= max_len) break;
            utf8_buf[out_idx++] = (char)unicode_char;
        } else if (unicode_char < 0x0800) {
            if (out_idx + 2 >= max_len) break;
            utf8_buf[out_idx++] = (char)(0xC0 | (unicode_char >> 6));
            utf8_buf[out_idx++] = (char)(0x80 | (unicode_char & 0x3F));
        } else {
            if (out_idx + 3 >= max_len) break;
            utf8_buf[out_idx++] = (char)(0xE0 | (unicode_char >> 12));
            utf8_buf[out_idx++] = (char)(0x80 | ((unicode_char >> 6) & 0x3F));
            utf8_buf[out_idx++] = (char)(0x80 | (unicode_char & 0x3F));
        }
    }
    utf8_buf[out_idx] = '\0'; // 确保安全结尾
}

// 2. [发送用] 将单片机里的 UTF-8 转换为 UTF-16 数组
static void utf8_to_utf16(const char* utf8_str, uint16_t* utf16_buf, size_t max_chars) {
    size_t u16_idx = 0;
    size_t i = 0;
    while (utf8_str[i] != '\0' && u16_idx < max_chars - 1) {
        uint8_t c = (uint8_t)utf8_str[i++];
        if (c < 0x80) {
            utf16_buf[u16_idx++] = c;
        } else if ((c & 0xE0) == 0xC0) {
            uint8_t c2 = (uint8_t)utf8_str[i++];
            utf16_buf[u16_idx++] = ((c & 0x1F) << 6) | (c2 & 0x3F);
        } else if ((c & 0xF0) == 0xE0) {
            uint8_t c2 = (uint8_t)utf8_str[i++];
            uint8_t c3 = (uint8_t)utf8_str[i++];
            utf16_buf[u16_idx++] = ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
        }
    }
    utf16_buf[u16_idx] = 0; // 补充结束符
}

// 3. [发送用] 直接将 UTF-8 字符串添加到 MTP Container 中
static void mtp_container_add_utf8_string(mtp_container_info_t* container, const char* utf8_str) {
    uint16_t utf16_buf[64] = {0}; 
    utf8_to_utf16(utf8_str, utf16_buf, 64);
    (void) mtp_container_add_string(container, utf16_buf); // 调用 TinyUSB 原始函数发送宽字符
}

enum { SUPPORTED_STORAGE_ID = 0x00010001u };

#define MAX_MTP_HANDLES 32
#define MAX_PATH_LEN    32
static uint32_t pending_event_handle = 0;
static uint16_t pending_event_code = 0;
static void notify_host_object_event(uint32_t handle, uint16_t event_code) {
if (!tud_mtp_mounted()) return;
    
    mtp_event_t evt = {0};
    evt.code = event_code;
    
    // MTP 规范中，异步事件（如文件增加/删除）不属于特定事务，因此设为 0xFFFFFFFF
    evt.transaction_id = 0xFFFFFFFF; 
    
    // 大多数 MTP 主机在处理异步事件时忽略 Session ID，填 0 即可
    evt.session_id = 0; 
    
    // 参数 1：发生变动的 Object Handle
    evt.params[0] = handle;
    
    tud_mtp_event_send(&evt);
}
typedef struct {
    bool used;
    uint32_t handle;
    uint32_t parent;
    bool is_dir;
    uint16_t association_type;
    uint32_t size;
    char name[64];
    char path[MAX_PATH_LEN];
    bool scanned;
} mtp_obj_t;

static mtp_obj_t handle_map[MAX_MTP_HANDLES];
static bool is_session_opened = false;
static uint32_t send_obj_handle = 0;
static int active_fd = -1;

static uint32_t allocate_handle(void) {
    for (int i = 0; i < MAX_MTP_HANDLES; i++) {
        if (!handle_map[i].used) {
            handle_map[i].used = true;
            return i + 1; 
        }
    }
    return 0; 
}

static mtp_obj_t* get_obj(uint32_t handle) {
    if (handle == 0 || handle > MAX_MTP_HANDLES) return NULL;
    if (!handle_map[handle - 1].used) return NULL;
    return &handle_map[handle - 1];
}

static void build_path_from_string(const char* parent_path, const char* name, char* out_path, size_t max_len) {
    if (strcmp(parent_path, "/") == 0) {
        snprintf(out_path, max_len, "/%s", name);
    } else {
        snprintf(out_path, max_len, "%s/%s", parent_path, name);
    }
}

static void build_path_from_handle(uint32_t parent_handle, const char* name, char* out_path, size_t max_len) {
    const char* p_path = usbd_mtp_fs_root_path(); // 默认为根目录 "/"
    
    if (parent_handle != 0 && parent_handle != 0xFFFFFFFFu) {
        mtp_obj_t* p = get_obj(parent_handle);
        if (p) {
            p_path = p->path;
        }
    }
    
    build_path_from_string(p_path, name, out_path, max_len);
}

static void scan_single_dir(const char* base_path, uint32_t parent_handle) {
    MTP_DIR *d = usbd_mtp_opendir(base_path);
    if (!d) return;

    struct mtp_dirent *ent;
    static mtp_stat_t st;

    while ((ent = usbd_mtp_readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') {
            if (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0')) continue;
        }
        
        uint32_t handle = allocate_handle();
        if (handle == 0) break; 

        mtp_obj_t* obj = &handle_map[handle - 1];
        obj->handle = handle;
        obj->parent = parent_handle;
        obj->scanned = false;
        snprintf(obj->name, sizeof(obj->name), "%s", ent->d_name);

        build_path_from_string(base_path, ent->d_name, obj->path, sizeof(obj->path));

        if (usbd_mtp_stat(obj->path, &st) == 0) {
            obj->size = st.size;
            obj->is_dir = st.is_dir;
        }

        if (obj->is_dir) {
            obj->association_type = MTP_ASSOCIATION_GENERIC_FOLDER;
        } else {
            obj->association_type = MTP_ASSOCIATION_UNDEFINED;
        }
    }
    usbd_mtp_closedir(d);
}

static void scan_all_directories(void) {
    scan_single_dir(usbd_mtp_fs_root_path(), 0);
    bool more_to_scan = true;
    while (more_to_scan) {
        more_to_scan = false;
        for (int i = 0; i < MAX_MTP_HANDLES; i++) {
            if (handle_map[i].used && handle_map[i].is_dir && !handle_map[i].scanned) {
                handle_map[i].scanned = true; 
                scan_single_dir(handle_map[i].path, handle_map[i].handle); 
                more_to_scan = true; 
            }
        }
    }
}

static bool is_valid_parent(uint32_t parent_handle) {
    if (parent_handle == 0 || parent_handle == 0xFFFFFFFFu) return true; // 根目录永远合法
    return get_obj(parent_handle) != NULL; // 否则必须在内存映射中能找到
}

static void queue_pending_event(uint32_t handle, uint16_t event_code) {
    pending_event_handle = handle;
    pending_event_code = event_code;
}

static int32_t fs_get_device_info(tud_mtp_cb_data_t* cb_data) {
    mtp_container_info_t* io_container = &cb_data->io_container;
    (void) mtp_container_add_cstring(io_container, DEV_INFO_MANUFACTURER);
    (void) mtp_container_add_cstring(io_container, DEV_INFO_MODEL);
    (void) mtp_container_add_cstring(io_container, DEV_INFO_VERSION);
    uint16_t serial_utf16[33] = {0};
    board_usb_get_serial(serial_utf16, 32);
    (void) mtp_container_add_string(io_container, serial_utf16);
    if (!tud_mtp_data_send(io_container)) return MTP_RESP_DEVICE_BUSY;
    return 0;
}

static int32_t fs_open_close_session(tud_mtp_cb_data_t* cb_data) {
    const mtp_container_command_t* command = cb_data->command_container;
    if (command->header.code == MTP_OP_OPEN_SESSION) {
        memset(handle_map, 0, sizeof(handle_map));
        scan_all_directories();
        is_session_opened = true;
    } else { 
        is_session_opened = false;
        memset(handle_map, 0, sizeof(handle_map)); 
    }
    return MTP_RESP_OK;
}

static int32_t fs_get_storage_ids(tud_mtp_cb_data_t* cb_data) {
    uint32_t storage_ids [] = { SUPPORTED_STORAGE_ID };
    (void) mtp_container_add_auint32(&cb_data->io_container, 1, storage_ids);
    tud_mtp_data_send(&cb_data->io_container);
    return 0;
}

static int32_t fs_get_storage_info(tud_mtp_cb_data_t* cb_data) {
    mtp_container_info_t* io_container = &cb_data->io_container;
    const uint32_t storage_id = cb_data->command_container->params[0];
    if (storage_id != SUPPORTED_STORAGE_ID) return MTP_RESP_INVALID_STORAGE_ID;

    struct mtp_statfs sfs;
    uint64_t max_cap = 0, free_cap = 0;
    if (usbd_mtp_statfs(usbd_mtp_fs_root_path(), &sfs) == 0 && sfs.f_blocks > 0) {
        max_cap = (uint64_t)sfs.f_blocks * sfs.f_bsize;
        free_cap = (uint64_t)sfs.f_bfree * sfs.f_bsize;
    } else {
        max_cap = 2 * 1024 * 1024; free_cap = 2 * 1024 * 1024; 
    }
    
    mtp_container_add_uint16(io_container, MTP_STORAGE_TYPE_FIXED_RAM);
    mtp_container_add_uint16(io_container, MTP_FILESYSTEM_TYPE_GENERIC_HIERARCHICAL);
    mtp_container_add_uint16(io_container, MTP_ACCESS_CAPABILITY_READ_WRITE);
    mtp_container_add_uint64(io_container, max_cap);
    mtp_container_add_uint64(io_container, free_cap);
    mtp_container_add_uint32(io_container, 0xFFFFFFFF); 
    mtp_container_add_cstring(io_container, "Keyboard Flash");
    mtp_container_add_cstring(io_container, "vol");

    tud_mtp_data_send(io_container);
    return 0;
}

static int32_t fs_get_device_properties(tud_mtp_cb_data_t* cb_data) {
    mtp_container_info_t* io_container = &cb_data->io_container;
    const uint16_t dev_prop_code = (uint16_t) cb_data->command_container->params[0];
    if (cb_data->command_container->header.code == MTP_OP_GET_DEVICE_PROP_DESC) {
        if (dev_prop_code == MTP_DEV_PROP_DEVICE_FRIENDLY_NAME) {
            mtp_device_prop_desc_header_t hdr = {
                .device_property_code = dev_prop_code, .datatype = MTP_DATA_TYPE_STR, .get_set = MTP_MODE_GET
            };
            (void) mtp_container_add_raw(io_container, &hdr, sizeof(hdr));
            (void) mtp_container_add_cstring(io_container, DEV_PROP_FRIENDLY_NAME);
            (void) mtp_container_add_cstring(io_container, DEV_PROP_FRIENDLY_NAME);
            (void) mtp_container_add_uint8(io_container, 0);
            tud_mtp_data_send(io_container);
            return 0;
        }
    } else { 
        if (dev_prop_code == MTP_DEV_PROP_DEVICE_FRIENDLY_NAME) {
            (void) mtp_container_add_cstring(io_container, DEV_PROP_FRIENDLY_NAME);
            tud_mtp_data_send(io_container);
            return 0;
        }
    }
    return MTP_RESP_PARAMETER_NOT_SUPPORTED;
}
static int32_t fs_get_object_handles(tud_mtp_cb_data_t* cb_data) {
    mtp_container_info_t* io_container = &cb_data->io_container;
    const uint32_t storage_id    = cb_data->command_container->params[0];
    const uint32_t format_code   = cb_data->command_container->params[1];
    const uint32_t parent_handle = cb_data->command_container->params[2];

    if (storage_id != 0 && storage_id != 0xFFFFFFFFu && storage_id != SUPPORTED_STORAGE_ID) {
        return MTP_RESP_INVALID_STORAGE_ID;
    }

    static uint32_t handles[MAX_MTP_HANDLES];
    uint32_t count = 0;

    for (uint32_t i = 0; i < MAX_MTP_HANDLES; i++) {
        if (handle_map[i].used) {
            bool parent_match = false;
            if (parent_handle == 0xFFFFFFFFu || parent_handle == 0x00000000u) {
                parent_match = (handle_map[i].parent == 0);
            } else {
                parent_match = (handle_map[i].parent == parent_handle);
            }

            if (parent_match) {
                bool format_match = (format_code == 0 || format_code == 0xFFFFFFFFu || 
                    format_code == (handle_map[i].is_dir ? MTP_OBJ_FORMAT_ASSOCIATION : MTP_OBJ_FORMAT_UNDEFINED));
                if (format_match) handles[count++] = handle_map[i].handle;
            }
        }
    }
    
    (void) mtp_container_add_auint32(io_container, count, handles);
    tud_mtp_data_send(io_container);
    return 0;
}
static int32_t fs_get_object_info(tud_mtp_cb_data_t* cb_data) {
    mtp_container_info_t* io_container = &cb_data->io_container;
    const uint32_t obj_handle = cb_data->command_container->params[0];
    mtp_obj_t* f = get_obj(obj_handle);
    if (!f) return MTP_RESP_INVALID_OBJECT_HANDLE;

    // 【致命修复区】手动按字节装配 52 字节的 Dataset，彻底消灭结构体内存对齐导致的偏移
    uint8_t info_buf[52] = {0};
    uint32_t storage_id = SUPPORTED_STORAGE_ID;
    uint16_t object_format = f->is_dir ? MTP_OBJ_FORMAT_ASSOCIATION : MTP_OBJ_FORMAT_UNDEFINED;
    uint16_t protection_status = 0; 
    uint32_t object_compressed_size = f->is_dir ? 0 : f->size;
    uint16_t thumb_format = 0;
    uint32_t thumb_compressed_size = 0;
    uint32_t thumb_pix_width = 0;
    uint32_t thumb_pix_height = 0;
    uint32_t image_pix_width = 0;
    uint32_t image_pix_height = 0;
    uint32_t image_bit_depth = 0;
    uint32_t parent_object = f->parent;
    uint16_t association_type = f->association_type;
    uint32_t association_desc = 0;
    uint32_t sequence_number = 0;

    memcpy(&info_buf[0], &storage_id, 4);
    memcpy(&info_buf[4], &object_format, 2);
    memcpy(&info_buf[6], &protection_status, 2);
    memcpy(&info_buf[8], &object_compressed_size, 4);
    memcpy(&info_buf[12], &thumb_format, 2);
    memcpy(&info_buf[14], &thumb_compressed_size, 4);
    memcpy(&info_buf[18], &thumb_pix_width, 4);
    memcpy(&info_buf[22], &thumb_pix_height, 4);
    memcpy(&info_buf[26], &image_pix_width, 4);
    memcpy(&info_buf[30], &image_pix_height, 4);
    memcpy(&info_buf[34], &image_bit_depth, 4);
    memcpy(&info_buf[38], &parent_object, 4);
    memcpy(&info_buf[42], &association_type, 2);
    memcpy(&info_buf[44], &association_desc, 4);
    memcpy(&info_buf[48], &sequence_number, 4);

    (void) mtp_container_add_raw(io_container, info_buf, 52);
    mtp_container_add_utf8_string(io_container, f->name);
    //(void) mtp_container_add_cstring(io_container, f->name);
    (void) mtp_container_add_cstring(io_container, ""); 
    (void) mtp_container_add_cstring(io_container, ""); 
    (void) mtp_container_add_cstring(io_container, ""); 
    
    tud_mtp_data_send(io_container);
    return 0;
}

static int32_t fs_get_object(tud_mtp_cb_data_t* cb_data) {
    mtp_container_info_t* io_container = &cb_data->io_container;
    mtp_obj_t* f = get_obj(cb_data->command_container->params[0]);
    if (!f) return MTP_RESP_INVALID_OBJECT_HANDLE;

    if (cb_data->phase == MTP_PHASE_COMMAND) {
        active_fd = usbd_mtp_open(f->path, 0); 
        if (active_fd < 0) return MTP_RESP_GENERAL_ERROR;

        io_container->header->len = sizeof(mtp_container_header_t) + f->size;
        uint32_t to_read = tu_min32(f->size, CFG_TUD_MTP_EP_BUFSIZE - sizeof(mtp_container_header_t));
        int bytes_read = usbd_mtp_read(active_fd, io_container->payload, to_read);
        io_container->payload_bytes = (bytes_read > 0) ? bytes_read : 0;
        
        tud_mtp_data_send(io_container);
        if (bytes_read >= (int)f->size) {
            usbd_mtp_close(active_fd);
            active_fd = -1;
        }
    } else if (cb_data->phase == MTP_PHASE_DATA) {
        uint32_t offset = cb_data->total_xferred_bytes - sizeof(mtp_container_header_t);
        uint32_t to_read = tu_min32(f->size - offset, CFG_TUD_MTP_EP_BUFSIZE);
        
        int bytes_read = usbd_mtp_read(active_fd, io_container->payload, to_read);
        if (bytes_read > 0) {
            io_container->payload_bytes = bytes_read;
            tud_mtp_data_send(io_container);
        }
        if (offset + bytes_read >= f->size) {
            usbd_mtp_close(active_fd);
            active_fd = -1;
        }
    }
    return 0;
}

static int32_t fs_send_object_info(tud_mtp_cb_data_t* cb_data) {
    mtp_container_info_t* io_container = &cb_data->io_container;

    if (cb_data->phase == MTP_PHASE_COMMAND) {
        (void) tud_mtp_data_receive(io_container);
    } else if (cb_data->phase == MTP_PHASE_DATA) {
        uint8_t* payload = io_container->payload;
        uint32_t payload_len = io_container->payload_bytes; // 获取实际接收到的长度

        // 【安全防线 1】防止主机发来异常短包导致越界死机
        if (payload_len < 52) {
            return MTP_RESP_GENERAL_ERROR;
        }
        
        uint32_t parent_object = cb_data->command_container->params[1];
        if (parent_object == 0xFFFFFFFFu) {
            parent_object = 0; 
        }

        uint32_t object_compressed_size;
        uint16_t object_format;
        uint16_t association_type;

        memcpy(&object_format, &payload[4], 2);
        memcpy(&object_compressed_size, &payload[8], 4);
        memcpy(&association_type, &payload[42], 2);

        uint32_t new_handle = allocate_handle();
        if (new_handle == 0) return MTP_RESP_STORE_FULL;

        mtp_obj_t* f = &handle_map[new_handle - 1];
        f->handle = new_handle;
        f->parent = parent_object;
        f->size = object_compressed_size;
        f->association_type = association_type;
        f->is_dir = (object_format == MTP_OBJ_FORMAT_ASSOCIATION);

        memset(f->name, 0, sizeof(f->name)); // 清空文件名残留

        if (payload_len > 52) {
            uint8_t* str_buf = payload + 52; 
            uint8_t char_count = str_buf[0]; 
            uint8_t* utf16_bytes = str_buf + 1;

            uint32_t max_chars = (payload_len - 53) / 2;
            if (char_count > max_chars) char_count = max_chars;

            utf16le_to_utf8(utf16_bytes, char_count, f->name, sizeof(f->name));
        } else {
            strcpy(f->name, "New Folder"); 
        }
        // =========================================================

        build_path_from_handle(f->parent, f->name, f->path, sizeof(f->path));

        if (f->is_dir) {
            usbd_mtp_mkdir(f->path);
            queue_pending_event(f->handle, MTP_EVENT_OBJECT_ADDED);
        }
        send_obj_handle = new_handle; 
    }
    return 0;
}
static int32_t fs_send_object(tud_mtp_cb_data_t* cb_data) {
    mtp_container_info_t* io_container = &cb_data->io_container;
    mtp_obj_t* f = get_obj(send_obj_handle);
    if (!f) return MTP_RESP_INVALID_OBJECT_HANDLE;

    if (cb_data->phase == MTP_PHASE_COMMAND) {
        active_fd = usbd_mtp_open(f->path, 1); 
        if (active_fd < 0) return MTP_RESP_GENERAL_ERROR;

        io_container->header->len += f->size;
        tud_mtp_data_receive(io_container);
    } else {
        uint32_t offset = cb_data->total_xferred_bytes - sizeof(mtp_container_header_t) - io_container->payload_bytes;
        uint32_t write_size = io_container->payload_bytes;
        
        if (offset + write_size > f->size) write_size = f->size - offset; 

        if (write_size > 0) usbd_mtp_write(active_fd, io_container->payload, write_size);

        if (cb_data->total_xferred_bytes - sizeof(mtp_container_header_t) < f->size) {
            tud_mtp_data_receive(io_container);
        } else {
            usbd_mtp_close(active_fd);
            active_fd = -1;
            queue_pending_event(f->handle, MTP_EVENT_OBJECT_ADDED);
        }
    }
    return 0;
}

static void delete_children_handles(uint32_t parent_handle);
static int32_t fs_delete_object(tud_mtp_cb_data_t* cb_data) {
    mtp_obj_t* f = get_obj(cb_data->command_container->params[0]);
    if (!f) return MTP_RESP_INVALID_OBJECT_HANDLE;

    if (f->is_dir) {
        usbd_mtp_rmdir(f->path);
        delete_children_handles(f->handle);
    } else {
        usbd_mtp_unlink(f->path);
    }
    queue_pending_event(f->handle, MTP_EVENT_OBJECT_REMOVED);
    f->used = false; 
    return MTP_RESP_OK;
}
static void delete_children_handles(uint32_t parent_handle) {
    for (int i = 0; i < MAX_MTP_HANDLES; i++) {
        if (handle_map[i].used && handle_map[i].parent == parent_handle) {
            handle_map[i].used = false;
            if (handle_map[i].is_dir) delete_children_handles(handle_map[i].handle);
        }
    }
}

static int32_t fs_move_object(tud_mtp_cb_data_t* cb_data) {
    uint32_t obj_handle = cb_data->command_container->params[0];
    uint32_t parent_handle = cb_data->command_container->params[2];

    mtp_obj_t* f = get_obj(obj_handle);
    if (!f) return MTP_RESP_INVALID_OBJECT_HANDLE;

    char new_path[MAX_PATH_LEN];
    if (!is_valid_parent(parent_handle))
        return MTP_RESP_INVALID_PARENT_OBJECT;
    build_path_from_handle(parent_handle, f->name, new_path, sizeof(new_path));

    usbd_mtp_rename(f->path, new_path); 
    
    strncpy(f->path, new_path, sizeof(f->path));
    f->parent = parent_handle;

    return MTP_RESP_OK;
}

static int32_t fs_copy_object(tud_mtp_cb_data_t* cb_data) {
    uint32_t obj_handle    = cb_data->command_container->params[0]; // 源文件句柄
    uint32_t storage_id    = cb_data->command_container->params[1];
    uint32_t parent_handle = cb_data->command_container->params[2]; // 目标父文件夹句柄

    if (storage_id != 0 && storage_id != 0xFFFFFFFFu && storage_id != SUPPORTED_STORAGE_ID) {
        return MTP_RESP_INVALID_STORAGE_ID;
    }

    mtp_obj_t* src = get_obj(obj_handle);
    if (!src) return MTP_RESP_INVALID_OBJECT_HANDLE;

    // 为了单片机安全，暂不支持在 MTP 直接拖拽复制整个文件夹
    if (src->is_dir) return MTP_RESP_OPERATION_NOT_SUPPORTED;

    char new_path[MAX_PATH_LEN];
    if (!is_valid_parent(parent_handle))
        return MTP_RESP_INVALID_PARENT_OBJECT;
    build_path_from_handle(parent_handle, src->name, new_path, sizeof(new_path));

    // ======================================
    // 调用底层 API，使用 128 字节缓冲切片拷贝
    // ======================================
    int fd_src = usbd_mtp_open(src->path, 0); // 0: 读
    if (fd_src < 0) return MTP_RESP_GENERAL_ERROR;

    int fd_dst = usbd_mtp_open(new_path, 1);  // 1: 写(覆盖/新建)
    if (fd_dst < 0) {
        usbd_mtp_close(fd_src);
        return MTP_RESP_GENERAL_ERROR;
    }

    uint8_t buffer[128]; // 安全缓冲区
    int bytes_read;
    while ((bytes_read = usbd_mtp_read(fd_src, buffer, sizeof(buffer))) > 0) {
        usbd_mtp_write(fd_dst, buffer, bytes_read);
    }
    
    usbd_mtp_close(fd_src);
    usbd_mtp_close(fd_dst);

    // ======================================
    // 拷贝完成，向内存中注册新的 Handle
    // ======================================
    uint32_t new_handle = allocate_handle();
    if (new_handle == 0) {
        usbd_mtp_unlink(new_path); // 没句柄了，撤销拷贝
        return MTP_RESP_STORE_FULL;
    }

    mtp_obj_t* f = &handle_map[new_handle - 1];
    f->handle = new_handle;
    f->parent = (parent_handle == 0xFFFFFFFFu) ? 0 : parent_handle;
    f->is_dir = false;
    f->size = src->size;
    f->scanned = true; // 已经是确定存在的文件，直接标记为已扫描
    strncpy(f->name, src->name, sizeof(f->name));
    strncpy(f->path, new_path, sizeof(f->path));

    queue_pending_event(f->handle, MTP_EVENT_OBJECT_ADDED);

    return MTP_RESP_OK;
}

static int32_t fs_format_store(tud_mtp_cb_data_t* cb_data) {
    uint32_t storage_id = cb_data->command_container->params[0];
    if (storage_id != 0xFFFFFFFFu && storage_id != SUPPORTED_STORAGE_ID) {
        return MTP_RESP_INVALID_STORAGE_ID;
    }

    // 1. 调用底层执行物理擦除与空目录重建
    if (usbd_mtp_format_store() != 0) {
        return MTP_RESP_GENERAL_ERROR;
    }

    // 2. 彻底抹除旧的内存 MTP Context
    memset(handle_map, 0, sizeof(handle_map));
    
    // 3. 重新执行一次 BFS 目录树扫描（载入你刚新建的空目录）
    scan_all_directories();

    return MTP_RESP_OK;
}

static int32_t fs_set_object_prop_value(tud_mtp_cb_data_t* cb_data) {
    mtp_container_info_t* io_container = &cb_data->io_container;
    if (cb_data->phase == MTP_PHASE_COMMAND) {
        tud_mtp_data_receive(io_container);
    } else if (cb_data->phase == MTP_PHASE_DATA) {
        uint32_t obj_handle = cb_data->command_container->params[0];
        uint32_t prop_code  = cb_data->command_container->params[1];
        
        if (prop_code == 0xDC07) { 
            mtp_obj_t* f = get_obj(obj_handle);
            if (!f) return MTP_RESP_INVALID_OBJECT_HANDLE;
            
            uint8_t* str_buf = io_container->payload;
            uint32_t payload_len = io_container->payload_bytes;
            
            // 防止空包
            if (payload_len < 1) return MTP_RESP_GENERAL_ERROR;

            uint8_t char_count = str_buf[0];
            
            uint8_t* utf16_bytes = str_buf + 1;
            
            char new_name[64] = {0};
            utf16le_to_utf8(utf16_bytes, char_count, new_name, sizeof(new_name));
            new_name[63] = '\0';
            // =========================================================
            
            char new_path[MAX_PATH_LEN];
            build_path_from_handle(f->parent, new_name, new_path, sizeof(new_path));
            
            usbd_mtp_rename(f->path, new_path); 
            //usbd_mtp_notify_object_remove(f->path);
            
            strncpy(f->name, new_name, sizeof(f->name));
            strncpy(f->path, new_path, sizeof(f->path));
            
            queue_pending_event(f->handle, MTP_EVENT_OBJECT_INFO_CHANGED);
            return 0; 
        }
    }
    return 0;
}

static int32_t fs_get_obj_props_supported(tud_mtp_cb_data_t* cb_data) {
    mtp_container_info_t* io_container = &cb_data->io_container;
    uint16_t props[] = {
        0xDC01, 0xDC02, 0xDC04, 0xDC07, 0xDC0B, 0xDC41
    };
    uint32_t count = sizeof(props) / sizeof(props[0]);

    (void) mtp_container_add_uint32(io_container, count);
    for(uint32_t i = 0; i < count; i++) {
        (void) mtp_container_add_uint16(io_container, props[i]);
    }
    tud_mtp_data_send(io_container);
    return 0;
}

static int32_t fs_get_obj_prop_desc(tud_mtp_cb_data_t* cb_data) {
    mtp_container_info_t* io_container = &cb_data->io_container;
    const uint16_t prop_code = (uint16_t) cb_data->command_container->params[0];

    uint16_t datatype = 0x0000;
    uint8_t get_set = 0x00; 

    switch(prop_code) {
        case 0xDC01: datatype = MTP_DATA_TYPE_UINT32; break; 
        case 0xDC02: datatype = MTP_DATA_TYPE_UINT16; break; 
        case 0xDC04: datatype = 0x0008 /* UINT64 */; break; 
        case 0xDC07: datatype = MTP_DATA_TYPE_STR; get_set = 0x01; break; 
        case 0xDC0B: datatype = MTP_DATA_TYPE_UINT32; break; 
        case 0xDC41: datatype = 0x000A /* UINT128 */; break; 
        default: return MTP_RESP_OBJECT_PROP_NOT_SUPPORTED;
    }

    (void) mtp_container_add_uint16(io_container, prop_code);
    (void) mtp_container_add_uint16(io_container, datatype);
    (void) mtp_container_add_uint8(io_container, get_set);

    if (datatype == MTP_DATA_TYPE_STR) {
        (void) mtp_container_add_uint8(io_container, 0); 
    } else if (datatype == 0x000A) { 
        uint32_t zeros[4] = {0};
        (void) mtp_container_add_raw(io_container, zeros, 16);
    } else if (datatype == 0x0008) { 
        (void) mtp_container_add_uint64(io_container, 0);
    } else if (datatype == MTP_DATA_TYPE_UINT32) {
        (void) mtp_container_add_uint32(io_container, 0);
    } else if (datatype == MTP_DATA_TYPE_UINT16) {
        (void) mtp_container_add_uint16(io_container, 0);
    }

    (void) mtp_container_add_uint32(io_container, 0);
    (void) mtp_container_add_uint8(io_container, 0);
    tud_mtp_data_send(io_container);
    return 0;
}

static int32_t fs_get_obj_prop_value(tud_mtp_cb_data_t* cb_data) {
    mtp_container_info_t* io_container = &cb_data->io_container;
    uint32_t obj_handle = cb_data->command_container->params[0];
    uint16_t prop_code = (uint16_t) cb_data->command_container->params[1];

    mtp_obj_t* f = get_obj(obj_handle);
    if (!f) return MTP_RESP_INVALID_OBJECT_HANDLE;

    switch(prop_code) {
        case 0xDC01: (void) mtp_container_add_uint32(io_container, SUPPORTED_STORAGE_ID); break;
        case 0xDC02: (void) mtp_container_add_uint16(io_container, f->is_dir ? MTP_OBJ_FORMAT_ASSOCIATION : MTP_OBJ_FORMAT_UNDEFINED); break;
        case 0xDC04: (void) mtp_container_add_uint64(io_container, f->is_dir ? 0 : f->size); break; 
        case 0xDC07: mtp_container_add_utf8_string(io_container, f->name); break;
        case 0xDC0B: (void) mtp_container_add_uint32(io_container, f->parent); break;
        case 0xDC41: { 
            // 基于路径的稳定哈希，防止 Linux 误认重复
            uint32_t puid[4] = {0};
            uint32_t hash = 0;
            for (int i = 0; f->path[i] != '\0'; i++) hash = hash * 31 + f->path[i];
            puid[0] = hash;
            puid[1] = f->parent;
            (void) mtp_container_add_raw(io_container, puid, 16);
            break;
        }
        default: return MTP_RESP_OBJECT_PROP_NOT_SUPPORTED;
    }
    tud_mtp_data_send(io_container);
    return 0;
}

static int32_t fs_reset_device(tud_mtp_cb_data_t* cb_data) {
    (void) cb_data; // 该命令通常不需要解析参数

    // 1. 关闭可能因为意外中断而尚未关闭的底层文件
    if (active_fd >= 0) {
        usbd_mtp_close(active_fd);
        active_fd = -1;
    }

    // 2. 清除所有等待发送的异步事件
    queue_pending_event(0, 0);
    send_obj_handle = 0;

    // 3. 彻底重置 MTP 会话状态和内存映射
    is_session_opened = false;
    memset(handle_map, 0, sizeof(handle_map));
    usbd_mtp_device_reset();

    // 如果不硬重启，就老老实实回复 OK，让主机自己发 OpenSession 重新连接
    return MTP_RESP_OK;
}

//--------------------------------------------------------------------+
// Control Request callbacks
//--------------------------------------------------------------------+
bool tud_mtp_request_cancel_cb(tud_mtp_request_cb_data_t* cb_data) { return true; }
bool tud_mtp_request_device_reset_cb(tud_mtp_request_cb_data_t* cb_data) { return true; }
int32_t tud_mtp_request_get_extended_event_cb(tud_mtp_request_cb_data_t* cb_data) { return false; }
int32_t tud_mtp_request_get_device_status_cb(tud_mtp_request_cb_data_t* cb_data) {
  uint16_t* buf16 = (uint16_t*)(uintptr_t) cb_data->buf;
  buf16[0] = 4; buf16[1] = MTP_RESP_OK;
  return 4;
}

typedef int32_t (*fs_op_handler_t)(tud_mtp_cb_data_t* cb_data);
typedef struct {
  uint32_t op_code;
  fs_op_handler_t handler;
} fs_op_handler_dict_t;

static const fs_op_handler_dict_t fs_op_handler_dict[] = {
  { MTP_OP_GET_DEVICE_INFO,       fs_get_device_info    },
  { MTP_OP_OPEN_SESSION,          fs_open_close_session },
  { MTP_OP_CLOSE_SESSION,         fs_open_close_session },
  { MTP_OP_GET_STORAGE_IDS,       fs_get_storage_ids       },
  { MTP_OP_GET_STORAGE_INFO,      fs_get_storage_info      },
  { MTP_OP_GET_DEVICE_PROP_DESC,  fs_get_device_properties  },
  { MTP_OP_GET_DEVICE_PROP_VALUE, fs_get_device_properties },
  { MTP_OP_GET_OBJECT_HANDLES,    fs_get_object_handles    },
  { MTP_OP_GET_OBJECT_INFO,       fs_get_object_info       },
  { MTP_OP_GET_OBJECT,            fs_get_object            },
  { MTP_OP_DELETE_OBJECT,         fs_delete_object         },
  { MTP_OP_SEND_OBJECT_INFO,      fs_send_object_info      },
  { MTP_OP_SEND_OBJECT,           fs_send_object           },
  { MTP_OP_MOVE_OBJECT,           fs_move_object           },
  { MTP_OP_COPY_OBJECT,           fs_copy_object           },
  { MTP_OP_FORMAT_STORE,          fs_format_store          },
  { MTP_OP_RESET_DEVICE,          fs_reset_device          },
  { MTP_OP_GET_OBJECT_PROPS_SUPPORTED, fs_get_obj_props_supported },
  { MTP_OP_GET_OBJECT_PROP_DESC,       fs_get_obj_prop_desc       },
  { MTP_OP_GET_OBJECT_PROP_VALUE,      fs_get_obj_prop_value      },
  { MTP_OP_SET_OBJECT_PROP_VALUE,      fs_set_object_prop_value   },
};

// 统一的指令分发器
static int32_t dispatch_mtp_command(tud_mtp_cb_data_t* cb_data) {
    const mtp_container_command_t* command = cb_data->command_container;
    fs_op_handler_t handler = NULL;
    for (size_t i = 0; i < TU_ARRAY_SIZE(fs_op_handler_dict); i++) {
        if (fs_op_handler_dict[i].op_code == command->header.code) {
            handler = fs_op_handler_dict[i].handler; break;
        }
    }
    return (handler == NULL) ? MTP_RESP_OPERATION_NOT_SUPPORTED : handler(cb_data);
}

// 接收到 Command 阶段的回调
int32_t tud_mtp_command_received_cb(tud_mtp_cb_data_t* cb_data) {
    int32_t resp_code = dispatch_mtp_command(cb_data);
    if (resp_code > MTP_RESP_UNDEFINED) {
        cb_data->io_container.header->code = (uint16_t)resp_code;
        tud_mtp_response_send(&cb_data->io_container);
    }
    return resp_code;
}

// 接收到 Data 阶段的回调
int32_t tud_mtp_data_xfer_cb(tud_mtp_cb_data_t* cb_data) {
    int32_t resp_code = dispatch_mtp_command(cb_data);
    if (resp_code > MTP_RESP_UNDEFINED) {
        cb_data->io_container.header->code = (uint16_t)resp_code;
        tud_mtp_response_send(&cb_data->io_container);
    }
    return 0; // 注意：TinyUSB 要求此函数固定返回 0
}

int32_t tud_mtp_data_complete_cb(tud_mtp_cb_data_t* cb_data) {
  const mtp_container_command_t* command = cb_data->command_container;
  mtp_container_info_t* resp = &cb_data->io_container;
  switch (command->header.code) {
    case MTP_OP_SEND_OBJECT_INFO: {
      mtp_obj_t* f = get_obj(send_obj_handle);
      if (f == NULL) {
        resp->header->code = MTP_RESP_GENERAL_ERROR; break;
      }
      (void) mtp_container_add_uint32(resp, SUPPORTED_STORAGE_ID);
      (void) mtp_container_add_uint32(resp, f->parent);
      (void) mtp_container_add_uint32(resp, send_obj_handle);
      resp->header->code = MTP_RESP_OK;
      break;
    }
    case MTP_OP_SET_OBJECT_PROP_VALUE:
      resp->header->code = MTP_RESP_OK; break;
    default:
      resp->header->code = (cb_data->xfer_result == XFER_RESULT_SUCCESS) ? MTP_RESP_OK : MTP_RESP_GENERAL_ERROR; break;
  }
  tud_mtp_response_send(resp);
  return 0;
}

int32_t tud_mtp_response_complete_cb(tud_mtp_cb_data_t* cb_data)
{
    if (pending_event_handle != 0) {
        notify_host_object_event(pending_event_handle, pending_event_code);
        pending_event_handle = 0;
        pending_event_code = 0;
    }
    return 0;
}