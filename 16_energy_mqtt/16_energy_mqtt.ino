#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;
constexpr int OLED_SDA = 21;
constexpr int OLED_SCL = 22;
constexpr uint8_t OLED_ADDRESS = 0x3C;

constexpr int PZEM_RX = 16;
constexpr int PZEM_TX = 17;
constexpr size_t PZEM_RESPONSE_SIZE = 25;

constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
constexpr char MQTT_HOST[] = "mqttgo.io";
constexpr uint16_t MQTT_PORT = 1883;
constexpr char MQTT_TOPIC[] = "eric1030/class701/data";

constexpr unsigned long MQTT_INTERVAL_MS = 10000UL;

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
byte pzemBuffer[PZEM_RESPONSE_SIZE];

struct EnergyData {
  float voltage;
  float current;
  float power;
  float energy;
  float frequency;
  float powerFactor;
  bool valid;
};

void drawInvalid(const char *status) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  // First row intentionally left blank.
  display.setCursor(0, 10); display.println("V=-V");
  display.setCursor(0, 20); display.println("I=-A");
  display.setCursor(0, 30); display.println("W=-W");
  display.setCursor(0, 40); display.println("kWh=-");
  display.setCursor(0, 50); display.print("Hz=- PF=- "); display.println(status);
  display.display();
}

void drawEnergy(const EnergyData &data, const char *status) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  // First row intentionally left blank.
  display.setCursor(0, 10); display.print("V="); display.print(data.voltage, 1); display.println("V");
  display.setCursor(0, 20); display.print("I="); display.print(data.current, 3); display.println("A");
  display.setCursor(0, 30); display.print("W="); display.print(data.power, 1); display.println("W");
  display.setCursor(0, 40); display.print("kWh="); display.print(data.energy, 3);
  display.setCursor(0, 50); display.print("Hz="); display.print(data.frequency, 1);
  display.print(" PF="); display.print(data.powerFactor, 2); display.print(" "); display.println(status);
  display.display();
}

bool readPzem(EnergyData &data) {
  while (Serial2.available()) Serial2.read();

  const byte request[] = {0xF8, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x64, 0x64};
  Serial2.write(request, sizeof(request));
  Serial2.flush();
  delay(100);

  size_t count = 0;
  while (Serial2.available() && count < PZEM_RESPONSE_SIZE) {
    pzemBuffer[count++] = Serial2.read();
  }
  if (count < PZEM_RESPONSE_SIZE) return false;

  uint16_t voltageRaw = (static_cast<uint16_t>(pzemBuffer[3]) << 8) | pzemBuffer[4];
  uint32_t currentRaw = (static_cast<uint32_t>(pzemBuffer[7]) << 24) |
                        (static_cast<uint32_t>(pzemBuffer[8]) << 16) |
                        (static_cast<uint32_t>(pzemBuffer[5]) << 8) | pzemBuffer[6];
  uint32_t powerRaw = (static_cast<uint32_t>(pzemBuffer[11]) << 24) |
                      (static_cast<uint32_t>(pzemBuffer[12]) << 16) |
                      (static_cast<uint32_t>(pzemBuffer[9]) << 8) | pzemBuffer[10];
  uint32_t energyRaw = (static_cast<uint32_t>(pzemBuffer[15]) << 24) |
                       (static_cast<uint32_t>(pzemBuffer[16]) << 16) |
                       (static_cast<uint32_t>(pzemBuffer[13]) << 8) | pzemBuffer[14];
  uint16_t frequencyRaw = (static_cast<uint16_t>(pzemBuffer[17]) << 8) | pzemBuffer[18];
  uint16_t powerFactorRaw = (static_cast<uint16_t>(pzemBuffer[19]) << 8) | pzemBuffer[20];

  data.voltage = voltageRaw * 0.1f;
  data.current = currentRaw * 0.001f;
  data.power = powerRaw * 0.1f;
  data.energy = energyRaw * 0.001f;
  data.frequency = frequencyRaw * 0.1f;
  data.powerFactor = powerFactorRaw * 0.01f;
  data.valid = true;
  return true;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("WiFi connecting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000UL) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi failed");
  }
}

bool connectMqtt() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (mqttClient.connected()) return true;

  uint64_t mac = ESP.getEfuseMac();
  char clientId[32];
  snprintf(clientId, sizeof(clientId), "esp32-energy-%04X%08X",
           static_cast<uint16_t>(mac >> 32), static_cast<uint32_t>(mac));
  bool connected = mqttClient.connect(clientId);
  Serial.println(connected ? "MQTT connected" : "MQTT connect failed");
  return connected;
}

bool publishEnergy(const EnergyData &data) {
  if (!connectMqtt()) return false;

  char payload[96];
  snprintf(payload, sizeof(payload),
           "{\"V\":%.1f,\"I\":%.2f,\"W\":%.1f,\"kWh\":%.3f}",
           data.voltage, data.current, data.power, data.energy);
  bool ok = mqttClient.publish(MQTT_TOPIC, payload);
  Serial.print(ok ? "MQTT: " : "MQTT publish failed: ");
  Serial.println(payload);
  return ok;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    while (true) delay(1000);
  }
  display.setRotation(2);

  // ESP32 Serial2 default pins: RX=GPIO16, TX=GPIO17.
  Serial2.begin(9600);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  connectWiFi();
}

void loop() {
  EnergyData data = {0, 0, 0, 0, 0, 0, false};

  if (readPzem(data)) {
    bool published = publishEnergy(data);
    mqttClient.loop();
    drawEnergy(data, published ? "OK" : "NO");
  } else {
    Serial.println("PZEM read failed");
    drawInvalid("NO");
  }

  delay(MQTT_INTERVAL_MS);
}

