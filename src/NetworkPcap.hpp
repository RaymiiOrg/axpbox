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

#if !defined(INCLUDED_NETWORK_PCAP_H)
#define INCLUDED_NETWORK_PCAP_H

#include "NetworkBackend.hpp"

#if defined(WIN32)
#define HAVE_REMOTE
#endif
#include <pcap.h>

#if defined(_WIN32)
/* wpcap.dll is loaded at run time from the Npcap install directory, so the
 * executable does not require Npcap at load time and works with Npcap
 * installed outside WinPcap-compatibility mode. All pcap calls in AXPbox go
 * through these pointers (the defines below remap the pcap_* names). */
bool load_wpcap();

struct pcap_rmtauth;

#if !defined(PCAP_OPENFLAG_PROMISCUOUS)
#define PCAP_OPENFLAG_PROMISCUOUS 0x00000001
#endif
#if !defined(PCAP_OPENFLAG_NOCAPTURE_LOCAL)
#define PCAP_OPENFLAG_NOCAPTURE_LOCAL 0x00000008
#endif

extern int (*f_pcap_findalldevs)(pcap_if_t **, char *);
extern void (*f_pcap_freealldevs)(pcap_if_t *);
extern pcap_t *(*f_pcap_open)(const char *, int, int, int,
                              struct pcap_rmtauth *, char *);
extern int (*f_pcap_setnonblock)(pcap_t *, int, char *);
extern int (*f_pcap_sendpacket)(pcap_t *, const unsigned char *, int);
extern int (*f_pcap_next_ex)(pcap_t *, struct pcap_pkthdr **,
                             const unsigned char **);
extern char *(*f_pcap_geterr)(pcap_t *);
extern int (*f_pcap_compile)(pcap_t *, struct bpf_program *, const char *, int,
                             bpf_u_int32);
extern int (*f_pcap_setfilter)(pcap_t *, struct bpf_program *);
extern void (*f_pcap_close)(pcap_t *);

#define pcap_findalldevs f_pcap_findalldevs
#define pcap_freealldevs f_pcap_freealldevs
#define pcap_open f_pcap_open
#define pcap_setnonblock f_pcap_setnonblock
#define pcap_sendpacket f_pcap_sendpacket
#define pcap_next_ex f_pcap_next_ex
#define pcap_geterr f_pcap_geterr
#define pcap_compile f_pcap_compile
#define pcap_setfilter f_pcap_setfilter
#define pcap_close f_pcap_close
#endif // _WIN32

class CNetworkPcap : public CNetworkBackend {
public:
  CNetworkPcap();
  virtual ~CNetworkPcap();

  virtual bool init(const char *devid_string, CConfigurator *cfg);
  virtual int send(const u8 *data, int len);
  virtual int receive(const u8 **data, int *len);
  virtual void set_filter(const NetworkFilter &filter);
  virtual void close();

private:
  pcap_t *fp;
  struct bpf_program fcode;
  bool opened;
};

#endif // !defined(INCLUDED_NETWORK_PCAP_H)
