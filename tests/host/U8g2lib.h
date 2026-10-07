#pragma once
#include <Arduino.h>
#define U8X8_PIN_NONE 255
#define U8G2_R0 0
struct U8G2_SH1106_128X64_NONAME_F_HW_I2C{
 U8G2_SH1106_128X64_NONAME_F_HW_I2C(int,int){}
 void begin(){} void setBusClock(long){} void clearBuffer(){} void sendBuffer(){}
 void setFont(const uint8_t*){} void drawStr(int,int,const char*){}
 int getStrWidth(const char*){return 0;} void drawLine(int,int,int,int){}
 void drawPixel(int,int){} void drawHLine(int,int,int){} void drawDisc(int,int,int){} void drawCircle(int,int,int){}
};
extern const uint8_t u8g2_font_6x10_tr[],u8g2_font_5x8_tr[],u8g2_font_helvB12_tr[],u8g2_font_6x12_tr[],u8g2_font_helvB14_tr[];
