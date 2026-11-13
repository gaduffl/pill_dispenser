#pragma once
enum HTTPMethod { HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE, HTTP_OPTIONS };
struct WebServer { WebServer(int) {} void sendHeader(const char *, const char *) {} void send(int, const char * = nullptr, const String & = String("")) {}
  void send_P(int, PGM_P, PGM_P) {} String header(const char *) { return String(); } bool hasArg(const char *) { return false; } int args() { return 0; } String argName(int) { return String(); } String arg(const char *) { return String(); }
  String uri() { return String(); } HTTPMethod method() { return HTTP_GET; } void collectHeaders(const char *[], size_t) {} void onNotFound(void (*)()) {} void begin() {} void handleClient() {} };
