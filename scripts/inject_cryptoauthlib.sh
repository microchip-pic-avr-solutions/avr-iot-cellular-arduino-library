#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
CRYPTOAUTH_PATH=$SCRIPTPATH/../lib/cryptoauth
SRC_PATH=$SCRIPTPATH/../src
MCU=avr128db64

# Build cryptoauthlib
pushd "$CRYPTOAUTH_PATH" > /dev/null
    mkdir build
    pushd build
        cmake -Wno-dev ..
        make
    popd 
popd > /dev/null

# Copy static library in designated folder
mkdir "$SRC_PATH/$MCU"
cp "$CRYPTOAUTH_PATH/build/cryptoauthlib/lib/libcryptoauth.a" "$SRC_PATH/$MCU/"

# Copy sources
cp -r "$CRYPTOAUTH_PATH/cryptoauthlib/lib/" "$SRC_PATH/cryptoauthlib/"
cp -r "$CRYPTOAUTH_PATH/atca_config.h" "$SRC_PATH/cryptoauthlib/"

# Remove everything we don't need, so we are only left with .h files
pushd "$SRC_PATH/cryptoauthlib"
    find . ! -name "*.h" -type f -exec rm -f {} +

    rm -rf mbedtls pkcs11 openssl wolfssl jwt

    pushd hal
        find . ! -name "atca_hal.*" -type f -exec rm -f {} +
    popd
popd

# Copy over certificate definitions
cp -r "$CRYPTOAUTH_PATH/cert_def"* "$SRC_PATH/cryptoauthlib/"

# Move everything to src since arduino's include path is set to that and it's 
# messy changing it without the user having to do things with their Arduino 
# configuration
mv "$SRC_PATH/cryptoauthlib/"* "$SRC_PATH"

rm -r "$SRC_PATH/cryptoauthlib"
