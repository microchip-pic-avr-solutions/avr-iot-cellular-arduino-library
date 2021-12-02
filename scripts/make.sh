#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

TARGET=$SCRIPTPATH/../src/examples/mqtt_polling/mqtt_polling.ino
EXTRA_ARGS=

if [ "$1" = "clean" ]; then
    EXTRA_ARGS=--clean
fi

rm -r $SCRIPTPATH/../build

arduino-cli compile \
                $TARGET \
                $EXTRA_ARGS \
                -t \
                -b DxCore:megaavr:avrdb:appspm=no,chip=avr128db64,clock=24internal,bodvoltage=1v9,bodmode=disabled,eesave=enable,resetpin=reset,millis=tcb2,startuptime=8 \
                --output-dir $SCRIPTPATH/../build