
/home/simengangstad/.arduino15/packages/DxCore/tools/avr-gcc/7.3.0-atmel3.6.1-azduino4b/bin/avr-gcc \
    -w \
    -Os \
    -g \
    -flto \
    -fuse-linker-plugin \
    -Wl,--gc-sections,--section-start=.text=0x0,--section-start=.FLMAP_SECTION1=0x8000,--section-start=.FLMAP_SECTION2=0x10000,--section-start=.FLMAP_SECTION3=0x18000 \
    -mmcu=avr128db64 \
    -o \
    /tmp/arduino-sketch-4ABAB3DCE79E835FB2B0CE2612CAD28F/avr-iot-cellular-arduino-firmware.ino.elf \
    /tmp/arduino-sketch-4ABAB3DCE79E835FB2B0CE2612CAD28F/sketch/avr-iot-cellular-arduino-firmware.ino.cpp.o \
    /tmp/arduino-sketch-4ABAB3DCE79E835FB2B0CE2612CAD28F/sketch/src/ecc/ecc_controller.c.o \
    /tmp/arduino-sketch-4ABAB3DCE79E835FB2B0CE2612CAD28F/sketch/src/lte/http_client.c.o \
    /tmp/arduino-sketch-4ABAB3DCE79E835FB2B0CE2612CAD28F/sketch/src/lte/lte_client.c.o \
    /tmp/arduino-sketch-4ABAB3DCE79E835FB2B0CE2612CAD28F/sketch/src/lte/sequans_controller.c.o \
    /tmp/arduino-sketch-4ABAB3DCE79E835FB2B0CE2612CAD28F/../arduino-core-cache/core_75ec5c562969408e36ebeb15e0f8cecf.a \
    -L/tmp/arduino-sketch-4ABAB3DCE79E835FB2B0CE2612CAD28F \
    -lm \
    -L/home/simengangstad/develop/avr-iot-cellular-arduino-firmware/lib/cryptoauth/build/cryptoauthlib/lib \
    -lcryptoauth
