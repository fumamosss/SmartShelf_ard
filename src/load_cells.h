#ifndef LOAD_CELLS_H
#define LOAD_CELLS_H

#include <Arduino.h>
#include "config.h"

// Initialize all HX711 modules.
// Each sensor has its own DT pin and shares SCK (configured in config.h).
// Determines sensor_online status — sensors not responding at init are marked offline.
// Returns true if at least one sensor is online.
bool load_cells_begin();

// Read raw ADC value from a specific sensor (0..NUM_SENSORS-1).
// Rate-limited per sensor — returns cached value if polled too soon.
long load_cells_read_raw(int sensor_id);

// Read all sensors and store values in the output array.
// Rate-limited per sensor — returns cached value for sensors polled too soon
// or currently not ready. Does NOT mark sensors as failed on transient not-ready.
// Returns true (cache is always available).
bool load_cells_read_all(long values[NUM_SENSORS]);

// Get the last cached reading for a specific sensor.
long load_cells_get_cached(int sensor_id);

// Check if a specific sensor is physically responding (DT goes LOW).
// This is the raw HX711 hardware ready state — may flip rapidly.
bool load_cells_is_ready(int sensor_id);

// Check if a sensor was detected during init (set once, never changes).
bool load_cells_is_online(int sensor_id);

// Check if a sensor returned a brand-new reading in the last
// load_cells_read_all() call. Returns false if cached value was used.
bool load_cells_has_new_data(int sensor_id);

// Consume a pending fresh reading (if available).
// On the first call after a fresh read, returns true and writes the value to *value,
// then clears the pending flag. Subsequent calls return false until the next fresh read.
bool load_cells_consume_new_data(int sensor_id, long* value);

// Get a human-readable status for each sensor.
// Returns "OK" (online) or "NO_RESPONSE" (offline at init).
const char* load_cells_sensor_status(int sensor_id);

// Get number of sensors that responded at init.
int load_cells_get_healthy_count();

#endif
