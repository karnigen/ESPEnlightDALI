#pragma once

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define UART_MSG_LEN        19

extern QueueHandle_t uart_queue;

void uart_task(void *pvParameters);
