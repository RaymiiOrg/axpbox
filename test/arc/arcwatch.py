#!/usr/bin/env python3
"""Watch the serial console during an ARC/AlphaBIOS boot.

The auto-arc flash's nvram script runs 'arc' from SRM; AlphaBIOS then
takes over the VGA (S3/SDL window). Capture what the serial console
says and report markers.

Usage: arcwatch.py <timeout-seconds> <logfile>
"""
import socket
import sys
import time

timeout = int(sys.argv[1])
logfile = sys.argv[2]

s = socket.create_connection(("127.0.0.1", 21000), timeout=15)
s.settimeout(1.0)
buf = b""
deadline = time.time() + timeout
seen = set()

MARKERS = [b"P00>>>", b"arc", b"AlphaBIOS", b"nvram", b"Initializing"]

while time.time() < deadline:
    try:
        d = s.recv(8192)
        if d:
            buf += d
    except socket.timeout:
        pass
    except OSError:
        print("connection lost", flush=True)
        break
    clean = buf.replace(b"\x00", b"")
    for m in MARKERS:
        if m not in seen and m in clean:
            seen.add(m)
            print("saw:", m.decode(), flush=True)

with open(logfile, "wb") as f:
    f.write(buf)
print("markers seen:", sorted(x.decode() for x in seen), flush=True)
