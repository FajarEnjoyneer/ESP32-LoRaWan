#ifndef CONFIG_H
#define CONFIG_H

#include <RadioLib.h>
#include "EspHal.h" 
#include "esp_log.h"
#include <stdio.h>

// --- KONFIGURASI LORAWAN KEYS ---
// Menggunakan Macro seperti kode asli Anda, tapi dikonversi ke tipe data yang benar
#define RADIOLIB_LORAWAN_JOIN_EUI  0x0000000000000000
#define RADIOLIB_LORAWAN_DEV_EUI   0x0000000000000000
// Key dalam format array byte
#define RADIOLIB_LORAWAN_APP_KEY   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define RADIOLIB_LORAWAN_NWK_KEY   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

uint64_t joinEUI = RADIOLIB_LORAWAN_JOIN_EUI;
uint64_t devEUI  = RADIOLIB_LORAWAN_DEV_EUI;
uint8_t appKey[] = RADIOLIB_LORAWAN_APP_KEY;
uint8_t nwkKey[] = RADIOLIB_LORAWAN_NWK_KEY;

uint32_t uplinkIntervalSeconds = 60; // 1 Menit

// --- KONFIGURASI PIN (Berdasarkan blok #else kode Anda) ---
// ESP-IDF tidak mendeteksi board ARDUINO_TTGO dsb secara otomatis.
// Kita gunakan pin standar VSPI/HSPI ESP32 yang umum dipakai modul LoRa.
#define LORA_SS       25
#define LORA_SCK      18
#define LORA_MOSI     23
#define LORA_MISO     19
#define LORA_DIO0     26
#define LORA_DIO1     35
#define LORA_RST      27
// Pin DIO2 biasanya tidak wajib untuk LoRaWAN dasar di SX1276

// --- INISIALISASI OBJEK (Gaya ESP-IDF/C++) ---

// 1. Inisialisasi Region & SubBand (Sesuai kode Anda: AS923_2)
const LoRaWANBand_t Region = AS923_2;
const uint8_t subBand = 0;

// 2. Buat Instance HAL
// Kita wajib mengalokasikan memori untuk HAL kita sendiri
EspHal* hal = new EspHal(LORA_SCK, LORA_MISO, LORA_MOSI);

// 3. Buat Instance Radio
// Perhatikan sintaks ini berbeda dengan Arduino. Kita pass pointer module ke constructor.
// Parameter: HAL, CS, IRQ(DIO0), RST, GPIO(DIO1)
SX1276 radio(new Module(hal, LORA_SS, LORA_DIO0, LORA_RST, LORA_DIO1));

// 4. Buat Node LoRaWAN
LoRaWANNode node(&radio, &Region, subBand);

// --- HELPER FUNCTIONS (Pengganti Serial Arduino) ---

// Fungsi Debug: Menggunakan ESP_LOGE pengganti Serial.print
// __FlashStringHelper dihapus karena tidak relevan di ESP-IDF
void debug(bool isFail, const char* message, int state, bool Freeze) {
    if (isFail) {
        ESP_LOGE("LoRaWAN", "%s (State: %d)", message, state);
        if (Freeze) {
            while (true) { vTaskDelay(pdMS_TO_TICKS(1000)); }
        }
    }
}

// Fungsi ArrayDump: Menggunakan printf pengganti Serial
void arrayDump(uint8_t *buffer, uint16_t len) {
    for(uint16_t c = 0; c < len; c++) {
        printf("%02X ", buffer[c]);
    }
    printf("\n");
}

#endif