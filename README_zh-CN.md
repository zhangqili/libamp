# libamp

[English](README.md)

## 示例与参考工程

下列开源固件工程集成了 libamp：

- [oholeo-keyboard-v2-firmware](https://github.com/zhangqili/oholeo-keyboard-v2-firmware)
- [oholeo-keyboard-firmware](https://github.com/zhangqili/oholeo-keyboard-firmware)
- [at32-keyboard-firmware](https://github.com/zhangqili/at32-keyboard-firmware)
- [trinity-pad-firmware](https://github.com/zhangqili/trinity-pad-firmware)

# 将 libamp 移植到新的键盘平台

## 1. 快速开始

### 1.1 最终效果

完成最小移植后，模拟按键在校准范围内移动时会产生正常的键盘按下和释放报告。最小移植包含：

- `keyboard_config.h` 配置头文件；
- 模拟量采样流程和默认键位表；
- USB HID 键盘传输层；
- 周期性递增 `g_keyboard_tick` 并调用 `keyboard_task()`，以及在前台调用
  `keyboard_process()`。

### 1.2 移植模型

在 `keyboard_config.h` 中配置 libamp。建议自行创建 `keyboard_user.c`，在其中统一放置键位表、进一步的平台配置和适配函数的实现。

## 2. 构建最小模拟键盘

### 2.1 前置条件

主机需要 CMake 3.14 或更高版本，以及可执行的主机 `gcc`。libamp 会在配置/编译过程中构建一个生成 mquickjs 头文件的小型主机工具；即使固件使用交叉编译，它也使用主机编译器。最终生成固件还需要目标 C 工具链和 USB 设备协议栈。

如果源码树中还没有 libamp，可以将它添加为子模块：

```bash
git submodule add https://github.com/zhangqili/libamp.git third_party/libamp
git submodule update --init --recursive
```

### 2.2 在构建系统中加入 libamp

在添加库之前，先指定包含 `keyboard_config.h` 的目录。将应用源码加入固件目标，然后链接 libamp 和数学库。USB 后端源码在 2.6 节中加入。

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

除上述最小目标外，还需加入选用的 USB 协议栈、控制器端口和后端源码。如果目标需要架构相关的编译选项，应同时应用到 `libamp` 和应用目标。不要把主机生成器改为使用交叉编译器：`libamp/CMakeLists.txt` 会特意使用主机 `gcc` 配置该工具。

### 2.3 创建 `keyboard_config.h`

下面是一个最小的纯模拟按键配置：一层、一个高级（模拟）按键，以及默认的 6KRO 键盘接口。发布硬件前请替换为自己的 USB 标识符。

```c
#pragma once

#define LAYER_NUM               1
#define ADVANCED_KEY_NUM        1
#define KEY_NUM                 0
#define POLLING_RATE            1000  /* 使用 CONFIG_USB_HS 时设为 8000。 */

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

`POLLING_RATE` 同时是 USB 键盘报告率、`g_keyboard_tick` 的递增频率和 `keyboard_task()` 的调用频率。USB 全速键盘使用 `1000`（1 kHz）；USB 高速键盘使用 `8000`（8 kHz），并定义 `CONFIG_USB_HS`。`USB_POLLING_INTERVAL_MS` 保持为 `1`：在全速下表示 1 ms，在高速下表示一个 125 us 微帧。

`ADVANCED_KEY_NUM` 和 `KEY_NUM` 定义两类输入。高级按键接收连续值，ID 范围为 `0` 至 `ADVANCED_KEY_NUM - 1`；普通按键接收布尔状态，ID 范围为 `ADVANCED_KEY_NUM` 至 `TOTAL_KEY_NUM - 1`。`g_default_keymap[layer][id]` 使用这套统一的 ID。

描述符生成器会使用该头文件中的 USB 标识符和接口宏。在最小 USB 设备正常工作前，不要启用 `NKRO_ENABLE`、`RAW_ENABLE`、`SHARED_EP_ENABLE` 或其他可选接口。

### 2.4 定义键位表和模拟映射

创建 `platform/keyboard_user.c`，并包含定义数据所需的 libamp 头文件。第一个模拟按键的 ID 是 `0`，以下示例将它映射为 `A` 键，并从环形缓冲区 `0` 读取数据。

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

`g_default_keymap` 必须为每一层中的每一个按键提供一个条目。下面的缓冲区输入方式使用 `g_analog_map` 将高级按键 ID 映射到 `g_adc_ringbufs` 的索引；它本身不会选择物理 ADC 通道。

### 2.5 采集并归一化模拟量

libamp 提供两种模拟量输入方式，平台应选择其中一种。

**1. 向 libamp 环形缓冲区填充值。** 这是默认方式：`advanced_key_read_raw()` 返回 `g_adc_ringbufs[g_analog_map[key_id]]` 的平均值。每当平台收到一次转换完成的采样时，都应将数据写入对应的环形缓冲区：

```c
void platform_adc_sample_ready(uint16_t buffer_index, uint16_t raw_sample)
{
    ringbuf_push(&g_adc_ringbufs[buffer_index], raw_sample);
}
```

**2. 覆写 `advanced_key_read_raw()`。** 当采样值已保存在平台管理的数据结构中，或读取过程需要板级专用的流程时，使用这种方式。该钩子会接收按键指针，因此可通过按键 ID 选择数据源：

```c
AnalogRawValue advanced_key_read_raw(AdvancedKey *key)
{
    return platform_read_analog_key(key->key.id);
}
```

这种方式不需要向 `g_adc_ringbufs` 填充数据，也不需要为读取操作定义 `g_analog_map`。它必须在不阻塞键盘时钟周期的情况下返回最新原始采样值。

默认的 `advanced_key_normalize()` 使用校准边界完成线性转换。只有传感器需要非线性传递函数时才应覆写它。覆写版本必须将结果限制在 `ANALOG_VALUE_MIN` 到 `ANALOG_VALUE_MAX` 之间，并保持校准得到的方向。一个线性实现如下：

```c
AnalogValue advanced_key_normalize(AdvancedKey *key, AnalogRawValue sample)
{
    const int32_t upper = key->config.upper_bound;
    const int32_t lower = key->config.lower_bound;
    const int32_t span = upper - lower;

    if (span == 0) {
        return ANALOG_VALUE_MIN;
    }

    int64_t value = ((int64_t)(upper - (int32_t)sample) * ANALOG_VALUE_MAX) / span;
    if (value < ANALOG_VALUE_MIN) return ANALOG_VALUE_MIN;
    if (value > ANALOG_VALUE_MAX) return ANALOG_VALUE_MAX;
    return (AnalogValue)value;
}
```

对于非线性的磁场响应，可使用 `tools/lut_generator/lut_generator.py` 生成查找表和 `advanced_key_normalize()` 覆写实现。应按按键的几何参数设置脚本中的常量：

| 常量 | 含义 |
| --- | --- |
| `R` | 磁铁半径，单位 mm。 |
| `L` | 磁铁沿充磁轴的高度，单位 mm。 |
| `Z_END` | 行程终点时传感器到磁铁的轴向距离，单位 mm。它必须与 `Z_START` 使用相同的物理参考点。 |
| `Z_START` | 行程起点时传感器到磁铁的轴向距离，单位 mm。它必须大于 `Z_END`，两者之差就是模型中的行程。 |
| `LUT_LENGTH` | 查找表项数，必须与固件配置中的 `LUT_LENGTH` 一致。 |
| `ANALOG_VALUE_MIN` / `ANALOG_VALUE_MAX` | 生成的归一化值在行程起点和终点分别使用的输出值。 |

然后生成 C 源码（脚本依赖 NumPy）：

```bash
python3 -m pip install numpy
python3 lut_generator.py > analog_lut.c
```

将生成的查找表和函数与固件一起编译，并确保该源码包含所需的 libamp 类型声明。

### 2.6 集成 USB 传输

最小移植可选择以下两种 USB 集成路径。随库提供的后端基于 CherryUSB；它是可选但建议优先使用的 USB 协议栈。

**1. 使用随库提供的后端。** 按照[设备移植指南](https://cherryusb.readthedocs.io/en/latest/quick_start/transplant.html)加入核心和控制器驱动，实现底层时钟/引脚/中断初始化，连接 USB 中断，并在必要时配置对缓存安全的 USB 内存。

设备移植成功后，将 `libamp/usb/template/cherryusb/` 下的全部源码加入固件目标。该模板根据 `keyboard_config.h` 构建描述符，并提供 libamp 所需的 HID 传输回调；无需自行实现 `hid_send_keyboard()`。

使用 CMake 时，可按下例加入模板源码和头文件目录：

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

初始化平台 USB 控制器后，使用协议栈的总线 ID 和 USB 控制器寄存器基地址调用模板提供的 `usb_init()`：

```c
#include "usbd_user.h"

usb_init(0, PLATFORM_USB_CONTROLLER_BASE);
```

**2. 适配其他 USB 协议栈（TODO）。** 此路径需要实现描述符注册、端点回调和报告传输的后端；目前尚未提供移植说明。

### 2.7 初始化 libamp 并运行任务

在启用键盘报告前，先初始化时钟、输入硬件和 USB 控制器。输入以及可选的存储/LED 依赖项就绪后调用一次 `keyboard_init()`。每个键盘时钟周期都应将 `g_keyboard_tick` 递增一次，并调用一次 `keyboard_task()`。它可以来自定时器、RTOS 调度器或基于截止时间的裸机循环；频率必须严格等于 `POLLING_RATE`。

在非时间关键的前台上下文中持续调用 `keyboard_process()`。它处理队列事件、协议工作和可选的运行时处理。

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
        /* 建议由周期调度器调用 keyboard_task()，不要在此调用。 */
    }
}

void platform_keyboard_tick(void)
{
    g_keyboard_tick++;
    keyboard_task();
}
```

不要在多个上下文中并发调用 `keyboard_task()`。如果 ADC 或 USB 回调与它共享状态，应使用目标平台适用的同步方式。libamp 会在 `keyboard_init()` 中将 `g_keyboard_tick` 清零，但之后的每次递增都由平台负责。

为在开机时校准按键，建议在 `keyboard_init()` 后等待一小段时间，使模拟量采样稳定，再调用一次 `analog_calibrate()`。延时应足以让输入采集链路提供有代表性的采样值。

### 2.8 验证第一个按键

1. 编译并烧录固件。
2. 确认主机枚举出一个 USB HID 键盘接口。
3. 在按键移动时记录或观察原始采样值；行程两端都应达到稳定值。
4. 等待校准完成后按下和释放按键。主机应收到配置的 `A` 键按下和释放。
5. 若按键方向相反，调整传感器接线/ADC 方向或归一化函数；不要手动颠倒按下和释放事件。

## 3. 完善输入层

### 3.1 多 ADC 通道和多路复用器

将 `ADVANCED_KEY_NUM` 设为模拟按键数量，并根据独立缓冲的采样源数量设置 `ANALOG_BUFFER_LENGTH`。在 `g_analog_map` 中将每个按键映射到对应缓冲区。多路复用输入通常按以下流程扫描：选择通道、等待采样保持、完成转换、将结果推入该通道的缓冲区。应将映射和扫描顺序集中维护，避免硬件连线变化后悄悄改变按键 ID。

### 3.2 校准、死区和滤波

默认高级按键配置控制普通、快速触发和速度模式。`keyboard_init()` 会恢复默认值，并由库执行校准流程。收集真实传感器数据后，再调整 `DEFAULT_ESTIMATED_RANGE`、死区、触发距离和释放距离。

`FILTER_TYPE`、`FILTER_DOMAIN` 和 `FILTER_LOWPASS_ALPHA` 选择内置滤波行为。只有在为原始域或归一化域选择了合适的迟滞量后，才启用 `FILTER_HYSTERESIS_ENABLE`。滤波应消除采样噪声，而不能在选定轮询率下掩盖真实的行程变化。

### 3.3 普通按键

增加 `KEY_NUM` 并扩展 `g_default_keymap` 的每一层，即可添加普通按键。它们的 ID 从 `ADVANCED_KEY_NUM` 开始。覆写 `keyboard_scan()`，将当前状态传给 `keyboard_key_update()`：

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

矩阵扫描、扩展器读取和去抖策略仍由平台负责。`DEBOUNCE_*` 的单位是键盘时钟周期。

`keyboard_task()` 会在每个键盘时钟周期调用一次 `keyboard_scan()`。`keyboard_key_update()` 接收一个 `Key` 和对应的物理按下状态，按配置执行去抖并更新该按键的报告状态；返回值表示报告状态是否发生变化。

### 3.4 旋转编码器

启用 `ENCODER_ENABLE`，定义 `ENCODER_NUM`，并按 `encoder.h` 定义 `g_encoders`。由平台驱动通过 `encoder_input()` 提供每个编码器的累计位置。每个编码器的顺时针和逆时针动作都应映射为普通按键。

```c
void encoder_input_delta(uint16_t id, int16_t delta);
void encoder_input(uint16_t id, int32_t count);
void encoder_process(void);
```

每个编码器使用两个普通按键 ID：`cw_id` 对应顺时针，`ccw_id` 对应逆时针。应在 `KEY_NUM` 中为每个编码器预留两个普通按键：

```c
#include "encoder.h"

#define ENCODER_CW_KEY_ID   (ADVANCED_KEY_NUM + 0)
#define ENCODER_CCW_KEY_ID  (ADVANCED_KEY_NUM + 1)

Encoder g_encoders[ENCODER_NUM] = {
    { .cw_id = ENCODER_CW_KEY_ID, .ccw_id = ENCODER_CCW_KEY_ID },
};
```

例如，平台读取到编码器 `0` 的当前计数后，可调用：

```c
void platform_encoder_updated(int32_t count)
{
    encoder_input(0, count);
}
```

`encoder_input()` 接收绝对计数，并计算相对于上次更新的移动量。若驱动直接给出相对移动量，可使用 `encoder_input_delta(id, delta)`。启用 `ENCODER_ENABLE` 后，`keyboard_task()` 会自动调用 `encoder_process()`，将正、负移动量转换为配置的顺时针或逆时针按键状态。

### 3.5 自定义事件、复位和 Bootloader 跳转

`keyboard_reboot()`、`keyboard_jump_to_bootloader()`、`keyboard_delay()` 和 `keyboard_user_event_handler()` 都是弱钩子。只需覆写键位表中实际暴露的操作。自定义键码应使用 `KEY_USER`，并在 `keyboard_user_event_handler()` 中处理；复位和 Bootloader 策略应保留在平台代码中，而不是修改 libamp。

```c
void keyboard_reboot(void);
void keyboard_jump_to_bootloader(void);
void keyboard_delay(uint32_t ms);
void keyboard_user_event_handler(KeyboardEvent event);
```

内置的重启和 Bootloader 操作分别调用 `keyboard_reboot()` 和 `keyboard_jump_to_bootloader()`。当 libamp 需要毫秒级延时，应由平台实现 `keyboard_delay(ms)`。`keyboard_user_event_handler(KeyboardEvent event)` 会收到每个 `KEY_USER` 事件，其中包含键码和按下/释放状态，可在此分发应用自定义动作。

## 4. 添加持久化存储

同时启用 `STORAGE_ENABLE` 和 `LFS_ENABLE`。存储使用 `driver.h` 中声明的回调：

```c
int flash_read(uint32_t address, uint32_t size, uint8_t *data);
int flash_write(uint32_t address, uint32_t size, const uint8_t *data);
int flash_erase(uint32_t address, uint32_t size);
```

应为这些回调实现专门保留的可擦写区域。`LFS_READ_SIZE`、`LFS_PROG_SIZE`、`LFS_BLOCK_SIZE`、`LFS_BLOCK_COUNT`、`LFS_CACHE_SIZE`、`LFS_LOOKAHEAD_SIZE` 和 `LFS_BLOCK_CYCLES` 必须与闪存器件和保留区域相匹配。回调实现尤其要遵守编程和擦除对齐要求。

启动时，`keyboard_init()` 会挂载存储、检查保存的版本并恢复选中的配置文件。因此必须在调用 `keyboard_init()` 之前使存储可用。不要将文件系统放在固件、Bootloader 或其他应用数据上。

## 5. 添加灯光

启用 `RGB_ENABLE`，设置 `RGB_NUM`，并实现 LED 驱动回调：

```c
int led_set(uint16_t index, uint8_t red, uint8_t green, uint8_t blue);
int led_flush(void);
```

`led_set()` 更新一个逻辑 LED，`led_flush()` 将待发送的帧显示出来。串行 LED 驱动通常会累积所有颜色，直到调用 `led_flush()` 再统一输出。

要显示键盘启动闪光动画，可在 `keyboard_init()` 后调用 `rgb_init_flash()` 或 `rgb_flash()`。必须先启动周期 tick：这两个函数会等待 `g_keyboard_tick` 推进，若它尚未以 `POLLING_RATE` 开始递增，函数将无法返回。

RGB 的三张表使用以下相互关联的索引空间：

| 表 | 索引 | 元素 | 作用 |
| --- | --- | --- | --- |
| `g_rgb_mapping` | LED ID（`0` 至 `RGB_NUM - 1`） | 按键 ID | 将逻辑 LED 与按键关联。 |
| `g_rgb_inverse_mapping` | 按键 ID（`0` 至 `TOTAL_KEY_NUM - 1`） | LED ID | 查找与按键关联的 LED。 |
| `g_rgb_locations` | LED ID（`0` 至 `RGB_NUM - 1`） | `RGBLocation { x, y }` | 给出同一 LED 的物理位置。 |

因此，`g_rgb_mapping[led_id]` 与 `g_rgb_locations[led_id]` 必须描述同一个 LED。未启用 `RGB_CUSTOM_INVERSE_MAPPING` 时，libamp 会在 `rgb_init()` 中根据 `g_rgb_mapping` 生成 `g_rgb_inverse_mapping`；自动生成要求每个已映射按键至多对应一个 LED。定义位置时，可使用 `UNIT_TO_UM()` 表示以键位单位描述的布局。只选择产品实际需要的 RGB 模式和基础效果；每个 `RGB_MODE_USE_*` 宏都会包含相应的行为。

## 6. 扩展 USB 设备

随库提供的后端会根据 `keyboard_config.h` 启用接口。设置对应的宏后重新编译，并验证枚举出的描述符：`NKRO_ENABLE`、`EXTRAKEY_ENABLE`、`MOUSE_ENABLE`、`RAW_ENABLE`、`MIDI_ENABLE`、`JOYSTICK_ENABLE`、`DIGITIZER_ENABLE` 和 `GAMEPAD_ENABLE` 分别加入对应的键盘功能或 USB 接口。使用随库提供的 CherryUSB 模板时，启用这些宏无需额外的 USB 端配置。

`SHARED_EP_ENABLE` 通过一个中断 IN 端点传输多个 HID 报告 ID，从而减少端点占用。应先使用独立报告，只有在控制器资源不足时再引入共享端点。将 `MAX_ENDPOINTS` 设为控制器可用端点数量。若控制器支持 IN 和 OUT 端点共用同一端点号，应启用 `USB_ENDPOINTS_ARE_REORDERABLE`；描述符生成器会让每组 IN/OUT 端点共用端点号，从而节省端点数量。

随库提供的后端已经包含这些功能的端点回调和发送函数。对于 Raw HID，它会将每个完整 OUT 报告转交给 libamp 传输层。游戏手柄输出可通过覆写 `gamepad_out_callback()` 使用。其他 USB 后端仍是 TODO。

定义 `SERIAL_NUMBER` 可使用固定序列号。定义 `SERIAL_NUMBER_USE_CUSTOM` 并覆写 `usb_descriptor_get_serial_number(char *buffer, size_t buffer_size)`，可使用由设备生成的序列号。返回值是写入的 ASCII 字符数，不包括末尾的空字符。

## 7. 启用高级运行时功能

`DYNAMICKEY_ENABLE` 提供可配置的高级按键行为，例如 Mod-Tap、切换键、动态击键和 Mutex 键。`MACRO_ENABLE` 启用宏录制/播放。两者都使用与普通物理按键相同的事件路径，因此应在基础输入和报告路径稳定后再验证。

`SCRIPT_ENABLE` 同时依赖 `STORAGE_ENABLE` 和 `LFS_ENABLE`。选择 `SCRIPT_RUNTIME_STRATEGY` 后，根据可用 RAM 设置 `SCRIPT_MEMORY_SIZE` 以及对应的源码或字节码缓冲区大小。即使禁用了脚本，libamp 的构建仍包含主机 mquickjs 头文件生成步骤。

`MTP_ENABLE` 通过 USB 暴露文件访问。它需要 MTP 后端源码、对应 USB 端点，以及一个能在键盘运行时安全暴露给主机的文件系统。在发布固件前，应测试文件传输、拔插和断电行为。

## 8. 构建、测试和排错

### 8.1 运行 libamp 主机测试

主机测试在没有目标硬件的情况下验证 libamp 核心：

```bash
cmake -S third_party/libamp -B build/libamp-tests -DLIBAMP_BUILD_TESTS=ON
cmake --build build/libamp-tests --parallel
ctest --test-dir build/libamp-tests --output-on-failure
```

如果 libamp 本身就是仓库根目录而不是子目录，将 `third_party/libamp` 替换为 `.`。

### 8.2 固件构建检查表

构建目标固件前，确认：

- `LIBAMP_INCLUDE_DIR` 包含正确的 `keyboard_config.h`；
- 平台适配层、选用的 USB 协议栈和 libamp 后端源码已经加入固件目标；
- 已链接 libamp 和数学库；
- 目标编译选项同时应用到应用和 libamp；
- USB DMA 缓冲区、缓存维护和中断连接符合选用控制器端口的要求；
- 每个启用的功能宏都已使用选用后端验证。

### 8.3 常见集成问题

| 现象 | 检查项 |
| --- | --- |
| 找不到 `keyboard_config.h` | 在 `add_subdirectory(libamp)` 前设置 `LIBAMP_INCLUDE_DIR`。 |
| mquickjs 头文件生成失败 | 安装主机 `gcc`，不能只安装交叉编译器。 |
| 设备已枚举但没有按键输出 | 确认 `g_keyboard_tick` 和 `keyboard_task()` 都按 `POLLING_RATE` 运行、`keyboard_process()` 在前台运行、后端源码已加入，并且在 USB 初始化后调用了 `usb_init()`。 |
| 模拟按键状态不变化 | 检查原始采样、`g_analog_map`、缓冲区索引、校准范围和归一化方向。 |
| 第一次按键后报告停止 | 检查端点回调、DMA 缓冲区位置和缓存维护。 |
| 存储损坏固件或无法挂载 | 重新检查保留闪存范围、擦写对齐和 littlefs 几何参数。 |
| 启用功能后 USB 失效 | 同时重建描述符和后端注册，并检查端点数量、方向、报告 ID 以及 DMA 安全缓冲区。 |
