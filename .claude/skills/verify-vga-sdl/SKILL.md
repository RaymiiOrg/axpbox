---
name: verify-vga-sdl
description: Visually verify the S3/VGA/SDL render pipeline (framebuffer dumps, SRM-on-VGA text check) and test mouse/keyboard input paths. Use after graphics, GUI, S3, or input changes, or when a guest's VGA output needs inspection on a headless/WSLg host.
---

# VGA / SDL visual verification

## Framebuffer dumps (works everywhere)

The SDL backend has a debug hook: `AXPBOX_DUMP_FB=<prefix>` writes the
frame the SDL renderer actually receives as a PPM every ~2 s, numbered:
`<prefix>-NNN-<w>x<h>.ppm` (in `src/gui/sdl.cpp`
`graphics_frame_update`). This verifies the S3 → SDL pixel pipeline
without any host display tooling.

```bash
AXPBOX_DUMP_FB=fb ./build/axpbox run &
# later: convert with PIL (PIL is installed; ImageMagick may not be)
python3 -c "from PIL import Image; Image.open('fb-055-720x400.ppm').save('frame.png')"
```

Fully window-less operation (no display server needed, nothing pops up
on the user's desktop): add `SDL_VIDEO_DRIVER=offscreen`. Rendering,
fb dumps, and the key-injection hooks all still work.

Deduplicate a long run to find the distinct screens:

```bash
python3 -c "
import hashlib, glob
seen = {}
for f in sorted(glob.glob('fb-*.ppm')):
    h = hashlib.md5(open(f,'rb').read()).hexdigest()[:8]
    seen.setdefault(h, []).append(f)
for h, fs in sorted(seen.items(), key=lambda kv: kv[1][0]):
    print(h, len(fs), fs[0], '->', fs[-1])"
```

Two frames differing only in an 8x2 pixel block at the top-left is a
blinking text-mode cursor on an otherwise static screen.

## Text-rendering sanity check: SRM on the VGA console

Set `vga_console = true;` in the `ali` config section (with an s3
section + gui=sdl) and boot: the SRM console output renders as
white-on-blue 720x400 text ending at `P00>>>`. If that text is
correct, S3 text mode, the font path, and the SDL upload all work —
any blank guest screen is then the guest's doing, not a render bug.

## Host display notes (WSLg)

- SDL3 picks the **Wayland** backend by default. The x11 backend IS
  compiled in (verified 2026-07: drivers = wayland, x11, kmsdrm,
  offscreen, dummy, evdev) but needs the right DISPLAY: the shell's
  `DISPLAY` may point at a TCP X server (`10.255.255.254:0`) that is
  unreachable — WSLg's local XWayland socket is **`:0`**
  (`/tmp/.X11-unix/X0`).
- Under `SDL_VIDEO_DRIVER=x11 DISPLAY=:0` the GPU renderer dies with a
  fatal `X Error ... BadValue` — add `SDL_RENDER_DRIVER=software`.
- A `serial` section with a `port` value blocks startup until a telnet
  client connects — `nc localhost <port> &` first, or configure
  `null_attach = true`.
- The legacy X11 GUI module (`gui = X11`) segfaults if the X server is
  unreachable; check `xwininfo -root` connects first.

## Input injection (headless interaction)

All hooks live in `bx_sdl_gui_c::handle_events` (src/gui/sdl.cpp) and
are documented user-facing in README.md:

- `AXPBOX_AUTOKEY_ENTER=<sec>` — Enter every N seconds. Mind the
  timing: keystrokes during SRM's nvram script abort the script (see
  the test-arc skill).
- `AXPBOX_KEYSCRIPT="40:a,41:r,42:c,43:enter"` — named keys at fixed
  second offsets (timed from GUI start, which is AFTER any serial-port
  wait). Full letter/digit/f-key/navigation name set.
- `AXPBOX_KEYPIPE=<file>` — interactive: `echo "f2 down enter" >> file`
  while running; one token consumed per ~120 ms.
- `AXPBOX_AUTOMOUSE=<sec>` — synthetic PS/2 mouse motion (square + a
  periodic click) injected guest-side from N seconds in. Moves the
  guest cursor with zero host input: separates guest-side mouse
  problems from host event delivery.

## Mouse verification and the WSLg pitfalls (found 2026-07-10)

Trace with `AXPBOX_MOUSE_DEBUG=1` (stderr):
- `MOUSEDBG motion xrel=.. yrel=.. grab=N` — host motion events.
- `MOUSEDBG grab -> 0/1`, `focus lost/gained` — grab lifecycle.
- `MOUSEDBG aux cmd XX (enable=..)` — every command the guest sends to
  the PS/2 aux device (from Keyboard.cpp). `f4` = guest enabled stream
  mode; if it never appears the guest never detected the mouse.

Interpretation:
- Motion lines with `grab=1` → host input reaches the guest path.
- `grab -> 1` immediately followed by `grab -> 0` → compositor focus
  bounce (handled: the GUI re-grabs on focus-gained, bounded to 8).
- `grab -> 1` then **zero** motion lines → the backend delivers no
  relative motion. That is WSLg's Wayland compositor: relative
  pointer motion is silently absent (SDL reports success). Workaround:
  `SDL_VIDEO_DRIVER=x11 DISPLAY=:0 SDL_RENDER_DRIVER=software` — X11
  relative motion works and reaches the guest (verified: hundreds of
  grab=1 motion events).

Guest-side status (as of 2026-07-10): AlphaBIOS 5.71 moves its pointer
from our PS/2 packets but with BOTH axes inverted relative to the
standard PS/2 sign convention our packet builder emits —
`mouse.invert_x = true; mouse.invert_y = true;` in the sdl config
section compensates. Windows 2000, once booted to the desktop, showed
no pointer movement at all in the same session — its serial MCR
toggling (SRL1 DTR/RTS) suggests it was probing for a serial mouse;
use the `aux cmd` trace to check whether W2K's i8042 driver ever sends
`f4`. Unresolved — continue there.
