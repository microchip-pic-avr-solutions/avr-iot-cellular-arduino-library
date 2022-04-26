#!/bin/bash

# Test for AVR-IoT Cellular
for d in examples/*/ ; do
        echo "Compiling $d...";
        arduino-cli compile -b DxCore:megaavr:avrdb:appspm=no,chip=avr128db64,clock=24internal,bodvoltage=1v9,bodmode=disabled,eesave=enable,resetpin=reset,millis=tcb2,startuptime=8,wiremode=mors2 --libraries=".." "$d";
done

# Test for AVR-IoT Cellular Mini
for d in examples/*/ ; do
        echo "Compiling $d...";
        arduino-cli compile -b DxCore:megaavr:avrdb:appspm=no,chip=avr128db48,clock=24internal,bodvoltage=1v9,bodmode=disabled,eesave=enable,resetpin=reset,millis=tcb2,startuptime=8,wiremode=mors2 --libraries=".." "$d" --output-dir "builds/mini/$(basename $d)";
done