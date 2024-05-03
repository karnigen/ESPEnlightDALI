#pragma once

#include <stdint.h>
#include "dali_hw.h"

/*
  Addressing: 102.7.2.1
    0AAA AAAx - short address           AAAAAA 0-63
    100G GGGx - group address           GGG    0-15
    1111 1101 - broadcast unaddressed
    1111 1111 - broadcast
            x - 0 - direct arc power control (DAPC), 1 - standard command
*/


// create generic DALI message - for any bit length
Dali_msg_t dali_msg_new_generic(uint8_t bit_length, uint8_t address, uint8_t cmd1, uint8_t cmd2, uint8_t cmd3);

// create standard DALI message: 16,24,32 bits
Dali_msg_t dali_msg_new(uint8_t address, uint8_t cmd1);
Dali_msg_t dali_msg_new_3B(uint8_t address, uint8_t cmd1, uint8_t cmd2);
Dali_msg_t dali_msg_new_4B(uint8_t address, uint8_t cmd1, uint8_t cmd2, uint8_t cmd3);


// short address 0-63 - 6 bits mask 0b00111111=0x3F
//  - create device address from short address
//  - device address: 0b 0AAA AAA1
inline uint8_t dali_short_address(uint8_t address) {
    return ((address & 0x3F) << 1) | 0x01;
}

// group address 0-15 - 4 bits mask 0b00001111=0x0F
//  - create group address from short address
//  - group address: 0b100G GGG1
inline uint8_t dali_group_address(uint8_t grp) {
    return ((grp & 0x0F) << 1) | 0x81;
}

void dali_send(Dali_msg_t *tx_msg);
void dali_send_double(Dali_msg_t *dali_msg);
int dali_query(Dali_msg_t *tx_msg, Dali_msg_t *rx_msg);

// DALI queries
uint32_t dali_cmd_query_rand_addr24(uint8_t addr);


void dali_find_all_short(uint8_t *dev, int with_print);
void dali_device_info(uint8_t addr);
void dali_device_identify(uint8_t device);
void dali_device_reset(uint8_t device);

// DALI commands
void dali_cmd(uint8_t device, uint8_t cmd);
void dali_cmd_double(uint8_t device, uint8_t cmd);

void dali_cmd_off(uint8_t device);
void dali_cmd_on(uint8_t device);

void dali_set_level(uint8_t device, uint8_t cmd, uint8_t level);
void dali_group(uint8_t device1, uint8_t cmd, uint8_t group);


void dali_set_dtr0(uint8_t value);
void dali_set_dtr1(uint8_t value);
void dali_set_dtr2(uint8_t value);


void dali_cmd_initialise(uint8_t device);
void dali_cmd_terminate();
void dali_cmd_randomize(uint8_t device);
void dali_cmd_withdraw(uint32_t addr24);

// DALI special commands
void dali_reindex_addresses(uint8_t device);

void dali_set_search_addr24(uint32_t addr24);
uint32_t dali_binary_search();
void dali_change_short_address(int addr1, int addr2);



