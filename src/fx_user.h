/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FX_USER_H_
#define FX_USER_H_

/*
 * FileX user defines.
 *
 * Every FileX translation unit reaches this file through fx_api.h -> fx_port.h
 * when FX_INCLUDE_USER_DEFINE_FILE is defined (the CMake target sets it), so
 * whatever is configured here applies both to the FileX sources and to the code
 * that calls them. That makes the defines below safe to change from the
 * application's keyboard_config.h, which is included first.
 *
 * Keep it that way: a FileX option that is only defined for some translation
 * units silently changes struct layouts such as FX_MEDIA and FX_PATH.
 */
#include "keyboard_config.h"

/* libamp builds FileX in standalone mode, without ThreadX. This also implies
 * FX_SINGLE_THREAD, FX_NO_LOCAL_PATH and FX_NO_TIMER. */
#ifndef FX_STANDALONE_ENABLE
#define FX_STANDALONE_ENABLE
#endif

/* FileX's generic port defines its scalar types inside an `#ifndef VOID` block,
 * and fx_port.h includes this file *before* that block, so the types can be
 * supplied from here. They are supplied, deliberately, without BOOL:
 * mquickjs/cutils.h typedefs BOOL as int and script.c includes both headers, so
 * a `typedef char BOOL` in the same translation unit is a hard error. Defining
 * VOID here makes the port skip its whole block, which keeps
 * lib/filex/ports/generic/inc/fx_port.h byte-identical to upstream.
 *
 * Keep this list in sync with that file if the port ever changes its types. */
#define VOID void
typedef char CHAR;
typedef unsigned char UCHAR;
typedef int INT;
typedef unsigned int UINT;
typedef long LONG;
typedef unsigned long ULONG;
typedef short SHORT;
typedef unsigned short USHORT;

/* The FileX media buffer is 4096 bytes. Under the LevelX layer FileX uses
 * 512-byte sectors, so eight is the largest cache it can hold there. */
#ifndef FX_MAX_SECTOR_CACHE
#define FX_MAX_SECTOR_CACHE 8
#endif

/* The FileX options and their defaults are documented with the values in the
 * application's keyboard_config.h. FX_SINGLE_THREAD, FX_NO_LOCAL_PATH and
 * FX_NO_TIMER are already implied by FX_STANDALONE_ENABLE above. */

#endif /* FX_USER_H_ */
