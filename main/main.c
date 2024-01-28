/* Scan Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
    This example shows how to scan for available set of APs.
*/
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "crc32.c"

#define DEFAULT_SCAN_LIST_SIZE CONFIG_EXAMPLE_SCAN_LIST_SIZE

static const char *TAG = "prng";

TaskHandle_t  prngTask = NULL;
QueueHandle_t prngQueue = NULL;


void wifi_scan(void* buf, wifi_promiscuous_pkt_type_t packet_type) {
    switch (packet_type) {
        case WIFI_PKT_DATA:
        case WIFI_PKT_CTRL:
        case WIFI_PKT_MGMT: {

            wifi_promiscuous_pkt_t* packet = (wifi_promiscuous_pkt_t*)buf;
            uint32_t hash = crc32(1337, packet->payload, packet->rx_ctrl.sig_len);
            
            // Enqueue the hash
            xQueueSend(
                prngQueue,
                (void*)&hash,
                0
            );

            break;
        }

        default: {
            break;
        }
    }
}

void queue_worker(void* params) {
    uint16_t mask = 1;
    uint16_t rnd  = 0;
    uint32_t digest;

    while(1) {
        if (prngQueue != NULL) {
            if (xQueueReceive(prngQueue, &digest, 0) == pdPASS) {
                if (digest % 2 == 1) {
                    rnd |= mask;
                }

                if (mask == (1 << 15)) {
                    ESP_LOGI(TAG, "%d", rnd);
                    mask = 1;
                    rnd = 0;
                } else {
                    mask <<= 1;
                }
            }
        }
    }
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    
    // Init wifi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    prngQueue = xQueueCreate(1024, sizeof(uint32_t));
    xTaskCreate(
        queue_worker,
        "PRNG WORKER",
        2048,
        (void*)1,
        0,
        &prngTask
    );

    // Enter promiscuous mode
    esp_wifi_set_promiscuous_rx_cb(&wifi_scan);
    esp_wifi_set_promiscuous(true);

    ESP_LOGI(TAG, "Configuration complete!");

}