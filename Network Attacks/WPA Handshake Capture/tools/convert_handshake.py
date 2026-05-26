#!/usr/bin/env python3
"""
Convert ESP32 WPA Handshake Capture binary file to hashcat 22000 format.
Usage: python convert_handshake.py /path/to/handshake.bin
"""

import sys
import struct

def mac_to_hex(mac_bytes):
    return mac_bytes.hex().upper()

def read_handshake(filename):
    with open(filename, 'rb') as f:
        magic = f.read(4)
        if magic != b'WPA1':
            raise ValueError("Invalid handshake file magic")

        bssid = f.read(6)
        stamac = f.read(6)
        ssid_len = struct.unpack('B', f.read(1))[0]
        ssid = f.read(32)[:ssid_len]
        anonce = f.read(32)
        snonce = f.read(32)
        mic_m2 = f.read(16)
        mic_m3 = f.read(16)

        m1_len = struct.unpack('<H', f.read(2))[0]
        m2_len = struct.unpack('<H', f.read(2))[0]
        m3_len = struct.unpack('<H', f.read(2))[0]
        m4_len = struct.unpack('<H', f.read(2))[0]

        m1 = f.read(m1_len) if m1_len else b''
        m2 = f.read(m2_len) if m2_len else b''
        m3 = f.read(m3_len) if m3_len else b''
        m4 = f.read(m4_len) if m4_len else b''

        return {
            'bssid': bssid,
            'stamac': stamac,
            'ssid': ssid,
            'anonce': anonce,
            'snonce': snonce,
            'mic_m2': mic_m2,
            'mic_m3': mic_m3,
            'm1': m1,
            'm2': m2,
            'm3': m3,
            'm4': m4,
        }

def to_hashcat_22000(hs):
    """
    hashcat mode 22000 format:
    For M1+M2 pair: WPA*01*000...*apmac*stamac*ssid*anonce*eapol_m1*eapol_m1_len
    For M2: WPA*02*mic_m2*apmac*stamac*ssid*anonce*eapol_m2*eapol_m2_len
    """
    bssid_hex = mac_to_hex(hs['bssid'])
    stamac_hex = mac_to_hex(hs['stamac'])
    ssid_hex = hs['ssid'].hex().upper()
    anonce_hex = hs['anonce'].hex().upper()

    lines = []

    if hs['m1']:
        m1_hex = hs['m1'].hex().upper()
        m1_len_hex = format(len(hs['m1']), '04x').upper()
        line = f"WPA*01*{'0'*32}*{bssid_hex}*{stamac_hex}*{ssid_hex}*{anonce_hex}*{m1_hex}*{m1_len_hex}"
        lines.append(line)

    if hs['m2']:
        mic = hs['mic_m2'].hex().upper() if len(hs['mic_m2']) == 16 else '0' * 32
        m2_hex = hs['m2'].hex().upper()
        m2_len_hex = format(len(hs['m2']), '04x').upper()
        line = f"WPA*02*{mic}*{bssid_hex}*{stamac_hex}*{ssid_hex}*{anonce_hex}*{m2_hex}*{m2_len_hex}"
        lines.append(line)

    if hs['m3']:
        mic = hs['mic_m3'].hex().upper() if len(hs['mic_m3']) == 16 else '0' * 32
        m3_hex = hs['m3'].hex().upper()
        m3_len_hex = format(len(hs['m3']), '04x').upper()
        line = f"WPA*01*{mic}*{bssid_hex}*{stamac_hex}*{ssid_hex}*{anonce_hex}*{m3_hex}*{m3_len_hex}"
        lines.append(line)

    return '\n'.join(lines)

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <handshake.bin>")
        sys.exit(1)

    filename = sys.argv[1]
    hs = read_handshake(filename)

    print(f"[+] BSSID: {mac_to_hex(hs['bssid'])}")
    print(f"[+] STA:   {mac_to_hex(hs['stamac'])}")
    print(f"[+] SSID:  {hs['ssid'].decode('utf-8', errors='ignore')}")
    print(f"[+] M1: {len(hs['m1'])} bytes")
    print(f"[+] M2: {len(hs['m2'])} bytes")
    print(f"[+] M3: {len(hs['m3'])} bytes")
    print(f"[+] M4: {len(hs['m4'])} bytes")

    hashcat_lines = to_hashcat_22000(hs)
    outname = filename + ".22000"
    with open(outname, 'w') as f:
        f.write(hashcat_lines)
        f.write('\n')

    print(f"\n[+] Saved hashcat 22000 format to: {outname}")
    print("\n--- hashcat usage ---")
    print(f"hashcat -m 22000 {outname} wordlist.txt")

if __name__ == '__main__':
    main()
