# Current Limitations

This document lists the known limitations of the current CNC IoT monitoring prototype.

## 1. Simulated CNC Data

The current ESP-12E edge nodes generate representative CNC telemetry values in firmware. They do not yet read data from a real CNC machine.

The simulated data includes fields such as:

- cutting speed
- feed rate
- depth of cut
- machined length
- force values
- surface roughness values
- machine status
- alarm status

The original Kaggle dataset is still useful for backend and dashboard simulation, but the embedded ESP-12E prototype currently produces lightweight representative values directly in code.

## 2. Timestamp Handling

Some embedded messages currently use uptime-like timestamps such as:

```text
ESP32_MILLIS_6192
```

This represents device uptime rather than real wall-clock time.

This is acceptable for the prototype stage. In a real CNC integration, timestamp data may come from the CNC controller, the data acquisition interface, or the central backend.

## 3. ESP-12E Button State Node Stability

The button-controlled ESP-12E node includes the planned operating states:

- `LIVE_IDLE`
- `RUNNING`
- `OFFLINE_STANDBY`
- alarm packet blink behavior

However, this node still needs stabilization. Observed issues include:

- delayed button response in some situations
- LED state changes that may not always match the expected state immediately
- temporary offline transitions when packet delivery is interrupted

The next development step is to simplify and stabilize the state machine.

## 4. ESP-NOW Reliability

ESP-NOW is lightweight and suitable for local edge-to-gateway communication, but packet loss can occur.

The dashboard therefore should not mark a node offline after only one missed packet. The offline timeout should be long enough to tolerate occasional packet loss.

## 5. Automatic Machine Naming Is Volatile

The ESP32 gateway can automatically assign names such as:

```text
CNC-01
CNC-02
CNC-03
```

to edge nodes that do not provide a label.

Currently, this mapping is stored in RAM. If the ESP32 restarts, automatic naming starts again from the beginning.

Future improvement:

- store MAC-to-machine-name mappings in non-volatile memory
- or define device identities in configuration files

## 6. Local Network Dependency

The current prototype assumes that the macOS computer, ESP32 gateway, and ESP-12E edge nodes are on the same local network or compatible Wi-Fi channel.

For demos, a phone hotspot or dedicated local router is preferred.

University, enterprise, or proxy-based networks may block local device-to-device communication.

## 7. Security

The current Mosquitto local configuration allows anonymous MQTT connections for simple local demo testing:

```text
allow_anonymous true
```

This is acceptable only for a closed local prototype network.

Future improvement:

- add MQTT username/password authentication
- use TLS for remote deployments
- restrict broker access to trusted devices

## 8. Hardware Wiring Requires Care

The ESP32 and ESP-12E boards should be powered only through verified power pins.

For ESP32 external power:

```text
5V  -> ESP32 5V / VIN pin
GND -> verified ESP32 GND pin
```

Do not power the board through:

- 3V3 pin
- EN pin
- GPIO pins
- BOOT / IO0 pin

## 9. Documentation Still Needs Expansion

The current documentation now includes the main architecture and state diagrams, but additional improvements may still be useful:

- photographed real wiring diagrams
- breadboard-level circuit images
- troubleshooting guide for ESP-NOW and MQTT
- report-ready architecture figure
- presentation-ready system flow diagram

