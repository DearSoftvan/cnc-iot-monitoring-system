#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Adafruit_NeoPixel.h>
#include <string.h>

#include "secrets.h"
#include "cnc_espnow_packet.h"

// Pin mapping for NodeMCU ESP-12E
#define BUTTON_PIN 14   // D5 = GPIO14
#define LED_PIN 4       // D2 = GPIO4
#define NUM_LEDS 1

Adafruit_NeoPixel led(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ESP-NOW broadcast address
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Leave empty for automatic CNC-01, CNC-02... naming by ESP32 gateway
const char* NODE_LABEL = "";

enum NodeState {
  LIVE_IDLE,
  RUNNING,
  OFFLINE_STANDBY
};

NodeState currentState = LIVE_IDLE;

uint32_t sequenceNo = 0;

unsigned long lastSendTime = 0;
const unsigned long runningSendIntervalMs = 2000;
const unsigned long idleHeartbeatIntervalMs = 3000;

bool networkReady = false;

// Debounce
bool lastReading = HIGH;
bool stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 60;

// Colors with low brightness values
void setColor(uint8_t r, uint8_t g, uint8_t b) {
  led.setPixelColor(0, led.Color(r, g, b));
  led.show();
}

void colorLiveIdle() {
  setColor(40, 25, 0);   // Yellow
}

void colorRunning() {
  setColor(28, 0, 40);   // Purple
}

void colorOfflineStandby() {
  setColor(30, 30, 30);  // White
}

void colorTransitionError() {
  setColor(40, 0, 0);    // Red
}

void colorConnecting() {
  setColor(0, 0, 40);    // Blue
}

void blinkAlarmWhileRunning() {
  // Alarm packet indication: blink in RUNNING color 3 times
  for (int i = 0; i < 3; i++) {
    colorRunning();
    delay(130);
    setColor(0, 0, 0);
    delay(130);
  }
  colorRunning();
}

void showTransitionErrorAndReturn(NodeState fallbackState) {
  colorTransitionError();
  delay(1200);

  if (fallbackState == LIVE_IDLE) {
    colorLiveIdle();
  } else if (fallbackState == RUNNING) {
    colorRunning();
  } else {
    colorOfflineStandby();
  }
}

void onDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("ESP-NOW send status: ");
  Serial.println(sendStatus == 0 ? "SUCCESS" : "FAILED");
}

bool startNetworkStack() {
  Serial.println();
  Serial.println("Starting Wi-Fi and ESP-NOW...");
  colorConnecting();

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();
  const unsigned long wifiTimeoutMs = 15000;

  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < wifiTimeoutMs) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println();
    Serial.println("Wi-Fi connection failed.");
    return false;
  }

  Serial.println();
  Serial.println("Wi-Fi connected.");
  Serial.print("ESP-12E IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("ESP-12E MAC address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Wi-Fi channel: ");
  Serial.println(WiFi.channel());

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed.");
    return false;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(onDataSent);

  uint8_t channel = WiFi.channel();

  int result = esp_now_add_peer(
    broadcastAddress,
    ESP_NOW_ROLE_SLAVE,
    channel,
    NULL,
    0
  );

  if (result != 0) {
    Serial.print("ESP-NOW add peer failed. Code: ");
    Serial.println(result);
    return false;
  }

  Serial.println("ESP-NOW broadcast peer added.");
  Serial.println("Network stack ready.");

  networkReady = true;
  return true;
}

void stopNetworkStack() {
  Serial.println("Stopping Wi-Fi and ESP-NOW for standby...");

  esp_now_deinit();

  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(100);

  networkReady = false;
  Serial.println("Offline standby active.");
}

CncEspNowPacket createPacket(uint8_t machineStatus, bool alarmPacket) {
  sequenceNo++;

  CncEspNowPacket packet;
  memset(&packet, 0, sizeof(packet));

  packet.magic = CNC_PACKET_MAGIC;
  packet.version = CNC_PACKET_VERSION;
  packet.sequence_no = sequenceNo;

  if (NODE_LABEL != nullptr && strlen(NODE_LABEL) > 0) {
    strncpy(packet.node_label, NODE_LABEL, CNC_LABEL_SIZE - 1);
    packet.node_label[CNC_LABEL_SIZE - 1] = '\0';
  }

  packet.tool_id = 22;
  packet.machine_status = machineStatus;

  if (machineStatus == 2) {
    // LIVE_IDLE / CNC not working / no CNC data
    // We still send heartbeat packets so dashboard knows the node is alive.
    packet.depth_of_cut_ap = 0.0;
    packet.cutting_speed_vc = 0.0;
    packet.feed_rate_f = 0.0;
    packet.machined_length = 0.0;

    packet.fx = 0.0;
    packet.fy = 0.0;
    packet.fz = 0.0;
    packet.total_force = 0.0;

    packet.ra = 0.0;
    packet.rz = 0.0;
    packet.rt = 0.0;

    return packet;
  }

  if (!alarmPacket) {
    // Normal running telemetry
    packet.depth_of_cut_ap = 0.25;
    packet.cutting_speed_vc = 240.0 + (sequenceNo % 35);
    packet.feed_rate_f = 0.075;
    packet.machined_length = sequenceNo % 120;

    packet.fx = 42.0 + (sequenceNo % 10);
    packet.fy = 37.0 + (sequenceNo % 8);
    packet.fz = 19.0 + (sequenceNo % 6);
    packet.total_force = packet.fx + packet.fy + packet.fz;

    packet.ra = 0.48 + ((sequenceNo % 10) * 0.025);
    packet.rz = 1.75 + ((sequenceNo % 10) * 0.050);
    packet.rt = 2.05 + ((sequenceNo % 10) * 0.040);

    return packet;
  }

  // Alarm packet during running state
  packet.depth_of_cut_ap = 0.25;
  packet.cutting_speed_vc = 270.0;
  packet.feed_rate_f = 0.095;
  packet.machined_length = sequenceNo % 120;

  packet.fx = 145.0 + (sequenceNo % 20);
  packet.fy = 120.0 + (sequenceNo % 15);
  packet.fz = 105.0 + (sequenceNo % 10);
  packet.total_force = packet.fx + packet.fy + packet.fz;

  packet.ra = 1.40;
  packet.rz = 4.90;
  packet.rt = 5.30;

  packet.machine_status = 3; // ALARM
  return packet;
}

void sendPacket(uint8_t machineStatus, bool alarmPacket) {
  if (!networkReady) {
    Serial.println("Network is not ready. Packet not sent.");
    return;
  }

  CncEspNowPacket packet = createPacket(machineStatus, alarmPacket);

  int result = esp_now_send(
    broadcastAddress,
    (uint8_t *)&packet,
    sizeof(packet)
  );

  Serial.print("Sending packet #");
  Serial.print(packet.sequence_no);
  Serial.print(" state=");

  if (packet.machine_status == 1) {
    Serial.print("RUNNING");
  } else if (packet.machine_status == 2) {
    Serial.print("LIVE_IDLE/STOPPED");
  } else if (packet.machine_status == 3) {
    Serial.print("ALARM");
  } else {
    Serial.print("UNKNOWN");
  }

  Serial.print(" size=");
  Serial.print(sizeof(packet));
  Serial.print(" result=");
  Serial.println(result);

  if (packet.machine_status == 3) {
    blinkAlarmWhileRunning();
  }
}

void enterLiveIdle() {
  Serial.println("State change: LIVE_IDLE");
  currentState = LIVE_IDLE;

  if (!networkReady) {
    if (!startNetworkStack()) {
      Serial.println("Failed to enter LIVE_IDLE.");
      showTransitionErrorAndReturn(OFFLINE_STANDBY);
      currentState = OFFLINE_STANDBY;
      colorOfflineStandby();
      return;
    }
  }

  colorLiveIdle();

  // Send immediate heartbeat so dashboard sees LIVE but STOPPED
  sendPacket(2, false);
}

void enterRunning() {
  Serial.println("State change: RUNNING");
  currentState = RUNNING;

  if (!networkReady) {
    if (!startNetworkStack()) {
      Serial.println("Failed to enter RUNNING.");
      showTransitionErrorAndReturn(LIVE_IDLE);
      currentState = LIVE_IDLE;
      colorLiveIdle();
      return;
    }
  }

  colorRunning();

  // Send immediate running packet
  sendPacket(1, false);
}

void enterOfflineStandby() {
  Serial.println("State change: OFFLINE_STANDBY");

  currentState = OFFLINE_STANDBY;
  stopNetworkStack();
  colorOfflineStandby();
}

void handleButtonPress() {
  if (currentState == LIVE_IDLE) {
    enterRunning();
  } else if (currentState == RUNNING) {
    enterOfflineStandby();
  } else {
    enterLiveIdle();
  }
}

void handleButton() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != stableButtonState) {
      stableButtonState = reading;

      if (stableButtonState == LOW) {
        Serial.println("Button pressed.");
        handleButtonPress();
      }
    }
  }

  lastReading = reading;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  led.begin();
  led.setBrightness(40);
  led.show();

  Serial.println();
  Serial.println("ESP-12E Button State Node Started");

  colorConnecting();

  // Startup state: LIVE_IDLE
  enterLiveIdle();

  Serial.println("State cycle:");
  Serial.println("LIVE_IDLE -> RUNNING -> OFFLINE_STANDBY -> LIVE_IDLE");
}

void loop() {
  handleButton();

  unsigned long now = millis();

  if (currentState == LIVE_IDLE) {
    if (networkReady && now - lastSendTime >= idleHeartbeatIntervalMs) {
      lastSendTime = now;
      sendPacket(2, false);
      colorLiveIdle();
    }
  }

  if (currentState == RUNNING) {
    if (networkReady && now - lastSendTime >= runningSendIntervalMs) {
      lastSendTime = now;

      // Every 8th packet is an alarm packet.
      bool alarmPacket = ((sequenceNo + 1) % 8 == 0);

      if (alarmPacket) {
        sendPacket(3, true);
      } else {
        sendPacket(1, false);
        colorRunning();
      }
    }
  }

  // OFFLINE_STANDBY sends no packets and keeps Wi-Fi off.
}
