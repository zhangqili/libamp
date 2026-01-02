#include <math.h>
#include <stdio.h>
#include <string.h>

#include "mquickjs_build.h"
#include "keyboard_conf.h"
static const JSPropDef js_keyboard[] = {
    //JS_CFUNC_DEF("suspend", 1, js_keyboard_suspend),
    JS_PROP_END,
};
static const JSClassDef js_keyboard_obj =
    JS_OBJECT_DEF("Keyboard", js_keyboard);
/* include the full standard library too */

#define LIBAMP_CONFIG_CLASS
#include "mqjs_stdlib.c"
