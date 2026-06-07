# System Architecture Diagram

```mermaid
flowchart LR
    A[ESP-12E Edge Node<br/>Auto Sender] -->|ESP-NOW| G[ESP32 Gateway]
    B[ESP-12E Edge Node<br/>Button State Node] -->|ESP-NOW| G

    G -->|MQTT<br/>factory/cnc/&lt;machine_id&gt;/telemetry| M[macOS Mosquitto Broker]
    G -->|MQTT<br/>factory/gateway/ESP32-GW-01/status| M

    M --> S[Python Dashboard Server]
    S --> DB[(SQLite Database)]
    S --> W[Local Web Dashboard<br/>HTTP / WebSocket]

    W --> U[Browser / Phone<br/>Same Wi-Fi Network]
```

## Notes

- ESP-12E nodes send compact ESP-NOW packets.
- ESP32 acts as a gateway and converts ESP-NOW packets into MQTT JSON messages.
- macOS runs the Mosquitto broker, dashboard server, database, and web UI.
- The dashboard shows both gateway status and CNC device status.
