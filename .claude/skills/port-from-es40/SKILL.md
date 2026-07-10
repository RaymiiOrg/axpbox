---
name: port-from-es40
description: Port upstream ES40-Emu commits into axpbox. Use whenever the user asks to bring in, sync, or port changes from the es40 repo (github ES40-Emu/es40, local clone to /tmp/es40). Covers delta computation, wholesale-replace vs manual patching, the axpbox invariants (std:: threading, .hpp, clang-format, rebrand), and the verification gates.
---


# Porting upstream ES40-Emu changes into axpbox

axpbox (this repo) tracks the upstream ES40-Emu emulator. Upstream
lives in a sibling repo https://github.com/ES40-Emu/es40 (git clone that locally to /tmp/es40/). Ported ranges
so far: everything up to upstream `7d94c9e` (2026-07-05), then
`7d94c9e..7140555` = v0.75.1 (2026-07-09). When asked to port "new
upstream changes", the base of the new range is the tip of the last
ported range — check this file's history section (bottom) and
`git -C /tmp/es40 log --oneline` to find it.

**NEVER `git apply` / `git cherry-pick` upstream patches.** They never
apply: axpbox is clang-formatted, renames headers `.h` → `.hpp`,
rewrote threading to `std::`, merged the two upstream binaries into
one, and moved files. Follow the recipe below instead.

## Step 0 — survey the range

```bash
E=/tmp/es40           # upstream clone
BASE=<last-ported-upstream-commit>     # e.g. 7140555
git -C $E fetch --all                  # only if the user says they updated it
git -C $E log --oneline $BASE..HEAD    # list commits to port
git -C $E diff --stat $BASE..HEAD      # list files touched
```

Group the commits into themes (JIT, timing, disk, net, config, GUI,
build, docs). Plan roughly one axpbox commit per theme. Every commit
must build in all lanes and pass the SRM test.

Files to ALWAYS skip (axpbox does not have them):
`src/Visual Studio/*`, `configure.ac`, `config_vms.h`,
`config_win32.h`, `.github/ISSUE_TEMPLATE/*`. Upstream `CMakeLists.txt`
changes almost never apply — axpbox has its own; read the upstream
hunk and decide if the *idea* (a new source file, a new dependency) is
needed. axpbox versioning is its own (`project(AXPBox VERSION x.y.z)`
in CMakeLists.txt) — NEVER adopt upstream's version number.

## Step 1 — file mapping (upstream path → axpbox path)

- `src/X.cpp` → `src/X.cpp` (same name).
- `src/X.h` → `src/X.hpp` (ALL headers).
- `src/es40.cfg` → repo-root `es40.cfg` (plus per-test copies in
  `test/rom/`, `test/vms/`, `test/nt/`, `test/arc/`).
- `src/es40-cfg.cpp` → `src/es40-cfg.cpp`, but its `main()` is named
  `main_cfg()` (single `axpbox` binary; `src/Main.cpp` dispatches
  `axpbox run` → `main_sim`, `axpbox configure` → `main_cfg`).
- New upstream headers: create as `.hpp`, rename the include guard
  (`__X_H__` → `__X_HPP__`), give it the axpbox license header (see
  Step 4), and add nothing to CMake — `file(GLOB ...)` picks up
  sources automatically (re-run the cmake *configure* step after
  adding a new `.cpp`).

## Step 2 — choose a strategy per file

Compute two diffs for each file F in the range:

```bash
S=<scratch dir>
# (a) what upstream changed:
git -C $E diff $BASE..HEAD -- src/F        # use: git --no-pager diff --no-ext-diff
# (b) what axpbox changed relative to the last ported upstream state:
git -C $E show $BASE:src/F > $S/F.base
diff -u $S/F.base <(git show <preformat>:src/F_axpbox_path)
```

For (b) you need the axpbox snapshot from BEFORE the repo-wide
clang-format, or the diff drowns in formatting noise. For the original
port that snapshot is `3e35a80~1`. If the file was clang-formatted
since, fall back to `diff -w -B` against the current file and read
carefully. ALSO check for axpbox commits made after that snapshot:
`git log --oneline <snapshot>..HEAD -- src/F` — their changes are part
of the axpbox delta too.

Pick the strategy:

- **Manual edit (M)** — when (a) is small (< ~100 changed lines).
  Apply upstream's hunks by hand with the Edit tool onto the current
  axpbox file, translating style (see Step 3).
- **Wholesale replace (W)** — when (a) is large (hundreds+ of lines,
  many commits of churn) AND (b) is small/enumerable. Recipe:
  1. `git -C $E show HEAD:src/F > src/F` (into axpbox).
  2. `sed`-rename every `#include "X.h"` to `"X.hpp"`. Verify none
     remain: `grep -n '\.h"' src/F | grep -v '\.hpp"'`.
  3. Re-apply the axpbox delta. Fastest reliable way: save diff (b) to
     a file and run `patch --no-backup-if-mismatch -F3 src/F <
     delta.diff`, then open every `.rej` and hand-apply what failed.
     Read the result around every merge point.
  4. Deliberately DROP delta hunks that upstream has now superseded
     (e.g. a config option upstream removed) — list these in the
     commit message.
  5. `clang-format-14 -i --style=file src/F` (clang-format is
     installed as `clang-format-14`, no bare `clang-format`).

## Step 3 — axpbox invariants (apply to every ported line)

1. **No Poco-style wrappers in new code.** Upstream uses `CThread`,
   `CMutex`, `CSemaphore`, `SCOPED_M_LOCK`, `CRunnable` (from
   `src/base/`). New/ported code must use `std::` equivalents:
   - `CThread::sleep(ms)` → `std::this_thread::sleep_for(std::chrono::milliseconds(ms))`
   - `CThread* myThread` → `std::unique_ptr<std::thread> myThread;`
     started as `myThread = std::make_unique<std::thread>([this]() { this->run(); });`
     joined as `myThread->join(); myThread = nullptr;`
   - thread-death flag: `std::atomic_bool myThreadDead{false};`
     set in the run() catch block, checked in check_state().
   - `CMutex` + `SCOPED_M_LOCK(m)` → `std::recursive_mutex m;` +
     `std::lock_guard<std::recursive_mutex> lock(m);` (upstream CMutex
     IS recursive — keep recursive semantics unless you can prove
     no re-entry). `#include <mutex>` in the header.
   - Pre-existing `CMutex`/`CSemaphore` uses already in axpbox
     (e.g. Sym53C810, the CPU semaphore) may stay — do not churn them.
2. **Preserve the axpbox-only bug fixes** when replacing CPU/JIT code:
   - `execute()` advances the PC with `next_pc()`, never bare
     `state.pc += 4` (keeps `pc_phys`/`rem_ins_in_page` in sync).
   - `break_seq_icache()` in AlphaCPU.hpp also clears
     `state.rem_ins_in_page = 0;`.
   - `vmspal_int_initiate_exception` has the `vmspal_exc_depth >= 3`
     recursion guard (guard in AlphaCPU.cpp, ++/-- in
     AlphaCPU_vmspal.cpp, member in AlphaCPU.hpp).
   - `skip_memtest_hack` config option (AlphaCPU init + gated
     `0x8b000` page check in `execute()`), replaces upstream's
     `#ifdef SKIP_SRM_MEMTEST`.
   - `fread` return-value checks in RestoreState.
   - Debug env hooks (documented in README "Headless testing"):
     `AXPBOX_PC_SAMPLE` (AlphaCPU::check_state), `AXPBOX_DUMP_FB`,
     `AXPBOX_AUTOKEY_ENTER`, `AXPBOX_KEYSCRIPT`, `AXPBOX_KEYPIPE`,
     `AXPBOX_AUTOMOUSE`, `AXPBOX_MOUSE_DEBUG` (gui/sdl.cpp; the aux-cmd
     trace part of MOUSE_DEBUG is in Keyboard.cpp ctrl_to_mouse).
   - SDL grab handling: re-grab on FOCUS_GAINED after a compositor
     focus bounce (WSLg) — do not revert to plain
     "FOCUS_LOST => ungrab" when porting upstream sdl.cpp changes.
   - DiskFile: default-image-filename fallback (`defaultFilename`
     member) instead of upstream's FAILURE when no file is configured.
   - Disk: `SCSICMD_START_STOP_UNIT` case answered with
     `do_scsi_error(SCSI_OK)`.
   - Configurator: the `cirrus` class row exists and fails with a
     helpful "use s3 instead" message.
   - es40-cfg: the `skip_memtest_hack` question.
3. **Settled decisions — do not re-litigate:**
   - icache is hardcoded ON (`icache_enabled = true` in init/reset,
     no config read, no configurator question) — upstream ed9bf49,
     user-confirmed 2026-07-09.
   - The interval timer fires immediately at thread start
     (`next_timer_fire = start_time`) — the old 1-second grace is
     gone; the RTC boot tick (`toy_stored_data[0x0a] = 0x26`) and the
     vmspal guard cover the early-boot transient.
   - mouse is always present/captured (no `mouse.enabled`).
4. **Configurator allow-lists** (`kv_*[]` arrays in
   src/Configurator.cpp): any NEW config key a ported device reads
   must be added to that device's list, and axpbox-only keys must
   never be lost: `skip_memtest_hack` (kv_ev68cb), `timezone`
   (kv_ali), `rom.decompressed` (kv_tsunami), `address` (kv_serial).
   Audit after porting:
   ```bash
   grep -rn 'myCfg->get_\(text\|num\|bool\)_value("' src/*.cpp src/gui/*.cpp \
     | grep -o 'value("[^"]*"' | sort -u
   ```
   then start the emulator once with every shipped config and require
   ZERO `%SYS-W-UNKNOWNCFG` (see Step 5.4).

## Step 4 — rebranding rules

All HOST-side user-visible text says **AXPbox**, never ES40:
window titles (sdl/x11/win32), the startup banner
(`src/banner.hpp` — `print_axpbox_banner`, shows axpbox's own
`VERSION`, credits authors by era: Camiel Vanderhoeven 2007-2010,
Tim Stark/fsword7 2018, Tomas Glozar 2020-2023, Remy van Elst
2020-2026, Gary/gdwnldsKSC 2023-2026), configurator wizard text,
serial telnet greeting, debugger greeting, stat lines.

Do NOT rename GUEST-visible or protocol identifiers: disk serial
default `"ES40EM00000"`, ramdisk model `"ES40RAMDISK"`, the MAC seed
`"ES40"`, the `es40.cfg` file name, `"[]ES40.CFG"`, hardware/SRM
references ("AlphaServer ES40" is the emulated machine — keep), state
file magics, include guards.

License header for files whose axpbox copy already has it, and for
new files:

```
/* AXPbox Alpha Emulator
 * Copyright (C) 2020 Tomáš Glozar
 * Website: https://github.com/lenticularis39/axpbox
 *
 * Forked from: ES40 emulator
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 * Copyright (C) 2007 by Camiel Vanderhoeven
 * ... (GPL v2 boilerplate as in existing files)
 */
```

Some ported files (e.g. AlphaCPU.cpp) still carry the upstream ES40
header — match whatever the file's current axpbox copy has. Strip
upstream's `$Id$` / `X-1.xx` CVS changelog blocks from any file you
wholesale-replace (keep only the `\file` description line).

Text in `banner.hpp` must stay ASCII (`Tomas`, not `Tomáš`) — the
column padding is strlen-based and multi-byte UTF-8 breaks alignment.

## Step 5 — verification gates (run in this order)

1. **Build all lanes** (see the build-lanes skill): `build` (default),
   `build-jit` (`-DES40_DISABLE_ASMJIT=OFF`, asmjit pinned
   `0bd5787b` in `third_party/asmjit`), `build-nosdl`
   (`-DDISABLE_SDL=yes`). All three after every commit.
2. **SRM regression** (see srm-boot-test skill): `cd test/rom && bash
   test.sh` — expect literal `diff clean`. Run against build-jit too
   (copy test.sh and point it at `../../build-jit/axpbox`).
   Pitfalls: `test.sh` DELETES the tracked `cl67srmrom.exe`,
   `flash.rom`, `decompressed.rom` at the end — restore with
   `git checkout -- test/rom/` before committing. `axp_correct.log`
   contains NUL bytes (grep needs `-a`) and embeds the serial telnet
   greeting — if you change that greeting, patch the expected log
   binary-safely (python bytes replace, not sed).
3. **JIT differential check** after ANY jit/ or AlphaCPU change:
   ```bash
   cmake -S . -B /tmp/build-verify -DCMAKE_BUILD_TYPE=Release \
     -DES40_DISABLE_ASMJIT=OFF -DCMAKE_CXX_FLAGS="-DJIT_VERIFY"
   cmake --build /tmp/build-verify -j$(nproc)
   # run the SRM test against it; the log must show
   # "[JIT][VERIFY] ... 0 mismatches" lines and end at P00>>>
   ```
   Also do one throwaway compile with
   `-DCMAKE_CXX_FLAGS="-DJIT_TRACES -DJIT_STATS"` so the dormant trace
   tier doesn't rot.
4. **Config-warning sweep**: for each of `test/rom`, `test/vms`,
   `test/nt`, `test/arc` and a scratch dir holding the root
   `es40.cfg`:
   ```bash
   (cd <dir> && timeout 8 stdbuf -oL <repo>/build/axpbox run > w.log 2>&1; \
    grep -aE "SYS-W" w.log | sort -u)
   ```
   Expected: nothing except `%SYS-W-NOSERIAL` on configs that omit
   serial1. NOTE: output MUST go to a file (`stdbuf` + redirect) —
   piping `timeout`'d output loses everything to stdio buffering.
5. **OpenVMS boot** after CPU/disk/timing changes (see boot-openvms
   skill) — run on both build and build-jit lanes; expect
   `RESULT: SUCCESS: login prompt reached` and OPCOM timestamps
   consistent with the date the script answers.
6. **ARC smoke** after RTC/superio/VGA changes (see test-arc skill).
7. **Configurator smoke**: `printf 'no\n' | build/axpbox configure`
   shows the AXPbox banner and exits cleanly.
8. Kill strays ONLY with `pkill -x axpbox` (never `pkill -f` — it
   matches the invoking shell's own command line).

## Step 6 — commit conventions

One thematic commit per upstream area, message = what was ported +
which upstream commits + which axpbox deltas were preserved/dropped.
Do not sweep in unrelated untracked files (`test/nt/` may hold user
disk images and logs; there may be a user stash `WIP on s3-port` —
leave both alone). Trailer:
`Co-Authored-By: Claude <the current model's attribution line>`.

## Port history

- up to upstream `7d94c9e` — 12 commits on baseline `e81dce5`
  (2026-07-05).
- `7d94c9e..7140555` (v0.74→0.75.1) — 7 commits `29f83b4..377c362`
  (2026-07-09): JIT 0.75.1 + timing bundle, BIN/CUE, DEC21143 lock,
  Sym53C810 disconnect, configurator warnings, SDL mouse options,
  AXPbox rebrand + banner. Next port starts at upstream `7140555`.
- Post-port axpbox-only additions (2026-07-10, keep across future
  ports): input-injection/diagnostic hooks (AXPBOX_KEYPIPE,
  AXPBOX_AUTOMOUSE, AXPBOX_MOUSE_DEBUG + aux-cmd trace in
  Keyboard.cpp), the focus-bounce re-grab in sdl.cpp, README
  "Headless testing" + "Mouse on WSLg" sections.
