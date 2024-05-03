#!/bin/bash

# Ctrl+] to exit monitor mode

#PORT=/dev/ttyUSB0
#idf.py -p $PORT monitor
#idf.py monitor

# speed: 9600 57600 74880 (default)   115200 230400 921600

# make monitor MONITORBAUD=115200 ESPPORT=/dev/ttyUSB0
#make monitor MONITORBAUD=115200
#make monitor MONITORBAUD=74880
#make monitor MONITORBAUD=9600

make monitor
