/* AXPbox Alpha Emulator
 * Copyright (C) 2020 Tomáš Glozar
 * Website: https://github.com/lenticularis39/axpbox
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
 */

#if !defined(INCLUDED_NETWORK_BACKEND_H)
#define INCLUDED_NETWORK_BACKEND_H

#include "StdAfx.hpp"

class CConfigurator;

/**
 * \brief Packet filtering state derived from the emulated NIC's CSR6 bits
 * and setup frame.
 *
 * Backends map this onto whatever host-side filtering they support; pcap
 * compiles it into a BPF expression, tap ignores it (the guest driver's
 * filter is authoritative there).
 */
struct NetworkFilter {
  u8 mac_list[16][6];  ///< perfect-filter entries (all-zero = unused slot)
  bool promiscuous;    ///< CSR6 PR: accept all packets
  bool receive_all;    ///< CSR6 RA: pass all packets regardless of filtering
  bool pass_multicast; ///< CSR6 PM: pass all multicast packets
  bool inverse;        ///< CSR6 IF: invert the perfect-filter match
  u8 own_mac[6];       ///< NIC MAC, fallback before a setup frame arrives
};

/**
 * \brief Abstract network backend interface.
 *
 * Separates the emulated NIC (DEC21143) from the host networking method.
 * Implementations: CNetworkPcap (libpcap), CNetworkTap (Linux TAP/TUN).
 */
class CNetworkBackend {
public:
  virtual ~CNetworkBackend() {}

  /**
   * Initialize the backend from configuration.
   * Returns true on success.
   */
  virtual bool init(const char *devid_string, CConfigurator *cfg) = 0;

  /**
   * Send a packet to the host network.
   * Returns 0 on success, -1 on error.
   */
  virtual int send(const u8 *data, int len) = 0;

  /**
   * Receive a packet from the host network (non-blocking).
   * On success, sets data pointer and len, returns 1.
   * If no packet available, returns 0.
   * On error, returns -1.
   * The data pointer is valid until the next call to receive().
   */
  virtual int receive(const u8 **data, int *len) = 0;

  /**
   * Set up MAC-based packet filtering from the NIC's filtering state.
   */
  virtual void set_filter(const NetworkFilter &filter) = 0;

  /**
   * Close the backend and release resources.
   */
  virtual void close() = 0;
};

/**
 * Factory: create the appropriate backend based on config.
 * Reads "type" config value: "pcap" (default) or "tap".
 */
CNetworkBackend *create_network_backend(CConfigurator *cfg);

#endif // !defined(INCLUDED_NETWORK_BACKEND_H)
