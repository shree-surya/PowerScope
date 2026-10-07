#pragma once
#include <Arduino.h>
#include <functional>
#include <map>
// Keeps the route handlers so a test can call them: see call() in test_measure.cpp.
enum WebRequestMethod{HTTP_GET=1,HTTP_POST=2};
struct AsyncWebParameter{ std::string v; String value()const{return String(v.c_str());} };
struct AsyncWebServerResponse{ int code=0; std::string body; void addHeader(const char*,const char*){} };
struct AsyncResponseStream: public AsyncWebServerResponse, public Print{ size_t write(uint8_t c)override{body+=(char)c;return 1;} using Print::print; using Print::printf; };
struct AsyncWebServerRequest{
  std::map<std::string,AsyncWebParameter> params; AsyncWebServerResponse out; AsyncResponseStream stream;
  bool hasHeader(const char*){return false;} String header(const char*){return String();}
  bool hasParam(const char*k){return params.count(k)>0;} AsyncWebParameter* getParam(const char*k){return &params[k];}
  AsyncWebServerResponse* beginResponse(int c){out=AsyncWebServerResponse();out.code=c;return &out;}
  AsyncWebServerResponse* beginResponse(int c,const char*,const char*b){out=AsyncWebServerResponse();out.code=c;out.body=b;return &out;}
  AsyncWebServerResponse* beginResponse(int c,const char*,const uint8_t*,size_t){out=AsyncWebServerResponse();out.code=c;return &out;}
  AsyncResponseStream* beginResponseStream(const char*){stream.body.clear();stream.code=200;return &stream;}
  void send(AsyncWebServerResponse*r){if(r!=&out){out.code=r->code;out.body=r->body;}} void send(int c){out.code=c;out.body.clear();}
  void send(int c,const char*,const char*b){out.code=c;out.body=b;}
  void redirect(const char*){out.code=302;}
};
typedef std::function<void(AsyncWebServerRequest*)> ArRequestHandlerFunction;
struct AsyncWebServer{
  std::map<std::pair<std::string,int>,ArRequestHandlerFunction> routes;
  AsyncWebServer(int){}
  void on(const char*u,int m,ArRequestHandlerFunction f){routes[{u,m}]=f;}
  void onNotFound(ArRequestHandlerFunction){} void begin(){}
};
