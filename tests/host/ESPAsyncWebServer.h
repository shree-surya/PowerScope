#pragma once
#include <Arduino.h>
#include <functional>
enum WebRequestMethod{HTTP_GET=1,HTTP_POST=2};
struct AsyncWebParameter{ String value()const{return String("1");} };
struct AsyncWebServerResponse{ void addHeader(const char*,const char*){} };
struct AsyncResponseStream: public AsyncWebServerResponse, public Print{ size_t write(uint8_t)override{return 1;} using Print::print; using Print::printf; };
struct AsyncWebServerRequest{
  bool hasHeader(const char*){return false;} String header(const char*){return String();}
  bool hasParam(const char*){return false;} AsyncWebParameter* getParam(const char*){return nullptr;}
  AsyncWebServerResponse* beginResponse(int){return nullptr;}
  AsyncWebServerResponse* beginResponse(int,const char*,const char*){return nullptr;}
  AsyncWebServerResponse* beginResponse(int,const char*,const uint8_t*,size_t){return nullptr;}
  AsyncResponseStream* beginResponseStream(const char*){return nullptr;}
  void send(AsyncWebServerResponse*){} void send(int){} void send(int,const char*,const char*){}
  void redirect(const char*){}
};
typedef std::function<void(AsyncWebServerRequest*)> ArRequestHandlerFunction;
struct AsyncWebServer{ AsyncWebServer(int){} void on(const char*,int,ArRequestHandlerFunction){} void onNotFound(ArRequestHandlerFunction){} void begin(){} };
