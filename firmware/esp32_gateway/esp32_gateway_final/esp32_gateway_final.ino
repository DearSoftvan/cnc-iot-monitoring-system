#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>

#include "secrets.h"
#include "../../common/cnc_espnow_packet.h"

const char* GATEWAY_ID = "ESP32-GW-01";
const char* GATEWAY_STATUS_TOPIC = "factory/gateway/ESP32-GW-01/status";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

const int MAX_EDGE_NODES = 10;

struct EdgeNodeMap {
  bool active;
  uint8_t mac[6];
  char machine_id[16];
  char device_id[16];
  unsigned long last_seen_ms;
};

EdgeNodeMap edgeNodes[MAX_EDGE_NODES];
int edgeNodeCount = 0;

volatile bool packetPending = false;
uint8_t pendingMac[6];
CncEspNowPacket pendingPacket;

unsigned long lastGatewayStatusMs = 0;
const unsigned long gatewayStatusIntervalMs = 10000;

String macToString(const uint8_t* mac) {
  char buffer[18];
  snprintf(
    buffer,
    sizeof(buffer),
    "%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
  );
  return String(buffer);
}

bool sameMac(const uint8_t* a, const uint8_t* b) {
  for (int i = 0; i < 6; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

const char* machineStatusToText(uint8_t status) {
  switch (status) {
    case 1: return "RUNNING";
    case 2: return "STOPPED";
    case 3: return "ALARM";
    default: return "UNKNOWN";
  }
}

int findOrCreateEdgeNode(const uint8_t* mac, const char* requestedLabel) {
  for (int i = 0; i < edgeNodeCount; i++) {
    if (edgeNodes[i].active && sameMac(edgeNodes[i].mac, mac)) {
      edgeNodes[i].last_seen_ms = millis();
      return i;
    }
  }

  if (edgeNodeCount >= MAX_EDGE_NODES) {
    Serial.println("Edge node limit reached. Packet ignored.");
    return -1;
  }

  int index = edgeNodeCount;
  edgeNodes[index].active = true;

  for (int i = 0; i < 6; i++) {
    edgeNodes[index].mac[i] = mac[i];
  }

  bool hasRequestedLabel = requestedLabel != nullptr && strlen(requestedLabel) > 0;

  if (hasRequestedLabel) {
    snprintf(edgeNodes[index].machine_id, sizeof(edgeNodes[index].machine_id), "%s", requestedLabel);
  } else {
    snprintf(edgeNodes[index].machine_id, sizeof(edgeNodes[index].machine_id), "CNC-%02d", index + 1);
  }

  snprintf(edgeNodes[index].device_id, sizeof(edgeNodes[index].device_id), "EDGE-%02d", index + 1);
  edgeNodes[index].last_seen_ms = millis();

  edgeNodeCount++;

  Serial.print("New edge node registered. MAC=");
  Serial.print(macToString(mac));
  Serial.print(" machine_id=");
  Serial.print(edgeNodes[index].machine_id);
  Serial.print(" device_id=");
  Serial.println(edgeNodes[index].device_id);

  return index;
}

void connectToWiFi() {
  Serial.println();
  Serial.println("Connecting to Wi-Fi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected.");
  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi channel: ");
  Serial.println(WiFi.channel());
}

void publishGatewayStatus(const char* status) {
  String payload = "{";
  payload += "\"gateway_id\":\"" + String(GATEWAY_ID) + "\",";
  payload += "\"status\":\"" + String(status) + "\",";
  payload += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  payload += "\"wifi_channel\":" + String(WiFi.channel()) + ",";
  payload += "\"edge_node_count\":" + String(edgeNodeCount);
  payload += "}";

  mqttClient.publish(GATEWAY_STATUS_TOPIC, payload.c_str(), true);
}

void connectToMQTT() {
  mqttClient.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT);
  mqttClient.setBufferSize(1024);

  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT broker ");
    Serial.print(MQTT_BROKER_IP);
    Serial.print(":");
    Serial.println(MQTT_BROKER_PORT);

    String clientId = String(GATEWAY_ID) + "-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);

    String offlinePayload = "{";
    offlinePayload += "\"gateway_id\":\"" + String(GATEWAY_ID) + "\",";
    offlinePayload += "\"status\":\"OFFLINE\"";
    offlinePayload += "}";

    if (mqttClient.connect(
          clientId.c_str(),
          GATEWAY_STATUS_TOPIC,
          1,
          true,
          offlinePayload.c_str()
        )) {
      Serial.println("MQTT connected.");
      publishGatewayStatus("ONLINE");
    } else {
      Serial.print("MQTT connection failed. State: ");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

// ESP32 Arduino Core 3.x callback signature
void onEspNowReceive(const esp_now_recv_info_t* info, const uint8_t* incomingData, int len) {
  if (len != sizeof(CncEspNowPacket)) {
    Serial.print("Invalid ESP-NOW packet size: ");
    Serial.println(len);
    return;
  }

  CncEspNowPacket packet;
  memcpy(&packet, incomingData, sizeof(packet));

  if (packet.magic != CNC_PACKET_MAGIC || packet.version != CNC_PACKET_VERSION) {
    Serial.println("Invalid ESP-NOW packet magic/version.");
    return;
  }

  memcpy(pendingMac, info->src_addr, 6);
  memcpy((void*)&pendingPacket, &packet, sizeof(packet));
  packetPending = true;
}

void setupEspNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed. Restarting...");
    delay(2000);
    ESP.restart();
  }

  esp_now_register_recv_cb(onEspNowReceive);
  Serial.println("ESP-NOW receiver initialized.");
}

String createTelemetryJson(const CncEspNowPacket& packet, const uint8_t* mac, int nodeIndex) {
  const char* machineId = edgeNodes[nodeIndex].machine_id;
  const char* deviceId = edgeNodes[nodeIndex].device_id;

  float totalForce = packet.total_force;
  if (totalForce <= 0.001) {
    totalForce = packet.fx + packet.fy + packet.fz;
  }

  String qualityStatus = "GOOD";
  if (packet.ra >= 0.8 && packet.ra < 1.2) {
    qualityStatus = "WARNING";
  } else if (packet.ra >= 1.2) {
    qualityStatus = "BAD";
  }

  String loadStatus = "NORMAL_LOAD";
  if (totalForce >= 250 && totalForce < 350) {
    loadStatus = "HIGH_LOAD";
  } else if (totalForce >= 350) {
    loadStatus = "FORCE_ALARM";
  }

  String alarmCode = "null";
  if (packet.machine_status == 3) {
    alarmCode = "\"EDGE_ALARM\"";
  }

  String payload = "{";

  payload += "\"device_id\":\"" + String(deviceId) + "\",";
  payload += "\"gateway_id\":\"" + String(GATEWAY_ID) + "\",";
  payload += "\"machine_id\":\"" + String(machineId) + "\",";
  payload += "\"edge_mac\":\"" + macToString(mac) + "\",";
  payload += "\"source_file\":\"ESP12E_ESPNOW\",";
  payload += "\"sequence_no\":" + String(packet.sequence_no) + ",";
  payload += "\"timestamp\":\"ESP32_MILLIS_" + String(millis()) + "\",";

  payload += "\"run_id\":\"ESP12E_EDGE_PACKET\",";
  payload += "\"tool_id\":" + String(packet.tool_id) + ",";

  payload += "\"process\":{";
  payload += "\"depth_of_cut_ap\":" + String(packet.depth_of_cut_ap, 2) + ",";
  payload += "\"cutting_speed_vc\":" + String(packet.cutting_speed_vc, 2) + ",";
  payload += "\"feed_rate_f\":" + String(packet.feed_rate_f, 3) + ",";
  payload += "\"machined_length\":" + String(packet.machined_length, 2);
  payload += "},";

  payload += "\"force\":{";
  payload += "\"fx\":" + String(packet.fx, 2) + ",";
  payload += "\"fy\":" + String(packet.fy, 2) + ",";
  payload += "\"fz\":" + String(packet.fz, 2) + ",";
  payload += "\"total_force\":" + String(totalForce, 2);
  payload += "},";

  payload += "\"quality\":{";
  payload += "\"ra\":" + String(packet.ra, 3) + ",";
  payload += "\"rz\":" + String(packet.rz, 3) + ",";
  payload += "\"rt\":" + String(packet.rt, 3);
  payload += "},";

  payload += "\"status\":{";
  payload += "\"machine_status\":\"" + String(machineStatusToText(packet.machine_status)) + "\",";
  payload += "\"load_status\":\"" + loadStatus + "\",";
  payload += "\"quality_status\":\"" + qualityStatus + "\",";
  payload += "\"alarm_code\":" + alarmCode;
  payload += "}";

  payload += "}";

  return payload;
}

void publishEdgePacket(const CncEspNowPacket& packet, const uint8_t* mac) {
  char safeLabel[CNC_LABEL_SIZE + 1];
  memcpy(safeLabel, packet.node_label, CNC_LABEL_SIZE);
  safeLabel[CNC_LABEL_SIZE] = '\0';

  int nodeIndex = findOrCreateEdgeNode(mac, safeLabel);
  if (nodeIndex < 0) return;

  String topic = "factory/cnc/";
  topic += edgeNodes[nodeIndex].machine_id;
  topic += "/telemetry";

  String payload = createTelemetryJson(packet, mac, nodeIndex);

  bool published = mqttClient.publish(topic.c_str(), payload.c_str());

  if (published) {
    Serial.print("Forwarded ESP-NOW packet to MQTT topic: ");
    Serial.println(topic);
    Serial.println(payload);
  } else {
    Serial.println("Failed to publish edge packet to MQTT.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("ESP32 Gateway Final Started");

  connectToWiFi();
  connectToMQTT();
  setupEspNow();

  Serial.println("Gateway is ready. Waiting for ESP-12E edge nodes...");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  if (!mqttClient.connected()) {
    connectToMQTT();
  }

  mqttClient.loop();

  if (packetPending) {
    CncEspNowPacket localPacket;
    uint8_t localMac[6];

    memcpy(&localPacket, (const void*)&pendingPacket, sizeof(localPacket));
    memcpy(localMac, pendingMac, 6);

    packetPending = false;

    publishEdgePacket(localPacket, localMac);
  }

  unsigned long now = millis();
  if (now - lastGatewayStatusMs >= gatewayStatusIntervalMs) {
    lastGatewayStatusMs = now;
    publishGatewayStatus("ONLINE");
  }
}
