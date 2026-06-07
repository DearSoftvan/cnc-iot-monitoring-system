#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

// Device identity
const char* GATEWAY_ID = "ESP32-GW-01";
const char* DEVICE_ID = "GATEWAY-ESP32-01";
const char* TEST_MACHINE_ID = "CNC-01";

// MQTT topics
const char* TELEMETRY_TOPIC = "factory/cnc/CNC-01/telemetry";
const char* GATEWAY_STATUS_TOPIC = "factory/gateway/ESP32-GW-01/status";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastPublishTime = 0;
const unsigned long publishIntervalMs = 2000;
unsigned long sequenceNo = 0;

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

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("MQTT connected.");

      String statusPayload = "{";
      statusPayload += "\"gateway_id\":\"" + String(GATEWAY_ID) + "\",";
      statusPayload += "\"status\":\"ONLINE\",";
      statusPayload += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
      statusPayload += "}";

      mqttClient.publish(GATEWAY_STATUS_TOPIC, statusPayload.c_str(), true);
    } else {
      Serial.print("MQTT connection failed. State: ");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

String createTelemetryPayload() {
  sequenceNo++;

  float cuttingSpeed = 250 + (sequenceNo % 50);
  float feedRate = 0.07;
  float depthOfCut = 0.25;
  float machinedLength = sequenceNo % 100;

  float fx = 45.0 + (sequenceNo % 10);
  float fy = 40.0 + (sequenceNo % 8);
  float fz = 20.0 + (sequenceNo % 6);
  float totalForce = fx + fy + fz;

  float ra = 0.45 + ((sequenceNo % 10) * 0.03);
  float rz = 1.80 + ((sequenceNo % 10) * 0.05);
  float rt = 2.10 + ((sequenceNo % 10) * 0.04);

  String qualityStatus = "GOOD";
  if (ra >= 0.8 && ra < 1.2) {
    qualityStatus = "WARNING";
  } else if (ra >= 1.2) {
    qualityStatus = "BAD";
  }

  String loadStatus = "NORMAL_LOAD";
  if (totalForce >= 250 && totalForce < 350) {
    loadStatus = "HIGH_LOAD";
  } else if (totalForce >= 350) {
    loadStatus = "FORCE_ALARM";
  }

  String payload = "{";

  payload += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  payload += "\"gateway_id\":\"" + String(GATEWAY_ID) + "\",";
  payload += "\"machine_id\":\"" + String(TEST_MACHINE_ID) + "\",";
  payload += "\"source_file\":\"ESP32_TEST\",";
  payload += "\"sequence_no\":" + String(sequenceNo) + ",";
  payload += "\"timestamp\":\"ESP32_MILLIS_" + String(millis()) + "\",";

  payload += "\"run_id\":\"ESP32_TEST_RUN\",";
  payload += "\"tool_id\":32,";

  payload += "\"process\":{";
  payload += "\"depth_of_cut_ap\":" + String(depthOfCut, 2) + ",";
  payload += "\"cutting_speed_vc\":" + String(cuttingSpeed, 2) + ",";
  payload += "\"feed_rate_f\":" + String(feedRate, 3) + ",";
  payload += "\"machined_length\":" + String(machinedLength, 2);
  payload += "},";

  payload += "\"force\":{";
  payload += "\"fx\":" + String(fx, 2) + ",";
  payload += "\"fy\":" + String(fy, 2) + ",";
  payload += "\"fz\":" + String(fz, 2) + ",";
  payload += "\"total_force\":" + String(totalForce, 2);
  payload += "},";

  payload += "\"quality\":{";
  payload += "\"ra\":" + String(ra, 3) + ",";
  payload += "\"rz\":" + String(rz, 3) + ",";
  payload += "\"rt\":" + String(rt, 3);
  payload += "},";

  payload += "\"status\":{";
  payload += "\"machine_status\":\"RUNNING\",";
  payload += "\"load_status\":\"" + loadStatus + "\",";
  payload += "\"quality_status\":\"" + qualityStatus + "\",";
  payload += "\"alarm_code\":null";
  payload += "}";

  payload += "}";

  return payload;
}

void publishTelemetry() {
  String payload = createTelemetryPayload();

  bool published = mqttClient.publish(TELEMETRY_TOPIC, payload.c_str());

  if (published) {
    Serial.print("Published to ");
    Serial.println(TELEMETRY_TOPIC);
    Serial.println(payload);
  } else {
    Serial.println("MQTT publish failed.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("ESP32 Gateway MQTT Test Started");

  connectToWiFi();
  connectToMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  if (!mqttClient.connected()) {
    connectToMQTT();
  }

  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastPublishTime >= publishIntervalMs) {
    lastPublishTime = now;
    publishTelemetry();
  }
}
