#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

TARGET=src/examples/mqtt_interrupt/mqtt_interrupt.ino
EXTRA_ARGS=

if [ "$1" = "clean" ]; then
    EXTRA_ARGS=--clean
fi

arduino-cli compile \
                $TARGET \
                $EXTRA_ARGS \
                -t \
                -b DxCore:megaavr:avrdb:appspm=no,chip=128DB64,clock=24internal,bodvoltage=1v9,bodmode=disabled,eesave=enable,resetpin=reset,millis=tcb2,startuptime=8 \
                --output-dir build
