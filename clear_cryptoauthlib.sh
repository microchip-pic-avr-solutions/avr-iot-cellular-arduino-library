#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
CRYPTOAUTH_PATH=$SCRIPTPATH/lib/cryptoauth
MCU=avr128db64

pushd $SCRIPTPATH/src > /dev/null

    rm -rf $MCU calib crypto hal host atcacert
    find . -name "atca_*" -exec rm -f {} +
    find . -name "cryptoauth*" -exec rm -f {} +

popd > /dev/null
