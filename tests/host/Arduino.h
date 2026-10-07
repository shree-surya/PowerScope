#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <string>
using std::min; using std::max;
#define PROGMEM
template<class T> T constrain(T a,T lo,T hi){return a<lo?lo:(a>hi?hi:a);}
#define ADC_11db 3
struct String{ std::string s; const char* c_str()const{return s.c_str();} String(){} String(const char*c):s(c){} int toInt()const{return atoi(s.c_str());} float toFloat()const{return atof(s.c_str());} bool operator==(const char*c)const{return s==c;} };
#include <stdarg.h>
struct Print{ virtual size_t write(uint8_t)=0;
  size_t print(const char*s){size_t n=0;while(*s)n+=write((uint8_t)*s++);return n;} size_t print(char c){return write((uint8_t)c);}
  size_t printf(const char*f,...) __attribute__((format(printf,2,3))){char b[512];va_list a;va_start(a,f);vsnprintf(b,sizeof b,f,a);va_end(a);return print(b);} };
struct SerialS{void begin(int){} template<class...A> void printf(const char*f,A...a){ ::printf(f,a...);} void println(const char*t){ ::puts(t);} int available(){return 0;} int read(){return 0;} };
static SerialS Serial;
inline void analogSetAttenuation(int){} inline void analogReadResolution(int){}
extern uint64_t fakeT; extern double VPK_MV, IPK_MV, PHI, NOISE_SD, SPIKE_P;
inline double gauss(){ double u=(rand()+1.0)/(RAND_MAX+2.0), v=(rand()+1.0)/(RAND_MAX+2.0); return sqrt(-2*log(u))*cos(2*M_PI*v); }
inline uint32_t micros(){ fakeT+=5; return (uint32_t)fakeT; }
inline uint32_t millis(){ return (uint32_t)(fakeT/1000); }
inline int analogReadMilliVolts(int pin){
  double w=2*M_PI*50.0*(fakeT/1e6);
  if(pin==34) return (int)(1650+VPK_MV*sin(w)+0.5);
  double n=NOISE_SD*gauss(); if((rand()/(double)RAND_MAX)<SPIKE_P) n+=((rand()&1)?55:-55);
  return (int)(1650-IPK_MV*sin(w-PHI)+n+0.5);
} inline void delay(int){}
typedef void* TaskHandle_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))
#define pdMS_TO_TICKS(x) (x)
inline void vTaskDelay(int){}
inline int xTaskCreatePinnedToCore(void(*)(void*),const char*,int,void*,int,TaskHandle_t*,int){return 1;}
