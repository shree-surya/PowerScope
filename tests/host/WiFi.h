#pragma once
#include <Arduino.h>
#include <DNSServer.h>
#define WIFI_AP 2
struct WiFiC{ void mode(int){} bool softAP(const char*,const char*,int,int,int){return true;} void setSleep(bool){} IPAddress softAPIP(){return IPAddress();} int softAPgetStationNum(){return 0;} };
static WiFiC WiFi;
