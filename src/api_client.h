#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>
#include "config.h"

// ── Result structures ────────────────────────────────────────────────────────

struct NfcCheckResult {
    bool success;       // HTTP request succeeded
    bool allowed;       // user is allowed to operate
    String user_name;   // user display name (empty if not found)
};

struct OperationResult {
    bool success;       // HTTP request succeeded
    bool accepted;      // 1C accepted the operation
    String message;     // server message (for logging)
};

// ── API functions ────────────────────────────────────────────────────────────

// Check if NFC card UID is authorized.
// Sends: GET {API_BASE_URL}{API_NFC_CHECK_ENDPOINT}?uid={uid}
// Expects JSON: {"success":true,"allowed":true,"user":"Name"}
NfcCheckResult api_check_nfc(const String &uid);

// Submit weight operation result.
// Sends: POST {API_BASE_URL}{API_OPERATION_ENDPOINT}
// Body JSON with nfc_uid, baseline, final, delta for all 8 sensors.
// Expects JSON: {"success":true,"accepted":true,"message":"..."}
OperationResult api_submit_operation(
    const String &nfc_uid,
    const long baseline[NUM_SENSORS],
    const long final_values[NUM_SENSORS],
    const long delta[NUM_SENSORS]
);

#endif
