#ifndef BLE_PROV_H
#define BLE_PROV_H

#include <Arduino.h>

// Start BLE-based Wi-Fi provisioning.
// Blocks until provisioning is complete or times out.
// service_name: unique per device, e.g. "PROV_A1B2C3D4E5F6"
// Returns true on success, false on timeout.
bool ble_prov_start(const char *service_name);

#endif
