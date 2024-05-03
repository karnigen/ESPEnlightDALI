
#include <stdio.h>                   // default baudrate is 74880
#include <memory.h>                  // for memset

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"

// CONFIG_IDF_TARGET_ESP8266 CONFIG_IDF_TARGET_ESP32 CONFIG_IDF_TARGET_ESP32S2 ??
#ifdef CONFIG_IDF_TARGET_ESP8266
    #include "esp_timer.h"                // for timer
#else
    #include "esp_system.h"               // for timer
#endif
#include "driver/gpio.h"              // for GPIO
#include "driver/hw_timer.h"          // for timer


#include "dali_hw.h"


// public queue for sending and receiving data
QueueHandle_t dali_send_queue;        // [Dali_msg_t] from other tasks to dali task
QueueHandle_t dali_send_replay_queue; // [Dali_msg_t] from dali task to other tasks
QueueHandle_t dali_receive_queue;


// ----------------------------------------------

// internal queues for debug data
struct Dali_rx_dbg_data {
    uint32_t rx_pulse_width;
    uint32_t rx_tx_delta;
    uint8_t level;
};
typedef struct Dali_rx_dbg_data Dali_rx_dbg_data_t;
static QueueHandle_t rx_dbg_queue;      // [Dali_rx_dbg_data_t] from GPIO ISR to debug task, data:
uint8_t rx_debug_enabled = 0;           // enable debug data - set to 1 in main.c

static QueueHandle_t tx_queue;          // [Dali_msg_t] from task to HW TIMER ISR
static QueueHandle_t tx_reply_queue;    // [Dali_msg_t] from HW TIMER ISR to task - result about transmission
// static QueueHandle_t rx_queue;          // [Dali_msg_t] from GPIO ISR to task

//
// DALI speed: 1200 bps = 833us/bit half period = 416us
// DALI coding 1 = low to high, 0 = high to low
//

// Check if hardware is capable of multi master, very restricted timing
// #define DALI_MULTIMASTER

// -------------------------------------------------------------------------------------
// Common Timing: 101.8.1.1 tab 16
#define DALI_TIME_HB              416  // us = HB   - half bit
#define DALI_TIME_2HB             833  // us = 2*HB - full bit or double half bit
#define DALI_TIME_BUS_DOWN      45000  // us = 45ms = 108*HB  bus power down time


// we use hardware timer for timing and crystal so values are very accurate and we assume 416us pulse width
#ifndef DALI_MULTIMASTER
    // Transmit Timing: 101.8.1.1 tab 16
    #define DALI_TX_HB_MIN            366  // us = 0.88*HB
    #define DALI_TX_HB_MAX            467  // us = 1.12*HB
    #define DALI_TX_2HB_MIN           733  // us = 0.88*2*HB
    #define DALI_TX_2HB_MAX           934  // us = 1.12*2*HB
#else
    // Multimaster transmission: 101.8.3.1 tab 21
    #define DALI_TX_HB_MIN            400  // us = 0.96*HB
    #define DALI_TX_HB_MAX            433  // us = 1.04*HB
    #define DALI_TX_2HB_MIN           800  // us = 0.96*2*HB
    #define DALI_TX_2HB_MAX           867  // us = 1.04*2*HB
#endif

#define DALI_TX_STOP_COND        2450  // us = 2450/416 = 5.89 = 6*HB from last edge

// Transmit Timing: 101.8.1.2 tab 17
// backward frame are sent independently of bus state, even if bus is busy
#define DALI_TX_FF_BF_MIN        5500  // us = 5.5ms = 13*HB  forward to backward frame time interval
#define DALI_TX_FF_BF_MAX       10500  // us = 10.5ms = 25*HB

#define DALI_TX_FF_FF_MIN       13500  // us = 13.5ms = 32*HB   forward to forward frame time interval
#define DALI_TX_FF_FF_MAX       75500  // us = 75.5ms = 182*HB  twice forward frame 101.9.4

// -------------------------------------------------------------------------------------
// Receiving Timing:
#ifndef DALI_MULTIMASTER
    // Receiving Timing: 101.8.2.1 tab 18 and 19
    #define DALI_RX_HB_MIN            333  // us = 0.8*HB
    #define DALI_RX_HB_MAX            500  // us = 1.2*HB
    #define DALI_RX_2HB_MIN           666  // us = 0.8*2*HB
    #define DALI_RX_2HB_MAX          1000  // us = 1.2*2*HB
#else
    // Multimaster reception: 101.9.2.3 tab 23 and 24
    #define DALI_RX_HB_MIN            400  // us = 0.96*HB
    #define DALI_RX_HB_MAX            433  // us = 1.04*HB
    #define DALI_RX_2HB_MIN           800  // us = 0.96*2*HB
    #define DALI_RX_2HB_MAX           867  // us = 1.04*2*HB
#endif

#define DALI_RX_STOP_COND        2400 // us = 2400/416 = 5.77 = 6*HB from last edge

// Timing violation may inform about not identical backward frame: 101.9.6.2
// Timing violation for start bit
#define DALI_RX_START_TIMING_MIN  750  // us =  750/416 = 1.8 = 2*HB
#define DALI_RX_START_TIMING_MAX 1400  // us = 1400/416 = 3.4 = 4*HB

// Timing violation for other bits, see break time in collision detection
#define DALI_RX_BIT_TIMING_MIN   1200   // us = 1200/416 = 2.9 = 3*HB
#define DALI_RX_BIT_TIMING_MAX   1400   // us = 1400/416 = 3.4 = 4*HB

// collision recovery: 101.9.2.4 tab 25
#define DALI_TIME_BREAK_MIN      1200   // us = 1200/416 = 2.9 = 3*HB
#define DALI_TIME_RECOVERY_MIN   4000   // us = 4000/416 = 9.6 = 10*HB

// Receiver setting times: 101.8.2.4 tab 20
#define DALI_RX_FF_BF_MIN        2400  // us = 2400/416 = 5.77 = 6*HB  forward to backward frame time interval
#define DALI_RX_FF_BF_MAX       12400  // us = 12400/416 = 29.8 = 30*HB


#define DALI_RX_BF_FF_MIN        2400  // us = 2400/416 = 5.77 = 6*HB  backward to forward frame time interval

// -------------------------------------------------------------------------------------
// Collision detection:

// if we are transmitting we also listen to the bus in GPIO ISR, delay between TX and RX should be less then 84us
// - value is hardware specific but in worst case it should be 500-416=84us or 416-333=83us - we use 100us
// - multimaster is more restrictive 416-400=16us or 433-416=17us - we use 50us

#ifndef DALI_MULTIMASTER
    #define DALI_COLLISION_TXRX_DELTA     100  // us = 100/416 = 0.24 = 1/4*HB
#else
    #define DALI_COLLISION_TXRX_DELTA      50  // us =  50/416 = 0.12 = 1/8*HB
#endif

typedef enum {
    DALI_BUS_UNKNOWN = 0,        // after startup
    DALI_BUS_POWER_DOWN,         // bus power down
    DALI_BUS_ERROR,              // states <= this state are all errors
    DALI_BUS_READY,              // bus powered - ready to transmit or receive
    DALI_BUS_TRANSMITTING,       // bus is transmitting
    DALI_BUS_RECEIVING,          // bus is receiving

    DALI_BUS_TIME_BREAK,         // bus is in time break after collision 101.9.2.4 tab 25, now is LOW=ACTIVE
    DALI_BUS_RECOVERY,           // bus is in recovery after collision, wait for bus to be free, now is HIGH=IDLE
} dali_bus_state_t;

typedef enum {
    TX_STATE_ERROR = 0,
    TX_STATE_COLLISION,   // collision detected
    TX_STATE_IDLE,        // we are not transmitting
    TX_STATE_START,       // sending start bit
    TX_STATE_DATA,        // sending data bits
    TX_STATE_STOP,        // sending stop state
} dali_tx_state_t;

typedef enum {
    RX_STATE_ERROR = 0,
    RX_STATE_IDLE,        // we are not receiving
    RX_STATE_START,
    RX_STATE_DATA,
    RX_STATE_STOP,
    RX_STATE_END,         // final state with received data
} dali_rx_state_t;

// TX RX pins
static uint8_t dali_tx_pin;
static uint8_t dali_rx_pin;

#define WITHIN_RANGE(x, min, max) ((x) > (min) && (x) < (max))

#define DALI_SET_BUS_HIGH   gpio_set_level(dali_tx_pin, (DALI_TX_HIGH))            // set bus level
#define DALI_SET_BUS_LOW    gpio_set_level(dali_tx_pin, (DALI_TX_LOW))             // set bus level
#define DALI_SET_BUS_LEVEL(x) gpio_set_level(dali_tx_pin, ((x)==DALI_TX_HIGH))     // set bus level

// !!! read from RX pin, we need real bus level, not logic level of TX pin
// return:  0 - bus level low, active state
//          1 - bus level high, idle state
#define DALI_GET_BUS_LEVEL  (gpio_get_level(dali_rx_pin) == (DALI_RX_HIGH)) // get bus level
// return: 0 - tx pin drive bus low, active state
//         1 - tx pin drive bus high, idle state
#define DALI_GET_TX_LEVEL   (gpio_get_level(dali_tx_pin) == (DALI_TX_HIGH))  // get TX pin level


// TODO add get TX pin level  to check if we are keeping bus low !!!
// never use DALI_TX_HIGH DALI_TX_LOW after this define


// state variables
static dali_tx_state_t tx_state = TX_STATE_IDLE;
static dali_rx_state_t rx_state = RX_STATE_IDLE;
static dali_bus_state_t bus_state = DALI_BUS_UNKNOWN;

// TX RX timing
static uint64_t rx_last_edge_time=0;  // time of last edge
static uint64_t tx_last_edge_time=0;  // time of last edge

static uint32_t rx_tx_delta=0;        // delta between RX and TX - for collision detection
static uint32_t rx_pulse_width=0;     // region between two edges

static uint8_t  rx_level=0;           // level of RX pin

// TX RX data
static Dali_msg_t tx_data;        // data to send
static Dali_msg_t rx_data;        // received data

// TX counters
static uint8_t tx_half_bit_counter;
static uint8_t tx_data_bit_counter;      // actually sent bits count

// RX counters
static uint8_t rx_half_bit_counter;
static uint8_t rx_data_bit_counter;      // actually received bits count

// ISR in esp8266 cannot be interrupted by another ISR
// taskENTER_CRITICAL_FROM_ISR(); do nothing

// GPIO ISR handler
// define rx_gpio_isr_handler on any edge
void IRAM_ATTR rx_gpio_isr_handler(void* arg)
{
    static uint64_t rx_current_edge_time;
    static uint8_t  rx_previous_level;

    static Dali_rx_dbg_data_t dbg;

    // gpio_set_level(DALI_LED_PIN, !gpio_get_level(DALI_LED_PIN));  // toggle LED - debug

    rx_current_edge_time = esp_timer_get_time();  // get time in us
    rx_previous_level = rx_level;

    // rx_level = 1 if and only if DALI bus is really high, idle
    rx_level          = DALI_GET_BUS_LEVEL;  // get level of RX pin - not depend on hw: 0 - low, 1 - high

    rx_pulse_width    = rx_current_edge_time - rx_last_edge_time;  // time from last edge
    rx_tx_delta       = rx_current_edge_time - tx_last_edge_time;  // time from last edge

    // always save time of last edge
    rx_last_edge_time = rx_current_edge_time;  // get time in us

    if(bus_state == DALI_BUS_READY && rx_level == 0) // found start bit
    {
        // within range for backward frame
        uint32_t time_ms = rx_pulse_width / 1000;  // 1ms = 1000us
        if(time_ms>255) rx_data.type = 255;
        else            rx_data.type = time_ms;          //  pulse width in ms for later identification of type msg

        rx_data.id = 0;  // no ID
        rx_data.length = 0;
        rx_data.status = DALI_FRAME_UNKNOWN;         // on start in unknown state
        memset(rx_data.data, 0, DALI_MAX_BYTES);  // clear data

        rx_state = RX_STATE_START;       // start receiving
        bus_state = DALI_BUS_RECEIVING;  // bus is receiving
        rx_half_bit_counter = 0;
        rx_data_bit_counter = 0;         // actually received bits count
    }
    else if(bus_state == DALI_BUS_RECEIVING)
    {
        if(rx_state == RX_STATE_START) {
            // start bit always has width HB and level 1
            if(rx_level == 1 && WITHIN_RANGE(rx_pulse_width, DALI_RX_HB_MIN, DALI_RX_HB_MAX)) {
                rx_state = RX_STATE_DATA;  // start receiving data
                rx_half_bit_counter++;
            }
            else {
                bus_state = DALI_BUS_ERROR; // not in range
                rx_data.status = DALI_FRAME_TIME_VIOLATION;
                goto end_rx_isr;
            }
        }
        else if(rx_state == RX_STATE_DATA) {
            // for long gap there are two valid cases:
            // 1. previus_level == 1 and current_level == 0 then result is 0 bit
            // 2. previus_level == 0 and current_level == 1 then result is 1 bit

            // if for whatever reason we get the same level as previous we are in error, eg we missed edge
            if(rx_previous_level == rx_level) {
                    rx_state = RX_STATE_ERROR;            // invalid bit
                    rx_data.status = DALI_FRAME_SEQUENCE_ERROR;
                    goto end_rx_isr;
            }

            if(WITHIN_RANGE(rx_pulse_width, DALI_RX_2HB_MIN, DALI_RX_2HB_MAX))
            {
                if((rx_half_bit_counter & 0x01) == 0) {   // even half bit
                    rx_state = RX_STATE_ERROR;            // invalid bit
                    goto end_rx_isr;
                }
                if(rx_previous_level == 1) { // edge:1->0 value 0, skip because data[] is already set to 0
                }
                else {                       // edge:0->1 value 1
                    if(rx_level == 1) {      // result to: 1
                        rx_data.data[rx_data_bit_counter/8] |= (1 << (7 - (rx_data_bit_counter % 8)));  // set bit
                    }
                }
                rx_half_bit_counter+=2;          // 3,5,7...
                rx_data_bit_counter++;           // increment bit counter
            }
            else if(WITHIN_RANGE(rx_pulse_width, DALI_RX_HB_MIN, DALI_RX_HB_MAX))
            {

                if(rx_half_bit_counter & 0x01) { // first half of bit
                    rx_half_bit_counter++;
                }
                else { // second half of bit
                    rx_half_bit_counter++;
                    if(rx_previous_level == 1) { // edge:1->0 value 0 - skip because data[] is already set to 0
                    }
                    else {                       // edge:0->1 value 1
                        if(rx_level == 1) {      // 0->1 is 1
                            rx_data.data[rx_data_bit_counter/8] |= (1 << (7 - (rx_data_bit_counter % 8)));  // set bit
                        }
                    }
                    rx_data_bit_counter++;  // increment bit counter - only on second half
                }
                if(rx_data_bit_counter >= DALI_MAX_BITS) {
                    rx_state = RX_STATE_STOP;  // stop receiving - we cannot receive more bits
                }
            }
            else {
                rx_data.status = DALI_FRAME_TIME_VIOLATION;
                rx_state = RX_STATE_ERROR;            // invalid bit
                goto end_rx_isr;
            }

        }
    }


    // if collision detected: we are too late after bit was transmitted
    else if (bus_state == DALI_BUS_TRANSMITTING && rx_tx_delta > DALI_COLLISION_TXRX_DELTA)
    {
        // we need now to start collision recovery with time break: 101.9.2.4
        DALI_SET_BUS_LOW;   // force TX low - active state, inform about collision, this also generate new GPIO ISR

        tx_last_edge_time = esp_timer_get_time(); // get time in us
        bus_state = DALI_BUS_TIME_BREAK;          // we are in time break state
        tx_data.status = DALI_FRAME_COLLISION;   // collision detected
    }
    else if (bus_state == DALI_BUS_RECOVERY)
    {                               // we should not receive data during recover
        bus_state = DALI_BUS_ERROR; // not idle wait for idle state or cancel transmission and start receiving
    }

    // gpio_set_level(dali_tx_pin, !gpio_get_level(dali_tx_pin)); // test that immediately generate ISR on TX pin - debug

    end_rx_isr:

    if(rx_debug_enabled & 0x02) {
        dbg.level = rx_level;
        dbg.rx_pulse_width = rx_pulse_width;
        dbg.rx_tx_delta = rx_tx_delta;
        xQueueSendToBackFromISR(rx_dbg_queue, &dbg, NULL); // send data to queue
    }
}


// ---------------------------------------------------

// restrict max time delta to avoid overflow
#define MAX_DELTA_RELOAD_TIME 600000000  // 600s    - max u32: 4,294,967,295~4,294s

// HW TIMER ISR
// is triggered at interval of 416us (half bit)
void IRAM_ATTR hw_timer_callback(void *arg)  // IRAM_ATTR is needed for interrupt to speed up
{
    // time from last edge
    uint64_t time = esp_timer_get_time();  // get time in us
    uint32_t rx_delta = time - rx_last_edge_time;  // time from last edge
    uint32_t tx_delta = time - tx_last_edge_time;  // time from last edge

    // ----------------------------------------------
    if(rx_delta > MAX_DELTA_RELOAD_TIME)
    {
        rx_last_edge_time = time - MAX_DELTA_RELOAD_TIME/2;  // half of max time
    }
    if(tx_delta > MAX_DELTA_RELOAD_TIME)
    {
        tx_last_edge_time = time - MAX_DELTA_RELOAD_TIME/2;  // half of max time
    }
    // ----------------------------------------------

    // recovery from different error states: UNKNOWN, ERROR, POWER_DOWN
    if(bus_state <= DALI_BUS_ERROR)
    {   // 101.8.2.4 - startup BUS after 2.4ms
        if(rx_level==1 && rx_delta > DALI_RX_STOP_COND)
        {
            bus_state = DALI_BUS_READY;  // bus is ready
        }
    }
    // if bus power down - if bus is low for more then 45ms
    if(rx_level==0 && rx_delta > DALI_TIME_BUS_DOWN)
    {   // power lost
        bus_state = DALI_BUS_POWER_DOWN;  // bus is power down - recovery see previous if
        DALI_SET_BUS_HIGH;   // make sure TX is high

    }
    // recovery from collision detection
    // if end of TIME BREAK BUS is LOW - collision recovery: 101.9.2.4 tab 25
    //   BUS: IDLE, HIGH  - we are last device who keep bus low, if we want to restart transmission wait for TIME RECOVERY
    //   BUS: ACTIVE, LOW - BUS is busy, let caller to restart transmission
    if(bus_state == DALI_BUS_TIME_BREAK && rx_delta > DALI_TIME_BREAK_MIN)
    {
        DALI_SET_BUS_HIGH;   // TX high - idle state - generate ISR on RX pin

        tx_last_edge_time = esp_timer_get_time();   // get time in us
        // read bus state
        if(DALI_GET_BUS_LEVEL == 0) // other device is keeping bus low
        {
            // wait for bus to be idle before starting recovery - caller will restart transmission
            bus_state = DALI_BUS_ERROR;  // bus is in multi master state: 101.9.2.4 Fig 17
        }
        else
        {
            // bus is free - we are the last one who keeps bus low, we can start recovery
            bus_state = DALI_BUS_RECOVERY;   // start recovery for 4ms: 101.9.2.4 tab 25
            // TODO: add check if we want to restart transmission !!!
        }
    }
    else if(bus_state == DALI_BUS_RECOVERY && rx_delta > DALI_TIME_RECOVERY_MIN)
    {
        bus_state = DALI_BUS_READY;  // bus is ready
        // immediately start transmitting if we have data
    }

    // start transmitting
    else if(bus_state == DALI_BUS_READY && tx_state == TX_STATE_IDLE && xQueueReceiveFromISR(tx_queue, &tx_data, NULL) == pdTRUE)
    {
        tx_data.status = DALI_FRAME_ERROR;  // error status - will be set ok on success
        bus_state = DALI_BUS_TRANSMITTING;  // bus is transmitting
        tx_state = TX_STATE_START;  // start transmitting
        tx_half_bit_counter = 0;
        tx_data_bit_counter = 0;                   // actually sent bits count
        DALI_SET_BUS_LOW;   // start bit first half

        tx_last_edge_time = esp_timer_get_time();  // get time in us
    }
    else if(bus_state == DALI_BUS_TRANSMITTING)
    {
        // transmit data
        if(tx_state == TX_STATE_START) {
            tx_state = TX_STATE_DATA;  // start transmitting data
            tx_half_bit_counter++;
            DALI_SET_BUS_HIGH;         // start bit second half

            tx_last_edge_time = esp_timer_get_time();  // get time in us
        }
        else if(tx_state == TX_STATE_DATA) {
            bool value = (tx_data.data[tx_data_bit_counter/8] >> ( 7 - (tx_data_bit_counter % 8) )) & 0x01;
            value ^= tx_half_bit_counter & 0x01; // xor=invert value for odd half bit 1:0->1 and 0:1->0
            DALI_SET_BUS_LEVEL(value);

            tx_last_edge_time = esp_timer_get_time();  // get time in us
            tx_half_bit_counter++;               // increment half bit counter before next test
            if(tx_half_bit_counter & 0x01) {     // next bit
                tx_data_bit_counter++;
                if(tx_data_bit_counter >= tx_data.length) {
                    tx_state = TX_STATE_STOP;
                }
            }
        }
        else if(tx_state == TX_STATE_STOP) {
            // here we check TX (NOT RX) bit state
            if(DALI_GET_TX_LEVEL == 0) // really ok - otherwise we will keep bus low forever
            {
                DALI_SET_BUS_HIGH;
                tx_last_edge_time = esp_timer_get_time();  // get time in us
            }
            else if(tx_delta > DALI_TX_STOP_COND) {
                tx_data.status = DALI_FRAME_OK;  // frame is OK
                xQueueSendToBackFromISR(tx_reply_queue, &tx_data, NULL); // send data to queue
                tx_state = TX_STATE_IDLE;  // final state with transmitted data
                bus_state = DALI_BUS_READY;  // bus is ready
            }
        }
    }
    else if(bus_state == DALI_BUS_READY && tx_state > TX_STATE_IDLE)
    {
        // we are not transmitting but we have data - replay to queue and let error state in tx_data.status
        xQueueSendToBackFromISR(tx_reply_queue, &tx_data, NULL); // send data to queue
        tx_state = TX_STATE_IDLE;  // clear state
    }

    // recover receiving
    if(bus_state == DALI_BUS_RECEIVING) {
        if(rx_state == RX_STATE_ERROR) {
            // wait until bus is idle
            if(rx_delta > DALI_RX_STOP_COND) { // minimum time 2400us
                rx_state = RX_STATE_IDLE;
                bus_state = DALI_BUS_READY;
                // rx_data.status = DALI_FRAME_ERROR;  // should be set inside ISR
                rx_data.length = rx_data_bit_counter;  // set length of data
                // rx_data.data[0] = 0xAA;  // debug
                xQueueSendToBackFromISR(dali_receive_queue, &rx_data, NULL); // send data to queue
            }
        }
        else if(rx_state == RX_STATE_DATA || rx_state == RX_STATE_STOP) {
            // wait until bus is idle
            if(rx_delta > DALI_RX_STOP_COND) { // minimum time 2400us
                rx_state = RX_STATE_IDLE;
                bus_state = DALI_BUS_READY;
                rx_data.status = DALI_FRAME_OK;  // frame is OK
                rx_data.length = rx_data_bit_counter;  // set length of data
                // rx_data.data[0] = 0xBB;  // debug
                xQueueSendToBackFromISR(dali_receive_queue, &rx_data, NULL); // send data to queue
            }
        }
    }

}

// ---------------------------------------------------
// should be at lowest priority
void debug_task(void *pvParameters)
{
    Dali_rx_dbg_data_t dbg;
    static uint8_t i=0;
    static uint8_t HB=0;   // half bit counter
    char v = '*';  // interpret value
    while (1)
    {
        // gpio_set_level(LED_PIN, !gpio_get_level(LED_PIN));  // toggle LED
        if(xQueueReceive(rx_dbg_queue, &dbg, portMAX_DELAY) == pdTRUE) // wait forever
        {
            if(dbg.rx_pulse_width > 1000) { i=0; HB=0; v='*';}
            else if ((HB & 0x01) == 1) {
                if(HB == 2) v = 'S';
                else v = '0' + dbg.level;
            }
            else v=' ';
            printf("rx: [%2d] pw=%d v=%u rtd=%d [%c]\n", i++, dbg.rx_pulse_width, dbg.level, dbg.rx_tx_delta, v);
            if(dbg.rx_pulse_width < 1000) {
                if(dbg.rx_pulse_width > 550) HB+=2;
                else HB++;
            }
        }
    }
}



int dali_init_ports(uint8_t _dali_tx_pin, uint8_t _dali_rx_pin)
{
    dali_tx_pin = _dali_tx_pin;
    dali_rx_pin = _dali_rx_pin;

    gpio_config_t io_conf;

    // DALI TX
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT(dali_tx_pin);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // DALI RX
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = BIT(dali_rx_pin);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;                   // no pull-up or pull-down - bus has default pull-down
    gpio_config(&io_conf);

    // set initial state
    DALI_SET_BUS_HIGH;   // TX high - idle state

    // queue
    dali_send_queue = xQueueCreate(10, sizeof(Dali_msg_t));
    dali_send_replay_queue = xQueueCreate(10, sizeof(Dali_msg_t));

    dali_receive_queue = xQueueCreate(50, sizeof(Dali_msg_t));
    rx_dbg_queue = xQueueCreate(100, sizeof(Dali_rx_dbg_data_t));

    // internal queues
    tx_queue = xQueueCreate(1, sizeof(Dali_msg_t));
    tx_reply_queue = xQueueCreate(4, sizeof(Dali_msg_t));
    // rx_queue = xQueueCreate(4, sizeof(DALI_message_t));

    xTaskCreate(debug_task, "debug_task", 2048, NULL, 1, NULL);  // at low priority !!!

    // init gpio isr service
    gpio_install_isr_service(0);  // install gpio isr service, error means that isr is already installed
    ESP_ERROR_CHECK(gpio_isr_handler_add(dali_rx_pin, rx_gpio_isr_handler, NULL));  // add isr handler for specific gpio pin

    rx_last_edge_time = esp_timer_get_time();  // get time in us - startup time
    rx_level = DALI_GET_BUS_LEVEL;              // get level of RX pin

    ESP_ERROR_CHECK(gpio_set_intr_type(dali_rx_pin, GPIO_INTR_ANYEDGE));     // enable isr handler for any edge

    // init timer
    tx_last_edge_time = esp_timer_get_time();  // get time in us - startup time
    ESP_ERROR_CHECK(hw_timer_init(hw_timer_callback, NULL));
    ESP_ERROR_CHECK(hw_timer_alarm_us(DALI_TIME_HB, true));  // 1_000_000 = 1s timer with reload

    return ESP_OK;
}

// communication with ISR - send data to queue and wait for reply
int dali_tx(Dali_msg_t *dali_msg)
{
    if(xQueueSendToBack(tx_queue, dali_msg, pdMS_TO_TICKS(50)) == pdFALSE) {
        xQueueReset(tx_queue);            // clear queue
        printf("dali_tx: Queue full\n");
        return ESP_FAIL;
    }
    // printf("data send to queue\n");  // debug portTICK_PERIOD_MS
    if(xQueueReceive(tx_reply_queue, dali_msg, pdMS_TO_TICKS(50)) == pdFALSE) {
        xQueueReset(tx_reply_queue);      // clear queue
        printf("dali_tx: No reply\n");
        return ESP_FAIL;
    }
    // printf("data received from queue\n"); // debug

    return ESP_OK;
}

// dali_task - should run at highest priority
void dali_task(void *pvParameters)
{
    Dali_msg_t dali_msg;
    while (1)
    {
        // data from queue -> copy to local variable
        if(xQueueReceive(dali_send_queue, &dali_msg, portMAX_DELAY) == pdTRUE) {
            dali_tx(&dali_msg);
            // send data to queue
            xQueueSendToBack(dali_send_replay_queue, &dali_msg, 0);

        }
    }
}

