#include "load_cells.h"
#include <HX711.h>
#include "config.h"

// 8 HX711 instances — each with its own DT pin, sharing SCK
static HX711 scales[NUM_SENSORS];

// Pin arrays parsed from config macros
static const int DT_PINS[NUM_SENSORS] = HX711_DT_PINS;
static const int SCK_PINS[NUM_SENSORS] = HX711_SCK_PINS;

// Cache of last readings
static long cached_values[NUM_SENSORS] = {0};
static bool sensor_ok[NUM_SENSORS] = {false};
static int healthy_count = 0;

bool load_cells_begin() {
    Serial.printf("[LOAD] initializing %d HX711 sensors...\n", NUM_SENSORS);

    healthy_count = 0;
    for (int i = 0; i < NUM_SENSORS; i++) {
        scales[i].begin(DT_PINS[i], SCK_PINS[i]);

        // Wait for sensor with timeout
        unsigned long start = millis();
        while (!scales[i].is_ready()) {
            if (millis() - start > HX711_READY_TIMEOUT_MS) {
                break;
            }
            delay(1);
        }

        if (scales[i].is_ready()) {
            sensor_ok[i] = true;
            cached_values[i] = scales[i].read();
            healthy_count++;
            Serial.printf("[LOAD]   sensor %d: OK (DT=%d, SCK=%d, raw=%ld)\n",
                          i, DT_PINS[i], SCK_PINS[i], cached_values[i]);
        } else {
            sensor_ok[i] = false;
            cached_values[i] = 0;
            Serial.printf("[LOAD]   sensor %d: NOT READY (DT=%d, SCK=%d)\n",
                          i, DT_PINS[i], SCK_PINS[i]);
        }

        // HX711 power-up produces -8388608 or 8388607 — treat as invalid
        if (sensor_ok[i] && (cached_values[i] == -8388608 || cached_values[i] == 8388607)) {
            sensor_ok[i] = false;
            cached_values[i] = 0;
            healthy_count--;
            Serial.printf("[LOAD]   sensor %d: INVALID READ\n", i);
        }
    }

    Serial.printf("[LOAD] %d/%d sensors responding\n", healthy_count, NUM_SENSORS);

    if (healthy_count == 0) {
        Serial.println("[LOAD] WARNING: no HX711 sensors found — check wiring");
    }

    return healthy_count > 0;
}

long load_cells_read_raw(int sensor_id) {
    if (sensor_id < 0 || sensor_id >= NUM_SENSORS) return 0;

    if (scales[sensor_id].is_ready()) {
        long value = scales[sensor_id].read();

        // HX711 power-up produces -8388608 or 8388607 — treat as invalid
        if (value == -8388608 || value == 8388607) {
            sensor_ok[sensor_id] = false;
            return cached_values[sensor_id];
        }

        cached_values[sensor_id] = value;
        sensor_ok[sensor_id] = true;
        return value;
    }

    return cached_values[sensor_id];
}

bool load_cells_read_all(long values[NUM_SENSORS]) {
    bool all_ok = true;

    for (int i = 0; i < NUM_SENSORS; i++) {
        if (scales[i].is_ready()) {
            long value = scales[i].read();

            // HX711 power-up produces -8388608 or 8388607 — treat as invalid
            if (value == -8388608 || value == 8388607) {
                values[i] = cached_values[i];
                sensor_ok[i] = false;
                all_ok = false;
            } else {
                values[i] = value;
                cached_values[i] = value;
                sensor_ok[i] = true;
            }
        } else {
            // Sensor not ready — use cached value
            values[i] = cached_values[i];
            sensor_ok[i] = false;
            all_ok = false;
        }
    }

    // Update healthy count
    healthy_count = 0;
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (sensor_ok[i]) healthy_count++;
    }

    return all_ok;
}

long load_cells_get_cached(int sensor_id) {
    if (sensor_id < 0 || sensor_id >= NUM_SENSORS) return 0;
    return cached_values[sensor_id];
}

bool load_cells_is_ready(int sensor_id) {
    if (sensor_id < 0 || sensor_id >= NUM_SENSORS) return false;
    return scales[sensor_id].is_ready();
}

const char* load_cells_sensor_status(int sensor_id) {
    if (sensor_id < 0 || sensor_id >= NUM_SENSORS) return "INVALID";
    if (sensor_ok[sensor_id]) return "OK";
    return "NO_RESPONSE";
}

int load_cells_get_healthy_count() {
    return healthy_count;
}
