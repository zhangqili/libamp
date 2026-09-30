/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef LX_USER_H_
#define LX_USER_H_

/*
 * LevelX user defines.
 *
 * Every LevelX translation unit reaches this file through lx_api.h when
 * LX_INCLUDE_USER_DEFINE_FILE is defined (the CMake target sets it when the
 * LevelX flash layer is selected), so the settings below apply to the LevelX
 * sources and to the code that uses them. keyboard_config.h is included first
 * and can override any of them.
 */
#include "keyboard_config.h"

/* libamp builds LevelX in standalone mode, without ThreadX. */
#ifndef LX_STANDALONE_ENABLE
#define LX_STANDALONE_ENABLE
#endif

/* The LevelX options and their defaults are documented with the values in the
 * application's keyboard_config.h. LX_NOR_SECTOR_SIZE must stay 512 bytes:
 * FileX sizes its sectors from it. */

#endif /* LX_USER_H_ */
