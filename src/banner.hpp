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
 * Although this is not required, the author would appreciate being notified
 * of, and receiving any modifications you may make to the source code that
 * might serve the general public.
 */

/**
 * \file
 * Startup banner shared by the axpbox run and configure subcommands. Padding
 * is computed at runtime, so the border stays aligned no matter how long
 * VERSION (or any other line) gets.
 **/
#if !defined(__BANNER_HPP__)
#define __BANNER_HPP__

#include <stdio.h>
#include <string.h>

#if !defined(VERSION)
#define VERSION "unknown"
#endif

#define BANNER_INNER_WIDTH 70

static inline void banner_border(void) {
  int i;
  printf("   **");
  for (i = 0; i < BANNER_INNER_WIDTH; i++)
    putchar('=');
  printf("**\n");
}

static inline void banner_line(const char *text, bool centered) {
  int len = (int)strlen(text);
  if (len > BANNER_INNER_WIDTH - 4)
    len = BANNER_INNER_WIDTH - 4; // truncate rather than break the border

  int lpad = centered ? (BANNER_INNER_WIDTH - len + 1) / 2 : 2;
  int rpad = BANNER_INNER_WIDTH - len - lpad;
  printf("   ||%*s%.*s%*s||\n", lpad, "", len, text, rpad, "");
}

static inline void print_axpbox_banner(const char *title) {
  printf("\n\n");
  banner_border();
  banner_line(title, true);
  banner_line("Version " VERSION, true);
  banner_line("", false);
  banner_line("Copyright (C) 2007-2026 by:", false);
  banner_line("  2007-2010  Camiel Vanderhoeven and the ES40 Emulator Project",
              false);
  banner_line("  2018       Tim Stark (fsword7)", false);
  banner_line("  2020-2023  Tomas Glozar", false);
  banner_line("  2020-2026  Remy van Elst", false);
  banner_line("  2023-2026  gdwnldsKSC and the ES40-Emu project", false);
  banner_line("", false);
  banner_line("Website: https://github.com/lenticularis39/axpbox", false);
  banner_line("", false);
  banner_line("This program is free software; you can redistribute it and/or",
              false);
  banner_line("modify it under the terms of the GNU General Public License",
              false);
  banner_line("as published by the Free Software Foundation; either version 2",
              false);
  banner_line("of the License, or (at your option) any later version.", false);
  banner_border();
  printf("\n\n");
}
#endif //! defined(__BANNER_HPP__)
