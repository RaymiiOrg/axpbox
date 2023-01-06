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
 * Contains code for the base class for devices that connect to the chipset.
 **/
#include "SystemComponent.hpp"
#include "StdAfx.hpp"
#include "System.hpp"

/**
 * Constructor.
 **/
CSystemComponent::CSystemComponent(CConfigurator *cfg, CSystem *system) {
  char *a;
  char *b;

  system->RegisterComponent(this);
  cSystem = system;
  myCfg = cfg;

  a = myCfg->get_myName();
  b = myCfg->get_myValue();

  CHECK_ALLOCATION(devid_string = (char *)malloc(strlen(a) + strlen(b) + 3));
  sprintf(devid_string, "%s(%s)", a, b);
}

/**
 * destructor.
 **/
CSystemComponent::~CSystemComponent() {
  cSystem->UnregisterComponent(this);
  free(devid_string);
  devid_string = nullptr;
}
