#include <Adafruit_NeoPixel.h>

#define WS2812_PIN 32
#define WS2812_COUNT 1

Adafruit_NeoPixel strip(WS2812_COUNT, WS2812_PIN, NEO_GRB + NEO_KHZ800);

void showColor(uint8_t r, uint8_t g, uint8_t b, uint16_t onMs, uint16_t offMs) {
  strip.setPixelColor(0, strip.Color(r, g, b));
  strip.show();
  delay(onMs);

  strip.setPixelColor(0, 0);
  strip.show();
  delay(offMs);
}

void setup() {
  strip.begin();
  strip.setBrightness(32);
  strip.clear();
  strip.show();
}

void loop() {
  showColor(255, 0, 0, 500, 200);
  showColor(0, 255, 0, 500, 200);
  showColor(0, 0, 255, 500, 500);
}

