# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

AXPbox emulates an HP/DEC AlphaServer ES40 (Alpha EV68 CPU + Tsunami/Typhoon
chipset) well enough to boot OpenVMS, Tru64, NetBSD, and Windows NT/2000.
It is a modernized fork of the es40 emulator and tracks the upstream
[ES40-Emu/es40](https://github.com/ES40-Emu/es40) revival, whose changes are
periodically ported in (see the `port-from-es40` skill; local upstream clone
at `../es40`).

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
```

Three lanes must always compile (see the `build-lanes` skill):

- `build` — interpreter, SDL3 GUI (default). SDL3 comes from the system
  when available, else built statically from the `third_party/SDL`
  submodule (`git submodule update --init`)
- `build-jit` — `-DES40_DISABLE_ASMJIT=OFF`; needs asmjit cloned at pin
  `0bd5787b54b575ed94bf32ac452153b34385c514` into `third_party/asmjit`
  (gitignored plain clone)
- `build-nosdl` — `-DDISABLE_SDL=yes`; headless/CI

Single executable `axpbox` with subcommands: `axpbox run` (main_sim in
AlphaSim.cpp) and `axpbox configure` (main_cfg in es40-cfg.cpp). Sources are
collected by `file(GLOB ...)` — re-run the cmake configure step after adding
files. C++17. Debug builds of the JIT: add
`-DCMAKE_CXX_FLAGS="-DJIT_VERIFY"` (differential check of every compiled
block against the interpreter; expect 0 mismatches) or `-DJIT_STATS`;
see `src/config_debug.hpp` for all debug flags.

## Test

```bash
cd test/rom && bash test.sh        # SRM firmware boot to P00>>> + console-log diff; expect "diff clean"
```

Pitfalls: `test.sh` deletes the tracked ROM files at the end — restore with
`git checkout -- test/rom/` before committing. `test/rom/axp_correct.log`
contains NUL bytes (grep needs `-a`; edit binary-safe). Kill stray emulators
with `pkill -x axpbox` only (never `pkill -f`).

Deeper verification (each has a skill with the full recipe): `boot-openvms`
(full guest boot from `../run-axpbox` media — always copy disk images before
booting them), `test-arc` (AlphaBIOS/ARC console via flash + S3),
`verify-vga-sdl` (framebuffer inspection + input debugging), `srm-boot-test`.
For headless driving of the emulator (fb dumps, key/mouse injection,
`SDL_VIDEO_DRIVER=offscreen`), the `AXPBOX_*` env hooks are documented in
README "Headless testing".

Formatting: `clang-format-14 -i --style=file <changed files>` (repo LLVM
style; the binary on this host is `clang-format-14`, not `clang-format`).

## Architecture

Everything hangs off `CSystem` (System.cpp), which owns physical memory and
the Tsunami chipset model (Cchip/Dchip/Pchip: memory routing, PCI windows,
interrupts via `cSystem->interrupt()`). Devices derive from
`CSystemComponent` (base class in SystemComponent.cpp), register memory
ranges with the system, and implement `ReadMem`/`WriteMem`, optional
`init()`/`start_threads()`/`stop_threads()`/`check_state()`, and
`SaveState`/`RestoreState`. PCI devices derive from `CPCIDevice`
(config space, BARs); disk controllers from `CDiskController` with `CDisk`
children (`CDiskFile`/`CDiskDevice`/`CDiskRam`, BIN/CUE support in
DiskFileBinCue.hpp).

Major devices: `CAliM1543C` (ISA bridge: PIT/RTC-TOY/PIC/DMA + SuperIO) with
separate `_ide`/`_usb`/`_pmu` PCI functions, `CSerial` (telnet or
null_attach UARTs), `CKeyboard` (KBC + PS/2 aux mouse, Bochs-derived),
`CDEC21143` (NIC via pcap), `CSym53C810/895` (SCSI), `CS3Trio64` + `CVGA` +
MAME-derived rendering into the `bx_gui` plugin layer (`src/gui/`, SDL3 is
the maintained backend), `CFlash`+`CDPR` (firmware NVRAM).

CPU: `CAlphaCPU` (AlphaCPU.cpp) is a per-instruction interpreter
(`execute()`, opcode implementations in `cpu_*.hpp` headers included into
it); `AlphaCPU_vmspal.cpp` is a native fast-path reimplementation of OpenVMS
PALcode entry points; `AlphaCPU_ieeefloat/vaxfloat` implement FP.
`state` struct = the whole architectural state (savefile format). Guest
timing is wall-clock based: `state.cc` (RPCC) advances by real elapsed time,
CPU 0 fires the Cchip interval timer at dispatch-batch boundaries, and the
8254 PIT/TOY in AliM1543C are wall-clock paced (see AlphaCPU.hpp comments).

JIT (`src/jit/jitengine.cpp`, `ES40_JIT` builds only): translates Alpha
basic blocks to x86-64 via asmjit, direct-mapped block cache keyed by
physical PC, poly-link direct chaining between blocks, register pinning;
bails to the interpreter for anything hairy. The trace tier (`JIT_TRACES`)
is deliberately dormant. `jit_run()` in AlphaCPU.cpp is the dispatch loop;
memory access goes through `jit_read/jit_write` helpers that mirror
`cpu_memory.hpp` semantics.

Configuration: `CConfigurator` (Configurator.cpp) parses `es40.cfg` into a
tree and instantiates the device graph; each device class has an allow-list
(`kv_*[]`) of the config values it reads — unknown values warn at startup
(`%SYS-W-UNKNOWNCFG`), so add new config keys to the matching list. The
sample `es40.cfg` at the repo root documents every value.

Threading: each active device runs a `std::thread` (`myThread`, lambda
calling `run()`, `std::atomic_bool myThreadDead` checked by
`check_state()`). `src/base/` contains inherited Poco-style wrappers
(CMutex, CSemaphore, ...) still used by old code — do NOT use them in new
or newly ported code; use `std::mutex`/`std::thread`/`std::chrono`
equivalents (hard project rule).

## Project rules and settled decisions

- Headers are `.hpp`; upstream es40 patches never apply textually (see the
  `port-from-es40` skill for the porting recipe and the list of
  axpbox-specific code to preserve).
- icache is hardcoded ON, the mouse is always present/captured, and the
  first interval-timer tick fires immediately — these mirror upstream
  0.75.1 and were deliberate; don't reintroduce the config options.
- User-facing text says "AXPbox" (banner in `src/banner.hpp` with the
  author-era credits); guest-visible identifiers deliberately keep their
  ES40 names (`ES40EM00000` disk serial, `ES40RAMDISK`, MAC seed "ES40",
  the `es40.cfg` filename).
- AXPbox versions itself via `project(AXPBox VERSION x.y.z)` in
  CMakeLists.txt — never adopt upstream's version number.
- In `src/gui/sdl.cpp`, keep the focus-bounce re-grab logic (WSLg
  compositors bounce focus when the mouse grab engages) and the `AXPBOX_*`
  debug hooks when merging upstream GUI changes.
