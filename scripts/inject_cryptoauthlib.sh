#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
CRYPTOAUTH_PATH=$SCRIPTPATH/../lib/cryptoauth
SRC_PATH=$SCRIPTPATH/../src

# Update cryptoauthlib
if [ -d "$CRYPTOAUTH_PATH/cryptoauthlib" ]; then
    echo "Cryptoauthlib already cloned, doing pull"

    pushd $CRYPTOAUTH_PATH/cryptoauthlib 1> /dev/null
        git pull 1> /dev/null
    popd 1> /dev/null
else
    git clone https://github.com/MicrochipTech/cryptoauthlib.git $CRYPTOAUTH_PATH/cryptoauthlib
fi

echo -n "Injecting... "

# Copy sources
cp -r "$CRYPTOAUTH_PATH/cryptoauthlib/lib/" "$SRC_PATH/cryptoauthlib/"
cp -r "$CRYPTOAUTH_PATH/atca_config.h" "$SRC_PATH/cryptoauthlib/"

# Remove everything we don't need, so we are only left with .h files
pushd "$SRC_PATH/cryptoauthlib" 1> /dev/null 
    rm -rf mbedtls pkcs11 openssl wolfssl jwt cmake CMakeLists.txt

    pushd hal 1> /dev/null 
        find . ! -name "atca_hal.*" -type f -exec rm -f {} +
    popd 1> /dev/null
popd 1> /dev/null

# Copy over certificate definitions
cp -r "$CRYPTOAUTH_PATH/cert_def"* "$SRC_PATH/cryptoauthlib/"

# Move everything to src since we can't add other include paths for 
# Arduino, has to be top level in src
mv "$SRC_PATH/cryptoauthlib/"* "$SRC_PATH" 1> /dev/null

rm -r "$SRC_PATH/cryptoauthlib"

echo "Done!"
