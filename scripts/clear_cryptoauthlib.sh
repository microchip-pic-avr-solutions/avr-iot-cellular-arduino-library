#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
CRYPTOAUTH_PATH=$SCRIPTPATH/../lib/cryptoauth
SRC_PATH=$SCRIPTPATH/../src
MCU=avr128db64

pushd $SRC_PATH 
    rm -rf $MCU calib crypto hal host atcacert
    find . -name "atca_*" -exec rm -f {} +
    find . -name "cryptoauth*" -exec rm -f {} +
    find . -name "cert_def*" -exec rm -f {} +
popd 
