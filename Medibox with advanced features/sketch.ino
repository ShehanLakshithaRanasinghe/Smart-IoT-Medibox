#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include "DHTesp.h"
#include <ArduinoJson.h>  // Added this include

#define LDR_PIN 34
#define DHT_PIN 15
#define SERVO_PIN 13

// Default configuration
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
Servo windowServo;
DHTesp dhtSensor;

// Configuration parameters with defaults
float ts = 5.0;        // Sampling interval (seconds)
float tu = 120.0;      // Sending interval (seconds)
float theta_offset = 30.0;  // Minimum angle
float gamma_factor = 0.75;  // Renamed from gamma to avoid conflict
float T_med = 30.0;    // Ideal storage temp

// Data storage
const int samples = 24; // 120s/5s = 24 samples
float lightReadings[samples];
int currentSample = 0;
unsigned long lastSampleTime = 0;
unsigned long lastSendTime = 0;

void setup() {
  Serial.begin(115200);
  setupWiFi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  
  dhtSensor.setup(DHT_PIN, DHTesp::DHT11);
  windowServo.attach(SERVO_PIN);
  windowServo.write(theta_offset);
  
  // Initialize light readings array
  for (int i = 0; i < samples; i++) {
    lightReadings[i] = 0;
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentTime = millis();
  
  // Take light reading every ts seconds
  if (currentTime - lastSampleTime >= ts * 1000) {
    float lightValue = analogRead(LDR_PIN) / 4095.0; // Normalize to 0-1
    lightReadings[currentSample] = lightValue;
    currentSample = (currentSample + 1) % samples;
    lastSampleTime = currentTime;
    
    // Read temperature
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    float temperature = data.temperature;
    
    // Calculate and set servo angle
    float theta = calculateServoAngle(lightValue, temperature);
    windowServo.write(theta);
  }

  // Send averaged data every tu seconds
  if (currentTime - lastSendTime >= tu * 1000) {
    float avgLight = calculateAverageLight();
    publishData(avgLight);
    lastSendTime = currentTime;
  }
}

float calculateAverageLight() {
  float sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += lightReadings[i];
  }
  return sum / samples;
}

float calculateServoAngle(float lightIntensity, float temperature) {
  float term1 = lightIntensity * gamma_factor;  // Changed from gamma
  float term2 = log(ts / tu);
  float term3 = temperature / T_med;
  float angle = theta_offset + (180 - theta_offset) * term1 * term2 * term3;
  
  // Constrain angle between theta_offset and 180
  return constrain(angle, theta_offset, 180);
}

void publishData(float lightIntensity) {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  
  DynamicJsonDocument doc(256);
  doc["light"] = lightIntensity;
  doc["temperature"] = data.temperature;
  doc["servo_angle"] = windowServo.read();
  
  char payload[256];
  serializeJson(doc, payload);
  
  client.publish("medibox/data", payload);
  Serial.println(payload);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  DynamicJsonDocument doc(256);
  deserializeJson(doc, message);
  
  if (doc.containsKey("ts")) ts = doc["ts"];
  if (doc.containsKey("tu")) tu = doc["tu"];
  if (doc.containsKey("theta_offset")) theta_offset = doc["theta_offset"];
  if (doc.containsKey("gamma")) gamma_factor = doc["gamma"];  // Changed from gamma
  if (doc.containsKey("T_med")) T_med = doc["T_med"];
}

void setupWiFi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("MediboxClient")) {
      Serial.println("connected");
      client.subscribe("medibox/config");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
