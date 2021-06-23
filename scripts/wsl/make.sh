#!/bin/bash

PORT=COM12
TARGET=avr-iot-cellular-arduino-firmware.ino

arduino-cli.exe compile $TARGET -t -p $PORT -P nedbg --clean -b DxCore:megaavr:avrdb:appspm=no,chip=128DB64,clock=24internal,bodvoltage=1v9,bodmode=disabled,eesave=enable,resetpin=reset,millis=tcb2,startuptime=8
