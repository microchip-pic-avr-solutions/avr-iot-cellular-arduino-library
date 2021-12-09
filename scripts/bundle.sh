#!/bin/bash

mkdir release
cp src release/src -r
cp library.properties release
cd release

echo $1

if [ -z $1 ]; then
	zip -r ../avr-iot-cellular.zip . *
else
	zip -r ../avr-iot-cellular-$1.zip . *
fi