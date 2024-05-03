#pragma once

#include <stdint.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define DALI_MAX_BYTES 4
#define DALI_MAX_BITS  (DALI_MAX_BYTES * 8)   // total bits

// Frame types: 101.7.4
typedef enum {
    DALI_MSG_FORWARD = 0,      // 16,24,32 bits
    DALI_MSG_BACKWARD,         // 8 bits
    DALI_MSG_RESERVED,         // 20 bits - should not be used
    DALI_MSG_PROPRIETARY,      // other, 8-to-15 bits may conflict with backward frames, priority only 5 101.8.3.2 tab 22
} dali_msg_type_t;

// Message status
typedef enum {
    DALI_FRAME_UNKNOWN = 0,     // status not defined
    DALI_FRAME_ERROR,           // error in frame
    DALI_FRAME_SEQUENCE_ERROR,  // such frame should be ignored
    DALI_FRAME_COLLISION,       // such frame should be ignored
    DALI_FRAME_SIZE_VIOLATION,  // such frame should be ignored
    DALI_FRAME_TIME_VIOLATION,  // such frame should be ignored
    DALI_FRAME_GRAY_AREA,       // allow multiple interpretations - such frame should be ignored
    DALI_FRAME_OK,              // frame is OK
} dali_msg_status_t;

// size is 8 bytes
struct Dali_msg {
    uint8_t id;                    // message ID (0-255)
    uint8_t type;                  // [dali_msg_type_t] type of message (forward, backward, reserved, proprietary)
                                   // receiving data contains time of reception from last edge change in [ms]
    uint8_t status;                // [dali_msg_status_t] status of message frame (0-255)
    uint8_t length;                // [1-32] length of data in bits: 8,16,24,32 bits or nonstandard eg 20 bits reserved
    uint8_t data[DALI_MAX_BYTES];
} __attribute__((packed));

typedef struct Dali_msg Dali_msg_t;

// DEFINE in main.c and then stored in dali.c in global variables
//   #define DALI_TX_PIN    ?
//   #define DALI_RX_PIN    ?
// ----------------------------------------------

// define HW DALI for gpio functions
#ifndef DALI_HW_PINS
    // define HW DALI for gpio functions
    // - what we should write to pin to get HIGH/LOW
    #define DALI_TX_HIGH        1             // idle state
    #define DALI_TX_LOW         0             // active state
    // - what we should read from pin to get HIGH/LOW
    #define DALI_RX_HIGH        1             // idle state
    #define DALI_RX_LOW         0             // active state
#endif

// LED onboard - debug
#define DALI_LED_PIN        2        // output D4 = GPIO2 esp8266 and esp32

// onboard LED uses inverted logic - debug
#define DALI_LED_ON         0
#define DALI_LED_OFF        1

// ----------------------------------------------

extern QueueHandle_t dali_send_queue;
extern QueueHandle_t dali_send_replay_queue;
extern QueueHandle_t dali_receive_queue;

extern uint8_t rx_debug_enabled;      // 1 - enable debug for received messages, timing, etc

// preferred way: pdMS_TO_TICKS(ms)         == ms * (configTICK_RATE_HZ ) / 1000
// old way:       ms / portTICK_PERIOD_MS   == ms / ( 1000 / configTICK_RATE_HZ )
inline void dali_delay_ms(TickType_t ms)  { vTaskDelay(pdMS_TO_TICKS(ms)); }

int dali_init_ports(uint8_t _dali_tx_pin, uint8_t _dali_rx_pin);
void dali_task(void *pvParameters);

