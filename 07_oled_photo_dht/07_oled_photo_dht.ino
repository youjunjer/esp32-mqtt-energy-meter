/*
  OLED + 光敏 + DHT11 範例
  - 光敏接在 IO33
  - DHT11 接在 IO14
  - 使用 U8g2 顯示中文
*/

#include <Wire.h>
#include <U8g2lib.h>
#include <SimpleDHT.h>

// 0.96 吋 SSD1306
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
// 1.3 吋 SH1106 若需要可改用這行
// U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

constexpr int LIGHT_PIN = 33;
constexpr int DHT_PIN = 14;

SimpleDHT11 dht11(DHT_PIN);  // 如果你接的是 DHT22，改成 SimpleDHT22

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

void setup() {
  analogReadResolution(12);
  pinMode(LIGHT_PIN, INPUT);

  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_unifont_t_chinese1);
  u8g2.setFontPosTop();
}

void loop() {
  int lightPercent = readLightPercent();
  readDht11();

  u8g2.clearBuffer();

  u8g2.setCursor(0, 5);
  u8g2.print("2027秀工公民營");

  u8g2.setCursor(0, 25);
  u8g2.print("亮度：");
  if (lightPercent < 100) {
    u8g2.print(" ");
  }
  if (lightPercent < 10) {
    u8g2.print(" ");
  }
  u8g2.print(lightPercent);
  u8g2.print(" %");

  u8g2.setCursor(0, 45);
  u8g2.print("溫溼度：");
  u8g2.print(lastTemperature);
  u8g2.print("C/");
  u8g2.print(lastHumidity);
  u8g2.print("%");

  u8g2.sendBuffer();
  delay(1000);
}

