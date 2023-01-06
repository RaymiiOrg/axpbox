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
 * Contains definitions to use a raw device as a disk image.
 **/
#if !defined(__DISKDEV_H__)
#define __DISKDEV_H__

#include "Disk.hpp"

/**
 * \brief Emulated disk that uses a raw device.
 **/
class CDiskDevice : public CDisk {
public:
  CDiskDevice(CConfigurator *cfg, CSystem *sys, CDiskController *c, int idebus,
              int idedev);
  virtual ~CDiskDevice(void);

  virtual bool seek_byte(off_t_large byte);
  virtual size_t read_bytes(void *dest, size_t bytes);
  virtual size_t write_bytes(void *src, size_t bytes);

protected:
#if defined(_WIN32)
  HANDLE handle;
  char *buffer;
  size_t buffer_size;
  size_t dev_block_size;
#else
  FILE *handle;
#endif
  char *filename;
};
#endif //! defined(__DISKFILE_H__)
