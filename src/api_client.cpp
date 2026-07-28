#include "api_client.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ── Helper: parse sensor JSON object ─────────────────────────────────────────
// Parses {"sensor_0":123,"sensor_1":456,...} into a long array.
// Returns the number of sensors found.
static int parse_sensor_json(const char *json, long values[NUM_SENSORS]) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return 0;

    int count = 0;
    for (int i = 0; i < NUM_SENSORS; i++) {
        char key[16];
        snprintf(key, sizeof(key), "sensor_%d", i);
        if (doc.containsKey(key)) {
            values[i] = doc[key].as<long>();
            count++;
        }
    }
    return count;
}

// ── NFC Check ────────────────────────────────────────────────────────────────

NfcCheckResult api_check_nfc(const String &uid) {
    NfcCheckResult result = {false, false, ""};

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[API] WiFi not connected — cannot check NFC");
        return result;
    }

    // Build URL
    String url = String(API_BASE_URL) + String(API_NFC_CHECK_ENDPOINT) + "?uid=" + uid;

    HTTPClient http;
    http.begin(url);
    http.setTimeout(NET_CLIENT_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");

    Serial.printf("[API] GET %s\n", url.c_str());
    int code = http.GET();

    if (code <= 0) {
        Serial.printf("[API] NFC check FAILED: %d (%s)\n", code, http.errorToString(code).c_str());
        http.end();
        return result;
    }

    String response = http.getString();
    http.end();

    Serial.printf("[API] NFC check response: %d — %s\n", code, response.c_str());

    // Parse JSON
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, response);
    if (err) {
        Serial.printf("[API] JSON parse error: %s\n", err.c_str());
        return result;
    }

    result.success = doc["success"] | false;
    result.allowed = doc["allowed"] | false;
    result.user_name = doc["user"] | "";

    return result;
}

// ── Submit Operation ─────────────────────────────────────────────────────────

OperationResult api_submit_operation(
    const String &nfc_uid,
    const long baseline[NUM_SENSORS],
    const long final_values[NUM_SENSORS],
    const long delta[NUM_SENSORS]
) {
    OperationResult result = {false, false, ""};

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[API] WiFi not connected — cannot submit operation");
        return result;
    }

    // Build JSON body
    StaticJsonDocument<1024> doc;
    doc["nfc_uid"] = nfc_uid;

    JsonObject bl = doc.createNestedObject("baseline");
    JsonObject fv = doc.createNestedObject("final");
    JsonObject dt = doc.createNestedObject("delta");

    for (int i = 0; i < NUM_SENSORS; i++) {
        char key[16];
        snprintf(key, sizeof(key), "sensor_%d", i);
        bl[key] = baseline[i];
        fv[key] = final_values[i];
        dt[key] = delta[i];
    }

    char body[1024];
    serializeJson(doc, body, sizeof(body));

    // Send POST
    String url = String(API_BASE_URL) + String(API_OPERATION_ENDPOINT);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(NET_CLIENT_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");

    Serial.printf("[API] POST %s (%d bytes)\n", url.c_str(), (int)strlen(body));
    Serial.printf("[API] body: %s\n", body);

    int code = http.POST((uint8_t *)body, strlen(body));

    if (code <= 0) {
        Serial.printf("[API] Operation submit FAILED: %d (%s)\n", code, http.errorToString(code).c_str());
        http.end();
        return result;
    }

    String response = http.getString();
    http.end();

    Serial.printf("[API] Operation response: %d — %s\n", code, response.c_str());

    // Parse JSON
    StaticJsonDocument<256> doc2;
    DeserializationError err = deserializeJson(doc2, response);
    if (err) {
        Serial.printf("[API] JSON parse error: %s\n", err.c_str());
        return result;
    }

    result.success = doc2["success"] | false;
    result.accepted = doc2["accepted"] | false;
    result.message = doc2["message"] | "";

    return result;
}
