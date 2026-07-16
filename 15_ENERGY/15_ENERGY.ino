#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;
constexpr int OLED_SDA = 21;
constexpr int OLED_SCL = 22;
constexpr uint8_t OLED_ADDRESS = 0x3C;

constexpr int PZEM_RX = 16;
constexpr int PZEM_TX = 17;
constexpr size_t PZEM_RESPONSE_SIZE = 25;

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
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

void showInvalidEnergy() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  // First row intentionally left blank.
  display.setCursor(0, 10);
  display.println("V=-V");
  display.setCursor(0, 20);
  display.println("I=-A");
  display.setCursor(0, 30);
  display.println("W=-W");
  display.setCursor(0, 40);
  display.println("kWh=-");
  display.setCursor(0, 50);
  display.println("Hz=- PF=-");
  display.display();
}

bool readPzem(EnergyData &data) {
  while (Serial2.available()) {
    Serial2.read();
  }

  // PZEM-004T request: read 10 registers from address 0.
  const byte request[] = {0xF8, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x64, 0x64};
  Serial2.write(request, sizeof(request));
  Serial2.flush();
  delay(100);

  size_t count = 0;
  while (Serial2.available() && count < PZEM_RESPONSE_SIZE) {
    pzemBuffer[count++] = Serial2.read();
  }

  if (count < PZEM_RESPONSE_SIZE) {
    return false;
  }

  // Data layout follows the supplied working example.
  uint16_t voltageRaw = (static_cast<uint16_t>(pzemBuffer[3]) << 8) | pzemBuffer[4];
  uint32_t currentRaw = (static_cast<uint32_t>(pzemBuffer[7]) << 24) |
                        (static_cast<uint32_t>(pzemBuffer[8]) << 16) |
                        (static_cast<uint32_t>(pzemBuffer[5]) << 8) |
                        pzemBuffer[6];
  uint32_t powerRaw = (static_cast<uint32_t>(pzemBuffer[11]) << 24) |
                      (static_cast<uint32_t>(pzemBuffer[12]) << 16) |
                      (static_cast<uint32_t>(pzemBuffer[9]) << 8) |
                      pzemBuffer[10];
  uint32_t energyRaw = (static_cast<uint32_t>(pzemBuffer[15]) << 24) |
                       (static_cast<uint32_t>(pzemBuffer[16]) << 16) |
                       (static_cast<uint32_t>(pzemBuffer[13]) << 8) |
                       pzemBuffer[14];
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

void printEnergy(const EnergyData &data) {
  Serial.print("V=");
  Serial.print(data.voltage, 1);
  Serial.print("V, I=");
  Serial.print(data.current, 2);
  Serial.print("A, W=");
  Serial.print(data.power, 1);
  Serial.print("W, kWh=");
  Serial.print(data.energy, 3);
  Serial.print(" kWh, Hz=");
  Serial.print(data.frequency, 1);
  Serial.print(", PF=");
  Serial.print(data.powerFactor, 2);
  Serial.println();
}

void drawEnergy(const EnergyData &data) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // First row intentionally left blank.
  display.setCursor(0, 10);
  display.print("V=");
  display.print(data.voltage, 1);
  display.println("V");

  display.setCursor(0, 20);
  display.print("I=");
  display.print(data.current, 3);
  display.println("A");

  display.setCursor(0, 30);
  display.print("W=");
  display.print(data.power, 1);
  display.println("W");

  display.setCursor(0, 40);
  display.print("kWh=");
  display.print(data.energy, 3);

  display.setCursor(0, 50);
  display.print("Hz=");
  display.print(data.frequency, 1);
  display.print(" PF=");
  display.print(data.powerFactor, 2);
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(300);

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
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("PZEM-004T starting...");
  display.display();

  // ESP32 Serial2 default pins: RX=GPIO16, TX=GPIO17.
  Serial2.begin(9600);
  Serial.println("PZEM-004T raw protocol ready");
  Serial.println("Serial2 baud=9600, RX=GPIO16, TX=GPIO17");
}

void loop() {
  EnergyData data = {0, 0, 0, 0, 0, 0, false};

  if (readPzem(data)) {
    printEnergy(data);
    drawEnergy(data);
  } else {
    Serial.println("PZEM read failed");
    showInvalidEnergy();
  }

  delay(10000);
}

