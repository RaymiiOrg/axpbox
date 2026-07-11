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
 * Contains the datatype definitions for use with Microsoft Visual C++ and
 *Linux.
 **/
#if !defined(INCLUDED_DATATYPES_H)
#define INCLUDED_DATATYPES_H

#if defined(HAVE_STDINT_H)
#include <stdint.h>
#endif

#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS // needed for inttypes
#endif

#if defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#endif

#if defined(_WIN32) && !defined(__GNUWIN32__)

#define U64(a) a##ull

#else // defined(_WIN32)

#define U64(a) UINT64_C(a)

#endif // defined(_WIN32)

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using u_int8_t = uint8_t;
using u_int16_t = uint16_t;
using u_int32_t = uint32_t;
using u_int64_t = uint64_t;

using offs_t = uint32_t;

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
                                        : ((a)&U64(0x00000000000000ff)));
#else // little endian optimization
  auto aa = static_cast<s64>(*reinterpret_cast<s8 *>(&a));
  return *reinterpret_cast<u64 *>(&aa);
#endif
}

/**
 * Zero-extend an 8-bit value to 64 bits (LDBU). Masks off everything above the
 *byte so that an MMIO device read wider than the access size is narrowed to the
 *architected byte (HRM: LDBU is "Load zero-extended byte"). DRAM byte reads are
 *already <= 0xff, so this is a no-op there.
 **/
inline u64 zext_u64_8(u64 a) { return a & U64(0x00000000000000ff); }

/**
 * Zero-extend a 16-bit value to 64 bits (LDWU).
 **/
inline u64 zext_u64_16(u64 a) { return a & U64(0x000000000000ffff); }

/**
 * Sign-extend a 12-bit value to 64 bits.
 **/
inline u64 sext_u64_12(u64 a) {
  return (((a)&U64(0x0000000000000800)) ? ((a) | U64(0xfffffffffffff000))
                                        : ((a)&U64(0x0000000000000fff)));
}

/**
 * Sign-extend a 13-bit value to 64 bits.
 **/
inline u64 sext_u64_13(u64 a) {
  return (((a)&U64(0x0000000000001000)) ? ((a) | U64(0xffffffffffffe000))
                                        : ((a)&U64(0x0000000000001fff)));
}

/**
 * Sign-extend an 16-bit value to 64 bits.
 **/
inline u64 sext_u64_16(u64 a) {
#if defined(ES40_BIG_ENDIAN)
  return (((a)&U64(0x0000000000008000)) ? ((a) | U64(0xffffffffffff0000))
                                        : ((a)&U64(0x000000000000ffff)));
#else // little endian optimization
  auto aa = static_cast<s64>(*reinterpret_cast<s16 *>(&a));
  return *reinterpret_cast<u64 *>(&aa);
#endif
}

/**
 * Sign-extend a 21-bit value to 64 bits.
 **/
inline u64 sext_u64_21(u64 a) {
  return (((a)&U64(0x0000000000100000)) ? ((a) | U64(0xffffffffffe00000))
                                        : ((a)&U64(0x00000000001fffff)));
}

/**
 * Sign-extend a 32-bit value to 64 bits.
 **/
inline u64 sext_u64_32(u64 a) {
#if defined(ES40_BIG_ENDIAN)
  return (((a)&U64(0x0000000080000000)) ? ((a) | U64(0xffffffff00000000))
                                        : ((a)&U64(0x00000000ffffffff)));
#else // little endian optimization
  auto aa = static_cast<s64>(*reinterpret_cast<s32 *>(&a));
  return *reinterpret_cast<u64 *>(&aa);
#endif
}

/**
 * Sign-extend a 48-bit value to 64 bits.
 **/
inline u64 sext_u64_48(u64 a) {
  return (((a)&U64(0x0000800000000000)) ? ((a) | U64(0xffff000000000000))
                                        : ((a)&U64(0x0000ffffffffffff)));
}

inline bool test_bit_64(u64 x, int bit) {
  return (x & (U64(0x1) << bit)) ? true : false;
}

/**
 * Sign-extend a 24-bit value to 32 bits.
 **/
inline u32 sext_u32_24(u32 a) {
  return (((a)&0x00800000) ? ((a) | 0xff000000) : ((a)&0x00ffffff));
}
#endif // INCLUDED_DATATYPES_H
