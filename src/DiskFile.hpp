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
 * Contains definitions to use a file as a disk image.
 **/

#if !defined(__DISKFILE_H__)
#define __DISKFILE_H__

#include "Disk.hpp"
#include "DiskFileBinCue.hpp"
#include <string>

/**
 * \brief Emulated disk that uses an image file.
 *
 * Supports raw/ISO images (existing behaviour, unchanged) and BIN/CUE
 * images (new). BIN/CUE support is activated automatically when the
 * configured filename ends in .cue (case-insensitive). If the .cue
 * cannot be parsed the code falls back to treating the file as a raw
 * image so that no existing configuration is broken.
 **/
class CDiskFile : public CDisk {
public:
  CDiskFile(CConfigurator *cfg, CSystem *sys, CDiskController *c, int idebus,
            int idedev);
  virtual ~CDiskFile(void);
  std::string defaultFilename;

  virtual bool seek_byte(off_t_large byte);
  virtual size_t read_bytes(void *dest, size_t bytes);
  virtual size_t write_bytes(void *src, size_t bytes);
  virtual void flush();

  void reload_file(char *filename);
  FILE *get_handle() { return handle; }

  // ---------------------------------------------------------------
  // BIN/CUE query interface (safe to call even for ISO images;
  // returns sane defaults when bin/cue is not active).
  // ---------------------------------------------------------------
  bool is_bincue_image() const { return is_bincue; }
  int get_track_count() const { return track_count; }

  /**
   * \brief Return a pointer to track \a idx, or nullptr if out of range.
   *
   * Valid indices are 0 .. get_track_count()-1.
   * Returns nullptr when bin/cue is not active.
   **/
  const CueTrack *get_track(int idx) const {
    if (!is_bincue || !tracks || idx < 0 || idx >= track_count)
      return nullptr;
    return &tracks[idx];
  }

  /**
   * \brief Return the track that contains \a lba, or nullptr.
   **/
  const CueTrack *get_track_for_lba(long lba) const;

protected:
  FILE *handle = nullptr;
  char *filename = nullptr;

private:
  // ------------------------------------------------------------------
  // BIN/CUE internal state
  // All members are initialised in the constructor and reset by
  // reset_bincue_state().  Nothing here is touched by the ISO/raw path.
  // ------------------------------------------------------------------
  bool is_bincue;               ///< True while a .cue is loaded
  CueTrack *tracks;             ///< Heap-allocated track array
  int track_count;              ///< Elements in tracks[]
  long current_lba;             ///< Logical LBA of current position
  off_t_large logical_byte_pos; ///< Logical byte position (LBA * 2048)

  // Internal helpers ------------------------------------------------
  void reset_bincue_state();
  bool try_parse_cue(const char *cue_path);
  bool open_bin_files();
  void close_bin_handles();

  CueTrackMode parse_mode_string(const char *mode_str, int &sector_size,
                                 int &data_offset, int &data_size);

  bool lba_to_file_position(long lba, int &track_idx,
                            off_t_large &file_offset) const;

  // MSF <-> LBA helpers (static: no instance state required)
  static long msf_to_lba(int m, int s, int f);
  static void lba_to_msf(long lba, int &m, int &s, int &f);
  static bool has_cue_extension(const char *path);
  static char path_separator();
};

#endif // !defined(__DISKFILE_H__)
