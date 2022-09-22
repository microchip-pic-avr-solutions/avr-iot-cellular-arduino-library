import os

from pathlib import Path

def pytest_addoption(parser):
    """Sets up the arguments which are passed to the tests

    Args:
        parser (obj): PyTest parser object
    """

    file_directory = os.path.dirname(os.path.realpath(__file__))

    sketch_dir = Path(f"{file_directory}/../examples")
    build_dir = Path(f"{file_directory}/build")

    parser.addoption("--port", action="store", help="Device port (COMx, /dev/ttyACMx, etc.)", required=True)
    parser.addoption("--device", action="store", help="Device to test against", default="avr128db48")
    parser.addoption("--sketchdir", action="store",
                     help="Relative path to the sketch directory with the example sketches", default=sketch_dir)
    parser.addoption("--builddir", action="store", help="Relative path to the build directory", default=build_dir)
