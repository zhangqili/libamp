# libamp

[简体中文](README_zh-CN.md)

`libamp` is an embedded keyboard firmware library designed for analog (Hall-effect / inductive) and digital mechanical keyboards. It provides analog key processing (Rapid Trigger, dead zones, calibration, filtering), digital key debouncing, multi-layer keymaps, dynamic keys, macros, RGB lighting, persistent storage, and USB HID transport templates.

## Example and Reference Firmware

The following open-source firmware projects integrate `libamp`:

- [oholeo-keyboard-v2-firmware](https://github.com/zhangqili/oholeo-keyboard-v2-firmware)
- [oholeo-keyboard-firmware](https://github.com/zhangqili/oholeo-keyboard-firmware)
- [at32-keyboard-firmware](https://github.com/zhangqili/at32-keyboard-firmware)
- [trinity-pad-firmware](https://github.com/zhangqili/trinity-pad-firmware)

---

## 1. Quick Start Overview

To bring up a minimal keyboard with `libamp`, you only need:

1. **`keyboard_config.h`**: Compile-time hardware and feature configuration.
2. **`keyboard_user.c`**: Default keymap (`g_default_keymap`), analog channel mapping (`g_analog_map`), and platform hooks.
3. **Input feeding**: Push ADC samples into `g_adc_ringbufs` (or override `advanced_key_read_raw()`).
4. **USB transport**: Link the bundled CherryUSB template and call `usb_init()`.
5. **Runtime loop**: Call `keyboard_tick_inc(1)` + `keyboard_task()` at `POLLING_RATE`, and run `keyboard_process()` in the main loop.

---

## 2. Build a Minimal Analog Keyboard

### 2.1 Prerequisites

- **CMake 3.14+** and a **host `gcc` compiler** (used during the build to generate `mquickjs` headers, even when cross-compiling for an MCU).
- A target C toolchain and a USB device stack ([CherryUSB](https://github.com/cherry-embedded/CherryUSB) is recommended and supported out-of-the-box).

Add `libamp` as a Git submodule:

```bash
git submodule add https://github.com/zhangqili/libamp.git third_party/libamp
git submodule update --init --recursive
```

### 2.2 Add `libamp` to CMake

Set `LIBAMP_INCLUDE_DIR` to the directory containing `keyboard_config.h` **before** calling `add_subdirectory()`, then link `libamp` and the math library `m`:

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

> **Note:** Apply your target MCU architecture flags to both `libamp` and your firmware target, but do not point the host tool generator in `libamp/CMakeLists.txt` at your cross-compiler.

### 2.3 Create `keyboard_config.h`

Below is a minimal configuration for a single analog key and a standard 6KRO USB keyboard interface:

```c
#pragma once

#define LAYER_NUM               1
#define ADVANCED_KEY_NUM        1
#define KEY_NUM                 0
#define POLLING_RATE            1000  /* Set to 8000 with CONFIG_USB_HS for 8 kHz HS USB */

#define DEBOUNCE_PRESS          0
#define DEBOUNCE_PRESS_EAGER    1
#define DEBOUNCE_RELEASE        0
#define DEBOUNCE_RELEASE_EAGER  1

#define LUT_LENGTH                          4096
#define DEFAULT_ADVANCED_KEY_MODE           ADVANCED_KEY_ANALOG_NORMAL_MODE
#define DEFAULT_CALIBRATION_MODE            ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED
#define DEFAULT_TRIGGER_DISTANCE            0.10f
#define DEFAULT_RELEASE_DISTANCE            0.10f
#define DEFAULT_UPPER_DEADZONE              0.01f
#define DEFAULT_LOWER_DEADZONE              0.01f
#define DEFAULT_ACTIVATION_VALUE            0.50f
#define DEFAULT_DEACTIVATION_VALUE          0.49f
#define DEFAULT_ESTIMATED_RANGE             500

#define RING_BUF_LEN             8
#define ANALOG_BUFFER_LENGTH     1

#define USB_POLLING_INTERVAL_MS     1
#define FIXED_CONTROL_ENDPOINT_SIZE 0x40
#define FIXED_NUM_CONFIGURATIONS    1
#define VENDOR_ID                   0x1234
#define PRODUCT_ID                  0x0001
#define DEVICE_VER                  0x0001
#define MANUFACTURER                "Example Manufacturer"
#define PRODUCT                     "Example Analog Keyboard"
#define SERIAL_NUMBER               "0001"
```

- **`POLLING_RATE`**: Frequency of `keyboard_task()` in Hz (`1000` for Full-Speed USB, `8000` with `CONFIG_USB_HS` for High-Speed USB). Keep `USB_POLLING_INTERVAL_MS` at `1`.
- **Key ID space**: Advanced (analog) keys use IDs `0` to `ADVANCED_KEY_NUM - 1`. Digital keys use IDs `ADVANCED_KEY_NUM` to `TOTAL_KEY_NUM - 1`.

### 2.4 Define the Keymap and Analog Mapping

Create `platform/keyboard_user.c` to map key `0` to `KEY_A` and bind it to ADC ring buffer `0`:

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

### 2.5 Feed Analog Samples

Choose one of two ways to provide raw ADC readings:

**Option A: Push into `libamp` ring buffers (Default)**  
By default, `advanced_key_read_raw()` averages `g_adc_ringbufs[g_analog_map[key_id]]`. Push samples when ADC conversions complete:

```c
void platform_adc_sample_ready(uint16_t buffer_index, uint16_t raw_sample)
{
    ringbuf_push(&g_adc_ringbufs[buffer_index], raw_sample);
}
```

**Option B: Override `advanced_key_read_raw()`**  
If your platform already buffers ADC values by key ID, override the weak reader directly (no `g_adc_ringbufs` or `g_analog_map` needed, must not block):

```c
AnalogRawValue advanced_key_read_raw(AdvancedKey *key)
{
    return platform_read_analog_key(key->key.id);
}
```

### 2.6 Integrate USB Transport

After porting [CherryUSB](https://cherryusb.readthedocs.io/en/latest/quick_start/transplant.html) to your MCU, add the bundled CherryUSB template to your firmware target. It builds descriptors from `keyboard_config.h` and implements the HID callbacks automatically:

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

Initialize your USB peripheral, then start the template:

```c
#include "usbd_user.h"

usb_init(0, PLATFORM_USB_CONTROLLER_BASE);
```

### 2.7 Initialize and Run Tasks

1. Initialize hardware, call `keyboard_init()`, wait briefly for ADC readings to stabilize, and call `analog_calibrate()` once with keys at rest.
2. From a timer interrupt or scheduler running at `POLLING_RATE`, call `keyboard_tick_inc(1)` and `keyboard_task()`.
3. In the foreground `while (1)` loop, call `keyboard_process()`.

```c
int main(void)
{
    platform_init();
    platform_input_start();
    platform_usb_prepare();

    keyboard_init();
    usb_init(0, PLATFORM_USB_CONTROLLER_BASE);

    /* Wait briefly for first ADC samples to settle, then calibrate resting bounds */
    platform_delay_ms(10);
    analog_calibrate();

    while (1) {
        keyboard_process();
    }
}

/* Called periodically at POLLING_RATE Hz */
void platform_keyboard_tick(void)
{
    keyboard_tick_inc(1);
    keyboard_task();
}
```

### 2.8 Verify the First Key Press

1. Flash the firmware and confirm that a USB HID keyboard enumerates on the host.
2. Verify that raw ADC readings change smoothly across the key travel.
3. Press and release the key to confirm the host receives `A`. If press/release is inverted, reverse the ADC polarity or normalization direction rather than swapping key events.

---

## 3. Input Layer Configuration

### 3.1 Multiple Analog Keys and Multiplexers

Set `ADVANCED_KEY_NUM` to the total number of analog keys and `ANALOG_BUFFER_LENGTH` to the number of ADC sample buffers. Use `g_analog_map` to associate each key ID with its buffer index so hardware mux/channel ordering stays decoupled from logical key IDs.

### 3.2 Calibration, Non-linear LUT, and Filtering

- **Calibration**: `analog_calibrate()` records the resting values and resets calibration direction to `ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED`; pressing keys extends the travel range automatically.
- **Non-linear Sensor LUT**: By default, `advanced_key_normalize()` performs linear normalization. For non-linear magnetic flux curves, edit the magnet geometry constants (`R`, `L`, `Z_START`, `Z_END`, `LUT_LENGTH`) in `tools/lut_generator/lut_generator.py` and generate a lookup table + `advanced_key_normalize()` override:
  ```bash
  python3 -m pip install numpy
  python3 tools/lut_generator/lut_generator.py > analog_lut.c
  ```
  Compile `analog_lut.c` into your firmware target.
- **Filtering**: Enable `FILTER_ENABLE` and configure `FILTER_TYPE`, `FILTER_DOMAIN`, `FILTER_LOWPASS_ALPHA` (default `0.5f`), and optional `FILTER_HYSTERESIS_ENABLE` to suppress ADC noise.

### 3.3 Digital Keys

Increase `KEY_NUM` and extend `g_default_keymap`. Digital key IDs start at `ADVANCED_KEY_NUM`, while `g_keyboard_keys` is indexed from `0` to `KEY_NUM - 1`. Override `keyboard_scan()` to feed physical pin/matrix states into `keyboard_key_update()`, which applies debouncing (`DEBOUNCE_*` in ticks):

```c
void keyboard_scan(void)
{
    bool pressed = platform_gpio_read(0);
    keyboard_key_update(&g_keyboard_keys[0], pressed);
}
```

### 3.4 Rotary Encoders

Enable `ENCODER_ENABLE`, set `ENCODER_NUM`, and reserve two ordinary keys per encoder in `KEY_NUM` (one for clockwise, one for counter-clockwise):

```c
#include "encoder.h"

Encoder g_encoders[ENCODER_NUM] = {
    { .cw_id = ADVANCED_KEY_NUM + 0, .ccw_id = ADVANCED_KEY_NUM + 1 },
};
```

Feed absolute counts via `encoder_input(id, count)` or relative steps via `encoder_input_delta(id, delta)`.

### 3.5 Platform Hooks (Reset, Bootloader, Custom Keys)

Override these hooks in `keyboard_user.c` as needed:

```c
void keyboard_reboot(void);
void keyboard_jump_to_bootloader(void);
void keyboard_delay(uint32_t ms); /* Required if using Kalman filter calibration */
void keyboard_user_event_handler(KeyboardEvent event); /* Handles KEY_USER keycodes */
```

When overriding `keyboard_user_event_handler()`, call `keyboard_key_event_down_dispatch(event)` on key-down and `keyboard_key_event_up_dispatch(event)` on key-up so statistics, macros, and scripts continue to work.

---

## 4. Persistent Storage and Statistics

### 4.1 Flash Storage

Enable `STORAGE_ENABLE` to persist profiles and keymaps (uses LittleFS by default, or FileX FAT when `FILE_SYSTEM_TYPE` is `FILE_SYSTEM_FILEX`; set `FLASH_LAYER` to `FLASH_LAYER_DIRECT` or `FLASH_LAYER_LEVELX`). Implement the flash callbacks declared in `driver.h`:

```c
int flash_read(uint32_t address, uint32_t size, uint8_t *data);
int flash_write(uint32_t address, uint32_t size, const uint8_t *data);
int flash_erase(uint32_t address, uint32_t size);
```

Configure `FS_FLASH_OFFSET`, `FS_READ_SIZE`, `FS_PROG_SIZE`, `FS_BLOCK_SIZE`, `FS_BLOCK_COUNT`, and `FS_CACHE_SIZE` to match a dedicated flash partition. Ensure flash hardware is initialized **before** `keyboard_init()`.

### 4.2 Key Press Statistics

Enable `COUNTER_ENABLE` for runtime key counters, and add `STORAGE_ENABLE` + `RECORD_PERSIST_ENABLE` to periodically save press counts and uptime to flash (`RECORD_STATISTICS_SAVE_INTERVAL_MS` and `RECORD_STATISTICS_IDLE_SAVE_MS`).

---

## 5. RGB Lighting

Enable `RGB_ENABLE`, define `RGB_NUM`, and implement the LED driver callbacks and layout tables:

```c
int led_set(uint16_t index, uint8_t red, uint8_t green, uint8_t blue);
int led_flush(void);
```

Define the LED mapping and physical position tables (use `UNIT_TO_UM()` for key-unit coordinates):

| Table | Index | Element | Purpose |
| --- | --- | --- | --- |
| `g_rgb_mapping` | LED ID (`0` .. `RGB_NUM - 1`) | Key ID | Associates an LED with a key. |
| `g_rgb_locations` | LED ID (`0` .. `RGB_NUM - 1`) | `RGBLocation { x, y }` | Physical coordinates of the LED. |
| `g_rgb_inverse_mapping` | Key ID (`0` .. `TOTAL_KEY_NUM - 1`) | LED ID | Auto-generated in `rgb_init()` unless `RGB_CUSTOM_INVERSE_MAPPING` is set. |

- **Frame rate & Gamma**: Set `RGB_MAX_FRAME_RATE` (divisor of `POLLING_RATE`) to cap rendering frequency. Enable `RGB_GAMMA_ENABLE` (and optional `RGB_GAMMA_LUT_ENABLE`) for gamma correction. Use `rgb_set_brightness(0..255)` to adjust global brightness.
- **Startup animation**: Call `rgb_init_flash()` or `rgb_flash()` only **after** the periodic tick (`keyboard_tick_inc`) has started.

---

## 6. Extend USB Interfaces

With the bundled CherryUSB template, define any of the following macros in `keyboard_config.h` to enable extra interfaces:

- `NKRO_ENABLE`, `EXTRAKEY_ENABLE`, `MOUSE_ENABLE`, `RAW_ENABLE` (host configurator communication), `MIDI_ENABLE`, `JOYSTICK_ENABLE`, `DIGITIZER_ENABLE`, `GAMEPAD_ENABLE`.
- **Endpoint conservation**: Enable `SHARED_EP_ENABLE` to multiplex HID reports over fewer endpoints, and `USB_ENDPOINTS_ARE_REORDERABLE` if the USB controller supports sharing IN/OUT endpoint numbers. Set `MAX_ENDPOINTS` to your controller's limit.
- **Dynamic serial number**: Define `SERIAL_NUMBER_USE_CUSTOM` and override `usb_descriptor_get_serial_number(char *buf, size_t size)` to return a hardware UID string.

---

## 7. Advanced Runtime Features

- **Dynamic Keys & Macros**: Enable `DYNAMICKEY_ENABLE` (Mod-Tap, Toggle, DKS, Mutex/SOCD) and `MACRO_ENABLE`.
- **Scripting (`mquickjs`)**: Enable `SCRIPT_ENABLE` (requires `STORAGE_ENABLE`), select `SCRIPT_RUNTIME_STRATEGY`, and configure `SCRIPT_MEMORY_SIZE`.
- **MTP File Access**: Enable `MTP_ENABLE` to expose the filesystem over USB MTP.
- **64-bit Uptime Clock**: Set `KEYBOARD_TICK_64BIT_ENABLE 1` if your application needs tick timestamps that do not wrap after `2^32` ticks (~6.2 days at 8 kHz).
- **Nexus Multi-Board Link (`NEXUS_ENABLE`)**: Connects a master board (holding USB and keymap) with one or more slave boards (`NEXUS_IS_SLAVE 1`). Implement `nexus_send()` and `nexus_report()` from `driver.h`, feed received frames to `nexus_process_buffer(slave_id, frame, len)`, and define `g_nexus_slave_configs[NEXUS_SLAVE_NUM]` on the master.

---

## 8. Troubleshooting & Reference

### 8.1 Execution Context Rules

| Context | Functions | Requirements |
| --- | --- | --- |
| **Keyboard Tick (`POLLING_RATE`)** | `keyboard_tick_inc(1)`, `keyboard_task()` | Must not be re-entered; USB/Nexus send calls inside must not block. |
| **Foreground Loop (`while(1)`)** | `keyboard_process()` | Handles RGB rendering, deferred calibration, and background storage sync. |
| **Interrupt / Transport Callbacks** | `ringbuf_push()`, `encoder_input()`, `packet_process_buffer()` | Note: `packet_process_buffer()` (Raw HID) and key-bound save/profile actions access flash synchronously unless deferred (`KEYBOARD_OPERATION_POLLING` defers key-bound storage ops to `keyboard_process()`). |

### 8.2 Firmware Build Checklist

- [ ] `LIBAMP_INCLUDE_DIR` is set before `add_subdirectory(libamp)`.
- [ ] Platform adapter, USB stack, and CherryUSB template sources are added to the target.
- [ ] Both `libamp` and `m` (math library) are linked.
- [ ] USB DMA buffers are placed in non-cached / DMA-accessible RAM as required by the MCU.

### 8.3 Common Integration Issues

| Symptom | Check |
| --- | --- |
| `keyboard_config.h` not found | Set `LIBAMP_INCLUDE_DIR` before `add_subdirectory()`. |
| `mquickjs` header build fails | Ensure host `gcc` is installed alongside your cross-compiler. |
| USB enumerates but no keys send | Confirm `keyboard_tick_inc(1)` and `keyboard_task()` run at `POLLING_RATE`, `keyboard_process()` runs in `main`, and `usb_init()` was called. |
| Analog key never triggers | Check raw ADC values, `g_analog_map`, `analog_calibrate()` call, and travel direction. |
| Reports stop after first key press | Check USB endpoint interrupt routing, DMA buffer alignment, and cache coherency. |
| Flash mount fails or corrupts firmware | Verify `FS_FLASH_OFFSET`, block size/count, and erase/program alignment in `flash_*` callbacks. |

### 8.4 Running Host Unit Tests

To run `libamp`'s built-in unit tests on your host machine:

```bash
cmake -S third_party/libamp -B build/libamp-tests -DLIBAMP_BUILD_TESTS=ON
cmake --build build/libamp-tests --parallel
ctest --test-dir build/libamp-tests --output-on-failure
```
