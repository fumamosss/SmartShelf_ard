#ifndef SHELF_SM_H
#define SHELF_SM_H

#include <Arduino.h>

// ── States ───────────────────────────────────────────────────────────────────
enum ShelfState {
    SS_IDLE,                    // waiting for NFC card
    SS_AUTHENTICATING,          // sending UID to 1C, waiting for response
    SS_CAPTURING_BASELINE,      // reading stable baseline from sensors
    SS_WAITING_FOR_WEIGHT,      // waiting for user to take/put item
    SS_SENDING_OPERATION,       // sending results to 1C
    SS_SUCCESS,                 // operation accepted — brief pause then back to IDLE
    SS_ERROR                    // error — log it, brief pause then back to IDLE
};

// Initialize the shelf state machine.
void shelf_sm_begin();

// Drive one tick of the state machine. Call in loop().
void shelf_sm_update();

// Get current state.
ShelfState shelf_sm_get_state();

// Get human-readable name for the current state.
const char* shelf_sm_state_name(ShelfState s);

#endif
