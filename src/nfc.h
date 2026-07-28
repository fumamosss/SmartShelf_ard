#ifndef NFC_H
#define NFC_H

#include <Arduino.h>

// initialize NFC module - stub for now
void nfc_begin();

// check if an NFC card is present - returns false in stub
bool nfc_card_available();

// read card UID - returns empty string in stub
String nfc_read_card_uid();

#endif
