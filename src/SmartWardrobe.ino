#include <Arduino.h>
#include <esp_arduino_version.h>
#include "config.h"
#include "wifi_manager.h"
#include "ble_prov.h"
#include "nfc.h"
#include "load_cells.h"
#include "net_client.h"

// build service name from MAC: PROV_A1B2C3
String build_service_name() {
    String mac = wifi_manager_get_mac();
    // remove colons and add prefix: A1:B2:C3:D4:E5:F6 -> PROV_A1B2C3D4E5F6
    mac.replace(":", "");
    return String(PROV_SERVICE_PREFIX) + mac;
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);

    Serial.println("=== SmartWardrobe ===");
    Serial.printf("Arduino-ESP32: %d.%d.%d\n",
                  ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
    Serial.printf("ESP-IDF: %s\n", esp_get_idf_version());
    Serial.printf("MAC: %s\n", wifi_manager_get_mac().c_str());

    // initialize stubs
    nfc_begin();
    load_cells_begin();

    if (wifi_manager_has_saved_credentials()) {
        // ── Reboot path: credentials exist, just connect ──────────────────
        Serial.println("[BOOT] saved credentials found - connecting...");
        wifi_manager_begin();
    } else {
        // ── First boot: no credentials, run BLE provisioning ─────────────
        Serial.println("[BOOT] no saved credentials - starting BLE provisioning");
        String service_name = build_service_name();
        if (!ble_prov_start(service_name.c_str())) {
            Serial.println("[BOOT] provisioning failed or timed out");
            return;
        }

        // Provisioning complete. The manager's internal WiFi station handler
        // was torn down during wifi_prov_mgr_deinit() inside PROV_END.
        // WiFi is typically disconnected at this point.
        // Always reconnect using saved credentials — WiFi.begin() internally
        // detects same-config+connected and returns early if WiFi is still up.
        Serial.println("[BOOT] provisioning done - connecting to WiFi...");
        wifi_manager_begin();
    }

    // final status
    if (wifi_manager_is_connected()) {
        Serial.printf("[BOOT] WiFi connected — IP: %s\n", wifi_manager_get_ip().c_str());
    } else {
        Serial.println("[BOOT] WiFi not connected — will retry in loop()");
    }
}

void loop() {
    // handle Wi-Fi reconnection
    wifi_manager_check_reconnect();

    // one-shot HTTP test after WiFi connects
    static bool http_tested = false;
    if (!http_tested && wifi_manager_is_connected()) {
        http_tested = true;
        Serial.println("[BOOT] testing HTTP requests...");
        String get_resp = net_client_get("http://httpbin.org/get");
        String post_resp = net_client_post_json("http://httpbin.org/post", "{\"test\":\"hello\"}");
        if (get_resp.length() > 0 && post_resp.length() > 0) {
            Serial.println("[BOOT] HTTP test passed — GET and POST work");
        } else {
            Serial.println("[BOOT] HTTP test FAILED — check network");
        }
    }

    // stubs - will be replaced with real implementations later
    if (nfc_card_available()) {
        String uid = nfc_read_card_uid();
        Serial.printf("[NFC] card detected: %s\n", uid.c_str());
    }

    if (load_cells_has_weight_change()) {
        int cell = load_cells_get_changed_cell();
        float weight = load_cells_get_weight(cell);
        Serial.printf("[LOAD CELLS] cell %d weight: %.2f g\n", cell, weight);
    }
}
