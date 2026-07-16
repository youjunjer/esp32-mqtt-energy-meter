# esp32-mqtt-energy-meter

ESP32 智慧電表與智慧物聯網整合專案。此 repository 收錄從基礎 ESP32、感測器與 OLED 測試，到 MQTT、Node-RED、環境監測、公開資訊、歷史影像測試，以及 PZEM-004T 智慧電表整合的程式與成果素材。

## 專案特色

- 以 ESP32 Wrover Module 為核心控制器。
- 使用 PZEM-004T 讀取電壓、電流、功率與累計電量。
- 使用 DHT11 與光敏電阻監測溫度、濕度與亮度。
- 以 SSD1306 OLED 顯示本地狀態、用電資訊與環境資訊。
- 透過 Wi-Fi 與 MQTT 發布感測資料。
- 透過 MQTT 控制 GPIO18 繼電器與 GPIO5 SG90 伺服馬達。
- 以 Node-RED 建立能源／環境儀表板、歷史曲線與控制介面。
- 收錄早期 ESP32-CAM、Base64／MQTT 影像與公開交通資訊整合測試。

## 目錄結構

| 路徑 | 說明 |
|---|---|
| `01_led` ～ `05_oled` | ESP32 GPIO、LED、WS2812、PIR 與 OLED 基礎測試 |
| `06_oled_photo` ～ `09_oled_npt_page` | 光敏電阻、DHT11、NTP 時間與多頁顯示 |
| `10_mqtt_oled` ～ `13_mqtt` | MQTT 感測資料、遠端控制與 ESP32-CAM 歷史測試 |
| `14_ENERGY_OLED` ～ `19_energy_mqtt_relay_sg90_sensor` | PZEM、MQTT、繼電器、SG90 與感測器逐步整合 |
| `20_oled_show` | 目前主要整合版本 |
| `U8g2` | OLED 相關程式庫與範例素材 |
| `結案報告/` | 結案報告圖片素材 |
| `結案報告.docx` | 本次專案結案報告 |
| `handoff.md` | 專案交接紀錄、腳位、Topic 與操作說明 |

## 主要版本：`20_oled_show`

此版本整合以下功能：

1. PZEM-004T 電力資料讀取。
2. DHT11 溫溼度與光敏電阻亮度感測。
3. SSD1306 OLED 用電／環境頁面與狀態列。
4. Wi-Fi 連線與 NTP 網路時間。
5. MQTT 感測資料發布與控制命令訂閱。
6. GPIO18 低電位觸發繼電器控制。
7. GPIO5 SG90 伺服馬達 0～180 度控制。

## 硬體接線摘要

| 模組 | ESP32 腳位／設定 |
|---|---|
| OLED SSD1306 | SDA GPIO21、SCL GPIO22、I2C `0x3C` |
| PZEM-004T | RX GPIO16、TX GPIO17、Serial2 9600 baud |
| DHT11 | GPIO14 |
| 光敏電阻 | GPIO33 ADC |
| 繼電器 | GPIO18，LOW 開啟、HIGH 關閉 |
| SG90 | GPIO5，PWM 50 Hz |

## MQTT 設定

- Broker：`mqttgo.io`
- Port：`1883`
- 資料發布：`eric1030/class702/data`
- 控制訂閱：`eric1030/class702/ctrl`

感測資料範例：

```json
{"V":112.3,"I":0.05,"W":0.5,"kWh":1.2,"temp":25,"humi":58,"light":85}
```

控制命令範例：

```json
{"RELAY":"ON"}
{"RELAY":"OFF"}
{"SG90":180}
```

## 編譯與燒錄

1. 安裝 Arduino CLI 與 ESP32 board core。
2. 複製 `20_oled_show/secrets.example.h` 為 `20_oled_show/secrets.h`，填入本機 Wi-Fi 設定。
3. 在 repository 根目錄執行：

```text
arduino-cli compile --fqbn esp32:esp32:esp32wrover 20_oled_show
arduino-cli upload -p COM4 --fqbn esp32:esp32:esp32wrover 20_oled_show
```

## 開發狀態

目前主要版本已完成編譯、燒錄與 Flash 驗證。後續可再改善 RSSI 分級 Wi-Fi 圖示、光敏電阻校正、PZEM 失敗提示、MQTT TLS／帳號驗證，以及 SG90 獨立供電與市電安全隔離。

## 安全注意事項

- 不要提交 `secrets.h` 或其他含有帳號密碼的檔案。
- MQTT 目前以 1883 埠進行功能驗證，正式部署應使用加密與權限控管。
- PZEM 交流市電端具有危險性，接線與測試必須斷電、絕緣，並由具備相關能力的人員執行。
- SG90 建議使用獨立 5V 電源，並與 ESP32 共地。

## License

本 repository 主要作為 ESP32 課程與專案成果保存用途；第三方程式庫請依其各自授權條款使用。
