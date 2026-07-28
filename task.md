# SmartWardrobe ESP32 - Project Handoff

## Project Overview

SmartWardrobe is an ESP32-based warehouse cabinet system.

The cabinet contains drawers/shelves with double bottoms. Under each storage cell there are 4 load cells connected through HX711 amplifiers. The cabinet also has an NFC/RFID reader.

The ESP32 communicates with a 1C ERP system through 1C HTTP services.

Main workflow:

1. Employee presents an NFC card.
2. ESP32 reads the card UID.
3. ESP32 sends the UID and device MAC address to 1C over HTTP.
4. If the employee is found:

   * play one short high-pitched beep.
5. If the employee is not found:

   * play two short low-pitched beeps.
6. The employee takes or places an item.
7. Load cells detect the weight change and determine the affected storage cell.
8. If the action is valid:

   * play one short medium-pitched beep.
9. If the weight change cannot be reliably detected:

   * play three short fast beeps at the same pitch.
10. If an item is placed in the wrong cell:

* play one long low-pitched beep.

The ESP32 MAC address must be available so that each physical ESP32 device can be registered and associated with a specific device in 1C.

---

# Current Development Goal

At this stage, implement only the basic network foundation.

The current goal is:

1. ESP32 starts.
2. ESP32 checks whether Wi-Fi credentials have already been configured.
3. If credentials are missing, ESP32 starts BLE-based Wi-Fi provisioning.
4. A phone can configure the Wi-Fi network through the ESP BLE Provisioning application.
5. Wi-Fi credentials are stored persistently in ESP32 flash/NVS.
6. On subsequent boots, the device uses the saved credentials.
7. ESP32 connects to Wi-Fi automatically.
8. ESP32 automatically attempts to reconnect if Wi-Fi is disconnected.
9. ESP32 prints useful diagnostic information to Serial Monitor.
10. ESP32 prints its MAC address and IP address after successful connection.

Do not implement the 1C HTTP integration yet.

Do not implement real NFC functionality yet.

Do not implement real load cell functionality yet.

Do not implement real speaker functionality yet.

The project should contain clean stubs/interfaces for NFC and load cells so that real implementations can be added later without restructuring the whole project.

---

# Development Approach

This is a learning project as well as a real prototype.

Keep the implementation simple and understandable.

Do not introduce unnecessary abstractions, RTOS tasks, complex state machines, or complicated architecture unless they are actually needed.

The developer is currently learning Arduino framework and ESP32 development.

When providing code, explain new concepts briefly and clearly.

Do not hide important functionality behind excessive abstractions.

The code should be easy to read and debug from the Serial Monitor.

---

# Platform

* ESP32-WROOM-32
* ESP32 DevKitC V2 / ESP32 Dev Module
* PlatformIO
* Arduino framework
* VS Code

The PlatformIO environment is:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
```

Do not add custom partition configuration unless it is actually required.

---

# Project Structure

Use the following initial structure:

```text
SmartWardrobe/
├── src/
│   ├── SmartWardrobe.ino
│   ├── config.h
│   │
│   ├── wifi_manager.h
│   ├── wifi_manager.cpp
│   │
│   ├── ble_prov.h
│   ├── ble_prov.cpp
│   │
│   ├── nfc.h
│   ├── nfc.cpp
│   │
│   ├── load_cells.h
│   └── load_cells.cpp
│
├── lib/
│
├── platformio.ini
│
└── HANDOFF.md
```

The initial implementation should focus on:

```text
SmartWardrobe.ino
    │
    ├── wifi_manager
    │
    ├── ble_prov
    │
    ├── nfc stub
    │
    └── load_cells stub
```

---

# Wi-Fi

Implement a simple Wi-Fi manager.

Required functionality:

* connect to Wi-Fi;
* check connection status;
* automatically reconnect after disconnection;
* expose whether Wi-Fi is currently connected;
* print connection diagnostics;
* print local IP address;
* print ESP32 MAC address.

The Wi-Fi manager should not contain NFC, HTTP, or load cell logic.

The Wi-Fi manager should have a simple API, for example:

```cpp
bool wifi_manager_begin();
bool wifi_manager_is_connected();
String wifi_manager_get_mac();
String wifi_manager_get_ip();
```

The exact API can be adjusted if there is a better simple design.

---

# BLE Wi-Fi Provisioning

Implement BLE-based Wi-Fi provisioning.

Desired flow:

```text
ESP32 boot
    │
    ▼
check saved Wi-Fi credentials
    │
    ├── credentials exist
    │       │
    │       ▼
    │   connect to Wi-Fi
    │
    └── credentials missing
            │
            ▼
       start BLE provisioning
            │
            ▼
       phone configures Wi-Fi
            │
            ▼
       credentials saved
            │
            ▼
       connect to Wi-Fi
```

Use the Arduino ESP32 provisioning facilities available in the installed Arduino-ESP32 version.

Before implementing, verify which provisioning API is actually available in the current PlatformIO environment.

If `WiFiProv.h` and the required provisioning API are available, use them.

If they are not available or the API differs between versions, explain the difference and implement the correct approach for the installed ESP32 Arduino core instead of inventing APIs.

The provisioning service name should be unique per device, preferably based on the ESP32 MAC address, for example:

```text
PROV_A1B2C3
```

The device should be identifiable from the Serial Monitor during provisioning.

Do not add complex custom BLE services.

The goal is only Wi-Fi provisioning.

---

# Persistent Wi-Fi Credentials

Use ESP32 NVS through the Arduino `Preferences` API if the provisioning implementation does not already persist credentials automatically.

Credentials should survive a reboot.

Do not store credentials in hardcoded `#define` values in the source code.

The expected behavior is:

```text
first boot
    ↓
no saved credentials
    ↓
BLE provisioning
    ↓
Wi-Fi credentials configured
    ↓
credentials persist in flash
    ↓
Wi-Fi connects
```

After reboot:

```text
ESP32 boot
    ↓
saved credentials available
    ↓
connect automatically
```

---

# NFC Stub

Create:

```text
nfc.h
nfc.cpp
```

For now, do not implement the actual MFRC522 reader.

Create a simple interface that can later be replaced with a real implementation.

For example:

```cpp
void nfc_begin();
bool nfc_card_available();
String nfc_read_card_uid();
```

The stub can simply return `false` from `nfc_card_available()`.

The main program should be able to call the NFC functions without having a real NFC reader connected.

Later, the real MFRC522 implementation will replace the stub.

---

# Load Cell Stub

Create:

```text
load_cells.h
load_cells.cpp
```

For now, do not implement real HX711 reading.

The project will eventually have multiple storage cells.

Each storage cell will have 4 load cells connected to HX711 amplifiers.

Important:

HX711 does NOT use SPI. It uses its own two-wire interface with DT and SCK pins.

Create a simple placeholder interface that can later be expanded.

For example:

```cpp
void load_cells_begin();
bool load_cells_has_weight_change();
int load_cells_get_changed_cell();
float load_cells_get_weight(int cell);
```

The stub may return default values for now.

Do not implement the weight detection algorithm yet.

Do not implement calibration yet.

Do not implement filtering yet.

---

# Future Architecture

The expected future architecture is:

```text
ESP32
│
├── wifi_manager
│     └── Wi-Fi connection
│
├── ble_prov
│     └── first-time Wi-Fi configuration
│
├── http_client
│     └── communication with 1C
│
├── nfc
│     └── employee card detection
│
├── load_cells
│     └── weight measurement
│
└── speaker
      └── audio feedback
```

Main business flow:

```text
NFC card
    │
    ▼
read UID
    │
    ▼
HTTP request to 1C
    │
    ├── employee found
    │       └── one short high beep
    │
    └── employee not found
            └── two short low beeps
                    │
                    ▼
              employee action
                    │
                    ▼
              load cell analysis
                    │
                    ├── valid action
                    │      └── one short medium beep
                    │
                    ├── weight not recognized
                    │      └── three short fast beeps
                    │
                    └── wrong storage cell
                           └── one long low beep
```

---

# Hardware

Hardware pinout is not defined yet.

Do not invent final GPIO assignments.

Current planned hardware:

* ESP32-WROOM-32
* NFC/RFID reader, likely MFRC522
* multiple HX711 modules
* 4 load cells per storage cell
* speaker/buzzer

The exact GPIO mapping will be defined after the physical wiring is decided.

---

# Important Constraints

For the current implementation:

* focus only on Wi-Fi;
* focus only on BLE Wi-Fi provisioning;
* include persistent Wi-Fi configuration;
* include automatic Wi-Fi reconnect;
* include MAC address output;
* include IP address output;
* include NFC stub;
* include HX711/load cell stub;
* keep the project compilable and runnable;
* keep the code simple.

Do not implement yet:

* 1C HTTP integration;
* real NFC reading;
* real HX711 reading;
* weight calibration;
* weight filtering;
* item identification;
* speaker sound patterns;
* OTA;
* complex task architecture.

The immediate goal is to get a clean working foundation that can be flashed to the ESP32 and tested through Serial Monitor.

---

# Comments Style

Comments should:

* start with lowercase;
* be brief;
* explain WHY rather than WHAT;
* use regular hyphens (-), never em dashes (—);
* avoid unnecessary comments for obvious API calls.

---

# First Task

Create the complete initial PlatformIO Arduino project according to this HANDOFF.

First implement:

1. `platformio.ini`
2. `SmartWardrobe.ino`
3. `config.h`
4. `wifi_manager.h/.cpp`
5. `ble_prov.h/.cpp`
6. `nfc.h/.cpp` stub
7. `load_cells.h/.cpp` stub

Then explain:

1. how to build the project;
2. how to upload it to ESP32;
3. how to open Serial Monitor;
4. how BLE provisioning works;
5. how to connect the ESP32 to Wi-Fi for the first time;
6. how to verify the MAC address;
7. how to verify the IP address;
8. what should happen after reboot.

Do not move on to HTTP, NFC, HX711, or 1C until the Wi-Fi provisioning and reconnection flow is confirmed to work.
