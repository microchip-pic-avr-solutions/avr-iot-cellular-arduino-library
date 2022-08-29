import pytest
import logging
import argparse
import textwrap
import glob
import os
import json
import serial
import time
import subprocess
import re

from pymcuprog.backend import SessionConfig
from pymcuprog.toolconnection import ToolUsbHidConnection
from pymcuprog.backend import Backend
from pymcuprog.hexfileutils import read_memories_from_hex
from pymcuprog.deviceinfo.memorynames import MemoryNameAliases
from pymcuprog.deviceinfo.deviceinfokeys import DeviceMemoryInfoKeys



BOARD_CONFIG = "DxCore:megaavr:avrdb:appspm=no,chip=avr128db48,clock=24internal,"\
    "bodvoltage=1v9,bodmode=disabled,eesave=enable,resetpin=reset,"\
    "millis=tcb2,startuptime=8,wiremode=mors2,printf=full"

TIMEOUT = 30

def program(request):
    """Builds and programs the sketch file

    Args:
        request (obj): PyTest request object 
    """


    example_name = request.node.name[5:]

    build_directory = request.config.getoption("--builddir")
    sketch_directory = request.config.getoption("--sketchdir")

    # TODO: fix pathing
    sketch_path = f"{sketch_directory}/{example_name}/{example_name}.ino"

    print(f"Buillding and programming {os.path.basename(sketch_path)}...")

    if not os.path.exists(build_directory):
        os.mkdir(build_directory)

    compilation_return_code = subprocess.run([f"arduino-cli compile {sketch_path} -b {BOARD_CONFIG} --output-dir {build_directory}"], shell=True).returncode

    assert compilation_return_code == 0


    hex_file = build_directory + f"/{os.path.basename(sketch_path)}.hex"

    try:
        memory_segments = read_memories_from_hex(hex_file, backend.device_memory_info)
        backend.erase(MemoryNameAliases.ALL, address=None)

        for segment in memory_segments:
            memory_name = segment.memory_info[DeviceMemoryInfoKeys.NAME]
            backend.write_memory(segment.data, memory_name, segment.offset)
            verify_ok = backend.verify_memory(segment.data, memory_name, segment.offset)

            if verify_ok:
                logging.info("OK")
            else:
                logging.error("Verification failed!")
                return False

    except Exception as exception:
        logging.warning(f"Error programming: {exception}")
        return False

    return True

def test_debug_modem(request):
    # TODO: backend for pymcuprog has to be defined as a prerequisite and set up before all tests 


    test = program(request)

    assert test == "debug_modem"
