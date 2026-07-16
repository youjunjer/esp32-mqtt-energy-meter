# ESP32 智慧電表專案交接紀錄

## 一、開發環境

- 工作目錄：`D:\AIoT\esp32`
- 開發板：ESP32 Wrover Module
- 開發核心：`esp32:esp32:esp32wrover`
- 使用序列埠：`COM4`
- Arduino CLI：`C:\Program Files\Arduino CLI\arduino-cli.exe`
- ESP32 核心已安裝，板子可正常編譯、燒錄與重新啟動
- 目前主要程式：`20_oled_show/20_oled_show.ino`

## 二、目前主要專案：`20_oled_show`

目前程式整合以下功能：

- PZEM-004T 電力量測
- OLED 顯示
- DHT11 溫溼度感測
- 光敏電阻亮度感測
- Wi‑Fi 連線
- MQTT 資料發布
- MQTT 控制命令回呼
- GPIO18 繼電器控制
- GPIO5 SG90 伺服馬達控制
- OLED 頁面切換與命令提示畫面

最後一次狀態：

- 已成功編譯
- 已成功燒錄至 `COM4`
- Flash 寫入驗證成功
- 程式使用 Flash 約 63%，動態記憶體約 14%

## 三、硬體接線

### OLED

- 顯示器：SSD1306，128×64，I2C
- 程式庫：Adafruit GFX、Adafruit SSD1306
- I2C 位址：`0x3C`
- SDA：`GPIO21`
- SCL：`GPIO22`
- OLED 實際安裝方向上下顛倒，程式使用 `display.setRotation(2)` 修正

### PZEM-004T

- 使用 ESP32 `Serial2`
- PZEM TX → ESP32 `GPIO16`（RX）
- PZEM RX → ESP32 `GPIO17`（TX）
- 通訊設定：`Serial2.begin(9600)`
- 使用原始指令讀取，不依賴 PZEM 專用程式庫
- 回覆長度：25 位元組
- 讀取間隔：10 秒

PZEM 回覆欄位解析：

- 電壓：0.1 V
- 電流：0.001 A
- 功率：0.1 W
- 累計電量：0.001 kWh
- 頻率：0.1 Hz
- 功率因子：0.01

### DHT11

- 資料腳：`GPIO14`
- 溫度與溼度每 10 秒讀取一次
- 讀取失敗時保留上一次有效數值

### 光敏電阻

- 類比輸入：`GPIO33`
- ADC 範圍：0～4095
- 顯示與發布數值轉換為 0～100
- 目前採反向換算：越亮數值越高

### 繼電器

- 控制腳：`GPIO18`
- 低電位觸發
- `GPIO18=LOW`：繼電器開啟
- `GPIO18=HIGH`：繼電器關閉
- 開機預設：`LOW`，也就是繼電器開啟

### SG90

- 訊號腳：`GPIO5`
- PWM 頻率：50 Hz
- 角度範圍：0～180 度
- 開機預設角度：0 度
- 超出 0～180 的控制命令會忽略

## 四、MQTT 設定

- 代理伺服器：`mqttgo.io`
- 埠號：`1883`
- Wi‑Fi SSID：請依本機環境設定（實際憑證未納入版本庫）
- 資料發布主題：`eric1030/class702/data`
- 控制訂閱主題：`eric1030/class702/ctrl`
- MQTT 已使用 `setCallback(mqttCallback)` 設定回呼函式
- 主迴圈會持續執行 `mqttClient.loop()`，控制命令可即時處理

### 資料發布格式

每 10 秒發布一次，格式如下：

```json
{"V":112.3,"I":0.05,"W":0.5,"kWh":1.2,"temp":25,"humi":58,"light":85}
```

欄位說明：

- `V`：電壓
- `I`：電流
- `W`：功率
- `kWh`：累計電量
- `temp`：溫度
- `humi`：溼度
- `light`：亮度百分比

### 繼電器控制命令

發布至 `eric1030/class702/ctrl`：

```json
{"RELAY":"ON"}
```

結果：`GPIO18=LOW`，繼電器開啟。

```json
{"RELAY":"OFF"}
```

結果：`GPIO18=HIGH`，繼電器關閉。

### SG90 控制命令

發布至 `eric1030/class702/ctrl`：

```json
{"SG90":180}
```

伺服馬達轉到 180 度。

```json
{"SG90":35}
```

伺服馬達轉到 35 度。

只有 0～180 的整數角度會執行，其他數值忽略。

## 五、OLED 顯示設計

### 開機畫面

1. 顯示 `System Starting...`
2. 顯示 `WiFi connecting...`
3. Wi‑Fi 連線成功後顯示 IP 位址

### 頂端狀態列

- 左側：Wi‑Fi 狀態圖示與連線狀態
- 中間：月／日與時間，格式為 `07/16 11:52`
- 右側：MQTT 狀態圖示
- 目前已修正頂端狀態列文字重疊問題

### 用電頁

- 閃電圖示與 `POWER` 標題
- 功率使用大字體置中顯示，例如 `16.8W`
- 下方小字顯示：
  - `V`：電壓
  - `I`：電流
  - `E`：累計電量

### 環境頁

- 溫度計圖示與 `ENVIRONMENT` 標題
- 溫度使用大字體置中顯示，例如 `25 C`
- 下方小字顯示：
  - `H`：溼度
  - `L`：亮度

### 頁面切換

- 用電頁與環境頁每 5 秒自動切換
- 收到繼電器或 SG90 控制命令時，顯示實際動作 3 秒
- 命令畫面不顯示原始 JSON，而是顯示口語化結果，例如：
  - `Relay ON`
  - `Relay OFF`
  - `SG90 moved`
  - `Servo 180 degree`
  - `SG90 ignored`

## 六、已完成的歷史測試程式

- `01_led`：內建 LED 閃爍測試
- `02_ws2812`：GPIO32 的 WS2812 紅綠藍測試
- `03_RGLed`：GPIO4、GPIO2、GPIO15 紅黃綠交通燈
- `04_PIR`：GPIO14 人體感測器與紅綠燈
- `05_oled`：OLED 中文顯示測試
- `06_oled_photo`：OLED 與光敏電阻
- `07_oled_photo_dht`：OLED、光敏電阻與 DHT11
- `08_oled_dht_ntp`：DHT11 與網路時間
- `09_oled_npt_page`：多頁環境與空氣品質顯示
- `10_mqtt_oled`：MQTT 感測資料與 LED 遠端控制
- `11_mqttgoio`：MQTT 感測資料發布與控制
- `12_camera`：ESP32-CAM 網頁相機測試
- `13_mqtt`：ESP32-CAM MQTT Base64 影像發布
- `14_ENERGY_OLED`：Adafruit OLED HelloWorld 測試
- `15_ENERGY`：PZEM-004T 與 OLED 電力顯示
- `16_energy_mqtt`：PZEM 電力資料 JSON 發布
- `17_energy_mqtt_relay`：PZEM、MQTT 與繼電器
- `18_energy_mqtt_relay_sg90`：PZEM、繼電器與 SG90
- `19_energy_mqtt_relay_sg90_sensor`：再加入 DHT11 與光敏電阻
- `20_oled_show`：目前整合與美化後的 OLED 顯示版本

## 七、相機專案狀態

- 相機硬體已移除
- `12_camera` 與 `13_mqtt` 僅保留歷史程式，不屬於目前智慧電表主線
- 後續請勿沿用 ESP32-CAM 的腳位配置

## 八、目前待辦事項

1. Wi‑Fi 左上角目前使用程式內的線條圖示；使用者希望改用附圖風格的既有 Wi‑Fi 圖示，並依 RSSI 訊號強度切換。Adafruit GFX 目前沒有內建 Wi‑Fi 圖示，後續需選定可用的現成點陣圖或圖示字型後再整合。
2. 若需要更準確的光敏百分比，應依實際光線環境重新校正 ADC 的 0 與 4095 對應值。
3. 若 PZEM 讀取失敗，應繼續確認 RX、TX、共地與模組供電；目前 OLED 會顯示無效狀態。
4. SG90 建議使用獨立 5V 電源，並與 ESP32 共地，避免伺服馬達啟動造成 ESP32 重置。
5. PZEM 交流市電端具有危險性，接線與測試時必須斷電並做好絕緣。

## 九、重新燒錄指令

在工作目錄 `D:\AIoT\esp32` 執行：

```text
arduino-cli compile --fqbn esp32:esp32:esp32wrover 20_oled_show
arduino-cli upload -p COM4 --fqbn esp32:esp32:esp32wrover 20_oled_show
```

目前正式主程式檔案：

`D:\AIoT\esp32\20_oled_show\20_oled_show.ino`

最後更新日期：2026-07-16
