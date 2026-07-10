#!/usr/bin/env python3
"""Boot OpenVMS end-to-end: SRM boot, answer the date prompt, wait for
the Username: login prompt.

Usage: bootvms2.py <boot-device> <timeout-seconds> <logfile>
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
sent_boot = False
sent_date = False
deadline = time.time() + timeout
result = "TIMEOUT"

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
    if not sent_boot and b"P00>>>" in clean:
        time.sleep(1)
        s.sendall(("boot %s\r" % boot_dev).encode())
        sent_boot = True
        print("issued: boot", boot_dev, flush=True)
    if sent_boot and not sent_date and b"Please enter date and time" in clean:
        time.sleep(1)
        s.sendall(b"05-JUL-2026 08:25\r")
        sent_date = True
        print("answered date prompt", flush=True)
    if b"Username:" in clean or b"username: " in clean:
        result = "SUCCESS: login prompt reached"
        break
    tail = clean[-3000:]
    if b"bootstrap failure" in tail or b"halt code" in tail:
        result = "FAILURE marker in console"
        time.sleep(3)
        break

with open(logfile, "wb") as f:
    f.write(buf)
print("RESULT:", result, flush=True)
