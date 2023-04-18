#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
CRYPTOAUTH_PATH=$SCRIPTPATH/../lib/cryptoauth
TEMPDIR_PATH=$CRYPTOAUTH_PATH/temp
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

echo "Injecting... "

# Fix the include paths so that all are relative to src since Arduino doesn't 
# allow us to add extra include paths for the compiler
python "$SCRIPTPATH/update_include_paths.py" "$CRYPTOAUTH_PATH/cryptoauthlib" "$TEMPDIR_PATH"

# Prevent removing items if the temporary directory was not created, just to be 
# safe and not removing files from the project
if [ ! -d "$TEMPDIR_PATH" ]; then
    echo "Temporary directory for cryptoauthlib not created, aborting"
    exit 1
else 
    # Remove everything we don't need, so we are only left with .h files
    pushd "$TEMPDIR_PATH/lib" 1> /dev/null 
        rm -rf mbedtls pkcs11 openssl wolfssl jwt cmake CMakeLists.txt

        pushd hal 1> /dev/null 
            find . ! -name "atca_hal.*" -type f -exec rm -f {} +
        popd 1> /dev/null
    popd 1> /dev/null
fi

# Make the destination folders
mkdir "$SRC_PATH/cryptoauthlib"
mkdir "$SRC_PATH/cryptoauthlib/app"

# Copy sources
cp -r "$TEMPDIR_PATH/lib" "$SRC_PATH/cryptoauthlib/"
cp -r "$TEMPDIR_PATH/app/tng"  "$SRC_PATH/cryptoauthlib/app/"
cp -r "$CRYPTOAUTH_PATH/atca_config.h" "$SRC_PATH/cryptoauthlib/"

rm -r "$TEMPDIR_PATH"

echo "Done!"
