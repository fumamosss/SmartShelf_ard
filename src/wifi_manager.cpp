#include "wifi_manager.h"
#include <WiFi.h>
#include <Preferences.h>
#include "config.h"

static unsigned long last_reconnect_attempt = 0;
// Must be >= connect() timeout (30s). Otherwise check_reconnect() fires
// while WiFi.begin() is still in progress and kills it with disconnect().
static const unsigned long RECONNECT_INTERVAL_MS = 35000;

// Internal: load credentials from NVS. Returns true if SSID is present.
static bool load_credentials(String &ssid, String &password)
{
    Preferences prefs;
    prefs.begin(NVS_WIFI_NAMESPACE, true);
    ssid = prefs.getString(NVS_KEY_SSID, "");
    password = prefs.getString(NVS_KEY_PASSWORD, "");
    prefs.end();
    return ssid.length() > 0;
}

// Internal: connect to Wi-Fi with blocking wait (up to 30s).
static bool connect(const char *ssid, const char *password)
{
    // Disconnect from any previous connection to ensure clean state.
    // Without this, WiFi.begin() may fail with status 1 (IDLE) or 5 (DISCONNECTED)
    // if the previous connection was not properly torn down (e.g. after a crash/reset).
    WiFi.disconnect(true);
    delay(500);

    Serial.printf("[WIFI] connecting to %s ...\n", ssid);
    WiFi.begin(ssid, password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 30000)
    {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("[WIFI] connected - IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WIFI] MAC: %s\n", WiFi.macAddress().c_str());
        return true;
    }

    Serial.printf("[WIFI] connection failed (status: %d)\n", WiFi.status());
    return false;
}

bool wifi_manager_begin()
{
    WiFi.mode(WIFI_STA);
    WiFi.persistent(true);

    String ssid, password;
    if (load_credentials(ssid, password))
    {
        Serial.printf("[WIFI] found saved credentials for: %s\n", ssid.c_str());
        return connect(ssid.c_str(), password.c_str());
    }

    Serial.println("[WIFI] no saved credentials found");
    return false;
}

bool wifi_manager_is_connected()
{
    return WiFi.status() == WL_CONNECTED;
}

String wifi_manager_get_mac()
{
    return WiFi.macAddress();
}

String wifi_manager_get_ip()
{
    return WiFi.localIP().toString();
}

bool wifi_manager_has_saved_credentials()
{
    Preferences prefs;
    prefs.begin(NVS_WIFI_NAMESPACE, false); // write mode: creates namespace if absent
    bool has = prefs.isKey(NVS_KEY_SSID);
    prefs.end();
    return has;
}

void wifi_manager_check_reconnect()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    if (millis() - last_reconnect_attempt >= RECONNECT_INTERVAL_MS)
    {
        last_reconnect_attempt = millis();

        // WiFi.reconnect() is unreliable in Arduino-ESP32 2.0.17 —
        // it often fails with status 1 (IDLE) or 5 (DISCONNECTED) because
        // the WiFi stack is left in a bad state after disconnect.
        // Use the same reliable path as initial connection:
        // full disconnect → load credentials → WiFi.begin().
        String ssid, password;
        if (load_credentials(ssid, password))
        {
            Serial.println("[WIFI] connection lost - reconnecting...");
            WiFi.disconnect(true);
            delay(500);
            WiFi.begin(ssid.c_str(), password.c_str());
        }
        else
        {
            Serial.println("[WIFI] reconnect failed - no saved credentials");
        }
    }
}
