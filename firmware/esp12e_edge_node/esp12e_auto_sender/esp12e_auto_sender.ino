#include <ESP8266WiFi.h>
#include <espnow.h>
#include <string.h>

#include "secrets.h"
#include "cnc_espnow_packet.h"

// Broadcast address: ESP32 gateway will receive this ESP-NOW packet.
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Leave empty for automatic naming by ESP32 gateway.
// Gateway will assign CNC-01, CNC-02, ...
const char* NODE_LABEL = "";

unsigned long lastSendTime = 0;
const unsigned long sendIntervalMs = 2000;

uint32_t sequenceNo = 0;

void onDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("ESP-NOW send status: ");
  Serial.println(sendStatus == 0 ? "SUCCESS" : "FAILED");
}

void connectToWiFi() {
  Serial.println();
  Serial.println("Connecting to Wi-Fi...");

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected.");
  Serial.print("ESP-12E IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("ESP-12E MAC address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Wi-Fi channel: ");
  Serial.println(WiFi.channel());
}

void setupEspNow() {
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed. Restarting...");
    delay(2000);
    ESP.restart();
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

  if (result == 0) {
    Serial.println("ESP-NOW broadcast peer added.");
  } else {
    Serial.print("ESP-NOW add peer failed. Code: ");
    Serial.println(result);
  }

  Serial.println("ESP-NOW sender initialized.");
}

CncEspNowPacket createPacket() {
  sequenceNo++;

  CncEspNowPacket packet;
  memset(&packet, 0, sizeof(packet));

  packet.magic = CNC_PACKET_MAGIC;
  packet.version = CNC_PACKET_VERSION;
  packet.sequence_no = sequenceNo;

  // Optional label. Empty label means gateway will auto-assign CNC-01, CNC-02...
  if (NODE_LABEL != nullptr && strlen(NODE_LABEL) > 0) {
    strncpy(packet.node_label, NODE_LABEL, CNC_LABEL_SIZE - 1);
    packet.node_label[CNC_LABEL_SIZE - 1] = '\0';
  }

  packet.tool_id = 12;

  // Every 7th packet simulates an error/alarm condition.
  bool alarmPacket = (sequenceNo % 7 == 0);

  if (!alarmPacket) {
    // Normal CNC-like telemetry values.
    packet.depth_of_cut_ap = 0.25;
    packet.cutting_speed_vc = 220.0 + (sequenceNo % 40);
    packet.feed_rate_f = 0.070;
    packet.machined_length = sequenceNo % 120;

    packet.fx = 40.0 + (sequenceNo % 10);
    packet.fy = 35.0 + (sequenceNo % 8);
    packet.fz = 18.0 + (sequenceNo % 6);
    packet.total_force = packet.fx + packet.fy + packet.fz;

    packet.ra = 0.45 + ((sequenceNo % 10) * 0.025);
    packet.rz = 1.70 + ((sequenceNo % 10) * 0.050);
    packet.rt = 2.00 + ((sequenceNo % 10) * 0.040);

    // 1 = RUNNING
    packet.machine_status = 1;
  } else {
    // Simulated alarm/error packet.
    // ESP32 gateway will convert machine_status=3 to alarm_code="EDGE_ALARM".
    packet.depth_of_cut_ap = 0.25;
    packet.cutting_speed_vc = 260.0;
    packet.feed_rate_f = 0.090;
    packet.machined_length = sequenceNo % 120;

    // Higher force values simulate abnormal cutting load.
    packet.fx = 130.0 + (sequenceNo % 20);
    packet.fy = 110.0 + (sequenceNo % 15);
    packet.fz = 95.0 + (sequenceNo % 10);
    packet.total_force = packet.fx + packet.fy + packet.fz;

    // Higher surface roughness simulates quality issue.
    packet.ra = 1.35;
    packet.rz = 4.80;
    packet.rt = 5.20;

    // 3 = ALARM
    packet.machine_status = 3;
  }

  return packet;
}

void sendPacket() {
  CncEspNowPacket packet = createPacket();

  int result = esp_now_send(
    broadcastAddress,
    (uint8_t *)&packet,
    sizeof(packet)
  );

  Serial.print("Sending packet #");
  Serial.print(packet.sequence_no);
  Serial.print(" size=");
  Serial.print(sizeof(packet));
  Serial.print(" status=");
  Serial.print(packet.machine_status == 3 ? "ALARM" : "RUNNING");
  Serial.print(" result=");
  Serial.println(result);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("ESP-12E Auto Sender Started");

  connectToWiFi();
  setupEspNow();

  Serial.println("ESP-12E is ready. Sending CNC packets automatically...");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected. Reconnecting...");
    connectToWiFi();
    setupEspNow();
  }

  unsigned long now = millis();

  if (now - lastSendTime >= sendIntervalMs) {
    lastSendTime = now;
    sendPacket();
  }
}
