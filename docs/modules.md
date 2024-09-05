# モジュール一覧

# Tracker(T, 0x54)
GNSS及び気圧温湿度センサを搭載し，位置情報と高度といった成層圏気球実験において必要最小限の情報を送信するモジュール．LiPoの充放電の機能を有し，電源状態のモニタリングを行う．

## コンポーネント構成
|コンポーネント名|ID|
| --- | --- |
| ESP32Core | 0 |
| LiPoPower | 1 |
| Logger | ? |
| GPSActive | ? |
| LoRa | 11~18 |
| Pressure | ? |

# GS(G, 0x47)
機体からのダウンリンクの受信およびアップリンクを行う地上局の中心となるモジュール自身の位置がわかるようにGNSSを搭載する．サーバーの更新を行う．

## コンポーネント構成
|コンポーネント名|ID|
| --- | --- |
| ESP32Core | 0 |
| LiPoPowerSimple | 1 |
| Logger | ? |
| ModuleIFSimple | - |
| GPSPassive | ? |

# LoRa(L, 0x4C)
LoRaモジュールを2機搭載しアップリンクとダウンリンクの送受信を行う．

## コンポーネント構成
|コンポーネント名|ID|
| --- | --- |
| PR2040Core | 0 |
| ModuleIFSimple | - |
| Lora | 11~18 |

# Controller(C, 0x43)
主に地上局で用いるが，インジケータとなるLEDやLCDと，コマンド送信用のスイッチを搭載する．

## コンポーネント構成
|コンポーネント名|ID|
| --- | --- |
| PR2040Core | 0 |
| ModuleIFSimple | - |
| LCD | 25 |

# MissionBus(M, ox4D)
ミッション電装の中心となるモジュール．GMSS，気圧温湿度，6軸，地磁気センサによって機体の情報を取得し，LoRaで送信を行う機能を有する．

## コンポーネント構成
|コンポーネント名|ID|
| --- | --- |
| ESP32Core | 0 |
| LiPoPower | 1 |
| Logger | ? |
| ModuleIF | 3~10 |
| GPSActive | |
| LoRa | 11~18 |
| Pressure | ? |
| IMU | |

# IGN(I, 0x49)
点火回路を有し，モデルロケットの点火を行うモジュール．

## コンポーネント構成
|コンポーネント名|ID|
| --- | --- |
| ERP2040Core | 0 |
| ModuleIFSimple | - |
| Ignite | |

# RCS(V, 0x56)
姿勢制御のためのスラスタの駆動を行う．そのほか圧縮空気タンクの圧力とロードセルのデータの取得を行う．

## コンポーネント構成
|コンポーネント名|ID|
| --- | --- |
| ERP2040Core | 0 |
| ModuleIFSimple | - |
| RCS | |
| HX711 | |
| TankPressure | |

# Rocket(R, 0x72)
ロケットに搭載する小型電装．GMSS，気圧温湿度，6軸，地磁気センサによって機体の情報を取得し，LoRaで送信を行う機能を有する．

## コンポーネント構成
|コンポーネント名|ID|
| --- | --- |
| ESP32Core | 0 |
| LiPoPower | 1 |
| Logger | ? |
| ModuleIF | 3~10 |
| GPSActive | |
| LoRa | 11~18 |
| Pressure | ? |
| IMU | |