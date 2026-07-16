#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;
constexpr int OLED_SDA = 21;
constexpr int OLED_SCL = 22;
constexpr uint8_t OLED_ADDRESS = 0x3C;

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);
  delay(300);

  // ESP32 I2C: SDA = GPIO21, SCL = GPIO22
  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    while (true) {
      delay(1000);
    }
  }

  // OLED is mounted upside down in the enclosure.
  display.setRotation(2);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(18, 22);
  display.println("HelloWorld");
  display.display();

  Serial.println("OLED HelloWorld ready");
  Serial.println("SDA=GPIO21, SCL=GPIO22, address=0x3C");
}

void loop() {
  // Keep the test message on screen.
}

