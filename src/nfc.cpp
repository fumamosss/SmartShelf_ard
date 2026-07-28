#include "nfc.h"

// stub - will be replaced with MFRC522 implementation later
void nfc_begin()
{
    Serial.println("[NFC] stub - no real reader initialized");
}

bool nfc_card_available()
{
    // stub - always returns false
    return false;
}

String nfc_read_card_uid()
{
    // stub - returns empty UID
    return "";
}
