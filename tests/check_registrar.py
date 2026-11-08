"""Run with python3 tests/check_registrar.py; no probe or pyserial needed."""
from collections import deque
from contextlib import redirect_stdout, redirect_stderr
import csv
import io
from pathlib import Path
import re
import sys
from tempfile import TemporaryDirectory
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import registrar

PROFILE = re.findall(r'printSummaryRow\(\s*(\d+), F\("([^"]+)"\), F\("([^"]+)"\)\)',
                     (Path(__file__).resolve().parents[1] / "AquaTroll500/AquaTroll500.ino").read_text())


class Nano:
    def __init__(self, fail_first=False, reset_first=False, silent=False):
        self.now = 0
        self.events = deque()
        self.commands = []
        self.starts = []
        self.fail_first = fail_first
        self.reset_first = reset_first
        self.silent = silent

    def queue(self, line, delay):
        self.events.append((self.now + delay, (line + "\r\n").encode("ascii")))

    def write(self, command):
        self.commands.append(command)
        if command == b"s\n":
            self.queue("# STOPPED", 0.01)
        elif command == b"v\n":
            self.queue("# STATUS device_low=+32 device_high=+4 sensor_connections=+127", 0.01)
        elif command == b"m\n":
            if len(self.starts) >= 2:
                raise KeyboardInterrupt
            self.starts.append(self.now)
            if self.silent:
                return
            first = len(self.starts) == 1
            self.queue("# SCAN_BEGIN", 0.02)
            count = 1 if first and self.reset_first else 9 if first and self.fail_first else 20
            for channel in range(1, count + 1):
                group = (channel - 1) // 9
                value = "-99999.0" if channel == 8 else "+18.97"
                status = "MISSING_SENSOR" if channel == 8 else "RECEIVED"
                self.queue(f"5000,0,{group},{channel},{value},{status}", 16 + group * 17)
            if first and self.reset_first:
                self.queue("# Aqua TROLL 500 / Nano / SDI-12 with CRC", 17)
            elif first and self.fail_first:
                self.queue("# ERROR response_timeout_overflow_or_crc command=0MC1!", 20)
                self.queue("# SCAN_FAILED group=1", 20.01)
            else:
                self.queue("# SCAN_END", 51)
                self.queue("# TABLE_BEGIN", 51.01)
                self.queue("# Ch | Parameter | Value | Unit", 51.02)
                for channel, name, unit in PROFILE:
                    value = "NO_SENSOR" if channel == "8" else "+18.97"
                    self.queue(f"# {channel} | {name.strip()} | {value} | {unit}", 51.03)
                self.queue("# 17 | Unknown | +0.1 | ?", 51.04)
                self.queue("# TABLE_END", 51.05)

    def readline(self):
        if self.events and self.events[0][0] <= self.now + 1:
            self.now, line = self.events.popleft()
            return line
        if not self.events and len(self.starts) >= 2:
            raise KeyboardInterrupt
        self.now += 1
        return b""

    def flush(self):
        pass


def run(nano, interval=600):
    with TemporaryDirectory() as folder:
        csv_path, log_path = Path(folder) / "lake.csv", Path(folder) / "lake.csv.log"
        error = None
        with csv_path.open("w", newline="") as output, log_path.open("w") as log:
            with patch.object(registrar.time, "monotonic", lambda: nano.now):
                with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                    try:
                        registrar.record(nano, output, log, interval)
                    except (KeyboardInterrupt, TimeoutError) as exc:
                        error = exc
        with csv_path.open(newline="") as saved:
            rows = list(csv.DictReader(saved))
        text = log_path.read_text()
    assert nano.commands[-1] == b"s\n"
    assert b"r\n" not in nano.commands and b"a\n" not in nano.commands
    return rows, text, error


normal = Nano()
rows, text, error = run(normal)
assert isinstance(error, KeyboardInterrupt)
assert len(rows) == 32 and 600 <= normal.starts[1] - normal.starts[0] <= 601
assert list(rows[0]) == registrar.CSV_HEADER
assert rows[7]["Value"] == "NO_SENSOR" and "-99999.0,MISSING_SENSOR" in text
assert rows[1]["Parameter"] == "Actual conductivity" and rows[1]["Unit"] == "uS/cm"
assert rows[0]["Date/time (Europe/Brussels)"] == rows[15]["Date/time (Europe/Brussels)"]
assert len(rows[0]["Date/time (Europe/Brussels)"]) == 19
assert text.count("# SCAN_END") == 2 and text.count("# TABLE_END") == 2
assert all(1 <= int(row["Ch"]) <= 16 for row in rows)

rows, text, error = run(Nano(fail_first=True))
assert len(rows) == 16 and "# SCAN_FAILED group=1" in text
assert text.count("\t5000,0,") == 29  # All raw readings, including the partial round, survive.
assert text.count("# SCAN_END") == 1  # Earlier partial readings remain identifiable in the log.

reset = Nano(reset_first=True)
rows, text, error = run(reset)
assert len(rows) == 16 and "# Aqua TROLL 500 /" in text
assert text.count("\t5000,0,") == 21
assert reset.starts[1] - reset.starts[0] < 600

short = Nano()
rows, text, error = run(short, interval=10)
assert len(rows) == 32 and short.starts[1] - short.starts[0] >= 51

rows, text, error = run(Nano(silent=True))
assert isinstance(error, TimeoutError) and "180 s" in str(error)
assert not rows
assert registrar.table_row("# 5 | Resistivity | +99999 | ohm*cm") == ["5", "Resistivity", "+99999", "ohm*cm"]
for invalid in ("# Ch | Parameter | Value | Unit", "# 17 | Unknown | +1 | ?",
                "# 1 | Temperature | +18.97", "5000,0,0,1,+18.97,RECEIVED"):
    assert registrar.table_row(invalid) is None
print("Logger checks passed: 10-minute interval, saved CSV/log, partial scan, reset, no overlap, timeout, stop.")
