#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// SMARTWARDROBE CONFIGURATION
// =============================================================================
// All hardware pins, API endpoints, and algorithm parameters are defined here.
// Change only what you need before flashing.
// =============================================================================

// ── General ──────────────────────────────────────────────────────────────────
#define SERIAL_BAUD 115200

// ── BLE Provisioning ────────────────────────────────────────────────────────
#define PROV_SERVICE_PREFIX "PROV_"
#define PROV_TIMEOUT_MS 300000

// ── Wi-Fi NVS ───────────────────────────────────────────────────────────────
#define NVS_WIFI_NAMESPACE "wifi_creds"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASSWORD "password"

// ── HTTP Client ──────────────────────────────────────────────────────────────
#define NET_CLIENT_TIMEOUT_MS 10000

// ── 1C API Endpoints ────────────────────────────────────────────────────────
// USER CONFIGURATION: CHANGE THIS — set your 1C server IP and port
#define API_BASE_URL "http://31.25.243.238:8213/SmartShelf_2237"

// USER CONFIGURATION: CHANGE THIS — NFC verification endpoint
#define API_NFC_CHECK_ENDPOINT "/api/shelf/nfc"

// USER CONFIGURATION: CHANGE THIS — weight operation endpoint
#define API_OPERATION_ENDPOINT "/api/shelf/operation"

// ── PN532 NFC Reader ────────────────────────────────────────────────────────
// TODO: SET YOUR PN532 SDA PIN (GPIO21 is default I2C SDA on ESP32)
#define NFC_SDA_PIN 21

// TODO: SET YOUR PN532 SCL PIN (GPIO22 is default I2C SCL on ESP32)
#define NFC_SCL_PIN 22

// PN532 I2C address (default: 0x24, some boards use 0x48)
#define NFC_I2C_ADDRESS 0x24

// Anti-repeat: minimum ms between reads of the same card UID
#define NFC_REPEAT_DELAY_MS 3000

// ── HX711 Load Cells ────────────────────────────────────────────────────────
#define NUM_SENSORS 8
#define SENSORS_PER_SHELF 4
#define NUM_SHELVES 2

// TODO: SET YOUR HX711 DT PINS — one per sensor
// Shelf 0: sensors 0..3
// Shelf 1: sensors 4..7
#define HX711_DT_PINS { 4, 5, 16, 17, 18, 19, 23, 25 }

// TODO: SET YOUR HX711 SCK PINS — one per sensor (or all same for shared SCK)
#define HX711_SCK_PINS { 26, 26, 26, 26, 26, 26, 26, 26 }

// HX711 gain: 128 (default), 64, or 32
#define HX711_GAIN 128

// Boot tare: how many fresh samples to average per sensor for zero offset
#define TARE_SAMPLES 10

// HX711 readiness timeout (ms) — how long to wait for a sensor to respond
#define HX711_READY_TIMEOUT_MS 200

// HX711 rate limit — minimum interval between polling loops (10 SPS ≈ 100 ms)
#define HX711_SAMPLE_INTERVAL_MS 100

// Minimum interval between reads of the same sensor (10 SPS = 100 ms between readings)
#define HX711_READ_INTERVAL_MS 100

// ── Weight Algorithm ────────────────────────────────────────────────────────
// Minimum per-shelf delta to consider a change (4-sensor sum).
// With noise threshold of 200 per sensor, 4 sensors sum to 800 worst-case noise.
// Set 1500+ to avoid false triggers from summed noise.
#define WEIGHT_CHANGE_THRESHOLD 1500

// Small per-sensor changes below this are treated as noise (delta = 0)
#define WEIGHT_NOISE_THRESHOLD 200

// Maximum allowed variation during "stable" detection
#define WEIGHT_STABLE_TOLERANCE 200

// Number of consecutive stable samples to confirm weight stability
#define WEIGHT_STABLE_SAMPLES 5

// Total operation timeout (ms) — cancel if user takes too long
#define WEIGHT_OPERATION_TIMEOUT_MS 30000

// Baseline capture window (ms) — accumulate fresh readings for this duration, then average per sensor
#define BASELINE_CAPTURE_TIME_MS 2000

#endif
