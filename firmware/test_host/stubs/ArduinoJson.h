// loose stand-in: every JSON type is the same permissive proxy (checks syntax and names, not ArduinoJson overloads)
#pragma once
struct DeserializationError { explicit operator bool() const { return false; } };
struct V {
  template <class T> V &operator=(const T &) { return *this; }
  template <class T> T to() { return T(); }
  template <class T> T as() const { return T(); }
  template <class T> bool is() const { return true; }
  V operator[](const char *) const { return V(); }
  size_t size() const { return 0; } bool isNull() const { return false; }
  template <class T> T add() { return T(); }
  template <class T> void add(const T &) {}
  const V *begin() const { return this; } const V *end() const { return this; }
};
template <class T> T operator|(const V &, T def) { return def; }
typedef V JsonDocument; typedef V JsonObject; typedef V JsonArray; typedef V JsonObjectConst; typedef V JsonArrayConst; typedef V JsonVariantConst;
inline size_t serializeJson(const V &, String &) { return 0; }
inline DeserializationError deserializeJson(V &, const char *) { return DeserializationError(); }
inline DeserializationError deserializeJson(V &, const String &) { return DeserializationError(); }
