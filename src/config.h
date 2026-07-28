#ifndef CONFIG_H
#define CONFIG_H

// device identification prefix for BLE provisioning service name
#define PROV_SERVICE_PREFIX "PROV_"

// NVS namespace for Wi-Fi credentials
#define NVS_WIFI_NAMESPACE "wifi_creds"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASSWORD "password"

// serial baud rate - must match monitor_speed in platformio.ini
#define SERIAL_BAUD 115200

// provisioning timeout - 5 minutes to enter Wi-Fi credentials
#define PROV_TIMEOUT_MS 300000

// HTTP request timeout (ms)
#define NET_CLIENT_TIMEOUT_MS 10000

#endif
