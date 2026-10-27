/* Exercise the MCU arithmetic on a 64-bit test host. Only the word-size
 * selection changes; fixed-width types, layouts, and the implementation are
 * the same as in the library. This translation unit replaces the archive's
 * advanced_key.c in libamp_calibration_32bit_tests. */
#include <stdint.h>
#undef UINTPTR_MAX
#define UINTPTR_MAX UINT32_MAX
#include "../../src/advanced_key.c"
