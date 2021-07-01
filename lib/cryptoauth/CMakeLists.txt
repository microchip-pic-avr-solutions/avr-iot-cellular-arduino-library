cmake_minimum_required(VERSION 3.1.0)

# This is to prevent platform specific rules and includes for UNIX, Windows 
# and macOS to trigger in the CMakeLists for the cryptoauthlib which are not 
# needed and relevant for AVR
set(CMAKE_SYSTEM_NAME Generic)

# We don't want to compile a dynamic library
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Force CMake to use avr-gcc/g++ and their related tools
set(CMAKE_CXX_COMPILER /home/simengangstad/.arduino15/packages/DxCore/tools/avr-gcc/7.3.0-atmel3.6.1-azduino4b/bin/avr-g++)
set(CMAKE_C_COMPILER /home/simengangstad/.arduino15/packages/DxCore/tools/avr-gcc/7.3.0-atmel3.6.1-azduino4b/bin/avr-gcc)
set(CMAKE_ADDR2LINE /home/simengangstad/.arduino15/packages/DxCore/tools/avr-gcc/7.3.0-atmel3.6.1-azduino4b/bin/avr-addr2line)
set(CMAKE_AR /home/simengangstad/.arduino15/packages/DxCore/tools/avr-gcc/7.3.0-atmel3.6.1-azduino4b/bin/avr-ar)
set(CMAKE_LINKER /home/simengangstad/.arduino15/packages/DxCore/tools/avr-gcc/7.3.0-atmel3.6.1-azduino4b/bin/avr-ld)
set(CMAKE_NM /home/simengangstad/.arduino15/packages/DxCore/tools/avr-gcc/7.3.0-atmel3.6.1-azduino4b/bin/avr-nm)

set(CMAKE_CXX_FLAGS "-fdata-sections -Os -Wl,--relax,--gc-sections -mmcu=avr128db64")
set(CMAKE_C_FLAGS "-fdata-sections -Os -Wl,--relax,--gc-sections -mmcu=avr128db64")

project(cryptoauth)

# We have to add the subdirectory here to actually initiate the compilation
# of the rest of the chain of the library down in the hierarchy
add_subdirectory(cryptoauthlib)


file(COPY atca_config.h DESTINATION cryptoauthlib/lib)
