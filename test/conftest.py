import os

def pytest_addoption(parser):

    file_directory = os.path.dirname(os.path.realpath(__file__))

    # TODO: fix proper pathing
    sketch_dir = f"{file_directory}/../examples"
    build_dir = f"{file_directory}/build"


    parser.addoption("--port", action="store", help="Device port (COMx, /dev/ttyACMx, etc.)", required=True)
    parser.addoption("--device", action="store", help="Device to test against", default="avr128db48")
    parser.addoption("--logging", action="store", help="Logging verbosity level (critical, error, warning, info, debug)", default="warn")
    parser.addoption("--sketchdir", action="store", help="Relative path to the sketch directory with the example sketches", default=sketch_dir)
    parser.addoption("--builddir", action="store", help="Relative path to the build directory", default=build_dir)
