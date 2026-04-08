#pragma once

// ---- Board pin and identity configuration ----
#ifdef BOARD_ESP32PICO
#  define LED_PIN              27
#  define BUTTON_PIN           39
#  define HOSTNAME             "kprox"
#  define BLUETOOTH_DEVICE_NAME "KProx"
#endif

#ifdef BOARD_M5STACK_ATOMS3
#  define LED_PIN              35
#  define BUTTON_PIN           41
#  define HOSTNAME             "kprox"
#  define BLUETOOTH_DEVICE_NAME "KProx"
#  define BOARD_HAS_USB_HID
#endif

#ifdef BOARD_M5STACK_CARDPUTER
// STAMP S3A onboard WS2812 is on GPIO 21.
// GPIO 35 is display MOSI on the Cardputer — do NOT use it for FastLED.
#  define LED_PIN              21
#  define BUTTON_PIN           0
#  define HOSTNAME             "kprox"
#  define BLUETOOTH_DEVICE_NAME "KProx"
#  define DISPLAY_WIDTH        240
#  define DISPLAY_HEIGHT       135
#  define BOARD_HAS_USB_HID
#endif

// ---- FastLED ----
#define NUM_LEDS    1
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB

// ---- Default credentials ----
#define DEFAULT_WIFI_SSID      "kprox"
#define DEFAULT_WIFI_PASSWORD  "1337prox"
#define DEFAULT_API_KEY        "kprox1337"

// ---- Default device identity ----
#define DEFAULT_MANUFACTURER   "KProx"
#define DEFAULT_USB_VID        0xDEAD   // default spoof VID (settable via Device Identity)
#define DEFAULT_USB_PID        0xBEEF   // default spoof PID
#define DEFAULT_PRODUCT_NAME   "Robo Key-mouse"
#define USB_SERIAL_NUMBER      "KProx-001"
#define BATTERY_LEVEL          100

// ---- Watchdog timeout (seconds) ----
#define WDT_TIMEOUT            30

// ---- Button press thresholds (ms) ----
#define BUTTON_HALT_THRESHOLD        2000
#define BUTTON_DELETE_ALL_THRESHOLD  5000
#define DOUBLE_CLICK_THRESHOLD       300
#define BUTTON_DEBOUNCE              50

// ---- Network timings (ms) ----
#define UDP_DISCOVERY_PORT       48269
#define UDP_BROADCAST_INTERVAL   5000
#define WIFI_RECONNECT_INTERVAL  30000
#define STATUS_PRINT_INTERVAL    30000

// ---- Register limits ----
#define MAX_REGISTER_LENGTH      512

// ---- Mouse batching (ms) ----
#define MOUSE_BATCH_TIMEOUT      50

// ---- Heap guard (bytes) ----
// Must stay above lwIP's minimum packet-buffer requirement (~3-4 KB).
// Values of 8000/5000 caused spurious restarts during WebSocket operation;
// 4000 is the practical floor for stable WiFi + WebSocket coexistence.
#define MIN_HEAP_FREE            500
