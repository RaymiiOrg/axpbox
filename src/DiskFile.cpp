/* AXPbox Alpha Emulator
 * Copyright (C) 2020 Tomáš Glozar
 * Copyright (C) 2020 Remy van Elst
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
 * Contains code to use a file as a disk image.
 **/
#include "DiskFile.hpp"
#include "StdAfx.hpp"

#include <vector>
std::vector<CDiskFile *> cd_diskfiles;

#ifdef _WIN32
#include <commdlg.h>
#include <windows.h>

void win32_select_file(HWND hwnd) {
  OPENFILENAME ofn;
  char szFileName[MAX_PATH] = "";

  ZeroMemory(&ofn, sizeof(ofn));

  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd;
  ofn.lpstrFilter = "ISO Files (*.iso)\0*.iso\0All Files (*.*)\0*.*\0";
  ofn.lpstrFile = szFileName;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
  ofn.lpstrDefExt = "iso";

  if (GetOpenFileNameA(&ofn)) {
    if (cd_diskfiles.size() > 0) {
      printf("Change ISO file to %s\n", szFileName);
      cd_diskfiles[0]->reload_file(szFileName);
    }
  }
}
#elif defined(HAVE_SDL) && defined(HAVE_SDL3)
#include <SDL3/SDL.h>

static const SDL_DialogFileFilter filters[] = {{"ISO files", "iso"},
                                               {"All files", "*"}};

static void SDLCALL callback(void *userdata, const char *const *filelist,
                             int filter) {
  if (!filelist) {
    SDL_Log("An error occured: %s", SDL_GetError());
    return;
  } else if (!*filelist) {
    SDL_Log("The user did not select any file.");
    return;
  }

  if (cd_diskfiles.size() > 0) {
    char *szFileName = SDL_strdup(filelist[0]);
    SDL_Log("Change ISO file to %s", szFileName);
    cd_diskfiles[0]->reload_file(szFileName);
    SDL_free(szFileName);
  }
}

void sdl_select_file(SDL_Window *window) {
  SDL_ShowOpenFileDialog(callback, nullptr, window, filters,
                         SDL_arraysize(filters), nullptr, false);
}
#endif

CDiskFile::CDiskFile(CConfigurator *cfg, CSystem *sys, CDiskController *c,
                     int idebus, int idedev)
    : CDisk(cfg, sys, c, idebus, idedev) {
  filename = myCfg->get_text_value("file");
  if (!filename) {
    // AXPbox behavior: fall back to a default image name (created on
    // first use through the autocreate path in reload_file).
    defaultFilename = std::string(devid_string) + ".default.img";
    fprintf(stderr, "%s: Disk has no filename attached! Assuming default: %s\n",
            devid_string, defaultFilename.c_str());
    filename = const_cast<char *>(defaultFilename.c_str());
  }

  reload_file(filename);
  state.scsi.media_changed = 0;

  model_number = myCfg->get_text_value("model_number", filename);

  // skip to the filename portion of the path.
  char *p = model_number;
#if defined(_WIN32)
  char x = '\\';
#elif defined(__VMS)
  char x = ']';
#else
  char x = '/';
#endif
  while (*p) {
    if (*p == x)
      model_number = p + 1;
    p++;
  }

  if (cdrom()) {
    printf("CD FILE\n");
    cd_diskfiles.push_back((CDiskFile *)this);
  }
  printf("%s: Mounted file %s, %" PRId64 " %zu-byte blocks, %" PRId64
         "/%ld/%ld.\n",
         devid_string, filename, byte_size / state.block_size, state.block_size,
         cylinders, heads, sectors);
}

CDiskFile::~CDiskFile(void) {
  printf("%s: Closing file.\n", devid_string);
  fclose(handle);
}

void CDiskFile::reload_file(char *_filename) {
  if (handle) {
    fclose(handle);
    handle = nullptr;
  }
  if (read_only)
    handle = fopen(_filename, "rb");
  else
    handle = fopen_large(_filename, "rb+");
  if (!handle) {
    printf("%s: Could not open file %s!\n", devid_string, _filename);

    int sz = myCfg->get_num_value("autocreate_size", false, 0) / 1024 / 1024;
    if (!sz)
      FAILURE_1(Runtime, "%s: File does not exist and no autocreate_size set",
                devid_string);

    void *crt_buf;
    handle = fopen_large(_filename, "wb");
    if (!handle)
      FAILURE_1(Runtime, "%s: File does not exist and could not be created",
                devid_string);
    crt_buf = calloc(1024, 1024);
    printf("%s: writing %d 1kB blocks:   0%%\b\b\b\b", devid_string, sz);

    int lastpc = 0;
    for (int a = 0; a < sz; a++) {
      fwrite(crt_buf, 1024, 1024, handle);

      int pc = a * 100 / sz;
      if (pc != lastpc) {
        printf("%3d\b\b\b", pc);
        lastpc = pc;
      }

      fflush(stdout);
    }

    printf("100%%\n");
    fclose(handle);
    free(crt_buf);
    if (read_only)
      handle = fopen_large(_filename, "rb");
    else
      handle = fopen_large(_filename, "rb+");
    if (!handle) {
      FAILURE_1(Runtime, "%s: File created could not be opened", devid_string);
    }

    printf("%s: %d MB file %s created.\n", devid_string, sz, _filename);
  }

  // determine size...
  fseek_large(handle, 0, SEEK_END);
  byte_size = ftell_large(handle);
  fseek_large(handle, 0, SEEK_SET);
  state.byte_pos = ftell_large(handle);

  sectors = 32;
  heads = 8;

  // calc_cylinders();
  determine_layout();
  state.scsi.media_changed = 1;
}

bool CDiskFile::seek_byte(off_t_large byte) {
  if (byte >= byte_size) {
    FAILURE_1(InvalidArgument, "%s: Seek beyond end of file!\n", devid_string);
  }

  fseek_large(handle, byte, SEEK_SET);
  state.byte_pos = ftell_large(handle);

  return true;
}

size_t CDiskFile::read_bytes(void *dest, size_t bytes) {
  size_t r;
  r = fread(dest, 1, bytes, handle);
  state.byte_pos = ftell_large(handle);
  return r;
}

size_t CDiskFile::write_bytes(void *src, size_t bytes) {
  if (read_only)
    return 0;

  size_t r;
  r = fwrite(src, 1, bytes, handle);
  state.byte_pos = ftell_large(handle);
  return r;
}

void CDiskFile::flush() {
  if (handle && !read_only)
    fflush(handle);
}
