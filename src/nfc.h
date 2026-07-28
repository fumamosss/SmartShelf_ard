#ifndef NFC_H
#define NFC_H

#include <Arduino.h>

// Initialize PN532 NFC reader over I2C.
// Returns true if PN532 firmware version was read successfully.
// Returns false if PN532 is not connected or not responding.
bool nfc_begin();

// Check if an NFC card/tag is present and readable.
// Returns true if a card is detected within range.
bool nfc_card_available();

// Read the UID of the currently present card.
// Returns UID as hex string (e.g. "A1B2C3D4").
// Returns empty string if no card or read error.
String nfc_read_card_uid();

// Check if a UID was read recently (anti-repeat protection).
// Returns true if the given UID was read within NFC_REPEAT_DELAY_MS.
bool nfc_is_duplicate(const String &uid);

// Reset the anti-repeat timer (call after processing a card).
void nfc_reset_repeat_timer();

#endif
