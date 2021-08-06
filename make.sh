#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

PORT=COM12
TARGET=src/examples/mqtt/mqtt.ino
EXTRA_ARGS=

if [ "$1" = "flash" ]; then
    EXTRA_ARGS=-u
elif [ "$1" = "clean" ]; then
    EXTRA_ARGS=--clean
fi

arduino-cli compile \
                $TARGET \
                $EXTRA_ARGS \
                -t \
                -p $PORT \
                -P nedbg \
                -b DxCore:megaavr:avrdb:appspm=no,chip=128DB64,clock=24internal,bodvoltage=1v9,bodmode=disabled,eesave=enable,resetpin=reset,millis=tcb2,startuptime=8 
