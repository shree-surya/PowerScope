#pragma once
#include <Arduino.h>
struct Preferences{ bool begin(const char*,bool){return true;} float getFloat(const char*,float d){return d;} size_t putFloat(const char*,float){return 4;} };
