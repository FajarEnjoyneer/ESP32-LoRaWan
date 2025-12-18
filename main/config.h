#ifndef CONFIG_H
#define CONFIG_H
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"      
#include "esp_rom_gpio.h"     
#include "soc/rtc.h"
#include "soc/dport_reg.h"
#include "soc/spi_reg.h"
#include "soc/spi_struct.h"
#include "hal/gpio_hal.h"
#include <RadioLib.h>
#ifndef ESP_HAL_H
#define ESP_HAL_H
#if CONFIG_IDF_TARGET_ESP32 == 0
  #error This example HAL only supports ESP32 targets. Support for ESP32S2/S3 etc. can be added by adjusting this file to user needs.
#endif
#define LOW                         (0x0)
#define HIGH                        (0x1)
#define INPUT                       (0x01)
#define OUTPUT                      (0x03)
#define RISING                      (0x01)
#define FALLING                     (0x02)
#define NOP()                       asm volatile ("nop")
#define MATRIX_DETACH_OUT_SIG       (0x100)
#define MATRIX_DETACH_IN_LOW_PIN    (0x30)
#define ClkRegToFreq(reg)           (apb_freq / (((reg)->clkdiv_pre + 1) * ((reg)->clkcnt_n + 1)))
typedef union {
        uint32_t value;
        struct {
            uint32_t clkcnt_l:       6;
            uint32_t clkcnt_h:       6;
            uint32_t clkcnt_n:       6;
            uint32_t clkdiv_pre:    13;
            uint32_t clk_equ_sysclk: 1;
    };
} spiClk_t;
static uint32_t getApbFrequency() {
        rtc_cpu_freq_config_t conf;
        rtc_clk_cpu_freq_get_config(&conf);
        if(conf.freq_mhz >= 80) {
            return(80 * MHZ);
    }
    return((conf.source_freq_mhz * MHZ) / conf.div);
}
static uint32_t spiFrequencyToClockDiv(uint32_t freq) {
        uint32_t apb_freq = getApbFrequency();
        if(freq >= apb_freq) {
            return SPI_CLK_EQU_SYSCLK;
    }
    const spiClk_t minFreqReg = { 0x7FFFF000 };
    uint32_t minFreq = ClkRegToFreq((spiClk_t*) &minFreqReg);
    if(freq < minFreq) {
            return minFreqReg.value;
    }
    uint8_t calN = 1;
    spiClk_t bestReg = { 0 };
    int32_t bestFreq = 0;
    while(calN <= 0x3F) {
            spiClk_t reg = { 0 };
        int32_t calFreq;
        int32_t calPre;
        int8_t calPreVari = -2;
        reg.clkcnt_n = calN;
        while(calPreVari++ <= 1) {
                calPre = (((apb_freq / (reg.clkcnt_n + 1)) / freq) - 1) + calPreVari;
                if(calPre > 0x1FFF) {
                    reg.clkdiv_pre = 0x1FFF;
            } else if(calPre <= 0) {
                    reg.clkdiv_pre = 0;
            } else {
                    reg.clkdiv_pre = calPre;
            }
            reg.clkcnt_l = ((reg.clkcnt_n + 1) / 2);
            calFreq = ClkRegToFreq(&reg);
            if(calFreq == (int32_t) freq) {
                    memcpy(&bestReg, &reg, sizeof(bestReg));
                    break;
            } else if(calFreq < (int32_t) freq) {
                    if(RADIOLIB_ABS(freq - calFreq) < RADIOLIB_ABS(freq - bestFreq)) {
                        bestFreq = calFreq;
                        memcpy(&bestReg, &reg, sizeof(bestReg));
                }
            }
        }
        if(calFreq == (int32_t) freq) {
                break;
        }
        calN++;
    }
    return(bestReg.value);
}
class EspHal : public RadioLibHal {
    public:
        EspHal(int8_t sck, int8_t miso, int8_t mosi)
            : RadioLibHal(INPUT, OUTPUT, LOW, HIGH, RISING, FALLING),
              spiSCK(sck), spiMISO(miso), spiMOSI(mosi) {
    
    }
    void init() override {
            spiBegin();
    }
    void term() override {
            spiEnd();
    }
    void pinMode(uint32_t pin, uint32_t mode) override {
            if(pin == RADIOLIB_NC) {
                return;
        }
    
        gpio_config_t conf = {};
        conf.pin_bit_mask = (1ULL << pin);
        conf.mode = (gpio_mode_t)mode;
        conf.pull_up_en = GPIO_PULLUP_DISABLE;
        conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        conf.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&conf);
    }
    void digitalWrite(uint32_t pin, uint32_t value) override {
            if(pin == RADIOLIB_NC) {
                return;
        }
        gpio_set_level((gpio_num_t)pin, value);
    }
    uint32_t digitalRead(uint32_t pin) override {
            if(pin == RADIOLIB_NC) {
                return(0);
        }
        return(gpio_get_level((gpio_num_t)pin));
    }
    void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override {
            if(interruptNum == RADIOLIB_NC) {
                return;
        }
        gpio_install_isr_service((int)ESP_INTR_FLAG_IRAM);
        gpio_set_intr_type((gpio_num_t)interruptNum, (gpio_int_type_t)(mode & 0x7));
        gpio_isr_handler_add((gpio_num_t)interruptNum, (void (*)(void*))interruptCb, NULL);
    }
    void detachInterrupt(uint32_t interruptNum) override {
            if(interruptNum == RADIOLIB_NC) {
                return;
        }
        gpio_isr_handler_remove((gpio_num_t)interruptNum);
        gpio_wakeup_disable((gpio_num_t)interruptNum);
        gpio_set_intr_type((gpio_num_t)interruptNum, GPIO_INTR_DISABLE);
    }
    void delay(unsigned long ms) override {
            vTaskDelay(ms / portTICK_PERIOD_MS);
    }
    void delayMicroseconds(unsigned long us) override {
            uint64_t m = (uint64_t)esp_timer_get_time();
            if(us) {
                uint64_t e = (m + us);
                if(m > e) {
                    while((uint64_t)esp_timer_get_time() > e) {
                        NOP();
                }
            }
            while((uint64_t)esp_timer_get_time() < e) {
                    NOP();
            }
        }
    }
    unsigned long millis() override {
            return((unsigned long)(esp_timer_get_time() / 1000ULL));
    }
    unsigned long micros() override {
            return((unsigned long)(esp_timer_get_time()));
    }
    long pulseIn(uint32_t pin, uint32_t state, unsigned long timeout) override {
            if(pin == RADIOLIB_NC) {
                return(0);
        }
        this->pinMode(pin, INPUT);
        uint32_t start = this->micros();
        uint32_t curtick = this->micros();
        while(this->digitalRead(pin) == state) {
                if((this->micros() - curtick) > timeout) {
                    return(0);
            }
        }
        return(this->micros() - start);
    }
    void spiBegin() {
            DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_SPI2_CLK_EN);
            DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_SPI2_RST);
            this->spi->slave.trans_done = 0;
            this->spi->slave.val = 0;
            this->spi->pin.val = 0;
            this->spi->user.val = 0;
            this->spi->user1.val = 0;
            this->spi->ctrl.val = 0;
            this->spi->ctrl1.val = 0;
            this->spi->ctrl2.val = 0;
            this->spi->clock.val = 0;
            this->spi->user.usr_mosi = 1;
            this->spi->user.usr_miso = 1;
            this->spi->user.doutdin = 1;
            for(uint8_t i = 0; i < 16; i++) {
                this->spi->data_buf[i] = 0x00000000;
        }
        this->spi->pin.ck_idle_edge = 0;
        this->spi->user.ck_out_edge = 0;
        this->spi->ctrl.wr_bit_order = 0;
        this->spi->ctrl.rd_bit_order = 0;
        this->spi->clock.val = spiFrequencyToClockDiv(2000000);
        this->pinMode(this->spiSCK, OUTPUT);
        this->pinMode(this->spiMISO, INPUT);
        this->pinMode(this->spiMOSI, OUTPUT);
        esp_rom_gpio_connect_out_signal((gpio_num_t)this->spiSCK, HSPICLK_OUT_IDX, false, false);
        esp_rom_gpio_connect_in_signal((gpio_num_t)this->spiMISO, HSPIQ_OUT_IDX, false);
        esp_rom_gpio_connect_out_signal((gpio_num_t)this->spiMOSI, HSPID_IN_IDX, false, false);
    }
    void spiBeginTransaction() {
    
    
    
    }
    uint8_t spiTransferByte(uint8_t b) {
            this->spi->mosi_dlen.usr_mosi_dbitlen = 7;
            this->spi->miso_dlen.usr_miso_dbitlen = 7;
            this->spi->data_buf[0] = b;
            this->spi->cmd.usr = 1;
            while(this->spi->cmd.usr);
            return(this->spi->data_buf[0] & 0xFF);
    }
    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) override {
            for(size_t i = 0; i < len; i++) {
                in[i] = this->spiTransferByte(out[i]);
        }
    }
    void spiEndTransaction() {
    
    
    
    }
    void spiEnd() {
            esp_rom_gpio_connect_out_signal((gpio_num_t)this->spiSCK, MATRIX_DETACH_OUT_SIG, false, false);
            esp_rom_gpio_connect_in_signal((gpio_num_t)this->spiMISO, MATRIX_DETACH_IN_LOW_PIN, false);
            esp_rom_gpio_connect_out_signal((gpio_num_t)this->spiMOSI, MATRIX_DETACH_OUT_SIG, false, false);
    }
private:
    int8_t spiSCK;
    int8_t spiMISO;
    int8_t spiMOSI;
    volatile spi_dev_t * spi = (volatile spi_dev_t *)(DR_REG_SPI2_BASE);
};
#endif
#define RADIOLIB_LORAWAN_JOIN_EUI  0x0000000000000000
#define RADIOLIB_LORAWAN_DEV_EUI   0x0000000000000000
#define RADIOLIB_LORAWAN_APP_KEY   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define RADIOLIB_LORAWAN_NWK_KEY   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
uint64_t joinEUI = RADIOLIB_LORAWAN_JOIN_EUI;
uint64_t devEUI  = RADIOLIB_LORAWAN_DEV_EUI;
uint8_t appKey[] = RADIOLIB_LORAWAN_APP_KEY;
uint8_t nwkKey[] = RADIOLIB_LORAWAN_NWK_KEY;
uint32_t uplinkIntervalSeconds = 60;
#define LORA_SS       5
#define LORA_SCK      18
#define LORA_MOSI     23
#define LORA_MISO     19
#define LORA_DIO0     26
#define LORA_DIO1     35
#define LORA_RST      27
const LoRaWANBand_t Region = AS923_2;
const uint8_t subBand = 0;
EspHal* hal = new EspHal(LORA_SCK, LORA_MISO, LORA_MOSI);
SX1276 radio(new Module(hal, LORA_SS, LORA_DIO0, LORA_RST, LORA_DIO1));
LoRaWANNode node(&radio, &Region, subBand);
void debug(bool isFail, const char* message, int state, bool Freeze) {
        if (isFail) {
            ESP_LOGE("LoRaWAN", "%s (State: %d)", message, state);
            if (Freeze) {
                while (true) { vTaskDelay(pdMS_TO_TICKS(1000)); }
        }
    }
}
void arrayDump(uint8_t *buffer, uint16_t len) {
        for(uint16_t c = 0; c < len; c++) {
            printf("%02X ", buffer[c]);
    }
    printf("\n");
}
#endif