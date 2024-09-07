# コンポーネント一覧

# ESP32Core（0）
※ESP32CoreではなくMainコンポーネントのidが0

# RP2040Core (0)
※RP2040CoreではなくMainコンポーネントのidが0

# LiPoPower (1)

1セルのLiPoを充放電する．LiPo電圧不足時は，予備電源に切り替わる．

昇降圧コンバータで3.3V電源を作る．

LiPo，主電源，メイン3.3V系出力の電力を監視する．

LiPoの温度を監視し，ヒーターで一定温度を保つ．

- LiPo充放電コントローラ：MCP73871
- 電流センサ（LiPo, Vpp, Vdd）：INA226
- 予備電源切り替え回路
- 3.3V系出力 昇降圧コンバータ：TPS63070
- 外部サーミスタ温度計測
- 外部ヒーター駆動

## 内部IF

| ピン名 | 種別 | 内容 |
| --- | --- | --- |
| VBus | Power In | 充電用USBバスパワー（5V）|
| VPP | Power Out | 主電源（LiPoと予備電源を統合した電源系統） |
| Vdd\_BUS | Power Out | 外部モジュール駆動用電源（3.3V） |
| Vdd | Power Out | モジュール内駆動用電源（3.3V） |
| I2C\_SCL | I2C | INA226との通信用．ID: 0x4D (VPP)，0x4E (Vdd), 0x4F (LiPo) |
| I2C\_SDA | I2C | |
| CHAEGE\_IND | Digital In | 充電状態インジケータLED |
| PG | Digital Out (OC) | MCP73831 充電状態 |
| STAT1 | Digital Out (OC) | |
| STAT2 | Digital Out (OC) | |
| TEMP | Analog Out | サーミスタ出力．サーミスタに50uAの定電流を流したときの電圧を出力 |
| HEATER | Digital In | ヒーターON |
| BATTERY_V | Analog Out | 予備電源電圧．抵抗分圧4.7倍 |

## 外部IF

| 名前 | 種類 | 内容 |
| --- | --- | --- |
| Battery | XH 2pin | 予備電源入力（1: +, 2: -） |
| LiPo | PH 2pin | 1セル LiPo（1: +, 2: -）|
| Temp&Heater | PH 3pin | サーミスタとヒーター接続用（1: ヒーター+, 2: サーミスタ+, 3: ヒーター-・サーミスタ-） |
| Ext Switch | PH 2pin | 外部電源スイッチ接続用．ショートでON |
| Power Switch | スライドスイッチ | 電源スイッチ．外部電源スイッチとOR |
| PROG | VR | MCP73871 充電電流設定用可変抵抗 |
| USB Power | LED | USBバスパワー有効 |
| Backup Power | LED | 予備電源使用中 |
| Heater | LED | ヒーター駆動中 |
| VDD | LED | Vdd出力中 |
| Charge | LED | 充電状態 |

## テレメトリ

### 電源状態（P）

| エントリ名 | データ型 | 内容 |
| --- | --- | --- |
| Sc | enum | 電源ソース（0: LiPoまたはUSB，1: 予備電源）|
| Vb | int | 予備電源電圧(mV) |
| Vp | int | 主電源電圧(mV) |
| Ip | int | 主電源電流(mA) |
| Pp | int | 主電源電力(mW) |
| Wp | int | 主電源積算電力量(mWh) |
| Vd | int | Vdd電圧(mV) |
| Id | int | Vdd電流(mA) |
| Pd | int | Vdd電力(mW) |
| Wd | int | Vdd積算電力量(mWh) |

### LiPo状態（L）

| Cg | bool | 充電中 |
| --- | --- | --- |
| Vl | int | LiPo電圧(mV) |
| Il | int | LiPo電流(mA)，充電中は負の値 |
| Pl | int | LiPo電力(mW)，充電中は負の値 |
| Wl | int | LiPo積算放電電力量(mWh) |
| Dl | int | LiPo電池残量(%) |

### LiPo温度（H）

| エントリ名 | データ型 | 内容 |
| --- | --- | --- |
| Tm | float16 | 温度(℃) |
| Ht | int | ヒーター駆動Duty比(%) |


# LiPoPowerSimple (1)

1セルのLiPoを充放電する．乾電池を電源に使うこともできる（同時使用不可）．

昇降圧コンバータで3.3V電源を作る．

LiPoの電力を監視する．

- LiPo充放電コントローラ：MCP73871
- 電流センサ（LiPo）：INA226
- 3.3V系出力 昇降圧コンバータ：TPS63070

## 内部IF

| ピン名 | 種別 | 内容 |
| --- | --- | --- |
| VUSB | Power In | 充電用USBバスパワー（5V）|
| VDD | Power Out | 電源（3.3V） |
| I2C\_SCL | I2C | INA226との通信用．ID: 0x4F (LiPo) |
| I2C\_SDA | I2C | |
| CHAEGE\_IND | Digital In | 充電状態インジケータLED |
| PG | Digital Out (OC) | MCP73831 充電状態 |
| STAT1 | Digital Out (OC) | |
| STAT2 | Digital Out (OC) | |

## 外部IF

| 名前 | 種類 | 内容 |
| --- | --- | --- |
| Battery | XH 2pin | 予備電源入力（1: +, 2: -） |
| LiPo | PH 2pin | 1セル LiPo（1: +, 2: -）|
| Power Switch | スライドスイッチ | 電源スイッチ |
| PROG | VR | MCP73871 充電電流設定用可変抵抗 |
| USB Power | LED | USBバスパワー有効 |
| VDD | LED | Vdd出力中 |
| Charge | LED | 充電状態 |

## テレメトリ

### LiPo状態（L）

| Cg | bool | 充電中 |
| Vl | int | LiPo電圧(mV) |
| Il | int | LiPo電流(mA)，充電中は負の値 |
| Pl | int | LiPo電力(mW)，充電中は負の値 |
| Wl | int | LiPo積算放電電力量(mWh) |
| Dl | int | LiPo電池残量(%) |


# ModuleIF (3~10)

他モジュールと接続し，電源を供給する．親側にのみ用いる．

- モジュール間コネクタ：4pin Grove
- 電流センサ：INA226
- ロードスイッチ：TCK106AF

## 内部IF

| ピン名 | 種別 | 内容 |
| --- | --- | --- |
| VIN | Power In | 駆動用電源（3.3V） |
| EN | Digital In | ロードスイッチON |
| I2C\_SCL | I2C | INA226との通信用．ID: 0x48~0x4Bを選択 |
| I2C\_SDA | I2C | |

## 外部IF

| 名前 | 種類 | 内容 |
| --- | --- | --- |
| Jumper | 1.27mm ジャンパピン | ショート時 CAN終端抵抗有効 |
| Power | LED | 電源出力インジーケータ |


## テレメトリ

### 電源状態（P）

| エントリ名 | データ型 | 内容 |
| --- | --- | --- |
| On | bool | 電源出力ON/OFF |
| Vo | int | 出力電圧(mV) |
| Io | int | 出力電流(mA) |
| Po | int | 出力電力(mW) |
| Wo | int | 積算出力電力量(mWh) |

## コマンド

### 電源ON/OFF (P)

| エントリ名 | データ型 | 内容 |
| --- | --- | --- |
| On | bool | 電源出力ON/OFF |

# ModuleIFSimple (-)

他モジュールと接続する．親側・子側どちらにも用いる．
ModuleIFと異なり，電源ON/OFFや電力測定機能はない．

- モジュール間コネクタ：4pin Grove

## 外部IF

| 名前 | 種類 | 内容 |
| --- | --- | --- |
| Jumper | 1.27mm ジャンパピン | ショート時 CAN終端抵抗有効 |


# LoRa (11~18)



# Logger (20)

RTCにより絶対時刻を提供する．
Micro SDカードにタイムスタンプ付きでログを記録する．

- Micro SDカードコネクタ
- RTC：PCF8563

## 内部IF

| ピン名 | 種別 | 内容 |
| --- | --- | --- |
| +BATT | Power In | RTCバックアップ用電源 3V |
| I2C\_SCL | I2C | RTCとの通信用．ID: 0x51 |
| I2C\_SDA | I2C | |
| SD\_CLK | SPI | SDカード通信用 |
| SD\_MOSI | SPI | |
| SD\_MISO | SPI | |
| SD\_SS | SPI | |
| SD\_DET | Digital Out (OC) | SDカード挿入検出 |

## 外部IF

| 名前 | 種類 | 内容 |
| --- | --- | --- |
| Power | LED | 電源出力インジーケータ |
| Jumper | 1.27mm ジャンパピン | ショート時 CAN終端抵抗有効 |

## テレメトリ

### 絶対時刻 (T)

| エントリ名 | データ型 | 内容 |
| --- | --- | --- |
| Ut | int | UNIX時間 UTC（ms）．日本時間にするには+9h |
| Ts | int | 起動からの経過時間（ms） |

## コマンド

### 絶対時刻セット (T)

| エントリ名 | データ型 | 内容 |
| --- | --- | --- |
| Ut | int | UNIX時間 UTC（ms）．|

# GPS

## GPSActive (21)

## GPSPassive (23)

# Pressure (25)

# IMU (30)

# LCD (35)

# Ignite (36)

# RCS (37)

# TankPressure (40)

# HX711 (45)