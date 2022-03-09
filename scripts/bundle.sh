#!/bin/bash

mkdir -p release/avr-iot-cellular-library
cp src release/avr-iot-cellular-library -r
cp library.properties release/avr-iot-cellular-library
cd release

echo $1

if [ -z $1 ]; then
	zip -r ../avr-iot-cellular.zip . *
else
	zip -r ../avr-iot-cellular-$1.zip . *
fi