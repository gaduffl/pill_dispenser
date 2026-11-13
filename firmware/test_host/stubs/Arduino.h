// Host-side stand-in for the Arduino-ESP32 API: only for a syntax / type check of the sketch with g++.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <string>
#define ESP_ARDUINO_VERSION_MAJOR 3
#define PROGMEM
typedef const char *PGM_P;
enum { D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, D10 };
enum { INPUT, OUTPUT, INPUT_PULLUP };
enum { LOW, HIGH };
class String {
  std::string s;
 public:
  String() {} String(const char *c) : s(c ? c : "") {} String(int v) : s(std::to_string(v)) {} String(const std::string &x) : s(x) {}
  size_t length() const { return s.size(); } const char *c_str() const { return s.c_str(); }
  bool startsWith(const char *p) const { return s.rfind(p, 0) == 0; }
  String substring(int a) const { return String(s.substr(a)); } String substring(int a, int b) const { return String(s.substr(a, b - a)); }
  int indexOf(char c, int from = 0) const { auto p = s.find(c, from); return p == std::string::npos ? -1 : (int)p; }
  long toInt() const { return atol(s.c_str()); }
  bool operator==(const char *o) const { return s == o; } bool operator==(const String &o) const { return s == o.s; }
  String &operator=(const char *c) { s = c ? c : ""; return *this; }
  String &operator+=(const char *c) { s += c; return *this; } String &operator+=(char c) { s += c; return *this; }
};
struct SerialT { void begin(int) {} void println(const char *) {} int printf(const char *, ...) { return 0; } };
extern SerialT Serial;
struct EspT { uint64_t getEfuseMac() { return 0; } uint32_t getFreeHeap() { return 0; } void restart() {} };
extern EspT ESP;
void pinMode(int, int); void digitalWrite(int, int); int digitalRead(int); int analogRead(int); void analogReadResolution(int);
uint32_t millis(); uint32_t micros(); void delay(uint32_t); void delayMicroseconds(uint32_t);
bool ledcAttach(uint8_t, uint32_t, uint8_t); bool ledcWrite(uint8_t, uint32_t);
void configTzTime(const char *, const char *, const char * = nullptr);
// FreeRTOS
typedef void *SemaphoreHandle_t; typedef void *QueueHandle_t; typedef uint32_t TickType_t; typedef int BaseType_t;
#define pdTRUE 1
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(); SemaphoreHandle_t xSemaphoreCreateBinary();
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t, TickType_t); BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t);
BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t); BaseType_t xSemaphoreGive(SemaphoreHandle_t);
QueueHandle_t xQueueCreate(int, int); BaseType_t xQueueReceive(QueueHandle_t, void *, TickType_t); BaseType_t xQueueSend(QueueHandle_t, const void *, TickType_t);
BaseType_t xTaskCreate(void (*)(void *), const char *, int, void *, int, void *); void vTaskDelay(TickType_t);
