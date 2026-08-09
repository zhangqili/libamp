/*
 * 将此文件复制到应用配置目录并命名为 keyboard_config.h，然后在 CMake 中
 * 添加 libamp 前，将 LIBAMP_INCLUDE_DIR 设为该目录。已启用的配置构成最小
 * 模拟 USB 键盘；可按需取消注释并调整其余配置。
 */
#ifndef KEYBOARD_CONFIG_H_
#define KEYBOARD_CONFIG_H_

/************/
/* 键盘 */
/************/
#define LAYER_NUM               1       /* 键位表层数。 */
#define ADVANCED_KEY_NUM        1       /* 模拟/高级按键数量。 */
#define KEY_NUM                 0       /* 普通数字按键数量。 */
#define POLLING_RATE            1000    /* USB 报告率和键盘时钟频率。 */
#define CALIBRATION_DELAY       1000    /* 手动校准前的延时，单位 ms。 */
#define DEBUG_INTERVAL          0       /* 调试数据包间隔，单位 tick；0 为关闭。 */
// #define CONFIG_USB_HS                 /* 使用 USB 高速；同时将 POLLING_RATE 设为 8000。 */
// #define KEYBOARD_OPERATION_POLLING    /* 在事件轮询器中处理键盘操作。 */
// #define KEYBOARD_VERSION_MAJOR 1      /* 覆盖上报的主版本号。 */
// #define KEYBOARD_VERSION_MINOR 0      /* 覆盖上报的次版本号。 */
// #define KEYBOARD_VERSION_PATCH 0      /* 覆盖上报的修订版本号。 */
// #define KEYBOARD_VERSION_INFO "custom" /* 覆盖上报的版本文本。 */

/* 去抖时间以键盘时钟周期为单位。 */
#define DEBOUNCE_PRESS          0       /* 按下去抖时长。 */
#define DEBOUNCE_PRESS_EAGER    1       /* 按下去抖完成前即上报按下。 */
#define DEBOUNCE_RELEASE        0       /* 释放去抖时长。 */
#define DEBOUNCE_RELEASE_EAGER  1       /* 释放去抖完成前即上报释放。 */

/* 可选键盘行为和内存/性能调优。 */
// #define SUSPEND_ENABLE                /* 启用 USB 挂起和远程唤醒处理。 */
// #define CONSOLE_ENABLE                /* 启用 libamp 控制台传输。 */
// #define CONSOLE_BUFFER_LENGTH 256     /* 控制台缓冲区大小，单位字节。 */
// #define LOG_ENABLE                    /* 启用 libamp 日志输出。 */
// #define LOG_USE_COLOR                 /* 在日志中使用 ANSI 颜色。 */
// #define LOG_OUTPUT_FILE               /* 在日志中包含源码文件和行号。 */
// #define KEY_CALLBACK_ENABLE           /* 启用每个按键的按下/释放回调。 */
#define OPTIMIZE_KEY_BITMAP           /* 使用紧凑的按键位图更新路径。 */
#define OPTIMIZE_MOVING_AVERAGE_FOR_RINGBUF /* 为模拟缓冲区维护滑动求和。 */
// #define EVENT_BUFFER_LENGTH 32        /* 键盘事件队列容量。 */
// #define EVENT_CACHE_LENGTH 16         /* 事件缓存条目容量。 */
// #define EVENT_CACHE_BUFFER_LENGTH 4   /* 事件缓存队列容量。 */

/************/
/* 高级按键 */
/************/
#define ANALOG_VALUE_MAX                    65535   /* 归一化模拟量最大值。 */
#define ANALOG_VALUE_MIN                    0       /* 归一化模拟量最小值。 */
#define LUT_LENGTH                          4096    /* 高级按键查找表长度。 */
#define DEFAULT_ADVANCED_KEY_MODE            ADVANCED_KEY_ANALOG_NORMAL_MODE /* 默认按键模式。 */
#define DEFAULT_CALIBRATION_MODE             ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED /* 自动检测传感器方向。 */
#define DEFAULT_TRIGGER_DISTANCE             0.10f   /* 默认触发行程比例。 */
#define DEFAULT_RELEASE_DISTANCE             0.10f   /* 默认释放行程比例。 */
#define DEFAULT_UPPER_DEADZONE               0.01f   /* 上端死区。 */
#define DEFAULT_LOWER_DEADZONE               0.01f   /* 下端死区。 */
#define DEFAULT_ACTIVATION_VALUE             0.50f   /* 默认激活值。 */
#define DEFAULT_DEACTIVATION_VALUE           0.49f   /* 默认失活值。 */
#define DEFAULT_ESTIMATED_RANGE              500     /* 初始传感器原始量程估计。 */

/*
 * DEFAULT_ADVANCED_KEY_MODE 可选值：
 * ADVANCED_KEY_DIGITAL_MODE：将非零输入值视为按下。
 * ADVANCED_KEY_ANALOG_NORMAL_MODE：使用固定的激活/失活值。
 * ADVANCED_KEY_ANALOG_RAPID_MODE：按相对极值的行程重新触发。
 * ADVANCED_KEY_ANALOG_SPEED_MODE：按行程变化速度切换状态。
 *
 * DEFAULT_CALIBRATION_MODE 可选值：
 * ADVANCED_KEY_NO_CALIBRATION：保持配置的量程不变。
 * ADVANCED_KEY_AUTO_CALIBRATION_POSITIVE：跟踪原始值递增的量程。
 * ADVANCED_KEY_AUTO_CALIBRATION_NEGATIVE：跟踪原始值递减的量程。
 * ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED：先自动判断原始值方向。
 */

/************/
/* 模拟量输入 */
/************/
#define RING_BUF_LEN             8       /* 每个环形缓冲区保留的采样数。 */
#define ANALOG_BUFFER_LENGTH     1       /* libamp 模拟量环形缓冲区数量。 */
// #define CALIBRATION_LPF_ENABLE         /* 校准时对原始值进行低通滤波。 */

/********/
/* 滤波 */
/* FILTER_TYPE_LOW_PASS 平滑采样；FILTER_TYPE_KALMAN 估算采样值。 */
/* FILTER_DOMAIN_RAW 在归一化前滤波；FILTER_DOMAIN_NORMALIZED 在归一化后滤波。 */
/********/
// #define FILTER_ENABLE                  /* 启用所选的模拟量滤波器。 */
// #define FILTER_TYPE FILTER_TYPE_LOW_PASS /* FILTER_TYPE_LOW_PASS 或 FILTER_TYPE_KALMAN。 */
// #define FILTER_DOMAIN FILTER_DOMAIN_RAW /* FILTER_DOMAIN_RAW 或 FILTER_DOMAIN_NORMALIZED。 */
// #define FILTER_LOWPASS_ALPHA 0.5f      /* 低通滤波平滑系数。 */
// #define FILTER_HYSTERESIS_ENABLE       /* 追加迟滞滤波。 */
// #define FILTER_HYSTERESIS 3            /* 所选域中的迟滞量。 */

/************/
/* 输入扩展 */
/************/
// #define ENCODER_ENABLE                 /* 启用旋转编码器处理。 */
// #define ENCODER_NUM 1                  /* 旋转编码器数量。 */
// #define ENCODER_TAP_DElAY 8            /* 编码器按键状态保持 tick 数。 */

/**********/
/* 存储 */
/**********/
/* STORAGE_ENABLE 和 LFS_ENABLE 必须同时启用。 */
// #define STORAGE_ENABLE                  /* 启用配置文件和持久化设置。 */
// #define LFS_ENABLE                      /* 启用 littlefs 存储。 */
// #define LFS_READ_SIZE 16                /* 最小闪存读取粒度。 */
// #define LFS_PROG_SIZE 16                /* 最小闪存编程粒度。 */
// #define LFS_BLOCK_SIZE 4096             /* 闪存擦除块大小。 */
// #define LFS_BLOCK_COUNT 16              /* 保留擦除块数量。 */
// #define LFS_CACHE_SIZE 16               /* littlefs 缓存大小。 */
// #define LFS_LOOKAHEAD_SIZE 16           /* littlefs 分配预读大小。 */
// #define LFS_BLOCK_CYCLES 500            /* 磨损均衡迁移间隔。 */
// #define STORAGE_PROFILE_FILE_NUM 4      /* 持久化配置文件数量。 */

/********/
/* RGB */
/********/
// #define RGB_ENABLE                      /* 启用 RGB 状态和效果。 */
// #define RGB_NUM ADVANCED_KEY_NUM        /* 可寻址 LED 数量。 */
// #define RGB_MAX_DURATION 1000           /* 效果最长持续时间，单位 ms。 */
// #define FADING_DISTANCE 5.0f            /* Fading 效果行程距离。 */
// #define JELLY_DISTANCE 5.0f             /* Jelly 效果行程距离。 */
// #define BUBBLE_DISTANCE 2.5f            /* Bubble 效果行程距离。 */
// #define PORT_LOCATION {0, 0}            /* RGB 效果原点，单位为按键间距。 */
// #define RGB_FLASH_MAX_DURATION 1000     /* Flash 效果最长持续时间，单位 ms。 */
// #define RGB_FLASH_RIPPLE_SPEED 500      /* Flash Ripple 传播速度。 */
// #define RGB_DEFAULT_MODE RGB_MODE_LINEAR /* 初始 RGB 效果模式。 */
// #define RGB_DEFAULT_SPEED 20            /* 初始 RGB 效果速度。 */
// #define RGB_DEFAULT_COLOR_HSV {273, 78, 99} /* 初始 RGB HSV 颜色。 */
// #define RGB_LEFT 0.0f                   /* RGB 布局左边界。 */
// #define RGB_TOP -0.5f                   /* RGB 布局上边界。 */
// #define RGB_RIGHT 15.0f                 /* RGB 布局右边界。 */
// #define RGB_BOTTOM 4.5f                 /* RGB 布局下边界。 */
// #define RGB_DEPTH 0.0f                  /* RGB 布局深度边界。 */
// #define RGB_GAMMA_ENABLE                /* 应用伽马校正。 */
// #define RGB_GAMMA 2.2f                  /* 伽马校正指数。 */
// #define RGB_ARGUMENT_LIST_BUFFER_LENGTH 64 /* RGB 命令参数缓冲区大小。 */
// #define RGB_CUSTOM_INVERSE_MAPPING      /* 自行提供 g_rgb_inverse_mapping。 */
// #define RGB_BASE_MODE_USE_RAINBOW 1     /* 包含 Rainbow 基础效果。 */
// #define RGB_BASE_MODE_USE_WAVE 1        /* 包含 Wave 基础效果。 */
// #define RGB_MODE_USE_STATIC 1           /* 包含 Static 效果。 */
// #define RGB_MODE_USE_CYCLE 1            /* 包含 Cycle 效果。 */
// #define RGB_MODE_USE_LINEAR 1           /* 包含 Linear 效果。 */
// #define RGB_MODE_USE_TRIGGER 1          /* 包含 Trigger 效果。 */
// #define RGB_MODE_USE_STRING 1           /* 包含 String 效果。 */
// #define RGB_MODE_USE_FADING_STRING 1    /* 包含 Fading String 效果。 */
// #define RGB_MODE_USE_DIAMOND_RIPPLE 1   /* 包含 Diamond Ripple 效果。 */
// #define RGB_MODE_USE_FADING_DIAMOND_RIPPLE 1 /* 包含 Fading Diamond Ripple 效果。 */
// #define RGB_MODE_USE_JELLY 1            /* 包含 Jelly 效果。 */
// #define RGB_MODE_USE_BUBBLE 1           /* 包含 Bubble 效果。 */

/*
 * RGB_DEFAULT_MODE 可选值：
 * RGB_MODE_FIXED：直接设置单颗 LED 颜色。 RGB_MODE_STATIC：混合静态颜色。
 * RGB_MODE_CYCLE：随时间循环色相。       RGB_MODE_LINEAR：将模拟行程映射为颜色。
 * RGB_MODE_TRIGGER：按键状态变化时闪烁。 RGB_MODE_STRING：绘制水平线条。
 * RGB_MODE_FADING_STRING：随时间淡出的水平线条。
 * RGB_MODE_DIAMOND_RIPPLE：绘制曼哈顿距离波纹。
 * RGB_MODE_FADING_DIAMOND_RIPPLE：随时间淡出的该波纹。
 * RGB_MODE_JELLY：在相邻 LED 间传播模拟量响应。
 * RGB_MODE_BUBBLE：绘制圆形波纹。
 */

/****************/
/* 运行时功能 */
/****************/
// #define DYNAMICKEY_ENABLE               /* 启用动态按键行为。 */
// #define DYNAMIC_KEY_NUM 32              /* 动态按键定义数量。 */
// #define DYNAMIC_KEY_HYSTERESIS A_ANTI_NORM(0.005f) /* 动态按键迟滞量。 */
// #define MACRO_ENABLE                    /* 启用宏录制和回放。 */
// #define MACRO_NUM 4                     /* 宏槽数量。 */
// #define MACRO_MAX_ACTIONS 128           /* 每个宏的最大动作数。 */
/* SCRIPT_ENABLE 依赖 STORAGE_ENABLE 和 LFS_ENABLE。 */
/* SCRIPT_AOT 执行保存的字节码；SCRIPT_JIT 编译保存的源码。 */
// #define SCRIPT_ENABLE                   /* 启用 JavaScript 运行时。 */
// #define SCRIPT_POLLING                  /* 从 keyboard_process() 运行脚本。 */
// #define SCRIPT_RUNTIME_STRATEGY SCRIPT_AOT /* SCRIPT_AOT 或 SCRIPT_JIT。 */
// #define SCRIPT_SOURCE_BUFFER_SIZE (1 * 1024) /* 脚本源码缓冲区大小。 */
// #define SCRIPT_BYTECODE_BUFFER_SIZE (1 * 1024) /* 脚本字节码缓冲区大小。 */
// #define SCRIPT_MEMORY_SIZE (4 * 1024)   /* 脚本运行时内存预算。 */
// #define SCRIPT_MAX_TIMERS 16            /* 脚本定时器数量上限。 */

/************/
/* 诊断数据 */
/************/
// #define COUNTER_ENABLE                  /* 收集基础按键计数。 */
// #define KPS_ENABLE                      /* 收集每秒按键数。 */
// #define KPS_HISTORY_ENABLE              /* 保留每秒按键数历史。 */
// #define BIT_STREAM_ENABLE               /* 保留按键位图历史。 */
// #define ANALOG_HISTORY_ENABLE           /* 保留模拟量历史。 */
// #define KPS_HISTORY_LENGTH 65           /* KPS 历史采样数。 */
// #define BIT_STREAM_LENGTH 128           /* 位图历史采样数。 */
// #define ANALOG_HISTORY_LENGTH 129       /* 模拟量历史采样数。 */
// #define RECORD_MAX_KEY_NUM 8            /* 记录器保留的最大按键数。 */
// #define KPS_REFRESH_RATE 144            /* KPS 刷新率，单位 Hz。 */

/*********/
/* Nexus */
/*********/
// #define NEXUS_ENABLE                    /* 启用多设备 Nexus 支持。 */
// #define NEXUS_IS_SLAVE 0                /* 主机为 0，从机固件为 1。 */
// #define NEXUS_USE_RAW 0                 /* 同步原始模拟量。 */
// #define NEXUS_SLAVE_NUM 1               /* Nexus 从机数量。 */
// #define NEXUS_SLICE_LENGTH_MAX 16       /* 单个从机切片的最大按键数。 */
// #define NEXUS_VALUE_MAX 65535           /* 同步按键值的最大值。 */
// #define NEXUS_BUFFER_SIZE 8             /* 每个从机的报告缓冲区大小。 */
// #define NEXUS_RETRY_COUNT 100           /* Nexus 发送重试上限。 */

/************/
/* USB 标识 */
/************/
#define USB_POLLING_INTERVAL_MS  1       /* USB 中断间隔；全速 1 ms / 高速 125 us。 */
#define FIXED_CONTROL_ENDPOINT_SIZE 0x40 /* 控制端点包大小。 */
#define FIXED_NUM_CONFIGURATIONS 1       /* USB 配置数量。 */
#define VENDOR_ID                0x1234  /* USB 厂商 ID。 */
#define PRODUCT_ID               0x0001  /* USB 产品 ID。 */
#define DEVICE_VER               0x0001  /* USB 设备版本号。 */
#define MANUFACTURER             "Example Manufacturer" /* USB 厂商字符串。 */
#define PRODUCT                  "Example Analog Keyboard" /* USB 产品字符串。 */
#define SERIAL_NUMBER            "0001" /* 固定 USB 序列号字符串。 */
#define USB_MAX_POWER_CONSUMPTION 500    /* 声明的 USB 总线电流，单位 mA。 */
#define USB_ENDPOINTS_ARE_REORDERABLE   /* 允许 IN/OUT 端点复用端点号。 */
// #define SERIAL_NUMBER_USE_CUSTOM        /* 通过平台钩子生成序列号。 */
// #define SERIAL_NUMBER_LENGTH 32         /* 自定义序列号最大长度。 */
// #define MAX_ENDPOINTS 8                 /* 可用的非控制端点数量。 */

/***************/
/* USB 接口 */
/***************/
// #define SHARED_EP_ENABLE                /* 为可选报告添加共享 HID 端点。 */
// #define KEYBOARD_SHARED_EP              /* 将键盘报告放入共享端点。 */

// #define NKRO_ENABLE                     /* 添加 NKRO 键盘报告。 */

// #define EXTRAKEY_ENABLE                 /* 添加多媒体/系统按键报告。 */

// #define MOUSE_ENABLE                    /* 添加鼠标报告。 */
// #define MOUSE_SHARED_EP                 /* 将鼠标报告放入共享端点。 */

// #define RAW_ENABLE                      /* 添加 Raw HID 接口。 */
// #define RAW_USAGE_PAGE 0xFF60           /* Raw HID 用法页。 */
// #define RAW_USAGE_ID 0x61               /* Raw HID 用法 ID。 */

// #define MIDI_ENABLE                     /* 添加 USB MIDI 接口。 */
// #define MIDI_DEFAULT_AUDIO_HANDLER_ENABLE 1 /* 启用默认 MIDI 音频回调。 */
// #define MIDI_STREAM_EPSIZE 64           /* MIDI 端点包大小；高速时使用 512。 */

// #define JOYSTICK_ENABLE                 /* 添加摇杆报告。 */
// #define JOYSTICK_SHARED_EP              /* 将摇杆报告放入共享端点。 */
// #define JOYSTICK_BUTTON_COUNT 8         /* 摇杆按钮数量。 */
// #define JOYSTICK_AXIS_COUNT 2           /* 摇杆轴数量。 */
// #define JOYSTICK_AXIS_RESOLUTION 8      /* 每个摇杆轴的位数。 */
// #define JOYSTICK_HAS_HAT                /* 添加八方向摇杆帽。 */

// #define DIGITIZER_ENABLE                /* 添加 Digitizer 报告。 */
// #define DIGITIZER_SHARED_EP             /* 将 Digitizer 报告放入共享端点。 */

// #define PROGRAMMABLE_BUTTON_ENABLE      /* 添加可编程按钮报告。 */

// #define GAMEPAD_ENABLE                  /* 添加兼容 XInput 的游戏手柄接口。 */

// #define VIRTSER_ENABLE                  /* 添加虚拟串口（CDC）接口。 */

// #define LIGHTING_ENABLE                 /* 添加 HID LampArray 接口。 */
// #define LAMPARRAY_KIND LAMPARRAY_KIND_KEYBOARD /* LampArray 设备类别。 */
// #define LAMPARRAY_UPDATE_INTERVAL 10000 /* LampArray 最小更新间隔，单位 us。 */
/* LAMPARRAY_KIND 可为 UNDEFINED、KEYBOARD、MOUSE、GAME_CONTROLLER、PERIPHERAL、
 * SCENE、NOTIFICATION、CHASSIS、WEARABLE、FURNITURE、ART、HEADSET、MICROPHONE
 * 或 SPEAKER；使用对应的 LAMPARRAY_KIND_* 常量。 */

/***************/
/* USB 扩展 */
/***************/
/* MTP_ENABLE 依赖启用存储的 USB 后端。 */
// #define MTP_ENABLE                      /* 添加媒体传输协议支持。 */
// #define MTP_DESCRIPTION "Keyboard Flash" /* MTP 存储描述。 */
// #define MTP_MAX_HANDLES 32              /* MTP 对象句柄上限。 */
// #define MTP_MAX_PATH_LEN 255            /* MTP 路径最大长度。 */
// #define MTP_DATA_EPSIZE 64              /* MTP 数据端点包大小。 */
// #define WEBUSB_ENABLE                   /* 添加 WebUSB 能力。 */
// #define WEBUSB_VENDOR_CODE 0x22         /* WebUSB 控制请求代码。 */
// #define WEBUSB_URL "example.com"        /* WebUSB 落地页 URL。 */
// #define MSOS20_VENDOR_CODE 0x21         /* Microsoft OS 2.0 控制请求代码。 */

#endif /* KEYBOARD_CONFIG_H_ */
