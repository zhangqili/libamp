/*
 * Micro QuickJS REPL library
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "script.h"
#include "layer.h"
#include "stdio.h"

#include "cutils.h"
#include "mquickjs.h"

#define JS_CLASS_KEY (JS_CLASS_USER + 0)
#define JS_CLASS_ADVANCED_KEY (JS_CLASS_USER + 1)
#define JS_CLASS_COUNT (JS_CLASS_USER + 2)

static void js_key_constructor(JSContext *ctx, JSValue *this_val, int argc,
                                        JSValue *argv)
{
    JSValue obj;
    Key *key;
    int id;
    if (argc > 0)
    {
        if (JS_ToInt32(ctx, &id, argv[0]))
            return JS_EXCEPTION;
    }
    else
    {
        id = 0;
    }
    key = keyboard_get_key(id);
    if (key == NULL)
    {
        return JS_ThrowRangeError(ctx, "Out of range");
    }
    argc &= ~FRAME_CF_CTOR;
    obj = JS_NewObjectClassUser(ctx, IS_ADVANCED_KEY(key) ? JS_CLASS_ADVANCED_KEY : JS_CLASS_KEY);
    JS_SetOpaque(ctx, obj, key);
    return obj;
}

static void js_key_finalizer(JSContext *ctx, void *opaque)
{
    UNUSED(ctx);
    UNUSED(opaque);
    //free(key);
}

static JSValue js_key_get_id(JSContext *ctx, JSValue *this_val, int argc,
                                  JSValue *argv)
{
    Key *key;
    int class_id = JS_GetClassID(ctx, *this_val);
    if (class_id != JS_CLASS_KEY && class_id != JS_CLASS_ADVANCED_KEY)
        return JS_ThrowTypeError(ctx, "expecting Key class");
    key = JS_GetOpaque(ctx, *this_val);
    return JS_NewInt32(ctx, key->id);
}

static JSValue js_key_get_state(JSContext *ctx, JSValue *this_val, int argc,
                                  JSValue *argv, int magic)
{
    Key *key;
    int class_id = JS_GetClassID(ctx, *this_val);
    if (class_id != JS_CLASS_KEY && class_id != JS_CLASS_ADVANCED_KEY)
        return JS_ThrowTypeError(ctx, "expecting Key class");
    key = JS_GetOpaque(ctx, *this_val);
    if (magic)
    {
        return JS_NewBool(key->state);
    }
    else
    {
        return JS_NewBool(key->report_state);
    }
}

static JSValue js_key_emit(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int event_id;
    Key* key;
    int class_id = JS_GetClassID(ctx, *this_val);
    if (class_id != JS_CLASS_KEY && class_id != JS_CLASS_ADVANCED_KEY)
        return JS_ThrowTypeError(ctx, "expecting Key class");
    key = JS_GetOpaque(ctx, *this_val);
    if (argc > 0)
    {
        JS_ToInt32(ctx, &event_id, argv[0]);
    }
    else
    {
        event_id = KEYBOARD_EVENT_KEY_DOWN;
    }
    printf("event_id:%d\n", event_id);
    uint8_t report_state = key->report_state;
    keyboard_event_handler(
        MK_EVENT(layer_cache_get_keycode(key->id),event_id,key));
    keyboard_key_set_report_state(key, report_state);//protect key state
    return JS_UNDEFINED;
}


//static void js_advanced_key_finalizer(JSContext *ctx, void *opaque)
//{
//    AdvancedKey *key = opaque;
//    //free(key);
//}

static JSValue js_advanced_key_get(JSContext *ctx, JSValue *this_val, int argc,
                                  JSValue *argv, int magic)
{
    AdvancedKey *key;
    int class_id = JS_GetClassID(ctx, *this_val);
    if (class_id != JS_CLASS_ADVANCED_KEY)
    {
        return JS_ThrowTypeError(ctx, "expecting AdvancedKey class");
    }
    key = JS_GetOpaque(ctx, *this_val);
#ifdef FIXED_POINT_EXPERIMENTAL
    switch (magic)
    {
    case 0:
        return JS_NewInt32(ctx, key->value);
        break;
    case 1:
        return JS_NewInt32(ctx, key->raw);
        break;
    case 2:
        return JS_NewInt32(ctx, key->extremum);
        break;
    case 3:
        return JS_NewInt32(ctx, key->difference);
        break;
    case 4:
        return JS_NewInt32(ctx, key->config.mode);
        break;
    case 5:
        return JS_NewInt32(ctx, key->config.calibration_mode);
        break;
    case 6:
        return JS_NewInt32(ctx, key->config.activation_value);
        break;
    case 7:
        return JS_NewInt32(ctx, key->config.deactivation_value);
        break;
    case 8:
        return JS_NewInt32(ctx, key->config.trigger_distance);
        break;
    case 9:
        return JS_NewInt32(ctx, key->config.release_distance);
        break;
    case 10:
        return JS_NewInt32(ctx, key->config.trigger_speed);
        break;
    case 11:
        return JS_NewInt32(ctx, key->config.release_speed);
        break;
    case 12:
        return JS_NewInt32(ctx, key->config.upper_deadzone);
        break;
    case 13:
        return JS_NewInt32(ctx, key->config.lower_deadzone);
        break;
    case 14:
        return JS_NewInt32(ctx, key->config.upper_bound);
        break;
    case 15:
        return JS_NewInt32(ctx, key->config.lower_bound);
        break;
    default:
        break;
    }
#else
    switch (magic)
    {
    case 0:
        return JS_NewFloat64(ctx, key->value);
        break;
    case 1:
        return JS_NewFloat64(ctx, key->raw);
        break;
    case 2:
        return JS_NewFloat64(ctx, key->extremum);
        break;
    case 3:
        return JS_NewFloat64(ctx, key->difference);
        break;
    case 4:
        return JS_NewInt32(ctx, key->config.mode);
        break;
    case 5:
        return JS_NewInt32(ctx, key->config.calibration_mode);
        break;
    case 6:
        return JS_NewFloat64(ctx, key->config.activation_value);
        break;
    case 7:
        return JS_NewFloat64(ctx, key->config.deactivation_value);
        break;
    case 8:
        return JS_NewFloat64(ctx, key->config.trigger_distance);
        break;
    case 9:
        return JS_NewFloat64(ctx, key->config.release_distance);
        break;
    case 10:
        return JS_NewFloat64(ctx, key->config.trigger_speed);
        break;
    case 11:
        return JS_NewFloat64(ctx, key->config.release_speed);
        break;
    case 12:
        return JS_NewFloat64(ctx, key->config.upper_deadzone);
        break;
    case 13:
        return JS_NewFloat64(ctx, key->config.lower_deadzone);
        break;
    case 14:
        return JS_NewFloat64(ctx, key->config.upper_bound);
        break;
    case 15:
        return JS_NewFloat64(ctx, key->config.lower_bound);
        break;
    default:
        break;
    }
#endif
    return JS_UNDEFINED;
}

static JSValue js_advanced_key_set(JSContext *ctx, JSValue *this_val, int argc,
                                  JSValue *argv, int magic)
{
    AdvancedKey *key;
    int class_id = JS_GetClassID(ctx, *this_val);
    if (class_id != JS_CLASS_ADVANCED_KEY)
        return JS_ThrowTypeError(ctx, "expecting Rectangle class");
    key = JS_GetOpaque(ctx, *this_val);
    return JS_NewInt32(ctx, key->value);
}

static JSValue js_keyboard_get_key(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int n;
    JSValue obj;
    Key *key;
    if (JS_ToInt32(ctx, &n, argv[0]))
    {
        return JS_EXCEPTION;
    }
    key = keyboard_get_key(n);
    if (key == NULL)
    {
        return JS_ThrowRangeError(ctx, "Key index out of range");
    }
    if (IS_ADVANCED_KEY(key))
    {
        obj = JS_NewObjectClassUser(ctx, JS_CLASS_ADVANCED_KEY);
    }
    else
    {
        obj = JS_NewObjectClassUser(ctx, JS_CLASS_KEY);
    }
    JS_SetOpaque(ctx, obj, key);
    return obj;
}

static JSValue js_keyboard_get_tick(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv, int magic)
{
    if (magic)
    {
        return JS_NewInt64(ctx, (int64_t)KEYBOARD_TICK_TO_TIME(g_keyboard_tick));
    }
    else
    {
        return JS_NewInt64(ctx, (int64_t)g_keyboard_tick);
    }
}

static void watch_recursive(JSContext *ctx, JSValue val)
{
    if (JS_IsNumber(ctx, val)) {
        int32_t id;
        if (JS_ToInt32(ctx, &id, val) == 0) {
            script_watch(id);
        }
    } 
    else if (JS_GetClassID(ctx, val) == JS_CLASS_ARRAY) {
        JSGCRef arr_ref;
        JSValue *p_arr = JS_PushGCRef(ctx, &arr_ref);
        *p_arr = val;

        JSValue len_val = JS_GetPropertyStr(ctx, *p_arr, "length");
        int32_t len;
        
        if (JS_ToInt32(ctx, &len, len_val) == 0) {
            for (int i = 0; i < len; i++) {
                JSValue item = JS_GetPropertyUint32(ctx, *p_arr, i);
                watch_recursive(ctx, item);
            }
        }
        
        JS_PopGCRef(ctx, &arr_ref);
    }
}

static JSValue js_keyboard_watch(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    for (int i = 0; i < argc; i++)
    {
        watch_recursive(ctx, argv[i]);
    }
    return JS_UNDEFINED;
}

//static JSValue js_keyboard_suspend(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
//{
//    // 返回特殊的挂起异常值
//    return JS_VALUE_MAKE_SPECIAL(JS_TAG_EXCEPTION, JS_EX_SUSPEND);
//}

static JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int i;
    JSValue v;
    for(i = 0; i < argc; i++) {
        if (i != 0)
            putchar(' ');
        v = argv[i];
        if (JS_IsString(ctx, v)) {
            JSCStringBuf buf;
            const char *str;
            size_t len;
            str = JS_ToCStringLen(ctx, &len, v, &buf);
            fwrite(str, 1, len, stdout);
        } else {
            JS_PrintValueF(ctx, argv[i], JS_DUMP_LONG);
        }
    }
    putchar('\n');
    return JS_UNDEFINED;
}

/* timers */
typedef struct {
    BOOL allocated;
    JSGCRef func;
    int64_t timeout; /* in ms */
} JSTimer;

#define MAX_TIMERS 16

static JSTimer js_timer_list[MAX_TIMERS];

static int64_t get_time_ms(void)
{
    return KEYBOARD_TICK_TO_TIME(g_keyboard_tick);
}

static JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    JSTimer *th;
    int delay, i;
    JSValue *pfunc;
    
    if (!JS_IsFunction(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "not a function");
    if (JS_ToInt32(ctx, &delay, argv[1]))
        return JS_EXCEPTION;
    for(i = 0; i < MAX_TIMERS; i++) {
        th = &js_timer_list[i];
        if (!th->allocated) {
            pfunc = JS_AddGCRef(ctx, &th->func);
            *pfunc = argv[0];
            th->timeout = get_time_ms() + delay;
            th->allocated = TRUE;
            return JS_NewInt32(ctx, i);
        }
    }
    return JS_ThrowInternalError(ctx, "too many timers");
}

static JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int timer_id;
    JSTimer *th;

    if (JS_ToInt32(ctx, &timer_id, argv[0]))
        return JS_EXCEPTION;
    if (timer_id >= 0 && timer_id < MAX_TIMERS) {
        th = &js_timer_list[timer_id];
        if (th->allocated) {
            JS_DeleteGCRef(ctx, &th->func);
            th->allocated = FALSE;
        }
    }
    return JS_UNDEFINED;
}
#ifndef MQJS_MINIMAL
static JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    JS_GC(ctx);
    return JS_UNDEFINED;
}

static JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return JS_NewInt64(ctx, (int64_t)KEYBOARD_TICK_TO_TIME(g_keyboard_tick));
}

static JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return JS_NewInt64(ctx, get_time_ms());
}

static uint8_t *load_file(const char *filename, int *plen)
{
    FILE *f;
    uint8_t *buf;
    int buf_len;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    buf_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(buf_len + 1);
    fread(buf, 1, buf_len, f);
    buf[buf_len] = '\0';
    fclose(f);
    if (plen)
        *plen = buf_len;
    return buf;
}
/* load a script */
static JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return 0;
}

#endif
