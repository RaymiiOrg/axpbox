---
name: test-arc
description: Boot the ARC/AlphaBIOS console from a pre-programmed flash image with the S3 Trio64 on the SDL window. Use to verify the ARC/Windows-NT path (flash boot, nvram auto-arc script, S3 graphics).
---

# ARC / AlphaBIOS test

ARC (AlphaBIOS) is required for Windows NT-family guests. It lives in
the 2 MB system flash and displays on the VGA (S3) — there is no
serial console once ARC takes over.

## Ingredients (all fetchable)

```bash
mkdir -p test/arc && cd test/arc
# Pre-programmed flash: SRM + AlphaBIOS + nvram auto-arc script
curl -sL -o flash-programmed-auto-arc.zip \
  'https://archive.org/download/es40-flasher/flash-programmed-auto-arc.zip'
unzip -o flash-programmed-auto-arc.zip          # -> flash.rom
# Real S3 Trio64 VGA BIOS (required by the s3 device)
curl -sL -o 86c764x1.bin \
  'https://github.com/86Box/roms/raw/master/video/s3/86c764x1.bin'
wget -q -O cl67srmrom.exe 'http://raymii.org/s/inc/downloads/es40-srmon/cl67srmrom.exe'
```

(The same archive.org item also has `flash-programmed.zip` without the
auto-arc nvram script, and `es40-flasher.7z` with the v7.3 firmware
update CD for flashing manually via the LFU — SRM `boot dqbX`,
manual update, must program the TIG; on LFU exit the TIG SRCR write
triggers a full emulated system reset and reboot from flash.)

Config: `test/arc/es40.cfg` — gui=sdl, `pci0.2 = s3 { rom =
"86c764x1.bin"; }`, `arc_year_compat = true`, the flash.rom above,
serial on 21000.

## Run

```bash
cd test/arc && pkill -9 -x axpbox; sleep 1
rm -f decompressed.rom dpr.rom                 # force fresh flash boot
AXPBOX_DUMP_FB=fb ../../build/axpbox run > arcrun.log 2>&1 &
```

## What happens (verified behavior)

1. LoadROM detects the CPQ boot signature in flash and decompresses
   SRM **from flash** (console message `%SYS-I-READFLASH`).
2. This flash's SRM environment uses the **graphics console**: the
   whole SRM boot renders on the S3/SDL window (verify with the
   framebuffer dumps, see the verify-vga-sdl skill).
3. The nvram auto-script runs `show dev` then `arc`; the serial log
   shows `loading ARC firmware`.
4. ARC clears the screen to **solid blue with a blinking top-left
   cursor** and sits in its first-boot phase (NVRAM data error /
   power-up memory test — the NT guides document "screen goes
   black/blank, hit Enter to continue" and recommend disabling the
   power-up memory test in Advanced CMOS Setup once inside).

## Interactive step

Getting past the blue screen needs Enter/F2 **in the SDL window** at
the right moment. For headless runs there is a debug hook:
`AXPBOX_AUTOKEY_ENTER=<seconds>` presses Enter every N seconds via the
emulated keyboard.

**Timing pitfall:** an Enter press while SRM is still running the
nvram script aborts the script (you end at a graphical `P00>>>`
instead of ARC). ARC loads ~35–45 s in; use a period like 75 s, or
press by hand once the screen goes blue.

## Console-side markers

`test/arc/arcwatch.py <seconds> <logfile>` captures the serial side
and reports markers (`P00>>>`, `loading ARC firmware`, ...). Remember
the serial goes quiet once ARC owns the VGA.
