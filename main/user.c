
#include <stdio.h>                 // default baudrate is 74880
#include <memory.h>                // for memset
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_timer.h"             // for timer
#include "driver/gpio.h"           // for GPIO


#include "user.h"
#include "main.h"

void user_task(void *pvParameters)
{
    printf("Hello from user task\n");
    for(;;)
    {
        // change led state
        // gpio_set_level(LED_PIN, LED_ON);
        // vTaskDelay(pdMS_TO_TICKS(50));
        // gpio_set_level(LED_PIN, LED_OFF);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}