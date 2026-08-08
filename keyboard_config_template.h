/*
 * Copy this file to your application configuration directory as
 * keyboard_config.h, then set LIBAMP_INCLUDE_DIR to that directory before
 * adding libamp with CMake. The enabled settings form a minimal analog USB
 * keyboard; uncomment and tune optional settings as needed.
 */
#ifndef KEYBOARD_CONFIG_H_
#define KEYBOARD_CONFIG_H_

/************/
/* Keyboard */
/************/
#define LAYER_NUM               1       /* Number of keymap layers. */
#define ADVANCED_KEY_NUM        1       /* Number of analog/advanced keys. */
#define KEY_NUM                 0       /* Number of ordinary digital keys. */
#define POLLING_RATE            1000    /* USB report rate and keyboard tick rate. */
#define CALIBRATION_DELAY       1000    /* Delay in ms before manual calibration. */
#define DEBUG_INTERVAL          0       /* Debug-packet interval in ticks; 0 disables it. */
// #define CONFIG_USB_HS                 /* Use high-speed USB; set POLLING_RATE to 8000. */
// #define KEYBOARD_OPERATION_POLLING    /* Handle keyboard operations from the event poller. */
// #define KEYBOARD_VERSION_MAJOR 1      /* Override the reported major version. */
// #define KEYBOARD_VERSION_MINOR 0      /* Override the reported minor version. */
// #define KEYBOARD_VERSION_PATCH 0      /* Override the reported patch version. */
// #define KEYBOARD_VERSION_INFO "custom" /* Override the reported version text. */

/* Debounce duration is measured in keyboard ticks. */
#define DEBOUNCE_PRESS          0       /* Press debounce duration. */
#define DEBOUNCE_PRESS_EAGER    1       /* Report a press before press debounce expires. */
#define DEBOUNCE_RELEASE        0       /* Release debounce duration. */
#define DEBOUNCE_RELEASE_EAGER  1       /* Report a release before release debounce expires. */

/* Optional keyboard behavior and memory/performance tuning. */
// #define SUSPEND_ENABLE                /* Enable USB suspend and remote wakeup handling. */
// #define CONSOLE_ENABLE                /* Enable the libamp console transport. */
// #define CONSOLE_BUFFER_LENGTH 256     /* Console buffer size in bytes. */
// #define LOG_ENABLE                    /* Enable libamp log output. */
// #define LOG_USE_COLOR                 /* Use ANSI colors in log output. */
// #define LOG_OUTPUT_FILE               /* Include source file and line in log output. */
// #define KEY_CALLBACK_ENABLE           /* Enable per-key press/release callbacks. */
#define OPTIMIZE_KEY_BITMAP           /* Use the compact key bitmap update path. */
#define OPTIMIZE_MOVING_AVERAGE_FOR_RINGBUF /* Keep a running analog-buffer sum. */
// #define EVENT_BUFFER_LENGTH 32        /* Queued keyboard-event capacity. */
// #define EVENT_CACHE_LENGTH 16         /* Cached-event entry capacity. */
// #define EVENT_CACHE_BUFFER_LENGTH 4   /* Cached-event queue capacity. */
// #define AMP_RX_QUEUE_LENGTH 4         /* AMP receive-frame queue depth. */
// #define AMP_TX_HIGH_QUEUE_LENGTH 4    /* High-priority AMP transmit queue depth. */
// #define AMP_TX_STREAM_QUEUE_LENGTH 4  /* Stream AMP transmit queue depth. */
// #define AMP_TX_POLICY AMP_TX_POLICY_CONTROL_PRIORITY /* AMP queue scheduling policy. */
/* AMP_TX_POLICY_CONTROL_PRIORITY favors control frames; AMP_TX_POLICY_RELIABLE_FIFO is FIFO. */

/****************/
/* Advanced keys */
/****************/
#define ANALOG_VALUE_MAX                    65535   /* Maximum normalized analog value. */
#define ANALOG_VALUE_MIN                    0       /* Minimum normalized analog value. */
#define LUT_LENGTH                          4096    /* Advanced-key lookup-table length. */
#define DEFAULT_ADVANCED_KEY_MODE            ADVANCED_KEY_ANALOG_NORMAL_MODE /* Default key mode. */
#define DEFAULT_CALIBRATION_MODE             ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED /* Detect sensor direction. */
#define DEFAULT_TRIGGER_DISTANCE             0.10f   /* Default press travel ratio. */
#define DEFAULT_RELEASE_DISTANCE             0.10f   /* Default release travel ratio. */
#define DEFAULT_UPPER_DEADZONE               0.01f   /* Dead zone at the upper end. */
#define DEFAULT_LOWER_DEADZONE               0.01f   /* Dead zone at the lower end. */
#define DEFAULT_ACTIVATION_VALUE             0.50f   /* Default activation value. */
#define DEFAULT_DEACTIVATION_VALUE           0.49f   /* Default deactivation value. */
#define DEFAULT_ESTIMATED_RANGE              500     /* Initial raw sensor range estimate. */

/*
 * DEFAULT_ADVANCED_KEY_MODE choices:
 * ADVANCED_KEY_DIGITAL_MODE: Treat a nonzero input value as pressed.
 * ADVANCED_KEY_ANALOG_NORMAL_MODE: Use fixed activation/deactivation values.
 * ADVANCED_KEY_ANALOG_RAPID_MODE: Re-trigger from travel relative to extrema.
 * ADVANCED_KEY_ANALOG_SPEED_MODE: Change state from travel speed.
 *
 * DEFAULT_CALIBRATION_MODE choices:
 * ADVANCED_KEY_NO_CALIBRATION: Keep the configured range unchanged.
 * ADVANCED_KEY_AUTO_CALIBRATION_POSITIVE: Track an increasing raw range.
 * ADVANCED_KEY_AUTO_CALIBRATION_NEGATIVE: Track a decreasing raw range.
 * ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED: Detect the raw direction first.
 */

/****************/
/* Analog input */
/****************/
#define RING_BUF_LEN             8       /* Samples retained by each ring buffer. */
#define ANALOG_BUFFER_LENGTH     1       /* Number of libamp analog ring buffers. */
// #define CALIBRATION_LPF_ENABLE         /* Low-pass raw values during calibration. */

/*************/
/* Filtering */
/* FILTER_TYPE_LOW_PASS smooths samples; FILTER_TYPE_KALMAN estimates them. */
/* FILTER_DOMAIN_RAW filters before normalization; FILTER_DOMAIN_NORMALIZED filters after it. */
/*************/
// #define FILTER_ENABLE                  /* Enable the selected analog filter. */
// #define FILTER_TYPE FILTER_TYPE_LOW_PASS /* FILTER_TYPE_LOW_PASS or FILTER_TYPE_KALMAN. */
// #define FILTER_DOMAIN FILTER_DOMAIN_RAW /* FILTER_DOMAIN_RAW or FILTER_DOMAIN_NORMALIZED. */
// #define FILTER_LOWPASS_ALPHA 0.5f      /* Low-pass smoothing factor. */
// #define FILTER_HYSTERESIS_ENABLE       /* Apply an additional hysteresis filter. */
// #define FILTER_HYSTERESIS 3            /* Hysteresis amount in the selected domain. */

/*****************/
/* Input extras */
/*****************/
// #define ENCODER_ENABLE                 /* Enable rotary-encoder processing. */
// #define ENCODER_NUM 1                  /* Number of rotary encoders. */
// #define ENCODER_TAP_DElAY 8            /* Encoder key-state duration in ticks. */

/*************/
/* Storage */
/*************/
/* STORAGE_ENABLE and LFS_ENABLE must be enabled together. */
// #define STORAGE_ENABLE                  /* Enable profiles and persistent settings. */
// #define LFS_ENABLE                      /* Enable littlefs storage. */
// #define LFS_READ_SIZE 16                /* Minimum flash read size. */
// #define LFS_PROG_SIZE 16                /* Minimum flash program size. */
// #define LFS_BLOCK_SIZE 4096             /* Flash erase-block size. */
// #define LFS_BLOCK_COUNT 16              /* Number of reserved erase blocks. */
// #define LFS_CACHE_SIZE 16               /* littlefs cache size. */
// #define LFS_LOOKAHEAD_SIZE 16           /* littlefs allocation lookahead size. */
// #define LFS_BLOCK_CYCLES 500            /* Wear-leveling relocation interval. */
// #define STORAGE_PROFILE_FILE_NUM 4      /* Number of persistent profiles. */
// #define AMP_OBJECT_CRC32_ENABLE 1       /* Verify complete Object transfers with CRC32. */
// #define AMP_OBJECT_TEMP_FILE_ENABLE 1   /* Commit Object writes by atomically renaming a temporary file. */

/********/
/* RGB */
/********/
// #define RGB_ENABLE                      /* Enable RGB state and effects. */
// #define RGB_NUM ADVANCED_KEY_NUM        /* Number of addressable LEDs. */
// #define RGB_MAX_DURATION 1000           /* Maximum effect duration in ms. */
// #define FADING_DISTANCE 5.0f            /* Fading-effect travel distance. */
// #define JELLY_DISTANCE 5.0f             /* Jelly-effect travel distance. */
// #define BUBBLE_DISTANCE 2.5f            /* Bubble-effect travel distance. */
// #define PORT_LOCATION {0, 0}            /* RGB effect origin in key units. */
// #define RGB_FLASH_MAX_DURATION 1000     /* Maximum flash-effect duration in ms. */
// #define RGB_FLASH_RIPPLE_SPEED 500      /* Flash-ripple propagation speed. */
// #define RGB_DEFAULT_MODE RGB_MODE_LINEAR /* Initial RGB effect mode. */
// #define RGB_DEFAULT_SPEED 20            /* Initial RGB effect speed. */
// #define RGB_DEFAULT_COLOR_HSV {273, 78, 99} /* Initial RGB HSV color. */
// #define RGB_LEFT 0.0f                   /* RGB layout left boundary. */
// #define RGB_TOP -0.5f                   /* RGB layout top boundary. */
// #define RGB_RIGHT 15.0f                 /* RGB layout right boundary. */
// #define RGB_BOTTOM 4.5f                 /* RGB layout bottom boundary. */
// #define RGB_DEPTH 0.0f                  /* RGB layout depth boundary. */
// #define RGB_GAMMA_ENABLE                /* Apply gamma correction. */
// #define RGB_GAMMA 2.2f                  /* Gamma correction exponent. */
// #define RGB_ARGUMENT_LIST_BUFFER_LENGTH 64 /* RGB command-argument buffer size. */
// #define RGB_CUSTOM_INVERSE_MAPPING      /* Provide g_rgb_inverse_mapping yourself. */
// #define RGB_BASE_MODE_USE_RAINBOW 1     /* Include the rainbow base effect. */
// #define RGB_BASE_MODE_USE_WAVE 1        /* Include the wave base effect. */
// #define RGB_MODE_USE_STATIC 1           /* Include the static effect. */
// #define RGB_MODE_USE_CYCLE 1            /* Include the color-cycle effect. */
// #define RGB_MODE_USE_LINEAR 1           /* Include the linear effect. */
// #define RGB_MODE_USE_TRIGGER 1          /* Include the trigger effect. */
// #define RGB_MODE_USE_STRING 1           /* Include the string effect. */
// #define RGB_MODE_USE_FADING_STRING 1    /* Include the fading-string effect. */
// #define RGB_MODE_USE_DIAMOND_RIPPLE 1   /* Include the diamond-ripple effect. */
// #define RGB_MODE_USE_FADING_DIAMOND_RIPPLE 1 /* Include the fading diamond ripple. */
// #define RGB_MODE_USE_JELLY 1            /* Include the jelly effect. */
// #define RGB_MODE_USE_BUBBLE 1           /* Include the bubble effect. */

/*
 * RGB_DEFAULT_MODE choices:
 * RGB_MODE_FIXED: Direct per-LED color.          RGB_MODE_STATIC: Static mixed color.
 * RGB_MODE_CYCLE: Cycle hue over time.           RGB_MODE_LINEAR: Map analog travel to color.
 * RGB_MODE_TRIGGER: Flash on a key transition.   RGB_MODE_STRING: Draw a horizontal string.
 * RGB_MODE_FADING_STRING: Fade a horizontal string over time.
 * RGB_MODE_DIAMOND_RIPPLE: Draw a Manhattan-distance ripple.
 * RGB_MODE_FADING_DIAMOND_RIPPLE: Fade that ripple over time.
 * RGB_MODE_JELLY: Spread an analog response between nearby LEDs.
 * RGB_MODE_BUBBLE: Draw a circular ripple.
 */

/********************/
/* Runtime features */
/********************/
// #define DYNAMICKEY_ENABLE               /* Enable dynamic-key behaviors. */
// #define DYNAMIC_KEY_NUM 32              /* Number of dynamic-key definitions. */
// #define DYNAMIC_KEY_HYSTERESIS A_ANTI_NORM(0.005f) /* Dynamic-key hysteresis. */
// #define MACRO_ENABLE                    /* Enable macro recording and playback. */
// #define MACRO_NUM 4                     /* Number of macro slots. */
// #define MACRO_MAX_ACTIONS 128           /* Maximum actions per macro. */

/* SCRIPT_ENABLE requires STORAGE_ENABLE and LFS_ENABLE. */
/* SCRIPT_AOT executes stored bytecode; SCRIPT_JIT compiles stored source. */
// #define SCRIPT_ENABLE                   /* Enable the JavaScript runtime. */
// #define SCRIPT_POLLING                  /* Run scripts from keyboard_process(). */
// #define SCRIPT_RUNTIME_STRATEGY SCRIPT_AOT /* SCRIPT_AOT or SCRIPT_JIT. */
// #define SCRIPT_SOURCE_BUFFER_SIZE (1 * 1024) /* Script-source buffer size. */
// #define SCRIPT_BYTECODE_BUFFER_SIZE (1 * 1024) /* Script-bytecode buffer size. */
// #define SCRIPT_MEMORY_SIZE (4 * 1024)   /* Script runtime memory budget. */
// #define SCRIPT_MAX_TIMERS 16            /* Maximum script timers. */

/********************/
/* Diagnostics data */
/********************/
// #define COUNTER_ENABLE                  /* Collect basic key counters. */
// #define KPS_ENABLE                      /* Collect keys-per-second data. */
// #define KPS_HISTORY_ENABLE              /* Retain keys-per-second history. */
// #define BIT_STREAM_ENABLE               /* Retain key bitmap history. */
// #define ANALOG_HISTORY_ENABLE           /* Retain analog-value history. */
// #define KPS_HISTORY_LENGTH 65           /* KPS history sample count. */
// #define BIT_STREAM_LENGTH 128           /* Bitmap history sample count. */
// #define ANALOG_HISTORY_LENGTH 129       /* Analog history sample count. */
// #define RECORD_MAX_KEY_NUM 8            /* Maximum keys retained by recorder. */
// #define KPS_REFRESH_RATE 144            /* KPS refresh rate in Hz. */

/*********/
/* Nexus */
/*********/
// #define NEXUS_ENABLE                    /* Enable multi-device Nexus support. */
// #define NEXUS_IS_SLAVE 0                /* 0 for master, 1 for slave firmware. */
// #define NEXUS_USE_RAW 0                 /* Synchronize raw analog values. */
// #define NEXUS_SLAVE_NUM 1               /* Number of Nexus slaves. */
// #define NEXUS_SLICE_LENGTH_MAX 16       /* Maximum keys in one slave slice. */
// #define NEXUS_VALUE_MAX 65535           /* Maximum synchronized key value. */
// #define NEXUS_BUFFER_SIZE 8             /* Per-slave report buffer size. */
// #define NEXUS_RETRY_COUNT 100           /* Nexus send retry limit. */

/****************/
/* USB identity */
/****************/
#define USB_POLLING_INTERVAL_MS  1       /* USB interrupt interval; 1 ms/125 us. */
#define FIXED_CONTROL_ENDPOINT_SIZE 0x40 /* Control endpoint packet size. */
#define FIXED_NUM_CONFIGURATIONS 1       /* Number of USB configurations. */

#define VENDOR_ID                0x1234  /* USB vendor ID. */
#define PRODUCT_ID               0x0001  /* USB product ID. */
#define DEVICE_VER               0x0001  /* USB device release number. */
#define MANUFACTURER             "Example Manufacturer" /* USB manufacturer string. */
#define PRODUCT                  "Example Analog Keyboard" /* USB product string. */
#define SERIAL_NUMBER            "0001" /* Fixed USB serial string. */

#define USB_MAX_POWER_CONSUMPTION 500    /* Declared USB bus power in mA. */
#define USB_ENDPOINTS_ARE_REORDERABLE   /* Allow paired IN/OUT endpoint numbers. */

// #define SERIAL_NUMBER_USE_CUSTOM        /* Generate the serial with a platform hook. */
// #define SERIAL_NUMBER_LENGTH 32         /* Maximum custom serial length. */

// #define MAX_ENDPOINTS 8                 /* Usable non-control endpoint count. */

/******************/
/* USB interfaces */
/******************/
// #define SHARED_EP_ENABLE                /* Add a shared HID endpoint for optional reports. */
// #define KEYBOARD_SHARED_EP              /* Put the keyboard report on the shared endpoint. */

// #define NKRO_ENABLE                     /* Add the NKRO keyboard report. */

// #define EXTRAKEY_ENABLE                 /* Add consumer/system key reports. */

// #define MOUSE_ENABLE                    /* Add the mouse report. */
// #define MOUSE_SHARED_EP                 /* Put the mouse report on the shared endpoint. */

// #define RAW_ENABLE                      /* Add the Raw HID interface. */
// #define RAW_USAGE_PAGE 0xFF60           /* Raw HID usage page. */
// #define RAW_USAGE_ID 0x61               /* Raw HID usage ID. */

// #define MIDI_ENABLE                     /* Add the USB MIDI interface. */
// #define MIDI_DEFAULT_AUDIO_HANDLER_ENABLE 1 /* Enable default MIDI audio callbacks. */
// #define MIDI_STREAM_EPSIZE 64           /* MIDI endpoint packet size; use 512 for HS. */

// #define JOYSTICK_ENABLE                 /* Add the joystick report. */
// #define JOYSTICK_SHARED_EP              /* Put the joystick report on the shared endpoint. */
// #define JOYSTICK_BUTTON_COUNT 8         /* Number of joystick buttons. */
// #define JOYSTICK_AXIS_COUNT 2           /* Number of joystick axes. */
// #define JOYSTICK_AXIS_RESOLUTION 8      /* Bits per joystick axis. */
// #define JOYSTICK_HAS_HAT                /* Add an eight-direction joystick hat. */

// #define DIGITIZER_ENABLE                /* Add the digitizer report. */
// #define DIGITIZER_SHARED_EP             /* Put the digitizer report on the shared endpoint. */

// #define PROGRAMMABLE_BUTTON_ENABLE      /* Add the programmable-button report. */

// #define GAMEPAD_ENABLE                  /* Add the XInput-compatible gamepad interface. */

// #define VIRTSER_ENABLE                  /* Add the virtual serial (CDC) interface. */

// #define LIGHTING_ENABLE                 /* Add the HID LampArray interface. */
// #define LAMPARRAY_KIND LAMPARRAY_KIND_KEYBOARD /* LampArray device category. */
// #define LAMPARRAY_UPDATE_INTERVAL 10000 /* Minimum LampArray update interval in us. */
/* LAMPARRAY_KIND may be UNDEFINED, KEYBOARD, MOUSE, GAME_CONTROLLER, PERIPHERAL,
 * SCENE, NOTIFICATION, CHASSIS, WEARABLE, FURNITURE, ART, HEADSET, MICROPHONE,
 * or SPEAKER (use the corresponding LAMPARRAY_KIND_* constant). */

/*******************/
/* USB extensions */
/*******************/
/* MTP_ENABLE requires a storage-enabled USB backend. */
// #define MTP_ENABLE                      /* Add Media Transfer Protocol support. */
// #define MTP_DESCRIPTION "Keyboard Flash" /* MTP storage description. */
// #define MTP_MAX_HANDLES 32              /* Maximum MTP object handles. */
// #define MTP_MAX_PATH_LEN 255            /* Maximum MTP path length. */
// #define MTP_DATA_EPSIZE 64              /* MTP data endpoint packet size. */

// #define WEBUSB_ENABLE                   /* Add the WebUSB capability. */
// #define WEBUSB_VENDOR_CODE 0x22         /* WebUSB control-request code. */
// #define WEBUSB_URL "example.com"        /* WebUSB landing-page URL. */

// #define MSOS20_VENDOR_CODE 0x21         /* Microsoft OS 2.0 control-request code. */

#endif /* KEYBOARD_CONFIG_H_ */
