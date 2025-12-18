#include <Arduino.h>
#include <lorawan.h>
#include <ModbusMaster.h>

// Konfigurasi Pin RS485
#define EN 22

// Inisialisasi Modbus Master
ModbusMaster node; // ID 1: Sensor 4-20mA
ModbusMaster flow; // ID 2: Flowmeter

// OTAA Credentials (Ganti sesuai dengan data dari Network Server Anda)
const char *devEui = "1293847139471947";
const char *appEui = "0000000000000001";
const char *appKey = "12791384713294723471924719347913";

// Konfigurasi Waktu (Non-blocking)
const unsigned long interval = 30000; // Interval pengiriman 30 detik
unsigned long previousMillis = 0;

// Variabel Data Channel
int chn1 = 0, chn2 = 0, chn3 = 0, chn4 = 0, chn5 = 0;
int chn6 = 0, chn7 = 0, chn8 = 0, chn9 = 0, chn10 = 0;

// Buffers & Status
byte outStr[255];
byte recvStatus = 0;
int port, channel, freq;

// Konfigurasi Pin RFM95 (SX1276)
const sRFM_pins RFM_pins = {
  .CS = 5,
  .RST = 27,
  .DIO0 = 26,
  .DIO1 = 35,
  .DIO2 = 34,
};

/**
 * Callback Modbus: Mengaktifkan Driver TX RS485
 */
void preTransmission() {
  digitalWrite(EN, HIGH);
}

/**
 * Callback Modbus: Menonaktifkan Driver TX (Kembali ke RX)
 */
void postTransmission() {
  digitalWrite(EN, LOW);
}

/**
 * Fungsi Helper untuk Reset Data jika Modbus Timeout/Error
 */
void resetChannelData() {
    // Memberikan nilai 0 atau nilai khusus agar server tahu data bermasalah
    chn1 = chn2 = chn3 = chn4 = chn5 = 0;
    chn6 = chn7 = chn8 = chn9 = chn10 = 0;
}

void setup() {
  // Setup Hardware Serial & Pin
  pinMode(EN, OUTPUT);
  digitalWrite(EN, LOW); // Default: Listening Mode
  
  Serial.begin(115200);
  Serial2.begin(9600); // RS485 Modbus RTU

  // Inisialisasi Modbus
  node.begin(1, Serial2);
  flow.begin(2, Serial2);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  flow.preTransmission(preTransmission);
  flow.postTransmission(postTransmission);

  delay(2000);

  // Inisialisasi LoRa RFM95 via SPI
  SPI.begin(18, 19, 23, 5);
  if (!lora.init()) {
    Serial.println("RFM95 tidak terdeteksi!");
    while (1); // Berhenti jika hardware gagal
  }

  // Konfigurasi LoRaWAN
  lora.setDeviceClass(CLASS_C);
  lora.setDataRate(SF10BW125);
  lora.setFramePortTx(5);
  lora.setChannel(MULTI);
  lora.setTxPower(15);
  lora.setDevEUI(devEui);
  lora.setAppEUI(appEui);
  lora.setAppKey(appKey);

  // Prosedur Join OTAA
  Serial.println("Menghubungkan ke Jaringan LoRaWAN...");
  while (!lora.join()) {
    Serial.println("Gagal terhubung, mencoba lagi dalam 10 detik...");
    delay(10000);
  }
  Serial.println("Berhasil Terhubung!");
}

void loop() {
  unsigned long currentMillis = millis();

  // Task Utama: Pengiriman Data Berkala
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    uint8_t result;
    
    // 1. Baca Slave ID 1 (Sensor 4-20mA)
    // Membaca 8 register mulai dari address 0
    result = node.readInputRegisters(0, 8);
    if (result == node.ku8MBSuccess) {
      chn1  = node.getResponseBuffer(0);
      chn2  = node.getResponseBuffer(1);
      chn8  = node.getResponseBuffer(2);
      chn9  = node.getResponseBuffer(3);
      chn10 = node.getResponseBuffer(4);
      Serial.println("Data Sensor 4-20mA: Berhasil");
    } else {
      Serial.print("Error Sensor 4-20mA: 0x");
      Serial.println(result, HEX);
    }

    // Delay kecil non-blocking antar pembacaan slave agar tidak tabrakan di bus
    delay(200); 

    // 2. Baca Slave ID 2 (Flowmeter)
    // Membaca 15 register mulai dari address 1
    result = flow.readInputRegisters(1, 15);
    if (result == flow.ku8MBSuccess) {
      chn3 = flow.getResponseBuffer(0);
      chn4 = flow.getResponseBuffer(1);
      chn5 = flow.getResponseBuffer(2);
      chn6 = flow.getResponseBuffer(12);
      chn7 = flow.getResponseBuffer(13);
      Serial.println("Data Flowmeter: Berhasil");
    } else {
      Serial.print("Error Flowmeter: 0x");
      Serial.println(result, HEX);
    }

    // Debugging Data ke Serial Monitor
    Serial.printf("DEBUG: Ch1:%d, Ch2:%d, Ch3:%d, Ch4:%d, Ch5:%d\n", chn1, chn2, chn3, chn4, chn5);
    Serial.printf("DEBUG: Ch6:%d, Ch7:%d, Ch8:%d, Ch9:%d, Ch10:%d\n", chn6, chn7, chn8, chn9, chn10);

    // 3. Encoding Payload (Little Endian / LSB-MSB)
    byte myByte[] = {
      lowByte(chn1),  highByte(chn1),
      lowByte(chn2),  highByte(chn2),
      lowByte(chn3),  highByte(chn3),
      lowByte(chn4),  highByte(chn4),
      lowByte(chn5),  highByte(chn5),
      lowByte(chn6),  highByte(chn6),
      lowByte(chn7),  highByte(chn7),
      lowByte(chn8),  highByte(chn8),
      lowByte(chn9),  highByte(chn9),
      lowByte(chn10), highByte(chn10)
    };

    // 4. Kirim ke LoRaWAN Uplink
    lora.sendUplinkHex(myByte, sizeof(myByte), 0);
    
    Serial.printf("Uplink Terkirim! Ch:%d, Freq:%d\n", lora.getChannel(), lora.getChannelFreq(lora.getChannel()));
  }

  // Task Penting: Update Lora stack (Wajib dipanggil sering)
  lora.update();

  // 5. Handling Downlink & MAC Command
  recvStatus = lora.readDataByte(outStr);
  if (recvStatus > 0) {
    port = lora.getFramePortRx();
    channel = lora.getChannelRx();
    freq = lora.getChannelRxFreq(channel);

    if (port == 0) {
      Serial.print("MAC Command Diterima: ");
    } else {
      Serial.print("Data Downlink Diterima (Port ");
      Serial.print(port);
      Serial.print("): ");
    }

    for (int i = 0; i < recvStatus; i++) {
      Serial.print(outStr[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
}