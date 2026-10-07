#pragma once
#include <Arduino.h>
enum class DNSReplyCode{NoError};
struct IPAddress{ String toString()const{return String("192.168.4.1");} };
struct DNSServer{ void setErrorReplyCode(DNSReplyCode){} bool start(int,const char*,IPAddress){return true;} void processNextRequest(){} };
