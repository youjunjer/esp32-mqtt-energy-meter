#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <U8g2lib.h>
#include <SimpleDHT.h>
#include <string.h>

// Let's Encrypt ISRG Root X1
static const char MQTTGO_CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

// 0.96" SSD1306
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

constexpr int LIGHT_PIN = 33;
constexpr int DHT_PIN = 14;

constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
constexpr char MQTT_HOST[] = "mqttgo.vip";
constexpr uint16_t MQTT_PORT = 8883;
constexpr char MQTT_USER[] = "aaaaaa";
constexpr char MQTT_PASSWORD[] = "aVKv8kn07xTq";
constexpr char MQTT_TOPIC[] = "/user/aaaaaa/class701/data";
constexpr char MQTT_CTRL_TOPIC[] = "/user/aaaaaa/class701/ctrl";

constexpr int RED_PIN = 4;
constexpr int YELLOW_PIN = 2;
constexpr int GREEN_PIN = 15;

constexpr unsigned long SENSOR_PUBLISH_INTERVAL_MS = 10000UL;
constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 5000UL;
constexpr unsigned long MQTT_RETRY_INTERVAL_MS = 5000UL;
constexpr unsigned long OLED_REFRESH_INTERVAL_MS = 500UL;

SimpleDHT11 dht11(DHT_PIN);
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

int lastTemperature = 28;
int lastHumidity = 56;
int lastLightPercent = 0;

bool wifiReady = false;
bool mqttReady = false;
bool dhtReady = true;
bool lastPublishOk = false;
bool redLedOn = false;
bool yellowLedOn = false;
bool greenLedOn = false;

unsigned long lastPublishMs = 0;
unsigned long lastWifiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;
unsigned long lastOledRefreshMs = 0;

char currentState[32] = "BOOT";
char lastError[32] = "";
char ipText[16] = "--.--.--.--";
char mqttClientId[32] = "esp32-mqtt";

void setState(const char *state) {
  strncpy(currentState, state, sizeof(currentState));
  currentState[sizeof(currentState) - 1] = '\0';
}

void setError(const char *errorText) {
  strncpy(lastError, errorText, sizeof(lastError));
  lastError[sizeof(lastError) - 1] = '\0';
}

void applyLedOutputs() {
  digitalWrite(RED_PIN, redLedOn ? HIGH : LOW);
  digitalWrite(YELLOW_PIN, yellowLedOn ? HIGH : LOW);
  digitalWrite(GREEN_PIN, greenLedOn ? HIGH : LOW);
}

void setLedStates(bool redOn, bool yellowOn, bool greenOn) {
  redLedOn = redOn;
  yellowLedOn = yellowOn;
  greenLedOn = greenOn;
  applyLedOutputs();
}

bool parseOnOffValue(const String &payload, const char *key, bool &onValue) {
  String needle = String("\"") + key + "\"";
  int keyIndex = payload.indexOf(needle);
  if (keyIndex < 0) {
    return false;
  }

  int colonIndex = payload.indexOf(':', keyIndex + needle.length());
  if (colonIndex < 0) {
    return false;
  }

  int valueStart = colonIndex + 1;
  while (valueStart < payload.length() && isspace(static_cast<unsigned char>(payload[valueStart]))) {
    valueStart++;
  }

  if (valueStart < payload.length() && payload[valueStart] == '"') {
    valueStart++;
  }

  int valueEnd = valueStart;
  while (valueEnd < payload.length() && payload[valueEnd] != '"' && payload[valueEnd] != ',' && payload[valueEnd] != '}') {
    valueEnd++;
  }

  String value = payload.substring(valueStart, valueEnd);
  value.trim();
  value.toUpperCase();

  if (value == "ON") {
    onValue = true;
    return true;
  }
  if (value == "OFF") {
    onValue = false;
    return true;
  }
  return false;
}

void handleControlCommand(const char *payload, unsigned int length) {
  String message;
  message.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) {
    message += payload[i];
  }

  bool updated = false;
  bool newRed = redLedOn;
  bool newYellow = yellowLedOn;
  bool newGreen = greenLedOn;

  if (parseOnOffValue(message, "RLED", newRed)) {
    updated = true;
  }
  if (parseOnOffValue(message, "YLED", newYellow)) {
    updated = true;
  }
  if (parseOnOffValue(message, "GLED", newGreen)) {
    updated = true;
  }

  if (updated) {
    setLedStates(newRed, newYellow, newGreen);
    setState("CTRL OK");
    setError("");
    Serial.print("CTRL payload: ");
    Serial.println(message);
    Serial.print("LED state R/Y/G = ");
    Serial.print(redLedOn ? "1" : "0");
    Serial.print("/");
    Serial.print(yellowLedOn ? "1" : "0");
    Serial.print("/");
    Serial.println(greenLedOn ? "1" : "0");
  } else {
    setState("CTRL BAD");
    setError("CTRL BAD");
    Serial.print("CTRL parse failed: ");
    Serial.println(message);
  }
}

int readLightPercent() {
  long total = 0;
  const int samples = 10;

  for (int i = 0; i < samples; i++) {
    total += analogRead(LIGHT_PIN);
    delay(2);
  }

  int raw = total / samples;
  int percent = map(raw, 0, 4095, 100, 0);
  return constrain(percent, 0, 100);
}

void readDht11() {
  byte temperature = 0;
  byte humidity = 0;
  int err = dht11.read(&temperature, &humidity, NULL);
  if (err == SimpleDHTErrSuccess) {
    lastTemperature = temperature;
    lastHumidity = humidity;
    dhtReady = true;
    setError("");
  } else {
    dhtReady = false;
    setError("DHT ERR");
    Serial.print("DHT read failed: ");
    Serial.println(err);
  }
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiReady = true;
    return;
  }

  setState("WIFI CONNECT");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000UL) {
    delay(250);
  }

  wifiReady = (WiFi.status() == WL_CONNECTED);
  if (wifiReady) {
    IPAddress ip = WiFi.localIP();
    snprintf(ipText, sizeof(ipText), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    Serial.print("WiFi connected, IP: ");
    Serial.println(ipText);
    setState("WIFI OK");
    setError("");
  } else {
    strncpy(ipText, "--.--.--.--", sizeof(ipText));
    ipText[sizeof(ipText) - 1] = '\0';
    Serial.println("WiFi connect failed");
    setState("WIFI FAIL");
    setError("WIFI FAIL");
  }
}

void syncWiFiStatus() {
  wifiReady = (WiFi.status() == WL_CONNECTED);
  if (wifiReady) {
    IPAddress ip = WiFi.localIP();
    snprintf(ipText, sizeof(ipText), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  } else {
    strncpy(ipText, "--.--.--.--", sizeof(ipText));
    ipText[sizeof(ipText) - 1] = '\0';
  }
}

void configureMqtt() {
  secureClient.setInsecure();
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(128);
  mqttClient.setCallback([](char *topic, byte *payload, unsigned int length) {
    String topicText = topic;
    if (topicText == MQTT_CTRL_TOPIC) {
      handleControlCommand(reinterpret_cast<const char *>(payload), length);
    }
  });
}

void buildMqttClientId() {
  uint64_t mac = ESP.getEfuseMac();
  snprintf(
      mqttClientId, sizeof(mqttClientId), "esp32-%04X%08X",
      static_cast<uint16_t>(mac >> 32), static_cast<uint32_t>(mac));
}

bool connectMqtt() {
  if (!wifiReady) {
    mqttReady = false;
    return false;
  }

  if (mqttClient.connected()) {
    mqttReady = true;
    return true;
  }

  setState("MQTT CONNECT");
  if (mqttClient.connect(mqttClientId, MQTT_USER, MQTT_PASSWORD)) {
    mqttReady = true;
    setState("MQTT OK");
    setError("");
    Serial.println("MQTT connected");
    mqttClient.subscribe(MQTT_CTRL_TOPIC);
    Serial.print("MQTT subscribed: ");
    Serial.println(MQTT_CTRL_TOPIC);
    return true;
  }

  mqttReady = false;
  setState("MQTT FAIL");
  setError("MQTT FAIL");
  Serial.print("MQTT connect failed, state=");
  Serial.println(mqttClient.state());
  return false;
}

bool publishSensorData() {
  if (!wifiReady || !mqttReady) {
    lastPublishOk = false;
    return false;
  }

  readDht11();
  lastLightPercent = readLightPercent();

  char payload[96];
  snprintf(
      payload, sizeof(payload), "{\"temp\":%d,\"humi\":%d,\"light\":%d}",
      lastTemperature, lastHumidity, lastLightPercent);

  bool ok = mqttClient.publish(MQTT_TOPIC, payload);
  lastPublishOk = ok;

  if (ok) {
    setState("PUB OK");
    setError("");
    Serial.print("MQTT publish OK: ");
    Serial.println(payload);
  } else {
    setState("PUB FAIL");
    setError("PUB FAIL");
    Serial.println("MQTT publish failed");
  }

  return ok;
}

void drawWiFiIcon(int x, int y, bool ok) {
  u8g2.drawDisc(x + 2, y + 7, 1);
  u8g2.drawLine(x + 0, y + 5, x + 2, y + 3);
  u8g2.drawLine(x + 2, y + 3, x + 4, y + 5);
  u8g2.drawLine(x + 0, y + 2, x + 2, y + 0);
  u8g2.drawLine(x + 2, y + 0, x + 4, y + 2);
  if (ok) {
    u8g2.drawDisc(x + 2, y + 7, 1);
  } else {
    u8g2.drawLine(x + 0, y + 6, x + 4, y + 2);
    u8g2.drawLine(x + 4, y + 6, x + 0, y + 2);
  }
}

void drawMqttIcon(int x, int y, bool ok) {
  u8g2.drawCircle(x + 5, y + 7, 2);
  u8g2.drawCircle(x + 11, y + 4, 2);
  u8g2.drawCircle(x + 11, y + 10, 2);
  u8g2.drawLine(x + 7, y + 6, x + 9, y + 5);
  u8g2.drawLine(x + 7, y + 8, x + 9, y + 9);
  if (!ok) {
    u8g2.drawLine(x + 0, y + 1, x + 14, y + 13);
    u8g2.drawLine(x + 14, y + 1, x + 0, y + 13);
  }
}

void drawThermometerIcon(int x, int y) {
  u8g2.drawFrame(x + 3, y + 1, 5, 9);
  u8g2.drawDisc(x + 5, y + 11, 3);
  u8g2.drawVLine(x + 5, y + 2, 7);
}

void drawDropletIcon(int x, int y) {
  u8g2.drawCircle(x + 6, y + 7, 4);
  u8g2.drawLine(x + 6, y + 2, x + 2, y + 8);
  u8g2.drawLine(x + 6, y + 2, x + 10, y + 8);
}

void drawSunIcon(int x, int y) {
  u8g2.drawDisc(x + 6, y + 7, 3);
  u8g2.drawLine(x + 6, y + 0, x + 6, y + 2);
  u8g2.drawLine(x + 6, y + 12, x + 6, y + 14);
  u8g2.drawLine(x + 0, y + 7, x + 2, y + 7);
  u8g2.drawLine(x + 10, y + 7, x + 12, y + 7);
  u8g2.drawLine(x + 2, y + 3, x + 1, y + 1);
  u8g2.drawLine(x + 10, y + 3, x + 11, y + 1);
  u8g2.drawLine(x + 2, y + 11, x + 1, y + 13);
  u8g2.drawLine(x + 10, y + 11, x + 11, y + 13);
}

void drawLedDot(int x, int y, bool on) {
  if (on) {
    u8g2.drawDisc(x, y, 2);
  } else {
    u8g2.drawCircle(x, y, 2);
  }
}

void drawStatusBar() {
  const int barH = 11;
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, barH);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setFontPosTop();

  drawWiFiIcon(1, 0, wifiReady);
  u8g2.setCursor(17, 1);
  u8g2.print(wifiReady ? "OK" : "NO");

  drawMqttIcon(40, 0, mqttReady);
  u8g2.setCursor(57, 1);
  u8g2.print(mqttReady ? "OK" : "NO");

  const char *stateText = currentState;
  int stateX = 76;
  int maxChars = 7;
  char shortState[10];
  int len = strlen(stateText);
  if (len <= maxChars) {
    strncpy(shortState, stateText, sizeof(shortState));
    shortState[sizeof(shortState) - 1] = '\0';
  } else {
    strncpy(shortState, stateText, 4);
    shortState[4] = '.';
    shortState[5] = '.';
    shortState[6] = '\0';
  }
  u8g2.setCursor(stateX, 1);
  u8g2.print(shortState);

  u8g2.setDrawColor(1);
}

void drawMetricCard(int x, int y, int w, int h, const char *label, int value, const char *unit, void (*iconFn)(int, int)) {
  u8g2.drawRFrame(x, y, w, h, 3);
  iconFn(x + 1, y + 1);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setFontPosTop();
  u8g2.setCursor(x + 17, y + 2);
  u8g2.print(label);

  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.setFontPosTop();
  u8g2.setCursor(x + 17, y + 10);
  u8g2.print(value);
  u8g2.print(unit);
}

void drawLightCard(int x, int y, int w, int h, int value) {
  u8g2.drawRFrame(x, y, w, h, 3);
  drawSunIcon(x + 1, y + 2);

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setFontPosTop();
  u8g2.setCursor(x + 17, y + 3);
  u8g2.print("LIGHT");

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setFontPosTop();
  u8g2.setCursor(x + 17, y + 11);
  u8g2.print(value);
  u8g2.print("%");
}

void drawFooter() {
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setFontPosTop();
  drawLedDot(4, 58, redLedOn);
  drawLedDot(18, 58, yellowLedOn);
  drawLedDot(32, 58, greenLedOn);
  u8g2.setCursor(40, 56);
  u8g2.print("R");
  u8g2.print(redLedOn ? "1" : "0");
  u8g2.print(" Y");
  u8g2.print(yellowLedOn ? "1" : "0");
  u8g2.print(" G");
  u8g2.print(greenLedOn ? "1" : "0");
  if (lastError[0] != '\0') {
    u8g2.setCursor(76, 56);
    u8g2.print("ERR ");
    u8g2.print(lastError);
  } else {
    u8g2.setCursor(76, 56);
    u8g2.print("TX10s");
    u8g2.setCursor(106, 56);
    u8g2.print(lastPublishOk ? "PUB OK" : "WAIT");
  }
}

void drawDashboard() {
  drawStatusBar();

  drawMetricCard(1, 13, 62, 22, "TEMP", lastTemperature, "C", drawThermometerIcon);
  drawMetricCard(65, 13, 62, 22, "HUMI", lastHumidity, "%", drawDropletIcon);
  drawLightCard(1, 36, 126, 18, lastLightPercent);

  drawFooter();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("Boot");

  analogReadResolution(12);
  pinMode(LIGHT_PIN, INPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  setLedStates(false, false, false);

  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.setFontPosTop();

  buildMqttClientId();
  connectWiFi();
  configureMqtt();
  connectMqtt();

  lastPublishMs = millis() - SENSOR_PUBLISH_INTERVAL_MS;
  lastOledRefreshMs = millis();
}

void loop() {
  unsigned long now = millis();

  syncWiFiStatus();
  if (!wifiReady && now - lastWifiAttemptMs >= WIFI_RETRY_INTERVAL_MS) {
    lastWifiAttemptMs = now;
    connectWiFi();
  }

  if (wifiReady && !mqttClient.connected() && now - lastMqttAttemptMs >= MQTT_RETRY_INTERVAL_MS) {
    lastMqttAttemptMs = now;
    connectMqtt();
  }

  if (mqttClient.connected()) {
    mqttClient.loop();
    mqttReady = true;
  } else {
    mqttReady = false;
  }

  if (now - lastPublishMs >= SENSOR_PUBLISH_INTERVAL_MS) {
    lastPublishMs = now;
    if (wifiReady && mqttClient.connected()) {
      publishSensorData();
    } else {
      setState(wifiReady ? "MQTT WAIT" : "WIFI WAIT");
      setError(wifiReady ? "MQTT WAIT" : "WIFI WAIT");
      lastPublishOk = false;
    }
  }

  if (now - lastOledRefreshMs >= OLED_REFRESH_INTERVAL_MS) {
    lastOledRefreshMs = now;
    u8g2.clearBuffer();
    drawDashboard();
    u8g2.sendBuffer();
  }
}

