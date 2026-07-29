#include "nfc.h"
#include <Wire.h>
#include <Adafruit_PN532.h>
#include "config.h"

// PN532 over I2C — SDA and SCL pins from config.h
static Adafruit_PN532 nfc(NFC_SDA_PIN, NFC_SCL_PIN);

static bool nfc_initialized = false;
static String last_uid = "";
static unsigned long last_read_time = 0;

bool nfc_begin() {
    Serial.println("[NFC] initializing PN532 over I2C...");

    nfc.begin();

    uint32_t version = nfc.getFirmwareVersion();
    if (!version) {
        Serial.println("[NFC] ERROR: PN532 not found — check SDA/SCL wiring");
        Serial.printf("[NFC]   SDA=GPIO%d, SCL=GPIO%d\n", NFC_SDA_PIN, NFC_SCL_PIN);
        nfc_initialized = false;
        return false;
    }

    Serial.printf("[NFC] PN532 found, firmware: 0x%08X\n", version);

    // Configure board to read RFID tags
    nfc.SAMConfig();

    nfc_initialized = true;
    Serial.println("[NFC] PN532 ready");
    return true;
}

String nfc_read_card_uid() {
    if (!nfc_initialized) return "";

    uint8_t uid[7];
    uint8_t uid_len;

    bool success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uid_len, 100);
    if (!success) return "";

    // Convert UID bytes to hex string
    String uid_str = "";
    for (uint8_t i = 0; i < uid_len; i++) {
        if (uid[i] < 0x10) uid_str += "0";
        uid_str += String(uid[i], HEX);
    }
    uid_str.toUpperCase();

    return uid_str;
}

void nfc_accept_uid(const String &uid) {
    last_uid = uid;
    last_read_time = millis();
}

bool nfc_is_duplicate(const String &uid) {
    if (last_uid.length() == 0) return false;
    if (uid != last_uid) return false;
    if (millis() - last_read_time < NFC_REPEAT_DELAY_MS) return true;
    return false;
}

void nfc_reset_repeat_timer() {
    last_uid = "";
    last_read_time = 0;
}
