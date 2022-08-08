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

TIMEOUT = 10


def retrieve_examples(examples_directory):
    """Returns the absolute path to each example sketch file in the examples_directory.

    Args:
        examples_directory (str): Path to the examples directory
    """

    return glob.glob(examples_directory + "/**/*.ino", recursive=True)


def program(sketch_path, build_directory):
    """Builds and programs the sketch file

    Args:
        sketch_path (str): Path to the sketch 
        build_directory (str): Path to the build directory
    """

    print(f"Buillding and programming {os.path.basename(sketch_path)}...")

    if not os.path.exists(build_directory):
        os.mkdir(build_directory)

    # Use subprocess.check_output here and discard the output to not have any output from arduino-cli
    subprocess.check_output(f"arduino-cli compile {sketch_path} -b {BOARD_CONFIG} --output-dir {build_directory}")

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


def test(example, test_data, serial_handle):

    example_name = os.path.splitext(os.path.basename(example))[0]

    print(f"{example_name}: Testing...")

    for entry in test_data:

        expectation = entry.get("expectation", None)
        command = entry.get("command", None)
        repeat = entry.get("repeat", 1)

        for _ in range(0, repeat):
            if command != None:
                command_stripped = command.strip("\r")
                logging.info(f"\tTesting command: {command_stripped}")

                serial_handle.write(str.encode(command))
                serial_handle.flush()

            # Read until line feed
            output = serial_handle.read_until().decode("utf-8")

            response = re.search(expectation, output)

            if response == None:
                formatted_output = output.replace("\r", "\\r").replace("\n", "\\n")
                error = f"\tDid not get the expected response \"{expectation}\", got: \"{formatted_output}\""
                logging.error(error)
                logging.error(f"{example_name}: Failed")
                return (False, error)

            formatted_response = response.group(0).replace("\r", "\\r").replace("\n", "\\n")

            logging.info(f"\tGot valid response: {formatted_response}")

    print(f"{example_name}: Passed")

    return (True, None)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter,
                                     description=textwrap.dedent('''\
Automated tests for AVR-IoT Cellular Library
    '''),
                                     epilog=textwrap.dedent('''\
Usage:
    - python test.py PORT
'''))

    parser.add_argument("port",
                        type=str,
                        help="Device port (COMx, /dev/ttyACMx etc.)")

    parser.add_argument("-d",
                        "--device",
                        type=str,
                        help="Device to test against",
                        default="avr128db48")

    parser.add_argument("-v",
                        "--verbose",
                        type=str,
                        help="Logging verbosity level (critical, error, warning, info, debug)",
                        default="warn")

    parser.add_argument("-t",
                        "--testsfile",
                        type=str,
                        help="Path to JSON file with tests",
                        default="tests.json")

    parser.add_argument("-e",
                        "--examplesdir",
                        type=str,
                        help="Relative path to the example directory",
                        default="../examples")

    parser.add_argument("-b",
                        "--builddir",
                        type=str,
                        help="Relative path to the build directory",
                        default="build")

    arguments = parser.parse_args()

    logging.basicConfig(format="%(levelname)s: %(message)s",
                        level=getattr(logging, arguments.verbose.upper()))

    session_config = SessionConfig(arguments.device)

    transport = ToolUsbHidConnection()

    backend = Backend()

    try:
        backend.connect_to_tool(transport)
    except Exception as exception:
        logging.error(f"Failed to connect to tool: {exception}")
        exit(1)

    # Remove the build dir if it already exists
    if os.path.exists(arguments.builddir):
        for root, _, files in os.walk(arguments.builddir, topdown=False):
            for name in files:
                os.remove(os.path.join(root, name))

    examples = retrieve_examples(arguments.examplesdir)

    test_config = {}

    with open(arguments.testsfile) as file:
        test_config = json.load(file)

    examples_test_status = {}

    for example in examples:

        example_name = os.path.splitext(os.path.basename(example))[0]

        if not example_name in test_config:
            examples_test_status[example_name] = {"status": "No test defined", "error": None}
            continue

        if not test_config[example_name]["enabled"]:
            logging.warning(f"Skipping test for {example_name}, not enabled")
            examples_test_status[example_name] = {"status": "Test disabled", "error": None}
            continue

        backend.start_session(session_config)
        if not program(example, arguments.builddir):
            exit(1)

        try:
            # Start the serial session before before we close the pymcuprog session
            # so that the board is reset and running the programmed code
            with serial.Serial(arguments.port, 115200, timeout=TIMEOUT) as serial_handle:
                backend.end_session()
                (result, error) = test(example, test_config[example_name]["tests"], serial_handle)

                if not error:
                    examples_test_status[example_name] = {"status": "Passed", "error": None}
                else:
                    examples_test_status[example_name] = {"status": "Passed", "error": error}

        except serial.SerialException as exception:
            logging.error(f"Got exception while opening serial port: {exception}")
            exit(1)

        print("")

    backend.disconnect_from_tool()

    print("--------------- Test status ---------------")
    for example_name, entry in examples_test_status.items():
        status = entry["status"]
        error = entry["error"]

        if status == "Passed" or status == "No test defined" or status == "Test disabled":
            print(f"{example_name:<30}: {status}")
        else:
            print(f"{example_name:<30}: {status} - {error}")

    exit(0)
