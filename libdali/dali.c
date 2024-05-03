#include "dali.h"
#include "dali_hw.h"
#include "dali_define.h"

#include <memory.h>                // for memset

Dali_msg_t dali_msg_new_generic(uint8_t bit_length, uint8_t address, uint8_t cmd1, uint8_t cmd2, uint8_t cmd3) {
    Dali_msg_t dali_msg;
    dali_msg.id = 0;
    dali_msg.type = DALI_MSG_FORWARD;
    dali_msg.status = 0;
    dali_msg.length  = bit_length;         // bit count 1-32
    dali_msg.data[0] = address;
    dali_msg.data[1] = cmd1;
    dali_msg.data[2] = cmd2;
    dali_msg.data[3] = cmd3;
    return dali_msg;
}

Dali_msg_t dali_msg_new(uint8_t address, uint8_t cmd1) {
    return dali_msg_new_generic(16, address, cmd1, 0, 0);
}

Dali_msg_t dali_msg_new_3B(uint8_t address, uint8_t cmd1, uint8_t cmd2) {
    return dali_msg_new_generic(24, address, cmd1, cmd2, 0);
}

Dali_msg_t dali_msg_new_4B(uint8_t address, uint8_t cmd1, uint8_t cmd2, uint8_t cmd3) {
    return dali_msg_new_generic(32, address, cmd1, cmd2, cmd3);
}

// ----------------------------------------------------------------------------------------------

// send message to DALI task - not expecting replay
// TODO also restrict delay - to allow recovery
void dali_send(Dali_msg_t *tx_msg) {
    xQueueSendToBack(dali_send_queue, tx_msg, portMAX_DELAY); // send message to DALI task
    xQueueReceive(dali_send_replay_queue, tx_msg, portMAX_DELAY); // always wait for replay that message was sent
    // TODO check status
    if(rx_debug_enabled & 0x01) {
        printf("** tm=%d[ms] st=%d  len=%d d0=%d [0x%02X] d1=%d [0x%02X]\n",
            tx_msg->type, tx_msg->status, tx_msg->length, tx_msg->data[0], tx_msg->data[0], tx_msg->data[1], tx_msg->data[1]);
    }
}

// send double message
void dali_send_double(Dali_msg_t *dali_msg) {
    dali_send(dali_msg);
    // TODO check status
    dali_msg->id++; // increment message ID
    dali_delay_ms(10);                      // delay 13ms 101.8.1.2: 13.5 - 75ms
    dali_send(dali_msg);
    // TODO check status
}


int dali_query(Dali_msg_t *tx_msg, Dali_msg_t *rx_msg) {
    BaseType_t ret;

    // TODO check empty queue
    if(xQueueReceive(dali_receive_queue, rx_msg, 0) == pdTRUE) {
        printf("Queue not empty\n");
        return -1;
    }
    // printf("check A tx=%d tm=%d[ms]    st=%d  len=%d d0=0x%X\n", ret, tx_msg->type,   tx_msg->status, tx_msg->length, tx_msg->data[0]);

    dali_send(tx_msg);
     // receive message from DALI task
    ret = xQueueReceive(dali_receive_queue, rx_msg, pdMS_TO_TICKS(50));
    // printf("B rx=%d tm=%d[ms] st=%d  len=%d d0=0x%X\n", ret, rx_msg->type, rx_msg->status, rx_msg->length, rx_msg->data[0]);
    return ret;
}


// ----------------------------------------------------------------------------------------------

// addr: 0-63
uint32_t dali_cmd_query_rand_addr24(uint8_t addr)
{
    Dali_msg_t tx, rx;
    uint32_t addr24 = 0;

    uint8_t device = dali_short_address(addr);
    tx = dali_msg_new(device, DALI_CMD_QUERY_RAND_ADDR_H);
    if(dali_query(&tx, &rx) == pdTRUE) {
        addr24 |= rx.data[0] << 16;
    }
    else {
        printf("Cannot query long address H\n");
        return -1;
    }

    tx = dali_msg_new(device, DALI_CMD_QUERY_RAND_ADDR_M);
    if(dali_query(&tx, &rx) == pdTRUE) {
        addr24 |= rx.data[0] << 8;
    }
    else {
        printf("Cannot query long address M\n");
        return -1;
    }

    tx = dali_msg_new(device, DALI_CMD_QUERY_RAND_ADDR_L);
    if(dali_query(&tx, &rx) == pdTRUE) {
        addr24 |= rx.data[0];
    }
    else {
        printf("Cannot query long address L\n");
        return -1;
    }

    return addr24;
}

// commands
void dali_cmd(uint8_t device, uint8_t cmd)
{
    Dali_msg_t tx;
    tx = dali_msg_new(device, cmd);
    dali_send(&tx);
}
void dali_cmd_double(uint8_t device, uint8_t cmd)
{
    Dali_msg_t tx;
    tx = dali_msg_new(device, cmd);
    dali_send_double(&tx);
}


// device: 102.7.2.1 table 1
void dali_cmd_off(uint8_t device)
{
    dali_cmd(device, DALI_CMD_OFF);
}

void dali_cmd_on(uint8_t device)
{
    dali_cmd(device, DALI_CMD_ON);
}

// this is broadcast command - all devices set DTR0
void dali_set_dtr0(uint8_t value)
{
    dali_cmd(DALI_CMD_DTR0, value);
}

void dali_set_dtr1(uint8_t value)
{
    dali_cmd(DALI_CMD_DTR1, value);
}
void dali_set_dtr2(uint8_t value)
{
    dali_cmd(DALI_CMD_DTR2, value);
}

// LEVELS
void dali_set_level(uint8_t device, uint8_t cmd, uint8_t level)
{
    Dali_msg_t tx;
    dali_set_dtr0(level);
    tx = dali_msg_new(device, cmd);
    dali_send_double(&tx);
}
// GROUP add or remove
// group: 0-15
void dali_group(uint8_t device1, uint8_t cmd, uint8_t group)
{
    Dali_msg_t tx;
    cmd += group & 0x0F;
    tx = dali_msg_new(device1, cmd);
    dali_send_double(&tx);
}
// ----------------------------------------------------------------------------------------------
// device address: 0-63

#define _QQ(cmd, val) \
    tx = dali_msg_new(device, cmd); \
    if(dali_query(&tx, &rx) == pdTRUE) { \
        val = rx.data[0]; \
    } \
    else printf("Cannot query: %s\n", #cmd); \

void dali_device_info(uint8_t addr)
{
    Dali_msg_t tx, rx;
    // int ret;

    uint8_t device = dali_short_address(addr);
    uint32_t addr24 = 0;
    uint8_t op=0, status=0, dtr0=0, dtr1=0, dtr2=0;
    uint8_t levelActual=0, levelMax=0, levelMin=0, levelPowerOn=0, levelSystemFail=0, levelPhysMin=0;
    uint8_t fadeTime=0, extFadeTime=0;

    addr24 = dali_cmd_query_rand_addr24(addr);

    _QQ(DALI_CMD_QUERY_OPERATING_MODE, op);
    _QQ(DALI_CMD_QUERY_STATUS, status);
    _QQ(DALI_CMD_QUERY_DTR0, dtr0);
    _QQ(DALI_CMD_QUERY_DTR1, dtr1);
    _QQ(DALI_CMD_QUERY_DTR2, dtr2);

    printf("Device %2d [0x%zX]: MODE=0x%02X STAT=0x%02X DTR0=%3d [0x%02X] DTR1=%3d [0x%02X] DTR2=%3d [0x%02X]\n",
            addr, (size_t)addr24, op, status, dtr0, dtr0, dtr1, dtr1, dtr2, dtr2);

    _QQ(DALI_CMD_QUERY_ACTUAL_LEVEL, levelActual);
    _QQ(DALI_CMD_QUERY_MAX_LEVEL, levelMax);
    _QQ(DALI_CMD_QUERY_MIN_LEVEL, levelMin);
    _QQ(DALI_CMD_QUERY_POWER_ON_LEVEL, levelPowerOn);
    _QQ(DALI_CMD_QUERY_SYSTEM_FAILURE_LEVEL, levelSystemFail);
    _QQ(DALI_CMD_QUERY_PHYSICAL_MIN, levelPhysMin);
    _QQ(DALI_CMD_QUERY_FADE_TIME, fadeTime);
    _QQ(DALI_CMD_QUERY_EXT_FADE_TIME, extFadeTime);

    printf("* LEVELS ACT=%3d MAX=%3d MIN=%3d PMIN=%3d PWON=%3d SYSF=%3d FTM=0x%02X EFTM=0x%02X\n",
              levelActual, levelMax, levelMin, levelPhysMin, levelPowerOn, levelSystemFail, fadeTime, extFadeTime);

    // groups
    uint8_t group0=0, group1=0;
    _QQ(DALI_CMD_QUERY_GROUPS_0_7, group0);
    _QQ(DALI_CMD_QUERY_GROUPS_8_15, group1);
    printf("* GROUPS 0-7=0x%02X 8-15=0x%02X\n", group0, group1);
    uint8_t scenes[16];
    for(int i=0; i<16; i++) {
        _QQ(DALI_CMD_QUERY_SCENE_LEVEL + i, scenes[i]);
    }
    printf("* SCENES: ");
    for(int i=0; i<16; i++) {
        printf("%3d ", scenes[i]);
        if(i % 8 == 7) printf("  ");
    }
    printf("\n");

}
#undef _QQ   // undefine macro

void dali_device_identify(uint8_t device)
{
    Dali_msg_t tx;
    tx = dali_msg_new(device, DALI_CMD_IDENTIFY);
    dali_send_double(&tx);
}

// reset
// - random  24-bit address to 0xFFFFFF
// - levels to default values, clear groups and scenes
void dali_device_reset(uint8_t device)
{
    Dali_msg_t tx;
    tx = dali_msg_new(device, DALI_CMD_RESET);
    dali_send_double(&tx);
    dali_delay_ms(300); // wait for reset
}



// try identify all devices by asking for operating mode
// - check from 0 to 63
// - uint8 dev[64]
void dali_find_all_short(uint8_t *dev, int with_print)
{
    printf("Identifying all devices\n");
    Dali_msg_t tx, rx;
    int ret;
    memset(dev, 0, 64);
    int cnt = 0;
    for(int i=0; i<64; i++)
    {
        tx = dali_msg_new(dali_short_address(i), DALI_CMD_QUERY_OPERATING_MODE);
        ret = dali_query(&tx, &rx);
        if(ret == pdTRUE) {
            dev[i] = 1;
            cnt++;
            // printf("Device %2d: operating mode: 0x%X\n", i, rx.data[0]);
            if(with_print) printf("%d", i % 10);
        }
        else {
            if(with_print) printf(".");
        }
        if(with_print) {
            fflush(stdout);
            dali_delay_ms(100);
        }
    }
    if(with_print) printf(" \n");

    printf("Found %d devices:", cnt);
    for(int i=0; i<64; i++) if(dev[i]) printf(" %d", i);
    printf("\n");
}

// ----------------------------------------------------------------------------------------------


// RANDOMIZE
// device: 0AAA AAA1 - only device with short address
//         1111 1111 - all devices without short address
//         0000 0000 - all devices
void dali_cmd_randomize(uint8_t device)
{
    Dali_msg_t tx;
    dali_cmd_initialise(device); // 0x00=ALL 0xFF=without short address 0b0AAA AAA1=with short address

    // randomize 24-bit address
    tx = dali_msg_new(DALI_CMD_RANDOMISE, 0x00);
    dali_send_double(&tx);

    dali_delay_ms(100); // wait for randomize
    dali_cmd_terminate();
    printf("*** Randomize end\n");
}

// - requires INITIALISE state
void dali_set_search_addr24(uint32_t addr24)
{
    Dali_msg_t tx;
    tx = dali_msg_new(DALI_CMD_SEARCHADDRH, addr24 >> 16);
    dali_send(&tx);
    tx = dali_msg_new(DALI_CMD_SEARCHADDRM, addr24 >> 8);
    dali_send(&tx);
    tx = dali_msg_new(DALI_CMD_SEARCHADDRL, addr24);
    dali_send(&tx);
}

// only device in INITIALISE state and not in WITHDRAW state
// return: 24-bit address
//         0xFFFFFF - not found
uint32_t dali_binary_search()
{
    Dali_msg_t tx, rx;
    uint32_t high = 0xFFFFFF;   // we must start from 0xFFFFFF but max address is 0xFFFFFE, it simplifies search
    uint32_t low  = 0x000000;
    uint32_t mid;
    int ret;

    for(int i=0; i<24; i++)
    {
        mid = (high + low) / 2;
        dali_set_search_addr24(mid);

        // compare
        tx = dali_msg_new(DALI_CMD_COMPARE, 0x00);
        ret = dali_query(&tx, &rx);
        if(ret) high = mid; // found address <= mid and high is possible address
        else    low = mid;

        // printf("CMP i=%2d rx=%d tm=%d[ms] st=%d  len=%d d0=0x%X mid=0x%06X high=0x%06X low=0x%06X\n",
        //     i, ret, rx.type, rx.status, rx.length, rx.data[0], mid, high, low);

        if(high - low < 2) {
            if(low == 0 && mid != 0) { // special case for 0
                mid = 0;
                continue;
            }
            break;
        }

        dali_delay_ms(10);
    }
    uint32_t addr24 = high; // found address is always in high
    printf("* Address: 0x%zX\n", (size_t)addr24);
    return addr24;
}

// discard address in dali search command COMPARE
// - requires INITIALISE state
void dali_cmd_withdraw(uint32_t addr24)
{
    Dali_msg_t tx;
    dali_set_search_addr24(addr24);

    tx = dali_msg_new(DALI_CMD_WITHDRAW, 0x00);
    dali_send(&tx);
    printf("Withdraw 0x%06zX\n", (size_t)addr24);
}

// initialise all devices selected by device:
//      device: 0AAA AAA1 - only device with this short address
//              1111 1111 - all devices without short address
//              0000 0000 - all devices
void dali_cmd_initialise(uint8_t device)
{
    Dali_msg_t tx;
    tx = dali_msg_new(DALI_CMD_INITIALISE, device); // 0x00=ALL 0xFF=without short address 0b0AAA AAA1=with short address
    dali_send_double(&tx);
}

void dali_cmd_terminate()
{
    Dali_msg_t tx;
    tx = dali_msg_new(DALI_CMD_TERMINATE, 0x00);
    dali_send(&tx);
}

// Reassign short addresses to all devices selected by device:
// - see dali_cmd_initialise(device)
void dali_reindex_addresses(uint8_t device)
{
    Dali_msg_t tx;
    uint32_t addr24 = 0;

    printf("Start searching\n");
    dali_cmd_initialise(device);           // 0x00=ALL 0xFF=without short address 0b0AAA AAA1=with short address

    for(uint8_t i=0; i<63; i++)
    {
        addr24 = dali_binary_search();
        if(addr24 == 0xFFFFFF) break;
        uint8_t device = dali_short_address(i);

        dali_set_search_addr24(addr24);
        tx = dali_msg_new(DALI_CMD_PROGRAM_SHORT, device);
        dali_send(&tx);

        printf("Assign address: 0x%06zX --> %d\n", (size_t)addr24, i);
        dali_cmd_withdraw(addr24);
    }

    dali_cmd_terminate();

    printf("End searching\n");

}

void dali_change_short_address(int addr1, int addr2)
{
    Dali_msg_t tx;
    uint8_t device1, device2;
    device1 = dali_short_address(addr1 & 0x3F);
    if(addr2 == -1) device2 = DALI_ADR_BROADCAST; // delete short address
    else if(addr2 < 0 || addr2 > 63) {
        printf("Invalid address: %d\n", addr2);
        return;
    }
    else {
        device2 = dali_short_address(addr2 & 0x3F);
    }

    int32_t addr24 = dali_cmd_query_rand_addr24(addr1);
    if(addr24 == -1) {
        printf("Cannot read address 24  for %d\n", addr1);
        return;
    }

    dali_cmd_initialise(device1);

    dali_set_search_addr24(addr24);
    tx = dali_msg_new(DALI_CMD_PROGRAM_SHORT, device2);
    dali_send(&tx);

    dali_cmd_terminate();

    printf("Address changed: %d --> %d\n", addr1, addr2);
}
