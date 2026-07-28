# SmartShelf — Hardware Setup Guide

## Quick Start

1. Clone repo: `git clone https://github.com/fumamosss/SmartShelf_ard.git`
2. Install PlatformIO (VS Code extension or CLI)
3. ESP32 USB drivers: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
4. Connect ESP32, run `pio run -t upload`
5. Open Serial Monitor at 115200 baud

Libraries are auto-downloaded on first build. No manual install needed.

---

## 1. GPIO Configuration

All pins are in `src/config.h`. Change only what your wiring requires.

### PN532 NFC Reader (I2C)

| Signal | Default Pin | Notes |
|--------|------------|-------|
| SDA | GPIO21 | Standard I2C SDA on ESP32 |
| SCL | GPIO22 | Standard I2C SCL on ESP32 |

Change in `config.h`:
```c
#define NFC_SDA_PIN 21
#define NFC_SCL_PIN 22
```

PN532 module wiring: VCC→3.3V, GND→GND, SDA→GPIO21, SCL→GPIO22.
Set PN532 jumpers to I2C mode (both jumpers ON for most boards).

### 8×HX711 Load Cells

Each HX711 has its own DT pin. SCK is shared (all same GPIO).

| Sensor | Shelf | DT Pin | SCK Pin |
|--------|-------|--------|---------|
| sensor_0 | shelf_0 | GPIO4 | GPIO26 |
| sensor_1 | shelf_0 | GPIO5 | GPIO26 |
| sensor_2 | shelf_0 | GPIO16 | GPIO26 |
| sensor_3 | shelf_0 | GPIO17 | GPIO26 |
| sensor_4 | shelf_1 | GPIO18 | GPIO26 |
| sensor_5 | shelf_1 | GPIO19 | GPIO26 |
| sensor_6 | shelf_1 | GPIO23 | GPIO26 |
| sensor_7 | shelf_1 | GPIO25 | GPIO26 |

Change in `config.h`:
```c
#define HX711_DT_PINS  { 4, 5, 16, 17, 18, 19, 23, 25 }
#define HX711_SCK_PINS { 26, 26, 26, 26, 26, 26, 26, 26 }
```

HX711 module wiring: VCC→3.3V (or 5V), GND→GND, DT→GPIOxx, SCK→GPIOxx.

### Total GPIOs used: 9 (8 DT + 1 SCK) + 2 (I2C) = 11 pins

---

## 2. 1C Server Configuration

In `src/config.h`:
```c
#define API_BASE_URL "http://192.168.1.100:8080"
#define API_NFC_CHECK_ENDPOINT "/api/shelf/nfc"
#define API_OPERATION_ENDPOINT "/api/shelf/operation"
```

Replace `192.168.1.100:8080` with your computer's IP and port running the 1C mock server or 1C extension.

---

## 3. JSON API Reference

### Request 1: NFC Check

```
GET http://YOUR_IP:PORT/api/shelf/nfc?uid=A1B2C3D4
```

Expected 1C response:
```json
{
    "success": true,
    "allowed": true,
    "user": "Иван Иванов"
}
```

Or access denied:
```json
{
    "success": true,
    "allowed": false
}
```

### Request 2: Submit Operation

```
POST http://YOUR_IP:PORT/api/shelf/operation
Content-Type: application/json
```

Body:
```json
{
    "nfc_uid": "A1B2C3D4",
    "baseline": {
        "sensor_0": 123456,
        "sensor_1": 123789,
        "sensor_2": 124000,
        "sensor_3": 123500,
        "sensor_4": 223456,
        "sensor_5": 223789,
        "sensor_6": 224000,
        "sensor_7": 223500
    },
    "final": {
        "sensor_0": 123100,
        "sensor_1": 123450,
        "sensor_2": 123650,
        "sensor_3": 123100,
        "sensor_4": 223456,
        "sensor_5": 223789,
        "sensor_6": 224000,
        "sensor_7": 223500
    },
    "delta": {
        "sensor_0": -356,
        "sensor_1": -339,
        "sensor_2": -350,
        "sensor_3": -400,
        "sensor_4": 0,
        "sensor_5": 0,
        "sensor_6": 0,
        "sensor_7": 0
    }
}
```

Expected 1C response:
```json
{
    "success": true,
    "accepted": true,
    "message": "Operation recorded"
}
```

---

## 4. Calibration

After connecting real HX711 hardware, update in `config.h`:

```c
#define HX711_CALIBRATION_FACTORS { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 }
```

To calibrate:
1. Place a known weight on each sensor
2. Note the raw value in Serial Monitor (printed during `load_cells_begin()`)
3. Place nothing, note zero value
4. Calibration factor = (value_with_known_weight - zero) / known_weight_grams
5. Set factors in config.h

Weight algorithm thresholds:
```c
#define WEIGHT_CHANGE_THRESHOLD 500    // min raw value change to detect
#define WEIGHT_STABLE_TOLERANCE 200    // max variation during stability check
#define WEIGHT_STABLE_TIME_MS 2000     // how long weight must be stable
#define WEIGHT_OPERATION_TIMEOUT_MS 30000  // max time for entire operation
```

---

## 5. Testing Without Hardware

### Test PN532 alone
1. Wire PN532 to ESP32 (SDA/SCL/VCC/GND)
2. Flash firmware
3. Open Serial Monitor (115200)
4. You should see `[NFC] PN532 found, firmware: 0x...`
5. Tap an NFC card → should print `[SM] card detected: XXXXXXXX`
6. If you see `[NFC] ERROR: PN532 not found` → check wiring and I2C jumpers

### Test each HX711 alone
1. Wire one HX711 to ESP32
2. Flash firmware
3. Serial Monitor shows `[LOAD] sensor X: OK (DT=..., SCK=..., raw=...)`
4. If you see `NOT READY` → check wiring
5. Apply pressure to load cell → raw value should change in Serial Monitor

### Test API without 1C (mock server)
Run this Python mock on your computer:

```python
from http.server import HTTPServer, BaseHTTPRequestHandler
import json

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if '/api/shelf/nfc' in self.path:
            uid = self.path.split('uid=')[1] if 'uid=' in self.path else '???'
            print(f"NFC check: uid={uid}")
            resp = {"success": True, "allowed": True, "user": "Test User"}
        else:
            resp = {"error": "not found"}
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(resp).encode())

    def do_POST(self):
        length = int(self.headers['Content-Length'])
        body = self.rfile.read(length)
        data = json.loads(body)
        print(f"Operation: uid={data.get('nfc_uid')}")
        print(f"  delta: {json.dumps(data.get('delta'))}")
        resp = {"success": True, "accepted": True, "message": "Test OK"}
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(resp).encode())

    def log_message(self, format, *args):
        print(format % args)

print("Mock 1C server running on http://0.0.0.0:8080")
HTTPServer(('0.0.0.0', 8080), Handler).serve_forever()
```

Set `API_BASE_URL` in config.h to `http://YOUR_COMPUTER_IP:8080`, then flash.

---

## 6. Workflow After First Boot

1. **No saved credentials** → BLE provisioning starts → connect via ESP BLE Provisioning app → enter WiFi SSID/password
2. **Credentials saved** → WiFi connects automatically
3. **Ready** → `[SM] shelf state machine ready — waiting for NFC card`
4. **Tap card** → UID sent to 1C → if allowed, baseline captured
5. **Take/put item** → weight changes detected → stabilized → sent to 1C
6. **Reboot** → auto-connects to saved WiFi → ready again

---

## 7. Files Changed

| File | Purpose |
|------|---------|
| `src/config.h` | ALL hardware pins, API endpoints, algorithm params |
| `src/nfc.h/.cpp` | PN532 I2C reader with anti-repeat |
| `src/load_cells.h/.cpp` | 8×HX711 reader with shared SCK |
| `src/api_client.h/.cpp` | 1C API (NFC check + operation submit) |
| `src/weight_monitor.h/.cpp` | Baseline capture, change detection, stability |
| `src/shelf_sm.h/.cpp` | State machine: IDLE→AUTH→BASELINE→WEIGHT→SEND→DONE |
| `src/net_client.h/.cpp` | HTTP GET/POST (unchanged) |
| `src/SmartShelf.ino` | Main — setup/loop, delegates to modules |
| `platformio.ini` | Added Adafruit PN532, HX711, ArduinoJson deps |
