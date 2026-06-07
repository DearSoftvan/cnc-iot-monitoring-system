# ESP32 Gateway Power Wiring

This note documents the safe external power approach used for the ESP32 gateway board.

## Recommended External Power

```text
Power supply +5V → ESP32 5V / VIN pin
Power supply GND → verified ESP32 GND pin
```

Recommended power supply setting:

```text
Voltage: 5.0V
Current limit: 700mA - 1A
```

## Do Not Use

Do not power the ESP32 through these pins during this project:

```text
3V3
EN
IO0 / BOOT
GPIO pins
TX / RX
```

## Debugging Recommendation

For firmware upload and Serial Monitor debugging:

```text
ESP32 → USB only → MacBook
```

For standalone operation:

```text
ESP32 → external 5V supply only
```

Avoid mixing USB power and external 5V during early debugging unless the wiring is fully understood.
