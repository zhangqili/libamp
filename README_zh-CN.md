# libamp

[English](README.md)

`libamp` 是一个面向模拟轴（霍尔磁轴、电感轴）与数字机械键盘的嵌入式键盘固件核心库，内置模拟按键处理（Rapid Trigger、死区、自动校准、滤波）、数字去抖、多层键位映射、动态键（DKS/Mod-Tap/SOCD）、宏、RGB 灯效、持久化存储及 USB HID 传输模板。

## 示例与参考工程

下列开源固件工程集成了 `libamp`：

- [oholeo-keyboard-v2-firmware](https://github.com/zhangqili/oholeo-keyboard-v2-firmware)
- [oholeo-keyboard-firmware](https://github.com/zhangqili/oholeo-keyboard-firmware)
- [at32-keyboard-firmware](https://github.com/zhangqili/at32-keyboard-firmware)
- [trinity-pad-firmware](https://github.com/zhangqili/trinity-pad-firmware)

---

## 1. 快速开始概览

完成 `libamp` 的最小系统移植只需以下 5 步：

1. **`keyboard_config.h`**：定义硬件规模与功能宏。
2. **`keyboard_user.c`**：提供默认键位表（`g_default_keymap`）、模拟通道映射（`g_analog_map`）和平台钩子。
3. **输入采样对接**：将 ADC 采样值推入 `g_adc_ringbufs`（或覆写 `advanced_key_read_raw()`）。
4. **USB 传输层**：接入随库附带的 CherryUSB 模板并调用 `usb_init()`。
5. **任务调度**：以 `POLLING_RATE` 频率周期调用 `keyboard_tick_inc(1)` 与 `keyboard_task()`，并在主循环中运行 `keyboard_process()`。

---

## 2. 构建最小模拟键盘

### 2.1 前置条件

- **CMake 3.14+** 以及主机侧 **`gcc` 编译器**（构建期间用于编译小型主机工具以生成 `mquickjs` 头文件，即使固件使用交叉编译也需要主机 `gcc`）。
- 目标 MCU 的 C 交叉编译工具链，以及 USB 设备协议栈（推荐使用随库提供模板的 [CherryUSB](https://github.com/cherry-embedded/CherryUSB)）。

将 `libamp` 添加为子模块：

```bash
git submodule add https://github.com/zhangqili/libamp.git third_party/libamp
git submodule update --init --recursive
```

### 2.2 在 CMake 中接入 `libamp`

在调用 `add_subdirectory()` **之前**设置 `LIBAMP_INCLUDE_DIR` 指向包含 `keyboard_config.h` 的目录，并链接 `libamp` 与数学库 `m`：

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

> **注意**：目标 MCU 的架构编译选项应同时作用于 `libamp` 与固件目标，但不要将 `libamp/CMakeLists.txt` 中的主机生成器改为交叉编译器。

### 2.3 创建 `keyboard_config.h`

以下是一个最小单键模拟键盘（6KRO USB 接口）的配置示例：

```c
#pragma once

#define LAYER_NUM               1
#define ADVANCED_KEY_NUM        1
#define KEY_NUM                 0
#define POLLING_RATE            1000  /* 高速 USB (8 kHz) 时设为 8000 并定义 CONFIG_USB_HS */

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

- **`POLLING_RATE`**：设置键盘时钟与 `keyboard_task()` 的调用频率（全速 USB 设为 `1000`；高速 USB 设为 `8000` 并开启 `CONFIG_USB_HS`）。`USB_POLLING_INTERVAL_MS` 保持为 `1`。
- **统一按键 ID**：高级（模拟）按键 ID 为 `0` 至 `ADVANCED_KEY_NUM - 1`；普通（数字）按键 ID 为 `ADVANCED_KEY_NUM` 至 `TOTAL_KEY_NUM - 1`。

### 2.4 定义键位表和模拟映射

创建 `platform/keyboard_user.c`，将按键 `0` 映射为 `KEY_A` 并绑定到环形缓冲区 `0`：

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

### 2.5 接入模拟量采样

任选以下一种方式向 `libamp` 提供原始 ADC 数据：

**方式一：向 `libamp` 环形缓冲区推入采样（默认）**  
默认 `advanced_key_read_raw()` 会读取 `g_adc_ringbufs[g_analog_map[key_id]]` 的均值。每次 ADC 转换完成时调用：

```c
void platform_adc_sample_ready(uint16_t buffer_index, uint16_t raw_sample)
{
    ringbuf_push(&g_adc_ringbufs[buffer_index], raw_sample);
}
```

**方式二：覆写 `advanced_key_read_raw()`**  
若平台已自行维护采样数组，可直接覆写读取钩子（无需使用 `g_adc_ringbufs` 和 `g_analog_map`，注意不可阻塞）：

```c
AnalogRawValue advanced_key_read_raw(AdvancedKey *key)
{
    return platform_read_analog_key(key->key.id);
}
```

### 2.6 集成 USB 传输层

按 [CherryUSB 移植指南](https://cherryusb.readthedocs.io/en/latest/quick_start/transplant.html) 完成底层驱动适配后，将 `libamp/usb/template/cherryusb/` 下的源码加入固件目标。该模板会根据 `keyboard_config.h` 自动生成描述符并实现 HID 发送回调：

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

初始化 USB 外设后调用模板的 `usb_init()`：

```c
#include "usbd_user.h"

usb_init(0, PLATFORM_USB_CONTROLLER_BASE);
```

### 2.7 初始化并运行任务

1. 初始化外设后调用 `keyboard_init()`，延时等待首批 ADC 采样稳定后调用一次 `analog_calibrate()` 记录按键松开时的基准值。
2. 在频率为 `POLLING_RATE` 的定时器中断或调度器中调用 `keyboard_tick_inc(1)` 和 `keyboard_task()`。
3. 在前台 `while (1)` 主循环中调用 `keyboard_process()`。

```c
int main(void)
{
    platform_init();
    platform_input_start();
    platform_usb_prepare();

    keyboard_init();
    usb_init(0, PLATFORM_USB_CONTROLLER_BASE);

    /* 等待 ADC 采样稳定后校准初始松开位置 */
    platform_delay_ms(10);
    analog_calibrate();

    while (1) {
        keyboard_process();
    }
}

/* 以 POLLING_RATE Hz 周期调用 */
void platform_keyboard_tick(void)
{
    keyboard_tick_inc(1);
    keyboard_task();
}
```

### 2.8 验证第一个按键

1. 烧录固件，确认主机成功识别出一个 USB HID 键盘。
2. 检查按键移动时原始采样值是否在行程两端平稳变化。
3. 按下并释放按键，确认主机收到 `A` 键。若触发方向颠倒，请调整传感器极性或归一化方向，不要手动反转按键事件。

---

## 3. 完善输入层

### 3.1 多通道 ADC 与多路复用器

将 `ADVANCED_KEY_NUM` 设为模拟按键总数，`ANALOG_BUFFER_LENGTH` 设为独立采样缓冲通道数。通过 `g_analog_map` 维护逻辑按键 ID 到采样缓冲区的映射，从而将硬件走线/多路复用器扫描顺序与键位 ID 解耦。

### 3.2 校准、非线性查找表（LUT）与滤波

- **校准**：`analog_calibrate()` 在按键未按下时采集静止边界，并将方向设为 `ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED`；后续按下过程会自动判定方向并扩展下压边界。
- **非线性磁轴 LUT**：默认 `advanced_key_normalize()` 执行线性归一化。若需拟合非线性磁场曲线，可修改 `tools/lut_generator/lut_generator.py` 中的磁铁几何常量（`R`, `L`, `Z_START`, `Z_END`, `LUT_LENGTH`）生成查找表与 `advanced_key_normalize()` 覆写源码：
  ```bash
  python3 -m pip install numpy
  python3 tools/lut_generator/lut_generator.py > analog_lut.c
  ```
  将生成的 `analog_lut.c` 加入固件编译即可。
- **滤波**：开启 `FILTER_ENABLE` 并配置 `FILTER_TYPE`、`FILTER_DOMAIN`、`FILTER_LOWPASS_ALPHA`（默认 `0.5f`）及可选的 `FILTER_HYSTERESIS_ENABLE` 以抑制采样噪声。

### 3.3 普通（数字）按键

增大 `KEY_NUM` 并扩展 `g_default_keymap`。普通按键在键位表中的 ID 从 `ADVANCED_KEY_NUM` 开始，而 `g_keyboard_keys` 数组下标为 `0` 至 `KEY_NUM - 1`。覆写 `keyboard_scan()` 将矩阵或 GPIO 状态传入 `keyboard_key_update()`（内部按 `DEBOUNCE_*` 周期参数自动执行去抖）：

```c
void keyboard_scan(void)
{
    bool pressed = platform_gpio_read(0);
    keyboard_key_update(&g_keyboard_keys[0], pressed);
}
```

### 3.4 旋转编码器

启用 `ENCODER_ENABLE`、设置 `ENCODER_NUM`，并在 `KEY_NUM` 中为每个编码器预留两个普通按键 ID（顺时针 `cw_id` 与逆时针 `ccw_id`）：

```c
#include "encoder.h"

Encoder g_encoders[ENCODER_NUM] = {
    { .cw_id = ADVANCED_KEY_NUM + 0, .ccw_id = ADVANCED_KEY_NUM + 1 },
};
```

驱动层可通过 `encoder_input(id, count)` 传入累计计数，或通过 `encoder_input_delta(id, delta)` 传入相对增量。

### 3.5 平台事件钩子（复位、Bootloader 与自定义键）

按需在 `keyboard_user.c` 中覆写以下钩子：

```c
void keyboard_reboot(void);
void keyboard_jump_to_bootloader(void);
void keyboard_delay(uint32_t ms); /* 使用 Kalman 滤波校准时需实现 */
void keyboard_user_event_handler(KeyboardEvent event); /* 处理 KEY_USER 自定义键码 */
```

覆写 `keyboard_user_event_handler()` 时，请在按下和抬起分支中分别调用 `keyboard_key_event_down_dispatch(event)` 与 `keyboard_key_event_up_dispatch(event)`，以保留统计、宏和脚本触发。

---

## 4. 持久化存储与统计

### 4.1 闪存分区配置

启用 `STORAGE_ENABLE` 以保存配置文件（默认使用 LittleFS；定义 `FILE_SYSTEM_TYPE` 为 `FILE_SYSTEM_FILEX` 可切换至 FileX FAT，`FLASH_LAYER` 可选 `FLASH_LAYER_DIRECT` 或带磨损均衡的 `FLASH_LAYER_LEVELX`）。实现 `driver.h` 中的闪存回调：

```c
int flash_read(uint32_t address, uint32_t size, uint8_t *data);
int flash_write(uint32_t address, uint32_t size, const uint8_t *data);
int flash_erase(uint32_t address, uint32_t size);
```

根据预留的独立 Flash 分区配置 `FS_FLASH_OFFSET`、`FS_READ_SIZE`、`FS_PROG_SIZE`、`FS_BLOCK_SIZE`、`FS_BLOCK_COUNT` 和 `FS_CACHE_SIZE`，并确保在调用 `keyboard_init()` **之前**完成 Flash 初始化。

### 4.2 按键统计

启用 `COUNTER_ENABLE` 可统计每键敲击次数；配合 `STORAGE_ENABLE` 与 `RECORD_PERSIST_ENABLE` 可将统计次数和运行时间自动持久化至 `system/stat`（通过 `RECORD_STATISTICS_SAVE_INTERVAL_MS` 与 `RECORD_STATISTICS_IDLE_SAVE_MS` 控制保存周期）。

---

## 5. RGB 灯光

启用 `RGB_ENABLE`、定义 `RGB_NUM`，并实现 LED 驱动回调与位置映射表：

```c
int led_set(uint16_t index, uint8_t red, uint8_t green, uint8_t blue);
int led_flush(void);
```

定义 LED 映射与物理坐标表（可使用 `UNIT_TO_UM()` 将标准键距单位转换为微米坐标）：

| 表名 | 索引范围 | 元素含义 | 作用 |
| --- | --- | --- | --- |
| `g_rgb_mapping` | LED ID（`0` .. `RGB_NUM - 1`） | 按键 ID | 将逻辑 LED 关联到对应按键。 |
| `g_rgb_locations` | LED ID（`0` .. `RGB_NUM - 1`） | `RGBLocation { x, y }` | 该 LED 的物理坐标。 |
| `g_rgb_inverse_mapping` | 按键 ID（`0` .. `TOTAL_KEY_NUM - 1`） | LED ID | 默认在 `rgb_init()` 中自动生成（除非定义 `RGB_CUSTOM_INVERSE_MAPPING`）。 |

- **帧率与 Gamma**：设置 `RGB_MAX_FRAME_RATE`（建议取能整除 `POLLING_RATE` 的值）限制最高渲染帧率；启用 `RGB_GAMMA_ENABLE`（及可选 `RGB_GAMMA_LUT_ENABLE`）开启 Gamma 校正。调整全局亮度请调用 `rgb_set_brightness(0..255)`。
- **开机灯效**：`rgb_init_flash()` 或 `rgb_flash()` 依赖时钟推进，必须在启动周期性 `keyboard_tick_inc` 之后调用。

---

## 6. 扩展 USB 设备接口

使用内置 CherryUSB 模板时，在 `keyboard_config.h` 中开启相应宏即可扩展 USB 接口：

- **功能接口**：`NKRO_ENABLE`（全键无冲）、`EXTRAKEY_ENABLE`（多媒体/系统控制）、`MOUSE_ENABLE`（鼠标键）、`RAW_ENABLE`（上位机通讯）、`MIDI_ENABLE`、`JOYSTICK_ENABLE`、`DIGITIZER_ENABLE`、`GAMEPAD_ENABLE`。
- **端点节省**：硬件端点数量受限时，可开启 `SHARED_EP_ENABLE` 共用中断 IN 端点；若控制器支持相同编号的 IN/OUT 端点，可开启 `USB_ENDPOINTS_ARE_REORDERABLE`，并根据硬件上限设置 `MAX_ENDPOINTS`。
- **自定义序列号**：定义 `SERIAL_NUMBER_USE_CUSTOM` 并覆写 `usb_descriptor_get_serial_number(char *buf, size_t size)` 可使用芯片唯一 ID 作为 USB 序列号。

---

## 7. 高级运行时功能

- **动态键与宏**：启用 `DYNAMICKEY_ENABLE`（支持 Mod-Tap、Toggle、DKS、Mutex/SOCD）和 `MACRO_ENABLE`（宏录制与播放）。
- **脚本引擎（`mquickjs`）**：启用 `SCRIPT_ENABLE`（依赖 `STORAGE_ENABLE`），选择 `SCRIPT_RUNTIME_STRATEGY` 并配置 `SCRIPT_MEMORY_SIZE`。
- **MTP 文件传输**：启用 `MTP_ENABLE` 可通过 USB MTP 协议暴露内部文件系统。
- **64 位长时时钟**：若需统计超过 `2^32` 个 tick（8 kHz 下约 6.2 天）的运行时间，可在配置中定义 `#define KEYBOARD_TICK_64BIT_ENABLE 1`。
- **Nexus 多板互联（`NEXUS_ENABLE`）**：用于分体或多板拼接键盘。主机持有 USB 与键位表并定义 `g_nexus_slave_configs[NEXUS_SLAVE_NUM]`；从机定义 `NEXUS_IS_SLAVE 1` 扫描本地按键。平台实现 `driver.h` 中的 `nexus_send()` 与 `nexus_report()`，并在收到完整帧时调用 `nexus_process_buffer(slave_id, frame, len)`。

---

## 8. 排错与参考检查表

### 8.1 执行上下文规范

| 上下文 | 核心入口 | 约束说明 |
| --- | --- | --- |
| **键盘时钟周期（`POLLING_RATE`）** | `keyboard_tick_inc(1)`、`keyboard_task()` | 不可重入；此路径触发的 USB/Nexus 发送函数不得阻塞。 |
| **前台主循环（`while(1)`）** | `keyboard_process()` | 处理按键事件轮询、RGB 渲染、延迟校准及统计后台保存。 |
| **中断 / 传输回调** | `ringbuf_push()`、`encoder_input()`、`packet_process_buffer()` | 注意：`packet_process_buffer()`（Raw HID）及绑定按键的保存/切换配置操作默认同步访问 Flash（定义 `KEYBOARD_OPERATION_POLLING` 可将按键触发的存储操作移至 `keyboard_process()`）。 |

### 8.2 固件构建检查表

- [ ] `add_subdirectory(libamp)` 之前已正确设置 `LIBAMP_INCLUDE_DIR`。
- [ ] 平台适配代码、CherryUSB 核心驱动及 `libamp/usb/template/cherryusb/` 模板源码均已加入构建目标。
- [ ] 已链接 `libamp` 和数学库 `m`。
- [ ] USB 收发缓冲区位于非缓存（Non-cacheable）或已正确维护 Cache 一致性的 DMA 内存区域。

### 8.3 常见集成问题排查

| 现象 | 排查方向 |
| --- | --- |
| 编译报错找不到 `keyboard_config.h` | 在 `add_subdirectory(libamp)` 前设置 `LIBAMP_INCLUDE_DIR`。 |
| `mquickjs` 头文件生成失败 | 确认主机已安装原生 `gcc`，而非仅有交叉编译器。 |
| 设备已枚举但无按键输出 | 检查 `keyboard_tick_inc(1)` 与 `keyboard_task()` 是否按 `POLLING_RATE` 运行、`keyboard_process()` 是否在主循环执行、以及是否调用了 `usb_init()`。 |
| 模拟按键状态不变化 | 检查原始采样值、`g_analog_map`、是否调用了 `analog_calibrate()` 以及归一化方向。 |
| 触发一次按键后后续报告卡死 | 检查 USB 端点完成中断、DMA 缓冲区地址对齐及 Cache 维护。 |
| 挂载存储失败或破坏固件 | 检查 `FS_FLASH_OFFSET` 与预留 Flash 区间是否同固件重叠，以及 `flash_*` 回调的擦写对齐。 |

### 8.4 运行主机单元测试

如需在开发机上运行 `libamp` 单元测试以验证核心逻辑：

```bash
cmake -S third_party/libamp -B build/libamp-tests -DLIBAMP_BUILD_TESTS=ON
cmake --build build/libamp-tests --parallel
ctest --test-dir build/libamp-tests --output-on-failure
```
