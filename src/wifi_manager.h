#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

// Initialize Wi-Fi STA mode, load saved credentials, attempt connection.
// Returns true if connected within 15 seconds.
bool wifi_manager_begin();

// Check if currently connected to Wi-Fi.
bool wifi_manager_is_connected();

// Get ESP32 MAC address as string (e.g. "30:76:F5:E8:62:A4").
String wifi_manager_get_mac();

// Get current IP address as string (e.g. "192.168.1.79").
String wifi_manager_get_ip();

// Check if Wi-Fi credentials are saved in NVS.
bool wifi_manager_has_saved_credentials();

// Call in loop() to handle automatic reconnection on disconnect.
void wifi_manager_check_reconnect();

#endif
