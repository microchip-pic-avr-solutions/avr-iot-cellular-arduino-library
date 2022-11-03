@echo off

pushd %~dp0
set SCRIPTPATH=%CD%
popd

for /r "%SCRIPTPATH%\..\build" %%a in (*.hex) do set TARGET=%%~dpnxa

pymcuprog write --erase -d avr128db48 -t nEDBG -f %TARGET%