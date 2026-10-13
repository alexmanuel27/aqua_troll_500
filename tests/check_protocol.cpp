// Run: c++ -std=c++11 -Wall -Wextra -pedantic tests/check_protocol.cpp -o /tmp/aquatroll-check && /tmp/aquatroll-check
#include "../AquaTroll500/protocol.h"
#include <assert.h>
#include <initializer_list>
#include <stdio.h>

int main() {
  uint16_t seconds = 0;
  uint8_t count = 0;
  assert(parseAck("00159", '0', seconds, count) && seconds == 15 && count == 9);
  assert(parseAck("A0000", 'A', seconds, count) && seconds == 0 && count == 0);
  for (const char* bad : {"", "0", "0015", "10159", "001x9", "001599"})
    assert(!parseAck(bad, '0', seconds, count));
  char values[9][10] = {};
  assert(parseValues("0+21.12345-0.01+0", '0', values, 9) == 3);
  assert(strcmp(values[0], "+21.12345") == 0);
  assert(strcmp(values[1], "-0.01") == 0);
  assert(strcmp(values[2], "+0") == 0);
  assert(parseValues("0-99999.0", '0', values, 1) == 1);
  assert(strtod(values[0], nullptr) == -99999.0);
  assert(parseValues("0", '0', values, 9) == 0);
  assert(parseValues("z+.1-.2", 'z', values, 2) == 2);
  for (const char* bad : {"", "1+2", "0+", "0+.", "0+1.2.3", "0+12345678",
                          "0+1e3", "0+NaN", "0+1 garbage", "0++2", "0 2"})
    assert(parseValues(bad, '0', values, 9) == -1);
  assert(parseValues("0+1+2", '0', values, 1) == -1);
  for (char c : {'0', '9', 'a', 'z', 'A', 'Z'}) assert(validAddress(c));
  for (char c : {'\0', '?', ':', '[', '!'}) assert(!validAddress(c));
  puts("Protocol checks passed: acknowledgements, decimals, missing sensor, malformed/truncated data.");
}
