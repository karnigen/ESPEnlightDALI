
#include <stdio.h>                 // default baudrate is 74880
#include <memory.h>                // for memset
#include <string.h>

#include "dali_hw.h"
#include "dali.h"
#include "dali_define.h"
#include "uart_queue.h"
#include "uart_parser.h"

void parser_help()
{
    printf("Address: Short: 0-63[0x00-0x3F]  Group: 100-115[0x64-0x73]->[0x00-0x0F]: Broadcast: -1  As is: [0x1XX]->[0xXX]\n");
    printf("     eg: 5 = short address 5; 105 = group address 5; -1 = 0xFF broadcast; 0x105 = address as is 0x05\n");
    printf("Commands: ? dbg on off up dw sup sdw max min suo sdo cup cdw sm som spw sft sfr sxt reindex rand ter init d mv i ii id res\n");
}


// parse and create device address
// - short address 0-63 - 6 bits mask 0b00111111=0x3F
// - group address 100-115 - 4 bits mask 0b00001111=0x0F
// - broadcast -1
// - as is 0x100-0x1FF -> 0x00-0xFF
bool parse_device(int number, uint8_t *device)
{
    if     (    0 <= number && number <=    63) *device = dali_short_address(number & 0x3F); // 0b00111111=0x3F = mask short address
    else if(  100 <= number && number <=   115) *device = dali_group_address((number-100) & 0x0F); // 0b00001111=0x0F = mask group address
    else if(0x100 <= number && number <= 0x1FF) *device = number & 0xFF; // 0x100-0x1FF -> 0x00-0xFF
    else if(number == -1) *device = DALI_ADR_BROADCAST;
    else return false;
    return true;
}

void parse_set_level(int n, int number1, int number2, uint8_t cmd)
{
    uint8_t device1=0;
    if(n==1) { // missing address means use broadcast
        number2 = number1;
        number1 = -1;
    }
    if(parse_device(number1, &device1) == false || number2 < 0 || number2 > 255) {
        printf("-- err: set level\n");
        return;
    }
    dali_set_level(device1, cmd, number2);
    printf("Set Level num1: [0x%02X] device: [0x%02X] num2: [0x%02X]  cmd: [0x%02X]\n", number1, device1, number2, cmd);
}


void parse_scene(int n, int number1, int number2, uint8_t cmd, uint8_t double_cmd)
{
    uint8_t device1=0;
    if(n==1) { // missing address means use broadcast
        number2 = number1;
        number1 = -1;
    }
    if(parse_device(number1, &device1) == false || number2 < 0 || number2 > 15) {
        printf("-- err: set level\n");
        return;
    }
    cmd = cmd | (number2 & 0x0F);
    if(double_cmd) dali_cmd_double(device1, cmd);
    else           dali_cmd(device1, cmd);
    printf("Scene: [0x%02X] device: [0x%02X] num2: [0x%02X]  cmd: [0x%02X]\n", number1, device1, number2, cmd);
}


void parse_set_scene(int n, int number1, int number2, int number3, uint8_t cmd)
{
    uint8_t device1=0;
    if(n==2) { // missing address means use broadcast
        number3 = number2;
        number2 = number1;
        number1 = -1;
    }
    if(parse_device(number1, &device1) == false || number3 < 0 || number3 > 255 || number2 < 0 || number2 > 15) {
        printf("-- err: set level\n");
        return;
    }
    cmd |= (number2 & 0x0F);
    dali_set_level(device1, cmd, number3);
    printf("Set Scene num1: [0x%02X] device: [0x%02X] num2: [0x%02X]  cmd: [0x%02X]\n", number1, device1, number3, cmd);

}


void parse_group(int n, int number1, int number2, uint8_t cmd)
{
    uint8_t device1=0;
    if(n==1) { // missing address means use broadcast
        number2 = number1;
        number1 = -1;
    }
    if(parse_device(number1, &device1) == false || number2 < 0 || number2 > 15) {
        printf("-- err: group\n");
        return;
    }
    dali_group(device1, cmd, number2); // group add or remove
    printf("GROUP num1: [0x%02X] device: [0x%02X] num2: [0x%02X]  cmd: [0x%02X]\n", number1, device1, number2, cmd);
}


void parse_command(int n, int number1, uint8_t cmd) {
    uint8_t device1=0;
    if(n<=0) number1 = -1; // missing address means use broadcast, we use -1 as broadcast
    if(parse_device(number1, &device1) == true) {
        dali_cmd(device1, cmd);
    }
    else printf("-- err: command\n");

}

void uart_parser()
{

    parser_help();

    BaseType_t ret;
    Dali_msg_t tx_msg, rx_msg;

    char *string = (char *) malloc(UART_MSG_LEN+1);

    uint8_t dev_found[64];  // dev_found short addresses
    uint8_t found_done = 0; // flag lookup for devices was done

    memset(dev_found, 0, 64); // clear array

    int n;    // for sscanf count of arguments
    int number1, number2, number3; // for sscanf
    uint8_t device1;

    for(;;) {
        // receive message from UART
        n=0;
        if(xQueueReceive(uart_queue, string, portMAX_DELAY)== pdTRUE) // receive message from UART task
        {
            printf("-> %s\n", string);

            if (strcmp(string, "?") == 0) {
                parser_help();
            }
            else if (sscanf(string, "dbg %i", &number1) == 1) {
                if(0 <= number1 && number1 <= 2) {
                    rx_debug_enabled = number1;  // bits: 0-disable, 0x01:send, 0x02:pulses
                }
            }

            else if (strcmp(string, "find") == 0) { // find all devices
                dali_find_all_short(dev_found, 1);
                found_done = 1;
            }
            else if (strcmp(string, "ii") == 0) {  // info about all devices
                if(!found_done) {
                    dali_find_all_short(dev_found, 0);
                    found_done = 1;
                }
                for(int i=0; i<64; i++) {          // run before find
                    if(dev_found[i]) dali_device_info(i);
                }
                printf("Info done\n");
            }

            // COMMANDS ----------------------------------------------
            else if((n=sscanf(string, "on %i", &number1)) == 1 ||  strcmp(string, "on") == 0) {
                parse_command(n, number1, DALI_CMD_ON);
            }
            else if((n=sscanf(string, "off %i", &number1)) == 1 ||  strcmp(string, "off") == 0) {
                parse_command(n, number1, DALI_CMD_OFF);
            }
            else if((n=sscanf(string, "up %i", &number1)) == 1 ||  strcmp(string, "up") == 0) {
                parse_command(n, number1, DALI_CMD_UP);
            }
            else if((n=sscanf(string, "dw %i", &number1)) == 1 ||  strcmp(string, "dw") == 0) {
                parse_command(n, number1, DALI_CMD_DOWN);
            }

            else if((n=sscanf(string, "sup %i", &number1)) == 1 ||  strcmp(string, "sup") == 0) {
                parse_command(n, number1, DALI_CMD_STEP_UP);
            }
            else if((n=sscanf(string, "sdw %i", &number1)) == 1 ||  strcmp(string, "sdw") == 0) {
                parse_command(n, number1, DALI_CMD_STEP_DOWN);
            }

            else if((n=sscanf(string, "max %i", &number1)) == 1 ||  strcmp(string, "max") == 0) {
                parse_command(n, number1, DALI_CMD_RECALL_MAX);
            }
            else if((n=sscanf(string, "min %i", &number1)) == 1 ||  strcmp(string, "min") == 0) {
                parse_command(n, number1, DALI_CMD_RECALL_MIN);
            }

            else if((n=sscanf(string, "suo %i", &number1)) == 1 ||  strcmp(string, "suo") == 0) {
                parse_command(n, number1, DALI_CMD_ON_STEP_UP);
            }
            else if((n=sscanf(string, "sdo %i", &number1)) == 1 ||  strcmp(string, "sdo") == 0) {
                parse_command(n, number1, DALI_CMD_STEP_DOWN_OFF);
            }

            else if((n=sscanf(string, "cup %i", &number1)) == 1 ||  strcmp(string, "cup") == 0) {
                parse_command(n, number1, DALI_CMD_CONTIN_UP);
            }
            else if((n=sscanf(string, "cdw %i", &number1)) == 1 ||  strcmp(string, "cdw") == 0) {
                parse_command(n, number1, DALI_CMD_CONTIN_DOWN);
            }


            // INFO RESET -------------------------------------------------------------------------
            // i3  - info about device with short address 3
            else if (sscanf(string, "i %i", &number1) == 1) {
                dali_device_info(number1 & 0x3F); // 0b00111111=0x3F = mask short address
            }
            // id3 - identify device with short address 3
            else if (sscanf(string, "id %i", &number1) == 1) { // id3 identify device with short address 3
                device1 = dali_short_address(number1 & 0x3F); // 0b00111111=0x3F = mask short address
                dali_device_identify(device1);
                printf("Device %2d: identify\n", number1);
            }
            // res3 - reset device with short address 3
            else if (sscanf(string, "res %i", &number1) == 1) { // res3 reset device with short address 3
                if(parse_device(number1, &device1) == true) {
                    dali_device_reset(device1);
                    printf("Device %2d: reset\n", number1);
                }
                else printf("-- err: reset\n");
            }

            // LEVELS ----------------------------------------------
            // address or  group+100 or missing address = broadcast
            // smax 3 200 - set max level to 200 at short address 3
            // smax 200 - set max level to 200 at broadcast
            // smax 103 200 - set max level to 200 at group address 3
            else if ((n=sscanf(string, "smax %i %i", &number1, &number2)) >= 1) { // SET MAX LEVEL
                parse_set_level(n, number1, number2, DALI_CMD_SET_MAX_LEVEL);
            }
            else if ((n=sscanf(string, "smin %i %i", &number1, &number2)) >= 1) { // SET MIN LEVEL
                parse_set_level(n, number1, number2, DALI_CMD_SET_MIN_LEVEL);
            }
            else if ((n=sscanf(string, "ssys %i %i", &number1, &number2)) >= 1) { // SET SYSTEM FAILURE LEVEL
                parse_set_level(n, number1, number2, DALI_CMD_SET_SYS_FAIL_LEVEL);
            }
            else if ((n=sscanf(string, "spw %i %i", &number1, &number2)) >= 1) { // SET POWER ON LEVEL
                parse_set_level(n, number1, number2, DALI_CMD_SET_PW_ON_LEVEL);
            }
            else if ((n=sscanf(string, "sft %i %i", &number1, &number2)) >= 1) { // SET FADE TIME
                parse_set_level(n, number1, number2, DALI_CMD_SET_FADE_TIME);
            }
            else if ((n=sscanf(string, "sfr %i %i", &number1, &number2)) >= 1) { // SET FADE RATE
                parse_set_level(n, number1, number2, DALI_CMD_SET_FADE_RATE);
            }
            else if ((n=sscanf(string, "sxt %i %i", &number1, &number2)) >= 1) { // SET EXT FADE TIME
                parse_set_level(n, number1, number2, DALI_CMD_SET_EXT_FADE_TIME);
            }

            else if ((n=sscanf(string, "som %i %i", &number1, &number2)) >= 1) { // SET OPERATING MODE
                parse_set_level(n, number1, number2, DALI_CMD_SET_OPERATING_MODE);
            }

            // GROUPS: ----------------------------------------------
            // ga 3 5    - add short address 3 to group 5
            else if ((n=sscanf(string, "ga %i %i", &number1, &number2)) >= 1) {
                parse_group(n, number1, number2, DALI_CMD_ADD_TO_GROUP);
            }
            else if ((n=sscanf(string, "gr %i %i", &number1, &number2)) >= 1) {
                parse_group(n, number1, number2, DALI_CMD_REMOVE_FROM_GROUP);
            }

            // SCENES: ----------------------------------------------
            // ss 3 1 100   - on short address 3 set scene 1 to value 100
            // ss 1 100     - set scene 1 to value 100 at broadcast
            else if ((n=sscanf(string, "ss %i %i %i", &number1, &number2, &number3)) >= 2) { // SET SCENE
                parse_set_scene(n, number1, number2, number3, DALI_CMD_SET_SCENE);
            }
            // sg 3 1   - on short address 3 go to scene 1
            // sg 1     - go to scene 1 at broadcast
            else if ((n=sscanf(string, "sg %i %i", &number1, &number2)) >= 1) { // GO TO SCENE
                parse_scene(n, number1, number2, DALI_CMD_GO_TO_SCENE, 0);
            }
            // sr 3 1   - on short address 3 remove from scene 1
            // sr 1     - remove from scene 1 at broadcast
            else if ((n=sscanf(string, "sr %i %i", &number1, &number2)) >= 1) { // REMOVE FROM SCENE
                parse_scene(n, number1, number2, DALI_CMD_REMOVE_FROM_SCENE, 1);
            }


            else if (strcmp(string,"reindex")==0) { // re-assign short addresses from 0 to 63
                dali_reindex_addresses(0x00);  // 0x00=ALL 0xFF=without short address 0b0AAA AAA1=with short address
                found_done = 0; // clear found devices
            }
            else if (strcmp(string,"rand")==0) {  // randomize long addresses
                dali_cmd_randomize(0x00);
            }

            else if(strcmp(string, "ter") == 0) {      // terminate all DALI devices after INIT
                dali_cmd_terminate();
            }
            // initialises the control gear
            else if(strcmp(string, "init") == 0) {
                dali_cmd_initialise(0x00);  // 0x00=ALL 0xFF=without short address 0b0AAA AAA1=with short address
            }
            // D0 100 - set DTR0 to 100
            // D1 200 - set DTR1 to 200
            else if(sscanf(string, "d%i %i", &number1, &number2) == 2) { // set DTR0 DTR1 DTR2
                if(number2 >= 0 && number2 <= 255) {
                    switch(number1)
                    {
                        case 0: dali_set_dtr0(number2); break;
                        case 1: dali_set_dtr1(number2); break;
                        case 2: dali_set_dtr2(number2); break;
                        default: printf("Unknown DTR register\n");
                    }
                }
                else {
                    printf("DTR value out of range\n");
                }
            }
            // mv 3 5  - change short address from 3 to 5
            // mv 3 -1 - remove short address 3, device will not respond to short address
            else if(sscanf(string, "mv %i %i", &number1, &number2) == 2)
            {
                dali_change_short_address(number1, number2);
                found_done = 0; // clear found devices
            }

            // -------------------------------------------------------------------------

            // query - number1 is address and interpret by parse_device()
            // q 0x1A9 0x00 -> q 0xA9 0x00
            else if (sscanf(string, "q %i %i", &number1, &number2) == 2)
            {
                if(parse_device(number1, &device1) == false || number2 < 0 || number2 > 255) {
                    printf("-- err: query");
                }
                else {
                    printf("QUERY num1: %d [0x%02X] dev: [0x%02X] cmd: %d [0x%02X]\n", number1, number1, device1, number2, number2);
                    tx_msg = dali_msg_new(device1, number2);
                    if((ret=dali_query(&tx_msg, &rx_msg)) == pdTRUE)
                    {
                        printf("** rx=%d tm=%d[ms] st=%d  len=%d d0=%d [0x%X]\n",
                            ret, rx_msg.type, rx_msg.status, rx_msg.length, rx_msg.data[0], rx_msg.data[0]);
                    }
                    else
                    {
                        printf("No answer\n");
                    }
                }
            }
            // w 0x1A3 0x00 - write DTR0
            else if (sscanf(string, "w %i %i", &number1, &number2) == 2)
            {
                if(parse_device(number1, &device1) == false || number2 < 0 || number2 > 255) {
                    printf("-- err: write");
                }
                else {
                    printf("WRITE num1: %d [0x%02X] dev: [0x%02X] cmd: %d [0x%02X]\n", number1, number1, device1, number2, number2);
                    tx_msg = dali_msg_new(device1, number2);
                    dali_send(&tx_msg);
                }
            }
            // ww -1 0x2A set max level
            else if (sscanf(string, "ww %i %i", &number1, &number2) == 2)
            {
                if(parse_device(number1, &device1) == false || number2 < 0 || number2 > 255) {
                    printf("-- err: double write");
                }
                else {
                    printf("DOUBLE WRITE num1: %d [0x%02X] dev: [0x%02X] cmd: %d [0x%02X]\n", number1, number1, device1, number2, number2);
                    tx_msg = dali_msg_new(device1, number2);
                    dali_send_double(&tx_msg);
                }
            }
            else {
                printf("Unknown command\n");
                continue;
            }
        }
        else {
            printf("No message from UART\n");
        }
    }


}