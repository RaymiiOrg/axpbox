---
name: verify-vga-sdl
description: Visually verify the S3/VGA/SDL render pipeline (framebuffer dumps, SRM-on-VGA text check). Use after graphics, GUI, or S3 changes, or when a guest's VGA output needs inspection on a headless/WSLg host.
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
# later:
convert fb-055-720x400.ppm frame.png     # ImageMagick; then view it
```

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

- Debian's SDL3 has **no X11 backend** ("x11 not available") — windows
  are Wayland-only. X tools (`import`, `xwininfo`) cannot see or
  capture them, and no Wayland grabber works on WSLg's compositor.
  Hence the framebuffer-dump hook.
- SDL window creation itself can be smoke-tested with a 10-line SDL3
  program (SDL_InitSubSystem(SDL_INIT_VIDEO) + SDL_CreateWindow) —
  prints `video driver: wayland` on WSLg.
- The X11 GUI module (`gui = X11`) is an alternative when a real X
  display exists; it segfaults if the X server is unreachable, so
  check `xwininfo -root` connects first.

## Keyboard injection (headless interaction)

`AXPBOX_AUTOKEY_ENTER=<seconds>` presses Enter on a period through the
emulated keyboard controller (hook in `bx_sdl_gui_c::handle_events`).
Mind the timing: keystrokes during SRM's nvram script abort the script
(see the test-arc skill).
