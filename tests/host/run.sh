#!/bin/sh
# Builds the real PowerScope.ino against stub libraries and runs it on synthetic waveforms with noise.
# Like the Arduino IDE, it first adds a declaration of every function above the first function
# (arduino_protos.py), so a type used before it is defined fails here too.
# No hardware needed. Usage: sh tests/host/run.sh
cd "$(dirname "$0")" || exit 1
mkdir -p build
python3 arduino_protos.py ../../firmware/PowerScope/PowerScope.ino build/PowerScope.ino || exit 1
g++ -std=c++17 -O1 -Wall -I. -I../../firmware/PowerScope test_measure.cpp -o build/powerscope_host_test && build/powerscope_host_test
