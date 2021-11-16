#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

PORT=COM10
BUILD_DIR=$SCRIPTPATH/../build

# Do path conversion for WSL
if grep -q microsoft /proc/version; then
    BUILD_DIR=$(wslpath -wa $BUILD_DIR)
fi


/mnt/c/Users/m19936/go/bin/arduino-cli.exe upload \
                --input-dir $BUILD_DIR \
                -t \
                -p $PORT \
                -P nedbg \
                -b DxCore:megaavr:avrdb:appspm=no,chip=128DB64,clock=24internal,bodvoltage=1v9,bodmode=disabled,eesave=enable,resetpin=reset,millis=tcb2,startuptime=8 \
