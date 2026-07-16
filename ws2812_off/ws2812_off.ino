#include <Adafruit_NeoPixel.h>

#define WS2812_PIN 32
#define WS2812_COUNT 1

Adafruit_NeoPixel strip(WS2812_COUNT, WS2812_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  strip.begin();
  strip.setBrightness(0);
  strip.clear();
  strip.show();
}

void loop() {
  delay(1000);
}

