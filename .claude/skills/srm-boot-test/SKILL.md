---
name: srm-boot-test
description: Run the headless SRM firmware boot test (boots to P00>>> over telnet and diffs the console log). Use after any change to CPU, chipset, serial, or firmware code, and always once per verified stage of larger work.
---

# SRM boot test

The canonical regression test: boots the SRM console firmware with the
no-VGA config and checks that `P00>>>` appears on the serial console
(telnet port 21000), then byte-compares the console log against
`test/rom/axp_correct.log`.

## Run it

```bash
cd test/rom && timeout 170 bash test.sh; echo "exit=$?"
```

- Exit 0 + "diff clean" = pass. The script downloads `cl67srmrom.exe`
  (http://raymii.org/s/inc/downloads/es40-srmon/cl67srmrom.exe) itself.
- A healthy boot reaches `P00>>>` in ~20–40 s; the 170 s timeout is
  generous. The script's own 600 s loop is too slow for iterating.
- For flake detection run it 3–6 times in a loop; the boot uses
  wall-clock-driven interrupts, so races are timing-dependent.

## Pitfalls (all hit in practice)

- **test.sh leaks the emulator on its timeout path** — it only kills
  the axpbox/netcat pids on success. Always `pkill -9 -x axpbox`
  before a run. Use `-x` (exact process name), NEVER `pkill -f`: an
  `-f` pattern that appears in your own shell command line kills your
  shell mid-script (exit code 144, empty logs).
- **Stale ROM cache**: `decompressed.rom` saved by a *crashed* run is
  garbage and makes the next boot fail in confusing ways. Delete
  `decompressed.rom flash.rom dpr.rom` when a previous run crashed.
- **Port 21000 conflict**: a leftover instance holds the port; the new
  instance's serial silently gets no connection.
- **The log comparison filters the "CPU n speed is" line** (test.sh
  `normalize()`): SRM measures CPU speed from the wall-clock-pinned
  RPCC, so the value is host-dependent (e.g. 357 MHz, not the
  configured 800).
- **axp_correct.log policy**: `P00>>>` reachability is the real gate.
  Only refresh the reference log after eyeballing the diff (strip NULs
  first: `sed 's/\x00//g'`), e.g. when a port intentionally changes
  banner output.

## Manual boot (when you need to interact or capture)

```bash
cd test/rom && pkill -9 -x axpbox; sleep 1
../../build/axpbox run > run.log 2>&1 &
sleep 6
nc -t 127.0.0.1 21000 > axp.log &   # serial console capture
# poll: LC_ALL=C sed -n '$p' axp.log | sed 's/\x00//g'  == "P00>>>"
```

The emulator blocks in serial init until a telnet client connects —
always attach `nc` a few seconds after starting.

## Debugging a boot hang

- gdb can only be attached as the parent (`ptrace_scope=1`):
  `gdb -batch -ex run -ex 'thread apply all bt 14' --args ../../build/axpbox run`
  — on SIGSEGV the backtraces print automatically.
- A guest-side hang with the CPU thread spinning usually means the
  guest runs garbage: check for a runaway PC (see the pc/pc_phys
  desync class of bugs — anything that writes state.pc directly must
  reset the fetch cursor via set_pc()/break_seq_icache()).
- The stage-6 debugging playbook that found the upstream bugs:
  (1) sample guest PC over time, (2) ring-buffer of (pc, ins, pc_phys)
  dumped at first bad PC, (3) differential control-flow trace between
  a known-good and broken binary (log "frompc>topc" per jump, diff).
