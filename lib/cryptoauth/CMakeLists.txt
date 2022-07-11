cmake_minimum_required(VERSION 3.1.0)

# This is to prevent platform specific rules and includes for UNIX, Windows 
# and macOS to trigger in the CMakeLists for the cryptoauthlib which are not 
# needed and relevant for AVR
set(CMAKE_SYSTEM_NAME Generic)

# We don't want to compile a dynamic library
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

# Force CMake to use avr-gcc/g++ and their related tools
set(CMAKE_CXX_COMPILER /usr/bin/avr-g++)
set(CMAKE_C_COMPILER /usr/bin/avr-gcc)
set(CMAKE_ADDR2LINE /usr/bin/avr-addr2line)
set(CMAKE_AR /usr/bin/avr-ar)
set(CMAKE_LINKER /usr/bin/avr-ld)
set(CMAKE_NM /usr/bin/avr-nm)

set(CMAKE_CXX_FLAGS "-fdata-sections -B ${CMAKE_CURRENT_SOURCE_DIR} -Os -Wl,--relax,--gc-sections -mmcu=${MCU} -DF_CPU=24000000")
set(CMAKE_C_FLAGS "-fdata-sections -B ${CMAKE_CURRENT_SOURCE_DIR} -Os -Wl,--relax,--gc-sections -mmcu=${MCU} -DF_CPU=24000000")

project(cryptoauth)

# We have to add the subdirectory here to actually initiate the compilation
# of the rest of the chain of the library down in the hierarchy
add_subdirectory(cryptoauthlib)


file(COPY atca_config.h DESTINATION cryptoauthlib/lib)
