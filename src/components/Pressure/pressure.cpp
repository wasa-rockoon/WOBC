#include "pressure.h"

namespace component {

Pressure::Pressure(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz)
  : process::Component("Pressure", component_id),
    wire_(wire),
    unit_id_(unit_id),
    sample_timer_(*this, bme, unit_id, 1000 / sample_freq_hz) {
}

void Pressure::setup() {
  start(sample_timer_);
  storeOnCommand('Q'); // 高度規正値の設定コマンド
  Wire.begin();
  while(!bme.begin()){
    LOG("Could not find BME280");
    delay(1000);
  }

  // 高度計算のための初期化
  initialize_pressure_data();     // 圧力データの初期化
  initialize_coefficients();      // 既知の係数の初期化
}


// 高度計算用の既知の係数の初期化
void Pressure::initialize_pressure_data() {
  p[0] = {101325, 0};
  p[1] = {95461, 500};
  p[2] = {89875, 1000};
  p[3] = {84556, 1500};
  p[4] = {79495, 2000};
  p[5] = {70109, 3000};
  p[6] = {61640, 4000};
  p[7] = {54020, 5000};
  p[8] = {47181, 6000};
  p[9] = {41061, 7000};
  p[10] = {35600, 8000};
  p[11] = {30742, 9000};
  p[12] = {26436, 10000};
  p[13] = {22632, 11000};
  p[14] = {19330, 12000};
  p[15] = {16510, 13000};
  p[16] = {14102, 14000};
  p[17] = {12045, 15000};
  p[18] = {10287, 16000};
  p[19] = {8787, 17000};
  p[20] = {7505, 18000};
  p[21] = {6410, 19000};
  p[22] = {5475, 20000};
  p[23] = {4678, 21000};
  p[24] = {4000, 22000};
  p[25] = {3422, 23000};
  p[26] = {2930, 24000};
  p[27] = {2511, 25000};
  p[28] = {2153, 26000};
  p[29] = {1847, 27000};
  p[30] = {1586, 28000};
  p[31] = {1363, 29000};
  p[32] = {1172, 30000};
}

// 高度計算用の既知の係数の初期化
void Pressure::initialize_coefficients() {
    coe[0] = {3.5188232031550987e-07, -0.0832025920873124, 0.0};
    coe[1] = {3.9026495985604870e-07, -0.0873294679399727, 500.0};
    coe[2] = {4.3487948904184543e-07, -0.0916895080714845, 1000.0};
    coe[3] = {4.8981397508824425e-07, -0.0963157560759116, 1500.0};
    coe[4] = {5.6126194933308466e-07, -0.1012736531317548, 2000.0};
    coe[5] = {7.4011485166556683e-07, -0.1118096624446355, 3000.0};
    coe[6] = {9.0391968482651588e-07, -0.1243457278021469, 4000.0};
    coe[7] = {1.1842000049410562e-06, -0.1381214637989030, 5000.0};
    coe[8] = {1.4836178666618892e-06, -0.1543189514664867, 6000.0};
    coe[9] = {1.9480335375626762e-06, -0.1724784341544283, 7000.0};
    coe[10] = {2.4889194565639734e-06, -0.1937548564516878, 8000.0};
    coe[11] = {3.3202262129672648e-06, -0.2179371978916634, 9000.0};
    coe[12] = {4.2981576419397227e-06, -0.2465309860377375, 10000.0};
    coe[13] = {7.1518443858443532e-06, -0.2792313693776149, 11000.0};
    coe[14] = {9.9814820483273869e-06, -0.3264621497017310, 12000.0};
    coe[15] = {1.3506928393804260e-05, -0.3827577084542975, 13000.0};
    coe[16] = {1.8637722689718214e-05, -0.4478070755988588, 14000.0};
    coe[17] = {2.5224998370334986e-05, -0.5244826667443595, 15000.0};
    coe[18] = {3.5661937101472954e-05, -0.6131737610144573, 16000.0};
    coe[19] = {4.6701738634300871e-05, -0.7201595723188762, 17000.0};
    coe[20] = {6.6976419137165788e-05, -0.8399028301772236, 18000.0};
    coe[21] = {8.8703239026655979e-05, -0.9865811880876166, 19000.0};
    coe[22] = {1.2829221985398778e-04, -1.1524562450674634, 20000.0};
    coe[23] = {1.7400030998907922e-04, -1.3569540435147198, 21000.0};
    coe[24] = {2.3737948506654402e-04, -1.5928984638599113, 22000.0};
    coe[25] = {3.3579507440328372e-04, -1.8673091485968363, 23000.0};
    coe[26] = {4.5084330085696286e-04, -2.1977315018096677, 24000.0};
    coe[27] = {6.0826229457450372e-04, -2.5755381879278025, 25000.0};
    coe[28] = {8.3960740315687336e-04, -3.0110539908431471, 26000.0};
    coe[29] = {1.1744210840839830e-03, -3.5248937215751535, 27000.0};
    coe[30] = {1.5531991267642778e-03, -4.1379415274669924, 28000.0};
    coe[31] = {2.1200720221831240e-03, -4.8306683380038606, 29000.0};
    coe[32] = {0.0000000000000000e+00, 0.0000000000000000, 0.0};
}

// 圧力から高度を計算する関数
double Pressure::height(float pressure) {
    for (int i = 0; i < max_index; ++i) {
        if (p[i].pressure >= pressure && pressure > p[i + 1].pressure) {
            return coe[i].a * pow((pressure - p[i].pressure), 2) + coe[i].b * (pressure - p[i].pressure) + coe[i].c;
        }
    }
    if (pressure >= p[0].pressure) {
        return 2*coe[0].a*(pressure - p[0].pressure) + coe[0].b;
    }
    return 0.0;  // 圧力範囲外の場合は高度0を返す
}

Pressure::SampleTimer::SampleTimer(Pressure& pressure_ref, BME280I2C& bme_ref, uint8_t unit_id_ref, unsigned interval_ms)
  : process::Timer("Pressure", interval_ms),
    bme_(bme_ref), unit_id_(unit_id_ref), pressure_(pressure_ref) { 
}

void Pressure::SampleTimer::callback() { // Timerで定期的に実行される関数

  // 高度規正値を不揮発メモリから読み込み
  double sealevel_Pa = 100310.0; // デフォルト値は101325 Pa (1気圧)
  wcpp::Packet qnh = loadPacket('Q'); 
  if (qnh) {
    auto e = qnh.find("Sp");
    if (e) sealevel_Pa = (*e).getFloat32();
  }

  float temp(NAN), hum(NAN), pres(NAN);
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);
  bme_.read(pres, temp, hum, tempUnit, presUnit);

  // height関数を使用して圧力から高度を計算
  double pressureAlt = pressure_.height(pres) - pressure_.height(sealevel_Pa);  // オブジェクト pressure_ を使って height を呼び出す

  wcpp::Packet packet = newPacket(64);
  packet.telemetry(telemetry_id, component_id(), unit_id_, 0xFF, 1234);
  packet.append("Sp").setInt((int)sealevel_Pa);
  packet.append("PR").setInt((int)pres);
  packet.append("TE").setInt((int)temp);
  packet.append("HU").setInt((int)hum);
  packet.append("PA").setInt((int)pressureAlt);  // 計算された高度を追加
  // ... TODO
  sendPacket(packet);
}

}
