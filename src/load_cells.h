#ifndef LOAD_CELLS_H
#define LOAD_CELLS_H

#include <Arduino.h>
#include "config.h"

// Initialize all 8 HX711 modules.
// Each sensor has its own DT pin and shares SCK (configured in config.h).
// Returns true if at least one sensor is responding.
bool load_cells_begin();

// Read raw ADC value from a specific sensor (0..7).
// Returns the raw value, or 0 if sensor is not ready or not responding.
long load_cells_read_raw(int sensor_id);

// Read all 8 sensors and store values in the output array.
// Returns true if all sensors responded, false if some failed.
bool load_cells_read_all(long values[NUM_SENSORS]);

// Get the last cached reading for a specific sensor.
long load_cells_get_cached(int sensor_id);

// Check if a specific sensor is responding (DT goes LOW).
bool load_cells_is_ready(int sensor_id);

// Get a human-readable status for each sensor.
// Returns "OK", "NOT_READY", or "NO_RESPONSE".
const char* load_cells_sensor_status(int sensor_id);

// Get number of sensors that responded successfully in the last read_all().
int load_cells_get_healthy_count();

#endif
