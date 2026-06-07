# CNC IoT Monitoring System

This repository contains a local IoT-based CNC monitoring system prototype.

The project is designed to collect CNC machine telemetry from ESP-12E edge nodes, transfer the data through an ESP32 gateway, and display the received data on a local web dashboard running on a macOS central computer.

Because a real CNC machine is not available during development, early software validation uses Kaggle CNC dataset files and simulated CNC telemetry. The current hardware prototype also includes ESP-12E and ESP32 firmware for testing a real local IoT data path.

---

## Project Goal

The main goal of this project is to build a local CNC monitoring architecture that can:

- collect CNC-related telemetry from edge devices,
- transfer machine data over a local wireless network,
- use lightweight IoT communication protocols,
- route edge-node data through a gateway device,
- store received messages on a central computer,
- show CNC status and telemetry on a local web dashboard,
- distinguish between machine status, gateway status, and offline devices.

---

## Current Prototype Architecture

The current prototype uses ESP-12E edge nodes, an ESP32 gateway, MQTT, and a macOS dashboard.

```text
ESP-12E edge nodes
        ↓ ESP-NOW
ESP32 gateway
        ↓ MQTT
macOS Mosquitto broker
        ↓
Python dashboard server + SQLite database
        ↓
HTTP / WebSocket local dashboard
```

### Layered Communication Structure

| Layer | Technology | Purpose |
|------|------------|---------|
| Local network connection | Wi-Fi / hotspot | Connects macOS and ESP32 on the same local network |
| Edge-node to gateway communication | ESP-NOW | Sends compact CNC packets from ESP-12E nodes to ESP32 |
| Gateway to central computer communication | MQTT | Transfers CNC telemetry and gateway status to macOS |
| Dashboard access | HTTP | Serves the web dashboard to users |
| Live dashboard updates | WebSocket | Updates dashboard data in real time |
| Data format | JSON | Standard message format after ESP32 gateway conversion |
| Local storage | SQLite | Stores received CNC messages |

---

## Architecture Diagrams

Project diagrams are stored under:

```text
docs/diagrams/
```

Recommended files:

- [`docs/diagrams/system_architecture.md`](docs/diagrams/system_architecture.md)
- [`docs/diagrams/data_flow.md`](docs/diagrams/data_flow.md)
- [`docs/diagrams/esp12e_button_state_machine.md`](docs/diagrams/esp12e_button_state_machine.md)
- [`docs/diagrams/esp12e_button_node_wiring.md`](docs/diagrams/esp12e_button_node_wiring.md)
- [`docs/diagrams/esp32_gateway_power_wiring.md`](docs/diagrams/esp32_gateway_power_wiring.md)

---

## Current Prototype Status

Completed and tested:

- macOS CSV-to-MQTT simulator,
- Mosquitto MQTT broker,
- MQTT message inspection using `mosquitto_sub`,
- local dashboard access from macOS,
- dashboard access from a mobile device on the same Wi-Fi network,
- SQLite database recording,
- ESP32 MQTT gateway publishing test,
- ESP32 gateway status monitoring on the dashboard,
- dashboard offline timeout for CNC devices,
- shared local `secrets.h` configuration approach,
- ESP-NOW packet structure shared between ESP32 and ESP-12E firmware,
- ESP32 final gateway firmware that receives ESP-NOW packets and forwards them through MQTT,
- ESP-12E auto sender firmware,
- ESP-12E button state node firmware for multiple CNC states.

Current active development:

- stabilizing the ESP-12E button state node,
- improving hardware diagrams and documentation,
- preparing the full ESP-12E → ESP32 → MQTT → dashboard demonstration.

---

## Firmware Roles

### ESP32 Gateway

The ESP32 gateway is the central hardware bridge between ESP-12E edge nodes and the macOS MQTT system.

It is responsible for:

- connecting to the local Wi-Fi / hotspot network,
- connecting to the macOS Mosquitto MQTT broker,
- receiving ESP-NOW packets from ESP-12E edge nodes,
- assigning automatic machine names such as `CNC-01`, `CNC-02`, etc. when an ESP-12E does not provide a label,
- converting compact ESP-NOW packets into dashboard-compatible JSON messages,
- publishing CNC telemetry to MQTT,
- publishing gateway status to MQTT.

The ESP32 gateway does not generate CNC telemetry in the final gateway firmware. It only forwards and converts data received from ESP-12E edge nodes.

### ESP-12E Auto Sender

The ESP-12E auto sender is a simple edge-node firmware.

It is designed to:

- start automatically when powered,
- connect to the configured Wi-Fi / hotspot network,
- send compact CNC-like telemetry packets to the ESP32 gateway using ESP-NOW,
- require no button or LED to operate.

This version is useful for simple demonstrations where the device should start sending data immediately after power is applied.

### ESP-12E Button State Node

The ESP-12E button state node adds local control and visual state indication.

It uses:

- one push button,
- one WS2812 RGB LED,
- ESP-NOW communication to the ESP32 gateway.

The planned state model is:

| State | Meaning | ESP-12E Behavior | LED Color |
|------|---------|------------------|-----------|
| `LIVE_IDLE` | Gateway communication is active, but CNC is not producing data | Sends heartbeat / stopped packets | Yellow |
| `RUNNING` | CNC telemetry is being produced | Sends running telemetry packets | Purple |
| `OFFLINE_STANDBY` | Device stops publishing data | Sends no packets, dashboard marks it offline after timeout | White |
| `TRANSITION_ERROR` | A state transition or network operation failed | Error indication | Red |

Alarm/error packets during `RUNNING` are indicated by a short three-blink LED pattern.

---

## MQTT Topic Structure

The current topic structure includes both CNC telemetry and gateway status.

```text
factory/cnc/<machine_id>/telemetry
factory/gateway/ESP32-GW-01/status
```

Examples:

```text
factory/cnc/CNC-01/telemetry
factory/cnc/CNC-02/telemetry
factory/gateway/ESP32-GW-01/status
```

The ESP32 gateway can automatically assign machine names when the ESP-12E node does not provide a label.

Example automatic naming:

```text
First unknown ESP-12E  → CNC-01
Second unknown ESP-12E → CNC-02
Third unknown ESP-12E  → CNC-03
```

---

## Transmitted Data Types

The system is designed to transmit CNC-related telemetry data such as:

| Data Group | Example Fields |
|-----------|----------------|
| Identity data | `machine_id`, `device_id`, `gateway_id`, `run_id`, `tool_id` |
| Process parameters | `ap`, `vc`, `f`, `machined_length` |
| Force/load data | `Fx`, `Fy`, `Fz`, `F` |
| Quality data | `Ra`, `Rz`, `Rt`, `Rsk`, `Rku`, `RSm` |
| Status data | `machine_status`, `quality_status`, `load_status`, `alarm_code` |
| Time / sequence data | `timestamp`, `sequence_no` |

During simulation, timestamp values may represent device uptime such as `ESP32_MILLIS_6192`. In a real CNC integration, the timestamp field can be filled by the CNC device, the data acquisition interface, or the central backend depending on the final architecture.

---

## Example MQTT Message

```json
{
  "device_id": "EDGE-01",
  "gateway_id": "ESP32-GW-01",
  "machine_id": "CNC-01",
  "edge_mac": "AA:BB:CC:DD:EE:FF",
  "source_file": "ESP12E_ESPNOW",
  "sequence_no": 42,
  "timestamp": "ESP32_MILLIS_6192",
  "run_id": "ESP12E_EDGE_PACKET",
  "tool_id": 22,
  "process": {
    "depth_of_cut_ap": 0.25,
    "cutting_speed_vc": 240.0,
    "feed_rate_f": 0.075,
    "machined_length": 42
  },
  "force": {
    "fx": 49.0,
    "fy": 44.0,
    "fz": 21.0,
    "total_force": 114.0
  },
  "quality": {
    "ra": 0.48,
    "rz": 1.75,
    "rt": 2.05
  },
  "status": {
    "machine_status": "RUNNING",
    "load_status": "NORMAL_LOAD",
    "quality_status": "GOOD",
    "alarm_code": null
  }
}
```

---

## Data Source

The early software MVP uses Kaggle CNC dataset files as a simulated data source.

The raw dataset files should be stored locally under:

```text
data/raw/
```

Example local files:

```text
data/raw/Exp1.csv
data/raw/Exp2.csv
data/raw/Prep.csv
```

Large raw datasets are intentionally excluded from Git using `.gitignore`.

Only small sample files should be placed under:

```text
data/sample/
```

---

## Main Technologies

| Technology | Purpose |
|-----------|---------|
| macOS | Central development and runtime environment |
| Python | Simulator, backend, and dashboard server |
| MQTT | IoT telemetry transfer between gateway and macOS |
| Mosquitto | Local MQTT broker |
| SQLite | Local database storage |
| HTTP / WebSocket | Web dashboard and live updates |
| ESP-NOW | ESP-12E to ESP32 local wireless packet transfer |
| ESP-12E / ESP8266 | CNC edge node hardware |
| ESP32 | Gateway hardware |
| Arduino IDE | Firmware development |

---

## Repository Structure

```text
cnc-iot-monitoring-system/
├── README.md
├── LICENSE
├── .gitignore
├── docs/
│   ├── architecture/
│   ├── diagrams/
│   └── notes/
├── data/
│   ├── sample/
│   └── raw/
├── firmware/
│   ├── common/
│   │   ├── cnc_espnow_packet.h
│   │   └── secrets.example.h
│   ├── esp12e_edge_node/
│   │   ├── esp12e_auto_sender/
│   │   └── esp12e_button_state_node/
│   └── esp32_gateway/
│       ├── esp32_gateway_mqtt_test/
│       └── esp32_gateway_final/
├── macos_backend/
│   ├── simulator.py
│   ├── dashboard_server.py
│   ├── mosquitto_local.conf
│   ├── requirements.txt
│   └── static/
└── database/
```

### Directory Purpose

| Directory | Purpose |
|----------|---------|
| `docs/architecture/` | Architecture notes and design decisions |
| `docs/diagrams/` | System diagrams and wiring diagrams |
| `docs/notes/` | Development notes and known limitations |
| `data/sample/` | Small sample datasets that can be committed |
| `data/raw/` | Local raw datasets, ignored by Git |
| `firmware/common/` | Shared packet structure and example configuration |
| `firmware/esp12e_edge_node/` | ESP-12E edge node firmware |
| `firmware/esp32_gateway/` | ESP32 gateway firmware |
| `macos_backend/` | Python simulator, backend, and dashboard server |
| `database/` | Local database files, ignored by Git |

---

## macOS Backend Setup

### 1. Install Mosquitto

```bash
brew install mosquitto
```

Start the broker with the local configuration file so external devices on the same Wi-Fi / hotspot network can connect:

```bash
mosquitto -c macos_backend/mosquitto_local.conf -v
```

The local development config uses:

```text
listener 1883 0.0.0.0
allow_anonymous true
```

This is intended only for a trusted local demo network.

### 2. Create Python Virtual Environment

From the project root:

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install --upgrade pip
python3 -m pip install -r macos_backend/requirements.txt
```

### 3. Run Dashboard Server

```bash
python3 macos_backend/dashboard_server.py
```

Open the dashboard locally:

```text
http://127.0.0.1:8000
```

For access from another device on the same Wi-Fi network, use the macOS local IP address:

```bash
ipconfig getifaddr en0
```

Example:

```text
http://192.168.43.21:8000
```

### 4. Verify MQTT Messages

To inspect all gateway and CNC messages:

```bash
mosquitto_sub -h 127.0.0.1 -t "factory/#" -v
```

To inspect only CNC telemetry:

```bash
mosquitto_sub -h 127.0.0.1 -t "factory/cnc/+/telemetry" -v
```

To inspect only gateway status:

```bash
mosquitto_sub -h 127.0.0.1 -t "factory/gateway/#" -v
```

### 5. Optional CSV Simulator

The Python CSV simulator is still useful for backend-only testing:

```bash
python3 macos_backend/simulator.py --data-dir data/raw --interval 1 --loop
```

---

## Firmware Setup

### ESP32 Gateway

Recommended Arduino IDE board selection for the 30-pin ESP32 ESP-32S development board:

```text
Board: ESP32 Dev Module
Upload Speed: 115200
Flash Mode: DIO
Flash Size: 4MB
Partition Scheme: Default 4MB with spiffs
Serial Monitor: 115200 baud
```

Required library:

```text
PubSubClient by Nick O'Leary
```

`WiFi.h` is included with the ESP32 board package.

### ESP-12E / NodeMCU

Recommended Arduino IDE board selection:

```text
Board: NodeMCU 1.0 (ESP-12E Module)
Upload Speed: 115200
Serial Monitor: 115200 baud
```

For the button state node, the WS2812 LED requires:

```text
Adafruit NeoPixel by Adafruit
```

---

## Local Firmware Secrets

Wi-Fi SSID, Wi-Fi password, MQTT broker IP address, MQTT port, API keys, tokens, and other local configuration values must not be committed to GitHub.

Firmware sketches use a local configuration file:

```text
firmware/common/secrets.h
```

This file is ignored by Git and must remain only on the local development machine.

### Creating the Local Secrets File

Copy the example file:

```bash
cp firmware/common/secrets.example.h firmware/common/secrets.h
```

Then edit the local file:

```bash
nano firmware/common/secrets.h
```

Example structure:

```cpp
#pragma once

#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define MQTT_BROKER_IP "YOUR_MACBOOK_IP"
#define MQTT_BROKER_PORT 1883
```

Only the following example file should be committed:

```text
firmware/common/secrets.example.h
```

The real local file must not be committed:

```text
firmware/common/secrets.h
```

### Shared Use Across Firmware

The same local `secrets.h` file can be shared by all firmware sketches.

Sketch folders can include a local symbolic link named `secrets.h`, for example:

```text
firmware/esp32_gateway/esp32_gateway_final/secrets.h
firmware/esp12e_edge_node/esp12e_auto_sender/secrets.h
firmware/esp12e_edge_node/esp12e_button_state_node/secrets.h
```

These local links or copies must remain ignored by Git.

### Safety Check Before Commit

Before committing firmware changes, verify that the real secrets file is ignored:

```bash
git check-ignore -v firmware/common/secrets.h
git check-ignore -v firmware/esp32_gateway/esp32_gateway_final/secrets.h
git check-ignore -v firmware/esp12e_edge_node/esp12e_auto_sender/secrets.h
git check-ignore -v firmware/esp12e_edge_node/esp12e_button_state_node/secrets.h
```

Also inspect what Git would add before staging files:

```bash
git add -n .
```

The following files must not appear in the add list:

```text
firmware/common/secrets.h
firmware/esp32_gateway/esp32_gateway_final/secrets.h
firmware/esp12e_edge_node/esp12e_auto_sender/secrets.h
firmware/esp12e_edge_node/esp12e_button_state_node/secrets.h
data/raw/Exp1.csv
data/raw/Exp2.csv
data/raw/Prep.csv
*.db
```

---

## Hardware Wiring Notes

### ESP-12E Button State Node

| Component | NodeMCU Pin | GPIO | Notes |
|----------|-------------|------|-------|
| Push button | D5 | GPIO14 | Connected between D5 and GND, uses `INPUT_PULLUP` |
| WS2812 data | D2 | GPIO4 | D2 → 220Ω/330Ω resistor → WS2812 DIN |
| WS2812 VCC | 3V | - | Single LED, low brightness |
| WS2812 GND | GND | - | Common ground |

### ESP32 Gateway Power

For standalone operation:

```text
Power supply +5V → ESP32 5V / VIN pin
Power supply GND → verified ESP32 GND pin
```

Do not power the ESP32 from the `3V3` pin during this project.

During debugging and firmware upload, using USB power only is recommended.

---

## Git Data Policy

The following files should not be committed:

- raw Kaggle CSV files,
- SQLite database files,
- virtual environments,
- private keys,
- tokens,
- Wi-Fi passwords,
- real local `secrets.h` files,
- local backup files.

The `.gitignore` file is configured to exclude these categories.

Small sample CSV files can be committed only under:

```text
data/sample/
```

---

## Related Test Repository

The first hardware validation test is stored separately:

```text
esp12e-cnc-node-test
```

That repository contains:

- ESP-12E / NodeMCU test firmware,
- push button input test,
- WS2812 RGB LED output test,
- wiring table,
- wiring diagram,
- Arduino IDE setup instructions.

---

## Version Plan

| Version | Scope |
|--------|-------|
| `v0.1` | macOS CSV-to-MQTT dashboard MVP |
| `v0.2` | ESP32 MQTT gateway test and dashboard offline timeout |
| `v0.3` | ESP-NOW edge prototype with ESP32 gateway and ESP-12E nodes |
| `v0.4` | Stabilized ESP-12E button state node and documentation |
| `v0.5` | Full local IoT data pipeline demonstration |
| `v1.0` | Presentation-ready project |

---

## Known Limitations

Current limitations are documented in:

```text
docs/notes/current_limitations.md
```

Main current limitations:

- The ESP-12E button state node still needs stabilization.
- ESP32 automatic machine naming is stored in memory and resets after ESP32 restart.
- CNC telemetry is simulated; real CNC integration is not yet implemented.
- Timestamp values currently may represent device uptime instead of real CNC timestamps.
- The local Mosquitto configuration allows anonymous access and should only be used in a trusted demo network.

---

## License

This project is released under the MIT License.
