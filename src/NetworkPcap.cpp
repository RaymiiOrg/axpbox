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

#include "StdAfx.hpp"

#if defined(HAVE_PCAP)
#include "Configurator.hpp"
#include "NetworkPcap.hpp"

#if defined(_WIN32)
int (*f_pcap_findalldevs)(pcap_if_t **, char *);
void (*f_pcap_freealldevs)(pcap_if_t *);
pcap_t *(*f_pcap_open)(const char *, int, int, int, struct pcap_rmtauth *,
                       char *);
int (*f_pcap_setnonblock)(pcap_t *, int, char *);
int (*f_pcap_sendpacket)(pcap_t *, const unsigned char *, int);
int (*f_pcap_next_ex)(pcap_t *, struct pcap_pkthdr **, const unsigned char **);
char *(*f_pcap_geterr)(pcap_t *);
int (*f_pcap_compile)(pcap_t *, struct bpf_program *, const char *, int,
                      bpf_u_int32);
int (*f_pcap_setfilter)(pcap_t *, struct bpf_program *);
void (*f_pcap_close)(pcap_t *);

bool load_wpcap() {
  static HMODULE libhandle = nullptr;
  if (libhandle)
    return true;

  /* Npcap installs wpcap.dll in System32\Npcap, which is not on the default
   * DLL search path unless WinPcap compatibility mode was selected. */
  char npcap_dir[512];
  GetSystemDirectoryA(npcap_dir, 480);
  strcat(npcap_dir, "\\Npcap");
  SetDllDirectoryA(npcap_dir);
  libhandle = LoadLibraryA("wpcap.dll");
  SetDllDirectoryA(NULL); /* reset the DLL search path */
  if (!libhandle)
    return false;

#define LOAD_PCAP_FN(name)                                                     \
  f_##name = (decltype(f_##name))GetProcAddress(libhandle, #name)
  LOAD_PCAP_FN(pcap_findalldevs);
  LOAD_PCAP_FN(pcap_freealldevs);
  LOAD_PCAP_FN(pcap_open);
  LOAD_PCAP_FN(pcap_setnonblock);
  LOAD_PCAP_FN(pcap_sendpacket);
  LOAD_PCAP_FN(pcap_next_ex);
  LOAD_PCAP_FN(pcap_geterr);
  LOAD_PCAP_FN(pcap_compile);
  LOAD_PCAP_FN(pcap_setfilter);
  LOAD_PCAP_FN(pcap_close);
#undef LOAD_PCAP_FN

  return f_pcap_findalldevs && f_pcap_freealldevs && f_pcap_open &&
         f_pcap_setnonblock && f_pcap_sendpacket && f_pcap_next_ex &&
         f_pcap_geterr && f_pcap_compile && f_pcap_setfilter && f_pcap_close;
}
#endif // _WIN32

CNetworkPcap::CNetworkPcap() : fp(nullptr), opened(false) {
  memset(&fcode, 0, sizeof(fcode));
}

CNetworkPcap::~CNetworkPcap() { close(); }

bool CNetworkPcap::init(const char *devid_string, CConfigurator *cfg) {
  pcap_if_t *alldevs;
  pcap_if_t *d;
  u_int inum;
  u_int i = 0;
  char errbuf[PCAP_ERRBUF_SIZE];

#if defined(_WIN32)
  if (!load_wpcap()) {
    printf("%s: failed to load wpcap.dll; is Npcap installed?\n", devid_string);
    return false;
  }
#endif

  char *adapter = cfg->get_text_value("adapter");
  if (!adapter) {
    printf("\n%s: Choose a network adapter to connect to:\n", devid_string);
    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
      printf("%s: Error in pcap_findalldevs: %s\n", devid_string, errbuf);
      return false;
    }

    for (d = alldevs; d; d = d->next) {
      printf("%d. %s\n    ", ++i, d->name);
      if (d->description)
        printf(" (%s)\n", d->description);
      else
        printf(" (No description available)\n");
    }

    if (i == 0) {
      printf("%s: No network interfaces found\n", devid_string);
      return false;
    }

    if (i == 1)
      inum = 1;
    else {
      for (;;) {
        char input_buf[64];
        int parsed;

        printf("%%NIC-Q-NICNO: Enter the interface number (1-%d): ", i);
        fflush(stdout);

        if (fgets(input_buf, sizeof(input_buf), stdin) == NULL) {
          printf("%s: unexpected end of input while selecting network "
                 "interface\n",
                 devid_string);
          return false;
        }

        if (sscanf(input_buf, "%d", &parsed) != 1 || parsed < 1 ||
            (u_int)parsed > i) {
          printf("%%NIC-W-BADSEL: Invalid selection. Please enter a number "
                 "between 1 and %d.\n",
                 i);
          continue;
        }

        inum = (u_int)parsed;
        break;
      }
    }

    for (d = alldevs, i = 0; i < inum - 1; d = d->next, i++)
      ;

    adapter = d->name;
  }

#if defined(WIN32)
  // Opening with pcap_open on Windows allows specification of
  // PCAP_OPENFLAG_NOCAPTURE_LOCAL, which stops the pcap device from seeing
  // its own transmitted packets. Real ethernet cards don't reflect packets,
  // and DECNET Phase IV panics on startup when it sees its own address on
  // the wire. Loopback packets are handled via direct entry in the receive
  // queue instead.
  if ((fp = pcap_open(adapter, 65536,
                      PCAP_OPENFLAG_PROMISCUOUS | PCAP_OPENFLAG_NOCAPTURE_LOCAL,
                      10, 0, errbuf)) == NULL)
#else
  if ((fp = pcap_open_live(adapter, 65536, 1, 1, errbuf)) == nullptr)
#endif
  {
    printf("%s: Error opening adapter %s: %s\n", devid_string, adapter, errbuf);
    return false;
  }

  if (pcap_setnonblock(fp, 1, errbuf) == PCAP_ERROR) {
    printf("%s: Error setting adapter %s non-blocking: %s\n", devid_string,
           adapter, errbuf);
    pcap_close(fp);
    fp = nullptr;
    return false;
  }

  opened = true;
  printf("%s: Using pcap adapter %s\n", devid_string, adapter);
  return true;
}

int CNetworkPcap::send(const u8 *data, int len) {
  if (!fp)
    return -1;
  if (pcap_sendpacket(fp, data, len)) {
    printf("Error sending the packet: %s\n", pcap_geterr(fp));
    return -1;
  }
  return 0;
}

int CNetworkPcap::receive(const u8 **data, int *len) {
  if (!fp)
    return -1;
  struct pcap_pkthdr *packet_header;
  const u_char *packet_data = NULL;
  int res = pcap_next_ex(fp, &packet_header, &packet_data);
  if (res > 0) {
    *data = packet_data;
    *len = packet_header->caplen;
    return 1;
  }
  if (res == 0)
    return 0; // timeout, no packet
  return -1;  // error
}

void CNetworkPcap::set_filter(const NetworkFilter &f) {
  if (!fp)
    return;

  char mac_txt[16][20];
  char filter[1000];
  int numUnique = 0;
  int unique[16];
  int i, j;

  /* Deduplicate the non-empty perfect-filter entries. */
  for (i = 0; i < 16; i++) {
    if ((f.mac_list[i][0] | f.mac_list[i][1] | f.mac_list[i][2] |
         f.mac_list[i][3] | f.mac_list[i][4] | f.mac_list[i][5]) == 0)
      continue;

    bool u = true;
    for (j = 0; j < numUnique; j++) {
      if (memcmp(f.mac_list[i], f.mac_list[unique[j]], 6) == 0) {
        u = false;
        break;
      }
    }
    if (u)
      unique[numUnique++] = i;
  }

  for (j = 0; j < numUnique; j++) {
    i = unique[j];
    sprintf(mac_txt[j], "%02x:%02x:%02x:%02x:%02x:%02x", f.mac_list[i][0],
            f.mac_list[i][1], f.mac_list[i][2], f.mac_list[i][3],
            f.mac_list[i][4], f.mac_list[i][5]);
  }

  /* Build BPF per CSR6[PR,PM,IF,RA]; always allow broadcasts. */
  strcpy(filter, "ether broadcast");

  if (f.promiscuous || f.receive_all) {
    /* Accept everything: an empty expression compiles to match-all. */
    filter[0] = '\0';
  } else {
    if (f.pass_multicast)
      strcat(filter, " or ether multicast");

    char list[800];
    list[0] = '\0';
    if (numUnique == 0) {
      /* Fall back to our own MAC if no setup-frame was processed yet. */
      char self[20];
      sprintf(self, "%02x:%02x:%02x:%02x:%02x:%02x", f.own_mac[0], f.own_mac[1],
              f.own_mac[2], f.own_mac[3], f.own_mac[4], f.own_mac[5]);
      strcat(list, "ether dst ");
      strcat(list, self);
    } else {
      for (j = 0; j < numUnique; j++) {
        strcat(list, (j == 0) ? "ether dst " : " or ether dst ");
        strcat(list, mac_txt[j]);
      }
    }

    if (f.inverse) {
      strcat(filter, " or (not (");
      strcat(filter, list);
      strcat(filter, "))");
    } else {
      strcat(filter, " or (");
      strcat(filter, list);
      strcat(filter, ")");
    }
  }

  if (pcap_compile(fp, &fcode, filter, 1, 0xffffffff) < 0) {
    printf("Unable to compile the packet filter (%s)\n", filter);
    return;
  }

  if (pcap_setfilter(fp, &fcode) < 0)
    printf("Error setting the filter.\n");
}

void CNetworkPcap::close() {
  if (fp) {
    pcap_close(fp);
    fp = nullptr;
  }
  opened = false;
}

#endif // HAVE_PCAP
