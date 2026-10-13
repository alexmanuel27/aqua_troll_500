// Arduino Nano classic (ATmega328P, 5 V). Wiring and operation: ../README.md
#include <SDI12.h>
#include "protocol.h"

constexpr uint8_t DATA_PIN = 7;
constexpr unsigned long WAKE_BREAK_MS = 100;
constexpr unsigned long INTERVAL_MS = 60000;  // Pause after a complete scan.
constexpr unsigned long RESPONSE_TIMEOUT_MS = 1500;
constexpr unsigned long STARTUP_MS = 5000;
constexpr uint8_t RETRIES = 3;
SDI12 probe(DATA_PIN);
String response;
char address = '\0';
bool running = false;
unsigned long lastScan = 0;
constexpr uint8_t SUMMARY_CHANNELS = 20;
char scanValues[SUMMARY_CHANNELS][10];  // Original tokens from this scan only.

bool readLine() {
  response = "";
  unsigned long started = millis();
  while (millis() - started < RESPONSE_TIMEOUT_MS) {
    int available = probe.available();
    if (available < 0) return false;  // Library buffer overflow.
    if (available) {
      int c = probe.read();
      if (c == '\n') return response.length() > 0;
      if (c == '\r') continue;
      // CRC encoding can contain DEL (0x7f); it is valid protocol data.
      if (c < 32 || c > 127 || response.length() >= 80) return false;
      response += (char)c;
    } else delay(1);
  }
  return false;  // Never interpret a timeout as a zero measurement.
}

bool request(const char* command, bool crc = false) {
  for (uint8_t attempt = 0; attempt < RETRIES; ++attempt) {
    probe.clearBuffer();
    Serial.flush();  // Finish USB output before SDI-12 disables interrupts.
    // Wake for 100 ms before the library's normal break/mark sequence.
    // Avoid extraWakeTime: long delayMicroseconds calls overflow on AVR.
    probe.forceHold();  // Output mode, SDI-12 pin interrupt disabled.
    digitalWrite(DATA_PIN, HIGH);
    delay(WAKE_BREAK_MS);
    digitalWrite(DATA_PIN, LOW);
    delay(9);
    probe.sendCommand(command, 0);
    if (readLine() && (command[0] == '?' || response[0] == address)) {
      if (!crc) return true;
      if (response.length() >= 4 && probe.verifyCRC(response)) {
        response.remove(response.length() - 3);
        return true;
      }
    }
    delay(200);
  }
  Serial.print(F("# ERROR response_timeout_overflow_or_crc command="));
  Serial.println(command);
  return false;
}

// Wait the advertised measurement time, draining optional service requests.
void waitReady(uint16_t seconds) {
  unsigned long started = millis();
  while (millis() - started < seconds * 1000UL + 250) {
    if (probe.available() < 0) probe.clearBuffer();
    while (probe.available() > 0) probe.read();
    delay(1);
  }
}

bool identify() {
  address = '\0';
  running = false;
  if (!request("?!") || response.length() != 1 || !validAddress(response[0])) {
    Serial.println(F("# ERROR no_probe: check 12V, common GND and white wire"));
    return false;
  }
  address = response[0];
  char command[] = {address, 'I', '!', '\0'};
  if (!request(command) || response.length() < 4 ||
      strstr(response.c_str(), "IN-SITU") == nullptr) {
    address = '\0';
    Serial.println(F("# ERROR unexpected_identification"));
    return false;
  }
  Serial.print(F("# ID "));
  Serial.println(response);
  Serial.println(F("# Summary uses a provisional 20-channel profile; see README."));
  return true;
}

int measureGroup(uint8_t group) {
  char command[8];
  if (group == 0) snprintf(command, sizeof(command), "%cMC!", address);
  else snprintf(command, sizeof(command), "%cMC%u!", address, group);
  unsigned long started = millis();
  if (!request(command)) return -1;
  uint16_t seconds;
  uint8_t expected;
  if (!parseAck(response.c_str(), address, seconds, expected)) {
    Serial.print(F("# ERROR invalid_measurement_ack command="));
    Serial.print(command);
    Serial.print(F(" len=")); Serial.print(response.length());
    Serial.print(F(" hex="));
    for (unsigned int i = 0; i < response.length(); ++i) {
      if (i) Serial.print(' ');
      if ((uint8_t)response[i] < 16) Serial.print('0');
      Serial.print((uint8_t)response[i], HEX);
    }
    Serial.println();
    return -1;
  }
  if (!expected) return 0;
  waitReady(seconds);
  char values[9][10];
  uint8_t received = 0;
  for (uint8_t block = 0; block < 10 && received < expected; ++block) {
    snprintf(command, sizeof(command), "%cD%u!", address, block);
    if (!request(command, true)) return -1;
    int count = parseValues(response.c_str(), address, values + received,
                            expected - received);
    if (count <= 0) {
      Serial.println(F("# ERROR invalid_or_incomplete_data_group"));
      return -1;
    }
    received += count;
  }
  if (received != expected) {
    Serial.println(F("# ERROR incomplete_data_group"));
    return -1;
  }
  // Publish only complete, CRC-checked groups; never reuse a previous reading.
  for (uint8_t i = 0; i < received; ++i) {
    uint8_t index = group * 9 + i;
    if (index < SUMMARY_CHANNELS)
      memcpy(scanValues[index], values[i], sizeof(values[i]));
    Serial.print(started);
    Serial.print(','); Serial.print(address);
    Serial.print(','); Serial.print(group);
    Serial.print(','); Serial.print(group * 9 + i + 1);
    Serial.print(','); Serial.print(values[i]);
    Serial.print(',');
    Serial.println(strtod(values[i], nullptr) == -99999.0
                       ? F("MISSING_SENSOR") : F("RECEIVED"));
  }
  return received;
}

void verifyStatus() {
  if (!address && !identify()) return;
  char command[] = {address, 'V', '!', '\0'};
  uint16_t seconds;
  uint8_t count;
  if (!request(command) || !parseAck(response.c_str(), address, seconds, count) ||
      seconds > 3 || count != 3) {
    Serial.println(F("# ERROR invalid_verification_ack"));
    return;
  }
  waitReady(seconds);
  char dataCommand[] = {address, 'D', '0', '!', '\0'};
  char values[3][10];
  if (!request(dataCommand) ||
      parseValues(response.c_str(), address, values, 3) != 3) {
    Serial.println(F("# ERROR invalid_verification_data"));
    return;
  }
  Serial.print(F("# STATUS device_low=")); Serial.print(values[0]);
  Serial.print(F(" device_high=")); Serial.print(values[1]);
  Serial.print(F(" sensor_connections=")); Serial.println(values[2]);
}

void printSummaryRow(uint8_t channel, const __FlashStringHelper* parameter,
                     const __FlashStringHelper* unit) {
  const char* value = scanValues[channel - 1];
  if (strtod(value, nullptr) == -99999.0) value = "NO_SENSOR";
  Serial.print(F("# "));
  if (channel < 10) Serial.print(' ');
  Serial.print(channel);
  Serial.print(F(" | ")); Serial.print(parameter);
  Serial.print(F(" | ")); Serial.print(value);
  for (uint8_t width = strlen(value); width < 10; ++width) Serial.print(' ');
  Serial.print(F(" | ")); Serial.println(unit);
}

void printSummary(uint8_t total) {
  if (!total) return;
  bool complete = (total == SUMMARY_CHANNELS);
  for (uint8_t i = 0; i < SUMMARY_CHANNELS; ++i)
    if (!scanValues[i][0]) complete = false;
  if (!complete) {
    Serial.print(F("# TABLE_SKIPPED: profile requires 20 channels; received="));
    Serial.println(total);
    return;
  }
  Serial.println(F("# TABLE_BEGIN"));
  // ponytail: fixed observed profile; read Modbus metadata if sensors/order change.
  Serial.println(F("# Provisional mapping: P1=cond/temp P2=pH/ORP P3=empty P4=RDO."));
  Serial.println(F("# Default units assumed."));
  Serial.println(F("# Ch | Parameter                      | Value      | Unit"));
  Serial.println(F("# ---+---------------------------------+------------+----------"));
  printSummaryRow( 1, F("Temperature (conductivity)     "), F("degC"));
  printSummaryRow( 2, F("Actual conductivity            "), F("uS/cm"));
  printSummaryRow( 3, F("Specific conductivity          "), F("uS/cm"));
  printSummaryRow( 4, F("Salinity                       "), F("PSU"));
  printSummaryRow( 5, F("Resistivity                    "), F("ohm*cm"));
  printSummaryRow( 6, F("Water density                  "), F("g/cm3"));
  printSummaryRow( 7, F("Total dissolved solids (TDS)   "), F("ppt"));
  printSummaryRow( 8, F("pH                             "), F("pH"));
  printSummaryRow( 9, F("pH electrode potential         "), F("mV"));
  printSummaryRow(10, F("ORP                            "), F("mV"));
  printSummaryRow(11, F("Dissolved oxygen               "), F("mg/L"));
  printSummaryRow(12, F("Oxygen saturation              "), F("%"));
  printSummaryRow(13, F("Oxygen partial pressure        "), F("torr"));
  printSummaryRow(14, F("Temperature (RDO)              "), F("degC"));
  printSummaryRow(15, F("Supply voltage                 "), F("V"));
  printSummaryRow(16, F("Barometric pressure            "), F("mmHg"));
  Serial.println(F("# TABLE_END"));
}

void measureAll() {
  memset(scanValues, 0, sizeof(scanValues));
  if (!address && !identify()) return;
  Serial.println(F("# SCAN_BEGIN"));
  uint8_t total = 0;
  // ponytail: one probe, sequential groups; use Modbus for metadata or simultaneity.
  for (uint8_t group = 0; group < 10; ++group) {
    int count = measureGroup(group);
    if (count < 0) {
      Serial.print(F("# SCAN_FAILED group=")); Serial.println(group);
      return;
    }
    if (count == 0) {
      if (group == 0) Serial.println(F("# No configured channels. Send a to auto-configure."));
      Serial.println(F("# SCAN_END"));
      printSummary(total);
      return;
    }
    total += count;
  }
  Serial.println(F("# SCAN_END"));
  printSummary(total);
}

void autoConfigure() {
  // Explicit user command only: replaces the SDI-12 channel list, not calibration.
  running = false;
  if (!address && !identify()) return;
  char command[] = {address, 'X', 'A', 'C', '!', '\0'};
  uint16_t seconds;
  uint8_t count;
  if (!request(command) || !parseAck(response.c_str(), address, seconds, count) ||
      seconds > 5 || count != 1) {
    Serial.println(F("# ERROR invalid_autoconfigure_ack"));
    return;
  }
  waitReady(seconds);
  char dataCommand[] = {address, 'D', '0', '!', '\0'};
  char values[1][10];
  if (!request(dataCommand, true)) {
    Serial.println(F("# ERROR invalid_autoconfigure_data"));
    return;
  }
  if (parseValues(response.c_str(), address, values, 1) != 1) {
    Serial.print(F("# ERROR invalid_autoconfigure_data len="));
    Serial.print(response.length());
    Serial.print(F(" hex="));
    for (unsigned int i = 0; i < response.length(); ++i) {
      if (i) Serial.print(' ');
      if ((uint8_t)response[i] < 16) Serial.print('0');
      Serial.print((uint8_t)response[i], HEX);
    }
    Serial.println();
    return;
  }
  double total = strtod(values[0], nullptr);
  if (total < 1 || total > 30 || total != (int)total) {
    Serial.println(F("# ERROR autoconfigure_failed"));
    return;
  }
  Serial.print(F("# CONFIGURED channels=")); Serial.println((int)total);
  Serial.println(F("# Channel mapping changed: verify names/units again. Send m or r."));
}

void setup() {
  Serial.begin(115200);
  response.reserve(81);
  probe.begin();
  delay(STARTUP_MS);
  Serial.println(F("# Aqua TROLL 500 / Nano / SDI-12 with CRC"));
  Serial.println(F("# i=identify v=status a=auto-configure m=measure r=repeat s=stop"));
  Serial.println(F("uptime_ms,address,group,channel,value,status"));
  // Begin paused: the operator explicitly starts measurement and logging.
  identify();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'i') identify();
    else if (c == 'v') verifyStatus();
    else if (c == 'a') autoConfigure();
    else if (c == 's') { running = false; Serial.println(F("# STOPPED")); }
    else if (c == 'm' || c == 'r') {
      running = (c == 'r');
      measureAll();
      lastScan = millis();
    }
  }
  if (running && millis() - lastScan >= INTERVAL_MS) {
    measureAll();
    lastScan = millis();
  }
}
