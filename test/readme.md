# Testing the examples  

This folder is for testing the example sketches against expected output and thus provide an integration test for each example sketch.

## Setup 

Install the python packages needed: `pip install -r requirements.txt`

## Running

* Run all tests: `pytest . --port <device_port> -s`
* Run a test for a specific example sketch: `pytest . --port <device_port> -s -k test_<sketch_name_without_dot_ino>`. For example for `debug_modem`: `pytest . --port COM7 -s -k test_debug_modem`

