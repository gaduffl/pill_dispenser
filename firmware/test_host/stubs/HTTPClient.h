#pragma once
struct HTTPClient { void setTimeout(int){} bool begin(const char*){return true;} void addHeader(const char*,const char*){} int POST(const String&){return 200;} void end(){} };
