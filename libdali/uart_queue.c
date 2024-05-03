
#include <stdio.h>                 // default baudrate is 74880
#include <memory.h>                // for memset
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"

#include "driver/uart.h" // to set baudrate

#include "uart_queue.h"

#define BUF_SIZE (128)

QueueHandle_t uart_queue;

void uart_task(void *pvParameters)
{
    // create uart queue for 10 messages of string with length of 20 bytes
    uart_queue = xQueueCreate(10, UART_MSG_LEN+1);

    // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE+1);
    uint8_t *string = (uint8_t *) malloc(UART_MSG_LEN+1);
    int string_len = 0;

    uart_config_t uart_config = {
        .baud_rate = 74880,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0);

    while (1) {
        // Read data from the UART
        int len = uart_read_bytes(0, data, BUF_SIZE, pdMS_TO_TICKS(20));
        if (len > 0) {
            // echo
            uart_write_bytes(0, (const char *) data, len);

            // copy to not overflow but end on \n or \r
            for (int i = 0; i < len; i++)
            {
                if (data[i] == '\n' || data[i] == '\r') {
                    string[string_len] = 0;
                    printf("> %d: %s\n", string_len, string);
                    if(xQueueSendToBack(uart_queue, string, 0)== pdFAIL) // send message to DALI task
                    {
                        printf("UART queue full\n");
                    }
                    string_len = 0;
                }
                else {
                    string[string_len] = data[i];
                    if (string_len < UART_MSG_LEN) {
                        string_len++;
                    }
                }
            }
        }
    }

}
