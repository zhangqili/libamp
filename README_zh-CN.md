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

`POLLING_RATE` 设置 `g_keyboard_tick` 的递增频率和 `keyboard_task()` 的调用频率；USB 端点间隔由 `USB_POLLING_INTERVAL_MS` 单独配置。报告在有待发送数据时发送，启用 `continuous_poll` 后每个 tick 都会尝试发送。USB 全速键盘使用 `1000`（1 kHz）；USB 高速键盘使用 `8000`（8 kHz），并定义 `CONFIG_USB_HS`。`USB_POLLING_INTERVAL_MS` 保持为 `1`：在全速下表示 1 ms，在高速下表示一个 125 us 微帧。

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

    int64_t travel = ((int64_t)(upper - (int32_t)sample) * ANALOG_VALUE_RANGE) / span;
    if (travel < 0) return ANALOG_VALUE_MIN;
    if (travel > ANALOG_VALUE_RANGE) return ANALOG_VALUE_MAX;
    return (AnalogValue)(travel + ANALOG_VALUE_MIN);
}
```

默认实现通过 `advanced_key_set_range()` 预先计算 Q16 比例，得到限制范围后的 LUT 索引，再缩放到 `ANALOG_VALUE_RANGE` 并加上 `ANALOG_VALUE_MIN`。定点计算结果可能与上面的直接公式略有差异；最后的缩放仍包含对编译期常量 `LUT_LENGTH` 的除法，是否转换为乘法/移位取决于配置和编译器。

对于非线性的磁场响应，可使用 `tools/lut_generator/lut_generator.py` 生成查找表和 `advanced_key_normalize()` 覆写实现。该实现使用 `advanced_key_lut_index()` 将校准范围映射到 `0` 至 `LUT_LENGTH`（含端点），读取表前会将末端索引限制为 `LUT_LENGTH - 1`。表内保存绝对归一化值，已包含 `ANALOG_VALUE_MIN`，返回时不会再次叠加下限。应按按键的几何参数设置脚本中的常量：

| 常量 | 含义 |
| --- | --- |
| `R` | 磁铁半径，单位 mm。 |
| `L` | 磁铁沿充磁轴的高度，单位 mm。 |
| `Z_END` | 行程终点时传感器到磁铁的轴向距离，单位 mm。它必须与 `Z_START` 使用相同的物理参考点。 |
| `Z_START` | 行程起点时传感器到磁铁的轴向距离，单位 mm。它必须大于 `Z_END`，两者之差就是模型中的行程。 |
| `LUT_LENGTH` | 查找表项数，必须与固件配置中的 `LUT_LENGTH` 一致。 |
| `ANALOG_VALUE_MIN` / `ANALOG_VALUE_MAX` | 行程起点和终点的归一化输出值，必须与固件一致，并满足 `0 <= MIN < MAX <= 65535`。 |

然后生成 C 源码（脚本依赖 NumPy）：

```bash
python3 -m pip install numpy
python3 tools/lut_generator/lut_generator.py > analog_lut.c
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

在非时间关键的前台上下文中持续调用 `keyboard_process()`。它处理队列中的按键事件、延迟校准，以及已启用的 RGB 渲染、Nexus 配置同步和统计保存等后台工作。Raw HID 请求由收包回调同步处理，不会自动交给此前台循环。

RGB 渲染器每个 tick 至多生成一帧。可设置 `RGB_MAX_FRAME_RATE` 来拉大帧间隔；它默认等于 `POLLING_RATE`，实际间隔为 `max(POLLING_RATE / RGB_MAX_FRAME_RATE, 1)` 个 tick（整数除法）。若需要准确的目标帧率，应选择能整除 `POLLING_RATE` 的正值。动画按 tick 计时，降低帧率会减少采样帧数，不会改变时间尺度。

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

#### 执行上下文

下表列出内置任务和传输路径的执行位置。平台回调和应用直接调用的函数保留调用方的上下文。

| 上下文 | 入口 | 说明 |
| --- | --- | --- |
| 输入/传输回调 | `ringbuf_push()`、`encoder_input()`、`encoder_input_delta()`、`midi_input_callback()`、`nexus_process_buffer()`、`packet_process_buffer()` | 是否处于中断由平台决定。数据包会同步处理，可能访问闪存或调用用户代码；Nexus 从机的 `nexus_process_buffer()` 也会同步处理命令包。 |
| 键盘 tick（`POLLING_RATE`） | `keyboard_task()`：扫描、去抖、事件处理、HID 报告、`packet_buffer_flush()`、`nexus_process()` / `nexus_send_report()` | 不可重入。此路径中的传输发送函数不得阻塞。未定义 `SCRIPT_POLLING` 时，脚本也在此执行。 |
| 前台循环 | `keyboard_process()`：按键事件轮询、`rgb_process()`、`nexus_poll()`、控制台输出、统计保存和延迟校准 | 闪存写入和校准可能耗时数毫秒；定义 `SCRIPT_POLLING` 后，周期性脚本处理也移到此处。 |

应在允许相应延迟的上下文调用 `analog_calibrate()`、`rgb_init_flash()`、`rgb_flash()` 以及应用中的存储/文件系统操作。同步的 `nexus_request_timeout()` / `nexus_send_timeout()` 等待期间，tick 必须持续推进。

绑定到按键的保存、配置切换、恢复出厂等操作默认在事件处理器中访问存储。定义 `KEYBOARD_OPERATION_POLLING` 可将这些操作移至 `keyboard_process()`。此开关不会移动 Raw HID 命令或直接的 `storage_*()` / `fs_*()` 调用：随库提供的 CherryUSB Raw HID 回调直接调用 `packet_process_buffer()`，包括其中的统计写入、配置和文件系统操作。若闪存访问必须避开中断，平台还需延后处理这些完整请求，并与前台文件系统访问串行执行。

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

默认高级按键配置控制普通、快速触发和速度模式。启用存储时，`keyboard_init()` 恢复已保存的配置，否则恢复默认值；它不会调用 `analog_calibrate()`。应在按键松开、采样稳定时调用该函数，以设置静止边界并初始化已启用的滤波器。它会将校准方向重置为 `ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED`，后续采样用于判断方向并扩展按下边界。收集真实传感器数据后，再调整 `DEFAULT_ESTIMATED_RANGE`、死区、触发距离和释放距离。

`FILTER_ENABLE` 启用主滤波器，`FILTER_TYPE` 和 `FILTER_DOMAIN` 选择算法和输入域。低通滤波器的 `FILTER_LOWPASS_ALPHA` 是上一状态的权重（默认 `0.5f`），值越大平滑越强。只有在为原始域或归一化域选择了合适的迟滞量后，才启用 `FILTER_HYSTERESIS_ENABLE`。滤波应消除采样噪声，而不能在选定轮询率下掩盖真实的行程变化。

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

平台实现矩阵扫描和扩展器读取；libamp 在 `keyboard_key_update()` 中按平台配置的 `DEBOUNCE_*` 参数执行去抖，其单位是键盘时钟周期。

`keyboard_task()` 会在每个键盘时钟周期调用一次 `keyboard_scan()`。`keyboard_key_update()` 接收一个 `Key` 和对应的物理按下状态，按配置执行去抖并更新该按键的报告状态；返回值表示报告状态是否发生变化。

`KEYBOARD_SKIP_UNCHANGED_EVENTS` 默认为 `1`：状态未变化时跳过事件分发，但鼠标移动、摇杆/手柄轴和 `KEY_USER` 仍在每次更新时分发。设为 `0` 后，所有键码的未变化状态也会分发。此设置适用于普通按键、高级按键更新路径及 Nexus 位图处理，不改变去抖策略，也不会单独强制每个 tick 发送 USB 报告。

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

`keyboard_reboot()`、`keyboard_jump_to_bootloader()` 和 `keyboard_user_event_handler()` 是弱钩子；`keyboard_delay()` 仅有声明，没有默认实现。只需覆写键位表中实际暴露的钩子。自定义键码应使用 `KEY_USER`，并在 `keyboard_user_event_handler()` 中处理；复位和 Bootloader 策略应保留在平台代码中，而不是修改 libamp。

```c
void keyboard_reboot(void);
void keyboard_jump_to_bootloader(void);
void keyboard_delay(uint32_t ms);
void keyboard_user_event_handler(KeyboardEvent event);
```

内置的重启和 Bootloader 操作分别调用 `keyboard_reboot()` 和 `keyboard_jump_to_bootloader()`。使用 Kalman 滤波校准时，平台需实现 `keyboard_delay(ms)`，供采样循环请求 1 ms 延时。`keyboard_user_event_handler(KeyboardEvent event)` 接收 `KEY_USER` 事件，默认分发策略下也包含未变化状态。覆写它时，应在按下事件调用 `keyboard_key_event_down_dispatch(event)`，在释放事件调用 `keyboard_key_event_up_dispatch(event)`，以保留队列事件、物理按键回调、统计以及已启用的脚本/宏钩子；按住状态可按应用需要另行处理。

## 4. 添加持久化存储

启用 `STORAGE_ENABLE` 并配置选定的文件系统（默认为 LittleFS）。存储使用 `driver.h` 中声明的回调：

```c
int flash_read(uint32_t address, uint32_t size, uint8_t *data);
int flash_write(uint32_t address, uint32_t size, const uint8_t *data);
int flash_erase(uint32_t address, uint32_t size);
```

应为这些回调实现专门保留的可擦写区域。`FS_READ_SIZE`、`FS_PROG_SIZE`、`FS_BLOCK_SIZE`、`FS_BLOCK_COUNT` 和 `FS_CACHE_SIZE` 必须与闪存器件和保留区域相匹配；`FS_LOOKAHEAD_SIZE` 和 `LFS_BLOCK_CYCLES` 用于 LittleFS。`FS_FLASH_OFFSET` 指定分区在闪存回调地址空间中的字节偏移，默认 `0`，并兼容旧名 `FILEX_FLASH_OFFSET`；只定义其中一个，或确保两者相同。回调实现尤其要遵守编程和擦除对齐要求。

启动时，`keyboard_init()` 会挂载存储、检查保存的版本并恢复选中的配置文件。因此必须在调用 `keyboard_init()` 之前使存储可用。不要将文件系统放在固件、Bootloader 或其他应用数据上。
文件系统挂载/打开失败时会自动尝试格式化分区。切换文件系统或 flash 层不会迁移已有数据，并可能触发此恢复流程；更改这些设置前应备份需要保留的文件。

启用 `RECORD_PERSIST_ENABLE` 后，统计信息按 `RECORD_STATISTICS_SAVE_INTERVAL_MS`（默认 30 分钟）保存到 `system/stat`。按键次数变化时，将保存计时器推进到距到期 `RECORD_STATISTICS_IDLE_SAVE_MS`（默认 120 秒）的位置；后续次数变化会重新安排这 120 秒。保存后恢复正常周期。松开或保持按住按键不影响计时。上位机写入或清空统计仍立即保存。

### 4.1 FileX FAT 存储与 flash 层

`FILE_SYSTEM_TYPE` 默认为 `FILE_SYSTEM_LFS`；在 `keyboard_config.h` 中定义为 `FILE_SYSTEM_FILEX`，即可使用 FileX FAT。

定义 `FLASH_LAYER` 可选择文件系统访问闪存的方式：`FLASH_LAYER_DIRECT` 直接调用 `driver.h` 中声明的 `flash_*` 回调，不做磨损均衡；`FLASH_LAYER_LEVELX` 增加 LevelX NOR 磨损均衡，但可用容量变小、RAM 占用增加、挂载变慢。

### 4.2 统计与上位机协议

启用 `COUNTER_ENABLE` 可记录每键次数；同时启用 `STORAGE_ENABLE` 和 `RECORD_PERSIST_ENABLE`，可将次数和累计运行时间保存到 `system/stat`。`keyboard_init()` 会读取保存的统计并记录启动时的计数快照。正常保存间隔由 `RECORD_STATISTICS_SAVE_INTERVAL_MS` 指定，默认 30 分钟。物理按键的按下回调增加计数后，会将保存时间安排到 `RECORD_STATISTICS_IDLE_SAVE_MS`（默认 120 秒）后；后续增加计数会重新开始这段延时。释放和未变化的按住状态不影响计时。保存后恢复正常周期；连续输入可能一直推迟这次自动保存。空闲间隔应在零到正常保存间隔之间。启用持久化时，上位机写入或清空统计仍会立即保存。

启用 Raw HID 后，通过 `PACKET_CODE_GET`（`0x02`）或 `PACKET_CODE_SET`（`0x01`）访问 `PACKET_DATA_RECORD`（`0x0E`）。线格式由 `src/packet.h` 中的紧凑结构体定义：

| 子命令 | 值 | GET | SET |
| --- | --- | --- | --- |
| `PACKET_DATA_RECORD_RUNTIME` | `0x00` | 返回本次启动时长 `uptime` 和累计时长 `runtime`，单位均为毫秒。未启用持久化时，两者均返回当前 tick 转换的毫秒值。 | 启用持久化时设置累计 `runtime`，不改变 `uptime`。 |
| `PACKET_DATA_RECORD_KEYCOUNT` | `0x01` | 返回请求按键的当前累计次数。 | 替换请求按键的累计次数。 |
| `PACKET_DATA_RECORD_KEYCOUNT_SINCE_BOOT` | `0x02` | 启用持久化时返回当前次数减去启动快照，否则返回当前次数。 | 启用持久化时，将累计次数设为启动快照加请求值，否则直接替换当前次数。 |

`PacketRecordRuntime` 共 **20 字节**：3 字节头（`code`、`id`、`type`）、1 字节 `sub_cmd`、偏移 4 处的 `uint64_t uptime` 和偏移 12 处的 `uint64_t runtime`。上位机 GET 解析和 SET 编码需与固件同步升级，当前不支持旧的 12 字节格式。`uptime` 由 32 位 `g_keyboard_tick` 换算，64 位字段本身不会扩展 tick 的回绕范围；当前累计时长计算也使用这一换算后的 tick。

次数包在相同的头和子命令之后包含 `uint16_t length`，再跟随 `length` 个紧凑条目，每项为 `uint16_t key_index` 和 `uint32_t count`。索引必须小于 `TOTAL_KEY_NUM`，全部条目须容纳在报告内；64 字节 Raw HID 报告最多容纳九项。未启用 `COUNTER_ENABLE` 时，次数读回零，SET 不改变计数。启动快照在重新初始化前保持不变：修改累计次数不会重置快照，因此将累计值设为小于快照时，本次启动次数会按无符号 32 位减法回绕。

## 5. 添加灯光

启用 `RGB_ENABLE`，设置 `RGB_NUM`，并实现 LED 驱动回调：

```c
int led_set(uint16_t index, uint8_t red, uint8_t green, uint8_t blue);
int led_flush(void);
```

`led_set()` 更新一个逻辑 LED，`led_flush()` 将待发送的帧显示出来。串行 LED 驱动通常会累积所有颜色，直到调用 `led_flush()` 再统一输出。

使用 `rgb_set_brightness(uint8_t brightness)` 设置全局亮度（`0` 至 `255`）。启用 Gamma 查表时，直接写入 `g_rgb_base_config.brightness` 不会更新输出表。`rgb_init()`、配置读取、上位机亮度命令、亮度按键和恢复出厂路径已使用此接口。

`RGB_GAMMA_ENABLE` 启用 Gamma 校正，指数 `RGB_GAMMA` 默认为 `2.2f`。再定义 `RGB_GAMMA_LUT_ENABLE`，可使用 256 字节 RAM 表缓存 Gamma 和亮度换算，由亮度设置接口重建。只启用 Gamma 时逐输出通道计算；只启用 LUT 宏不会生效。

应用直接修改基础颜色时，应通过 `rgb_to_hsv()` / `hsv_to_rgb()` 同步 RGB 和 HSV 字段，副色也一样。彩虹基础效果直接读取 `g_rgb_base_config.hsv`；上位机颜色包处理器已同步更新两种表示。

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

`SCRIPT_ENABLE` 依赖 `STORAGE_ENABLE` 和已配置的文件系统。选择 `SCRIPT_RUNTIME_STRATEGY` 后，根据可用 RAM 设置 `SCRIPT_MEMORY_SIZE` 以及对应的源码或字节码缓冲区大小。即使禁用了脚本，libamp 的构建仍包含主机 mquickjs 头文件生成步骤。

`MTP_ENABLE` 通过 USB 暴露文件访问。它需要 MTP 后端源码、对应 USB 端点，以及一个能在键盘运行时安全暴露给主机的文件系统。在发布固件前，应测试文件传输、拔插和断电行为。

### 7.1 Nexus：连接多块板子

`NEXUS_ENABLE` 将多块板子组成一把键盘。主机持有 USB 和键位图；每个从机（`NEXUS_IS_SLAVE 1`）扫描自己的按键并流式上报给主机。链路由应用提供：实现 `driver.h` 中的 `nexus_send()`（主机到从机）和 `nexus_report()`（从机到主机），并在任一端收到完整帧时调用 `nexus_process_buffer(slave_id, frame, len)`。两个发送函数必须在返回前复制或发送完帧，成功时返回 `0`。帧格式见 `nexus.h`。

主机侧需定义 `g_nexus_slave_configs[NEXUS_SLAVE_NUM]`：每项把从机按键 `j` 映射到主机按键 `map[j]`，共 `length` 个（不超过 `NEXUS_SLICE_LENGTH_MAX`）。`keyboard_init()` 解析映射；`keyboard_task()` 应用从机上报，`keyboard_process()` 以非阻塞方式下发各从机的高级按键配置——启动时、配置变化时以及从机重新连接时都会下发。从机静默超过 `NEXUS_LINK_TIMEOUT_MS` 即视为拔出，其按键全部释放，可用 `nexus_slave_is_online()` 查询。位图模式下，默认的 `KEYBOARD_SKIP_UNCHANGED_EVENTS=1` 使主机只更新状态变化、仍在去抖或需要持续处理（鼠标移动、模拟轴、`KEY_USER`）的按键。`NEXUS_USE_RAW 1` 时从机改为发送原始采样，由主机归一化；没有任何从机映射的主机按键留给应用的 `keyboard_scan()` 处理。

`nexus_request_timeout()` 为应用提供同步请求/响应。它在 `g_keyboard_tick` 上自旋等待，因此只能在前台循环或优先级低于 tick 的中断中调用。

## 8. 构建、测试和排错

### 8.1 运行 libamp 主机测试

主机测试在没有目标硬件的情况下验证 libamp 核心：

```bash
cmake -S third_party/libamp -B build/libamp-tests -DLIBAMP_BUILD_TESTS=ON
cmake --build build/libamp-tests --parallel
ctest --test-dir build/libamp-tests --output-on-failure
```

如果 libamp 本身就是仓库根目录而不是子目录，将 `third_party/libamp` 替换为 `.`。

主机热点路径基准应使用 **Release** 构建，测量结果才有参考意义：

```bash
cmake -S . -B build/perf -DLIBAMP_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/perf --parallel
build/perf/test/libamp_bench 2000000 200000
```

它覆盖静止/移动按键、校准范围更新、HSV 转换、Nexus 从机按键和运行中的 RGB 效果。键盘负载使用普通键码，避免多键同时按下时切换配置或复位设备。这些是包含测试环境开销的主机时间，不是 MCU 周期或 USB 延迟。比较两个版本时，使用相同的编译器、选项、配置和基准源码，交替运行并比较中位数：

```bash
python3 test/bench/compare.py path/to/baseline/libamp_bench path/to/candidate/libamp_bench --json build/perf/comparison.json
```

处于运行间波动范围内的小差异不能说明性能变化。在宣称端到端延迟改善前，应在相同 ADC、USB 和 RGB 负载下，使用目标板定时器或周期计数器测量键盘 tick。

[性能报告](docs/performance/aggressive.md) 记录了当时相对 `codex-fable` 的变更、原始测量、行为检查和代码大小取舍，属于历史测量，不代表后续修改后的性能。比较两个源码目录的原始按键流水线可运行：

```bash
python3 test/advanced_key/check_pipeline.py path/to/reference-checkout .
python3 test/advanced_key/check_pipeline.py path/to/reference-checkout . --cflags='-flto -ffast-math'
```

`levelx` 组合的主机测试需使用 32 位工具链及 C/C++ 运行库（LevelX 磁盘字长必须与
32 位固件一致）；`FILE_SYSTEM_TYPE` 中 2 表示 FileX、1 表示 LittleFS：

```bash
cmake -S . -B b-fx-lx -DLIBAMP_BUILD_TESTS=ON '-DCMAKE_C_FLAGS=-m32 -DFILE_SYSTEM_TYPE=2 -DFLASH_LAYER=FLASH_LAYER_LEVELX' '-DCMAKE_CXX_FLAGS=-m32 -DFILE_SYSTEM_TYPE=2 -DFLASH_LAYER=FLASH_LAYER_LEVELX' && cmake --build b-fx-lx --parallel && ctest --test-dir b-fx-lx --output-on-failure   # FileX + LevelX
cmake -S . -B b-fx -DLIBAMP_BUILD_TESTS=ON '-DCMAKE_C_FLAGS=-m32 -DFILE_SYSTEM_TYPE=2' '-DCMAKE_CXX_FLAGS=-m32 -DFILE_SYSTEM_TYPE=2' && cmake --build b-fx --parallel && ctest --test-dir b-fx --output-on-failure                                   # FileX + 直连 flash
cmake -S . -B b-lfs-lx -DLIBAMP_BUILD_TESTS=ON '-DCMAKE_C_FLAGS=-m32 -DFILE_SYSTEM_TYPE=1 -DFLASH_LAYER=FLASH_LAYER_LEVELX' '-DCMAKE_CXX_FLAGS=-m32 -DFILE_SYSTEM_TYPE=1 -DFLASH_LAYER=FLASH_LAYER_LEVELX' && cmake --build b-lfs-lx --parallel && ctest --test-dir b-lfs-lx --output-on-failure   # LittleFS + LevelX
```

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
| 存储损坏固件或无法挂载 | 重新检查保留闪存范围、擦写对齐以及所选文件系统/flash 层的几何参数。 |
| 启用功能后 USB 失效 | 同时重建描述符和后端注册，并检查端点数量、方向、报告 ID 以及 DMA 安全缓冲区。 |
