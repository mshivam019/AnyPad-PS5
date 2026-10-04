#!/usr/bin/env python3
"""GPL-3.0-or-later. Read Redgear through Linux's joystick API, send at 125 Hz."""
import argparse
import array
import fcntl
import glob
import json
import os
from pathlib import Path
import select
import socket
import struct
import time

PACKET = struct.Struct("<4s16sII6BBB")
EVENT = struct.Struct("IhBB")
BUTTONS = {
    0x130: 0x4000, 0x131: 0x2000, 0x133: 0x8000, 0x134: 0x1000,
    0x136: 0x400, 0x137: 0x800, 0x13A: 1, 0x13B: 8,
    0x13C: 0x10000, 0x13D: 2, 0x13E: 4,
}

def axis_byte(value):
    return max(0, min(255, (value + 32767) * 255 // 65534)) if value else 128

def controller_state(axes, buttons):
    bits = 0
    for code, mask in BUTTONS.items():
        if buttons.get(code):
            bits |= mask
    if axes.get(16, 0) < 0: bits |= 0x80
    if axes.get(16, 0) > 0: bits |= 0x20
    if axes.get(17, 0) < 0: bits |= 0x10
    if axes.get(17, 0) > 0: bits |= 0x40
    l2, r2 = axis_byte(axes.get(2, -32767)), axis_byte(axes.get(5, -32767))
    if l2 > 30: bits |= 0x100
    if r2 > 30: bits |= 0x200
    # Redgear may have no separately reported Guide button.
    if buttons.get(0x13A) and buttons.get(0x13B):
        bits = (bits & ~9) | 0x10000
    # Back + L3 is a touchpad click substitute.
    if buttons.get(0x13A) and buttons.get(0x13D):
        bits = (bits & ~3) | 0x100000
    return bits, *(axis_byte(axes.get(code, 0)) for code in (0, 1, 3, 4)), l2, r2

def packet(token, sequence, state, stop=False):
    return PACKET.pack(b"RGP1", token, sequence & 0xFFFFFFFF, *state, int(stop), 0)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ps5", help="PS5's LAN IPv4 address")
    ap.add_argument("--port", type=int, default=8096)
    ap.add_argument("--device", help="Linux /dev/input/js device; auto-detects Redgear")
    ap.add_argument("--dry-run", action="store_true", help="read real inputs without networking")
    ap.add_argument("--seconds", type=float, default=0, help="stop after N seconds (0: indefinite)")
    ap.add_argument("--stop", action="store_true", help="stop the running console payload")
    args = ap.parse_args()
    if not args.dry_run and not args.ps5:
        ap.error("--ps5 is required unless --dry-run")
    token = bytes.fromhex(json.loads(Path(__file__).with_name("pairing.json").read_text())["token"])
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    if not args.dry_run:
        sock.connect((args.ps5, args.port))
        sock.setblocking(False)
    seq = time.time_ns() & 0xFFFFFFFF
    neutral = (0, 128, 128, 128, 128, 0, 0)
    if args.stop:
        if args.dry_run: ap.error("--stop requires networking")
        # Use a new sender port: console releases its previous sender after 2s.
        for _ in range(375):
            try: sock.send(packet(token, seq, neutral, True))
            except ConnectionRefusedError: break
            seq += 1
            time.sleep(.008)
        sock.close()
        print("Stop request sent.")
        return
    candidates = sorted(p for p in glob.glob("/dev/input/by-id/*Redgear*-joystick")
                        if "-event-joystick" not in p)
    if not args.device and not candidates:
        ap.error("Redgear not found. Connect its receiver, or supply --device.")
    device = args.device or candidates[0]
    fd = os.open(device, os.O_RDONLY | os.O_NONBLOCK)
    axmap, btnmap = array.array("B", [0]*64), array.array("H", [0]*512)
    fcntl.ioctl(fd, 0x80406A32, axmap, True)   # JSIOCGAXMAP
    fcntl.ioctl(fd, 0x84006A34, btnmap, True)  # JSIOCGBTNMAP
    axes, buttons = {}, {}
    start = report_at = last_ack = time.monotonic()
    next_send = start
    count = 0
    ready = None
    print(f"Reading {device}; {'local input check' if args.dry_run else 'sending to '+args.ps5}", flush=True)
    print("Back+Start = PS; Back+L3 = touchpad click; Ctrl+C = stop.", flush=True)
    try:
        while not args.seconds or time.monotonic()-start < args.seconds:
            now = time.monotonic()
            readable, _, _ = select.select([fd], [], [], max(0, next_send-now))
            if readable:
                data = os.read(fd, 4096)
                if not data: raise OSError("Controller disconnected")
                for offset in range(0, len(data), EVENT.size):
                    _, value, kind, index = EVENT.unpack_from(data, offset)
                    if kind & 2: axes[axmap[index]] = value
                    if kind & 1: buttons[btnmap[index]] = value
                    if not kind & 128: count += 1
            now = time.monotonic()
            state = controller_state(axes, buttons)
            if now >= next_send:
                if not args.dry_run:
                    try:
                        sock.send(packet(token, seq, state))
                        for _ in range(32):
                            ack = sock.recv(32)
                            if len(ack) == 8 and ack[:4] == b"RGA1":
                                ready, last_ack = bool(ack[4]), now
                    except BlockingIOError: pass
                    except ConnectionRefusedError:
                        ready = None
                seq += 1
                next_send = now + .008
            if now-report_at >= 1:
                status = "local" if args.dry_run else (
                    "no PS5 reply" if now-last_ack > 2 or ready is None else
                    "virtual pad READY" if ready else "PS5 receiving; virtual pad pending/failed (check log)")
                print(f"events={count} buttons=0x{state[0]:08x} axes={state[1:]} | {status}", flush=True)
                report_at = now
    except KeyboardInterrupt:
        pass
    finally:
        if not args.dry_run:
            for _ in range(3):
                try: sock.send(packet(token, seq, neutral, True))
                except OSError: pass
                seq += 1
                time.sleep(.008)
        os.close(fd)
        sock.close()
        print(f"Stopped. Observed {count} live input events.", flush=True)

if __name__ == "__main__":
    main()
