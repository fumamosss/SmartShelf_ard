#ifndef WARDROBE_SM_H
#define WARDROBE_SM_H

#include <Arduino.h>

// ── States ───────────────────────────────────────────────────────────────────
enum WardrobeState {
    WS_IDLE,                    // waiting for NFC card
    WS_AUTHENTICATING,          // sending UID to 1C, waiting for response
    WS_CAPTURING_BASELINE,      // reading stable baseline from sensors
    WS_WAITING_FOR_WEIGHT,      // waiting for user to take/put item
    WS_SENDING_OPERATION,       // sending results to 1C
    WS_SUCCESS,                 // operation accepted — brief pause then back to IDLE
    WS_ERROR                    // error — log it, brief pause then back to IDLE
};

// Initialize the wardrobe state machine.
void wardrobe_sm_begin();

// Drive one tick of the state machine. Call in loop().
void wardrobe_sm_update();

// Get current state.
WardrobeState wardrobe_sm_get_state();

// Get human-readable name for the current state.
const char* wardrobe_sm_state_name(WardrobeState s);

#endif
