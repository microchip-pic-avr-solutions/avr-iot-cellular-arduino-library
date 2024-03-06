import pytest
import os
import serial
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
        "custom_at_commands": [
            {
                "expectation": "\\[INFO\\] Starting up example for custom AT commands"
            },
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[ERROR\\] Error writing command, the response was:",
                "timeout": 20
            },
            {
                "expectation": "\\+CME ERROR: invalid characters in text string"
            },
            {
                "expectation": ""
            },
            {
                "expectation": "\\[INFO\\] Received the following ping response:"
            },
            {
                "expectation": "\\d{1,},\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3},\\d{1,},\\d{1,}",
                "repeat": 4
            },
            {
                "expectation": ""
            },
            {
                "expectation": "\\[INFO\\] Command written successfully, the response was:"
            },
            {
                "expectation": "\\+CEREG: 5,5,\"[a-zA-Z0-9]{1,}\",\"[a-zA-Z0-9]{1,}\",\\d{1,}"
            },
            {
                "expectation": ""
            },
            {
                "expectation": "The value was: 5"
            },
        ],
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
                "repeat": 4,
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Printing root certificate..."
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
                "repeat": 3,
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Printing signer certificate..."
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
                "repeat": 3,
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
        "gpio": [
            {
                "expectation": "The voltage supplied is: 4\\.[0-9]+"
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
                "expectation": "\\[INFO\\] GET - HTTP status code: 200, data size: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] POST - HTTP status code: 200, data size: (\\d{1,})"
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
                "expectation": "\\[INFO\\] Successfully performed GET request. HTTP status code = 200, Size = (\\d{1,})"
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
                "expectation": "\\[INFO\\] GET - HTTP status code: 200, data size: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] POST - HTTP status code: 200, data size: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Body: {"
            }

        ],
        "https_with_header": [
            {
                "expectation": "\\[INFO\\] Starting HTTPS with header example"
            },
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Connected to operator: (.*)"
            },
            {
                "expectation": "\\[INFO\\] Performing GET with header..."
            },
            {
                "expectation": "\\[INFO\\] GET - HTTP status code: 200, data size: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Response: {"
            }
        ],
        "mqtt_aws": [
            {
                "expectation": "\\[INFO\\] Starting MQTT for AWS example"
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Connecting to MQTT broker.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Published message"
            },
            {
                "expectation": "\\[INFO\\] Got new message: \\{\\\"light\\\": 9, \\\"temp\\\": 9\\}"
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Published message"
            },
            {
                "expectation": "\\[INFO\\] Got new message: \\{\\\"light\\\": 9, \\\"temp\\\": 9\\}"
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Published message"
            },
            {
                "expectation": "\\[INFO\\] Got new message: \\{\\\"light\\\": 9, \\\"temp\\\": 9\\}"
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Closing MQTT connection"
            }
        ],
        "mqtt_azure": [
            {
                "expectation": "\\[INFO\\] Starting MQTT for Azure example"
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Connecting to MQTT broker.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Published message"
            },
            {
                "expectation": "\\[INFO\\] Published message"
            },
            {
                "expectation": "\\[INFO\\] Published message"
            },
            {
                "expectation": "\\[INFO\\] Closing MQTT connection"
            }
        ],
        "mqtt_custom_broker": [
            {
                "expectation": "\\[INFO\\] Starting MQTT with custom broker"
            },
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Connecting to MQTT broker.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Published message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Got new message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Published message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Got new message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Published message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Got new message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Closing MQTT connection"
            }
        ],
        "mqtt_low_power": [
            {
                "expectation": "\\[INFO\\] Starting MQTT with low power"
            },
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "timeout": 90,
                "expectation": "\\[INFO\\] Connecting to MQTT broker.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Published message: \\d{1,}"
            },
            {
                "expectation": "\\[INFO\\] Entering low power"
            },
            {
                "timeout": 90,
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Woke up!"
            },
            {
                "timeout": 90,
                "expectation": "\\[INFO\\] Connecting to MQTT broker.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Published message: \\d{1,}"
            },
            {
                "expectation": "\\[INFO\\] Entering low power"
            },
            {
                "timeout": 90,
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Woke up!"
            },
        ],
        "mqtt_password_authentication": [
            {
                "expectation": "\\[INFO\\] Starting MQTT with username and password example"
            },
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Connecting to MQTT broker.{0,}OK!"
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
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Published message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Got new message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Published message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\\[INFO\\] Got new message: Hello world: (\\d{1,})"
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Closing MQTT connection"
            }
        ],
        "mqtt_with_connection_loss_handling": [
            {
                "expectation": "\\[INFO\\] Starting MQTT with Connection Loss Handling"
            },
            {
                "expectation": "\r\n"
            },
            {
                "expectation": "\\[INFO\\] Not connected to the network. Attempting to connect!"
            },
            {
                "expectation": "\\[INFO\\] Connecting to operator.{0,}OK!"
            },
            {
                "expectation": "\\[INFO\\] Not connected to broker. Attempting to connect!"
            },
            {
                "expectation": "\\[INFO\\] Connecting to MQTT broker.{0,}OK!"
            },
            {
                "repeat": 5,
                "expectation": "\\[INFO\\] Published message: \\d{1,}. Failed publishes: \\d{1,}."
            },
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
                "expectation": "\\[INFO\\] Will now connect to the operator. If the board hasn't previously connected to the operator/network, establishing the connection the first time might take some time."
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
        "serial": [
            {
                "expectation": "Hello world from Serial3"
            },
            {
                "expectation": "\\[INFO\\] Hello world from Log"
            },
            {
                "expectation": "This is a message without a prefix"
            },
            {
                "expectation": "\\[DEBUG\\] A debug message"
            },
            {
                "expectation": "\\[INFO\\] An info message"
            },
            {
                "expectation": "\\[WARN\\] A warning message"
            },
            {
                "expectation": "\\[ERROR\\] An error message"
            },
            {
                "expectation": ""
            },
            {
                "expectation": "\\[INFO\\] An info message"
            },
            {
                "expectation": "\\[WARN\\] A warning message"
            },
            {
                "expectation": "\\[ERROR\\] An error message"
            },
            {
                "expectation": ""
            },
            {
                "expectation": "\\[WARN\\] A warning message"
            },
            {
                "expectation": "\\[ERROR\\] An error message"
            },
            {
                "expectation": ""
            },
            {
                "expectation": "\\[ERROR\\] An error message"
            },
            {
                "expectation": ""
            },
            {
                "expectation": ""
            },
            {
                "expectation": "\\[INFO\\] This is a number: 10"
            },
            {
                "expectation": "\\[INFO\\] This is a string: Hello world"
            },
            {
                "expectation": "\\[INFO\\] This is a hexadecimal and a string: 1F - Hello world"
            },
            {
                "expectation": "\\[INFO\\] This is a flash string"
            },
            {
                "expectation": "\\[INFO\\] This is a flash string with formatting: 10"
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
        ["arduino-cli", "compile", f"{sketch_path}", "-b", f"{BOARD_CONFIG}", "--build-path", f"{build_directory}", "--warnings", "all"], shell=True).returncode

    assert compilation_return_code == 0, f"{example_name} failed to compile, return code: {compilation_return_code}"

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

            for _ in range(0, repeat):
                if command != None:
                    command_stripped = command.strip("\r")
                    print(f"\tTesting command: {command_stripped}")

                    serial_handle.write(str.encode(command))
                    serial_handle.flush()

                serial_handle.timeout = timeout
                output = serial_handle.read_until().decode("utf-8")

                try:
                    response = re.search(expectation, output)
                except Exception as exception:
                    pytest.fail(f"\tRegex error for string \"{expectation}\". The error was: {str(exception)}")

                formatted_output = output.replace("\r", "\\r").replace("\n", "\\n")
                assert response != None, f"\tDid not get the expected response \"{expectation}\" within the timeout of {timeout}, got: \"{formatted_output}\""

                formatted_response = response.group(0).replace("\r", "\\r").replace("\n", "\\n")

                print(f"\tGot valid response: {formatted_response}")


def run_test(request, backend, session_config, example_test_data):
    backend.start_session(session_config)
    program(request, backend)
    run_example(request, backend, example_test_data)


# ------------------------------- TESTS -----------------------------------


def test_custom_at_commands(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_debug_modem(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_extract_certificates(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_gpio(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_http(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_http_get_time(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_https(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_https_with_header(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_mqtt_aws(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_mqtt_azure(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_mqtt_custom_broker(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_mqtt_low_power(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_mqtt_password_authentication(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_mqtt_with_connection_loss_handling(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_power_down(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_power_print_voltage(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_power_save(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_sandbox(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)


def test_serial(request, backend, session_config, example_test_data):
    run_test(request, backend, session_config, example_test_data)
