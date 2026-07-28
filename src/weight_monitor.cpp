#include "weight_monitor.h"
#include "load_cells.h"

static WeightState state = WM_IDLE;

static long baseline[NUM_SENSORS] = {0};
static long current[NUM_SENSORS] = {0};
static long final_values[NUM_SENSORS] = {0};
static long delta[NUM_SENSORS] = {0};

static unsigned long baseline_start = 0;
static int baseline_sample_count = 0;
static unsigned long last_sample_time = 0;
static unsigned long stable_since = 0;
static unsigned long operation_start = 0;

void weight_monitor_begin() {
    state = WM_IDLE;
    Serial.println("[WM] weight monitor ready");
}

void weight_monitor_start() {
    // Reset all values
    for (int i = 0; i < NUM_SENSORS; i++) {
        baseline[i] = 0;
        current[i] = 0;
        final_values[i] = 0;
        delta[i] = 0;
    }

    baseline_sample_count = 0;
    baseline_start = millis();
    last_sample_time = 0;
    stable_since = 0;
    operation_start = millis();

    state = WM_CAPTURING_BASELINE;
    Serial.println("[WM] capturing baseline...");
}

void weight_monitor_update() {
    switch (state) {

    // ── CAPTURING BASELINE ───────────────────────────────────────────────────
    case WM_CAPTURING_BASELINE: {
        unsigned long now = millis();

        // Take samples at configured intervals
        if (now - last_sample_time < WEIGHT_BASELINE_SAMPLE_DELAY_MS) return;
        last_sample_time = now;

        long readings[NUM_SENSORS];
        bool all_ok = load_cells_read_all(readings);

        if (!all_ok) {
            // Some sensors not responding — log but continue with what we have
            Serial.printf("[WM] baseline sample %d: partial read (%d/%d sensors)\n",
                          baseline_sample_count + 1,
                          load_cells_get_healthy_count(), NUM_SENSORS);
        }

        // Accumulate
        for (int i = 0; i < NUM_SENSORS; i++) {
            baseline[i] += readings[i];
        }
        baseline_sample_count++;

        // Check if we have enough samples
        if (baseline_sample_count >= WEIGHT_BASELINE_SAMPLES) {
            // Average
            for (int i = 0; i < NUM_SENSORS; i++) {
                baseline[i] /= WEIGHT_BASELINE_SAMPLES;
            }

            state = WM_WAITING_FOR_CHANGE;
            Serial.println("[WM] baseline captured:");
            for (int i = 0; i < NUM_SENSORS; i++) {
                Serial.printf("[WM]   sensor_%d = %ld\n", i, baseline[i]);
            }
        }
        break;
    }

    // ── WAITING FOR CHANGE ───────────────────────────────────────────────────
    case WM_WAITING_FOR_CHANGE: {
        unsigned long now = millis();

        // Operation timeout
        if (now - operation_start > WEIGHT_OPERATION_TIMEOUT_MS) {
            Serial.println("[WM] TIMEOUT — no weight change detected");
            state = WM_TIMEOUT;
            return;
        }

        // Sample at configured interval
        if (now - last_sample_time < WEIGHT_SAMPLE_INTERVAL_MS) return;
        last_sample_time = now;

        long readings[NUM_SENSORS];
        load_cells_read_all(readings);

        // Check if any sensor changed beyond threshold
        bool change_detected = false;
        for (int i = 0; i < NUM_SENSORS; i++) {
            long diff = readings[i] - baseline[i];
            if (diff < 0) diff = -diff;
            if (diff > WEIGHT_CHANGE_THRESHOLD) {
                change_detected = true;
                break;
            }
        }

        if (change_detected) {
            // Store current readings as the start of the change
            for (int i = 0; i < NUM_SENSORS; i++) {
                current[i] = readings[i];
            }
            stable_since = now;
            state = WM_WAITING_FOR_STABLE;
            Serial.println("[WM] weight change detected — waiting for stability...");
        }
        break;
    }

    // ── WAITING FOR STABLE ───────────────────────────────────────────────────
    case WM_WAITING_FOR_STABLE: {
        unsigned long now = millis();

        // Operation timeout
        if (now - operation_start > WEIGHT_OPERATION_TIMEOUT_MS) {
            Serial.println("[WM] TIMEOUT — weight never stabilized");
            state = WM_TIMEOUT;
            return;
        }

        // Sample at configured interval
        if (now - last_sample_time < WEIGHT_SAMPLE_INTERVAL_MS) return;
        last_sample_time = now;

        long readings[NUM_SENSORS];
        load_cells_read_all(readings);

        // Check if all readings are close to what they were when change was detected
        bool still_stable = true;
        for (int i = 0; i < NUM_SENSORS; i++) {
            long diff = readings[i] - current[i];
            if (diff < 0) diff = -diff;
            if (diff > WEIGHT_STABLE_TOLERANCE) {
                still_stable = false;
                // Update current with latest reading for continued tracking
                current[i] = readings[i];
                break;
            }
        }

        if (still_stable) {
            // Check if stable long enough
            if (now - stable_since >= WEIGHT_STABLE_TIME_MS) {
                // Capture final values
                for (int i = 0; i < NUM_SENSORS; i++) {
                    final_values[i] = readings[i];
                    delta[i] = final_values[i] - baseline[i];
                }

                state = WM_OPERATION_COMPLETE;
                Serial.println("[WM] operation complete:");
                for (int i = 0; i < NUM_SENSORS; i++) {
                    Serial.printf("[WM]   sensor_%d: base=%ld final=%ld delta=%ld\n",
                                  i, baseline[i], final_values[i], delta[i]);
                }
            }
        } else {
            // Not stable — reset the stability timer
            stable_since = now;
        }
        break;
    }

    case WM_OPERATION_COMPLETE:
    case WM_TIMEOUT:
    case WM_IDLE:
        // Nothing to do
        break;
    }
}

WeightState weight_monitor_get_state() {
    return state;
}

const long* weight_monitor_get_baseline() {
    return baseline;
}

const long* weight_monitor_get_final() {
    return final_values;
}

const long* weight_monitor_get_delta() {
    return delta;
}

void weight_monitor_reset() {
    state = WM_IDLE;
    Serial.println("[WM] reset to idle");
}
