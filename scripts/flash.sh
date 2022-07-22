#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
PORT=COM4
BOARD_CONFIG="DxCore:megaavr:avrdb:appspm=no,chip=avr128db48,clock=24internal,bodvoltage=1v9,bodmode=disabled,eesave=enable,resetpin=reset,millis=tcb2,startuptime=8,wiremode=mors2"
BUILD_DIR=$SCRIPTPATH/../build

echo "Flashing..."

# Do path conversion for WSL
if grep -q microsoft /proc/version; then
    BUILD_DIR=$(wslpath -wa $BUILD_DIR)
    arduino-cli.exe upload --input-dir $BUILD_DIR -t -p $PORT -P nedbg -b $BOARD_CONFIG
else
    arduino-cli upload --input-dir $BUILD_DIR -t -p $PORT -P nedbg -b $BOARD_CONFIG
fi

