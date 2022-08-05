#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
TARGET=$(find $SCRIPTPATH/../build -type f -name "*.hex")

echo "Flashing..."

# Do path conversion for WSL
if grep -q microsoft /proc/version; then
    TARGET=$(wslpath -wa $TARGET)
    pymcuprog.exe write --erase -d avr128db48 -t nEDBG -f $TARGET
else
    pymcuprog write --erase -d avr128db48 -t nEDBG -f $TARGET
fi

