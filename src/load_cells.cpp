#include "load_cells.h"
#include <HX711.h>
#include "config.h"

// ── Debug: set to 1 to see per-sensor fresh/waiting logs ──────────────────────
#define HX711_READ_LOG 1

// ── Internal state ────────────────────────────────────────────────────────────

static HX711 scales[NUM_SENSORS];

static const int DT_PINS[NUM_SENSORS]  = HX711_DT_PINS;
static const int SCK_PINS[NUM_SENSORS] = HX711_SCK_PINS;

// Cache of last good reading per sensor
static long cached_values[NUM_SENSORS] = {0};

// Physical connection — set once at init, never changes
static bool sensor_online[NUM_SENSORS] = {false};

// True if the sensor returned fresh data in the last read_all()
static bool sensor_has_new_data[NUM_SENSORS] = {false};

// Per-sensor rate-limit timestamp
static unsigned long last_read_time[NUM_SENSORS] = {0};

// Count of sensors that passed init
static int healthy_count = 0;

// ── Init ──────────────────────────────────────────────────────────────────────

bool load_cells_begin() {
    Serial.printf("[LOAD] initializing %d HX711 sensors...\n", NUM_SENSORS);

    healthy_count = 0;
    for (int i = 0; i < NUM_SENSORS; i++) {
        scales[i].begin(DT_PINS[i], SCK_PINS[i]);

        // Wait for sensor with timeout
        unsigned long start = millis();
        while (!scales[i].is_ready()) {
            if (millis() - start > HX711_READY_TIMEOUT_MS) break;
            delay(1);
        }

        if (scales[i].is_ready()) {
            long raw = scales[i].read();

            // Reject power-up garbage values
            if (raw == -8388608 || raw == 8388607) {
                sensor_online[i] = false;
                cached_values[i] = 0;
                Serial.printf("[LOAD]   sensor %d: INVALID READ\n", i);
            } else {
                sensor_online[i] = true;
                cached_values[i] = raw;
                healthy_count++;
                Serial.printf("[LOAD]   sensor %d: OK (DT=%d, SCK=%d, raw=%ld)\n",
                              i, DT_PINS[i], SCK_PINS[i], raw);
            }
        } else {
            sensor_online[i] = false;
            cached_values[i] = 0;
            Serial.printf("[LOAD]   sensor %d: NOT READY (DT=%d, SCK=%d)\n",
                          i, DT_PINS[i], SCK_PINS[i]);
        }
    }

    Serial.printf("[LOAD] %d/%d sensors responding\n", healthy_count, NUM_SENSORS);
    if (healthy_count == 0) {
        Serial.println("[LOAD] WARNING: no HX711 sensors found — check wiring");
    }

    return healthy_count > 0;
}

// ── Read single sensor ────────────────────────────────────────────────────────

long load_cells_read_raw(int sensor_id) {
    if (sensor_id < 0 || sensor_id >= NUM_SENSORS) return 0;

    unsigned long now = millis();

    // Rate-limit: respect HX711 10 SPS
    if (now - last_read_time[sensor_id] < HX711_READ_INTERVAL_MS) {
        sensor_has_new_data[sensor_id] = false;
        return cached_values[sensor_id];
    }

    if (scales[sensor_id].is_ready()) {
        long value = scales[sensor_id].read();

        if (value == -8388608 || value == 8388607) {
            sensor_has_new_data[sensor_id] = false;
            return cached_values[sensor_id];
        }

        cached_values[sensor_id] = value;
        last_read_time[sensor_id] = now;
        sensor_has_new_data[sensor_id] = true;
        return value;
    }

    sensor_has_new_data[sensor_id] = false;
    return cached_values[sensor_id];
}

// ── Read all sensors ──────────────────────────────────────────────────────────

bool load_cells_read_all(long values[NUM_SENSORS]) {
    unsigned long now = millis();

    for (int i = 0; i < NUM_SENSORS; i++) {
        // Rate-limit per sensor — don't poll faster than ~10 SPS
        if (now - last_read_time[i] < HX711_READ_INTERVAL_MS) {
            values[i] = cached_values[i];
            sensor_has_new_data[i] = false;
#if HX711_READ_LOG
            Serial.printf("[HX711] sensor %d waiting\n", i);
#endif
            continue;
        }

        if (scales[i].is_ready()) {
            long value = scales[i].read();

            // Reject power-up garbage values
            if (value == -8388608 || value == 8388607) {
                values[i] = cached_values[i];
                sensor_has_new_data[i] = false;
#if HX711_READ_LOG
                Serial.printf("[HX711] sensor %d stale (power-up value)\n", i);
#endif
                continue;
            }

            // Fresh reading
            cached_values[i] = value;
            last_read_time[i] = now;
            sensor_has_new_data[i] = true;
            values[i] = value;
#if HX711_READ_LOG
            Serial.printf("[HX711] sensor %d fresh raw=%ld\n", i, value);
#endif
        } else {
            // Sensor not ready this cycle — return cached value, NOT an error
            values[i] = cached_values[i];
            sensor_has_new_data[i] = false;
#if HX711_READ_LOG
            Serial.printf("[HX711] sensor %d waiting\n", i);
#endif
        }
    }

    return true;  // cache always available
}

// ── Utility accessors ─────────────────────────────────────────────────────────

long load_cells_get_cached(int sensor_id) {
    if (sensor_id < 0 || sensor_id >= NUM_SENSORS) return 0;
    return cached_values[sensor_id];
}

bool load_cells_is_ready(int sensor_id) {
    if (sensor_id < 0 || sensor_id >= NUM_SENSORS) return false;
    return scales[sensor_id].is_ready();
}

bool load_cells_is_online(int sensor_id) {
    if (sensor_id < 0 || sensor_id >= NUM_SENSORS) return false;
    return sensor_online[sensor_id];
}

bool load_cells_has_new_data(int sensor_id) {
    if (sensor_id < 0 || sensor_id >= NUM_SENSORS) return false;
    return sensor_has_new_data[sensor_id];
}

const char* load_cells_sensor_status(int sensor_id) {
    if (sensor_id < 0 || sensor_id >= NUM_SENSORS) return "INVALID";
    if (sensor_online[sensor_id]) return "OK";
    return "NO_RESPONSE";
}

int load_cells_get_healthy_count() {
    return healthy_count;
}
