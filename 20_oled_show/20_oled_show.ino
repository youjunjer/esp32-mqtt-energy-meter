#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <SimpleDHT.h>
#include <stdlib.h>
#include <time.h>
#include "secrets.h"

constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;
constexpr int OLED_SDA = 21;
constexpr int OLED_SCL = 22;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr int PZEM_RX = 16;
constexpr int PZEM_TX = 17;
constexpr int RELAY_PIN = 18;
constexpr int SG90_PIN = 5;
constexpr int DHT_PIN = 14;
constexpr int LIGHT_PIN = 33;
constexpr size_t PZEM_RESPONSE_SIZE = 25;

constexpr char MQTT_HOST[] = "mqttgo.io";
constexpr uint16_t MQTT_PORT = 1883;
constexpr char MQTT_TOPIC[] = "eric1030/class702/data";
constexpr char MQTT_CTRL_TOPIC[] = "eric1030/class702/ctrl";
constexpr unsigned long DATA_INTERVAL_MS = 10000UL;
constexpr unsigned long PAGE_INTERVAL_MS = 5000UL;
constexpr unsigned long COMMAND_DISPLAY_MS = 3000UL;

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Servo sg90;
SimpleDHT11 dht11(DHT_PIN);
byte pzemBuffer[PZEM_RESPONSE_SIZE];

bool relayOn = true;
int sg90Angle = 0;
int temperature = 0;
int humidity = 0;
int lightPercent = 0;
unsigned long lastDataMs = 0;
unsigned long commandUntilMs = 0;
char commandLine1[24] = "";
char commandLine2[24] = "";

struct EnergyData {
  float voltage, current, power, energy;
  bool valid;
};
EnergyData latestData = {0, 0, 0, 0, false};

void centerText(const char *text, int y, uint8_t size) {
  int width = strlen(text) * 6 * size;
  if (width > OLED_WIDTH) size = 1;
  width = strlen(text) * 6 * size;
  display.setTextSize(size);
  display.setCursor(max(0, (OLED_WIDTH - width) / 2), y);
  display.print(text);
}

void showStatus(const char *line1, const char *line2, uint8_t size1, uint8_t size2) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  centerText(line1, 4, size1);
  centerText(line2, 34, size2);
  display.display();
}

void setCommand(const char *line1, const char *line2) {
  strncpy(commandLine1, line1, sizeof(commandLine1) - 1);
  commandLine1[sizeof(commandLine1) - 1] = '\0';
  strncpy(commandLine2, line2, sizeof(commandLine2) - 1);
  commandLine2[sizeof(commandLine2) - 1] = '\0';
  commandUntilMs = millis() + COMMAND_DISPLAY_MS;
}

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
    setCommand("Relay ON", "GPIO18 LOW");
  } else if (message.indexOf("\"RELAY\":\"OFF\"") >= 0 || message.indexOf("\"RELAY\": \"OFF\"") >= 0) {
    setRelay(false);
    setCommand("Relay OFF", "GPIO18 HIGH");
  }

  int requestedAngle = 0;
  if (parseSg90Command(message, requestedAngle)) {
    sg90.write(requestedAngle);
    sg90Angle = requestedAngle;
    char line2[24];
    snprintf(line2, sizeof(line2), "Servo %d degree", sg90Angle);
    setCommand("SG90 moved", line2);
  } else if (message.indexOf("\"SG90\"") >= 0) {
    setCommand("SG90 ignored", "Range 0-180");
  }
}

void drawLightningIcon(int x, int y) {
  // White lightning bolt on the monochrome OLED.
  display.fillTriangle(x + 20, y, x + 4, y + 30, x + 20, y + 30, SSD1306_WHITE);
  display.fillTriangle(x + 20, y, x + 38, y, x + 25, y + 20, SSD1306_WHITE);
  display.fillTriangle(x + 20, y + 20, x + 40, y + 20, x + 8, y + 60, SSD1306_WHITE);
}

void drawBoltSmall(int x, int y) {
  display.fillTriangle(x + 7, y, x, y + 11, x + 6, y + 11, SSD1306_WHITE);
  display.fillTriangle(x + 6, y + 8, x + 14, y + 8, x + 3, y + 20, SSD1306_WHITE);
}

void drawCurrentIcon(int x, int y) {
  display.drawCircle(x + 8, y + 8, 6, SSD1306_WHITE);
  display.drawLine(x + 8, y + 2, x + 8, y + 14, SSD1306_WHITE);
  display.drawLine(x + 2, y + 8, x + 14, y + 8, SSD1306_WHITE);
}

void drawPowerIcon(int x, int y) {
  display.drawRect(x + 2, y + 2, 12, 16, SSD1306_WHITE);
  display.fillRect(x + 5, y + 6, 6, 8, SSD1306_WHITE);
  display.drawLine(x + 5, y, x + 11, y, SSD1306_WHITE);
}

void drawThermometerIcon(int x, int y) {
  display.drawRoundRect(x + 5, y, 6, 14, 3, SSD1306_WHITE);
  display.fillCircle(x + 8, y + 16, 5, SSD1306_WHITE);
  display.drawLine(x + 8, y + 3, x + 8, y + 14, SSD1306_WHITE);
}

void drawDropIcon(int x, int y) {
  display.drawCircle(x + 8, y + 10, 6, SSD1306_WHITE);
  display.drawTriangle(x + 8, y, x + 2, y + 10, x + 14, y + 10, SSD1306_WHITE);
}

void drawSunIcon(int x, int y) {
  display.drawCircle(x + 8, y + 9, 4, SSD1306_WHITE);
  display.drawLine(x + 8, y, x + 8, y + 3, SSD1306_WHITE);
  display.drawLine(x + 8, y + 15, x + 8, y + 18, SSD1306_WHITE);
  display.drawLine(x, y + 9, x + 3, y + 9, SSD1306_WHITE);
  display.drawLine(x + 13, y + 9, x + 16, y + 9, SSD1306_WHITE);
  display.drawLine(x + 2, y + 3, x + 4, y + 5, SSD1306_WHITE);
  display.drawLine(x + 12, y + 13, x + 14, y + 15, SSD1306_WHITE);
}

void drawWifiIcon(int x, int y, bool connected) {
  display.drawLine(x, y + 4, x + 8, y, SSD1306_WHITE);
  display.drawLine(x + 8, y, x + 16, y + 4, SSD1306_WHITE);
  display.drawLine(x + 3, y + 9, x + 8, y + 6, SSD1306_WHITE);
  display.drawLine(x + 8, y + 6, x + 13, y + 9, SSD1306_WHITE);
  if (connected) display.fillCircle(x + 8, y + 13, 2, SSD1306_WHITE);
  else display.drawCircle(x + 8, y + 13, 2, SSD1306_WHITE);
}

void drawMqttIcon(int x, int y, bool connected) {
  display.drawCircle(x + 3, y + 9, 3, SSD1306_WHITE);
  display.drawCircle(x + 12, y + 4, 3, SSD1306_WHITE);
  display.drawLine(x + 5, y + 8, x + 10, y + 5, SSD1306_WHITE);
  if (!connected) display.drawLine(x, y, x + 15, y + 15, SSD1306_WHITE);
}

void drawHeader() {
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  bool wifiConnected = WiFi.status() == WL_CONNECTED;
  bool mqttConnected = mqttClient.connected();
  drawWifiIcon(0, 0, wifiConnected);
  display.setCursor(17, 1);
  display.print(wifiConnected ? "OK" : "--");
  drawMqttIcon(111, 0, mqttConnected);

  char timeText[15] = "--/-- --:--";
  struct tm timeInfo;
  if (getLocalTime(&timeInfo, 5)) strftime(timeText, sizeof(timeText), "%m/%d %H:%M", &timeInfo);
  int width = strlen(timeText) * 6;
  display.setCursor((OLED_WIDTH - width) / 2, 1);
  display.print(timeText);
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
}

void drawCard(int x, int y, int w, int h) {
  display.drawRoundRect(x, y, w, h, 3, SSD1306_WHITE);
}

void readSensors() {
  byte temp = 0, humi = 0;
  if (dht11.read(&temp, &humi, nullptr) == SimpleDHTErrSuccess) {
    temperature = temp;
    humidity = humi;
  }
  long total = 0;
  for (int i = 0; i < 10; i++) {
    total += analogRead(LIGHT_PIN);
    delay(2);
  }
  lightPercent = constrain(map(total / 10, 0, 4095, 100, 0), 0, 100);
}

void drawPowerPage() {
  char line[24];
  display.clearDisplay();
  drawHeader();
  drawBoltSmall(4, 28);
  display.setTextSize(1);
  display.setCursor(21, 15);
  display.print("POWER");

  snprintf(line, sizeof(line), "%.1fW", latestData.power);
  int width = strlen(line) * 6 * 3;
  display.setTextSize(3);
  display.setCursor((OLED_WIDTH - width) / 2, 26);
  display.print(line);

  display.setTextSize(1);
  display.setCursor(5, 53);
  display.print("V "); display.print(latestData.voltage, 1);
  display.setCursor(48, 53);
  display.print("I "); display.print(latestData.current, 2);
  display.setCursor(91, 53);
  display.print("E "); display.print(latestData.energy, 2);
  display.display();
}

void drawEnvironmentPage() {
  char line[24];
  display.clearDisplay();
  drawHeader();
  drawThermometerIcon(4, 28);
  display.setTextSize(1);
  display.setCursor(21, 15);
  display.print("ENVIRONMENT");

  snprintf(line, sizeof(line), "%d C", temperature);
  int width = strlen(line) * 6 * 3;
  display.setTextSize(3);
  display.setCursor((OLED_WIDTH - width) / 2, 26);
  display.print(line);

  display.setTextSize(1);
  display.setCursor(12, 53);
  display.print("H "); display.print(humidity); display.print("%");
  display.setCursor(75, 53);
  display.print("L "); display.print(lightPercent); display.print("%");
  display.display();
}

void drawCommandPage() {
  display.clearDisplay();
  drawHeader();
  drawCard(1, 13, 126, 48);
  drawBoltSmall(8, 28);
  centerText(commandLine1, 24, 1);
  centerText(commandLine2, 42, 1);
  display.display();
}

void drawCurrentPage() {
  if (millis() < commandUntilMs) {
    drawCommandPage();
  } else if (((millis() / PAGE_INTERVAL_MS) % 2) == 0) {
    drawPowerPage();
  } else {
    drawEnvironmentPage();
  }
}

bool readPzem(EnergyData &data) {
  while (Serial2.available()) Serial2.read();
  const byte request[] = {0xF8, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x64, 0x64};
  Serial2.write(request, sizeof(request));
  Serial2.flush();
  delay(100);
  size_t count = 0;
  while (Serial2.available() && count < PZEM_RESPONSE_SIZE) pzemBuffer[count++] = Serial2.read();
  if (count < PZEM_RESPONSE_SIZE) return false;

  uint16_t voltageRaw = (static_cast<uint16_t>(pzemBuffer[3]) << 8) | pzemBuffer[4];
  uint32_t currentRaw = (static_cast<uint32_t>(pzemBuffer[7]) << 24) | (static_cast<uint32_t>(pzemBuffer[8]) << 16) | (static_cast<uint32_t>(pzemBuffer[5]) << 8) | pzemBuffer[6];
  uint32_t powerRaw = (static_cast<uint32_t>(pzemBuffer[11]) << 24) | (static_cast<uint32_t>(pzemBuffer[12]) << 16) | (static_cast<uint32_t>(pzemBuffer[9]) << 8) | pzemBuffer[10];
  uint32_t energyRaw = (static_cast<uint32_t>(pzemBuffer[15]) << 24) | (static_cast<uint32_t>(pzemBuffer[16]) << 16) | (static_cast<uint32_t>(pzemBuffer[13]) << 8) | pzemBuffer[14];
  data.voltage = voltageRaw * 0.1f;
  data.current = currentRaw * 0.001f;
  data.power = powerRaw * 0.1f;
  data.energy = energyRaw * 0.001f;
  data.valid = true;
  return true;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  showStatus("WiFi", "connecting...", 2, 2);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000UL) delay(250);
  if (WiFi.status() == WL_CONNECTED) {
    configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    char ip[20];
    WiFi.localIP().toString().toCharArray(ip, sizeof(ip));
    showStatus("IP:", ip, 2, 1);
    delay(1500);
  }
}

bool connectMqtt() {
  if (WiFi.status() != WL_CONNECTED || mqttClient.connected()) return mqttClient.connected();
  uint64_t mac = ESP.getEfuseMac();
  char clientId[32];
  snprintf(clientId, sizeof(clientId), "esp32-energy-%04X%08X", static_cast<uint16_t>(mac >> 32), static_cast<uint32_t>(mac));
  if (!mqttClient.connect(clientId)) return false;
  mqttClient.subscribe(MQTT_CTRL_TOPIC);
  return true;
}

bool publishEnergy(const EnergyData &data) {
  if (!connectMqtt()) return false;
  char payload[160];
  snprintf(payload, sizeof(payload), "{\"V\":%.1f,\"I\":%.2f,\"W\":%.1f,\"kWh\":%.3f,\"temp\":%d,\"humi\":%d,\"light\":%d}", data.voltage, data.current, data.power, data.energy, temperature, humidity, lightPercent);
  bool ok = mqttClient.publish(MQTT_TOPIC, payload);
  Serial.println(payload);
  return ok;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) while (true) delay(1000);
  display.setRotation(2);
  showStatus("System", "Starting...", 2, 2);
  delay(1200);

  pinMode(RELAY_PIN, OUTPUT);
  setRelay(true);
  pinMode(LIGHT_PIN, INPUT);
  sg90.setPeriodHertz(50);
  sg90.attach(SG90_PIN, 500, 2400);
  sg90.write(sg90Angle);
  Serial2.begin(9600);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  connectWiFi();
  connectMqtt();
}

void loop() {
  if (mqttClient.connected()) mqttClient.loop();
  else { connectWiFi(); connectMqtt(); }

  unsigned long now = millis();
  if (lastDataMs == 0 || now - lastDataMs >= DATA_INTERVAL_MS) {
    lastDataMs = now;
    readSensors();
    EnergyData data = {0, 0, 0, 0, false};
    if (readPzem(data)) {
      latestData = data;
      publishEnergy(data);
    }
  }

  drawCurrentPage();
  delay(100);
}

