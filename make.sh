#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

PORT=COM12
TARGET=avr-iot-cellular-arduino-firmware.ino
EXTRA_ARGS=
CRYPTOAUTH_PATH=lib/cryptoauth
CRYPTOAUTH_LIB_DIRPATH=$SCRIPTPATH/$CRYPTOAUTH_PATH/build/cryptoauthlib/lib
CRYPTOAUTH_SRC_DIRPATH=$SCRIPTPATH/$CRYPTOAUTH_PATH/cryptoauthlib/lib/
CRYPTOAUTH_LIB_PATH=$CRYPTOAUTH_LIB_DIRPATH/libcryptoauth.a

# Do path conversion for WSL
if grep -q microsoft /proc/version; then
    CRYPTOAUTH_LIB_DIRPATH=$(wslpath -wa $CRYPTOAUTH_LIB_DIRPATH)
    CRYPTOAUTH_LIB_PATH=$(wslpath -wa $CRYPTOAUTH_LIB_PATH)
    CRYPTOAUTH_SRC_DIRPATH=$(wslpath -wa $CRYPTOAUTH_SRC_DIRPATH)
fi

pushd $CRYPTOAUTH_PATH
    mkdir build
    pushd build
        cmake -Wno-dev ..
        make
    popd
popd

if [ "$1" = "flash" ]; then
    EXTRA_ARGS=-u
elif [ "$1" = "clean" ]; then
    EXTRA_ARGS=--clean
fi

arduino-cli.exe compile \
                $TARGET \
                $EXTRA_ARGS \
                -t \
                -p $PORT \
                -P nedbg \
                -b DxCore:megaavr:avrdb:appspm=no,chip=128DB64,clock=24internal,bodvoltage=1v9,bodmode=disabled,eesave=enable,resetpin=reset,millis=tcb2,startuptime=8 \
                --build-property "compiler.c.extra_flags=-I$CRYPTOAUTH_SRC_DIRPATH -I$CRYPTOAUTH_LIB_DIRPATH" \
                --build-property "compiler.cpp.extra_flags=-I$CRYPTOAUTH_SRC_DIRPATH -I$CRYPTOAUTH_LIB_DIRPATH" \
                --build-property "combine.extra_paths=$CRYPTOAUTH_LIB_PATH"
