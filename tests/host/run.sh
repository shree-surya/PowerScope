#!/bin/sh
# Builds the real PowerScope.ino against stub libraries and runs it on synthetic waveforms with noise.
# No hardware needed. Usage: sh tests/host/run.sh
cd "$(dirname "$0")" || exit 1
g++ -std=c++17 -O1 -Wall -I. test_measure.cpp -o /tmp/powerscope_host_test && /tmp/powerscope_host_test
