/* AXPbox Alpha Emulator
 * Copyright (C) 2020 Tomáš Glozar
 * Website: https://github.com/lenticularis39/axpbox
 *
 * Forked from: ES40 emulator
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 * Copyright (C) 2007 by Camiel Vanderhoeven
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * Although this is not required, the author would appreciate being notified of,
 * and receiving any modifications you may make to the source code that might
 * serve the general public.
 */

 /**
  * \file
  * Contains code macros for the processor floating-point operate instructions.
  * Based on ARM chapter 4.10.
 **/
#define FP_IS_ZERO(val) (((val) & ~FPR_SIGN) == 0)
#define FP_IS_NEGATIVE(val) (((val) & FPR_SIGN) != 0)

/* copy sign */
#define DO_CPYS   FPSTART; \
  state.f[FREG_3] = (state.f[FREG_1] & FPR_SIGN) | (state.f[FREG_2] &~FPR_SIGN);

#define DO_CPYSN  FPSTART; \
  state.f[FREG_3] = ((state.f[FREG_1] & FPR_SIGN) ^ FPR_SIGN) | (state.f[FREG_2] &~FPR_SIGN);

#define DO_CPYSE  FPSTART; \
  state.f[FREG_3] = (state.f[FREG_1] & (FPR_SIGN | FPR_EXP)) | (state.f[FREG_2] &~(FPR_SIGN | FPR_EXP));

/* conditional moves */

/* FCMOVEQ: move if equal to zero */
#define DO_FCMOVEQ  FPSTART; \
  if (FP_IS_ZERO(state.f[FREG_1])) \
    state.f[FREG_3] = state.f[FREG_2];

/* FCMOVGE - move if greater than or equal to zero */
#define DO_FCMOVGE  FPSTART; \
  if (!FP_IS_NEGATIVE(state.f[FREG_1]) || FP_IS_ZERO(state.f[FREG_1])) \
    state.f[FREG_3] = state.f[FREG_2];

/* FCMOVGT - move if greater than zero */
#define DO_FCMOVGT  FPSTART; \
  if (!FP_IS_NEGATIVE(state.f[FREG_1]) && !FP_IS_ZERO(state.f[FREG_1])) \
    state.f[FREG_3] = state.f[FREG_2];

/* FCMOVLE - move if less than or equal to zero */
#define DO_FCMOVLE  FPSTART; \
  if (FP_IS_NEGATIVE(state.f[FREG_1]) || FP_IS_ZERO(state.f[FREG_1])) \
    state.f[FREG_3] = state.f[FREG_2];

/* FCMOVLT - move if less than zero */
#define DO_FCMOVLT  FPSTART; \
  if (FP_IS_NEGATIVE(state.f[FREG_1]) && !FP_IS_ZERO(state.f[FREG_1])) \
    state.f[FREG_3] = state.f[FREG_2];

/* FCMOVNE: move if not equal to zero */
#define DO_FCMOVNE  FPSTART; \
  if (!FP_IS_ZERO(state.f[FREG_1])) \
    state.f[FREG_3] = state.f[FREG_2];

/* floating-point control register */
#define DO_MF_FPCR  FPSTART;                             \
  state.f[FREG_1] = read_fpcr_arch();

#define DO_MT_FPCR  FPSTART;                         \
  write_fpcr_arch(state.f[FREG_1]);

/* add */
#define DO_ADDG  FPSTART; \
  state.f[FREG_3] = vax_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_G, /*sub=*/false);

#define DO_ADDF  FPSTART; \
  state.f[FREG_3] = vax_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_F, /*sub=*/false);

#define DO_ADDT  FPSTART; \
  state.f[FREG_3] = ieee_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_T, /*sub=*/false);

#define DO_ADDS  FPSTART; \
  state.f[FREG_3] = ieee_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_S, /*sub=*/false);

/* subtract */
#define DO_SUBG  FPSTART; \
  state.f[FREG_3] = vax_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_G, /*sub=*/true);

#define DO_SUBF  FPSTART; \
  state.f[FREG_3] = vax_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_F, /*sub=*/true);

#define DO_SUBT  FPSTART; \
  state.f[FREG_3] = ieee_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_T, /*sub=*/true);

#define DO_SUBS  FPSTART; \
  state.f[FREG_3] = ieee_fadd(state.f[FREG_1], state.f[FREG_2], ins, DT_S, /*sub=*/true);

/* comparison */
#define DO_CMPGEQ FPSTART; \
  { int c = vax_fcmp(state.f[FREG_1], state.f[FREG_2], ins); \
    state.f[FREG_3] = (c == 0) ? U64(0x4000000000000000) : 0; }

#define DO_CMPGLE FPSTART; \
  { int c = vax_fcmp(state.f[FREG_1], state.f[FREG_2], ins); \
    state.f[FREG_3] = (c <= 0) ? U64(0x4000000000000000) : 0; }

#define DO_CMPGLT FPSTART; \
  { int c = vax_fcmp(state.f[FREG_1], state.f[FREG_2], ins); \
    state.f[FREG_3] = (c < 0) ? U64(0x4000000000000000) : 0; }

/* HRM Table A-11: CMPTLT/CMPTLE raise INV on ANY NaN (incl. quiet); CMPTEQ
   (like CMPTUN) is quiet -- only a signaling NaN raises INV, via ieee_unpack. */
#define DO_CMPTEQ FPSTART; \
  { int c = ieee_fcmp(state.f[FREG_1], state.f[FREG_2], ins, 0); \
    state.f[FREG_3] = (c == 0) ? U64(0x4000000000000000) : 0; }

#define DO_CMPTLE FPSTART; \
  { int c = ieee_fcmp(state.f[FREG_1], state.f[FREG_2], ins, 1); \
    state.f[FREG_3] = (c <= 0) ? U64(0x4000000000000000) : 0; }

#define DO_CMPTLT FPSTART; \
  { int c = ieee_fcmp(state.f[FREG_1], state.f[FREG_2], ins, 1); \
    state.f[FREG_3] = (c < 0) ? U64(0x4000000000000000) : 0; }

/* CMPTUN is quiet: only sNaN raises INV (handled inside ieee_unpack via the
   QNAN-bit check). qNaN inputs return TRUE without raising. */
#define DO_CMPTUN FPSTART; \
  { int c = ieee_fcmp(state.f[FREG_1], state.f[FREG_2], ins, 0); \
    state.f[FREG_3] = (c == 2) ? U64(0x4000000000000000) : 0; }

/* format conversions */
#define DO_CVTQL  FPSTART;                                                                           \
  {                                                                                                  \
    u64 cvtql_src = state.f[FREG_2];                                                                 \
    state.f[FREG_3] = ((cvtql_src & 0xC0000000) << 32) | ((cvtql_src & 0x3FFFFFFF) << 29);            \
    if(FPR_GETSIGN(cvtql_src) ? (cvtql_src < U64(0xFFFFFFFF80000000)) :                              \
         (cvtql_src > U64(0x000000007FFFFFFF)))                                                      \
    {                                                                                                \
      write_fpcr_arch(state.fpcr | FPCR_IOV);                                                        \
      if(ins & I_FTRP_V)                                                                             \
        vax_trap(TRAP_IOV, ins);                                                                     \
    }                                                                                                \
  }

#define DO_CVTLQ  FPSTART;                                               \
  state.f[FREG_3] = sext_u64_32(((state.f[FREG_2] >> 32) & 0xC0000000) | \
                                  ((state.f[FREG_2] >> 29) & 0x3FFFFFFF));

#define DO_CVTGQ  FPSTART; \
  state.f[FREG_3] = vax_cvtfi(state.f[FREG_2], ins);

#define DO_CVTQG  FPSTART; \
  state.f[FREG_3] = vax_cvtif(state.f[FREG_2], ins, DT_G);

#define DO_CVTQF  FPSTART; \
  state.f[FREG_3] = vax_cvtif(state.f[FREG_2], ins, DT_F);

#define DO_CVTTQ  FPSTART; \
  state.f[FREG_3] = ieee_cvtfi(state.f[FREG_2], ins);

#define DO_CVTQT  FPSTART; \
  state.f[FREG_3] = ieee_cvtif(state.f[FREG_2], ins, DT_T);

#define DO_CVTQS  FPSTART; \
  state.f[FREG_3] = ieee_cvtif(state.f[FREG_2], ins, DT_S);

#define DO_CVTGD  FPSTART;                 \
  vax_unpack(state.f[FREG_2], &ufp2, ins); \
  state.f[FREG_3] = vax_rpack_d(&ufp2, ins);

#define DO_CVTDG  FPSTART;                   \
  vax_unpack_d(state.f[FREG_2], &ufp2, ins); \
  state.f[FREG_3] = vax_rpack(&ufp2, ins, DT_G);

#define DO_CVTGF  FPSTART;                 \
  vax_unpack(state.f[FREG_2], &ufp2, ins); \
  state.f[FREG_3] = vax_rpack(&ufp2, ins, DT_F);

#define DO_CVTST  FPSTART; \
  state.f[FREG_3] = ieee_cvtst(state.f[FREG_2], ins);

#define DO_CVTTS  FPSTART; \
  state.f[FREG_3] = ieee_cvtts(state.f[FREG_2], ins);

/* float <-> integer register moves
 * Alpha requires Rb == 31 for these bit-pattern moves.
 * QEMU enforces this with REQUIRE_REG_31; we use GO_PAL(OPCDEC). */
#define DO_FTOIS  FPSTART; \
  do { \
    if (REG_2 != 31) { GO_PAL(OPCDEC); } \
    else { state.r[REG_3] = sext_u64_32(ieee_sts(state.f[FREG_1])); } \
  } while (0)

#define DO_FTOIT  FPSTART; \
  do { \
    if (REG_2 != 31) { GO_PAL(OPCDEC); } \
    else { state.r[REG_3] = state.f[FREG_1]; } \
  } while (0)

 /* ITOFT: raw 64-bit move into the FP reg */
#define DO_ITOFT  FPSTART; \
  do { \
    if (REG_2 != 31) { GO_PAL(OPCDEC); } \
    else { state.f[FREG_3] = state.r[REG_1]; } \
  } while (0)

/* ITOFS: build an S-format value from the low 32 bits */
#define DO_ITOFS  FPSTART; \
  do { \
    if (REG_2 != 31) { GO_PAL(OPCDEC); } \
    else { state.f[FREG_3] = ieee_lds((u32)state.r[REG_1]); } \
  } while (0)

/* ITOFF: build a VAX F-format value from the low 32 bits */
#define DO_ITOFF  FPSTART; \
  do { \
    if (REG_2 != 31) { GO_PAL(OPCDEC); } \
    else { state.f[FREG_3] = vax_ldf(SWAP_VAXF((u32)state.r[REG_1])); } \
  } while (0)

/* Multiply */
#define DO_MULG FPSTART; \
  state.f[FREG_3] = vax_fmul(state.f[FREG_1], state.f[FREG_2], ins, DT_G);

#define DO_MULF FPSTART; \
  state.f[FREG_3] = vax_fmul(state.f[FREG_1], state.f[FREG_2], ins, DT_F);

#define DO_MULT FPSTART; \
  state.f[FREG_3] = ieee_fmul(state.f[FREG_1], state.f[FREG_2], ins, DT_T);

#define DO_MULS FPSTART; \
  state.f[FREG_3] = ieee_fmul(state.f[FREG_1], state.f[FREG_2], ins, DT_S);

/* Divide */
#define DO_DIVG FPSTART; \
  state.f[FREG_3] = vax_fdiv(state.f[FREG_1], state.f[FREG_2], ins, DT_G);

#define DO_DIVF FPSTART; \
  state.f[FREG_3] = vax_fdiv(state.f[FREG_1], state.f[FREG_2], ins, DT_F);

#define DO_DIVT FPSTART; \
  state.f[FREG_3] = ieee_fdiv(state.f[FREG_1], state.f[FREG_2], ins, DT_T);

#define DO_DIVS FPSTART; \
  state.f[FREG_3] = ieee_fdiv(state.f[FREG_1], state.f[FREG_2], ins, DT_S);

/* Square-root */
#define DO_SQRTG  FPSTART; \
  do { if (REG_1 != 31) { GO_PAL(OPCDEC); } \
       else { state.f[FREG_3] = vax_sqrt(state.f[FREG_2], ins, DT_G); } } while (0)

#define DO_SQRTF  FPSTART; \
  do { if (REG_1 != 31) { GO_PAL(OPCDEC); } \
       else { state.f[FREG_3] = vax_sqrt(state.f[FREG_2], ins, DT_F); } } while (0)

#define DO_SQRTT  FPSTART; \
  do { if (REG_1 != 31) { GO_PAL(OPCDEC); } \
       else { state.f[FREG_3] = ieee_sqrt(state.f[FREG_2], ins, DT_T); } } while (0)

#define DO_SQRTS  FPSTART; \
  do { if (REG_1 != 31) { GO_PAL(OPCDEC); } \
       else { state.f[FREG_3] = ieee_sqrt(state.f[FREG_2], ins, DT_S); } } while (0)
