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

/**
 * \file
 * Contains the definitions for the emulated DEC 21143 NIC device.
 **/
#if !defined(INCLUDED_DEC21143_H_)
#define INCLUDED_DEC21143_H_

#include "DEC21143_mii.hpp"
#include "DEC21143_tulipreg.hpp"
#include "PCIDevice.hpp"
#if defined(WIN32)
#define HAVE_REMOTE
#include <winsock.h>
#else
#include <pcap.h>
#endif
#include "Ethernet.hpp"
#include "base/Semaphore.hpp"

#include <mutex>

#if defined(WIN32)
typedef int bpf_int32;
typedef unsigned int bpf_u_int32;

/*
 * The instruction data structure.
 */
struct bpf_insn {
  unsigned short code;
  unsigned char jt;
  unsigned char jf;
  bpf_u_int32 k;
};

/*
 * Structure for "pcap_compile()", "pcap_setfilter()", etc..
 */
struct bpf_program {
  unsigned int bf_len;
  struct bpf_insn *bf_insns;
};

typedef struct pcap_if pcap_if_t;

#define PCAP_ERRBUF_SIZE 256

struct pcap_pkthdr {
  struct timeval ts;
  bpf_u_int32 caplen;
  bpf_u_int32 len;
};

struct pcap_if {
  struct pcap_if *next;
  char *name;
  char *description;
  void *addresses;
  bpf_u_int32 flags;
};

struct pcap_send_queue {
  unsigned int maxlen; /* Maximum size of the queue, in bytes. This
           variable contains the size of the buffer field. */
  unsigned int len;    /* Current size of the queue, in bytes. */
  char *buffer;        /* Buffer containing the packets to be sent. */
};

typedef struct pcap_send_queue pcap_send_queue;

typedef void (*pcap_handler)(unsigned char *user, const struct pcap_pkthdr *h,
                             const unsigned char *bytes);

typedef struct pcap pcap_t;
typedef struct pcap_dumper pcap_dumper_t;
typedef struct pcap_if pcap_if_t;
typedef struct pcap_addr pcap_addr_t;
#endif

/**
 * \brief Emulated DEC 21143 NIC device.
 *
 * Documentation consulted:
 *  - 21143 PCI/Cardbus 10/100Mb/s Ethernet LAN Controller Hardware Reference
 *Manual  [HRM]. (http://download.intel.com/design/network/manuals/27807401.pdf)
 *  - Tru64 Device Driver Kit Version 2 (Ethernet sample = tu driver!) [T64].
 *(http://h30097.www3.hp.com/docs/dev_doc/DOCUMENTATION/HTML/dev_docs_r2.html)
 *  .
 **/
class CDEC21143 : public CPCIDevice {
public:
  virtual int SaveState(FILE *f);
  virtual int RestoreState(FILE *f);
  void instant_tick();

  //    void interrupt(int number);
  virtual void check_state();
  virtual void WriteMem_Bar(int func, int bar, u32 address, int dsize,
                            u32 data);
  virtual u32 ReadMem_Bar(int func, int bar, u32 address, int dsize);

  CDEC21143(CConfigurator *confg, class CSystem *c, int pcibus, int pcidev);
  virtual ~CDEC21143();
  virtual void ResetPCI();
  void ResetNIC();
  void SetupFilter();
  void receive_process();
  virtual void run();
  virtual void init();
  virtual void start_threads();
  virtual void stop_threads();
  void update_irq();

private:
  static int nic_num;

  std::unique_ptr<std::thread> myThread;
  std::atomic_bool myThreadDead{false};
  bool StopThread;
  CSemaphore mySemaphore;
  /** serializes NIC thread vs CPU-thread CSR access (recursive: nic_read /
   * nic_write paths may re-enter through the interrupt plumbing) */
  std::recursive_mutex myRegLock;

  u32 nic_read(u32 address, int dsize);
  void nic_write(u32 address, int dsize, u32 data);
  void mii_access(uint32_t oldreg, uint32_t idata);
  void srom_access(uint32_t oldreg, uint32_t idata);
  void complete_sia_autoneg();
  void trace_packet(const char *dir, const u8 *frame, int len);

  int dec21143_rx();
  int dec21143_tx();
  void set_tx_state(int tx_state);
  void set_rx_state(int rx_state);

  inline u32 bswap32_local(u32 v);

  CPacketQueue *rx_queue;
  pcap_t *fp;
  struct bpf_program fcode;
  bool calc_crc;
  bool trace_packets;

  /// The state structure contains all elements that need to be saved to the
  /// statefile.
  struct SNIC_state {
    bool irq_was_asserted; /**< remember state of IRQ */

    u8 mac[6];            /**< ethernet address */
    u8 setup_filter[192]; /**< filter for perfect filtering */
    int descr_skip;       // Descriptor Skip Length [DSL] (in bytes)

    /// SROM emulation
    struct SNIC_srom {
      u8 data[1 << (7)];
      int curbit;
      int opcode;
      int opcode_has_started;
      int addr;
    } srom;

    /// MII PHY emulation
    struct SNIC_mii {
      u16 phy_reg[MII_NPHY * 32];
      int state;
      int bit;
      int opcode;
      int phyaddr;
      int regaddr;
    } mii;

    u32 reg[32]; /**< 21143 registers */

    /// Internal TX state
    struct SNIC_tx {
      u32 cur_addr;
      unsigned char *cur_buf;
      int cur_buf_len;
      bool suspend;
    } tx;

    /// Internal RX state
    struct SNIC_rx {
      u32 cur_addr;
      unsigned char *cur_buf;
      int cur_buf_len;
      int cur_offset;
      eth_packet current;
    } rx;
  } state;
};
#endif // !defined(INCLUDED_DEC21143_H_)
