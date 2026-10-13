// Run: c++ -std=c++11 -Wall -Wextra -pedantic -Itests/stubs tests/check_summary.cpp -o /tmp/aquatroll-summary && /tmp/aquatroll-summary
#include "../AquaTroll500/AquaTroll500.ino"
#include <algorithm>
#include <stdio.h>
#include <vector>

using Replies = std::deque<std::pair<std::string, std::string>>;

Replies frames(const std::vector<std::string>& values) {
  Replies result;
  size_t group = 0;
  for (size_t first = 0; first < values.size(); first += 9, ++group) {
    size_t count = std::min(size_t(9), values.size() - first);
    std::string command = group ? "0MC" + std::to_string(group) + "!" : "0MC!";
    result.push_back({command, "0016" + std::to_string(count)});
    for (size_t block = 0; block * 3 < count; ++block) {
      std::string data = "0";
      for (size_t i = block * 3; i < std::min(count, block * 3 + 3); ++i)
        data += values[first + i];
      result.push_back({"0D" + std::to_string(block) + "!", data + "CRC"});
    }
  }
  result.push_back({group ? "0MC" + std::to_string(group) + "!" : "0MC!", "00000"});
  return result;
}

std::string run(Replies replies, char command = 'm') {
  Serial.output.clear();
  Serial.input = command;
  address = '0';
  probe.replies = replies;
  loop();
  assert(probe.replies.empty());
  return Serial.output;
}

int main() {
  // Alex's third tap-water measurement, preserving precision and signs.
  std::vector<std::string> values = {
    "+18.97", "+707.20", "+799.17", "+0.40", "+1414.0", "+0.999", "+0.52",
    "+8.00", "-53.7", "+266.2", "+9.32", "+101.2", "+156.68", "+18.97",
    "+11.885", "+755.41", "-0.069", "+0.17", "+0.16", "-0.16"
  };
  std::string output = run(frames(values));
  size_t end = output.find("# SCAN_END\n");
  size_t table = output.find("# TABLE_BEGIN\n");
  assert(end != std::string::npos && table > end && table != std::string::npos);
  assert(output.find("# TABLE_END\n") != std::string::npos);
  assert(output.find("0,0,2,+707.20,RECEIVED\n") != std::string::npos);
  assert(output.find("0,2,20,-0.16,RECEIVED\n") != std::string::npos);
  std::string summary = output.substr(table);
  for (size_t i = 0; i < values.size(); ++i) {
    std::string prefix = "# " + std::string(i < 9 ? " " : "") +
                         std::to_string(i + 1) + " | ";
    size_t start = summary.find(prefix);
    if (i >= 16) {
      assert(start == std::string::npos);  // Unknown channels stay in raw CSV only.
      continue;
    }
    assert(start != std::string::npos);
    std::string row = summary.substr(start, summary.find('\n', start) - start);
    assert(row.find('|', 6) == 39 && row.find('|', 40) == 52);
    assert(row.find(" | " + values[i]) != std::string::npos);
  }
  assert(summary.find("Parameter") != std::string::npos);
  assert(summary.find("Actual conductivity") != std::string::npos);
  assert(summary.find("Dissolved oxygen") != std::string::npos);
  assert(summary.find(" | uS/cm\n") != std::string::npos);
  assert(summary.find(" | mg/L\n") != std::string::npos);

  // r shares the summary; missing sensors never look like numeric measurements.
  values[0] = "-99999.0";
  output = run(frames(values), 'r');
  assert(output.find("0,0,1,-99999.0,MISSING_SENSOR\n") != std::string::npos);
  assert(output.substr(output.find("# TABLE_BEGIN")).find("NO_SENSOR") != std::string::npos);

  // Changed channel counts and empty scans cannot reuse the last 20-value table.
  output = run(frames({"+22.00"}));
  assert(output.find("# TABLE_SKIPPED") != std::string::npos);
  assert(output.find("# TABLE_BEGIN") == std::string::npos);
  for (size_t i = 1; i < SUMMARY_CHANNELS; ++i) assert(scanValues[i][0] == '\0');
  output = run(frames({}));
  assert(output.find("# SCAN_END") != std::string::npos);
  assert(output.find("# TABLE_BEGIN") == std::string::npos);

  // A later failed group leaves raw earlier groups, but no successful summary.
  Replies failed = frames(std::vector<std::string>(values.begin(), values.begin() + 9));
  failed.back().second = "001x9";
  output = run(failed);
  assert(output.find("# SCAN_FAILED group=1") != std::string::npos);
  assert(output.find("# SCAN_END") == std::string::npos);
  assert(output.find("# TABLE_BEGIN") == std::string::npos);
  puts("Summary checks passed: real scan, m/r, precision, missing sensor, reset, changed count, failed scan.");
}
