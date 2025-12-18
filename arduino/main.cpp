#include <Arduino.h>
#include <lorawan.h>
#include <ModbusMaster.h>

#define EN 22

ModbusMaster node;
ModbusMaster flow;

// OTAA credentials
const char *devEui = "1293847139471947";
const char *appEui = "0000000000000001";
const char *appKey = "12791384713294723471924719347913";

const unsigned long interval =30000;    // 10 s interval to send message
unsigned long previousMillis = 0;  // will store last time message sent
unsigned int counter = 0;     // message counter

char myStr[50];
byte outStr[255];
byte recvStatus = 0;
int port, channel, freq;
bool newmessage = false;

int chn1 = 0;
int chn2 = 0;
int chn3 = 0;
int chn4 = 0;
int chn5 = 0;
int chn6 = 0;
int chn7 = 0;
int chn8 = 0;
int chn9 = 0;
int chn10 = 0;

const sRFM_pins RFM_pins = {
  .CS = 5,
  .RST = 27,
  .DIO0 = 26,
  .DIO1 = 35,
  .DIO2 = 34,
};

void preTransmission()
{
  digitalWrite(EN, 1);
}

void postTransmission()
{
  digitalWrite(EN, 0);
}

void setup() {
  // Setup loraid access
  pinMode(EN, OUTPUT);
  // Init in receive mode
  digitalWrite(EN, 0);
  Serial.begin(9600);
  Serial2.begin(9600);
  node.begin(1, Serial2);
  flow.begin(2, Serial2);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  flow.preTransmission(preTransmission);
  flow.postTransmission(postTransmission);
  delay(2000);
  SPI.begin(18, 19, 23, 5);
  if (!lora.init()) {
    //    Serial.println("RFM95 not detected");
    delay(5000);
    return;
  }

  // Set LoRaWAN Class change CLASS_A or CLASS_C
  lora.setDeviceClass(CLASS_C);

  // Set Data Rate
  lora.setDataRate(SF10BW125);

  // Set FramePort Tx
  lora.setFramePortTx(5);

  // set channel to random
  lora.setChannel(MULTI);

  // Set TxPower to 15 dBi (max)
  lora.setTxPower(15);

  // Put OTAA Key and DevAddress here
  lora.setDevEUI(devEui);
  lora.setAppEUI(appEui);
  lora.setAppKey(appKey);

  // Join procedure
  bool isJoined;
  do {
    //        Serial.println("Joining...");
    isJoined = lora.join();

    //wait for 10s to try again
    delay(10000);
  } while (!isJoined);
  //    Serial.println("Joined to network");
}

void loop() {
  // Check interval overflow
  if (millis() - previousMillis > interval) {
    previousMillis = millis();
    uint8_t result;
    uint8_t flow_rate;
    uint16_t data[6];
    // Read 16 registers starting at 0x3100)
    result = node.readInputRegisters(0, 8);

//    Jika ingin menambah sensor 4-20mA, uncomment dengan address yang diinginkan
//    Lalu ubah nama chn menjadi sesuai urutan terakhir, misal chn terakhir yang dipakai adalah chn7
//    seperti pada flow rate bawah, maka chn diganti menjadi chn8
//    Setelah membaca sekali wajib dikasih delay seperti dibawah

//    Setelah menambahkan sensor, ubah decoder juga pada gateway, sudah ada sample di awal, copas dan sesuaikan saja
    if (result == node.ku8MBSuccess)
    {
      chn1 = node.getResponseBuffer(0);
      chn2 = node.getResponseBuffer(1);
      chn8 = node.getResponseBuffer(2);
      chn9 = node.getResponseBuffer(3);
      chn10 = node.getResponseBuffer(4);
//      chn6 = node.getResponseBuffer(5);
//      chn7 = node.getResponseBuffer(6);
//      chn8 = node.getResponseBuffer(7);
      Serial.println("420 Succes");
    }
    
//  Wajib delay setelah membaca sekali
    delay(1000); 

//    Berikut code jika ingin menambahkan flowmeter, step sama seperti sensor 4-20mA
    flow_rate = flow.readInputRegisters(1, 15);
    if (flow_rate == flow.ku8MBSuccess) {
      chn3 = flow.getResponseBuffer(0);
      chn4 = flow.getResponseBuffer(1);
      chn5 = flow.getResponseBuffer(2);
      chn6 = flow.getResponseBuffer(12);
      chn7 = flow.getResponseBuffer(13);
//      chn8 = flow.getResponseBuffer(5);
//      chn9 = flow.getResponseBuffer(6);
      Serial.println("Flow Succes");
    }

//  Serial print untuk debugging data sensor yang diambil, cocokan dengan modbus poll terlebih dahulu
    Serial.print("REG 1: ");
    Serial.println(chn1);
    Serial.print("REG 2: ");
    Serial.println(chn2);
    Serial.print("REG 3: ");
    Serial.println(chn3);
    Serial.print("REG 4: ");
    Serial.println(chn4);
    Serial.print("REG 5: ");
    Serial.println(chn5);
    Serial.print("REG 6: ");
    Serial.println(chn6);
    Serial.print("REG 7: ");
    Serial.println(chn7);
    Serial.print("REG 8: ");
    Serial.println(chn8);
    Serial.print("REG 9: ");
    Serial.println(chn9);
    Serial.print("REG 10: ");
    Serial.println(chn10);


//  Setiap data yang sudah diambil, harus ditambahkan ke myByte untuk di encode dan dikirim ke Lora gateway
//  Data chanel harus dipisah dengan lowByte dan highByte seperti dibawah
//  Decoder di gateway lora juga harus disesuaikan, jika sensor 4-20mA maka copas dan sesuaikan dengan sample yang sudah ada
//  Begitupun untuk flowmeter
    byte myByte[] = {lowByte(chn1), highByte(chn1),
                     lowByte(chn2), highByte(chn2),
                     lowByte(chn3), highByte(chn3),
                     lowByte(chn4), highByte(chn4),
                     lowByte(chn5), highByte(chn5),
                     lowByte(chn6), highByte(chn6),
                     lowByte(chn7), highByte(chn7),
                     lowByte(chn8), highByte(chn8),
                     lowByte(chn9), highByte(chn9),
                     lowByte(chn10), highByte(chn10),
                    };
    //    sprintf(myStr, "Lora Counter-%d", counter);

    //    Serial.print("Sending: ");
    for (int i = 0; i < sizeof(myByte); i++)
    {
      //      Serial.print(myByte[i]); Serial.print(" ");
    }
    //    Serial.println();
    lora.sendUplinkHex(myByte, sizeof(myByte), 0);
    port = lora.getFramePortTx();
    channel = lora.getChannel();
    freq = lora.getChannelFreq(channel);
    //    Serial.print(F("fport: "));    Serial.print(port); Serial.print(" ");
    //    Serial.print(F("Ch: "));    Serial.print(channel); Serial.print(" ");
    //    Serial.print(F("Freq: "));    Serial.print(freq); Serial.println(" ");
  }

  // Check Lora RX
  lora.update();

  recvStatus = lora.readDataByte(outStr);

  if (recvStatus) {
    newmessage = true;
    int counter = 0;
    port = lora.getFramePortRx();
    channel = lora.getChannelRx();
    freq = lora.getChannelRxFreq(channel);

    for (int i = 0; i < recvStatus; i++)
    {
      if (((outStr[i] >= 32) && (outStr[i] <= 126)) || (outStr[i] == 10) || (outStr[i] == 13))
        counter++;
    }
    if (port != 0)
    {
      if (counter == recvStatus)
      {
        Serial.print(F("Received String : "));
        for (int i = 0; i < recvStatus; i++)
        {
          Serial.print(char(outStr[i]));
        }
      }
      else
      {
        Serial.print(F("Received Hex : "));
        for (int i = 0; i < recvStatus; i++)
        {
          Serial.print(outStr[i], HEX); Serial.print(" ");
        }
      }
      Serial.println();
      Serial.print(F("fport: "));    Serial.print(port); Serial.print(" ");
      Serial.print(F("Ch: "));    Serial.print(channel); Serial.print(" ");
      Serial.print(F("Freq: "));    Serial.println(freq); Serial.println(" ");
    }
    else
    {
      Serial.print(F("Received Mac Cmd : "));
      for (int i = 0; i < recvStatus; i++)
      {
        Serial.print(outStr[i], HEX); Serial.print(" ");
      }
      Serial.println();
      Serial.print(F("fport: "));    Serial.print(port); Serial.print(" ");
      Serial.print(F("Ch: "));    Serial.print(channel); Serial.print(" ");
      Serial.print(F("Freq: "));    Serial.println(freq); Serial.println(" ");
    }
  }

}
