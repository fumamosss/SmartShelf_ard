#include "weight_monitor.h"
#include "load_cells.h"

// ── Shelf geometry ────────────────────────────────────────────────────────────
#define NUM_SHELVES 2

// Which shelf a sensor belongs to (0-based)
static inline int sensor_shelf(int id) {
    return (id < 4) ? 0 : 1;
}

// Per-sensor (keep for API compatibility)
static long baseline[NUM_SENSORS] = {0};
static int baseline_count[NUM_SENSORS] = {0};
static long final_values[NUM_SENSORS] = {0};
static long delta[NUM_SENSORS] = {0};

// Per-shelf state
static long shelf_baseline[NUM_SHELVES] = {0};       // baseline shelf total (sum of normalized baselines)
static long shelf_changing_ref[NUM_SHELVES] = {0};   // shelf total at change moment
static int  shelf_stable_samples[NUM_SHELVES] = {0}; // consecutive stable counts
static bool shelf_changed[NUM_SHELVES] = {false};

// Timing
static unsigned long last_poll_time = 0;
static unsigned long operation_start = 0;
static WeightState state = WM_IDLE;

// ── Helpers ───────────────────────────────────────────────────────────────────

// Sum of normalized sensor values for a shelf (actual weight, for stabilization).
static long compute_shelf_total(int shelf_id, const long norms[NUM_SENSORS]) {
    long total = 0;
    int first = shelf_id * 4;
    int last  = first + 4;
    for (int i = first; i < last; i++) {
        total += norms[i];
    }
    return total;
}

// ── Public API ────────────────────────────────────────────────────────────────

void weight_monitor_begin() {
    state = WM_IDLE;
    Serial.println("[WM] weight monitor ready");
}

void weight_monitor_start() {
    for (int i = 0; i < NUM_SENSORS; i++) {
        baseline[i]         = 0;
        baseline_count[i]   = 0;
        final_values[i]     = 0;
        delta[i]            = 0;
    }
    for (int k = 0; k < NUM_SHELVES; k++) {
        shelf_baseline[k]        = 0;
        shelf_changing_ref[k]    = 0;
        shelf_stable_samples[k]  = 0;
        shelf_changed[k]         = false;
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

    long norms[NUM_SENSORS] = {0};  // populated from fresh data

    switch (state) {

    // ═══════════════════════════════════════════════════════════════════════════
    //  CAPTURING BASELINE — accumulate fresh normalized readings, time-windowed
    // ═══════════════════════════════════════════════════════════════════════════
    case WM_CAPTURING_BASELINE: {
        if (now - operation_start < BASELINE_CAPTURE_TIME_MS) {
            long readings[NUM_SENSORS];
            load_cells_read_all(readings);

            for (int i = 0; i < NUM_SENSORS; i++) {
                long fresh_val;
                if (!load_cells_consume_new_data(i, &fresh_val)) continue;
                long norm = load_cells_apply_tare(i, fresh_val);
                baseline[i] += norm;
                baseline_count[i]++;
            }
            break;
        }

        // Time window expired — per-sensor averages & shelf baselines
        Serial.println("[WM] baseline complete:");
        for (int i = 0; i < NUM_SENSORS; i++) {
            if (load_cells_is_online(i) && baseline_count[i] > 0) {
                baseline[i] /= baseline_count[i];
            } else {
                baseline[i] = 0;
            }
            Serial.printf("[WM]   sensor %d: samples=%d value=%ld\n",
                          i, baseline_count[i], baseline[i]);
        }

        for (int k = 0; k < NUM_SHELVES; k++) {
            int first = k * 4;
            shelf_baseline[k] = 0;
            for (int i = first; i < first + 4; i++) {
                shelf_baseline[k] += baseline[i];
            }
        }

        state = WM_WAITING_FOR_CHANGE;
        Serial.println("[WM] monitoring for change");
        break;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  WAITING FOR CHANGE — compare shelf totals against shelf baselines
    // ═══════════════════════════════════════════════════════════════════════════
    case WM_WAITING_FOR_CHANGE: {
        if (now - operation_start > WEIGHT_OPERATION_TIMEOUT_MS) {
            Serial.println("[WM] TIMEOUT — no weight change detected");
            state = WM_TIMEOUT;
            return;
        }

        long readings[NUM_SENSORS];
        load_cells_read_all(readings);

        // Build normalized array from fresh data (fallback to cached for stale)
        for (int i = 0; i < NUM_SENSORS; i++) {
            norms[i] = load_cells_apply_tare(i, readings[i]);
        }

        bool any_changed = false;
        for (int k = 0; k < NUM_SHELVES; k++) {
            int first = k * 4;
            long shelf_delta = 0;

            // Sum of noise-filtered per-sensor deltas
            for (int i = first; i < first + 4; i++) {
                long d = norms[i] - baseline[i];
                if (d < 0) d = -d;
                if (d > WEIGHT_NOISE_THRESHOLD) {
                    shelf_delta += (norms[i] - baseline[i]);
                }
                // else: |delta| ≤ noise threshold → treat as 0
            }

            if (shelf_delta < 0) shelf_delta = -shelf_delta;
            if (shelf_delta > WEIGHT_CHANGE_THRESHOLD) {
                shelf_changed[k] = true;
                // Capture absolute shelf weight at change moment (unfiltered)
                shelf_changing_ref[k] = compute_shelf_total(k, norms);
                shelf_stable_samples[k] = 0;
                any_changed = true;
                Serial.printf("[WM] shelf %d change: delta=%ld\n", k, shelf_delta);
            }
        }

        if (any_changed) {
            // Pre-mark unchanged shelves as stable
            for (int k = 0; k < NUM_SHELVES; k++) {
                if (!shelf_changed[k]) shelf_stable_samples[k] = WEIGHT_STABLE_SAMPLES;
            }
            state = WM_WAITING_FOR_STABLE;
            Serial.println("[WM] waiting for stability...");
        }
        break;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  WAITING FOR STABLE — each changed shelf must hold steady for N samples
    // ═══════════════════════════════════════════════════════════════════════════
    case WM_WAITING_FOR_STABLE: {
        if (now - operation_start > WEIGHT_OPERATION_TIMEOUT_MS) {
            Serial.println("[WM] TIMEOUT — weight never stabilized");
            state = WM_TIMEOUT;
            return;
        }

        long readings[NUM_SENSORS];
        load_cells_read_all(readings);

        for (int i = 0; i < NUM_SENSORS; i++) {
            norms[i] = load_cells_apply_tare(i, readings[i]);
        }

        for (int k = 0; k < NUM_SHELVES; k++) {
            if (!shelf_changed[k]) continue;

            long shelf_total = compute_shelf_total(k, norms);
            long diff = shelf_total - shelf_changing_ref[k];
            if (diff < 0) diff = -diff;

            if (diff <= WEIGHT_STABLE_TOLERANCE) {
                shelf_stable_samples[k]++;
                if (shelf_stable_samples[k] > WEIGHT_STABLE_SAMPLES)
                    shelf_stable_samples[k] = WEIGHT_STABLE_SAMPLES;
            } else {
                shelf_changing_ref[k] = shelf_total;
                shelf_stable_samples[k] = 0;
            }
        }

        // All changed shelves must be stable
        bool all_stable = true;
        for (int k = 0; k < NUM_SHELVES; k++) {
            if (!shelf_changed[k]) continue;
            if (shelf_stable_samples[k] < WEIGHT_STABLE_SAMPLES) {
                all_stable = false;
                break;
            }
        }

        if (all_stable) {
            for (int i = 0; i < NUM_SENSORS; i++) {
                final_values[i] = norms[i];
                delta[i] = final_values[i] - baseline[i];
            }

            state = WM_OPERATION_COMPLETE;
            Serial.println("[WM] operation complete:");
            for (int i = 0; i < NUM_SENSORS; i++) {
                Serial.printf("[WM]   sensor_%d: value=%ld delta=%ld\n",
                              i, final_values[i], delta[i]);
            }
            for (int k = 0; k < NUM_SHELVES; k++) {
                if (shelf_changed[k]) {
                    long shelf_delta = 0;
                    int first = k * 4;
                    for (int i = first; i < first + 4; i++) {
                        shelf_delta += delta[i];
                    }
                    Serial.printf("[WM]   shelf %d total delta=%ld\n", k, shelf_delta);
                }
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
