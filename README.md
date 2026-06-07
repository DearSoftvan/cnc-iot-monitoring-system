# CNC IoT Monitoring System

This repository contains an IoT-based CNC monitoring system prototype.

The project is designed to collect CNC machine telemetry data from edge devices, transfer the data through a gateway, and display the data on a local web dashboard running on a macOS central computer.

The current development phase uses Kaggle CNC datasets to simulate CNC machine telemetry because a real CNC machine is not available for testing.

---

## Project Goal

The main goal of this project is to build a local IoT monitoring system for CNC machines.

The system focuses on:

- collecting CNC-related telemetry data,
- transmitting machine data over a local network,
- using MQTT for lightweight IoT data transfer,
- storing received data on a central computer,
- displaying real-time CNC data on a local web dashboard,
- preparing the system for later ESP-12E and ESP32 hardware integration.

---

## Planned Architecture

The final planned architecture is:

```text
ESP-12E edge nodes
        ↓
ESP32 gateway
        ↓
MQTT
        ↓
macOS central computer
        ↓
SQLite database + HTTP/WebSocket dashboard
```

### Layered Communication Structure

| Layer | Technology | Purpose |
|------|------------|---------|
| Local network connection | Wi-Fi | Connects devices on the same local network |
| IoT data transfer | MQTT | Transfers CNC telemetry messages |
| Dashboard access | HTTP | Serves the web dashboard to users |
| Live dashboard updates | WebSocket | Updates dashboard data in real time |
| Data format | JSON | Standard message format for machine data |
| Local storage | SQLite | Stores received CNC messages |

---

## Current MVP

The current working MVP uses local CSV files as the CNC data source.

```text
Kaggle CNC CSV dataset
        ↓
Python simulator
        ↓
MQTT
        ↓
macOS backend
        ↓
SQLite database
        ↓
Local web dashboard
```

The MVP has already been tested successfully for:

- CSV-based CNC data simulation,
- MQTT publishing,
- MQTT subscription using `mosquitto_sub`,
- local dashboard access from macOS,
- dashboard access from a mobile phone on the same Wi-Fi network,
- SQLite database recording.

---

## Data Source

The current prototype uses Kaggle CNC dataset files.

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

## Transmitted Data Types

The system is designed to transmit CNC-related telemetry data such as:

| Data Group | Example Fields |
|-----------|----------------|
| Identity data | `machine_id`, `device_id`, `run_id`, `tool_id` |
| Process parameters | `ap`, `vc`, `f`, `machined_length` |
| Force/load data | `Fx`, `Fy`, `Fz`, `F` |
| Quality data | `Ra`, `Rz`, `Rt`, `Rsk`, `Rku`, `RSm` |
| Status data | `machine_status`, `quality_status`, `load_status`, `alarm_code` |
| Time data | `timestamp`, `sequence_no` |

---

## Example MQTT Message

```json
{
  "device_id": "EDGE-01",
  "gateway_id": "ESP32-GATEWAY-01",
  "machine_id": "CNC-01",
  "source_file": "Exp1.csv",
  "sequence_no": 42,
  "timestamp": "2026-05-27T15:30:10",
  "process": {
    "depth_of_cut_ap": 0.25,
    "cutting_speed_vc": 350,
    "feed_rate_f": 0.07,
    "machined_length": 12
  },
  "force": {
    "fx": 49.23,
    "fy": 44.46,
    "fz": 21.07,
    "total_force": 69.60
  },
  "quality": {
    "ra": 0.391,
    "rz": 1.855,
    "rt": 2.082
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

## MQTT Topic Structure

The initial topic structure is:

```text
factory/cnc/CNC-01/telemetry
factory/cnc/CNC-02/telemetry
factory/cnc/PREP-STATION/telemetry
```

Future topic extensions may include:

```text
factory/cnc/CNC-01/status
factory/cnc/CNC-01/quality
factory/cnc/CNC-01/alarm
```

---

## Main Technologies

| Technology | Purpose |
|-----------|---------|
| macOS | Central development and runtime environment |
| Python | Simulator, backend, and data handling |
| MQTT | IoT telemetry transfer |
| Mosquitto | Local MQTT broker |
| SQLite | Local database storage |
| HTTP / WebSocket | Web dashboard and live updates |
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
│   ├── esp12e_edge_node/
│   └── esp32_gateway/
├── macos_backend/
│   ├── simulator.py
│   ├── dashboard_server.py
│   ├── requirements.txt
│   └── static/
└── database/
```

### Directory Purpose

| Directory | Purpose |
|----------|---------|
| `docs/architecture/` | Architecture notes and design decisions |
| `docs/diagrams/` | System diagrams and wiring diagrams |
| `docs/notes/` | Development notes |
| `data/sample/` | Small sample datasets that can be committed |
| `data/raw/` | Local raw datasets, ignored by Git |
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

Start the broker manually during development:

```bash
mosquitto -v
```

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
http://10.243.103.102:8000
```

### 4. Run CNC Data Simulator

```bash
python3 macos_backend/simulator.py --data-dir data/raw --interval 1 --loop
```

### 5. Verify MQTT Messages

```bash
mosquitto_sub -h 127.0.0.1 -t "factory/cnc/+/telemetry" -v
```

If messages are shown in the terminal, MQTT publishing and subscription are working correctly.

---

## Git Data Policy

The following files should not be committed:

- raw Kaggle CSV files,
- SQLite database files,
- virtual environments,
- private keys,
- tokens,
- password or secret files.

The `.gitignore` file is configured to exclude:

```text
.DS_Store
.venv/
*.db
*.sqlite
*.csv
data/raw/
.env
*.pem
*.key
id_rsa*
id_ed25519*
```

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

## Current Project Status

Completed:

- macOS CSV-to-MQTT simulator tested,
- MQTT message flow verified with `mosquitto_sub`,
- local web dashboard tested,
- web dashboard tested from a mobile phone on the same Wi-Fi network,
- SQLite database recording verified,
- ESP-12E button and WS2812 LED hardware test completed in a separate repository.

Next steps:

1. Add ESP32 gateway MQTT publishing test.
2. Add ESP-12E edge node firmware.
3. Connect ESP-12E edge nodes to ESP32 gateway.
4. Integrate real hardware data flow with the macOS dashboard.
5. Prepare presentation-ready architecture diagrams and screenshots.

---

## Version Plan

| Version | Scope |
|--------|-------|
| `v0.1` | macOS CSV-to-MQTT dashboard MVP |
| `v0.2` | ESP32 MQTT gateway test |
| `v0.3` | ESP-12E edge node firmware |
| `v0.4` | ESP-12E to ESP32 communication |
| `v0.5` | Full local IoT data pipeline |
| `v1.0` | Presentation-ready project |

---

## License

This project is released under the MIT License.

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

The same local `secrets.h` file can be shared by all firmware sketches, including:

```text
firmware/esp32_gateway/
firmware/esp12e_edge_node/
```

For example, an ESP32 or ESP-12E sketch can access the shared local configuration through a local `secrets.h` reference.

This keeps common network configuration in one place:

- Wi-Fi SSID
- Wi-Fi password
- MQTT broker IP address
- MQTT broker port

Device-specific values should remain inside each sketch instead of `secrets.h`, for example:

- `DEVICE_ID`
- `MACHINE_ID`
- `GATEWAY_ID`
- MQTT topic names

This prevents multiple devices from accidentally using the same identity.

### Git Ignore Rules

The project `.gitignore` must include rules similar to:

```gitignore
# Local firmware secrets
**/secrets.h
**/wifi_credentials.h
**/local_config.h
```

### Safety Check Before Commit

Before committing firmware changes, verify that the real secrets file is ignored:

```bash
git check-ignore -v firmware/common/secrets.h
```

Expected result:

```text
.gitignore:...:**/secrets.h    firmware/common/secrets.h
```

If an ESP32 sketch folder contains a local symbolic link or local copy named `secrets.h`, check it too:

```bash
git check-ignore -v firmware/esp32_gateway/esp32_gateway_mqtt_test/secrets.h
```

Also inspect what Git would add before actually staging files:

```bash
git add -n .
```

The following files must not appear in the add list:

```text
firmware/common/secrets.h
firmware/esp32_gateway/esp32_gateway_mqtt_test/secrets.h
data/raw/Exp1.csv
data/raw/Exp2.csv
data/raw/Prep.csv
```

### MacBook MQTT Broker IP

For the current prototype, the MQTT broker IP address is configured manually in `secrets.h`.

To find the MacBook Wi-Fi IP address:

```bash
ipconfig getifaddr en0
```

Use the returned address as:

```cpp
#define MQTT_BROKER_IP "YOUR_MACBOOK_IP"
#define MQTT_BROKER_PORT 1883
```

This manual IP approach is used for the first hardware prototype because it is simple and reliable during local Wi-Fi or hotspot-based testing.
