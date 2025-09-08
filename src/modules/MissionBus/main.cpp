// #define NDEBUG

#include <library/wobc.h>
#include <components/LiPoPower/lipo_power.h>
#include <components/LoRa/lora.h>
#include <components/Pressure/pressure.h>
#include <components/GPS/gps.h>
#include <components/Logger/logger.h>
#include <SPI.h>

#define SPI0_SCK_PIN 5
#define SPI0_MOSI_PIN 1
#define SPI0_MISO_PIN 2
#define SPI0_CS_PIN 4

#define SD_INSERTED_PIN 5
#define SDCARD_MOSI_PIN SPI0_MOSI_PIN
#define SDCARD_MISO_PIN SPI0_MISO_PIN
#define SDCARD_SS_PIN SPI0_CS_PIN
#define SDCARD_SCK_PIN SPI0_SCK_PIN

#define ST 8
#define PG 11
#define STAT1 10
//#define STAT2 
#define HEAT 48
//#define CHARGELED 
#define TEMP 9

#define LORA_CHANNEL 11
#define LORA_TX_PIN 13
#define LORA_RX_PIN 12
#define LORA_AUX_PIN 21
#define LORA_M0_PIN 14
#define LORA_M1_PIN 18

constexpr uint8_t module_id = 0x54;
constexpr uint8_t unit_id = 0x61;

void setup() {
    // Initialize the MissionBus
}

void loop() {
    // Main loop for the MissionBus
}
