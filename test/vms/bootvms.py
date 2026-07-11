#!/usr/bin/env python3
"""Drive the SRM console over telnet: wait for P00>>>, issue a boot
command, and capture the console until an OpenVMS marker or timeout.

Usage: bootvms.py <boot-device> <timeout-seconds> <logfile>
"""
import socket
import sys
import time

boot_dev = sys.argv[1]
timeout = int(sys.argv[2])
logfile = sys.argv[3]

s = socket.create_connection(("127.0.0.1", 21000), timeout=15)
s.settimeout(1.0)
buf = b""
booted = False
deadline = time.time() + timeout
result = "TIMEOUT"

MARKERS = [
    b"OpenVMS (TM) Alpha Operating System",
    b"Username:",
    b"%SYSINIT",
    b"The OpenVMS system is now executing",
]
FAIL_MARKERS = [
    b"halted CPU",
    b"halt code",
    b"bootstrap failure",
    b"failed to open",
    b"inaccessible",
]

while time.time() < deadline:
    try:
        d = s.recv(8192)
        if d:
            buf += d
    except socket.timeout:
        pass
    except OSError:
        result = "CONNECTION LOST"
        break
    clean = buf.replace(b"\x00", b"")
    if not booted and b"P00>>>" in clean:
        time.sleep(1)
        s.sendall(("boot %s\r" % boot_dev).encode())
        booted = True
        print("issued: boot", boot_dev, flush=True)
    if booted:
        hit = next((m for m in MARKERS if m in clean), None)
        if hit:
            result = "SUCCESS: " + hit.decode()
            # keep capturing a little longer for context
            t_end = time.time() + 20
            while time.time() < t_end:
                try:
                    d = s.recv(8192)
                    if d:
                        buf += d
                except (socket.timeout, OSError):
                    pass
            break
        fhit = next((m for m in FAIL_MARKERS if m in clean[-4000:]), None)
        if fhit:
            result = "FAILURE MARKER: " + fhit.decode()
            time.sleep(3)
            break

with open(logfile, "wb") as f:
    f.write(buf)
print("RESULT:", result, flush=True)
