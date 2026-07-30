#include <Arduino.h>
#include <esp_arduino_version.h>
#include "config.h"
#include "wifi_manager.h"
#include "ble_prov.h"
#include "nfc.h"
#include "load_cells.h"
#include "net_client.h"
#include "weight_monitor.h"
#include "shelf_sm.h"

// build service name from MAC: PROV_A1B2C3
String build_service_name() {
    String mac = wifi_manager_get_mac();
    mac.replace(":", "");
    return String(PROV_SERVICE_PREFIX) + mac;
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);

    Serial.println("=== SmartShelf ===");
    Serial.printf("Arduino-ESP32: %d.%d.%d\n",
                  ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
    Serial.printf("ESP-IDF: %s\n", esp_get_idf_version());
    Serial.printf("MAC: %s\n", wifi_manager_get_mac().c_str());

    // ── Hardware initialization ──────────────────────────────────────────────
    nfc_begin();
    load_cells_begin();
    load_cells_tare();
    weight_monitor_begin();

    // ── Wi-Fi connection ─────────────────────────────────────────────────────
    if (wifi_manager_has_saved_credentials()) {
        Serial.println("[BOOT] saved credentials found - connecting...");
        wifi_manager_begin();
    } else {
        Serial.println("[BOOT] no saved credentials - starting BLE provisioning");
        String service_name = build_service_name();
        if (!ble_prov_start(service_name.c_str())) {
            Serial.println("[BOOT] provisioning failed or timed out");
            return;
        }
        Serial.println("[BOOT] provisioning done - connecting to WiFi...");
        wifi_manager_begin();
    }

    if (wifi_manager_is_connected()) {
        Serial.printf("[BOOT] WiFi connected — IP: %s\n", wifi_manager_get_ip().c_str());
    } else {
        Serial.println("[BOOT] WiFi not connected — will retry in loop()");
    }

    // ── Start state machine ──────────────────────────────────────────────────
    shelf_sm_begin();
    Serial.println("[BOOT] system ready\n");
}

void loop() {
    wifi_manager_check_reconnect();
    shelf_sm_update();
}
