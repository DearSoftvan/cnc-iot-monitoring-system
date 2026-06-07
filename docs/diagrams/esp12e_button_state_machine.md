# ESP-12E Button State Machine

```mermaid
stateDiagram-v2
    [*] --> LIVE_IDLE

    LIVE_IDLE: LED Yellow
    LIVE_IDLE: Gateway communication active
    LIVE_IDLE: CNC stopped / no CNC data
    LIVE_IDLE --> RUNNING: Button press

    RUNNING: LED Purple
    RUNNING: CNC telemetry flowing
    RUNNING --> OFFLINE_STANDBY: Button press

    OFFLINE_STANDBY: LED White
    OFFLINE_STANDBY: No packets sent
    OFFLINE_STANDBY --> LIVE_IDLE: Button press

    RUNNING --> RUNNING: Alarm packet<br/>Purple blink x3
    LIVE_IDLE --> TRANSITION_ERROR: Transition failure
    RUNNING --> TRANSITION_ERROR: Transition failure

    TRANSITION_ERROR: LED Red
    TRANSITION_ERROR --> LIVE_IDLE: Recovery
```

## LED Color Mapping

| State / Event | LED Color |
|--------------|-----------|
| LIVE_IDLE | Yellow |
| RUNNING | Purple |
| OFFLINE_STANDBY | White |
| TRANSITION_ERROR | Red |
| Alarm packet during RUNNING | Purple blink x3 |
