#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
CRYPTOAUTH_PATH=$SCRIPTPATH/lib/cryptoauth
MCU=avr128db64

# Build cryptoauthlib
pushd $CRYPTOAUTH_PATH > /dev/null
    mkdir build
    pushd build > /dev/null
        cmake -Wno-dev ..
        make
    popd > /dev/null
popd > /dev/null

# Copy static library in designated folder
mkdir $SCRIPTPATH/src/$MCU
cp $CRYPTOAUTH_PATH/build/cryptoauthlib/lib/libcryptoauth.a $SCRIPTPATH/src/$MCU/

# Copy sources
cp -r $CRYPTOAUTH_PATH/cryptoauthlib/lib/ $SCRIPTPATH/src/cryptoauthlib/
cp -r $CRYPTOAUTH_PATH/atca_config.h $SCRIPTPATH/src/cryptoauthlib/

# Remove everything we don't need, so we are only left with .h files
pushd $SCRIPTPATH/src/cryptoauthlib > /dev/null

    find . ! -name "*.h" -type f -exec rm -f {} +

    rm -rf mbedtls pkcs11 openssl wolfssl jwt

    pushd hal > /dev/null
        find . ! -name "atca_hal.*" -type f -exec rm -f {} +
    popd > /dev/null

popd > /dev/null

# Move everything to src since arduino's include path is set to that and it's 
# messy changing it without the user having to do things with their Arduino 
# configuration
mv $SCRIPTPATH/src/cryptoauthlib/* $SCRIPTPATH/src/

rm -r $SCRIPTPATH/src/cryptoauthlib
