/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "config_document.h"

#include "file_system.h"
#include "keyboard.h"
#include "layer.h"
#include "rgb.h"
#include "string.h"

static bool write_exact(File *file, const void *data, size_t size)
{
    return fs_write(file, (void *)data, size) == size;
}

static bool read_exact(File *file, void *data, size_t size)
{
    return fs_read(file, data, size) == size;
}

static bool skip_bytes(File *file, uint32_t size)
{
    return fs_seek(file, (FilePosition)size, FS_SEEK_CUR) >= 0;
}

static bool write_section_header(File *file, uint16_t type, uint32_t length)
{
    AmpConfigSectionHeader header = {
        .section_type = type,
        .section_version = 1,
        .section_length = length,
    };
    return write_exact(file, &header, sizeof(header));
}

static bool write_record_header(File *file, uint16_t size, uint32_t count)
{
    AmpRecordArrayHeader header = {
        .record_version = 1,
        .record_size = size,
        .record_count = count,
    };
    return write_exact(file, &header, sizeof(header));
}

static uint32_t config_section_count(void)
{
    uint32_t count = 3;
#ifdef RGB_ENABLE
    count++;
#endif
#ifdef DYNAMICKEY_ENABLE
    count++;
#endif
#ifdef MACRO_ENABLE
    count++;
#endif
    return count;
}

int amp_config_document_write(const char *path, uint16_t profile_id)
{
    File file;
    if (path == NULL || fs_open(&file, path, FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC) < 0)
    {
        return -1;
    }

    bool ok = true;
    AmpConfigDocumentHeader document = {
        .magic = AMP_CONFIG_DOCUMENT_MAGIC,
        .schema_version = AMP_CONFIG_SCHEMA_VERSION,
        .profile_id = profile_id,
        .section_count = config_section_count(),
    };
    ok = write_exact(&file, &document, sizeof(document));

    AmpConfigGeneral general = {
        .flags = (uint8_t)(g_keyboard_config.raw &
            (uint8_t)~((1U << KEYBOARD_CONFIG_DEBUG) |
                       (1U << KEYBOARD_CONFIG_CONSOLED))),
    };
    ok = ok && write_section_header(&file, AMP_CONFIG_SECTION_GENERAL,
                                    sizeof(general));
    ok = ok && write_exact(&file, &general, sizeof(general));

    const uint32_t advanced_length = sizeof(AmpRecordArrayHeader) +
        sizeof(AdvancedKeyConfiguration) * ADVANCED_KEY_NUM;
    ok = ok && write_section_header(&file, AMP_CONFIG_SECTION_ADVANCED_KEYS,
                                    advanced_length);
    ok = ok && write_record_header(&file, sizeof(AdvancedKeyConfiguration),
                                   ADVANCED_KEY_NUM);
    for (uint16_t i = 0; ok && i < ADVANCED_KEY_NUM; i++)
    {
        ok = write_exact(&file, &g_keyboard_advanced_keys[i].config,
                         sizeof(AdvancedKeyConfiguration));
    }

    const uint32_t keymap_count = (uint32_t)LAYER_NUM * TOTAL_KEY_NUM;
    const uint32_t keymap_length = sizeof(AmpRecordArrayHeader) +
                                   keymap_count * sizeof(uint16_t);
    ok = ok && write_section_header(&file, AMP_CONFIG_SECTION_KEYMAP,
                                    keymap_length);
    ok = ok && write_record_header(&file, sizeof(uint16_t), keymap_count);
    ok = ok && write_exact(&file, g_keymap, sizeof(g_keymap));

#ifdef RGB_ENABLE
    const uint32_t rgb_length = sizeof(AmpRgbBaseWire) +
        sizeof(AmpRecordArrayHeader) + sizeof(AmpRgbWire) * RGB_NUM;
    AmpRgbBaseWire rgb_base = {
        .mode = (uint8_t)g_rgb_base_config.mode,
        .r = g_rgb_base_config.rgb.r,
        .g = g_rgb_base_config.rgb.g,
        .b = g_rgb_base_config.rgb.b,
        .secondary_r = g_rgb_base_config.secondary_rgb.r,
        .secondary_g = g_rgb_base_config.secondary_rgb.g,
        .secondary_b = g_rgb_base_config.secondary_rgb.b,
        .speed = g_rgb_base_config.speed,
        .direction = g_rgb_base_config.direction,
        .density = g_rgb_base_config.density,
        .brightness = g_rgb_base_config.brightness,
    };
    ok = ok && write_section_header(&file, AMP_CONFIG_SECTION_RGB, rgb_length);
    ok = ok && write_exact(&file, &rgb_base, sizeof(rgb_base));
    ok = ok && write_record_header(&file, sizeof(AmpRgbWire), RGB_NUM);
    for (uint16_t key_index = 0; ok && key_index < RGB_NUM; key_index++)
    {
        uint16_t rgb_index = g_rgb_inverse_mapping[key_index];
        AmpRgbWire item = {0};
        if (rgb_index < RGB_NUM)
        {
            item.mode = (uint8_t)g_rgb_configs[rgb_index].mode;
            item.r = g_rgb_configs[rgb_index].rgb.r;
            item.g = g_rgb_configs[rgb_index].rgb.g;
            item.b = g_rgb_configs[rgb_index].rgb.b;
            item.speed = g_rgb_configs[rgb_index].speed;
        }
        ok = write_exact(&file, &item, sizeof(item));
    }
#endif

#ifdef DYNAMICKEY_ENABLE
    const uint32_t dynamic_length = (sizeof(AmpDynamicKeyRecordHeader) +
                                     sizeof(DynamicKey)) * DYNAMIC_KEY_NUM;
    ok = ok && write_section_header(&file, AMP_CONFIG_SECTION_DYNAMIC_KEYS,
                                    dynamic_length);
    for (uint16_t i = 0; ok && i < DYNAMIC_KEY_NUM; i++)
    {
        AmpDynamicKeyRecordHeader record = {
            .key_index = i,
            .dynamic_type = (uint16_t)g_dynamic_keys[i].type,
            .record_version = 1,
            .payload_len = sizeof(DynamicKey),
        };
        ok = write_exact(&file, &record, sizeof(record));
        ok = ok && write_exact(&file, &g_dynamic_keys[i], sizeof(DynamicKey));
    }
#endif

#ifdef MACRO_ENABLE
    const uint32_t macro_count = (uint32_t)MACRO_NUM * MACRO_MAX_ACTIONS;
    const uint32_t macro_length = sizeof(AmpRecordArrayHeader) +
                                  sizeof(AmpMacroActionWire) * macro_count;
    ok = ok && write_section_header(&file, AMP_CONFIG_SECTION_MACROS,
                                    macro_length);
    ok = ok && write_record_header(&file, sizeof(AmpMacroActionWire), macro_count);
    for (uint16_t macro = 0; ok && macro < MACRO_NUM; macro++)
    {
        for (uint16_t index = 0; ok && index < MACRO_MAX_ACTIONS; index++)
        {
            MacroAction *action = &g_macros[macro].actions[index];
            AmpMacroActionWire wire = {
                .delay = action->delay,
                .key_id = action->event.key == NULL ? 0 :
                          ((Key *)action->event.key)->id,
                .is_virtual = action->event.is_virtual,
                .event = action->event.event,
                .keycode = action->event.keycode,
            };
            ok = write_exact(&file, &wire, sizeof(wire));
        }
    }
#endif

    if (ok)
    {
        ok = fs_sync(&file) >= 0;
    }
    if (fs_close(&file) < 0)
    {
        ok = false;
    }
    return ok ? 0 : -1;
}

static AmpStatus process_general(File *file, uint32_t length, bool apply)
{
    AmpConfigGeneral general;
    if (length < sizeof(general) || !read_exact(file, &general, sizeof(general)) ||
        !skip_bytes(file, length - sizeof(general)))
    {
        return AMP_STATUS_INTEGRITY_ERROR;
    }
    if (apply)
    {
        const uint8_t session_bits = g_keyboard_config.raw &
            ((1U << KEYBOARD_CONFIG_DEBUG) | (1U << KEYBOARD_CONFIG_CONSOLED));
        g_keyboard_config.raw = (uint8_t)((general.flags &
            (uint8_t)~((1U << KEYBOARD_CONFIG_DEBUG) |
                       (1U << KEYBOARD_CONFIG_CONSOLED))) | session_bits);
    }
    return AMP_STATUS_OK;
}

static AmpStatus read_array_header(File *file, uint32_t section_length,
                                   uint16_t expected_size,
                                   uint32_t expected_count,
                                   AmpRecordArrayHeader *result)
{
    AmpRecordArrayHeader header;
    if (section_length < sizeof(header) || !read_exact(file, &header, sizeof(header)) ||
        header.record_version < 1 || header.record_size < expected_size ||
        header.record_count != expected_count)
    {
        return AMP_STATUS_INTEGRITY_ERROR;
    }
    if (section_length != sizeof(header) +
                          (uint32_t)header.record_size * header.record_count)
    {
        return AMP_STATUS_INTEGRITY_ERROR;
    }
    if (result != NULL)
    {
        *result = header;
    }
    return AMP_STATUS_OK;
}

static AmpStatus process_advanced(File *file, uint32_t length, bool apply)
{
    AmpRecordArrayHeader header;
    AmpStatus status = read_array_header(file, length,
        sizeof(AdvancedKeyConfiguration), ADVANCED_KEY_NUM, &header);
    if (status != AMP_STATUS_OK)
    {
        return status;
    }
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKeyConfiguration config;
        if (!read_exact(file, &config, sizeof(config)))
        {
            return AMP_STATUS_INTEGRITY_ERROR;
        }
        if (apply)
        {
            memcpy(&g_keyboard_advanced_keys[i].config, &config, sizeof(config));
            advanced_key_set_range(&g_keyboard_advanced_keys[i], config.upper_bound,
                                   config.lower_bound);
        }
        if (!skip_bytes(file, header.record_size - sizeof(config)))
        {
            return AMP_STATUS_INTEGRITY_ERROR;
        }
    }
    return AMP_STATUS_OK;
}

static AmpStatus process_keymap(File *file, uint32_t length, bool apply)
{
    const uint32_t count = (uint32_t)LAYER_NUM * TOTAL_KEY_NUM;
    AmpRecordArrayHeader header;
    AmpStatus status = read_array_header(file, length, sizeof(uint16_t), count,
                                         &header);
    if (status != AMP_STATUS_OK)
    {
        return status;
    }
    uint16_t *keymap = (uint16_t *)g_keymap;
    for (uint32_t i = 0; i < count; i++)
    {
        uint16_t keycode;
        if (!read_exact(file, &keycode, sizeof(keycode)) ||
            !skip_bytes(file, header.record_size - sizeof(keycode)))
        {
            return AMP_STATUS_INTEGRITY_ERROR;
        }
        if (apply)
        {
            keymap[i] = keycode;
        }
    }
    if (apply)
    {
        layer_cache_refresh();
    }
    return AMP_STATUS_OK;
}

#ifdef RGB_ENABLE
static AmpStatus process_rgb(File *file, uint32_t length, bool apply)
{
    AmpRgbBaseWire base;
    if (length < sizeof(base) + sizeof(AmpRecordArrayHeader) ||
        !read_exact(file, &base, sizeof(base)))
    {
        return AMP_STATUS_INTEGRITY_ERROR;
    }
    AmpRecordArrayHeader header;
    AmpStatus status = read_array_header(file, length - sizeof(base),
                                         sizeof(AmpRgbWire), RGB_NUM, &header);
    if (status != AMP_STATUS_OK)
    {
        return status;
    }
    if (apply)
    {
        g_rgb_base_config.mode = (RGBBaseMode)base.mode;
        g_rgb_base_config.rgb = (ColorRGB){base.r, base.g, base.b};
        rgb_to_hsv(&g_rgb_base_config.hsv, &g_rgb_base_config.rgb);
        g_rgb_base_config.secondary_rgb =
            (ColorRGB){base.secondary_r, base.secondary_g, base.secondary_b};
        rgb_to_hsv(&g_rgb_base_config.secondary_hsv,
                   &g_rgb_base_config.secondary_rgb);
        g_rgb_base_config.speed = base.speed;
        g_rgb_base_config.direction = base.direction;
        g_rgb_base_config.density = base.density;
        g_rgb_base_config.brightness = base.brightness;
        g_rgb_base_config.begin_tick = 0;
    }
    for (uint16_t key_index = 0; key_index < RGB_NUM; key_index++)
    {
        AmpRgbWire item;
        if (!read_exact(file, &item, sizeof(item)))
        {
            return AMP_STATUS_INTEGRITY_ERROR;
        }
        if (!skip_bytes(file, header.record_size - sizeof(item)))
        {
            return AMP_STATUS_INTEGRITY_ERROR;
        }
        uint16_t rgb_index = g_rgb_inverse_mapping[key_index];
        if (apply && rgb_index < RGB_NUM)
        {
            g_rgb_configs[rgb_index].mode = (RGBMode)item.mode;
            g_rgb_configs[rgb_index].rgb = (ColorRGB){item.r, item.g, item.b};
            rgb_to_hsv(&g_rgb_configs[rgb_index].hsv,
                       &g_rgb_configs[rgb_index].rgb);
            g_rgb_configs[rgb_index].speed = item.speed;
            g_rgb_configs[rgb_index].begin_tick = 0;
        }
    }
    return AMP_STATUS_OK;
}
#endif

#ifdef DYNAMICKEY_ENABLE
static AmpStatus process_dynamic(File *file, uint32_t length, bool apply)
{
    uint32_t consumed = 0;
    while (consumed < length)
    {
        AmpDynamicKeyRecordHeader header;
        if (length - consumed < sizeof(header) ||
            !read_exact(file, &header, sizeof(header)))
        {
            return AMP_STATUS_INTEGRITY_ERROR;
        }
        consumed += sizeof(header);
        if (header.payload_len > length - consumed)
        {
            return AMP_STATUS_INTEGRITY_ERROR;
        }
        if (header.record_version == 1 && header.payload_len >= sizeof(DynamicKey) &&
            header.key_index < DYNAMIC_KEY_NUM && header.dynamic_type < DYNAMIC_KEY_TYPE_NUM)
        {
            DynamicKey value;
            if (!read_exact(file, &value, sizeof(value)) ||
                value.type != header.dynamic_type)
            {
                return AMP_STATUS_INTEGRITY_ERROR;
            }
            if (apply)
            {
                memcpy(&g_dynamic_keys[header.key_index], &value, sizeof(value));
            }
            if (!skip_bytes(file, header.payload_len - sizeof(value)))
            {
                return AMP_STATUS_INTEGRITY_ERROR;
            }
        }
        else if (!skip_bytes(file, header.payload_len))
        {
            return AMP_STATUS_INTEGRITY_ERROR;
        }
        consumed += header.payload_len;
    }
    return consumed == length ? AMP_STATUS_OK : AMP_STATUS_INTEGRITY_ERROR;
}
#endif

#ifdef MACRO_ENABLE
static AmpStatus process_macros(File *file, uint32_t length, bool apply)
{
    const uint32_t count = (uint32_t)MACRO_NUM * MACRO_MAX_ACTIONS;
    AmpRecordArrayHeader header;
    AmpStatus status = read_array_header(file, length,
                                        sizeof(AmpMacroActionWire), count,
                                        &header);
    if (status != AMP_STATUS_OK)
    {
        return status;
    }
    for (uint16_t macro = 0; macro < MACRO_NUM; macro++)
    {
        for (uint16_t index = 0; index < MACRO_MAX_ACTIONS; index++)
        {
            AmpMacroActionWire wire;
            if (!read_exact(file, &wire, sizeof(wire)))
            {
                return AMP_STATUS_INTEGRITY_ERROR;
            }
            if (!skip_bytes(file, header.record_size - sizeof(wire)))
            {
                return AMP_STATUS_INTEGRITY_ERROR;
            }
            if (apply)
            {
                MacroAction *action = &g_macros[macro].actions[index];
                action->delay = wire.delay;
                action->event.keycode = wire.keycode;
                action->event.event = wire.event;
                action->event.is_virtual = wire.is_virtual != 0;
                action->event.key = wire.is_virtual ? NULL :
                                    keyboard_get_key(wire.key_id);
            }
        }
    }
    return AMP_STATUS_OK;
}
#endif

static AmpStatus walk_document(const char *path, uint16_t profile_id, bool apply)
{
    File file;
    if (path == NULL || fs_open(&file, path, FS_O_RDONLY) < 0)
    {
        return AMP_STATUS_NOT_FOUND;
    }
    AmpConfigDocumentHeader document;
    AmpStatus status = AMP_STATUS_OK;
    if (!read_exact(&file, &document, sizeof(document)) ||
        document.magic != AMP_CONFIG_DOCUMENT_MAGIC ||
        document.schema_version != AMP_CONFIG_SCHEMA_VERSION ||
        document.profile_id != profile_id)
    {
        status = AMP_STATUS_INTEGRITY_ERROR;
    }

    for (uint32_t i = 0; status == AMP_STATUS_OK && i < document.section_count; i++)
    {
        AmpConfigSectionHeader section;
        if (!read_exact(&file, &section, sizeof(section)))
        {
            status = AMP_STATUS_INTEGRITY_ERROR;
            break;
        }
        if (section.section_version != 1)
        {
            status = skip_bytes(&file, section.section_length) ? AMP_STATUS_OK :
                                                               AMP_STATUS_INTEGRITY_ERROR;
            continue;
        }
        switch (section.section_type)
        {
        case AMP_CONFIG_SECTION_GENERAL:
            status = process_general(&file, section.section_length, apply);
            break;
        case AMP_CONFIG_SECTION_ADVANCED_KEYS:
            status = process_advanced(&file, section.section_length, apply);
            break;
        case AMP_CONFIG_SECTION_KEYMAP:
            status = process_keymap(&file, section.section_length, apply);
            break;
#ifdef RGB_ENABLE
        case AMP_CONFIG_SECTION_RGB:
            status = process_rgb(&file, section.section_length, apply);
            break;
#endif
#ifdef DYNAMICKEY_ENABLE
        case AMP_CONFIG_SECTION_DYNAMIC_KEYS:
            status = process_dynamic(&file, section.section_length, apply);
            break;
#endif
#ifdef MACRO_ENABLE
        case AMP_CONFIG_SECTION_MACROS:
            status = process_macros(&file, section.section_length, apply);
            break;
#endif
        default:
            status = skip_bytes(&file, section.section_length) ? AMP_STATUS_OK :
                                                               AMP_STATUS_INTEGRITY_ERROR;
            break;
        }
    }
    if (fs_close(&file) < 0 && status == AMP_STATUS_OK)
    {
        status = AMP_STATUS_IO_ERROR;
    }
    return status;
}

AmpStatus amp_config_document_validate(const char *path, uint16_t profile_id)
{
    return walk_document(path, profile_id, false);
}

AmpStatus amp_config_document_apply(const char *path, uint16_t profile_id)
{
    AmpStatus status = amp_config_document_validate(path, profile_id);
    return status == AMP_STATUS_OK ? walk_document(path, profile_id, true) : status;
}
