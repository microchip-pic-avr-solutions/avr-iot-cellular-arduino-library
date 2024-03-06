#!/bin/bash

BOARD_CONFIG="DxCore:megaavr:avrdb:appspm=no,chip=avr128db48,clock=24internal,bodvoltage=1v9,bodmode=disabled,eesave=enable,resetpin=reset,millis=tcb2,startuptime=8,wiremode=mors2,printf=full"

# Test for AVR-IoT Cellular Mini
for d in examples/*/ ; do
        echo "Compiling $d...";
        arduino-cli compile -b $BOARD_CONFIG --libraries=".." "$d" --build-path "builds/mini/$(basename $d)" --warnings all;

        # Check the error code
        if [ $? != 0 ]; then 
            exit 1
        fi
done
