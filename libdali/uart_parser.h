#pragma once

#include <stdint.h>
#include <stdbool.h>

void parser_help();
bool parse_device(int number, uint8_t *device);
void parse_set_level(int n, int number1, int number2, uint8_t cmd);
void parse_command(int n, int number1, uint8_t cmd);
void uart_parser();

