#include "ble_prov.h"
#include <WiFi.h>
#include <WiFiProv.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include "config.h"

// ── Force-link esp32-hal-bt.c.o ──────────────────────────────────────────────
// Arduino-ESP32 2.0.17 bug: weak btInUse() in esp32-hal-misc.c returns false,
// causing initArduino() to call esp_bt_controller_mem_release(ESP_BT_MODE_BTDM)
// BEFORE setup() runs. This frees ALL BT memory, so simple_ble_start() fails.
// Reference btStarted() to force the linker to include esp32-hal-bt.c.o
// which provides the strong btInUse() returning true, preventing the release.
// See: https://github.com/espressif/arduino-esp32/issues/8176
extern "C" bool btStarted();
static volatile bool __attribute__((used)) _bt_hal_force_link = btStarted();
// ─────────────────────────────────────────────────────────────────────────────

static volatile bool prov_got_ip = false;
static volatile bool prov_ended  = false;

static void onProvEvent(arduino_event_t *event) {
    switch (event->event_id) {

    case ARDUINO_EVENT_PROV_START:
        Serial.println("[BLE PROV] provisioning started");
        break;

    case ARDUINO_EVENT_PROV_CRED_RECV: {
        Serial.println("[BLE PROV] credentials received");
        // Save credentials to NVS immediately when received.
        // The provisioning manager stores them internally too, but we need
        // them in our own NVS namespace for wifi_manager on next boot.
        wifi_config_t conf;
        esp_wifi_get_config(WIFI_IF_STA, &conf);
        Preferences p;
        p.begin(NVS_WIFI_NAMESPACE, false);
        p.putString(NVS_KEY_SSID,     (const char *)conf.sta.ssid);
        p.putString(NVS_KEY_PASSWORD, (const char *)conf.sta.password);
        p.end();
        Serial.printf("[BLE PROV] SSID saved: %s\n", (const char *)conf.sta.ssid);
        break;
    }

    case ARDUINO_EVENT_PROV_CRED_FAIL:
        Serial.println("[BLE PROV] credentials rejected");
        break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.printf("[BLE PROV] WiFi connected, IP: %s\n",
                      IPAddress(event->event_info.got_ip.ip_info.ip.addr)
                          .toString().c_str());
        prov_got_ip = true;
        break;

    case ARDUINO_EVENT_PROV_END:
        // NOTE: At this point wifi_prov_mgr_deinit() has ALREADY been called
        // by WiFiGeneric.cpp (line 528). The provisioning manager's internal
        // WiFi station handler has been destroyed. WiFi is typically disconnected
        // at this point.
        Serial.println("[BLE PROV] provisioning ended (WiFi state will settle)");
        prov_ended = true;
        break;

    default:
        break;
    }
}

bool ble_prov_start(const char* service_name) {
    prov_got_ip = false;
    prov_ended  = false;

    // Register event handler once — guard against repeated calls
    static bool handler_registered = false;
    if (!handler_registered) {
        WiFi.onEvent(onProvEvent);
        handler_registered = true;
    }

    Serial.printf("[BLE PROV] starting as: %s\n", service_name);
    Serial.println("[BLE PROV] open ESP BLE Provisioning app on your phone");

    WiFiProv.beginProvision(
        WIFI_PROV_SCHEME_BLE,
        WIFI_PROV_SCHEME_HANDLER_FREE_BTDM,
        WIFI_PROV_SECURITY_1,
        NULL,              // no PIN
        service_name);     // PROV_<MAC>

    // Phase 1: wait for WiFi connection (GOT_IP) or timeout
    uint32_t start_ms = millis();
    while (!prov_got_ip) {
        delay(500);
        if (millis() - start_ms > PROV_TIMEOUT_MS) {
            Serial.println("[BLE PROV] timeout");
            return false;
        }
    }

    // Phase 2: wait for provisioning manager to finish cleanup (PROV_END).
    // After GOT_IP this should take < 5 seconds, 30s is a generous cap.
    while (!prov_ended) {
        delay(100);
        if (millis() - start_ms > PROV_TIMEOUT_MS) {
            Serial.println("[BLE PROV] timeout waiting for PROV_END");
            return false;
        }
    }

    // Phase 3: settle delay.
    // wifi_prov_mgr_deinit() was already called by WiFiGeneric.cpp inside
    // the WIFI_PROV_END handler. It tears down the provisioning manager's
    // internal WiFi station handler, which typically disconnects WiFi.
    // Give the WiFi stack time to stabilize before the caller checks status.
    delay(500);

    Serial.printf("[BLE PROV] done (WiFi status: %d, IP: %s)\n",
                  WiFi.status(), WiFi.localIP().toString().c_str());
    return true;
}
