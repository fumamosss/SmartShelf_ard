#ifndef WEIGHT_MONITOR_H
#define WEIGHT_MONITOR_H

#include <Arduino.h>
#include "config.h"

// ── States of the weight monitor ─────────────────────────────────────────────
enum WeightState {
    WM_IDLE,                    // not monitoring
    WM_CAPTURING_BASELINE,      // taking baseline readings
    WM_WAITING_FOR_CHANGE,      // baseline captured, waiting for delta > threshold
    WM_WAITING_FOR_STABLE,      // change detected, waiting for values to stabilize
    WM_OPERATION_COMPLETE,      // stable final values captured — ready to read
    WM_TIMEOUT                  // operation timed out
};

// Initialize the weight monitor. Call once after load_cells_begin().
void weight_monitor_begin();

// Start a new monitoring session.
// Takes baseline readings and begins waiting for weight changes.
void weight_monitor_start();

// Call repeatedly in loop(). Drives the state machine.
void weight_monitor_update();

// Get current state.
WeightState weight_monitor_get_state();

// Get baseline values (only valid after baseline is captured).
const long* weight_monitor_get_baseline();

// Get final values (only valid when state == WM_OPERATION_COMPLETE).
const long* weight_monitor_get_final();

// Get delta values (final - baseline, only valid when complete).
const long* weight_monitor_get_delta();

// Reset monitor back to idle.
void weight_monitor_reset();

#endif
