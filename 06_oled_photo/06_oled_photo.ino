/*
  OLED + 光敏電阻範例
  - 光敏接在 IO33
  - 使用 U8g2 顯示中文
  - 溫溼度先保留佔位，之後可再接入感測器
*/

#include <Wire.h>
#include <U8g2lib.h>

// 0.96 吋 SSD1306
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
// 1.3 吋 SH1106 若需要可改用這行
// U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

constexpr int LIGHT_PIN = 33;

int readLightPercent() {
  long total = 0;
  const int samples = 10;

  for (int i = 0; i < samples; i++) {
    total += analogRead(LIGHT_PIN);
    delay(2);
  }

  int raw = total / samples;

  // 反向換算：越亮百分比越高，越暗百分比越低。
  int percent = map(raw, 0, 4095, 100, 0);
  return constrain(percent, 0, 100);
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
  u8g2.print("溫溼度：28C/56%");

  u8g2.sendBuffer();
  delay(1000);
}

