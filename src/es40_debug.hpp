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
 *
 * Parts of this file based upon GXemul, which is Copyright (C) 2004-2007
 * Anders Gavare.  All rights reserved.
 */

#include <cstdio>
#include <cstdlib>
#include <memory>

#if !defined(INCLUDED_DEBUG_H)
#define INCLUDED_DEBUG_H

#define DEBUG_BUFSIZE 1024
#define FAILMSG_BUFSIZE 8000
#define DEBUG_INDENTATION 4

#define FAILURE(cls, format, ...) \
    do { \
        int size = std::snprintf(nullptr, 0, format, ##__VA_ARGS__) + 1; \
        if (size <= 0) { \
            std::fprintf(stderr, "Error: Failed to determine log message size.\n"); \
            std::exit(EXIT_FAILURE); \
        } \
        std::unique_ptr<char[]> buffer(new char[size]); \
        int written = std::snprintf(buffer.get(), size, format, ##__VA_ARGS__); \
        if (written < 0 || written >= size) { \
            std::fprintf(stderr, "Error: Failed to format log message.\n"); \
            std::exit(EXIT_FAILURE); \
        } \
        std::fprintf(stderr, "%s, line %i, function '%s'\n", buffer.get(), __LINE__, __FILE__); \
        std::exit(EXIT_FAILURE); \
    } while(0)


#define FAILURE_1(cls, error_msg, a)                                           \
  {                                                                            \
    char what_msg[FAILMSG_BUFSIZE];                                            \
    sprintf(what_msg, error_msg, a);                                           \
    FAILURE(cls, what_msg);                                                    \
  }

#define FAILURE_2(cls, error_msg, a, b)                                        \
  {                                                                            \
    char what_msg[FAILMSG_BUFSIZE];                                            \
    sprintf(what_msg, error_msg, a, b);                                        \
    FAILURE(cls, what_msg);                                                    \
  }

#define FAILURE_3(cls, error_msg, a, b, c)                                     \
  {                                                                            \
    char what_msg[FAILMSG_BUFSIZE];                                            \
    sprintf(what_msg, error_msg, a, b, c);                                     \
    FAILURE(cls, what_msg);                                                    \
  }

#define FAILURE_4(cls, error_msg, a, b, c, d)                                  \
  {                                                                            \
    char what_msg[FAILMSG_BUFSIZE];                                            \
    sprintf(what_msg, error_msg, a, b, c, d);                                  \
    FAILURE(cls, what_msg);                                                    \
  }

#define FAILURE_5(cls, error_msg, a, b, c, d, e)                               \
  {                                                                            \
    char what_msg[FAILMSG_BUFSIZE];                                            \
    sprintf(what_msg, error_msg, a, b, c, d, e);                               \
    FAILURE(cls, what_msg);                                                    \
  }

#define FAILURE_6(cls, error_msg, a, b, c, d, e, f)                            \
  {                                                                            \
    char what_msg[FAILMSG_BUFSIZE];                                            \
    sprintf(what_msg, error_msg, a, b, c, d, e, f);                            \
    FAILURE(cls, what_msg);                                                    \
  }

#define CHECK_ALLOCATION(ptr)                                                  \
  {                                                                            \
    if ((ptr) == NULL)                                                         \
      FAILURE(OutOfMemory, "Out of memory");                                   \
  }

#define CHECK_REALLOCATION(dst, src, type)                                     \
  {                                                                            \
    type *rea_x;                                                               \
    rea_x = (type *)src;                                                       \
    if ((rea_x) == NULL) {                                                     \
      FAILURE(OutOfMemory, "Out of memory");                                   \
    } else {                                                                   \
      dst = rea_x;                                                             \
    }                                                                          \
  }

#endif
