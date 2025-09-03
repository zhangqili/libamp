/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "rgb.h"
#include "keyboard_def.h"
#include "string.h"
#include "math.h"
#include "driver.h"

#define rgb_loop_queue_foreach(q, type, item) for (uint16_t __index = (q)->front; __index != (q)->rear; __index = (__index + 1) % (q)->len)\
                                              for (type *item = &((q)->data[__index]); item; item = NULL)

#define MANHATTAN_DISTANCE(m, n) (fabsf((m)->x - (n)->x) + fabsf((m)->y - (n)->y))
#define MANHATTAN_DISTANCE_DIRECT(x1, y1, x2, y2) (fabsf((x1) - (x2)) + fabsf((y1) - (y2)))
#define EUCLIDEAN_DISTANCE(m, n) sqrtf(((m)->x - (n)->x)*((m)->x - (n)->x) + ((m)->y - (n)->y)*((m)->y - (n)->y))

__WEAK const uint8_t g_rgb_mapping[ADVANCED_KEY_NUM];
__WEAK const RGBLocation g_rgb_locations[RGB_NUM];

volatile bool g_rgb_hid_mode;
RGBBaseConfig g_rgb_base_config;
RGBConfig g_rgb_configs[RGB_NUM];
ColorRGB g_rgb_colors[RGB_NUM];

#ifdef RGB_USE_LIST_EXPERIMENTAL
static RGBArgumentList rgb_argument_list;
static RGBArgumentListNode RGB_Argument_List_Buffer[RGB_ARGUMENT_BUFFER_LENGTH];
#endif
static RGBLoopQueue rgb_argument_queue;
static RGBLoopQueueElm RGB_Argument_Buffer[RGB_ARGUMENT_BUFFER_LENGTH];

void rgb_init(void)
{
#ifdef RGB_USE_LIST_EXPERIMENTAL
    rgb_forward_list_init(&rgb_argument_list, RGB_Argument_List_Buffer, RGB_ARGUMENT_BUFFER_LENGTH);
#endif
    rgb_loop_queue_init(&rgb_argument_queue, RGB_Argument_Buffer, RGB_ARGUMENT_BUFFER_LENGTH);
}

#define COLOR_INTERVAL(key, low, up) (uint8_t)((key) < 0 ? (low) : ((key) > ANALOG_VALUE_MAX ? (up) : (key) * (up)))

void rgb_update(void)
{
    if (!g_rgb_base_config.mode 
#ifdef SUSPEND_ENABLE
        || g_keyboard_is_suspend
#endif
    )
    {
        rgb_turn_off();
        return;
    }
    if (g_rgb_hid_mode)
    {
        return;
    }
    ColorHSV temp_hsv;
    ColorRGB temp_rgb;
    float intensity;
    memset(g_rgb_colors, 0, sizeof(g_rgb_colors));
    
    switch (g_rgb_base_config.mode)
    {
    case RGB_BASE_MODE_RAINBOW:
        {
            rgb_to_hsv(&temp_hsv, &g_rgb_base_config.rgb);
            float direction_c = g_rgb_base_config.direction * M_PI / 180;
            float direction_sin = sinf(direction_c);
            float direction_cos = cosf(direction_c);
            for (uint8_t i = 0; i < RGB_NUM; i++)
            {
                const RGBLocation* location = &g_rgb_locations[i];
                float vertical_distance = location->x * direction_cos + location->y * direction_sin;
                temp_hsv.h = ((uint32_t)(g_rgb_base_config.hsv.h + vertical_distance * g_rgb_base_config.density + g_keyboard_tick * g_rgb_base_config.speed)) % 360;
                color_set_hsv(&temp_rgb, &temp_hsv);
                color_mix(&g_rgb_colors[i], &temp_rgb);
            }
        }
        break;
    case RGB_BASE_MODE_WAVE:
        {
            float direction_c = g_rgb_base_config.direction * M_PI / 180;
            float direction_sin = sinf(direction_c);
            float direction_cos = cosf(direction_c);
            for (uint8_t i = 0; i < RGB_NUM; i++)
            {
                const RGBLocation* location = &g_rgb_locations[i];
                float vertical_distance = location->x * direction_cos + location->y * direction_sin;
                float intensity = fmodf(((vertical_distance * g_rgb_base_config.density + g_keyboard_tick * g_rgb_base_config.speed)  / 180), 2.0f);
                intensity -= 1.0f;
                float secondary_intensity;
                if (intensity<0)
                {
                    intensity = -intensity;
                }
                secondary_intensity = 1 - intensity;
                temp_rgb.r = (uint8_t)(intensity * ((float)(g_rgb_base_config.rgb.r)) + secondary_intensity * ((float)(g_rgb_base_config.secondary_rgb.r)));
                temp_rgb.g = (uint8_t)(intensity * ((float)(g_rgb_base_config.rgb.g)) + secondary_intensity * ((float)(g_rgb_base_config.secondary_rgb.g)));
                temp_rgb.b = (uint8_t)(intensity * ((float)(g_rgb_base_config.rgb.b)) + secondary_intensity * ((float)(g_rgb_base_config.secondary_rgb.b)));
                color_mix(&g_rgb_colors[i], &temp_rgb);
            }
        }
        break;
    default:
        break;
    }


#ifdef RGB_USE_LIST_EXPERIMENTAL
    rgb_loop_queue_foreach(&rgb_argument_queue, RGBLoopQueueElm, item)
    {
        rgb_forward_list_insert_after(&rgb_argument_list,&rgb_argument_list.data[rgb_argument_list.head], *item);
        rgb_loop_queue_pop(&rgb_argument_queue);
    }
    RGBArgumentListNode * last_node = &rgb_argument_list.data[rgb_argument_list.head];
    for (int16_t iterator = rgb_argument_list.data[rgb_argument_list.head].next; iterator >= 0;)
    {
        RGBArgumentListNode* node = &(rgb_argument_list.data[iterator]);
        RGBArgument * item = &(node->data);
#else
    rgb_loop_queue_foreach(&rgb_argument_queue, RGBLoopQueueElm, item)
    {
#endif
        RGBConfig *config = g_rgb_configs + item->rgb_ptr;
        RGBLocation *location = (RGBLocation *)&g_rgb_locations[item->rgb_ptr];
        uint32_t duration = g_keyboard_tick - item->begin_time;
        float distance = duration * config->speed;
#ifndef RGB_USE_LIST_EXPERIMENTAL
        if (duration > RGB_MAX_DURATION)
        {
            rgb_loop_queue_pop(&rgb_argument_queue);
        }
#endif
        if (MANHATTAN_DISTANCE_DIRECT(location->x, RGB_LEFT, location->y, RGB_TOP) < distance - 5 &&
            MANHATTAN_DISTANCE_DIRECT(location->x, RGB_LEFT, location->y, RGB_BOTTOM) < distance - 5 &&
            MANHATTAN_DISTANCE_DIRECT(location->x, RGB_RIGHT, location->y, RGB_TOP) < distance - 5 &&
            MANHATTAN_DISTANCE_DIRECT(location->x, RGB_RIGHT, location->y, RGB_BOTTOM) < distance - 5)
        // if (distance > 25)
        {
#ifdef RGB_USE_LIST_EXPERIMENTAL
            rgb_forward_list_erase_after(&rgb_argument_list, last_node);
            iterator = last_node->next;
#endif
            continue;
        }
        switch (config->mode)
        {
        case RGB_MODE_FIXED:
            break;
        case RGB_MODE_STATIC:
            break;
        case RGB_MODE_CYCLE:
            break;
        case RGB_MODE_LINEAR:
            break;
        case RGB_MODE_TRIGGER:
            break;
        case RGB_MODE_STRING:
        case RGB_MODE_FADING_STRING:
        case RGB_MODE_DIAMOND_RIPPLE:
        case RGB_MODE_FADING_DIAMOND_RIPPLE:
        case RGB_MODE_BUBBLE:
            for (int8_t j = 0; j < RGB_NUM; j++)
            {
                switch (config->mode)
                {
                case RGB_MODE_STRING:
                    intensity = (1.0f - fabsf(distance - fabsf(location->x - g_rgb_locations[j].x)));
                    intensity = intensity > 0 ? intensity : 0;
                    intensity = fabsf(location->y - g_rgb_locations[j].y) < 0.5 ? intensity : 0;
                    break;
                case RGB_MODE_FADING_STRING:
                    intensity = (distance - fabsf(location->x - g_rgb_locations[j].x));
                    if (intensity > 0)
                    {
                        intensity = FADING_DISTANCE - intensity > 0 ? FADING_DISTANCE - intensity : 0;
                        intensity /= FADING_DISTANCE;
                    }
                    else
                    {
                        intensity = 1.0f + intensity > 0 ? 1.0f + intensity : 0;
                    }
                    intensity = fabsf(location->y - g_rgb_locations[j].y) < 0.5 ? intensity : 0;
                    break;
                case RGB_MODE_DIAMOND_RIPPLE:
                    intensity = (1.0f - fabsf(distance - MANHATTAN_DISTANCE(location, g_rgb_locations + j)));
                    intensity = intensity > 0 ? intensity : 0;
                    break;
                case RGB_MODE_FADING_DIAMOND_RIPPLE:
                    intensity = (distance - MANHATTAN_DISTANCE(location, g_rgb_locations + j));
                    if (intensity > 0)
                    {
                        intensity = FADING_DISTANCE - intensity > 0 ? FADING_DISTANCE - intensity : 0;
                        intensity /= FADING_DISTANCE;
                        break;
                    }
                    else
                    {
                        intensity = 1.0f + intensity > 0 ? 1.0f + intensity : 0;
                    }
                    break;
                case RGB_MODE_BUBBLE:
                    {
                        float e_distance = EUCLIDEAN_DISTANCE(location, g_rgb_locations + j);
                        if (e_distance > BUBBLE_DISTANCE)
                        {
                            intensity = 0;
                            continue;
                        }
                        intensity = (distance - e_distance);
                        if (intensity > 0)
                        {
                            intensity = FADING_DISTANCE - intensity > 0 ? FADING_DISTANCE - intensity : 0;
                            intensity /= FADING_DISTANCE;
                            break;
                        }
                        else
                        {
                            intensity = 1.0f + intensity > 0 ? 1.0f + intensity : 0;
                        }
                    }
                    break;
                default:
                    intensity = 0;
                    break;
                }
                temp_rgb.r = ((uint8_t)(intensity * ((float)(config->rgb.r)))) >> 1;
                temp_rgb.g = ((uint8_t)(intensity * ((float)(config->rgb.g)))) >> 1;
                temp_rgb.b = ((uint8_t)(intensity * ((float)(config->rgb.b)))) >> 1;
                color_mix(&g_rgb_colors[j], &temp_rgb);
            }
            break;
        case RGB_MODE_JELLY:
            break;
        default:
            break;
        }
#ifdef RGB_USE_LIST_EXPERIMENTAL
        last_node = node;
        iterator = (&rgb_argument_list)->data[iterator].next;
#endif
    }
    for (uint8_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        uint8_t rgb_index = g_rgb_mapping[g_keyboard_advanced_keys[i].key.id];
        intensity = advanced_key_get_effective_value(&g_keyboard_advanced_keys[i]);
        switch (g_rgb_configs[rgb_index].mode)
        {
        case RGB_MODE_LINEAR:
            temp_rgb.r = COLOR_INTERVAL(intensity, 0, (float)(g_rgb_configs[rgb_index].rgb.r));
            temp_rgb.g = COLOR_INTERVAL(intensity, 0, (float)(g_rgb_configs[rgb_index].rgb.g));
            temp_rgb.b = COLOR_INTERVAL(intensity, 0, (float)(g_rgb_configs[rgb_index].rgb.b));
            color_mix(&g_rgb_colors[rgb_index], &temp_rgb);
            break;
        case RGB_MODE_TRIGGER:
            if (g_keyboard_advanced_keys[i].key.report_state)
            {
                g_rgb_configs[rgb_index].begin_time = g_keyboard_tick;
            }
            intensity = powf(1 - g_rgb_configs[rgb_index].speed, g_keyboard_tick - g_rgb_configs[rgb_index].begin_time);
            temp_rgb.r = (uint8_t)((float)(g_rgb_configs[rgb_index].rgb.r) * intensity);
            temp_rgb.g = (uint8_t)((float)(g_rgb_configs[rgb_index].rgb.g) * intensity);
            temp_rgb.b = (uint8_t)((float)(g_rgb_configs[rgb_index].rgb.b) * intensity);
            color_mix(&g_rgb_colors[rgb_index], &temp_rgb);
            break;
        case RGB_MODE_FIXED:
            color_set_rgb(&g_rgb_colors[rgb_index], &g_rgb_configs[rgb_index].rgb);
            break;
        case RGB_MODE_STATIC:
            color_mix(&g_rgb_colors[rgb_index], &g_rgb_configs[rgb_index].rgb);
            break;
        case RGB_MODE_CYCLE:
            temp_hsv.s = g_rgb_configs[rgb_index].hsv.s;
            temp_hsv.v = g_rgb_configs[rgb_index].hsv.v;
            temp_hsv.h = (uint16_t)(g_rgb_configs[rgb_index].hsv.h + (g_keyboard_tick % (uint16_t)(360 / g_rgb_configs[rgb_index].speed)) * g_rgb_configs[rgb_index].speed) % 360;
            color_set_hsv(&temp_rgb, &temp_hsv);
            color_mix(&g_rgb_colors[rgb_index], &temp_rgb);
            break;
        case RGB_MODE_JELLY:
            for (int8_t j = 0; j < ADVANCED_KEY_NUM; j++)
            {
                intensity = (JELLY_DISTANCE * advanced_key_get_effective_value(&g_keyboard_advanced_keys[i]) - MANHATTAN_DISTANCE(&g_rgb_locations[j], &g_rgb_locations[rgb_index]));
                intensity = intensity > 0 ? intensity > 1 ? 1 : intensity : 0;

                temp_rgb.r = ((uint8_t)(intensity * ((float)(g_rgb_configs[j].rgb.r)))) >> 1;
                temp_rgb.g = ((uint8_t)(intensity * ((float)(g_rgb_configs[j].rgb.g)))) >> 1;
                temp_rgb.b = ((uint8_t)(intensity * ((float)(g_rgb_configs[j].rgb.b)))) >> 1;
                color_mix(&g_rgb_colors[j], &temp_rgb);
            }
            break;
        default:
            break;
        }
    }
    rgb_flush();
}

__WEAK void rgb_update_callback(void)
{

}

void rgb_set(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
#ifdef RGB_GAMMA_ENABLE
    r = GAMMA_CORRECT(r, 255)*(g_rgb_base_config.brightness / 255.0f) + 0.5;
    g = GAMMA_CORRECT(g, 255)*(g_rgb_base_config.brightness / 255.0f) + 0.5;
    b = GAMMA_CORRECT(b, 255)*(g_rgb_base_config.brightness / 255.0f) + 0.5;
#else
    r = (r * g_rgb_base_config.brightness) >> 8;
    g = (g * g_rgb_base_config.brightness) >> 8;
    b = (b * g_rgb_base_config.brightness) >> 8;
#endif
    led_set(index, r, g, b);
}

void rgb_init_flash(void)
{
    float intensity;
    ColorRGB temp_rgb;
    uint32_t begin_time = g_keyboard_tick;
    RGBLocation location = PORT_LOCATION;
    bool animation_playing = false;
    while (g_keyboard_tick - begin_time < RGB_FLASH_MAX_DURATION)
    {
        float distance = (g_keyboard_tick - begin_time) * RGB_FLASH_RIPPLE_SPEED;
        memset(g_rgb_colors, 0, sizeof(g_rgb_colors));
        animation_playing = false;
        for (int8_t i = 0; i < RGB_NUM; i++)
        {
            //rgb_flash();
            intensity = (distance - sqrtf((location.x - g_rgb_locations[i].x) * (location.x - g_rgb_locations[i].x) +
                                          (location.y - g_rgb_locations[i].y) * (location.y - g_rgb_locations[i].y)));
                                          
            //intensity = (distance - MANHATTAN_DISTANCE(&location, g_rgb_locations+i));
            if (intensity > 0)
            {
                intensity = 10 - intensity > 0 ? 10 - intensity : 0;
                intensity /= 10;
            }
            else
            {
                intensity = 1.0f + intensity > 0 ? 1.0f + intensity : 0;
            }
            if (intensity > 0 || distance < 4)
            {
                animation_playing = true;
            }
            
            temp_rgb.r = intensity * 255;
            temp_rgb.g = intensity * 255;
            temp_rgb.b = intensity * 255;
            color_mix(&g_rgb_colors[i], &temp_rgb);
        }
        if (!animation_playing)
        {
            break;
        }
        for (uint8_t i = 0; i < RGB_NUM; i++)
        {
            rgb_set(i, g_rgb_colors[i].r, g_rgb_colors[i].g, g_rgb_colors[i].b);
        }
    }
    rgb_turn_off();
}

void rgb_flash(void)
{
    float intensity;
    ColorRGB temp_rgb;
    uint32_t begin_time = g_keyboard_tick;
    while (g_keyboard_tick - begin_time < RGB_FLASH_MAX_DURATION)
    {
        float distance = (g_keyboard_tick - begin_time);
        memset(g_rgb_colors, 0, sizeof(g_rgb_colors));
        intensity = (RGB_FLASH_MAX_DURATION/2 - fabsf(distance - (RGB_FLASH_MAX_DURATION/2)))/((float)(RGB_FLASH_MAX_DURATION/2));
        for (int8_t i = 0; i < RGB_NUM; i++)
        {
            temp_rgb.r = (intensity * 255);
            temp_rgb.g = (intensity * 255);
            temp_rgb.b = (intensity * 255);
            color_mix(&g_rgb_colors[i], &temp_rgb);
        }
        for (uint8_t i = 0; i < RGB_NUM; i++)
        {
            rgb_set(i, g_rgb_colors[i].r, g_rgb_colors[i].g, g_rgb_colors[i].b);
        }
    }
    rgb_turn_off();
}

void rgb_turn_off(void)
{
    for (uint8_t i = 0; i < RGB_NUM; i++)
    {
        rgb_set(i, 0, 0, 0);
    }
}

void rgb_factory_reset(void)
{
    g_rgb_base_config.mode = RGB_BASE_MODE_BLANK;
    g_rgb_base_config.brightness = 255;
    g_rgb_base_config.density = 32;
    g_rgb_base_config.direction = 0;
    g_rgb_base_config.speed = RGB_DEFAULT_SPEED;
    ColorHSV temphsv = RGB_DEFAULT_COLOR_HSV;
    g_rgb_base_config.hsv = temphsv;
    color_set_hsv(&g_rgb_base_config.rgb, &temphsv);
    memset(&g_rgb_base_config.secondary_rgb,0,sizeof(g_rgb_base_config.secondary_rgb));
    memset(&g_rgb_base_config.secondary_hsv,0,sizeof(g_rgb_base_config.secondary_hsv));
    for (uint8_t i = 0; i < RGB_NUM; i++)
    {
        g_rgb_configs[i].mode = RGB_DEFAULT_MODE;
        g_rgb_configs[i].hsv = temphsv;
        g_rgb_configs[i].speed = RGB_DEFAULT_SPEED;
        color_set_hsv(&g_rgb_configs[i].rgb, &temphsv);
    }
}

void rgb_flush(void)
{
    rgb_update_callback();
    for (uint8_t i = 0; i < RGB_NUM; i++)
    {
        rgb_set(i, g_rgb_colors[i].r, g_rgb_colors[i].g, g_rgb_colors[i].b);
    }
}

void rgb_activate(uint16_t id)
{
    if (id >= RGB_NUM)
    {
        return;
    }
    RGBArgument a;
    a.rgb_ptr = g_rgb_mapping[id];
    a.begin_time = g_keyboard_tick;
    switch (g_rgb_configs[a.rgb_ptr].mode)
    {
    case RGB_MODE_STRING:
    case RGB_MODE_FADING_STRING:
    case RGB_MODE_DIAMOND_RIPPLE:
    case RGB_MODE_FADING_DIAMOND_RIPPLE:
    case RGB_MODE_BUBBLE:
        rgb_loop_queue_push(&rgb_argument_queue, a);
        break;
    default:
        break;
    }
}

void rgb_loop_queue_init(RGBLoopQueue *q, RGBLoopQueueElm *data, uint16_t len)
{
    q->data = data;
    q->front = 0;
    q->rear = 0;
    q->len = len;
}

RGBLoopQueueElm rgb_loop_queue_pop(RGBLoopQueue *q)
{
    RGBArgument a = {0, 0};
    if (q->front == q->rear)
        return a;
    q->front = (q->front + 1) % (q->len);
    return q->data[(q->front + q->len - 1) % (q->len)];
}

void rgb_loop_queue_push(RGBLoopQueue *q, RGBLoopQueueElm t)
{
    if (((q->rear + 1) % (q->len)) == q->front)
        return;
    q->data[q->rear] = t;
    q->rear = (q->rear + 1) % (q->len);
}

void rgb_forward_list_init(RGBArgumentList* list, RGBArgumentListNode* data, uint16_t len)
{
    list->data = data;
    list->head = -1;
    list->tail = 0;
    list->len = len;
    for (int i = 0; i < len; i++)
    {
        list->data[i].next = i + 1;
    }
    list->data[len - 1].next = -1;
    list->free_node = 0;
    rgb_forward_list_push_front(list, (RGBArgument){0,0});
}

void rgb_forward_list_erase_after(RGBArgumentList* list, RGBArgumentListNode* data)
{
    int16_t target = 0;
    target = data->next;
    data->next = list->data[target].next;
    list->data[target].next = list->free_node;
    list->free_node = target;
}

void rgb_forward_list_insert_after(RGBArgumentList* list, RGBArgumentListNode* data, RGBArgument t)
{
    if (list->free_node == -1)
    {
        return;
    }
    int16_t new_node = list->free_node;
    list->free_node = list->data[list->free_node].next;

    list->data[new_node].data = t;
    list->data[new_node].next = data->next;

    data->next = new_node;
}

void rgb_forward_list_push_front(RGBArgumentList* list, RGBArgument t)
{
    if (list->free_node == -1)
    {
        return;
    }
    int16_t new_node = list->free_node;
    list->free_node = list->data[list->free_node].next;

    list->data[new_node].data = t;
    list->data[new_node].next = list->head;

    list->head = new_node;
}
