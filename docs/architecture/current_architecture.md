# Current Architecture

The current prototype uses a layered local IoT architecture.

```text
ESP-12E edge nodes
        ↓ ESP-NOW
ESP32 gateway
        ↓ MQTT
macOS central computer
        ↓
SQLite + HTTP/WebSocket dashboard
```

## Components

### ESP-12E Edge Nodes

ESP-12E devices represent CNC edge nodes. Current firmware variants include:

- `esp12e_auto_sender`: automatically sends simulated CNC telemetry after power-up.
- `esp12e_button_state_node`: supports multiple CNC states using a push button and WS2812 LED.

### ESP32 Gateway

The ESP32 gateway receives ESP-NOW packets from ESP-12E nodes and forwards them to the macOS MQTT broker as JSON messages.

If an edge node does not provide a machine label, the ESP32 assigns automatic names such as:

```text
CNC-01
CNC-02
CNC-03
```

### macOS Backend

The macOS computer runs:

- Mosquitto MQTT broker,
- Python dashboard server,
- SQLite database,
- local web dashboard.

## Design Decision

ESP-NOW is used between ESP-12E and ESP32 because it is lightweight and suitable for local ESP-to-ESP communication.

MQTT is used between the ESP32 gateway and macOS because it is better suited for IoT telemetry distribution and dashboard/backend integration.
