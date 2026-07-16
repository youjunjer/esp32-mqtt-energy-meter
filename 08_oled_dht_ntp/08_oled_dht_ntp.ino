#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <U8g2lib.h>
#include <SimpleDHT.h>

// 0.96" SSD1306
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
// 1.3" SH1106
// U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

constexpr int LIGHT_PIN = 33;
constexpr int DHT_PIN = 14;

constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";

SimpleDHT11 dht11(DHT_PIN);

int lastTemperature = 28;
int lastHumidity = 56;

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
  }
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000UL) {
    delay(250);
  }
}

void setupTime() {
  // Taiwan time zone: UTC+8
  setenv("TZ", "CST-8", 1);
  tzset();
  configTzTime("CST-8", "pool.ntp.org", "time.google.com", "time.nist.gov");
}

bool getCurrentTimeString(char *buffer, size_t length) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 2000)) {
    snprintf(buffer, length, "--/-- --:--:--");
    return false;
  }

  strftime(buffer, length, "%m/%d %H:%M:%S", &timeinfo);
  return true;
}

void setup() {
  analogReadResolution(12);
  pinMode(LIGHT_PIN, INPUT);

  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_unifont_t_chinese1);
  u8g2.setFontPosTop();

  connectWiFi();
  setupTime();
}

void loop() {
  int lightPercent = readLightPercent();
  readDht11();

  char timeText[20];
  getCurrentTimeString(timeText, sizeof(timeText));

  u8g2.clearBuffer();

  u8g2.setCursor(0, 5);
  u8g2.print(timeText);

  u8g2.setCursor(0, 25);
  u8g2.print("光敏值:");
  if (lightPercent < 100) {
    u8g2.print(" ");
  }
  if (lightPercent < 10) {
    u8g2.print(" ");
  }
  u8g2.print(lightPercent);
  u8g2.print(" %");

  u8g2.setCursor(0, 45);
  u8g2.print("溫濕度:");
  u8g2.print(lastTemperature);
  u8g2.print("C/");
  u8g2.print(lastHumidity);
  u8g2.print("%");

  u8g2.sendBuffer();
  delay(1000);
}

