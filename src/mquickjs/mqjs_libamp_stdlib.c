#include <math.h>
#include <stdio.h>
#include <string.h>

#include "mquickjs_build.h"
#include "keyboard_conf.h"

static const JSPropDef js_key_proto[] = {
    JS_CGETSET_DEF("id", js_key_get_id, NULL),
    JS_CGETSET_MAGIC_DEF("state", js_key_get_state, NULL, 0),
    JS_CGETSET_MAGIC_DEF("reportState", js_key_get_state, NULL, 1),
    JS_CFUNC_DEF("emit", 1, js_key_emit ),
    JS_PROP_END,
};

static const JSPropDef js_key[] = {
    JS_PROP_END,
};

static const JSClassDef js_key_class =
    JS_CLASS_DEF("Key", 1, js_key_constructor, JS_CLASS_KEY, NULL, js_key_proto, NULL, js_key_finalizer);


static const JSPropDef js_advanced_key_proto[] = {
    JS_CGETSET_MAGIC_DEF("value", js_advanced_key_get, js_advanced_key_set, 0),
    JS_CGETSET_MAGIC_DEF("raw", js_advanced_key_get, js_advanced_key_set, 1),
    JS_CGETSET_MAGIC_DEF("extremum", js_advanced_key_get, js_advanced_key_set, 2),
    JS_CGETSET_MAGIC_DEF("difference", js_advanced_key_get, js_advanced_key_set, 3),
    JS_CGETSET_MAGIC_DEF("mode", js_advanced_key_get, js_advanced_key_set, 4),
    JS_CGETSET_MAGIC_DEF("calibrationMode", js_advanced_key_get, js_advanced_key_set, 5),
    JS_CGETSET_MAGIC_DEF("activationValue", js_advanced_key_get, js_advanced_key_set, 6),
    JS_CGETSET_MAGIC_DEF("deactivationValue", js_advanced_key_get, js_advanced_key_set, 7),
    JS_CGETSET_MAGIC_DEF("triggerDistance", js_advanced_key_get, js_advanced_key_set, 8),
    JS_CGETSET_MAGIC_DEF("releaseDistance", js_advanced_key_get, js_advanced_key_set, 9),
    JS_CGETSET_MAGIC_DEF("triggerSpeed", js_advanced_key_get, js_advanced_key_set, 10),
    JS_CGETSET_MAGIC_DEF("releaseSpeed", js_advanced_key_get, js_advanced_key_set, 11),
    JS_CGETSET_MAGIC_DEF("upperDeadzone", js_advanced_key_get, js_advanced_key_set, 12),
    JS_CGETSET_MAGIC_DEF("lowerDeadzone", js_advanced_key_get, js_advanced_key_set, 13),
    JS_CGETSET_MAGIC_DEF("upperBound", js_advanced_key_get, js_advanced_key_set, 14),
    JS_CGETSET_MAGIC_DEF("lowerBound", js_advanced_key_get, js_advanced_key_set, 15),
    JS_PROP_END,
};

static const JSPropDef js_advanced_key[] = {
    JS_PROP_END,
};

static const JSClassDef js_advanced_key_class =
    JS_CLASS_DEF("AdvancedKey", 2, js_key_constructor, JS_CLASS_ADVANCED_KEY, js_advanced_key, js_advanced_key_proto, &js_key_class, js_key_finalizer);


static const JSPropDef js_keyboard[] = {
    //JS_CFUNC_DEF("suspend", 1, js_keyboard_suspend),
    JS_CFUNC_DEF("getKey", 1, js_keyboard_get_key),
    JS_CFUNC_MAGIC_DEF("getTick", 0, js_keyboard_get_tick,0),
    JS_CFUNC_MAGIC_DEF("getTime", 0, js_keyboard_get_tick,1),
    JS_CFUNC_DEF("watch", 1, js_keyboard_watch),
    JS_CFUNC_MAGIC_DEF("press", 1, js_keyboard_press_release,0),
    JS_CFUNC_MAGIC_DEF("release", 1, js_keyboard_press_release,1),
    JS_CFUNC_DEF("tap", 1, js_keyboard_tap),
    JS_CFUNC_DEF("getLayerIndex", 0, js_keyboard_get_layer_index),
#ifndef MQJS_MINIMAL
    JS_CFUNC_MAGIC_DEF("command", 1, js_keyboard_command, KEYBOARD_CONFIG_BASE),
    JS_CFUNC_MAGIC_DEF("reboot",  0, js_keyboard_command, KEYBOARD_REBOOT),
    JS_CFUNC_MAGIC_DEF("factory_reset",  0, js_keyboard_command, KEYBOARD_FACTORY_RESET),
    JS_CFUNC_MAGIC_DEF("save",  0, js_keyboard_command, KEYBOARD_SAVE),
    JS_CFUNC_MAGIC_DEF("enterBootloader",  0, js_keyboard_command, KEYBOARD_BOOTLOADER),
    JS_CFUNC_MAGIC_DEF("resetToDefault",  0, js_keyboard_command, KEYBOARD_RESET_TO_DEFAULT),
    JS_CFUNC_MAGIC_DEF("setConfig",  1, js_keyboard_command, KEYBOARD_CONFIG0),
#endif
    JS_PROP_END,
};
static const JSClassDef js_keyboard_obj =
    JS_OBJECT_DEF("Keyboard", js_keyboard);

    
static const JSPropDef js_rgb[] = {
    JS_CFUNC_MAGIC_DEF("setRGB", 4, js_rgb_set_led, 0),
    JS_CFUNC_MAGIC_DEF("setHSV", 4, js_rgb_set_led, 1),
    JS_CFUNC_DEF("setMode", 2, js_rgb_set_led_mode),
    JS_PROP_END,
};
static const JSClassDef js_rgb_obj =
    JS_OBJECT_DEF("LED", js_rgb);
/* include the full standard library too */

#define LIBAMP_CONFIG_CLASS
#include "mqjs_stdlib.c"
