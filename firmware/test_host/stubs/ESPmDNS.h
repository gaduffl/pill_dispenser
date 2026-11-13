#pragma once
struct MDNST { bool begin(const char*){return true;} void addService(const char*,const char*,int){} void addServiceTxt(const char*,const char*,const char*,const char*){} void addServiceTxt(const char*,const char*,const char*,String){} }; extern MDNST MDNS;
