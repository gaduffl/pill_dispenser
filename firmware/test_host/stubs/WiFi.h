#pragma once
enum { WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3 }; enum { WL_CONNECTED = 3 };
struct IPAddress { String toString() const { return String(); } };
struct WiFiT { void persistent(bool) {} bool setHostname(const char *) { return true; } void setAutoReconnect(bool) {} bool mode(int) { return true; } int getMode() { return 0; }
  int begin(const char *, const char *) { return 0; } int status() { return 0; } bool softAP(const char *) { return true; } bool softAPdisconnect(bool) { return true; }
  String SSID() { return String(); } IPAddress localIP() { return IPAddress(); } int RSSI() { return 0; } };
extern WiFiT WiFi;
