#include "weight_monitor.h"
#include "load_cells.h"

static WeightState state = WM_IDLE;

// ── Baseline tracking (per-sensor) ────────────────────────────────────────────
static long baseline[NUM_SENSORS] = {0};
static int baseline_count[NUM_SENSORS] = {0};

// ── Current stable reference (pre-change) ────────────────────────────────────
static long stable_weight[NUM_SENSORS] = {0};

// ── Stabilization tracking ────────────────────────────────────────────────────
static long changing_ref[NUM_SENSORS] = {0};   // first reading after change
static int stable_samples[NUM_SENSORS] = {0};  // per-sensor consecutive stable count

// ── Operation results ─────────────────────────────────────────────────────────
static long final_values[NUM_SENSORS] = {0};
static long delta[NUM_SENSORS] = {0};

// ── Timing ────────────────────────────────────────────────────────────────────
static unsigned long last_poll_time = 0;
static unsigned long operation_start = 0;

// ── Public API ────────────────────────────────────────────────────────────────

void weight_monitor_begin() {
    state = WM_IDLE;
    Serial.println("[WM] weight monitor ready");
}

void weight_monitor_start() {
    for (int i = 0; i < NUM_SENSORS; i++) {
        baseline[i]         = 0;
        baseline_count[i]   = 0;
        stable_weight[i]    = 0;
        changing_ref[i]     = 0;
        stable_samples[i]   = 0;
        final_values[i]     = 0;
        delta[i]            = 0;
    }
    last_poll_time   = 0;
    operation_start  = millis();

    state = WM_CAPTURING_BASELINE;
    Serial.println("[WM] capturing baseline...");
}

void weight_monitor_update() {
    unsigned long now = millis();
    if (now - last_poll_time < HX711_SAMPLE_INTERVAL_MS) return;
    last_poll_time = now;

    switch (state) {

    // ═══════════════════════════════════════════════════════════════════════════
    //  CAPTURING BASELINE — each sensor collects N fresh readings
    // ═══════════════════════════════════════════════════════════════════════════
    case WM_CAPTURING_BASELINE: {
        long readings[NUM_SENSORS];
        load_cells_read_all(readings);

        // Accumulate only fresh readings per sensor
        for (int i = 0; i < NUM_SENSORS; i++) {
            if (!load_cells_has_new_data(i)) continue;
            baseline[i] += readings[i];
            baseline_count[i]++;
        }

        // Check completion — all online sensors must hit WEIGHT_BASELINE_SAMPLES
        bool done = true;
        for (int i = 0; i < NUM_SENSORS; i++) {
            if (!load_cells_is_online(i)) continue;
            if (baseline_count[i] >= WEIGHT_BASELINE_SAMPLES) {
                // Print milestone once (count just hit the target)
                if (baseline_count[i] == WEIGHT_BASELINE_SAMPLES) {
                    baseline[i] /= WEIGHT_BASELINE_SAMPLES;
                    Serial.printf("[WM] baseline sensor %d: %d/%d\n",
                                  i, WEIGHT_BASELINE_SAMPLES, WEIGHT_BASELINE_SAMPLES);
                }
            } else {
                done = false;
            }
        }

        if (done) {
            for (int i = 0; i < NUM_SENSORS; i++) {
                stable_weight[i] = baseline[i];
            }
            state = WM_WAITING_FOR_CHANGE;
            Serial.println("[WM] baseline complete — monitoring for change");
        }
        break;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  WAITING FOR CHANGE — compare fresh readings against stable_weight
    // ═══════════════════════════════════════════════════════════════════════════
    case WM_WAITING_FOR_CHANGE: {
        if (now - operation_start > WEIGHT_OPERATION_TIMEOUT_MS) {
            Serial.println("[WM] TIMEOUT — no weight change detected");
            state = WM_TIMEOUT;
            return;
        }

        long readings[NUM_SENSORS];
        load_cells_read_all(readings);

        // Check any sensor changed significantly from stable weight
        bool changed = false;
        for (int i = 0; i < NUM_SENSORS; i++) {
            if (!load_cells_has_new_data(i)) continue;

            long diff = readings[i] - stable_weight[i];
            if (diff < 0) diff = -diff;

            if (diff > WEIGHT_CHANGE_THRESHOLD) {
                changed = true;
                break;
            }
        }

        if (changed) {
            // Capture reference values at change moment
            for (int i = 0; i < NUM_SENSORS; i++) {
                changing_ref[i] = readings[i];
                stable_samples[i] = 0;
            }
            state = WM_WAITING_FOR_STABLE;
            Serial.println("[WM] weight change detected — waiting for stability...");
        }
        break;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  WAITING FOR STABLE — collect N consecutive stable fresh readings
    // ═══════════════════════════════════════════════════════════════════════════
    case WM_WAITING_FOR_STABLE: {
        if (now - operation_start > WEIGHT_OPERATION_TIMEOUT_MS) {
            Serial.println("[WM] TIMEOUT — weight never stabilized");
            state = WM_TIMEOUT;
            return;
        }

        long readings[NUM_SENSORS];
        load_cells_read_all(readings);

        // Per-sensor stability check against the reference (changing_ref)
        for (int i = 0; i < NUM_SENSORS; i++) {
            if (!load_cells_has_new_data(i)) continue;

            long diff = readings[i] - changing_ref[i];
            if (diff < 0) diff = -diff;

            if (diff <= WEIGHT_STABLE_TOLERANCE) {
                stable_samples[i]++;
                // Cap so int overflow doesn't cause false completion
                if (stable_samples[i] > WEIGHT_STABLE_SAMPLES)
                    stable_samples[i] = WEIGHT_STABLE_SAMPLES;
            } else {
                // Jump detected — shift reference, reset this sensor's counter
                changing_ref[i] = readings[i];
                stable_samples[i] = 0;
            }
        }

        // All online sensors must have reached the stable sample count
        bool all_stable = true;
        for (int i = 0; i < NUM_SENSORS; i++) {
            if (!load_cells_is_online(i)) continue;
            if (stable_samples[i] < WEIGHT_STABLE_SAMPLES) {
                all_stable = false;
                break;
            }
        }

        if (all_stable) {
            for (int i = 0; i < NUM_SENSORS; i++) {
                final_values[i] = readings[i];
                delta[i] = final_values[i] - baseline[i];
            }

            state = WM_OPERATION_COMPLETE;
            Serial.println("[WM] operation complete:");
            for (int i = 0; i < NUM_SENSORS; i++) {
                Serial.printf("[WM]   sensor_%d: stable=%ld final=%ld delta=%ld\n",
                              i, stable_weight[i], final_values[i], delta[i]);
            }
        }
        break;
    }

    case WM_OPERATION_COMPLETE:
    case WM_TIMEOUT:
    case WM_IDLE:
        break;
    }
}

// ── Accessors ─────────────────────────────────────────────────────────────────

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
