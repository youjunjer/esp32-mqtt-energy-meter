#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <stdlib.h>

constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;
constexpr int OLED_SDA = 21;
constexpr int OLED_SCL = 22;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr int PZEM_RX = 16;
constexpr int PZEM_TX = 17;
constexpr int RELAY_PIN = 18;
constexpr int SG90_PIN = 5;
constexpr size_t PZEM_RESPONSE_SIZE = 25;

constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
constexpr char MQTT_HOST[] = "mqttgo.io";
constexpr uint16_t MQTT_PORT = 1883;
constexpr char MQTT_TOPIC[] = "eric1030/class701/data";
constexpr char MQTT_CTRL_TOPIC[] = "eric1030/class701/ctrl";
constexpr unsigned long MQTT_INTERVAL_MS = 10000UL;

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Servo sg90;
byte pzemBuffer[PZEM_RESPONSE_SIZE];
bool relayOn = true;
int sg90Angle = 0;
unsigned long lastPublishMs = 0;

struct EnergyData {
  float voltage, current, power, energy, frequency, powerFactor;
  bool valid;
};

void setRelay(bool on) {
  relayOn = on;
  digitalWrite(RELAY_PIN, on ? LOW : HIGH);
}

bool parseSg90Command(const String &message, int &angle) {
  int key = message.indexOf("\"SG90\"");
  if (key < 0) return false;
  int colon = message.indexOf(':', key + 6);
  if (colon < 0) return false;
  String token = message.substring(colon + 1);
  int comma = token.indexOf(',');
  if (comma >= 0) token = token.substring(0, comma);
  int brace = token.indexOf('}');
  if (brace >= 0) token = token.substring(0, brace);
  token.trim();
  if (token.length() == 0) return false;

  char *endPtr = nullptr;
  long value = strtol(token.c_str(), &endPtr, 10);
  while (endPtr != nullptr && *endPtr == ' ') endPtr++;
  if (endPtr == token.c_str() || (endPtr != nullptr && *endPtr != '\0')) return false;
  if (value < 0 || value > 180) return false;
  angle = static_cast<int>(value);
  return true;
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  if (String(topic) != MQTT_CTRL_TOPIC) return;
  String message;
  for (unsigned int i = 0; i < length; i++) message += static_cast<char>(payload[i]);
  message.trim();

  if (message.indexOf("\"RELAY\":\"ON\"") >= 0 || message.indexOf("\"RELAY\": \"ON\"") >= 0) {
    setRelay(true);
  } else if (message.indexOf("\"RELAY\":\"OFF\"") >= 0 || message.indexOf("\"RELAY\": \"OFF\"") >= 0) {
    setRelay(false);
  }

  int requestedAngle = 0;
  if (parseSg90Command(message, requestedAngle)) {
    sg90.write(requestedAngle);
    sg90Angle = requestedAngle;
    Serial.print("SG90 angle: ");
    Serial.println(sg90Angle);
  } else if (message.indexOf("\"SG90\"") >= 0) {
    Serial.println("SG90 command ignored: angle must be 0..180");
  }
}

void drawInvalid(const char *status) {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 10); display.println("V=-V");
  display.setCursor(0, 20); display.println("I=-A");
  display.setCursor(0, 30); display.println("W=-W");
  display.setCursor(0, 40); display.println("kWh=-");
  display.setCursor(0, 50); display.print("R="); display.print(relayOn ? "ON" : "OFF"); display.print(" S="); display.print(sg90Angle); display.print(" "); display.println(status);
  display.display();
}

void drawEnergy(const EnergyData &data, const char *status) {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 10); display.print("V="); display.print(data.voltage, 1); display.println("V");
  display.setCursor(0, 20); display.print("I="); display.print(data.current, 3); display.println("A");
  display.setCursor(0, 30); display.print("W="); display.print(data.power, 1); display.println("W");
  display.setCursor(0, 40); display.print("kWh="); display.print(data.energy, 3);
  display.setCursor(0, 50); display.print("R="); display.print(relayOn ? "ON" : "OFF"); display.print(" S="); display.print(sg90Angle); display.print(" "); display.println(status);
  display.display();
}

bool readPzem(EnergyData &data) {
  while (Serial2.available()) Serial2.read();
  const byte request[] = {0xF8, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x64, 0x64};
  Serial2.write(request, sizeof(request)); Serial2.flush(); delay(100);
  size_t count = 0;
  while (Serial2.available() && count < PZEM_RESPONSE_SIZE) pzemBuffer[count++] = Serial2.read();
  if (count < PZEM_RESPONSE_SIZE) return false;

  uint16_t voltageRaw = (static_cast<uint16_t>(pzemBuffer[3]) << 8) | pzemBuffer[4];
  uint32_t currentRaw = (static_cast<uint32_t>(pzemBuffer[7]) << 24) | (static_cast<uint32_t>(pzemBuffer[8]) << 16) | (static_cast<uint32_t>(pzemBuffer[5]) << 8) | pzemBuffer[6];
  uint32_t powerRaw = (static_cast<uint32_t>(pzemBuffer[11]) << 24) | (static_cast<uint32_t>(pzemBuffer[12]) << 16) | (static_cast<uint32_t>(pzemBuffer[9]) << 8) | pzemBuffer[10];
  uint32_t energyRaw = (static_cast<uint32_t>(pzemBuffer[15]) << 24) | (static_cast<uint32_t>(pzemBuffer[16]) << 16) | (static_cast<uint32_t>(pzemBuffer[13]) << 8) | pzemBuffer[14];
  uint16_t frequencyRaw = (static_cast<uint16_t>(pzemBuffer[17]) << 8) | pzemBuffer[18];
  uint16_t powerFactorRaw = (static_cast<uint16_t>(pzemBuffer[19]) << 8) | pzemBuffer[20];
  data.voltage = voltageRaw * 0.1f; data.current = currentRaw * 0.001f; data.power = powerRaw * 0.1f;
  data.energy = energyRaw * 0.001f; data.frequency = frequencyRaw * 0.1f; data.powerFactor = powerFactorRaw * 0.01f; data.valid = true;
  return true;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000UL) delay(250);
}

bool connectMqtt() {
  if (WiFi.status() != WL_CONNECTED || mqttClient.connected()) return mqttClient.connected();
  uint64_t mac = ESP.getEfuseMac(); char clientId[32];
  snprintf(clientId, sizeof(clientId), "esp32-energy-%04X%08X", static_cast<uint16_t>(mac >> 32), static_cast<uint32_t>(mac));
  if (!mqttClient.connect(clientId)) return false;
  mqttClient.subscribe(MQTT_CTRL_TOPIC);
  Serial.print("MQTT subscribed: "); Serial.println(MQTT_CTRL_TOPIC);
  return true;
}

bool publishEnergy(const EnergyData &data) {
  if (!connectMqtt()) return false;
  char payload[96];
  snprintf(payload, sizeof(payload), "{\"V\":%.1f,\"I\":%.2f,\"W\":%.1f,\"kWh\":%.3f}", data.voltage, data.current, data.power, data.energy);
  bool ok = mqttClient.publish(MQTT_TOPIC, payload);
  Serial.println(payload);
  return ok;
}

void setup() {
  Serial.begin(115200); delay(300);
  pinMode(RELAY_PIN, OUTPUT); setRelay(true); // GPIO18 LOW by default.
  sg90.setPeriodHertz(50); sg90.attach(SG90_PIN, 500, 2400); sg90.write(sg90Angle);
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) while (true) delay(1000);
  display.setRotation(2);
  Serial2.begin(9600);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT); mqttClient.setCallback(mqttCallback);
  connectWiFi(); connectMqtt();
}

void loop() {
  if (mqttClient.connected()) mqttClient.loop(); else { connectWiFi(); connectMqtt(); }
  unsigned long now = millis();
  if (lastPublishMs == 0 || now - lastPublishMs >= MQTT_INTERVAL_MS) {
    lastPublishMs = now; EnergyData data = {0, 0, 0, 0, 0, 0, false};
    if (readPzem(data)) drawEnergy(data, publishEnergy(data) ? "OK" : "NO");
    else { Serial.println("PZEM read failed"); drawInvalid("NO"); }
  }
  delay(2);
}

