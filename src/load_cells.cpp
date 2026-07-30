#include "load_cells.h"
#include <HX711.h>
#include "config.h"

// ── Debug: set to 1 to see per-sensor fresh/waiting logs ──────────────────────
#define HX711_READ_LOG 0

// ── Internal state ────────────────────────────────────────────────────────────

static HX711 scales[NUM_SENSORS];

static const int DT_PINS[NUM_SENSORS]  = HX711_DT_PINS;
static const int SCK_PINS[NUM_SENSORS] = HX711_SCK_PINS;

// Cache of last good reading per sensor (always valid)
static long cached_values[NUM_SENSORS] = {0};

// Physical connection — set once at init, never changes
static bool sensor_online[NUM_SENSORS] = {false};

// ── Fresh-data tracking ───────────────────────────────────────────────────────
// last_fresh_values[i]    — most recent raw value read from HX711
// sensor_new_data_pending — true after a successful read, cleared by consume()
// last_read_time[i]       — timestamp of last successful read (rate-limit)
static long last_fresh_values[NUM_SENSORS] = {0};
static bool sensor_new_data_pending[NUM_SENSORS] = {false};
static unsigned long last_read_time[NUM_SENSORS] = {0};

// Round-robin: fair rotation over shared SCK (reading one consumes all)
static int robin_start = 0;
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

    if (now - last_read_time[sensor_id] < HX711_READ_INTERVAL_MS) {
        return cached_values[sensor_id];
    }

    if (scales[sensor_id].is_ready()) {
        long value = scales[sensor_id].read();

        if (value == -8388608 || value == 8388607) {
            return cached_values[sensor_id];
        }

        cached_values[sensor_id] = value;
        last_fresh_values[sensor_id] = value;
        last_read_time[sensor_id] = now;
        sensor_new_data_pending[sensor_id] = true;
        return value;
    }

    return cached_values[sensor_id];
}

// ── Read all sensors (round-robin, shared-SCK-safe) ───────────────────────────

bool load_cells_read_all(long values[NUM_SENSORS]) {
    unsigned long now = millis();
    int read_idx = -1;

    // Fair round-robin: try sensors starting from robin_start
    for (int offset = 0; offset < NUM_SENSORS; offset++) {
        int i = (robin_start + offset) % NUM_SENSORS;

        // Rate-limit per sensor
        if (now - last_read_time[i] < HX711_READ_INTERVAL_MS) continue;

        if (scales[i].is_ready()) {
            long value = scales[i].read();

            if (value == -8388608 || value == 8388607) continue;

            cached_values[i] = value;
            last_fresh_values[i] = value;
            last_read_time[i] = now;
            sensor_new_data_pending[i] = true;
            read_idx = i;
            break;  // shared SCK — only one sensor per cycle
        }
    }

    // Fill output with cached values (fresh one already updated above)
    for (int i = 0; i < NUM_SENSORS; i++) {
        values[i] = cached_values[i];
    }

    // Advance round-robin start for next cycle
    if (read_idx >= 0) {
        robin_start = (read_idx + 1) % NUM_SENSORS;
    }

    return true;
}

// ── Consume pending fresh data (persistent across cycles) ─────────────────────

bool load_cells_consume_new_data(int sensor_id, long* value) {
    if (sensor_id < 0 || sensor_id >= NUM_SENSORS) return false;
    if (!sensor_new_data_pending[sensor_id]) return false;
    *value = last_fresh_values[sensor_id];
    sensor_new_data_pending[sensor_id] = false;
    return true;
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
    return sensor_new_data_pending[sensor_id];
}

const char* load_cells_sensor_status(int sensor_id) {
    if (sensor_id < 0 || sensor_id >= NUM_SENSORS) return "INVALID";
    if (sensor_online[sensor_id]) return "OK";
    return "NO_RESPONSE";
}

int load_cells_get_healthy_count() {
    return healthy_count;
}
