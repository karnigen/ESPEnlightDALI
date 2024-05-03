
// ESP8266 RTOS SDK User Manual
// https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/api-reference/index.html

// FreeRTOS sdk:
// https://readthedocs.com/projects/espressif-esp8266-rtos-sdk/downloads/pdf/latest/

#include <stdio.h>                 // default baudrate is 74880
#include <memory.h>                // for memset
#include <string.h>
#include <inttypes.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
// #include "esp_spi_flash.h"
// #include "esp_timer.h"             // for timer
#include "driver/gpio.h"           // for GPIO


#include "dali_hw.h"
#include "dali.h"
#include "dali_define.h"
#include "uart_queue.h"
#include "uart_parser.h"
#include "user.h"
#include "main.h"

// Define GPIO pins for our project


void init_led() {
    // init LED
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT(LED_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;

    gpio_config(&io_conf);
    gpio_set_level(LED_PIN, LED_OFF);  // LED off
}

// ----------------------------------------------


// ----------------------------------------------
// special debug function
// - avoid sending valid DALI messages - use nonstandart count of bits
// - TODO add recovery times to avoid errors states
void dali_test() {
    static uint8_t i=0;
    Dali_msg_t dali_msg = dali_msg_new(0xAA, 0xAA);
    dali_msg.id = i;
    dali_msg.length  = 4;

    dali_send_double(&dali_msg);
    printf("i=%d id=%d ty=%d st=%d \n", i, dali_msg.id, dali_msg.type, dali_msg.status);

    dali_delay_ms(5000); // delay ms
    i++;
}

// ----------------------------------------------


// void esp_info()
// {
//     esp_chip_info_t chip_info;
//     esp_chip_info(&chip_info);
//     printf("*** This is ESP model: %d [0-ESP8266 1-ESP32] chip with %d CPU cores, WiFi, ", chip_info.model, chip_info.cores);
//     printf("*** silicon revision %d, ", chip_info.revision);
//     printf("*** %dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
//                                  (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
//     printf("*** SDK version:%s\n", esp_get_idf_version());  // old: system_get_sdk_version()

//     uint8_t mac[6];  // MAC address is 6 bytes
//     ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));
//     printf("*** MAC address from eFUSE: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
// }


// main task
void app_main()
{
    init_led(); // init LED

    /* Print chip information */
    // esp_info();

    vTaskPrioritySet(NULL, 3);   // change task priority to 2
    printf("*** dali_task priority: %d\n", uxTaskPriorityGet(NULL)); // print task priority


    // Init DALI - set TX and RX pins
    dali_init_ports(DALI_TX_PIN, DALI_RX_PIN);                  // initialize DALI ports
    xTaskCreate(dali_task, "dali_task", 4096, NULL, 5, NULL);   // create DALI task at high priority
    xTaskCreate(uart_task, "uart_task", 4096, NULL, 1, NULL);   // create UART task at low priority
    xTaskCreate(user_task, "user_task", 4096, NULL, 2, NULL);   // create USER task at medium priority

    dali_delay_ms(100);  // delay 100ms to setup queues and uart


    // for(;;) dali_test(); // test DALI

    uart_parser(); // parse UART messages

    for(;;);
}
