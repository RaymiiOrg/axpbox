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
 * Contains the definitions for the emulated Ali M1543C USB chipset part.
 **/
#if !defined(INCLUDED_ALIM1543C_USB_H_)
#define INCLUDED_ALIM1543C_USB_H_

#include "PCIDevice.hpp"

/**
 * \brief Emulated USB part of ALi M1543C multi-function device.
 *
 * \todo This device is just a stub. Not functional yet.
 *
 * Documentation consulted:
 *  - Ali M1543C B1 South Bridge Version 1.20
 *    (http://mds.gotdns.com/sensors/docs/ali/1543dScb1-120.pdf)
 *  .
 **/
class CAliM1543C_usb : public CPCIDevice {
public:
  virtual int SaveState(FILE *f);
  virtual int RestoreState(FILE *f);

  CAliM1543C_usb(CConfigurator *cfg, class CSystem *c, int pcibus, int pcidev);
  virtual ~CAliM1543C_usb();
  virtual void WriteMem_Bar(int func, int bar, u32 address, int dsize,
                            u32 data);
  virtual u32 ReadMem_Bar(int func, int bar, u32 address, int dsize);

private:
  u64 usb_hci_read(u64 address, int dsize);
  void usb_hci_write(u64 address, int dsize, u64 data);

  /// The state structure contains all elements that need to be saved to the
  /// statefile.
  struct SUSB_state {
    u32 usb_data[0x110 / 4];
  } state;
};
#endif // !defined(INCLUDED_ALIM1543C_USB_H)
