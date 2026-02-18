/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "script.h"
#include "stdio.h"
#include "string.h"

#include "cutils.h"
#include "mquickjs.h"
#include "mqjs_utils.c"

#include "storage.h"

#include "mqjs_stdlib.h"
void script_log_func(void *opaque, const void *buf, size_t buf_len) {
    fwrite(buf, 1, buf_len, stdout);
}
extern const JSSTDLibraryDef js_stdlib;

#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
uint8_t g_script_bytecode_buffer[SCRIPT_BYTECODE_BUFFER_SIZE];
#endif
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_JIT
uint8_t g_script_source_buffer[SCRIPT_SOURCE_BUFFER_SIZE];
#endif

uint32_t g_script_watcher_mask[KEY_BITMAP_SIZE];

static uint8_t js_memory_pool[SCRIPT_MEMORY_SIZE];
static JSGCRef loop_func_ref; 
static JSValue *loop_func_ptr = NULL;
static bool loop_func_set = false;
static JSGCRef on_key_down_func_ref; 
static JSValue *on_key_down_func_ptr = NULL;
static bool on_key_down_func_set = false;
static JSGCRef on_key_up_func_ref; 
static JSValue *on_key_up_func_ptr = NULL;
static bool on_key_up_func_set = false;

static void dump_error(JSContext *ctx)
{
    JSValue obj;
    obj = JS_GetException(ctx);
    JS_PrintValueF(ctx, obj, JS_DUMP_LONG);
}

static JSContext *js_ctx;
void script_run_function(JSContext *ctx, const char *func_name)
{
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue func = JS_GetPropertyStr(ctx, global_obj, func_name);
    if (JS_IsFunction(ctx, func)) {
        JS_PushArg(ctx, func); /* func name */
        JS_PushArg(ctx, JS_NULL); /* this */
        JSValue ret = JS_Call(ctx, 0);
        if (JS_IsException(ret)) {
            dump_error(ctx);
        }
    }
    else
    {
        printf("no %s function\n", func_name);
    }
}


static JSValue new_key_instance(JSContext *ctx, Key* key) {
    int class_id = IS_ADVANCED_KEY(key) ? JS_CLASS_ADVANCED_KEY : JS_CLASS_KEY;

    JSGCRef obj_ref;
    JSValue *obj = JS_PushGCRef(ctx, &obj_ref);

    *obj = JS_NewObjectClassUser(ctx, class_id);
    if (JS_IsException(*obj)) {
        JS_PopGCRef(ctx, &obj_ref);
        return JS_EXCEPTION;
    }

    JS_SetOpaque(ctx, *obj, key);
    JSValue ret = *obj;
    JS_PopGCRef(ctx, &obj_ref);
    
    return ret;
}

static bool find_function_by_name(JSContext *ctx, JSValue **func_ptr, JSGCRef *func_ref, const char *func_name)
{   
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue func = JS_GetPropertyStr(ctx, global, func_name);
    
    if (JS_IsFunction(ctx, func)) {
        *func_ptr = JS_PushGCRef(ctx, func_ref);
        **func_ptr = func;
        return true;
    } else {
        *func_ptr = NULL;
        return false;
    }
}

static void script_setup_hooks(JSContext *ctx)
{
    loop_func_set = find_function_by_name(ctx, &loop_func_ptr, &loop_func_ref, "loop");
    on_key_down_func_set = find_function_by_name(ctx, &on_key_down_func_ptr, &on_key_down_func_ref, "onKeyDown");
    on_key_up_func_set = find_function_by_name(ctx, &on_key_up_func_ptr, &on_key_up_func_ref, "onKeyUp");
}

void script_reset(void)
{
    memset(g_script_watcher_mask, 0, sizeof(g_script_watcher_mask));
    if (js_ctx)
    {
        JS_FreeContext(js_ctx);
        js_ctx = NULL;
    }
    memset(js_timer_list, 0, sizeof(js_timer_list));
    memset(js_memory_pool, 0, sizeof(js_memory_pool)); 

    loop_func_ptr = NULL;
    loop_func_set = false;
    
    on_key_down_func_ptr = NULL;
    on_key_down_func_set = false;
    
    on_key_up_func_ptr = NULL;
    on_key_up_func_set = false;
    js_ctx = JS_NewContext(js_memory_pool, sizeof(js_memory_pool), &js_stdlib);
    if (!js_ctx) {
        return;
    }
    JS_SetLogFunc(js_ctx, script_log_func);
}

void script_init(void)
{
    storage_read_script();
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    script_update_bytecode(g_script_bytecode_buffer, sizeof(g_script_bytecode_buffer));
#endif
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_JIT
    script_update_source((char *)g_script_source_buffer, strlen(g_script_source_buffer));
#endif
}

void script_eval(const char *code_buf, size_t len, const char *filename)
{
    if (!js_ctx || !code_buf) return;

    JSValue ret = JS_Eval(js_ctx, code_buf, len, filename, 0);

    if (JS_IsException(ret)) {
        dump_error(js_ctx);
    }
}

void script_update_source(const char *code, size_t len)
{
    script_reset();
    script_eval(code, len, "<runtime>");
    script_setup_hooks(js_ctx);
}

void script_load_bytecode(uint8_t *bytecode_buf, size_t len)
{
    if (!js_ctx || !bytecode_buf) return;

    if (!JS_IsBytecode(bytecode_buf, len)) {
        printf("Error: Invalid bytecode format.\n");
        return;
    }

    if (JS_RelocateBytecode(js_ctx, bytecode_buf, len)) {
        printf("Error: Bytecode relocation failed.\n");
        return;
    }

    JSValue func = JS_LoadBytecode(js_ctx, bytecode_buf);

    if (JS_IsException(func)) {
        dump_error(js_ctx);
        return;
    }

    JSValue ret = JS_Run(js_ctx, func);

    if (JS_IsException(ret)) {
        dump_error(js_ctx);
    }
}

void script_update_bytecode(uint8_t *bytecode_buf, size_t len)
{
    script_reset();
    script_load_bytecode(bytecode_buf, len);
    script_setup_hooks(js_ctx);
}

static void run_timers(JSContext *ctx)
{
    int64_t min_delay, delay, cur_time;
    BOOL has_timer;
    int i;
    JSTimer *th;
    min_delay = 1000;
    cur_time = get_time_ms();
    has_timer = FALSE;
    for(i = 0; i < SCRIPT_MAX_TIMERS; i++) {
        th = &js_timer_list[i];
        if (th->allocated) {
            has_timer = TRUE;
            delay = th->timeout - cur_time;
            if (delay <= 0) {
                JSValue ret;
                switch (th->type)
                {
                case TIMER_TYPE_JS_TIMEOUT:
                    /* the timer expired */
                    if (JS_StackCheck(ctx, 2))
                        goto fail;
                    JS_PushArg(ctx, th->func.val); /* func name */
                    JS_PushArg(ctx, JS_NULL); /* this */
                    
                    JS_DeleteGCRef(ctx, &th->func);
                    th->allocated = FALSE;
                    
                    ret = JS_Call(ctx, 0);
                    if (JS_IsException(ret)) {
                    fail:
                        dump_error(js_ctx);
                        return;
                    }
                    min_delay = 0;
                    break;
                case TIMER_TYPE_JS_RELEASE_KEYCODE:
                    /* the timer expired */
                    js_keyboard_release(ctx, th->keycode);
                    th->allocated = FALSE;
                    min_delay = 0;
                    
                    break;
                default:
                    break;
                }
                break;
            } else if (delay < min_delay) {
                min_delay = delay;
            }
        }
    }
    if (!has_timer)
        return;
}

void script_watch(uint16_t id)
{
    BIT_SET(g_script_watcher_mask[id / 32], id % 32);
}

void script_process(void)
{
    if (loop_func_set)
    {
        if (JS_StackCheck(js_ctx, 2))
        {
            goto fail;
        }
        JS_PushArg(js_ctx, *loop_func_ptr); /* func name */
        JS_PushArg(js_ctx, JS_NULL); /* this */
        JSValue ret = JS_Call(js_ctx, 0);
        if (JS_IsException(ret)) {
        fail:
            dump_error(js_ctx);
        }
    }
    run_timers(js_ctx);
}

void script_event_handler(KeyboardEvent event)
{
    JSGCRef func_ref;
    JSValue *pfunc;
    const uint16_t id = ((Key*)event.key)->id;
    if (!(BIT_GET(g_script_watcher_mask[id / 32], id % 32) || KEYCODE_GET_MAIN(event.keycode) == MACRO_COLLECTION))
    {
        return;
    }
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_DOWN:
        //if (!event.is_virtual)
        //{
        //    keyboard_key_event_down_callback((Key*)event.key);
        //}
    case KEYBOARD_EVENT_KEY_UP:
        if (event.event == KEYBOARD_EVENT_KEY_UP)
        {
            if (on_key_up_func_set)
            {
                pfunc = JS_PushGCRef(js_ctx, &func_ref);
                *pfunc = *on_key_up_func_ptr;
                if (JS_StackCheck(js_ctx, 3))
                {
                    JS_PopGCRef(js_ctx, &func_ref);
                    return;
                }
                JS_PushArg(js_ctx, new_key_instance(js_ctx, event.key));
                JS_PushArg(js_ctx, *pfunc); /* func name */
                JS_PushArg(js_ctx, JS_NULL); /* this */
                JSValue ret = JS_Call(js_ctx, 1);
                JS_PopGCRef(js_ctx, &func_ref);
                if (JS_IsException(ret)) {
                    dump_error(js_ctx);
                }
            }
            else
            {
                printf("no on_key_up function\n");
            }
        }
        else
        {
            if (on_key_down_func_set)
            {
                pfunc = JS_PushGCRef(js_ctx, &func_ref);
                *pfunc = *on_key_down_func_ptr;
                if (JS_StackCheck(js_ctx, 3))
                {
                    JS_PopGCRef(js_ctx, &func_ref);
                    return;
                }
                JS_PushArg(js_ctx, new_key_instance(js_ctx, event.key));
                JS_PushArg(js_ctx, *pfunc); /* func name */
                JS_PushArg(js_ctx, JS_NULL); /* this */
                JSValue ret = JS_Call(js_ctx, 1);
                JS_PopGCRef(js_ctx, &func_ref);
                if (JS_IsException(ret)) {
                    dump_error(js_ctx);
                }
            }
            else
            {
                printf("no on_key_down function\n");
            }
        }
        break;
    case KEYBOARD_EVENT_KEY_TRUE:
        break;
    case KEYBOARD_EVENT_KEY_FALSE:
        break;
    default:
        break;
    }
}

void script_deinit(void)
{
    if (js_ctx) {
        JS_FreeContext(js_ctx);
        js_ctx = NULL;
    }
    memset(g_script_watcher_mask, 0, sizeof(g_script_watcher_mask));
}

