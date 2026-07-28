#ifndef NET_CLIENT_H
#define NET_CLIENT_H

#include <Arduino.h>

// Initialize HTTP client module.
void net_client_begin();

// ── HTTP requests ─────────────────────────────────────────────────────────────
// All functions return the response body as String.
// On failure: returns empty string, logs error with [NET] prefix.

// HTTP GET
String net_client_get(const char *url);

// HTTP POST with raw body (content-type: text/plain)
String net_client_post(const char *url, const char *body);

// HTTP POST with JSON body (content-type: application/json)
String net_client_post_json(const char *url, const char *json);

// HTTP POST with custom content-type
String net_client_post(const char *url, const char *body, const char *content_type);

#endif
