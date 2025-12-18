#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "nvs.h"

#include <RadioLib.h>
#include "config.h"

static const char *TAG = "LORA_APP";

RTC_DATA_ATTR uint16_t bootCount = 1;
RTC_DATA_ATTR uint16_t bootCountSinceUnsuccessfulJoin = 0;
RTC_DATA_ATTR uint8_t LWsession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];

static esp_err_t nvs_save_bytes(const char* key, const uint8_t* data, size_t len) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("radiolib", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(my_handle, key, data, len);
    if (err == ESP_OK) err = nvs_commit(my_handle);
    nvs_close(my_handle);
    return err;
}

static bool nvs_load_bytes(const char* key, uint8_t* buffer, size_t len) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("radiolib", NVS_READONLY, &my_handle);
    if (err != ESP_OK) return false;
    size_t required_size = len;
    err = nvs_get_blob(my_handle, key, buffer, &required_size);
    nvs_close(my_handle);
    return (err == ESP_OK && required_size == len);
}

static void print_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "Wake from sleep");
    } else {
        ESP_LOGI(TAG, "Wake not caused by deep sleep: %d", (int)wakeup_reason);
    }
    ESP_LOGI(TAG, "Boot count: %u", (unsigned)bootCount++);
}

static void gotoSleep(uint32_t seconds) {
    ESP_LOGI(TAG, "Sleeping for %lu seconds", (unsigned long)seconds);
    esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
    /* flush logs a short while before sleeping */
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_deep_sleep_start();
    ESP_LOGE(TAG, "Sleep failed!");
}

/* app_main is C linkage entry for ESP-IDF when using C++ file */
extern "C" void app_main() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* short startup delay */
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "--- System Start ---");
    print_wakeup_reason();

    ESP_LOGI(TAG, "Initialise the radio");
    int16_t state = radio.begin();
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "Init radio failed, code: %d", state);
        ESP_LOGE(TAG, "Connection to LoRa module failed. Check wiring.");
        /* cannot proceed if radio init failed */
        for (;;) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    } else {
        ESP_LOGI(TAG, "Connection to LoRa module successful.");
    }

    node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);

    ESP_LOGI(TAG, "Recalling LoRaWAN nonces & session");
    uint8_t buffer_nonces[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
    if (nvs_load_bytes("nonces", buffer_nonces, RADIOLIB_LORAWAN_NONCES_BUF_SIZE)) {
        state = node.setBufferNonces(buffer_nonces);
        if (state != RADIOLIB_ERR_NONE) {
            ESP_LOGW(TAG, "Restoring nonces buffer failed: %d", state);
        } else {
            ESP_LOGI(TAG, "Nonces restored from NVS");
        }
    }

    state = node.setBufferSession(LWsession);
    if (state != RADIOLIB_ERR_NONE && bootCount > 2) {
        ESP_LOGW(TAG, "Restoring session buffer failed: %d", state);
    }

    /* Join loop */
    while ((state != RADIOLIB_LORAWAN_NEW_SESSION) && (state != RADIOLIB_LORAWAN_SESSION_RESTORED)) {
        ESP_LOGI(TAG, "Join ('login') to the LoRaWAN Network");
        state = node.activateOTAA();

        node.setDwellTime(false);

        if ((state != RADIOLIB_LORAWAN_NEW_SESSION) && (state != RADIOLIB_LORAWAN_SESSION_RESTORED)) {
            ESP_LOGW(TAG, "Join failed: %d", state);
            uint32_t sleepForSeconds = (uint32_t)((bootCountSinceUnsuccessfulJoin++ + 1) * 60);
            if (sleepForSeconds > 180) sleepForSeconds = 180;
            ESP_LOGI(TAG, "Retrying join in %lu seconds", (unsigned long)sleepForSeconds);
            gotoSleep(sleepForSeconds);
        } else {
            ESP_LOGI(TAG, "Joined!");
            ESP_LOGI(TAG, "Saving nonces to NVS");
            uint8_t *persist = node.getBufferNonces();
            if (persist) {
                nvs_save_bytes("nonces", persist, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
            }
            bootCountSinceUnsuccessfulJoin = 0;
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    ESP_LOGI(TAG, "Sending uplink");
    uint8_t value1 = 0x12;
    uint16_t value2 = 0xBEEF;
    uint8_t uplinkPayload[3];
    uplinkPayload[0] = value1;
    uplinkPayload[1] = (uint8_t)(value2 >> 8);
    uplinkPayload[2] = (uint8_t)(value2 & 0xFF);

    uint8_t fPort = 1;
    uint8_t downlinkPayload[256];
    size_t downlinkSize = 0;

    state = node.sendReceive(uplinkPayload, sizeof(uplinkPayload), fPort, downlinkPayload, &downlinkSize);
    if (state != RADIOLIB_LORAWAN_DOWNLINK && state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "Error in sendReceive: %d", state);
    } else {
        ESP_LOGI(TAG, "Uplink sent. FcntUp: %u", (unsigned)node.getFCntUp());
    }

    if (state != RADIOLIB_LORAWAN_DOWNLINK) {
        if (downlinkSize > 0) {
            ESP_LOGI(TAG, "Downlink received (%u bytes):", (unsigned)downlinkSize);
            char hexline[128];
            size_t pos = 0;
            for (size_t i = 0; i < downlinkSize; ++i) {
                int n = snprintf(hexline + pos, sizeof(hexline) - pos, "%02X ", downlinkPayload[i]);
                if (n < 0) break;
                pos += n;
                if (pos + 8 >= sizeof(hexline)) {
                    printf("%s\n", hexline);
                    pos = 0;
                }
            }
            if (pos > 0) printf("%s\n", hexline);
        } else {
            ESP_LOGI(TAG, "<MAC commands only>");
        }
    }

    uint8_t *persist = node.getBufferSession();
    if (persist) {
        memcpy(LWsession, persist, RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
    }

    ESP_LOGI(TAG, "Going to sleep for %u seconds", (unsigned)uplinkIntervalSeconds);
    gotoSleep(uplinkIntervalSeconds);

    /* Should never reach here */
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
