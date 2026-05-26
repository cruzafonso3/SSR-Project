#!/usr/bin/env python3
"""
capture_handshake.py - Capture handshake binary from ESP32.

Usage:
    python3 capture_handshake.py <serial_port>

Connects to ESP32, dumps the handshake and saves it to files/handshake.bin.

Tries 'dump_raw' first (handshake in memory), then falls back to
'load' + 'dump_raw' (handshake saved to SPIFFS).

Capture a handshake on the ESP32 first, then run this.
"""

import sys
import serial
import os
import time

START_MARKER = b"---HEX START---\r\n"
END_MARKER = b"---HEX END---\r\n"


def wait_for_dump(ser, timeout=15):
    buf = b""
    t0 = time.time()
    while time.time() - t0 < timeout:
        chunk = ser.read(1024)
        if not chunk:
            continue
        buf += chunk

        idx = buf.find(START_MARKER)
        if idx < 0:
            continue

        rest = buf[idx + len(START_MARKER):]
        end = rest.find(END_MARKER)
        if end < 0:
            continue

        hex_data = rest[:end]
        raw = b""
        for line in hex_data.split(b"\r\n"):
            line = line.strip()
            if line:
                raw += bytes.fromhex(line.decode("ascii"))
        return raw, buf

    return None, buf


def capture(port, out_path):
    ser = serial.Serial(port, 115200, timeout=2)
    time.sleep(2)
    ser.reset_input_buffer()

    print("[*] Trying 'dump_raw' (in-memory handshake)...")
    ser.write(b"dump_raw\n")
    raw, buf = wait_for_dump(ser)
    if raw:
        ser.close()
        return raw

    print("[*] No handshake in memory. Trying 'load' from SPIFFS...")
    ser.write(b"load\n")
    time.sleep(0.5)
    ser.reset_input_buffer()
    ser.write(b"dump_raw\n")
    raw, buf = wait_for_dump(ser)
    if raw:
        ser.close()
        return raw

    ser.close()
    print("[-] ESP32 response:")
    print(buf.decode(errors="replace"))
    return None


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <serial_port>")
        print(f"  e.g. {sys.argv[0]} /dev/ttyUSB0")
        sys.exit(1)

    port = sys.argv[1]

    script_dir = os.path.dirname(os.path.abspath(__file__))
    files_dir = os.path.join(os.path.dirname(script_dir), "files")
    os.makedirs(files_dir, exist_ok=True)
    out_path = os.path.join(files_dir, "handshake.bin")

    for f in os.listdir(files_dir):
        if f.startswith("handshake"):
            os.remove(os.path.join(files_dir, f))

    print(f"[*] Opening {port} at 115200 baud...")

    raw = capture(port, out_path)
    if not raw:
        print("[-] Could not retrieve handshake.")
        print("[-] Capture a handshake on the ESP32 first:")
        print("       set_ssid <name>")
        print("       set_channel <n>")
        print("       start")
        print("       deauth_start")
        print("       (wait for '[HS] *** COMPLETE HANDSHAKE ***')")
        print("       deauth_stop")
        print("       save")
        sys.exit(1)

    with open(out_path, "wb") as f:
        f.write(raw)
    print(f"[+] Saved {len(raw)} bytes to {out_path}")
    print(f"\nNext steps:")
    print(f"  python3 tools/convert_handshake.py {out_path}")
    print(f"  python3 tools/crack_handshake.py")


if __name__ == "__main__":
    main()
