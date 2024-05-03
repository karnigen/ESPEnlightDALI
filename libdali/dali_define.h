#pragma once

#define DALI_ADR_BROADCAST       0b11111111

#define DALI_CMD_OFF             0x00          // 102.9.7.2 11.3.2: tagetLevel=0 & OFF
#define DALI_CMD_UP              0x01          // 102.11.3.3: for 200 ms up: tagetLevel calculated from actualLevel & fadeRate
#define DALI_CMD_DOWN            0x02          // 102.11.3.4: for 200 ms down:
#define DALI_CMD_STEP_UP         0x03          // 102.11.3.5: immediate one step up
#define DALI_CMD_STEP_DOWN       0x04          // 102.11.3.6: immediate one step down
#define DALI_CMD_RECALL_MAX      0x05          // 102.11.3.7: tagetLevel=actualLevel=maxLevel
                                               //             INIT: moreover & output 100%, see RECALL MIN to identify light
#define DALI_CMD_ON              DALI_CMD_RECALL_MAX
#define DALI_CMD_RECALL_MIN      0x06          // 102.11.3.7: tagetLevel=actualLevel=minLevel
                                               //             INIT: moreover & output 0% or OFF, see RECALL MAX to identify light
#define DALI_CMD_STEP_DOWN_OFF   0x07          //
#define DALI_CMD_ON_STEP_UP      0x08          //
#define DALI_CMD_DAPC_SEQ        0x09          //
#define DALI_CMD_LAST_ACTIVE     0x0A          //
#define DALI_CMD_CONTIN_UP       0x0B          //
#define DALI_CMD_CONTIN_DOWN     0x0C          //

#define DALI_CMD_GOTO_SCENE      0x10          // + scene number 0-15

#define DALI_CMD_RESET           0x20          // 102.9.11.1 11.4.2
#define DALI_CMD_ACTUAL_IN_DTR0  0x21          //

#define DALI_CMD_SET_OPERATING_MODE   0x23          //
#define DALI_CMD_IDENTIFY             0x25          // 102.9.14.3 11.4.6

// SCENE commands
#define DALI_CMD_GO_TO_SCENE          0x10          //
#define DALI_CMD_SET_SCENE            0x40          //
#define DALI_CMD_REMOVE_FROM_SCENE    0x50          //

// GROUP commands
#define DALI_CMD_ADD_TO_GROUP         0x60          //
#define DALI_CMD_REMOVE_FROM_GROUP    0x70          //

// SET commands
#define DALI_CMD_SET_MAX_LEVEL        0x2A          // 102.9.6 11.4.7
#define DALI_CMD_SET_MIN_LEVEL        0x2B          // 102.9.6 11.4.8
#define DALI_CMD_SET_SYS_FAIL_LEVEL   0x2C          // 102.9.12 11.4.9
#define DALI_CMD_SET_PW_ON_LEVEL      0x2D          // 102.9.13 11.4.10
#define DALI_CMD_SET_FADE_TIME        0x2E          // 102.9.5.2 11.4.11
#define DALI_CMD_SET_FADE_RATE        0x2F          // 102.9.5.3 11.4.12
#define DALI_CMD_SET_EXT_FADE_TIME    0x30          // 102.9.5.4 11.4.13


// QUERY commands
#define DALI_CMD_QUERY_STATUS                0x90     // 102.9.16 102.11.5.2
#define DALI_CMD_QUERY_DTR0                  0x98     // 102.11.5.11
#define DALI_CMD_QUERY_DTR1                  0x9C     // 102.11.5.16
#define DALI_CMD_QUERY_DTR2                  0x9D     // 102.11.5.17
#define DALI_CMD_QUERY_OPERATING_MODE        0x9E     // 102.9.9 102.11.5.18

#define DALI_CMD_QUERY_PHYSICAL_MIN          0x9A     // 102.11.5.14
#define DALI_CMD_QUERY_ACTUAL_LEVEL          0xA0     // 102.11.5.20
#define DALI_CMD_QUERY_MAX_LEVEL             0xA1     // 102.11.5.21
#define DALI_CMD_QUERY_MIN_LEVEL             0xA2     // 102.11.5.22
#define DALI_CMD_QUERY_POWER_ON_LEVEL        0xA3     // 102.11.5.23
#define DALI_CMD_QUERY_SYSTEM_FAILURE_LEVEL  0xA4     // 102.11.5.24
#define DALI_CMD_QUERY_FADE_TIME             0xA5     // 102.11.5.25
#define DALI_CMD_QUERY_EXT_FADE_TIME         0xA8     // 102.11.5.65

#define DALI_CMD_QUERY_SCENE_LEVEL           0xB0     //
#define DALI_CMD_QUERY_GROUPS_0_7            0xC0     //
#define DALI_CMD_QUERY_GROUPS_8_15           0xC1     //
#define DALI_CMD_QUERY_RAND_ADDR_H           0xC2     // 102.11.4.31
#define DALI_CMD_QUERY_RAND_ADDR_M           0xC3     // 102.11.4.32
#define DALI_CMD_QUERY_RAND_ADDR_L           0xC4     // 102.11.4.33


// Special commands
#define DALI_CMD_TERMINATE       0xA1          // Releases the INITIALISE state.
#define DALI_CMD_DTR0            0xA3          // Data Transfer Register 0
#define DALI_CMD_INITIALISE      0xA5          // 2x Initialise 0x00=ALL 0xFF=without short address 0b0AAA AAA1=with short address
#define DALI_CMD_RANDOMISE       0xA7          // 2x Randomises the short address of the control gear.
#define DALI_CMD_COMPARE         0xA9          // Compares the stored short address with the received short address.
#define DALI_CMD_WITHDRAW        0xAB          // Withdraws the control gear from the system.
#define DALI_CMD_PING            0xAD          //
#define DALI_CMD_SEARCHADDRH     0xB1          // Search Address H
#define DALI_CMD_SEARCHADDRM     0xB3          // Search Address M
#define DALI_CMD_SEARCHADDRL     0xB5          // Search Address L
#define DALI_CMD_PROGRAM_SHORT   0xB7          // Program Short Address
#define DALI_CMD_VERIFY_SHORT    0xB9          // Verify Short Address
#define DALI_CMD_QUERY_SHORT     0xBB          // Query Short Address
#define DALI_CMD_ENABLE_DEVICE   0xC1          // Enable Device Type
#define DALI_CMD_DTR1            0xC3          // Data Transfer Register 1
#define DALI_CMD_DTR2            0xC5          // Data Transfer Register 2
#define DALI_CMD_WRITE_MEM_LOC   0xC7          // Write Memory Location
#define DALI_CMD_WRITE_MEM_NR    0xC9          // Write Memory no replay
