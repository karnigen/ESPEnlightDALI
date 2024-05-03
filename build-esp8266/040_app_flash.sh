#!/bin/bash
set -e

#PORT=/dev/ttyUSB0
#idf.py -p $PORT flash monitor
#idf.py flash monitor
#idf.py flash

make -j12 app-flash
make monitor



