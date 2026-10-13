// Host-only Arduino/SDI12 stand-ins for exercising the actual sketch.
// Timing and CRC hardware/library behavior are not simulated.
#pragma once
#include <assert.h>
#include <deque>
#include <sstream>
#include <string>
#include <utility>
#include <stdint.h>

struct __FlashStringHelper {};
#define F(text) reinterpret_cast<const __FlashStringHelper*>(text)
constexpr int HIGH = 1, LOW = 0, HEX = 16;

struct String : std::string {
  using std::string::operator=;
  void remove(size_t from) { erase(from); }
};

struct FakeSerial {
  std::string output, input;
  void begin(unsigned long) {}
  void flush() {}
  int available() { return input.size(); }
  int read() { char c = input.front(); input.erase(0, 1); return c; }
  void print(const char* text) { output += text; }
  void print(char* text) { output += text; }
  void print(const __FlashStringHelper* text) {
    print(reinterpret_cast<const char*>(text));
  }
  void print(const std::string& text) { output += text; }
  void print(const String& text) { output += text; }
  void print(char c) { output += c; }
  template<class T> void print(T value) { output += std::to_string(value); }
  void print(uint8_t value, int base) {
    assert(base == HEX);
    std::ostringstream text;
    text << std::uppercase << std::hex << static_cast<unsigned>(value);
    output += text.str();
  }
  template<class T> void println(T value) { print(value); output += '\n'; }
  void println() { output += '\n'; }
};
static FakeSerial Serial;
static unsigned long fakeTime = 0;
inline unsigned long millis() { return fakeTime++; }
inline void delay(unsigned long ms) { fakeTime += ms; }
inline void digitalWrite(uint8_t, int) {}

struct SDI12 {
  std::deque<std::pair<std::string, std::string>> replies;
  std::string buffer;
  explicit SDI12(uint8_t) {}
  void begin() {}
  void forceHold() {}
  void clearBuffer() { buffer.clear(); }
  int available() { return buffer.size(); }
  int read() { char c = buffer.front(); buffer.erase(0, 1); return c; }
  void sendCommand(const char* command, int) {
    assert(!replies.empty() && replies.front().first == command);
    buffer = replies.front().second + "\r\n";
    replies.pop_front();
  }
  bool verifyCRC(const String& text) {
    return text.size() >= 3 && text.substr(text.size() - 3) == "CRC";
  }
};
