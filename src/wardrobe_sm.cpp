#include "wardrobe_sm.h"
#include "nfc.h"
#include "load_cells.h"
#include "weight_monitor.h"
#include "api_client.h"

static WardrobeState state = WS_IDLE;
static unsigned long state_enter_time = 0;
static String current_uid = "";

// Duration to show success/error before returning to IDLE (ms)
static const unsigned long RESULT_DISPLAY_MS = 2000;

// ── State transitions ────────────────────────────────────────────────────────

static void enter_state(WardrobeState new_state) {
    state = new_state;
    state_enter_time = millis();
    Serial.printf("[SM] → %s\n", wardrobe_sm_state_name(new_state));
}

void wardrobe_sm_begin() {
    state = WS_IDLE;
    state_enter_time = millis();
    current_uid = "";
    Serial.println("[SM] wardrobe state machine ready — waiting for NFC card");
}

void wardrobe_sm_update() {
    unsigned long now = millis();

    switch (state) {

    // ═══════════════════════════════════════════════════════════════════════════
    // IDLE — wait for NFC card
    // ═══════════════════════════════════════════════════════════════════════════
    case WS_IDLE: {
        if (!nfc_card_available()) return;

        String uid = nfc_read_card_uid();
        if (uid.length() == 0) return;

        // Anti-repeat: skip if same card read recently
        if (nfc_is_duplicate(uid)) return;

        Serial.printf("[SM] card detected: %s\n", uid.c_str());
        current_uid = uid;

        // Verify card with 1C
        NfcCheckResult result = api_check_nfc(uid);

        if (!result.success) {
            Serial.println("[SM] 1C unavailable — cannot authenticate");
            enter_state(WS_ERROR);
            return;
        }

        if (!result.allowed) {
            Serial.printf("[SM] ACCESS DENIED for UID %s", uid.c_str());
            if (result.user_name.length() > 0) {
                Serial.printf(" (user: %s)", result.user_name.c_str());
            }
            Serial.println();
            nfc_reset_repeat_timer();
            enter_state(WS_ERROR);
            return;
        }

        Serial.printf("[SM] ACCESS GRANTED for %s (UID: %s)\n",
                      result.user_name.c_str(), uid.c_str());

        // Start weight monitoring
        weight_monitor_start();
        enter_state(WS_CAPTURING_BASELINE);
        break;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // CAPTURING_BASELINE — weight_monitor captures initial readings
    // ═══════════════════════════════════════════════════════════════════════════
    case WS_CAPTURING_BASELINE:
        weight_monitor_update();

        if (weight_monitor_get_state() == WM_WAITING_FOR_CHANGE) {
            enter_state(WS_WAITING_FOR_WEIGHT);
        } else if (weight_monitor_get_state() == WM_TIMEOUT) {
            Serial.println("[SM] baseline capture failed");
            enter_state(WS_ERROR);
        }
        break;

    // ═══════════════════════════════════════════════════════════════════════════
    // WAITING_FOR_WEIGHT — user takes/puts item, monitor detects change
    // ═══════════════════════════════════════════════════════════════════════════
    case WS_WAITING_FOR_WEIGHT:
        weight_monitor_update();

        if (weight_monitor_get_state() == WM_OPERATION_COMPLETE) {
            // Weight operation done — send results to 1C
            OperationResult op = api_submit_operation(
                current_uid,
                weight_monitor_get_baseline(),
                weight_monitor_get_final(),
                weight_monitor_get_delta()
            );

            if (op.success && op.accepted) {
                Serial.printf("[SM] operation accepted by 1C: %s\n", op.message.c_str());
                enter_state(WS_SUCCESS);
            } else if (op.success && !op.accepted) {
                Serial.printf("[SM] operation REJECTED by 1C: %s\n", op.message.c_str());
                enter_state(WS_ERROR);
            } else {
                Serial.println("[SM] failed to send operation to 1C");
                enter_state(WS_ERROR);
            }
        } else if (weight_monitor_get_state() == WM_TIMEOUT) {
            Serial.println("[SM] operation timed out — user took no action");
            enter_state(WS_ERROR);
        }
        break;

    // ═══════════════════════════════════════════════════════════════════════════
    // SENDING_OPERATION — (handled inline in WAITING_FOR_WEIGHT above)
    // ═══════════════════════════════════════════════════════════════════════════

    // ═══════════════════════════════════════════════════════════════════════════
    // SUCCESS — brief display, then return to IDLE
    // ═══════════════════════════════════════════════════════════════════════════
    case WS_SUCCESS:
        if (now - state_enter_time >= RESULT_DISPLAY_MS) {
            weight_monitor_reset();
            nfc_reset_repeat_timer();
            current_uid = "";
            Serial.println("[SM] operation complete — ready for next card");
            enter_state(WS_IDLE);
        }
        break;

    // ═══════════════════════════════════════════════════════════════════════════
    // ERROR — brief display, then return to IDLE
    // ═══════════════════════════════════════════════════════════════════════════
    case WS_ERROR:
        if (now - state_enter_time >= RESULT_DISPLAY_MS) {
            weight_monitor_reset();
            nfc_reset_repeat_timer();
            current_uid = "";
            Serial.println("[SM] error handled — ready for next card");
            enter_state(WS_IDLE);
        }
        break;
    }
}

WardrobeState wardrobe_sm_get_state() {
    return state;
}

const char* wardrobe_sm_state_name(WardrobeState s) {
    switch (s) {
        case WS_IDLE:                 return "IDLE";
        case WS_AUTHENTICATING:       return "AUTHENTICATING";
        case WS_CAPTURING_BASELINE:   return "CAPTURING_BASELINE";
        case WS_WAITING_FOR_WEIGHT:   return "WAITING_FOR_WEIGHT";
        case WS_SENDING_OPERATION:    return "SENDING_OPERATION";
        case WS_SUCCESS:              return "SUCCESS";
        case WS_ERROR:                return "ERROR";
        default:                      return "UNKNOWN";
    }
}
