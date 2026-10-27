# libamp

[简体中文](README_zh-CN.md)

## Example and Reference Firmware

The following open-source firmware projects integrate libamp:

- [oholeo-keyboard-v2-firmware](https://github.com/zhangqili/oholeo-keyboard-v2-firmware)
- [oholeo-keyboard-firmware](https://github.com/zhangqili/oholeo-keyboard-firmware)
- [at32-keyboard-firmware](https://github.com/zhangqili/at32-keyboard-firmware)
- [trinity-pad-firmware](https://github.com/zhangqili/trinity-pad-firmware)

# Porting libamp to a New Keyboard Platform

## 1. Quick Start

### 1.1 What You Will Build

At the end of the minimum port, moving an analog key through its calibrated
range produces normal keyboard press and release reports. The minimum port has:

- a `keyboard_config.h` configuration header;
- an analog-sample pipeline and a default keymap;
- a USB HID keyboard transport;
- a periodic tick that increments `g_keyboard_tick`, calls `keyboard_task()`,
  and a foreground call to `keyboard_process()`.

### 1.2 Porting Model

Configure libamp through `keyboard_config.h`. Create a `keyboard_user.c` file
to keep the keymap, additional platform configuration, and adapter function
implementations together.

## 2. Build a Minimal Analog Keyboard

### 2.1 Minimum Requirements

The host needs CMake 3.14 or newer and a host `gcc` executable. libamp builds a
small host tool to generate mquickjs headers during configuration/build; it uses
the host compiler even when the firmware is cross-compiled. A target C toolchain
and a USB device stack are also required for the final firmware.

Add libamp as a submodule when it is not already part of the source tree:

```bash
git submodule add https://github.com/zhangqili/libamp.git third_party/libamp
git submodule update --init --recursive
```

### 2.2 Add libamp to the Build

Make the directory containing `keyboard_config.h` available before adding the
library. Add the application sources to the firmware target, then link libamp
and the math library. Add the USB backend sources as described in section 2.6.

```cmake
set(LIBAMP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")
add_subdirectory(third_party/libamp)

target_sources(keyboard_firmware PRIVATE
    platform/keyboard_user.c
    platform/input.c
)

target_link_libraries(keyboard_firmware PRIVATE
    libamp
    m
)
```

Add the selected USB stack, controller-port, and backend sources in addition to
this minimum target. If the target needs architecture-specific compiler flags,
apply them to `libamp` as well as to the application target. Do not point the
host generator at the cross compiler: `libamp/CMakeLists.txt` deliberately
configures that tool with the host `gcc`.

### 2.3 Create `keyboard_config.h`

The following is a small analog-only configuration. It creates one layer with
one advanced (analog) key and the default 6KRO keyboard interface. Choose your
own USB identifiers before shipping hardware.

```c
#pragma once

#define LAYER_NUM               1
#define ADVANCED_KEY_NUM        1
#define KEY_NUM                 0
#define POLLING_RATE            1000  /* Use 8000 with CONFIG_USB_HS. */

#define DEBOUNCE_PRESS          0
#define DEBOUNCE_PRESS_EAGER    1
#define DEBOUNCE_RELEASE        0
#define DEBOUNCE_RELEASE_EAGER  1

#define LUT_LENGTH                          4096
#define DEFAULT_ADVANCED_KEY_MODE            ADVANCED_KEY_ANALOG_NORMAL_MODE
#define DEFAULT_CALIBRATION_MODE             ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED
#define DEFAULT_TRIGGER_DISTANCE             0.10f
#define DEFAULT_RELEASE_DISTANCE             0.10f
#define DEFAULT_UPPER_DEADZONE               0.01f
#define DEFAULT_LOWER_DEADZONE               0.01f
#define DEFAULT_ACTIVATION_VALUE             0.50f
#define DEFAULT_DEACTIVATION_VALUE           0.49f
#define DEFAULT_ESTIMATED_RANGE              500

#define RING_BUF_LEN             8
#define ANALOG_BUFFER_LENGTH     1

#define USB_POLLING_INTERVAL_MS  1
#define FIXED_CONTROL_ENDPOINT_SIZE 0x40
#define FIXED_NUM_CONFIGURATIONS 1
#define VENDOR_ID                0x1234
#define PRODUCT_ID               0x0001
#define DEVICE_VER               0x0001
#define MANUFACTURER             "Example Manufacturer"
#define PRODUCT                  "Example Analog Keyboard"
#define SERIAL_NUMBER            "0001"
```

`POLLING_RATE` sets the frequency of `g_keyboard_tick` and `keyboard_task()`.
The USB endpoint interval is configured separately by `USB_POLLING_INTERVAL_MS`;
reports are sent when pending, or attempted every tick with `continuous_poll`
enabled. Use `1000` for a full-speed USB keyboard (1 kHz) and `8000` for a high-speed USB keyboard (8 kHz); define
`CONFIG_USB_HS` for the latter. Keep `USB_POLLING_INTERVAL_MS` at `1`: it is a
1 ms interval at full speed and one 125 us microframe at high speed.

`ADVANCED_KEY_NUM` and `KEY_NUM` define two input classes. Advanced keys
receive continuous values and use IDs `0` through `ADVANCED_KEY_NUM - 1`.
Ordinary keys receive boolean states and use IDs `ADVANCED_KEY_NUM` through
`TOTAL_KEY_NUM - 1`. `g_default_keymap[layer][id]` uses this common ID space.

The descriptor generator uses the USB identity and interface macros in this
header. Do not enable `NKRO_ENABLE`, `RAW_ENABLE`, `SHARED_EP_ENABLE`, or any
other optional interface until the minimum USB device is working.

### 2.4 Define the Keymap and Analog Mapping

Create `platform/keyboard_user.c` and include the libamp headers needed by the
data you define. The first analog key has ID `0`, so this example maps it to
the `A` key and reads it from ring buffer `0`.

```c
#include "keyboard.h"
#include "analog.h"

const Keycode g_default_keymap[LAYER_NUM][TOTAL_KEY_NUM] = {
    { KEY_A },
};

const uint16_t g_analog_map[ADVANCED_KEY_NUM] = {
    0,
};
```

`g_default_keymap` must contain one entry for every key in every layer. The
buffered-input path below uses `g_analog_map` to map an advanced-key ID to an
index in `g_adc_ringbufs`; it does not select a physical ADC channel by itself.

### 2.5 Acquire and Normalize Analog Values

libamp supports two input paths. Choose one for the platform.

**1. Fill the libamp ring buffers.** This is the default path:
`advanced_key_read_raw()` returns the average of
`g_adc_ringbufs[g_analog_map[key_id]]`. Feed that ring buffer whenever the
platform receives a converted sample:

```c
void platform_adc_sample_ready(uint16_t buffer_index, uint16_t raw_sample)
{
    ringbuf_push(&g_adc_ringbufs[buffer_index], raw_sample);
}
```

**2. Override `advanced_key_read_raw()`.** Use this path when samples already
live in a platform-owned data structure or need a board-specific read sequence.
The override receives the key, so it can select the source by key ID:

```c
AnalogRawValue advanced_key_read_raw(AdvancedKey *key)
{
    return platform_read_analog_key(key->key.id);
}
```

This path does not need to fill `g_adc_ringbufs` or define `g_analog_map` for
the read operation. It must return the latest raw sample without blocking the
keyboard tick.

The default `advanced_key_normalize()` performs a linear conversion using the
calibrated bounds. Override it only when the sensor needs a non-linear transfer
function. An override must clamp the result to `ANALOG_VALUE_MIN` through
`ANALOG_VALUE_MAX` and preserve the calibrated direction. A linear form is:

```c
AnalogValue advanced_key_normalize(AdvancedKey *key, AnalogRawValue sample)
{
    const int32_t upper = key->config.upper_bound;
    const int32_t lower = key->config.lower_bound;
    const int32_t span = upper - lower;

    if (span == 0) {
        return ANALOG_VALUE_MIN;
    }

    int64_t travel = ((int64_t)(upper - (int32_t)sample) * ANALOG_VALUE_RANGE) / span;
    if (travel < 0) return ANALOG_VALUE_MIN;
    if (travel > ANALOG_VALUE_RANGE) return ANALOG_VALUE_MAX;
    return (AnalogValue)(travel + ANALOG_VALUE_MIN);
}
```

The default implementation uses a Q16 scale precomputed by
`advanced_key_set_range()` to obtain a clamped LUT index, then rescales that
index to `ANALOG_VALUE_RANGE` and adds `ANALOG_VALUE_MIN`. This fixed-point
mapping may differ slightly from the direct formula above. Its final rescaling
still contains a division by the compile-time `LUT_LENGTH`; whether that becomes
a multiply/shift depends on the configuration and compiler.

For a non-linear magnetic response, `tools/lut_generator/lut_generator.py` can
generate a lookup table and an `advanced_key_normalize()` override. The
override calls `advanced_key_lut_index()`, which maps the calibrated range onto
`0` through `LUT_LENGTH` (inclusive) without overflow and clamps the final
index to `LUT_LENGTH - 1` before reading the table. Entries are absolute
normalized values that already include `ANALOG_VALUE_MIN`; the override returns
them without adding the minimum again. Set the script's constants for the
switch geometry:

| Constant | Meaning |
| --- | --- |
| `R` | Magnet radius, in mm. |
| `L` | Magnet height along its magnetization axis, in mm. |
| `Z_END` | Axial distance, in mm, from the sensor to the magnet at the end of travel. Use the same physical reference point as `Z_START`. |
| `Z_START` | Axial sensor-to-magnet distance, in mm, at the start of travel. It must be greater than `Z_END`; their difference is the modeled travel. |
| `LUT_LENGTH` | Number of lookup-table entries. It must equal the firmware's `LUT_LENGTH` configuration. |
| `ANALOG_VALUE_MIN` / `ANALOG_VALUE_MAX` | Generated normalized values at the start and end of travel. Match the firmware and keep `0 <= MIN < MAX <= 65535`. |

Then generate the C source (the script requires NumPy):

```bash
python3 -m pip install numpy
python3 tools/lut_generator/lut_generator.py > analog_lut.c
```

Compile the generated table and function with the firmware, ensuring the source
includes the required libamp type declarations.

### 2.6 Integrate USB Transport

For the minimum port, choose one of the following USB integration paths. The
bundled backend uses CherryUSB, an optional but recommended USB stack.

**1. Use the bundled backend.** Follow the
[device porting guide](https://cherryusb.readthedocs.io/en/latest/quick_start/transplant.html)
to add its core and controller driver, implement low-level clock/pin/interrupt
initialization, route the USB interrupt, and configure cache-safe USB memory
when necessary.

Once the device port is working, add every source file under
`libamp/usb/template/cherryusb/` to the firmware target. The template builds
descriptors from `keyboard_config.h` and
provides the HID transport callbacks required by libamp. It includes the
keyboard interface without requiring a custom `hid_send_keyboard()` function.

For a CMake target, add the template sources and include directories as follows:

```cmake
file(GLOB_RECURSE LIBAMP_CHERRYUSB_TEMPLATE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libamp/usb/template/cherryusb/*.c"
)

target_sources(keyboard_firmware PRIVATE
    ${LIBAMP_CHERRYUSB_TEMPLATE_SOURCES}
)

target_include_directories(keyboard_firmware PRIVATE
    third_party/libamp/usb/template/cherryusb
    third_party/libamp/usb/template/cherryusb/mtp
)
```

Initialize the platform USB controller, then call the template's `usb_init()`
with the stack bus ID and USB controller register base:

```c
#include "usbd_user.h"

usb_init(0, PLATFORM_USB_CONTROLLER_BASE);
```

**2. Adapt another USB stack (TODO).** This requires a backend that provides
descriptor registration, endpoint callbacks, and report transport. A porting
guide for this path is not available yet.

### 2.7 Initialize libamp and Run Its Tasks

Initialize clocks, input hardware, and the USB controller before enabling
keyboard reports. Call `keyboard_init()` once after the input and optional
storage/LED dependencies are ready. On every keyboard tick, increment
`g_keyboard_tick` once and call `keyboard_task()` once. This can run from a
timer/RTOS schedule or from a deadline-driven bare-metal loop, at exactly
`POLLING_RATE`.

Run `keyboard_process()` continuously outside the time-critical sampling path.
It handles queued key events, delayed calibration, and enabled background work
such as RGB rendering, Nexus configuration sync, and statistics saving. Raw HID
requests are processed synchronously by the receive callback, not by this loop.
The RGB renderer produces at most one frame per tick. Set `RGB_MAX_FRAME_RATE`
to space frames further apart; it defaults to `POLLING_RATE`. The actual interval
is `max(POLLING_RATE / RGB_MAX_FRAME_RATE, 1)` ticks, using integer division.
Choose a positive divisor of `POLLING_RATE` for an exact target rate. Effects
remain timed by the tick; fewer frames sample the same animation time scale.

```c
int main(void)
{
    platform_init();
    platform_input_start();
    platform_usb_prepare();

    keyboard_init();
    usb_init(0, PLATFORM_USB_CONTROLLER_BASE);

    while (1) {
        keyboard_process();
        /* It's recommended to call keyboard_task() from the periodic scheduler, not here. */
    }
}

void platform_keyboard_tick(void)
{
    g_keyboard_tick++;
    keyboard_task();
}
```

Do not call `keyboard_task()` concurrently from more than one context. If ADC
or USB callbacks share state with it, use the synchronization rules appropriate
for the platform. libamp resets `g_keyboard_tick` in `keyboard_init()`, but the
platform is responsible for every subsequent increment.

#### Execution contexts

The following table describes where the built-in task and transport paths run.
Platform callbacks and application calls retain their caller's context.

| Context | Entry points | Notes |
| --- | --- | --- |
| Input/transport callback | `ringbuf_push()`, `encoder_input()`, `encoder_input_delta()`, `midi_input_callback()`, `nexus_process_buffer()`, `packet_process_buffer()` | The platform determines whether these run in an interrupt. Packet processing is synchronous and can access flash or invoke user code. On a Nexus slave, `nexus_process_buffer()` also processes command packets synchronously. |
| Keyboard tick (`POLLING_RATE`) | `keyboard_task()`: scan, debounce, event handlers, HID reports, `packet_buffer_flush()`, `nexus_process()` / `nexus_send_report()` | Must not be re-entered. Transport send functions reached here must not block. Scripts also run here unless `SCRIPT_POLLING` is defined. |
| Main loop | `keyboard_process()`: key-event poller, `rgb_process()`, `nexus_poll()`, console flush, statistics saving, delayed calibration | Flash writes and calibration can take milliseconds. Defining `SCRIPT_POLLING` also moves periodic script processing here. |

Call `analog_calibrate()`, `rgb_init_flash()`, `rgb_flash()`, and application
storage/filesystem operations from a context that permits their latency. The
synchronous `nexus_request_timeout()` / `nexus_send_timeout()` calls require
the tick to keep advancing while they wait.

Key-bound operations (`KEYBOARD_SAVE`, profile selection, factory reset) access
storage in the event handler by default. Define `KEYBOARD_OPERATION_POLLING` to
run those operations in `keyboard_process()` instead. This switch does not move
Raw HID commands or other direct `storage_*()` / `fs_*()` calls: the bundled
CherryUSB Raw HID callback calls `packet_process_buffer()` directly, including
statistics writes and profile/filesystem operations. If flash access must stay
out of interrupts, the platform must defer these complete requests as well and
serialize filesystem access with foreground work.

For startup calibration, wait briefly after `keyboard_init()` for analog
samples to become stable, then call `analog_calibrate()` once. The delay should
be long enough for the input acquisition path to provide representative samples.

### 2.8 Verify the First Key Press

1. Build and flash the firmware.
2. Confirm that the host enumerates one USB HID keyboard interface.
3. Log or inspect the raw sample while the key moves; it must reach a stable
   value at both ends of travel.
4. Let calibration complete, then press and release the key. The host should
   receive the configured `A` key press and release.
5. If the key is inverted, swap the sensor wiring/ADC direction or adjust the
   normalization function; do not reverse key press/release events manually.

## 3. Complete the Input Layer

### 3.1 Multiple ADC Channels and Multiplexers

Set `ADVANCED_KEY_NUM` to the number of analog keys and size
`ANALOG_BUFFER_LENGTH` for the number of independently buffered sample sources.
Map each key to its buffer in `g_analog_map`. A multiplexed input commonly uses
a scan schedule that selects a channel, waits for acquisition, converts it, and
pushes the result into that channel's buffer. Keep the mapping and scan order
in one place so a wiring change cannot silently remap key IDs.

### 3.2 Calibration, Dead Zones, and Filtering

The default advanced-key configuration controls normal, rapid-trigger, and
speed modes. `keyboard_init()` restores a stored profile when storage is
enabled, otherwise the defaults; it does not call `analog_calibrate()`.
Call that function with released, stable keys to establish the resting bounds
and initialize enabled filters. It resets the calibration direction to
`ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED`; subsequent samples determine the
direction and extend the pressed bound. Tune `DEFAULT_ESTIMATED_RANGE`, dead
zones, trigger and release distances after collecting real sensor data.

`FILTER_ENABLE` enables the main filter; `FILTER_TYPE` and `FILTER_DOMAIN`
select its algorithm and input domain. For the low-pass filter,
`FILTER_LOWPASS_ALPHA` weights the previous state (default `0.5f`); a larger
value smooths more. Enable `FILTER_HYSTERESIS_ENABLE` only after choosing a
hysteresis appropriate for the raw or normalized domain. Filtering should
remove sample noise without hiding real travel changes at the chosen polling
rate.

### 3.3 Digital Keys

Add ordinary keys by increasing `KEY_NUM` and extending every layer of
`g_default_keymap`. Their IDs start at `ADVANCED_KEY_NUM`. Override
`keyboard_scan()` to pass their current state to `keyboard_key_update()`:

```c
void keyboard_scan(void);
bool keyboard_key_update(Key *key, bool state);
```

```c
void keyboard_scan(void)
{
    const bool pressed = platform_gpio_read(0);
    keyboard_key_update(&g_keyboard_keys[0], pressed);
}
```

The platform implements matrix scanning and expander reads. libamp applies
debouncing in `keyboard_key_update()` according to the platform's `DEBOUNCE_*`
settings, expressed in keyboard ticks.

`keyboard_task()` calls `keyboard_scan()` once per keyboard tick.
`keyboard_key_update()` takes a `Key` and its physical pressed state, applies
the configured debounce policy, and updates the key's report state. Its return
value indicates whether that report state changed.

`KEYBOARD_SKIP_UNCHANGED_EVENTS` defaults to `1`: unchanged states skip event
dispatch except mouse movement, joystick/gamepad axes, and `KEY_USER`, which
continue on every update. Set it to `0` to dispatch unchanged states for every
keycode too. This applies to digital and advanced-key update paths and Nexus
bitmap processing; it does not change the debounce policy or itself force a USB
report on every tick.

### 3.4 Rotary Encoders

Enable `ENCODER_ENABLE`, define `ENCODER_NUM`, and provide `g_encoders` from
`encoder.h`. Feed each encoder's accumulated position with `encoder_input()`
from the platform driver. Map each encoder's clockwise and counter-clockwise
motions to ordinary keys.

```c
void encoder_input_delta(uint16_t id, int16_t delta);
void encoder_input(uint16_t id, int32_t count);
void encoder_process(void);
```

Each encoder uses two ordinary-key IDs: `cw_id` for clockwise motion and
`ccw_id` for counter-clockwise motion. Reserve two ordinary keys per encoder
in `KEY_NUM`:

```c
#include "encoder.h"

#define ENCODER_CW_KEY_ID   (ADVANCED_KEY_NUM + 0)
#define ENCODER_CCW_KEY_ID  (ADVANCED_KEY_NUM + 1)

Encoder g_encoders[ENCODER_NUM] = {
    { .cw_id = ENCODER_CW_KEY_ID, .ccw_id = ENCODER_CCW_KEY_ID },
};
```

For example, pass the current count for encoder `0` whenever the platform reads
it:

```c
void platform_encoder_updated(int32_t count)
{
    encoder_input(0, count);
}
```

`encoder_input()` accepts an absolute count and calculates the movement since
the previous update. When the driver reports a relative movement instead, use
`encoder_input_delta(id, delta)`. With `ENCODER_ENABLE`, `keyboard_task()`
calls `encoder_process()` automatically; it turns a positive or negative
movement into the configured clockwise or counter-clockwise key state.

### 3.5 Custom Events, Reset, and Bootloader Entry

`keyboard_reboot()`, `keyboard_jump_to_bootloader()`, and
`keyboard_user_event_handler()` are weak hooks. `keyboard_delay()` is a platform
function declaration with no default implementation. Override only the hooks
your keymap exposes. A custom keycode should use `KEY_USER` and be handled in
`keyboard_user_event_handler()`; keep reset and bootloader policy out of
libamp itself.

```c
void keyboard_reboot(void);
void keyboard_jump_to_bootloader(void);
void keyboard_delay(uint32_t ms);
void keyboard_user_event_handler(KeyboardEvent event);
```

The built-in reboot and bootloader operations call `keyboard_reboot()` and
`keyboard_jump_to_bootloader()` respectively. Implement `keyboard_delay(ms)`
when using Kalman-filter calibration, which requests 1 ms delays during its
sampling loop. `keyboard_user_event_handler(KeyboardEvent event)` receives each
`KEY_USER` event, including unchanged states under the default dispatch policy.
When overriding it, call `keyboard_key_event_down_dispatch(event)` on key-down
and `keyboard_key_event_up_dispatch(event)` on key-up to retain queued events,
physical-key callbacks, statistics, and enabled script/macro hooks. Handle held
states separately if the application needs them.

## 4. Add Persistent Storage

Enable `STORAGE_ENABLE` and configure the selected filesystem (LittleFS by default). Storage uses the callbacks
declared in `driver.h`:

```c
int flash_read(uint32_t address, uint32_t size, uint8_t *data);
int flash_write(uint32_t address, uint32_t size, const uint8_t *data);
int flash_erase(uint32_t address, uint32_t size);
```

Implement them for an erasable region that is reserved exclusively for libamp.
The `FS_READ_SIZE`, `FS_PROG_SIZE`, `FS_BLOCK_SIZE`,
`FS_BLOCK_COUNT`, and `FS_CACHE_SIZE` values must match the flash device and
the reserved region. `FS_LOOKAHEAD_SIZE` and `LFS_BLOCK_CYCLES` configure LittleFS.
`FS_FLASH_OFFSET` sets the partition's byte offset in the flash callback address
space; it defaults to `0` and accepts `FILEX_FLASH_OFFSET` as a compatibility
spelling. Define one spelling, or give both the same value.
In particular, programming and erase alignment must be honored by the callback
implementation.

On startup, `keyboard_init()` mounts storage, verifies the stored version, and
recovers the selected profile. Storage therefore must be usable before calling
`keyboard_init()`. Never place the filesystem over firmware, bootloader, or
other application data.
A mount/open failure triggers an automatic format attempt. Switching the
filesystem or flash layer does not migrate existing data and can trigger this
recovery; back up needed files before changing those settings.

With `RECORD_PERSIST_ENABLE`, statistics are saved to `system/stat` every
`RECORD_STATISTICS_SAVE_INTERVAL_MS` (30 minutes by default). A key count change
advances the save timer so only `RECORD_STATISTICS_IDLE_SAVE_MS` (120 seconds by
default) remain; subsequent count changes restart those 120 seconds. Saving
resets the normal interval. Key releases and held keys do not affect the timer.
Statistics writes and resets from the host still save immediately.

### 4.1 FileX FAT Storage and the Flash Layer

`FILE_SYSTEM_TYPE` defaults to `FILE_SYSTEM_LFS`; define it as
`FILE_SYSTEM_FILEX` in `keyboard_config.h` to use FileX FAT.

Define `FLASH_LAYER` to select how the filesystem accesses flash:
`FLASH_LAYER_DIRECT` calls the `flash_*` callbacks declared in `driver.h`
directly and does no wear leveling, while `FLASH_LAYER_LEVELX` adds LevelX NOR
wear leveling but reduces usable capacity, uses more RAM and mounts more slowly.

### 4.2 Statistics and Host Protocol

Enable `COUNTER_ENABLE` for per-key counts. Add `STORAGE_ENABLE` and
`RECORD_PERSIST_ENABLE` to persist counts and total runtime in `system/stat`.
`keyboard_init()` loads the saved statistics and snapshots the startup counts.
The normal save interval is `RECORD_STATISTICS_SAVE_INTERVAL_MS` (30 minutes by default).
A physical-key down callback that increments a count schedules saving after
`RECORD_STATISTICS_IDLE_SAVE_MS` (120 seconds by default); increments restart that
delay. Releases and unchanged held states do not affect it. Saving resets the
normal interval; continuous typing can keep postponing this automatic save.
Keep the idle interval between zero and the normal interval. Host statistics
writes, including resets, save immediately when persistence is enabled.

With Raw HID enabled, send `PACKET_DATA_RECORD` (`0x0E`) using
`PACKET_CODE_GET` (`0x02`) or `PACKET_CODE_SET` (`0x01`). The packed structures in
`src/packet.h` define the wire layout:

| Subcommand | Value | GET | SET |
| --- | --- | --- | --- |
| `PACKET_DATA_RECORD_RUNTIME` | `0x00` | Boot `uptime` and total `runtime`, both in milliseconds. Without persistence, both return the current tick converted to milliseconds. | Sets total `runtime` when persistence is enabled; does not change `uptime`. |
| `PACKET_DATA_RECORD_KEYCOUNT` | `0x01` | Current cumulative count for each requested key. | Replaces each requested cumulative count. |
| `PACKET_DATA_RECORD_KEYCOUNT_SINCE_BOOT` | `0x02` | Current count minus the startup snapshot when persistence is enabled; otherwise the current count. | Sets the requested boot count by storing startup count plus the supplied value; without persistence, replaces the current count. |

`PacketRecordRuntime` is **20 bytes**: a 3-byte header (`code`, `id`, `type`),
1-byte `sub_cmd`, `uint64_t uptime` at byte offset 4, and `uint64_t runtime` at
offset 12. Update the host parser and SET encoder together with the firmware;
the previous 12-byte layout is not supported. `uptime` is derived from the
32-bit `g_keyboard_tick`: the 64-bit field does not extend tick wraparound.
The current total-runtime calculation also uses that converted tick.

Key-count packets contain the same header and subcommand, a `uint16_t length`,
then `length` packed entries of `uint16_t key_index` and `uint32_t count`.
Keep every index below `TOTAL_KEY_NUM` and fit all entries into the report;
a 64-byte Raw HID report can hold at most nine entries. Counts are zero when
`COUNTER_ENABLE` is disabled, and SET has no counting effect. The startup
snapshot stays fixed until reinitialization: changing cumulative counts does
not reset it, so boot deltas use unsigned 32-bit subtraction if the total is
set below the snapshot.

## 5. Add Lighting

Enable `RGB_ENABLE`, set `RGB_NUM`, and implement the LED driver callbacks:

```c
int led_set(uint16_t index, uint8_t red, uint8_t green, uint8_t blue);
int led_flush(void);
```

`led_set()` updates one logical LED; `led_flush()` makes the pending frame
visible. A serial LED driver usually buffers all colors until `led_flush()`.

Set global brightness with `rgb_set_brightness(uint8_t brightness)` (`0` to
`255`). With the gamma LUT enabled, direct writes to
`g_rgb_base_config.brightness` leave the output table stale. `rgb_init()`,
profile loading, host brightness commands, brightness keys, and factory reset
already use the setter.

`RGB_GAMMA_ENABLE` enables gamma correction with `RGB_GAMMA` (default `2.2f`).
Also define `RGB_GAMMA_LUT_ENABLE` to cache gamma and brightness in a 256-byte
RAM table rebuilt by the setter. With gamma enabled but no LUT, every output
channel is calculated directly; the LUT flag alone has no effect.

If application code changes base colors directly, keep RGB and HSV fields in
sync with `rgb_to_hsv()` / `hsv_to_rgb()`, including the secondary color. The
rainbow base effect reads `g_rgb_base_config.hsv` directly; the host color
packet handler already updates both representations.

To show a startup flash animation, `rgb_init_flash()` or `rgb_flash()` may be
called after `keyboard_init()`. Start the periodic tick source first: both
functions wait for `g_keyboard_tick` to advance, so neither returns if the tick
has not begun incrementing at `POLLING_RATE`.

The RGB tables use these related index spaces:

| Table | Index | Element | Purpose |
| --- | --- | --- | --- |
| `g_rgb_mapping` | LED ID (`0` to `RGB_NUM - 1`) | Key ID | Associates a logical LED with a key. |
| `g_rgb_inverse_mapping` | Key ID (`0` to `TOTAL_KEY_NUM - 1`) | LED ID | Finds the LED associated with a key. |
| `g_rgb_locations` | LED ID (`0` to `RGB_NUM - 1`) | `RGBLocation { x, y }` | Gives the physical position of that same LED. |

Thus `g_rgb_mapping[led_id]` and `g_rgb_locations[led_id]` must describe the
same LED. Unless `RGB_CUSTOM_INVERSE_MAPPING` is enabled, libamp creates
`g_rgb_inverse_mapping` from `g_rgb_mapping` during `rgb_init()`; automatic
generation assumes that each mapped key has at most one LED. When defining
locations, use `UNIT_TO_UM()` to express the key-unit layout. Choose only the
RGB modes and base effects needed by the product; each `RGB_MODE_USE_*` macro
includes its corresponding behavior.

## 6. Extend the USB Device

The bundled backend enables interfaces from `keyboard_config.h`. Set the
corresponding macro, then rebuild and verify the enumerated descriptors:
`NKRO_ENABLE`, `EXTRAKEY_ENABLE`, `MOUSE_ENABLE`, `RAW_ENABLE`, `MIDI_ENABLE`,
`JOYSTICK_ENABLE`, `DIGITIZER_ENABLE`, and `GAMEPAD_ENABLE` add the matching
keyboard feature or USB interface. When using the bundled CherryUSB template,
enabling these macros needs no additional USB-side configuration.

`SHARED_EP_ENABLE` carries several HID report IDs through one interrupt-IN
endpoint and reduces endpoint use. Start with separate reports and introduce a
shared endpoint only when controller resources require it. Set `MAX_ENDPOINTS`
to the controller's usable capacity. If the controller supports bidirectional
endpoints that use the same endpoint number for IN and OUT, enable
`USB_ENDPOINTS_ARE_REORDERABLE`; the descriptor generator will share the number
within each IN/OUT pair and conserve endpoint numbers.

The bundled backend also contains the endpoint callbacks and send functions for
these features. For Raw HID it forwards each complete OUT report to libamp's
transport. Gamepad output can be consumed by overriding
`gamepad_out_callback()`. Other USB backends are TODO.

Define `SERIAL_NUMBER` for a fixed serial string. Define
`SERIAL_NUMBER_USE_CUSTOM` and override
`usb_descriptor_get_serial_number(char *buffer, size_t buffer_size)` for a
device-derived value. Return the number of ASCII characters written, excluding
the terminating null byte.

## 7. Enable Advanced Runtime Features

`DYNAMICKEY_ENABLE` adds configurable advanced-key behaviors such as mod-tap,
toggle, dynamic keystroke, and mutex keys. `MACRO_ENABLE` enables macro
recording/playback. Both use the same event path as normal physical keys, so
verify them after the basic input and report path is stable.

`SCRIPT_ENABLE` requires `STORAGE_ENABLE` and a configured filesystem. Choose
`SCRIPT_RUNTIME_STRATEGY`, then size `SCRIPT_MEMORY_SIZE` and the matching
source or bytecode buffer for the available RAM. The host mquickjs header
generation step remains part of the libamp build even if scripts are disabled.

`MTP_ENABLE` exposes file access over USB. It needs the MTP backend sources,
the corresponding USB endpoints, and a filesystem that is safe to expose to a
host while the keyboard is running. Test file transfer, unplug/replug, and
power-loss behavior before enabling it in a released firmware.

### 7.1 Nexus: Linking Several Boards

`NEXUS_ENABLE` joins several boards into one keyboard. The master owns USB and
the keymap; each slave (`NEXUS_IS_SLAVE 1`) scans its own keys and streams them
to the master. The application provides the link: implement `nexus_send()`
(master to slave) and `nexus_report()` (slave to master) from `driver.h`, and
call `nexus_process_buffer(slave_id, frame, len)` whenever a complete frame
arrives on either side. Both send functions must copy or finish sending the
frame before they return and return `0` on success. Frames are described in
`nexus.h`.

On the master, define `g_nexus_slave_configs[NEXUS_SLAVE_NUM]`: each entry maps
slave key `j` to master key `map[j]` for `length` keys (at most
`NEXUS_SLICE_LENGTH_MAX`). `keyboard_init()` resolves the maps; `keyboard_task()`
applies the slaves' reports and `keyboard_process()` pushes each slave's
advanced-key configuration without blocking, both at start-up and whenever it
changes or a slave reconnects. A slave that stops reporting for
`NEXUS_LINK_TIMEOUT_MS` is treated as unplugged and its keys release.
`nexus_slave_is_online()` exposes that state. In bitmap mode, with the default
`KEYBOARD_SKIP_UNCHANGED_EVENTS=1`, applying reports only updates keys whose
state changed, whose debounce is still settling, or whose keycode runs every
tick (mouse movement, analog axes, `KEY_USER`). With `NEXUS_USE_RAW 1` the
slaves send raw samples instead and the master normalizes them; master keys
that no slave maps are left to the application's `keyboard_scan()`.

`nexus_request_timeout()` offers synchronous request/response for application
use. It spins on `g_keyboard_tick`, so only call it from the foreground loop
or from an interrupt below the tick's priority.

## 8. Build, Test, and Troubleshoot

### 8.1 Run libamp Host Tests

The host tests exercise the libamp core without the target hardware:

```bash
cmake -S third_party/libamp -B build/libamp-tests -DLIBAMP_BUILD_TESTS=ON
cmake --build build/libamp-tests --parallel
ctest --test-dir build/libamp-tests --output-on-failure
```

When libamp is the repository root rather than a subdirectory, replace
`third_party/libamp` with `.`.

The host hot-path benchmark needs a **Release** build for useful measurements:

```bash
cmake -S . -B build/perf -DLIBAMP_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/perf --parallel
build/perf/test/libamp_bench 2000000 200000
```

It covers idle/travelling keys, calibration range updates, HSV conversion,
Nexus slave keys, and live RGB effects. The keyboard workload uses ordinary
bindings so simultaneous presses cannot switch profiles or reset the device.
These are host timings including fixture overhead, not MCU cycles or USB
latency. For comparisons, build both revisions with the same compiler, flags,
configuration, and benchmark source, then alternate runs and compare medians:

```bash
python3 test/bench/compare.py path/to/baseline/libamp_bench path/to/candidate/libamp_bench --json build/perf/comparison.json
```

Small differences within run-to-run variation are inconclusive. On the target,
measure keyboard-tick cycles with the board's timer or cycle counter under the
same ADC, USB, and RGB load before claiming an end-to-end latency improvement.

The [aggressive performance report](docs/performance/aggressive.md) records the
changes from `codex-fable`, raw measurements, behavior checks, and code-size
tradeoffs at that revision; it is historical evidence, not a measurement of
subsequent changes. To compare the raw-key pipeline across two source checkouts:

```bash
python3 test/advanced_key/check_pipeline.py path/to/reference-checkout .
python3 test/advanced_key/check_pipeline.py path/to/reference-checkout . --cflags='-flto -ffast-math'
```

The `levelx` combinations need 32-bit host C/C++ libraries (LevelX on-flash words
must match the 32-bit firmware); `FILE_SYSTEM_TYPE` is 2 for FileX and 1 for
LittleFS:

```bash
cmake -S . -B b-fx-lx -DLIBAMP_BUILD_TESTS=ON '-DCMAKE_C_FLAGS=-m32 -DFILE_SYSTEM_TYPE=2 -DFLASH_LAYER=FLASH_LAYER_LEVELX' '-DCMAKE_CXX_FLAGS=-m32 -DFILE_SYSTEM_TYPE=2 -DFLASH_LAYER=FLASH_LAYER_LEVELX' && cmake --build b-fx-lx --parallel && ctest --test-dir b-fx-lx --output-on-failure   # FileX + LevelX
cmake -S . -B b-fx -DLIBAMP_BUILD_TESTS=ON '-DCMAKE_C_FLAGS=-m32 -DFILE_SYSTEM_TYPE=2' '-DCMAKE_CXX_FLAGS=-m32 -DFILE_SYSTEM_TYPE=2' && cmake --build b-fx --parallel && ctest --test-dir b-fx --output-on-failure                                   # FileX + direct flash
cmake -S . -B b-lfs-lx -DLIBAMP_BUILD_TESTS=ON '-DCMAKE_C_FLAGS=-m32 -DFILE_SYSTEM_TYPE=1 -DFLASH_LAYER=FLASH_LAYER_LEVELX' '-DCMAKE_CXX_FLAGS=-m32 -DFILE_SYSTEM_TYPE=1 -DFLASH_LAYER=FLASH_LAYER_LEVELX' && cmake --build b-lfs-lx --parallel && ctest --test-dir b-lfs-lx --output-on-failure   # LittleFS + LevelX
```

### 8.2 Firmware Build Checklist

Before building the target firmware, confirm that:

- `LIBAMP_INCLUDE_DIR` contains the intended `keyboard_config.h`;
- platform adapter, selected USB stack, and selected libamp backend sources are part of the firmware target;
- libamp and the math library are linked;
- the target compiler flags apply to both the application and libamp;
- USB DMA buffers, cache handling, and interrupt routing match the selected controller port;
- enabled feature macros have been verified with the selected backend.

### 8.3 Common Integration Failures

| Symptom | Check |
| --- | --- |
| `keyboard_config.h` cannot be found | Set `LIBAMP_INCLUDE_DIR` before `add_subdirectory(libamp)`. |
| mquickjs header generation fails | Install a host `gcc`; do not use only a cross compiler. |
| The device enumerates but no key is sent | Confirm `g_keyboard_tick` and `keyboard_task()` both run at `POLLING_RATE`, `keyboard_process()` runs in the foreground, the backend sources are included, and `usb_init()` is called after USB is initialized. |
| The analog key never changes state | Inspect raw samples, `g_analog_map`, buffer indices, calibration range, and normalization direction. |
| Reports stop after the first key press | Check the endpoint callback, DMA buffer placement, and cache handling. |
| Storage corrupts firmware or fails to mount | Recheck the reserved flash range, erase/program alignment, and the selected filesystem/flash-layer geometry. |
| USB fails after enabling a feature | Rebuild the descriptor/backend registration together and verify endpoint count, directions, report IDs, and DMA-safe buffers. |
