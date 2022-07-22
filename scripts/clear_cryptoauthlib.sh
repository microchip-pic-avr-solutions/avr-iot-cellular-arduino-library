#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
CRYPTOAUTH_PATH=$SCRIPTPATH/../lib/cryptoauth
SRC_PATH=$SCRIPTPATH/../src

pushd "$SRC_PATH" 1> /dev/null
    rm -rf calib crypto hal host atcacert
    find . -name "atca_*" -exec rm -f {} +
    find . -name "cryptoauth*" -exec rm -f {} +
    find . -name "cert_def*" -exec rm -f {} +
popd  1> /dev/null
