#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

inline bool validAddress(char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
         (c >= 'a' && c <= 'z');
}

// M/MC/XAC acknowledgement, without CR/LF: atttn.
inline bool parseAck(const char* text, char address, uint16_t& seconds,
                     uint8_t& count) {
  if (strlen(text) != 5 || text[0] != address) return false;
  for (uint8_t i = 1; i < 5; ++i)
    if (text[i] < '0' || text[i] > '9') return false;
  seconds = (text[1] - '0') * 100 + (text[2] - '0') * 10 + text[3] - '0';
  count = text[4] - '0';
  return true;
}

// Strict SDI-12 decimal tokens. Preserve the probe's original precision.
// CRC must already have been checked and removed by the caller.
inline int parseValues(const char* text, char address, char values[][10],
                       uint8_t capacity) {
  if (text[0] != address) return -1;
  const char* p = text + 1;
  uint8_t count = 0;
  while (*p) {
    if (count >= capacity || (*p != '+' && *p != '-')) return -1;
    const char* start = p++;
    uint8_t digits = 0, dots = 0;
    while ((*p >= '0' && *p <= '9') || *p == '.') {
      if (*p == '.') ++dots;
      else ++digits;
      ++p;
    }
    size_t length = p - start;
    if (!digits || digits > 7 || dots > 1 || length > 9) return -1;
    memcpy(values[count], start, length);
    values[count++][length] = '\0';
  }
  return count;
}
