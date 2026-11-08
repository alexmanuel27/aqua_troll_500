#!/usr/bin/env python3
"""Save Nano SDI-12 output as CSV plus a complete diagnostic log."""
import argparse
import csv
from datetime import datetime, timezone
import os
from pathlib import Path
import sys
import time
from zoneinfo import ZoneInfo


CSV_HEADER = ["Ch", "Parameter", "Value", "Unit", "Date/time (Europe/Brussels)"]


def table_row(line):
    """Copy identified rows from the Nano's table, keeping names and precision."""
    if not line.startswith("#"):
        return None
    cells = [cell.strip() for cell in line[1:].split("|")]
    if (len(cells) == 4 and cells[0].isdigit() and 1 <= int(cells[0]) <= 16
            and all(cells[1:]) and cells[3] != "?"):
        return cells
    return None


def record(device, output, log, interval):
    writer = csv.writer(output)
    writer.writerow(CSV_HEADER)
    output.flush()
    ready_at = time.monotonic() + 15  # Allow startup and identification after DTR reset.
    stop_sent_at = None
    ready = measuring = False
    next_scan = scan_deadline = 0
    scan_time = ""
    in_table = False
    try:
        while True:
            current = time.monotonic()
            if stop_sent_at is None and current >= ready_at:
                device.write(b"s\n")
                stop_sent_at = current
            if stop_sent_at is not None and not ready and current - stop_sent_at > 90:
                raise TimeoutError("El Nano no confirmo STOPPED. Revisa puerto y programa cargado.")
            if measuring and current > scan_deadline:
                raise TimeoutError("La ronda no termino en 180 s. Lecturas recibidas conservadas.")
            if ready and not measuring and current >= next_scan:
                device.write(b"m\n")
                measuring = True
                next_scan = current + interval
                scan_deadline = current + 180
            raw = device.readline()
            if not raw:
                continue
            now = datetime.now(timezone.utc).isoformat(timespec="milliseconds")
            line = raw.decode("ascii", errors="replace").rstrip("\r\n")
            log.write(f"{now}\t{line}\n")
            log.flush()
            print(line)
            if line.startswith("# Aqua TROLL 500 /"):
                # A reset clears the Nano's state; repeat the stop handshake.
                ready = measuring = False
                in_table = False
                scan_time = ""
                stop_sent_at = None
                ready_at = time.monotonic() + 15
            elif line == "# STOPPED" and stop_sent_at is not None and not ready:
                ready = True
                next_scan = time.monotonic()
                device.write(b"v\n")
            elif (line == "# SCAN_END" or line.startswith("# SCAN_FAILED") or
                  line.startswith(("# ERROR no_probe", "# ERROR unexpected_identification"))):
                if line != "# SCAN_END":
                    measuring = False
                output.flush()
                os.fsync(output.fileno())
                os.fsync(log.fileno())
                if time.monotonic() > next_scan:
                    print("La ronda supero el intervalo; la siguiente empezara al quedar libre.")
            if line == "# SCAN_BEGIN":
                scan_time = datetime.fromisoformat(now).astimezone(
                    ZoneInfo("Europe/Brussels")).strftime("%Y-%m-%d %H:%M:%S")
                in_table = False
            elif line == "# TABLE_BEGIN":
                in_table = True
            elif line == "# TABLE_END":
                in_table = False
                measuring = False
                output.flush()
                os.fsync(output.fileno())
                os.fsync(log.fileno())
            elif line.startswith("# TABLE_SKIPPED"):
                measuring = False
            elif in_table:
                row = table_row(line)
                if row is not None:
                    writer.writerow(row + [scan_time])
                    output.flush()
    finally:
        # m leaves the Nano paused; s also cancels any earlier repeat mode.
        try:
            device.write(b"s\n")
            device.flush()
        except OSError:
            pass
        output.flush()
        log.flush()
        os.fsync(output.fileno())
        os.fsync(log.fileno())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial port; use --list to find it")
    parser.add_argument("--list", action="store_true", help="List serial ports")
    parser.add_argument("--interval", type=int, default=600,
                        help="Seconds between scan requests (default: 600 = 10 minutes)")
    parser.add_argument("--output", type=Path, default=Path(
        datetime.now().strftime("aqua_troll_%Y%m%d_%H%M%S.csv")))
    args = parser.parse_args()
    if args.interval <= 0:
        parser.error("--interval debe ser un numero entero de segundos mayor que cero")
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        parser.exit(1, "Falta pyserial. Instala: python -m pip install pyserial==3.5\n")
    if args.list:
        for port in list_ports.comports():
            print(f"{port.device}\t{port.description}")
        return
    if not args.port:
        parser.error("Indica --port o usa --list")
    log_path = args.output.with_suffix(args.output.suffix + ".log")
    # Exclusive creation: refuse to overwrite an earlier experiment.
    if args.output.exists() or log_path.exists():
        parser.error("El CSV o su log ya existe; elige otro --output")
    try:
        with serial.Serial(args.port, 115200, timeout=1) as device, \
                args.output.open("x", newline="", encoding="utf-8") as output, \
                log_path.open("x", encoding="utf-8") as log:
            print(f"Guardando en {args.output}. Ctrl+C para terminar.")
            print("CSV: Ch, Parameter, Value, Unit y fecha/hora de Bruselas. Datos crudos en .log.")
            print(f"Intervalo: {args.interval} s entre inicios; espera inicial de unos 15-20 s.")
            print("Manten el portatil despierto, la tapa abierta y el monitor serie cerrado.")
            record(device, output, log, args.interval)
    except KeyboardInterrupt:
        print("\nRegistro terminado; archivos guardados.")
    except (OSError, serial.SerialException) as exc:
        parser.exit(1, f"No se pudo registrar: {exc}\n")


if __name__ == "__main__":
    main()
