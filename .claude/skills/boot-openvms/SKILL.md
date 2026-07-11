---
name: boot-openvms
description: Boot OpenVMS from the disk image in test/run-axpbox to verify end-to-end guest operation (SRM boot command, bootstrap, SYSINIT, startup to login). Use for deep verification after CPU/disk/timing changes.
---

# Boot OpenVMS (full-guest verification)

Media lives in `test/run-axpbox/`:
- `openvms8.4-ww-welkom012.img` — 6 GB pre-installed OpenVMS 8.4-2L1
  system disk (boots from IDE as `dqa0`)
- `ALPHA0842L1.ISO` — OpenVMS 8.4 install CD (attach as ATAPI CD,
  shows up as `dqb0`)

This folder is gitignored. If the files do not exist, prompt the user to place them.

**Never boot the user's image directly** — OpenVMS writes to its
system disk. Copy it first:

```bash
mkdir -p test/vms && cd test/vms
cp ../../test/run-axpbox/openvms8.4-ww-welkom012.img vms-disk.img
```

Config: `test/vms/es40.cfg` (memory.bits=28, ali_ide disk0.0 =
vms-disk.img, disk1.0 = the ISO read-only as cdrom, serial on
127.0.0.1:21000, skip_memtest_hack=true for speed).

## Automated run

`test/vms/bootvms2.py` drives the whole flow over telnet: waits for
`P00>>>`, sends `boot dqa0`, answers the VMS date prompt
(`DD-MMM-YYYY HH:MM`), and waits for a login-ish marker.

```bash
cd test/vms && pkill -9 -x axpbox; sleep 1
[ -f cl67srmrom.exe ] || wget -q -O cl67srmrom.exe 'http://raymii.org/s/inc/downloads/es40-srmon/cl67srmrom.exe'
../../build/axpbox run > vmsrun.log 2>&1 &
sleep 6
python3 bootvms2.py dqa0 900 vmscon.log     # ~4 min to full startup
pkill -9 -x axpbox
sed 's/\x00//g' vmscon.log | tail -30
```

## What a good boot looks like

1. `boot dqa0` → "block 0 of dqa0... is a valid boot block" →
   "jumping to bootstrap code"
2. Banner: `OpenVMS (TM) Alpha Operating System, Version V8.4-2L1`
3. `Please enter date and time (DD-MMM-YYYY HH:MM)` — the welkom
   image prompts on every boot (TIMEPROMPTWAIT); the script answers.
4. DECnet, OPCOM, AUDIT startup messages with **correct current
   timestamps** (verifies the TOY/timezone plumbing), license
   warnings (normal for an unlicensed image), "Startup processing
   continuing...".

Boot takes ~3–5 minutes on the interpreter. The whole run fits within
a 900 s script timeout. `bootvms.py` (same dir) is the shorter variant
that stops at the OpenVMS banner.

Cleanup: the 6 GB `vms-disk.img` copy is disposable and gitignored —
delete it when done if disk space matters.
