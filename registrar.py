#!/usr/bin/env python3
"""Save Nano SDI-12 output as CSV plus a complete diagnostic log."""
import argparse
import csv
from datetime import datetime, timezone
from pathlib import Path
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial port; use --list to find it")
    parser.add_argument("--list", action="store_true", help="List serial ports")
    parser.add_argument("--output", type=Path, default=Path(
        datetime.now().strftime("aqua_troll_%Y%m%d_%H%M%S.csv")))
    args = parser.parse_args()
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
    header = ["uptime_ms", "address", "group", "channel", "value", "status"]
    try:
        with serial.Serial(args.port, 115200, timeout=1) as device, \
                args.output.open("x", newline="", encoding="utf-8") as output, \
                log_path.open("x", encoding="utf-8") as log:
            writer = csv.writer(output)
            writer.writerow(["received_utc"] + header)
            output.flush()
            print(f"Guardando en {args.output}. Ctrl+C para terminar.")
            # DTR normally resets a classic Nano. Allow its 5 s startup + retries.
            # No auto-configuration: preserve the probe's existing channel list.
            ready_at = time.monotonic() + 15
            started = False
            while True:
                if not started and time.monotonic() >= ready_at:
                    device.write(b"vr\n")
                    started = True
                raw = device.readline()
                if not raw:
                    continue
                now = datetime.now(timezone.utc).isoformat(timespec="milliseconds")
                line = raw.decode("ascii", errors="replace").rstrip("\r\n")
                log.write(f"{now}\t{line}\n")
                log.flush()
                print(line)
                fields = line.split(",")
                if line.startswith("#") or fields == header:
                    continue
                if len(fields) != 6 or fields[-1] not in {"RECEIVED", "MISSING_SENSOR"}:
                    print("Línea inválida conservada en el log.", file=sys.stderr)
                    continue
                writer.writerow([now] + fields)
                output.flush()
    except KeyboardInterrupt:
        print("\nRegistro terminado; archivos guardados.")
    except (OSError, serial.SerialException) as exc:
        parser.exit(1, f"No se pudo registrar: {exc}\n")


if __name__ == "__main__":
    main()
