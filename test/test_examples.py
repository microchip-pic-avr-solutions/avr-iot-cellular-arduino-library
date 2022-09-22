import pytest
import argparse
import textwrap
import glob
import os
import json
import serial
import time
import subprocess
import re

from pathlib import Path

from pymcuprog.backend import SessionConfig
from pymcuprog.toolconnection import ToolUsbHidConnection
from pymcuprog.backend import Backend
from pymcuprog.hexfileutils import read_memories_from_hex
from pymcuprog.deviceinfo.memorynames import MemoryNameAliases
from pymcuprog.deviceinfo.deviceinfokeys import DeviceMemoryInfoKeys

# Configuration passed to arduino-cli when compiling the examples
BOARD_CONFIG = "DxCore:megaavr:avrdb:appspm=no,chip=avr128db48,clock=24internal,"\
    "bodvoltage=1v9,bodmode=disabled,eesave=enable,resetpin=reset,"\
    "millis=tcb2,startuptime=8,wiremode=mors2,printf=full"

SERIAL_TIMEOUT = 30


@pytest.fixture
def backend(request):
    """Sets up the Pymcuprog backend for connecting to the device. Will also 
       clear up any files in the build directory

    Args:
        request (obj): PyTest requst object, used for retriving the arguments
                       passed to pytest when running the script

    Returns:
        backend (obj): The pymcuprog backend
    """
    transport = ToolUsbHidConnection()

    backend = Backend()
    backend.connect_to_tool(transport)

    build_dir = Path(request.config.getoption("--builddir"))

    # Remove the contents of the build directory if it already exists
    if build_dir.exists():
        for root, _, files in os.walk(build_dir, topdown=False):
            for name in files:
                os.remove(os.path.join(root, name))

    return backend


@pytest.fixture
def session_config(request):
    """Returns the session configuration for the pymcuprog backend

    Args:
        request (obj): Used to retrieve the device (avr128db48 etc.) which is 
                       used in the tests

    Returns:
        session_config (obj): The session configuration for the device
    """
    return SessionConfig(request.config.getoption("--device"))


@pytest.fixture
def example_test_data():
    """Contains all the test data

    Returns:
        test_data(dict): Dictionary with the test for each example
    """
    return {
        "debug_modem": [
            {
                "command": "AT\r",
                "expectation": "AT"
            },
            {
                "expectation": "OK"
            },
            {
                "command": "AT+CEREG?\r",
                "expectation": "AT\\+CEREG\\?"
            },
            {
                "expectation": "\\+CEREG: 5,0"
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "OK"
            }
        ],
        "extract_certificates": [
            {
                "expectation": "\\[INFO\\] Initialized ECC"
            },
            {
                "repeat": 3,
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Printing signing certificate..."
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "-----BEGIN CERTIFICATE-----"
            },
            {
                "repeat": 11,
                "expectation": "([^\\n]+)"
            },
            {
                "expectation": "-----END CERTIFICATE-----"
            },
            {
                "repeat": 2,
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Printing device certificate..."
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "-----BEGIN CERTIFICATE-----"
            },
            {
                "repeat": 11,
                "expectation": "([^\\n]+)"
            },
            {
                "expectation": "-----END CERTIFICATE-----"
            }
        ],
        "http": [
            {
                "expectation": "\\[INFO\\] Starting HTTP example"
            },
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Connected to operator: (.*)"
            },
            {
                "expectation": "\\[INFO\\] ---- Testing HTTP ----"
            },
            {
                "expectation": "\\[INFO\\] Configured to HTTP"
            },
            {
                "expectation": "\\[INFO\\] GET - status code: 200, data size: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] POST - status code: 200, data size: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Body: {"
            }
        ],
        "http_get_time": [
            {
                "expectation": "\\[INFO\\] Starting HTTP Get Time Example"
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Connected to operator: (.*)"
            },
            {
                "expectation": "\\[INFO\\] --- Configured to HTTP ---"
            },
            {
                "expectation": "\\[INFO\\] Successfully performed GET request. Status Code = 200, Size = (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Got the time \\(unixtime\\) (\\d{10})"
            }
        ],
        "https": [
            {
                "expectation": "\\[INFO\\] Starting HTTPS example"
            },
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Connected to operator: (.*)"
            },
            {
                "expectation": "\\[INFO\\] ---- Testing HTTPS ----"
            },
            {
                "expectation": "\\[INFO\\] Configured to HTTPS"
            },
            {
                "expectation": "\\[INFO\\] GET - status code: 200, data size: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] POST - status code: 200, data size: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Body: {"
            }
        ],
        "https_configure_ca": [
            {
                "expectation": "\\[INFO\\] Setting up security profile for HTTPS..."
            },
            {
                "expectation": "\\[INFO\\] Done!"
            }
        ],
        "mqtt_password_authentication": [
            {
                "expectation": "\\[INFO\\] Starting initialization of MQTT with username and password"
            },
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Connecting to broker.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Subscribed to mchp_topic"
            },
            {
                "expectation": "\\[INFO\\] Published message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Got new message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Published message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Got new message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Published message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Got new message: Hello world: (\\d{1,})"
            }
        ],
        "mqtt_polling": [
            {
                "expectation": "\\[INFO\\] Starting initialization of MQTT Polling"
            },
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Connecting to broker.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Published message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Got new message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Published message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Got new message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Published message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Got new message: Hello world: (\\d{1,})"
            }
        ],
        "mqtt_polling_aws": [
            {
                "expectation": "\\[INFO\\] Starting initialization of MQTT Polling for AWS"
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Connecting to broker.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Published message"
            },
            {
                "expectation": "\\[INFO\\] Got new message: \\{\\\"light\\\": 9, \\\"temp\\\": 9\\}"
            },
            {
                "expectation": "\\[INFO\\] Published message"
            },
            {
                "expectation": "\\[INFO\\] Got new message: \\{\\\"light\\\": 9, \\\"temp\\\": 9\\}"
            },
            {
                "expectation": "\\[INFO\\] Published message"
            },
            {
                "expectation": "\\[INFO\\] Got new message: \\{\\\"light\\\": 9, \\\"temp\\\": 9\\}"
            }
        ],
        "power_down": [
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Connected to operator: (.*)"
            },
            {
                "expectation": "\\[INFO\\] Powering down..."
            },
            {
                "timeout": 90,
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Woke up!"
            },
            {
                "expectation": "\\[INFO\\] Doing work..."
            }
        ],
        "power_print_voltage": [
            {
                "expectation": "\\[INFO\\] Starting up example for printing voltage supplied to the board"
            },
            {
                "repeat": 5,
                "expectation": "\\[INFO\\] The voltage supplied is: [0-9]\\.[0-9]+"
            }
        ],
        "power_save": [
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Connected to operator: (.*)"
            },
            {
                "expectation": "\\[INFO\\] Power saving..."
            },
            {
                "timeout": 300,
                "expectation": "\\[INFO\\] Woke up!"
            },
            {
                "expectation": "\\[INFO\\] Doing work..."
            }
        ],
        "sandbox": [
            {
                "expectation": "\\[INFO\\] Starting sandbox / landing page procedure. Version = [0-9]+\\.[0-9]+\\.[0-9]+"
            },
            {
                "expectation": "\\[INFO\\] Board name: [a-z0-9]+"
            },
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Connected to operator: (.*)"
            },
            {
                "expectation": "\\[INFO\\] Connecting to MQTT broker..."
            },
            {
                "expectation": "\\[INFO\\] Connected to MQTT broker, subscribing to topic: \\$aws/things/[a-z0-9]+/shadow/update/delta!"
            },
            {
                "repeat": 2,
                "expectation": "\\[INFO\\] Sending heartbeat"
            }
        ],
    }


def program(request, backend):
    """Builds and programs the sketch file

    Args:
        request (obj): PyTest request object
        backend (obj): pymcuprog backend
    """

    example_name = request.node.name[5:]

    build_directory = request.config.getoption("--builddir")
    sketch_directory = request.config.getoption("--sketchdir")

    sketch_path = Path(f"{sketch_directory}/{example_name}/{example_name}.ino")

    if not build_directory.exists():
        os.mkdir(build_directory)

    compilation_return_code = subprocess.run(
        ["arduino-cli", "compile", f"{sketch_path}", "-b", f"{BOARD_CONFIG}", "--output-dir", f"{build_directory}"], shell=True).returncode

    assert compilation_return_code == 0, f"{example_name} failed to compile"

    hex_file = build_directory / f"{os.path.basename(sketch_path)}.hex"

    memory_segments = read_memories_from_hex(hex_file, backend.device_memory_info)
    backend.erase(MemoryNameAliases.ALL, address=None)

    for segment in memory_segments:
        memory_name = segment.memory_info[DeviceMemoryInfoKeys.NAME]
        backend.write_memory(segment.data, memory_name, segment.offset)
        verify_ok = backend.verify_memory(segment.data, memory_name, segment.offset)

        assert verify_ok, "Verification of program memory failed"


def run_example(request, backend, example_test_data):
    """Runs the example sketch and test against the test data

    Args:
        request (obj): PyTest request object 
        backend (obj): Pymcuprog backend object 
        example_test_data (dict): Dictionary with all test data 
    """

    example_name = request.node.name[5:]
    test_data = example_test_data[example_name]

    # Start the serial session before before we close the pymcuprog session
    # so that the board is reset and running the programmed code
    with serial.Serial(request.config.getoption("--port"), 115200, timeout=SERIAL_TIMEOUT) as serial_handle:

        backend.end_session()

        example_name = request.node.name[5:]
        print(f"{example_name}: Testing...")

        for entry in test_data:

            expectation = entry.get("expectation", None)
            command = entry.get("command", None)
            repeat = entry.get("repeat", 1)
            timeout = entry.get("timeout", SERIAL_TIMEOUT)

            assert expectation != None, "Missing expectation in test data"

            for i in range(0, repeat):
                if command != None:
                    command_stripped = command.strip("\r")
                    print(f"\tTesting command: {command_stripped}")

                    serial_handle.write(str.encode(command))
                    serial_handle.flush()

                # Read until line feed or timeout
                start = time.time()
                output = ""

                while time.time() - start < timeout and output == "":
                    output = serial_handle.read_until().decode("utf-8")

                response = re.search(expectation, output)

                formatted_output = output.replace("\r", "\\r").replace("\n", "\\n")
                assert response != None, f"\tDid not get the expected response \"{expectation}\", got: \"{formatted_output}\""

                formatted_response = response.group(0).replace("\r", "\\r").replace("\n", "\\n")

                print(f"\tGot valid response: {formatted_response}")


def run_test(request, backend, session_config, example_test_data):
    backend.start_session(session_config)
    program(request, backend)
    run_example(request, backend, example_test_data)


# ------------------------------- TESTS -----------------------------------

def test_debug_modem(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_extract_certificates(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_http(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_http_get_time(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_https(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_https_configure_ca(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_mqtt_password_authentication(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_mqtt_polling(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_mqtt_polling_aws(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_power_down(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_power_print_voltage(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_power_save(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_sandbox(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)
