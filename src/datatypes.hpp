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

#if !defined(INCLUDED_DATATYPES_H)
#define INCLUDED_DATATYPES_H

#if defined(HAVE_STDINT_H)
#include <cstdint>
#endif

#if defined(HAVE_INTTYPES_H)
#include <cinttypes>
#endif

#if defined(_WIN32) && !defined(__GNUWIN32__)

#define U64(a) a##ui64

#else // defined(_WIN32)

#define U64(a)  UINT64_C(a)

#endif // defined(_WIN32)

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using u_int8_t = u8;
using u_int16_t = u16;
using u_int32_t = u32;
using u_int64_t = u64;

#define HAVE_U_INT8_T 1
#define HAVE_INT8_T 1
#define HAVE_U_INT16_T 1
#define HAVE_INT16_T 1
#define HAVE_U_INT32_T 1
#define HAVE_INT32_T 1
#define HAVE_U_INT64_T 1
#define HAVE_INT64_T 1

/**
 * Sign-extend an 8-bit value to 64 bits.
 **/
inline u64 sext_u64_8(u64 a) {
#if defined(ES40_BIG_ENDIAN)
  return (((a)&U64(0x0000000000000080)) ? ((a) | U64(0xffffffffffffff00))
                                        : ((a) & U64(0x00000000000000ff)));
#else
  // Optimised implementation for LE machines of this hotpath inline
  // function. The implementation above compiles to three opcodes but
  // the following compiles to just one movsx? opcode.
  auto aa = static_cast<s64>(*reinterpret_cast<s8*>(&a));
  return *reinterpret_cast<u64*>(&aa);
#endif
}

/**
 * Sign-extend a 12-bit value to 64 bits.
 **/
inline u64 sext_u64_12(u64 a) {
  return ((((a)&U64(0x0000000000000800)) != 0U) ? ((a) | U64(0xfffffffffffff000))
                                        : ((a) & U64(0x0000000000000fff)));
}

/**
 * Sign-extend a 13-bit value to 64 bits.
 **/
inline u64 sext_u64_13(u64 a) {
  return ((((a)&U64(0x0000000000001000)) != 0U) ? ((a) | U64(0xffffffffffffe000))
                                        : ((a) & U64(0x0000000000001fff)));
}

/**
 * Sign-extend an 16-bit value to 64 bits.
 **/
inline u64 sext_u64_16(u64 a) {
#if defined(ES40_BIG_ENDIAN)
  return (((a)&U64(0x0000000000008000)) ? ((a) | U64(0xffffffffffff0000))
                                        : ((a) & U64(0x000000000000ffff)));
#else
  // Optimised implementation for LE machines of this hotpath inline
  // function. The implementation above compiles to three opcodes but
  // the following compiles to just one movsx? opcode.
  auto aa = static_cast<s64>(*reinterpret_cast<s16*>(&a));
  return *reinterpret_cast<u64*>(&aa);
#endif
}

/**
 * Sign-extend a 21-bit value to 64 bits.
 **/
inline u64 sext_u64_21(u64 a) {
  return ((((a)&U64(0x0000000000100000)) != 0U) ? ((a) | U64(0xffffffffffe00000))
                                        : ((a) & U64(0x00000000001fffff)));
}

/**
 * Sign-extend a 32-bit value to 64 bits.
 **/
inline u64 sext_u64_32(u64 a) {
#if defined(ES40_BIG_ENDIAN)
  return (((a)&U64(0x0000000080000000)) ? ((a) | U64(0xffffffff00000000))
                                        : ((a) & U64(0x00000000ffffffff)));
#else
  // Optimised implementation for LE machines of this hotpath inline
  // function. The implementation above compiles to three opcodes but
  // the following compiles to just one movsx? opcode.
  auto aa = static_cast<s64>(*reinterpret_cast<s32*>(&a));
  return *reinterpret_cast<u64*>(&aa);
#endif
}

/**
 * Sign-extend a 48-bit value to 64 bits.
 **/
inline u64 sext_u64_48(u64 a) {
  return ((((a)&U64(0x0000800000000000)) != 0U) ? ((a) | U64(0xffff000000000000))
                                        : ((a) & U64(0x0000ffffffffffff)));
}

inline bool test_bit_64(u64 x, int bit) {
  return (x & (U64(0x1) << bit)) != 0;
}

/**
 * Sign-extend a 24-bit value to 32 bits.
 **/
inline u32 sext_u32_24(u32 a) {
  return ((((a)&0x00800000) != 0U) ? ((a) | 0xff000000) : ((a)&0x00ffffff));
}
#endif // INCLUDED_DATATYPES_H
