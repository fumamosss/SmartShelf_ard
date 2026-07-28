#include "net_client.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.h"

// ── Internal helpers ──────────────────────────────────────────────────────────

static bool check_wifi() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[NET] WiFi not connected — skipping request");
        return false;
    }
    return true;
}

// ── HTTP requests ─────────────────────────────────────────────────────────────

String net_client_get(const char *url) {
    if (!check_wifi()) return "";

    HTTPClient http;
    http.begin(url);
    http.setTimeout(NET_CLIENT_TIMEOUT_MS);

    Serial.printf("[NET] GET %s\n", url);
    int code = http.GET();

    String body = "";
    if (code > 0) {
        body = http.getString();
        Serial.printf("[NET] GET %s → %d (%d bytes)\n", url, code, (int)body.length());
    } else {
        Serial.printf("[NET] GET %s → ERROR %d: %s\n", url, code, http.errorToString(code).c_str());
    }
    http.end();
    return body;
}

String net_client_post(const char *url, const char *body) {
    return net_client_post(url, body, "text/plain");
}

String net_client_post_json(const char *url, const char *json) {
    return net_client_post(url, json, "application/json");
}

String net_client_post(const char *url, const char *body, const char *content_type) {
    if (!check_wifi()) return "";

    HTTPClient http;
    http.begin(url);
    http.setTimeout(NET_CLIENT_TIMEOUT_MS);
    http.addHeader("Content-Type", content_type);

    Serial.printf("[NET] POST %s (%s, %d bytes)\n", url, content_type, (int)strlen(body));
    int code = http.POST((uint8_t *)body, strlen(body));

    String response = "";
    if (code > 0) {
        response = http.getString();
        Serial.printf("[NET] POST %s → %d (%d bytes)\n", url, code, (int)response.length());
    } else {
        Serial.printf("[NET] POST %s → ERROR %d: %s\n", url, code, http.errorToString(code).c_str());
    }
    http.end();
    return response;
}
