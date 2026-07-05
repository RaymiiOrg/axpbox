/* ES40 emulator -- JIT engine
 *
 * Per-CPU, direct-mapped cache of translation blocks. The dispatcher runs a
 * block's compiled safe-ALU prefix natively, then interprets the remainder.
 *
 * Blocks are keyed by VIRTUAL PC + ASN (like the icache), so the dispatch hot
 * path needs no address translation. A TB invalidation flushes the cache (see
 * flush()); a global (ASM) block matches any ASN.
 */
#if !defined(INCLUDED_JITENGINE_H)
#define INCLUDED_JITENGINE_H

#ifdef ES40_JIT

#include "../config_debug.hpp" // JIT_VERIFY
#include <cstdint>
#ifdef JIT_STATS
#if defined(_MSC_VER)
#include <intrin.h> // __rdtsc -- host TSC for the JIT_STATS wall-time split
#else
#include <x86intrin.h>
#endif
static inline uint64_t jit_rdtsc() { return __rdtsc(); }
#endif
#if defined(JIT_REGPROF) && !defined(JIT_STATS)
#error                                                                         \
    "JIT_REGPROF needs JIT_STATS (its report rides note_exec's 100M-instruction window)"
#endif
#ifdef JIT_DISASM
#include <cstdio> // FILE* for the per-CPU disassembly trace
#endif

class CAlphaCPU; // compiled blocks call back into the CPU for memory accesses

class CJitEngine {
public:
  // 64K slots: the OS-active CPU's block working set (50K+) thrashed the old
  // 16K direct-mapped cache -- conflict evictions caused ~190K recompiles per
  // 100M instructions on that CPU.
  static constexpr int kCacheBits = 16;
  static constexpr int kCacheEntries = 1 << kCacheBits;
  static constexpr uint64_t kIndexMask = (uint64_t)kCacheEntries - 1;

  // Reclaim executable memory once compiled code passes this many bytes, rather
  // than tearing down the asmjit runtime on every flush (see flush()).
  static constexpr uint64_t kReclaimBytes = 32 * 1024 * 1024;

  // Compiled block entry point. Runs the prefix on regs[0..31], calling back
  // into cpu for memory accesses; returns the number of instructions fully
  // completed
  typedef uint32_t (*JitFn)(CAlphaCPU *cpu, uint64_t *regs);

  struct JitBlock {
    uint64_t tag;     // start VIRTUAL PC (validity tag / key)
    uint64_t phys;    // start physical PC (source bytes for compilation)
    uint32_t asn;     // address space number (key; ignored when asm_global)
    bool asm_global;  // global (ASM) page: matches any ASN, like the icache
    uint32_t n_instr; // instructions in the straight-line block
    bool valid;
    JitFn code; // compiled safe-prefix, or null (prologue entry, for C calls)
    void *jit_body; // chained re-entry point (after the prologue); null when
                    // not compiled
    JitBlock *link; // cached successor block (back-patched by the dispatcher);
                    // null = none
    uint32_t prefix_len; // # safe ALU ops in code
    bool compiled;       // compile has been attempted
    uint32_t body_off; // jit_body's offset within code -- restores the chained
                       // entry on revalidate
    uint64_t src_sum; // hash of the source words at compile time (revalidate vs
                      // self-mod)
    uint32_t hash_len; // word count src_sum covers -- frozen at compile time;
                       // n_instr drifts (interrupt-truncated cold passes shrink
                       // it), so it must NOT key the hash
    uint64_t vgen; // m_itb_gen + m_flush_gen at last full validation (phys +
                   // code bytes). Both counters are monotonic, so one sum
                   // compare detects either changing
                   // -- the single chain guard (see emit_chain / jit_indirect).
    uint64_t flush_gen; // icache-flush generation at which the code bytes were
                        // last hash-validated; stale => lookup misses and
                        // revalidate_flushed() re-hashes (lazy IC_FLUSH)
#ifdef JIT_REGPROF
    uint64_t rp_hits; // REGPROF: block executions since record (body-entry inc
                      // -- counts chained runs)
    uint32_t rp_mask; // REGPROF: Alpha GPRs touched by this block (bit r);
                      // compile-time, exec-weighted at report
    uint32_t rp_csz;  // REGPROF: emitted x86 bytes for this block -- rp_hits x
                      // rp_csz = exec-weighted expansion
#endif
  };

  // Byte offsets (from the CAlphaCPU*) of the fields the inline load fast path
  // reads, so compiled code can touch them via [rsi + offset]. Filled once by
  // set_offsets().
  struct JitOffsets {
    uint32_t dpc_valid, dpc_virt_page, dpc_phys_base, dpc_host_base, dpc_cm,
        dpc_asn; // offsets of READ slot [0][0]
    uint32_t dpc_stride,
        dpc_mask; // direct-mapped page cache: per-slot byte stride, index mask
    uint32_t dpc_write_row; // byte distance from read cache [0] to write cache
                            // [1] (store fast path)
    uint32_t state_cm, state_asn0, dram_ptr, dram_size, state_pc;
    uint32_t fpen, exc_sum, fpcr,
        f_base; // FP inline path: FPSTART gate + FPCR (rounding/INE) + f[] base
                // (f[i] = f_base + i*8)
    // For chaining: the budget ceiling and the interrupt-poll flags the
    // compiled epilogue checks before jumping on; link_from is where the
    // epilogue records a link-patch request.
    uint32_t jit_budget, check_int, check_timers, link_from;
    uint32_t exc_addr, pal_base,
        sde; // CALL_PAL: exc_addr save, PAL entry base, PALshadow enable
  };
  void set_offsets(const JitOffsets &o) { m_off = o; }

  explicit CJitEngine(
      int cpu_id = 0); // cpu_id tags the stats/diagnostic prints
  ~CJitEngine();

  static inline uint64_t index_of(uint64_t virt_pc) {
    return (virt_pc >> 2) & kIndexMask;
  }

  // Virtual+ASN keyed: no translation on the dispatch hot path. A global (ASM)
  // block matches any ASN, mirroring the icache's hit rule. flush_gen-stale
  // blocks miss here; revalidate_flushed() resurrects them after a source-hash
  // check.
  inline JitBlock *lookup(uint64_t virt_pc, uint32_t asn) {
    JitBlock &b = m_blocks[index_of(virt_pc)];
    return (b.valid && b.flush_gen == m_flush_gen && b.tag == virt_pc &&
            (b.asm_global || b.asn == asn))
               ? &b
               : nullptr;
  }

  // Lazy-flush survivor: hash-revalidate the slot in place (no interpreted
  // pass, no re-record).
  JitBlock *revalidate_flushed(uint64_t virt_pc, uint32_t asn, uint64_t phys_pc,
                               const uint8_t *dram);

  JitBlock *record(uint64_t virt_pc, uint64_t phys_pc, uint32_t asn,
                   bool asm_global, uint32_t n_instr, const uint8_t *dram);
  void compile_block(JitBlock *b, const uint8_t *dram, uint64_t dram_size,
                     void *read_helper, void *write_helper, void *opcdec_helper,
                     void *hw_mfpr_helper, void *hw_ld_helper,
                     void *hw_mtpr_helper, void *hw_st_helper,
                     void *indirect_helper, void *read_locked_helper,
                     void *stc_helper, void *misc_helper,
                     void *read_vpte_helper, void *itof_helper,
                     void *ftoi_helper, void *fltl_helper, void *fp_read_helper,
                     void *fp_write_helper, void *fltv_helper);
  void flush();
  void flush_non_global(); // flush only !asm_global blocks (the ASM-bit-clear /
                           // ASN icache flush)
  void reclaim_code();     // free ALL compiled code once past kReclaimBytes
                           // (cold-path only)
  // flush() can be reached from a compiled IC_FLUSH, so it DEFERS the reclaim
  // (sets m_reclaim_pending); the dispatcher calls this at a safe point (no
  // compiled frame live) to actually free the code.
  inline void reclaim_if_pending() {
    if (m_reclaim_pending) {
      m_reclaim_pending = false;
      reclaim_code();
    }
  }

  // ITB-generation counter for the indirect-chain staleness check
  // (jit_indirect). Bumped on every I-stream TB invalidate (tbia/tbiap/tbis,
  // ACCESS_EXEC) ... those can remap a code page WITHOUT flushing the JIT, so a
  // chained block could run stale bytes.
  inline void note_itb_invalidate() { ++m_itb_gen; }
  inline uint64_t vgen() const {
    return m_itb_gen + m_flush_gen;
  } // combined validation epoch

  // Bail-cause counters (JIT_STATS): why a compiled chain returned to the
  // dispatcher -- a branch/ fall-through cached-link miss vs a computed-jump
  // (jit_indirect) miss. Empty when stats are off, so the call sites need no
  // #ifdef.
#ifdef JIT_STATS
  void note_link_bail() { m_bail_link++; }
  void note_jmp_attempt() { m_jmp_attempt++; }
  void note_jmp_hit() { m_jmp_hit++; }
#else
  void note_link_bail() {}
  void note_jmp_attempt() {}
  void note_jmp_hit() {}
#endif

#ifdef JIT_VERIFY
  // Differential check: compiled result (jit) vs interpreter result (interp),
  // r[0..30]. Returns the ns spent in its periodic progress printf (0
  // otherwise) so the dispatcher can exclude that stall from the
  // wall-clock-pinned RPCC (same Heisenberg fix as note_exec).
  uint64_t verify_compare(uint64_t blk_virt, const uint64_t *interp,
                          const uint64_t *jit, const uint32_t *words,
                          uint32_t nwords);
#endif

#ifdef JIT_STATS
  // Accumulate native vs interpreted instruction counts; prints coverage
  // periodically. Returns the wall-clock ns spent in this call's stats-print
  // I/O (0 when it doesn't report), so the dispatcher can exclude that stall
  // from the wall-clock-pinned RPCC.
  uint64_t note_exec(uint32_t native_instr, uint32_t interp_instr,
                     uint64_t comp_tsc = 0, uint64_t interp_tsc = 0);
#endif

#ifdef JIT_REGPROF
  // Pin-selection profiler: per-block executions (rp_hits) x the block's
  // GPR-access mask (rp_mask), summed over the live cache -> an
  // execution-weighted histogram of which Alpha GPRs dominate the hot path.
  // Prints the top registers periodically so we can choose the global pin set.
  void regprof_report();
#endif

private:
  JitBlock m_blocks[kCacheEntries];
  int m_cpu_id;
  uint64_t m_recorded;
  uint64_t m_itb_gen =
      0; // current ITB generation (bumped on every I-stream TB invalidate)
  uint64_t m_flush_gen = 0; // current icache-flush generation (bumped by
                            // flush(); lazy IC_FLUSH/IMB)
  uint64_t m_code_bytes;    // compiled bytes since last reclaim (see flush())
  bool m_reclaim_pending =
      false; // flush() hit kReclaimBytes; reclaim at the next dispatch boundary
  void *m_rt;            // asmjit::JitRuntime*
  JitOffsets m_off = {}; // field offsets for the inline load fast path
#ifdef JIT_DISASM
  FILE *m_disasm_fp =
      nullptr; // per-CPU disassembly trace file (jit_disasm_cpuN.txt)
#endif
#ifdef JIT_VERIFY
  uint64_t m_v_exec, m_v_fail;
#endif
#ifdef JIT_STATS
  uint64_t m_stat_native,
      m_stat_interp; // windowed: instrs run native vs interpreted
  uint64_t m_stat_hot,
      m_stat_miss; // windowed: compiled-chain dispatches, interp blocks
  uint64_t m_stat_compiled,
      m_stat_plen_sum; // cumulative: compiled blocks, sum of their lengths
  uint64_t m_stat_code_bytes; // cumulative: emitted x86 bytes (code expansion =
                              // /plen_sum)
  uint64_t m_stat_wall_last_ns; // steady_clock ns at the last window report
                                // (throughput delta)
  uint64_t m_tsc_compiled,
      m_tsc_interp; // windowed: host TSC cycles in b->code() vs interp fallback
  uint64_t m_tsc_window_start; // host TSC at window start (the time-split
                               // denominator)
  uint64_t m_bail_link, m_jmp_attempt,
      m_jmp_hit; // windowed: link-miss bails, jit_indirect attempts/hits
  uint64_t
      m_term_op[64]; // cumulative: opcode that ended a block's compiled prefix
  uint64_t
      m_pal_func[256]; // cumulative: CALL_PAL function code that ended a block
  uint64_t m_mtpr_func[256]; // cumulative: HW_MTPR (0x1d) IPR index that ended
                             // a block
  uint64_t m_hwld_func[16];  // cumulative: HW_LD (0x1b) form (ins>>12 & 0xf)
                             // that ended a block
  uint64_t m_misc_func[16];  // cumulative: MISC (0x18) Ra==31 form (ins>>12 &
                             // 0xf: RPCC/RC/RS) that ended a block
  bool m_first_breaker_logged; // one-shot guard for the punch-list print
#endif
};

#endif // ES40_JIT
#endif // INCLUDED_JITENGINE_H
