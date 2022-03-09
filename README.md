# AVR-IoT Cellular Arduino Library
![GitHub release (latest by date)](https://img.shields.io/github/v/release/mchpTestArea/avr-iot-test?style=flat-square)
<center><img width="50%" style={{paddingTop: "10px", paddingBottom: "10px"}} src="./readme_images/mini-board-2.png" /></center>

The AVR-IoT Cellular Mini is a development board from Microchip to develop cellular IoT applications. 

📓 Full Arduino support through a library built on top of the open-source [DxCore](https://github.com/SpenceKonde/DxCore)

🔒 All the basic building blocks to create secure IoT applications (AVR Microcontroller, Secure Element and a Cellular Modem)

⚡ Free 150MB 90-Day SIM Card from [Truphone](https://truphone.com), providing coverage across many countries

📡 Bundled Antenna in the Box

🐞 On-Board Debugger, no need for any external tools

🔋  Battery Charging Circuitry with Connectors

🤝 Built & Designed to be Familiar to Makers, featuring a [Adafruit Feather](https://learn.adafruit.com/adafruit-feather) form-factor and a [Qwiic](https://www.sparkfun.com/qwiic) / [Stemma](https://learn.adafruit.com/introducing-adafruit-stemma-qt) Connector

<center>

<span style="font-size:2em;">👉 <u>Documentation: https://iot.microchip.com/docs/</u> 👈</span>

</center>

## Build Instructions

**A pre-built library can be downloaded from the releases tab. This section explains how to build the library locally for Linux. The process for Windows is similar. Most users do not need to build from source.**

The library depends on [MicrochipTech/cryptoauthlib](https://github.com/MicrochipTech/cryptoauthlib) to communicate with the ECC608B (Secure Element). It is added to this repository as a submodule. A CMake build configuration exists in [lib/cryptoauth](./lib/cryptoauth/), which builds `libcryptoauth.a`. Said file must be added to [./src/avr128db48](./src/avr128db48). The cryptoauth header files must be added to [./src](./src).

A script, [inject_cryptoauthlib.sh](./scripts/inject_cryptoauthlib.sh), builds the library and inserts the .a file together with the header files into the correct location.

### TLDR; How to build on

1. Download and install all dependencies
	- CMake
		* `apt install cmake`
	- AVR Toolchain
		* `apt install gcc-avr avr-libc`
2. Clone this repository with the `--recursive` flag
3. Run [./scripts/inject_cryptoauthlib.sh](./scripts/inject_cryptoauthlib.sh)
