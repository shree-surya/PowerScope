#pragma once
#include <Arduino.h>
#include <map>
#include <vector>
// Keeps values in a map so a test can check what was saved and "reboot" by calling setup() again.
struct Preferences{
  std::map<std::string,std::vector<uint8_t>> m;
  bool begin(const char*,bool){return true;}
  template<class T> size_t put(const char*k,const T&v){const uint8_t*p=(const uint8_t*)&v;m[k]=std::vector<uint8_t>(p,p+sizeof(T));return sizeof(T);}
  template<class T> T get(const char*k,T d){auto it=m.find(k);if(it==m.end()||it->second.size()!=sizeof(T))return d;T v;memcpy(&v,it->second.data(),sizeof(T));return v;}
  float getFloat(const char*k,float d){return get(k,d);}   size_t putFloat(const char*k,float v){return put(k,v);}
  double getDouble(const char*k,double d){return get(k,d);} size_t putDouble(const char*k,double v){return put(k,v);}
  int32_t getInt(const char*k,int32_t d){return get(k,d);} size_t putInt(const char*k,int32_t v){return put(k,v);}
  String getString(const char*k,const char*d){auto it=m.find(k);if(it==m.end())return String(d);return String(std::string(it->second.begin(),it->second.end()).c_str());}
  size_t putString(const char*k,const char*v){m[k]=std::vector<uint8_t>(v,v+strlen(v));return strlen(v);}
  size_t getBytesLength(const char*k){auto it=m.find(k);return it==m.end()?0:it->second.size();}
  size_t getBytes(const char*k,void*buf,size_t n){auto it=m.find(k);if(it==m.end())return 0;size_t c=std::min(n,it->second.size());memcpy(buf,it->second.data(),c);return c;}
  size_t putBytes(const char*k,const void*buf,size_t n){const uint8_t*p=(const uint8_t*)buf;m[k]=std::vector<uint8_t>(p,p+n);return n;}
  bool remove(const char*k){return m.erase(k)>0;}
};
