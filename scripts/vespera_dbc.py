#!/usr/bin/env python3
"""
vespera-dbc  —  VesperaOS xHCI Debug Capability monitor
========================================================

Usage:
    ./vespera-dbc.py                        # auto-detect, default /dev/ttyUSB0
    ./vespera-dbc.py -p /dev/ttyUSB1       # explicit port
    ./vespera-dbc.py -o kernel.log         # also write to file
    ./vespera-dbc.py --no-color            # plain output (for piping)
    ./vespera-dbc.py --no-timestamp        # suppress host-side timestamp column
    ./vespera-dbc.py -q                    # quiet: suppress heartbeat lines
    ./vespera-dbc.py --baud 115200         # custom baud rate

Requires:  pip install pyserial
"""

import argparse
import datetime
import os
import re
import sys
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Error: pyserial not found.  Run:  pip install pyserial")
    sys.exit(1)

RESET   = "\033[0m"
BOLD    = "\033[1m"
DIM     = "\033[2m"

FG_RED     = "\033[91m"
FG_YELLOW  = "\033[93m"
FG_GREEN   = "\033[92m"
FG_BLUE    = "\033[94m"
FG_CYAN    = "\033[96m"
FG_MAGENTA = "\033[95m"
FG_WHITE   = "\033[97m"
FG_GRAY    = "\033[90m"
FG_ORANGE  = "\033[38;5;214m"

# Map the [ TAG ] prefixes that Log:: emits to a colour + optional marker
TAG_STYLES = {
    "INFO":    (FG_BLUE,    ""),
    "OK":      (FG_GREEN,   ""),
    "WARNING": (FG_YELLOW,  " ⚠"),
    "ERROR":   (FG_RED,     " ✖"),
    "DEBUG":   (FG_ORANGE,  ""),
    "LOG":     (FG_GRAY,    ""),
}

# Regex: matches "[ TAG ] rest of line"  or  "[HB SSSS.UUUUUU]"  (heartbeat)
RE_TAG  = re.compile(r"^\[ (\w+) \] (.*)")
RE_HB   = re.compile(r"^\[HB \d+\.\d+\]")
# Timestamp prefix emitted by writeln():  "[SSSS.UUUUUU] "
RE_KERN_TS = re.compile(r"^\[(\d+)\.(\d+)\] (.*)")

def _host_ts() -> str:
    """Returns a short host-side wall-clock timestamp string."""
    return datetime.datetime.now().strftime("%H:%M:%S.%f")[:12]


def colorize(line: str, use_color: bool, quiet: bool, show_host_ts: bool) -> str | None:
    """
    Parse one log line and return a formatted string ready for printing,
    or None if the line should be suppressed (e.g. heartbeat in quiet mode).
    """
    line = line.rstrip("\r\n")
    if not line:
        return None

    # Suppress heartbeats in quiet mode
    if quiet and RE_HB.match(line):
        return None

    host_ts_col = f"{DIM}{_host_ts()}{RESET}  " if show_host_ts else ""

    if not use_color:
        return host_ts_col + line

    # Heartbeat — dim, no tag decoration
    if RE_HB.match(line):
        return f"{host_ts_col}{DIM}{line}{RESET}"

    # Strip optional kernel timestamp prefix from writeln() lines
    kern_ts = ""
    rest    = line
    m = RE_KERN_TS.match(line)
    if m:
        sec, us, rest = m.group(1), m.group(2), m.group(3)
        kern_ts = f"{DIM}[{sec}.{us}]{RESET} "

    # Tag-coloured prefix
    m = RE_TAG.match(rest)
    if m:
        tag, msg = m.group(1), m.group(2)
        style, marker = TAG_STYLES.get(tag, (FG_WHITE, ""))
        tag_str  = f"{FG_WHITE}[ {RESET}{BOLD}{style}{tag}{RESET}{FG_WHITE} ]{RESET}"
        msg_col  = FG_RED if tag == "ERROR" else (FG_YELLOW if tag == "WARNING" else FG_WHITE)
        return f"{host_ts_col}{kern_ts}{tag_str} {msg_col}{msg}{marker}{RESET}"

    # Plain line (e.g. from write() without a tag)
    return f"{host_ts_col}{kern_ts}{FG_WHITE}{rest}{RESET}"


def find_dbc_port() -> str | None:
    """
    Try to auto-detect the DbC tty.  Prefers /dev/ttyUSB* ports that look
    like xHCI DbC devices; falls back to the first USB-serial port found.
    """
    candidates = list(serial.tools.list_ports.comports())
    usb_ports  = [p for p in candidates if "USB" in (p.hwid or "").upper()
                  or "ttyUSB" in p.device]
    if usb_ports:
        return usb_ports[0].device
    return None


def open_port(port: str, baud: int) -> serial.Serial | None:
    """Open the serial port; return None on failure."""
    try:
        s = serial.Serial(port, baudrate=baud, timeout=0.1)
        return s
    except (serial.SerialException, OSError):
        return None

def _status(msg: str, use_color: bool) -> None:
    prefix = f"{FG_CYAN}[vespera-dbc]{RESET} " if use_color else "[vespera-dbc] "
    print(f"\r{prefix}{msg}", flush=True)


BANNER = r"""
  __   _____  ___ _ __  ___ _ __ __ _
  \ \ / / _ \/ __| '_ \/ _ \ '__/ _` |
   \ V /  __/\__ \ |_) |  __/ | | (_| |
    \_/ \___||___/ .__/ \___|_|  \__,_|  DbC monitor
                 |_|
"""

def print_banner(use_color: bool) -> None:
    if use_color:
        print(f"{FG_MAGENTA}{BOLD}{BANNER}{RESET}")
    else:
        print(BANNER)


def monitor(port: str, baud: int, logfile, use_color: bool, quiet: bool, show_host_ts: bool,
            retry_interval: float) -> None:
    """
    Outer reconnect loop.  Opens the port, reads lines, handles disconnect,
    retries until the user hits Ctrl-C.
    """
    buf      = b""
    ser      = None
    first    = True

    while True:
        while ser is None or not ser.is_open:
            if not first:
                time.sleep(retry_interval)
            first = False

            # Auto-detect if port is the sentinel
            target = port
            if target == "auto":
                target = find_dbc_port()
                if target is None:
                    _status(f"no USB-serial port found — retrying in {retry_interval:.0f}s …",
                            use_color)
                    time.sleep(retry_interval)
                    continue

            _status(f"connecting to {target} @ {baud} baud …", use_color)
            ser = open_port(target, baud)
            if ser is None:
                _status(f"{target} not available — retrying in {retry_interval:.0f}s …",
                        use_color)
                ser = None
                continue

            _status(f"connected to {target}  (Ctrl-C to quit)", use_color)
            buf = b""
            # Print a visible separator on reconnect
            sep = "─" * 72
            line_out = f"{FG_CYAN}{sep}{RESET}" if use_color else sep
            print(line_out)
            if logfile:
                logfile.write(sep + "\n")
                logfile.flush()

        try:
            chunk = ser.read(256)
        except (serial.SerialException, OSError) as exc:
            _status(f"disconnected ({exc})", use_color)
            ser.close()
            ser = None
            continue

        if not chunk:
            continue

        buf += chunk

        # Process all complete lines in the buffer
        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            try:
                text = raw.decode("utf-8", errors="replace")
            except Exception:
                text = repr(raw)

            rendered = colorize(text, use_color, quiet, show_host_ts)
            if rendered is not None:
                print(rendered)
                if logfile:
                    # Strip ANSI for the log file
                    clean = re.sub(r"\033\[[0-9;]*m", "", rendered)
                    logfile.write(clean + "\n")
                    logfile.flush()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="VesperaOS xHCI DbC serial monitor with auto-reconnect",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "-p", "--port",
        default="auto",
        help="Serial port (default: auto-detect first ttyUSB*)",
    )
    parser.add_argument(
        "-b", "--baud",
        type=int,
        default=9600,
        help="Baud rate (default: 9600; xHCI DbC is USB, so this is advisory)",
    )
    parser.add_argument(
        "-o", "--output",
        metavar="FILE",
        help="Also write output to FILE (ANSI codes stripped)",
    )
    parser.add_argument(
        "--no-color",
        action="store_true",
        help="Disable ANSI colour output",
    )
    parser.add_argument(
        "--no-timestamp",
        action="store_true",
        help="Suppress host-side wall-clock timestamp column",
    )
    parser.add_argument(
        "-q", "--quiet",
        action="store_true",
        help="Suppress [HB ...] heartbeat lines",
    )
    parser.add_argument(
        "-r", "--retry",
        type=float,
        default=2.0,
        metavar="SECS",
        help="Retry interval in seconds when port is unavailable (default: 2)",
    )

    args = parser.parse_args()

    use_color  = not args.no_color and sys.stdout.isatty()
    show_ts    = not args.no_timestamp
    logfile    = None

    if args.output:
        try:
            logfile = open(args.output, "a", encoding="utf-8")
        except OSError as e:
            print(f"Cannot open log file: {e}")
            sys.exit(1)

    print_banner(use_color)

    try:
        monitor(
            port           = args.port,
            baud           = args.baud,
            logfile        = logfile,
            use_color      = use_color,
            quiet          = args.quiet,
            show_host_ts   = show_ts,
            retry_interval = args.retry,
        )
    except KeyboardInterrupt:
        print()
        _status("exiting.", use_color)
    finally:
        if logfile:
            logfile.close()


if __name__ == "__main__":
    main()