#include "esp_camera.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <SimpleDHT.h>
#include <Wire.h>
#include <U8g2lib.h>

// AI Thinker ESP32-CAM
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char *MQTT_HOST = "mqttgo.io";
const uint16_t MQTT_PORT = 1883;
const char *MQTT_TOPIC = "eric1030/class701/pic";
const char *MQTT_SENSOR_TOPIC = "eric1030/class701/data";
const int LIGHT_PIN = 33;
const int DHT_PIN = 27;

// OLED: SDA = GPIO13, SCL = GPIO14
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
SimpleDHT11 dht11(DHT_PIN);

unsigned long lastUploadMs = 0;
unsigned long lastSensorMs = 0;
unsigned long lastMqttAttemptMs = 0;
unsigned long imageCount = 0;
char ipText[17] = "0.0.0.0";
char statusText[20] = "Starting";

void showOLED() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_5x8_tr);
  oled.drawStr(0, 9, "ESP32-CAM MQTT");
  oled.drawStr(0, 21, "IP:");
  oled.drawStr(18, 21, ipText);
  oled.drawStr(0, 34, "TOPIC: /pic");
  oled.drawStr(0, 47, statusText);
  oled.setCursor(0, 60);
  oled.print("IMG: ");
  oled.print(imageCount);
  oled.sendBuffer();
}

void setStatus(const char *text) {
  strncpy(statusText, text, sizeof(statusText) - 1);
  statusText[sizeof(statusText) - 1] = '\0';
  showOLED();
}

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = psramFound() ? 2 : 1;

  if (!psramFound()) {
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.frame_size = FRAMESIZE_QQVGA;
  }

  return esp_camera_init(&config) == ESP_OK;
}

bool connectMQTT() {
  if (mqttClient.connected()) {
    return true;
  }

  uint64_t mac = ESP.getEfuseMac();
  char clientId[32];
  snprintf(clientId, sizeof(clientId), "esp32cam-%08X", (uint32_t)mac);

  setStatus("MQTT connecting");
  bool connected = mqttClient.connect(clientId);
  setStatus(connected ? "MQTT ready" : "MQTT failed");
  return connected;
}

static const char BASE64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *makeBase64(const uint8_t *data, size_t dataLen, size_t &encodedLen) {
  encodedLen = 4 * ((dataLen + 2) / 3);
  char *output = (char *)malloc(encodedLen + 1);
  if (output == nullptr) {
    encodedLen = 0;
    return nullptr;
  }

  size_t input = 0;
  size_t outputIndex = 0;
  while (input < dataLen) {
    size_t remaining = dataLen - input;
    uint32_t value = (uint32_t)data[input++] << 16;
    if (remaining > 1) value |= (uint32_t)data[input++] << 8;
    if (remaining > 2) value |= data[input++];

    output[outputIndex++] = BASE64_TABLE[(value >> 18) & 0x3F];
    output[outputIndex++] = BASE64_TABLE[(value >> 12) & 0x3F];
    output[outputIndex++] = remaining > 1 ? BASE64_TABLE[(value >> 6) & 0x3F] : '=';
    output[outputIndex++] = remaining > 2 ? BASE64_TABLE[value & 0x3F] : '=';
  }
  output[encodedLen] = '\0';
  return output;
}

bool publishImage() {
  if (!mqttClient.connected()) {
    return false;
  }

  camera_fb_t *frame = esp_camera_fb_get();
  if (frame == nullptr) {
    Serial.println("Camera capture failed");
    setStatus("Capture failed");
    return false;
  }

  size_t encodedLen = 0;
  char *encoded = makeBase64(frame->buf, frame->len, encodedLen);
  esp_camera_fb_return(frame);

  if (encoded == nullptr) {
    Serial.println("Base64 allocation failed");
    setStatus("Base64 failed");
    return false;
  }

  bool ok = mqttClient.beginPublish(MQTT_TOPIC, encodedLen, false);
  if (ok) {
    size_t written = mqttClient.write((const uint8_t *)encoded, encodedLen);
    ok = written == encodedLen && mqttClient.endPublish();
  }
  free(encoded);

  if (ok) {
    imageCount++;
    Serial.printf("MQTT image #%lu sent, Base64 bytes=%u\n", imageCount, (unsigned)encodedLen);
    setStatus("Image sent");
  } else {
    Serial.println("MQTT image publish failed");
    setStatus("Publish failed");
  }
  return ok;
}

void publishSensorData() {
  byte temperature = 0;
  byte humidity = 0;
  int dhtResult = dht11.read(&temperature, &humidity, nullptr);
  int lightRaw = analogRead(LIGHT_PIN);
  int lightPercent = constrain(map(lightRaw, 0, 4095, 100, 0), 0, 100);

  if (dhtResult != SimpleDHTErrSuccess) {
    Serial.printf("DHT read failed: %d\n", dhtResult);
    return;
  }

  char payload[96];
  snprintf(payload, sizeof(payload), "{\"temp\":%d,\"humi\":%d,\"light\":%d}",
           temperature, humidity, lightPercent);
  bool ok = mqttClient.publish(MQTT_SENSOR_TOPIC, payload);
  Serial.printf("Sensor MQTT %s: %s\n", ok ? "OK" : "FAIL", payload);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(13, 14);
  pinMode(LIGHT_PIN, INPUT);
  oled.begin();
  showOLED();

  setStatus("Camera init");
  if (!initCamera()) {
    Serial.println("Camera init failed");
    setStatus("Camera failed");
    return;
  }

  setStatus("WiFi connecting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  IPAddress ip = WiFi.localIP();
  snprintf(ipText, sizeof(ipText), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  Serial.print("WiFi IP: ");
  Serial.println(ipText);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(512);
  connectMQTT();
  lastUploadMs = millis() - 1000UL;
  lastSensorMs = millis() - 10000UL;
}

void loop() {
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    setStatus("WiFi disconnected");
    delay(1000);
    return;
  }

  if (!mqttClient.connected() && now - lastMqttAttemptMs >= 5000UL) {
    lastMqttAttemptMs = now;
    connectMQTT();
  }
  mqttClient.loop();

  if (mqttClient.connected() && now - lastUploadMs >= 1000UL) {
    lastUploadMs = now;
    publishImage();
  }

  if (mqttClient.connected() && now - lastSensorMs >= 10000UL) {
    lastSensorMs = now;
    publishSensorData();
  }
}

