#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <U8g2lib.h>
#include <SimpleDHT.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

constexpr int LIGHT_PIN = 33;
constexpr int DHT_PIN = 14;
constexpr int PAGE_BUTTON_PIN = 0;
constexpr int TAINAN_SITE_ID = 46;

constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
constexpr char AQI_API_URL[] =
    "https://data.moenv.gov.tw/api/v2/aqx_p_432?api_key=e75b1660-e564-4107-aad5-a8be1f905dd9&limit=100&format=JSON";

constexpr unsigned long BUTTON_DEBOUNCE_MS = 200;
constexpr unsigned long AQI_REFRESH_MS = 10UL * 60UL * 1000UL;

SimpleDHT11 dht11(DHT_PIN);

int lastTemperature = 28;
int lastHumidity = 56;
int lastLightPercent = 0;

int tainanAqi = 0;
int tainanPm25 = 0;
char tainanSiteName[24] = "Tainan";
char tainanPublishTime[20] = "--/-- --:--";

int currentPage = 0;
int lastButtonReading = HIGH;
int stableButtonState = HIGH;
unsigned long lastButtonChangeMs = 0;
unsigned long lastAqiFetchMs = 0;

bool getJsonStringField(const String &text, const char *key, String &out) {
  String needle = String("\"") + key + "\"";
  int keyIndex = text.indexOf(needle);
  if (keyIndex < 0) {
    return false;
  }

  int colonIndex = text.indexOf(':', keyIndex + needle.length());
  if (colonIndex < 0) {
    return false;
  }

  int valueStart = colonIndex + 1;
  while (valueStart < text.length() && isspace(static_cast<unsigned char>(text[valueStart]))) {
    valueStart++;
  }

  if (valueStart >= text.length() || text[valueStart] != '"') {
    return false;
  }

  valueStart++;
  int valueEnd = text.indexOf('"', valueStart);
  if (valueEnd < 0) {
    return false;
  }

  out = text.substring(valueStart, valueEnd);
  return true;
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
  }
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi connecting");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000UL) {
    Serial.print(".");
    delay(250);
  }
  Serial.println();
  Serial.print("WiFi status: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "connected" : "failed");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  }
}

void setupTime() {
  setenv("TZ", "CST-8", 1);
  tzset();
  configTzTime("CST-8", "pool.ntp.org", "time.google.com", "time.nist.gov");
}

bool fetchTainanAirQuality() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("AQI fetch skipped: WiFi not connected");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, AQI_API_URL)) {
    Serial.println("AQI http.begin failed");
    return false;
  }

  int statusCode = http.GET();
  Serial.print("AQI HTTP code: ");
  Serial.println(statusCode);
  if (statusCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  String payload = http.getString();
  Serial.print("AQI payload length: ");
  Serial.println(payload.length());
  http.end();

  int start = 0;
  while (true) {
    int objStart = payload.indexOf('{', start);
    if (objStart < 0) {
      break;
    }
    int objEnd = payload.indexOf('}', objStart);
    if (objEnd < 0) {
      break;
    }

    String objectText = payload.substring(objStart, objEnd + 1);

    String siteIdText;
    if (getJsonStringField(objectText, "siteid", siteIdText) && siteIdText.toInt() == TAINAN_SITE_ID) {
      String aqiText;
      String pmText;
      String publishText;

      if (getJsonStringField(objectText, "aqi", aqiText)) {
        tainanAqi = aqiText.toInt();
      }
      if (getJsonStringField(objectText, "pm2.5", pmText)) {
        tainanPm25 = pmText.toInt();
      }
      if (getJsonStringField(objectText, "publishtime", publishText)) {
        publishText.toCharArray(tainanPublishTime, sizeof(tainanPublishTime));
      }

      strncpy(tainanSiteName, "Tainan", sizeof(tainanSiteName));
      tainanSiteName[sizeof(tainanSiteName) - 1] = '\0';

      Serial.print("AQI parsed site=");
      Serial.print(tainanSiteName);
      Serial.print(" siteid=");
      Serial.print(siteIdText);
      Serial.print(" aqi=");
      Serial.print(tainanAqi);
      Serial.print(" pm2.5=");
      Serial.print(tainanPm25);
      Serial.print(" publish=");
      Serial.println(tainanPublishTime);
      return true;
    }

    start = objEnd + 1;
  }

  Serial.println("AQI target site not found in JSON array");
  return false;
}

bool getCurrentDateString(char *buffer, size_t length) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 2000)) {
    snprintf(buffer, length, "----/--/--");
    return false;
  }
  strftime(buffer, length, "%Y/%m/%d", &timeinfo);
  return true;
}

bool getCurrentTimeString(char *buffer, size_t length) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 2000)) {
    snprintf(buffer, length, "--:--:--");
    return false;
  }
  strftime(buffer, length, "%H:%M:%S", &timeinfo);
  return true;
}

void updatePageButton() {
  int reading = digitalRead(PAGE_BUTTON_PIN);
  if (reading != lastButtonReading) {
    lastButtonChangeMs = millis();
    lastButtonReading = reading;
  }
  if (millis() - lastButtonChangeMs < BUTTON_DEBOUNCE_MS) {
    return;
  }
  if (reading != stableButtonState) {
    stableButtonState = reading;
    if (stableButtonState == LOW) {
      currentPage = (currentPage + 1) % 5;
    }
  }
}

void drawPage1(const char *dateText, const char *timeText) {
  u8g2.setCursor(0, 0);
  u8g2.print("2026 Event");
  u8g2.setCursor(0, 20);
  u8g2.print("  ");
  u8g2.print(dateText);
  u8g2.setCursor(0, 40);
  u8g2.print("  ");
  u8g2.print(timeText);
}

void drawPage2() {
  // Thermometer icon.
  u8g2.drawFrame(4, 10, 6, 28);
  u8g2.drawDisc(7, 39, 7);
  u8g2.drawVLine(7, 15, 16);
  u8g2.drawHLine(12, 16, 6);
  u8g2.drawHLine(12, 22, 4);
  u8g2.drawHLine(12, 28, 6);
  u8g2.drawHLine(12, 34, 4);

  u8g2.setCursor(28, 0);
  u8g2.print("Temp");
  u8g2.setCursor(28, 22);
  u8g2.print(lastTemperature);
  u8g2.print(" C");
}

void drawPage3() {
  // Water droplet icon.
  u8g2.drawCircle(8, 19, 7);
  u8g2.drawLine(8, 12, 3, 21);
  u8g2.drawLine(8, 12, 13, 21);
  u8g2.drawLine(3, 21, 8, 31);
  u8g2.drawLine(13, 21, 8, 31);

  u8g2.setCursor(28, 0);
  u8g2.print("Humidity");
  u8g2.setCursor(28, 22);
  u8g2.print(lastHumidity);
  u8g2.print(" %");
}

void drawPage4() {
  // Sun icon.
  u8g2.drawDisc(8, 22, 5);
  u8g2.drawLine(8, 10, 8, 4);
  u8g2.drawLine(8, 40, 8, 34);
  u8g2.drawLine(0, 22, 4, 22);
  u8g2.drawLine(12, 22, 16, 22);
  u8g2.drawLine(3, 17, 0, 14);
  u8g2.drawLine(13, 17, 16, 14);
  u8g2.drawLine(3, 27, 0, 30);
  u8g2.drawLine(13, 27, 16, 30);

  u8g2.setCursor(28, 0);
  u8g2.print("Light");
  u8g2.setCursor(28, 22);
  u8g2.print(lastLightPercent);
  u8g2.print(" %");
}

void drawPage5() {
  u8g2.setCursor(0, 0);
  u8g2.print("Air Quality");
  u8g2.setCursor(0, 18);
  u8g2.print("Station: ");
  u8g2.print(tainanSiteName);
  u8g2.setCursor(0, 34);
  u8g2.print("PM2.5: ");
  u8g2.print(tainanPm25);
  u8g2.print(" ug");
  u8g2.setCursor(0, 50);
  u8g2.print("AQI: ");
  u8g2.print(tainanAqi);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("Boot");

  analogReadResolution(12);
  pinMode(LIGHT_PIN, INPUT);
  pinMode(PAGE_BUTTON_PIN, INPUT_PULLUP);

  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_unifont_t_chinese1);
  u8g2.setFontPosTop();

  connectWiFi();
  setupTime();
  fetchTainanAirQuality();
  lastAqiFetchMs = millis();
}

void loop() {
  updatePageButton();
  lastLightPercent = readLightPercent();
  readDht11();

  if (millis() - lastAqiFetchMs >= AQI_REFRESH_MS) {
    if (fetchTainanAirQuality()) {
      lastAqiFetchMs = millis();
    }
  }

  char dateText[16];
  char timeText[16];
  getCurrentDateString(dateText, sizeof(dateText));
  getCurrentTimeString(timeText, sizeof(timeText));

  u8g2.clearBuffer();
  switch (currentPage) {
    case 0:
      drawPage1(dateText, timeText);
      break;
    case 1:
      drawPage2();
      break;
    case 2:
      drawPage3();
      break;
    case 3:
      drawPage4();
      break;
    case 4:
      drawPage5();
      break;
  }
  u8g2.sendBuffer();
  delay(100);
}

