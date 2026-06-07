# ESP-12E Button State Node Wiring

This document describes the wiring for the ESP-12E / NodeMCU button-controlled edge node used in the CNC IoT monitoring prototype.

The node supports three main operating states:

- `LIVE_IDLE`: the edge node is alive and communicating with the ESP32 gateway, but CNC data is not flowing.
- `RUNNING`: CNC telemetry data is being generated and sent to the ESP32 gateway.
- `OFFLINE_STANDBY`: the node stops sending packets, so the dashboard marks it offline after timeout.

## Components

| Component | Quantity | Purpose |
|---|---:|---|
| NodeMCU V3 / ESP-12E / ESP8266 | 1 | Edge node controller |
| WS2812 RGB LED | 1 | Visual status indicator |
| 4-pin push button | 1 | State transition control |
| 220 Ω or 330 Ω resistor | 1 | Data line protection for WS2812 |
| Breadboard | 1 | Prototyping |
| Jumper wires | Several | Connections |
| MicroUSB cable | 1 | Programming and power during debugging |

## Pin Mapping

| Function | NodeMCU Pin | ESP8266 GPIO | Code Constant |
|---|---|---:|---|
| Push button input | D5 | GPIO14 | `BUTTON_PIN 14` |
| WS2812 data input | D2 | GPIO4 | `LED_PIN 4` |
| LED power | 3V | - | - |
| Ground | G / GND | - | - |

## Wiring Table

| Component | Component Pin | Connect To | Notes |
|---|---|---|---|
| Push button | One side | D5 | Button input |
| Push button | Opposite side | GND | Internal pull-up is used |
| WS2812 RGB LED | DIN / IN | D2 through 220 Ω or 330 Ω resistor | Data input |
| WS2812 RGB LED | VCC / + | 3V | Single LED test power |
| WS2812 RGB LED | GND / - | GND | Common ground |

## Text Wiring Diagram

```text
NodeMCU / ESP-12E

D5 / GPIO14  -------------------- Push Button -------------------- GND

D2 / GPIO4   ---- 220Ω or 330Ω ---- WS2812 DIN / IN

3V           ---------------------- WS2812 VCC / +

GND          ---------------------- WS2812 GND / -
```

## Button Logic

The firmware uses the internal pull-up resistor:

```cpp
pinMode(BUTTON_PIN, INPUT_PULLUP);
```

Therefore:

| Button State | Digital Reading |
|---|---|
| Not pressed | `HIGH` |
| Pressed | `LOW` |

If the button does not respond correctly, rotate the 4-pin tactile button by 90 degrees on the breadboard and test again.

## LED State Colors

| Node State | LED Color | Meaning |
|---|---|---|
| `LIVE_IDLE` | Yellow | ESP-12E is alive and communicating, but CNC data is not flowing |
| `RUNNING` | Purple | CNC telemetry data is being generated and transmitted |
| `OFFLINE_STANDBY` | White | The node is not sending packets; dashboard will mark it offline after timeout |
| `TRANSITION_ERROR` | Red | State transition or communication initialization failed |
| Alarm packet during running | Purple blink x3 | A simulated CNC alarm/error packet was sent |

## Notes

- Connect the WS2812 data wire to `DIN / IN`, not `DOUT / OUT`.
- Use a short wire between D2 and the LED data input when possible.
- A 100 µF capacitor across LED VCC and GND is recommended for larger LED setups, but it is not required for the single low-brightness LED test.
- The ESP-12E should be programmed through MicroUSB.
- For stable demos, avoid powering the ESP-12E through uncertain external power wiring unless the voltage and ground pins are verified.

