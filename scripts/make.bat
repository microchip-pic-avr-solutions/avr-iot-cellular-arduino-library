@echo off

pushd %~dp0
set SCRIPTPATH=%CD%
popd

set TARGET="%SCRIPTPATH%\..\examples\mqtt_provision\mqtt_provision.ino"
set BUILD_DIR="%SCRIPTPATH%\..\build"
set BOARD_CONFIG="DxCore:megaavr:avrdb:appspm=no,chip=avr128db48,clock=24internal,bodvoltage=1v9,bodmode=disabled,eesave=enable,resetpin=reset,millis=tcb2,startuptime=8,wiremode=mors2,printf=full"

rmdir /s /q %BUILD_DIR%

echo Compiling...

arduino-cli compile %TARGET% -b %BOARD_CONFIG% --output-dir %BUILD_DIR% 

