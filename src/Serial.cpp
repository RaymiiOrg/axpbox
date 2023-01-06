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
 * Contains the code for the emulated Serial Port devices.
 **/
#include "Serial.hpp"
#include "AliM1543C.hpp"
#include "StdAfx.hpp"
#include "System.hpp"

#include "lockstep.hpp"

#define RECV_TICKS 10

int iCounter = 0;

#define FIFO_SIZE 1024

//#define DEBUG_SERIAL 1

/**
 * Constructor.
 **/
CSerial::CSerial(CConfigurator *cfg, CSystem *c, u16 number)
    : CSystemComponent(cfg, c) {
  state.iNumber = number;
  breakHit = false;
}

/**
 * Initialize the Serial port device.
 **/
void CSerial::init() {
  listenPort = (int)myCfg->get_num_value("port", false, 8000 + state.iNumber);
  if (!(listenAddress = myCfg->get_text_value("address"))) {
    listenAddress = "0.0.0.0";
  }
  cSystem->RegisterMemory(this, 0,
                          U64(0x00000801fc0003f8) - (0x100 * state.iNumber), 8);

  // Start Telnet server
#if defined(_WIN32)
  // Windows Sockets only work after calling WSAStartup.
  WSADATA wsa;
  WSAStartup(0x0101, &wsa);
#endif // defined (_WIN32)
  struct sockaddr_in Address;

  listenSocket = (int)socket(AF_INET, SOCK_STREAM, 0);
  if (listenSocket == INVALID_SOCKET) {
    printf("Could not open socket to listen on!\n");
  }

  inet_aton(listenAddress, (in_addr *) &Address.sin_addr.s_addr);
  Address.sin_port = htons((u16)(listenPort));
  Address.sin_family = AF_INET;

  int optval = 1;
  setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&optval,
             sizeof(optval));
  bind(listenSocket, (struct sockaddr *)&Address, sizeof(Address));
  listen(listenSocket, 1);

  printf("%s: Waiting for connection on port %d.\n", devid_string, listenPort);

  WaitForConnection();

#if defined(IDB) && defined(LS_MASTER)
  struct sockaddr_in dest_addr;
  int result = -1;

  throughSocket = socket(AF_INET, SOCK_STREAM, 0);

  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons((u16)(base + number));
  dest_addr.sin_addr.s_addr = inet_addr(ls_IP);

  printf("%s: Waiting to initiate remote connection to %s.\n", devid_string,
         ls_IP);

  while (result == -1) {
    result = connect(throughSocket, (struct sockaddr *)&dest_addr,
                     sizeof(struct sockaddr));
  }
#endif
  state.rcvW = 0;
  state.rcvR = 0;

  state.bLCR = 0x00;
  state.bLSR = 0x60; // THRE, TSRE
  state.bMSR = 0x30; // CTS, DSR
  state.bIIR = 0x01; // no interrupt
  state.irq_active = false;

  printf("%s: $Id: Serial.cpp,v 1.51 2008/06/03 09:07:56 iamcamiel Exp $\n",
         devid_string);
}

void CSerial::start_threads() {
  char buffer[5];
  if (!myThread) {
    sprintf(buffer, "srl%d", state.iNumber);
    printf(" %s", buffer);
    StopThread = false;
    myThread = std::make_unique<std::thread>([this](){ this->run(); });
  }
}

void CSerial::stop_threads() {
  char buffer[5];
  StopThread = true;
  if (myThread) {
    sprintf(buffer, "srl%d", state.iNumber);
    printf(" %s", buffer);
    if (!acceptingSocket) {
      myThread->join();
    }
    myThread = nullptr;
  }
}

/**
 * Destructor.
 **/
CSerial::~CSerial() { stop_threads(); }

u64 CSerial::ReadMem(int index, u64 address, int dsize) {
  u8 d;

  switch (address) {
  case 0: // data buffer
    if (state.bLCR & 0x80) {
      return state.bBRB_LSB;
    } else {
      if (state.rcvR != state.rcvW) {
        state.bRDR = state.rcvBuffer[state.rcvR];
        state.rcvR++;
        if (state.rcvR == FIFO_SIZE)
          state.rcvR = 0;
        TRC_DEV4("Read character %02x (%c) on serial port %d\n", state.bRDR,
                 printable(state.bRDR), state.iNumber);
#if defined(DEBUG_SERIAL)
        printf("Read character %02x (%c) on serial port %d\n", state.bRDR,
               printable(state.bRDR), state.iNumber);
#endif
      } else {
        TRC_DEV2("Read past FIFO on serial port %d\n", state.iNumber);
#if defined(DEBUG_SERIAL)
        printf("Read past FIFO on serial port %d\n", state.iNumber);
#endif
      }

      return state.bRDR;
    }

  case 1:
    if (state.bLCR & 0x80) {
      return state.bBRB_MSB;
    } else {
      return state.bIER;
    }

  case 2: // interrupt cause
    d = state.bIIR;
    state.bIIR = 0x01;
    return d;

  case 3:
    return state.bLCR;

  case 4:
    return state.bMCR;

  case 5: // serialization state
    if (state.rcvR != state.rcvW)
      state.bLSR = 0x61; // THRE, TSRE, RxRD
    else
      state.bLSR = 0x60; // THRE, TSRE
    return state.bLSR;

  case 6:
    return state.bMSR;

  default:
    return state.bSPR;
  }
}

void CSerial::WriteMem(int index, u64 address, int dsize, u64 data) {
  u8 d;
  char s[5];
  d = (u8)data;

  switch (address) {
  case 0:
    if (state.bLCR & 0x80) // divisor latch access bit set
    {

      // LSB of divisor latch
      state.bBRB_LSB = d;
    } else {

      // Transmit Hold Register
      sprintf(s, "%c", d);
      write(s);
      TRC_DEV4("Write character %02x (%c) on serial port %d\n", d, printable(d),
               state.iNumber);
#if defined(DEBUG_SERIAL)
      printf("Write character %02x (%c) on serial port %d\n", d, printable(d),
             state.iNumber);
#endif
      eval_interrupts();
    }
    break;

  case 1:
    if (state.bLCR & 0x80) // divisor latch access bit set
    {

      // MSB of divisor latch
      state.bBRB_MSB = d;
    } else {

      // Interrupt Enable Register
      state.bIER = d;
      eval_interrupts();
    }
    break;

  case 2:
    state.bFCR = d;
    break;

  case 3:
    state.bLCR = d;
    break;

  case 4:
    state.bMCR = d;
    break;

  default:
    state.bSPR = d;
  }
}

void CSerial::eval_interrupts() {
  state.bIIR = 0x01; // no interrupt
  if ((state.bIER & 0x01) && (state.rcvR != state.rcvW))
    state.bIIR = 0x04;
  else if (state.bIER & 0x2) // transmitter buffer empty enabled?
    state.bIIR = 0x02;       // transmitter buffer empty
  else
    state.bIIR = 0x01; // no interrupt
  if (state.bIIR > 0x01) {
    if (!state.irq_active)
      theAli->pic_interrupt(0, 4 - state.iNumber);
  } else {
    if (state.irq_active)
      theAli->pic_deassert(0, 4 - state.iNumber);
  }
}

void CSerial::write(const char *s) {
  send(connectSocket, s, (int)strlen(s) + 1, 0);
}

void CSerial::receive(const char *data) {
  char *x;

  x = (char *)data;

  while (*x) {
    state.rcvBuffer[state.rcvW++] = *x;
    if (state.rcvW == FIFO_SIZE)
      state.rcvW = 0;
    x++;
    eval_interrupts();
  }
}

/**
 * Thread entry point.
 **/
void CSerial::run() {
  try {
    for (;;) {
      if (StopThread)
        return;
      execute();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  catch (CException &e) {
    printf("Exception in Serial thread: %s.\n", e.displayText().c_str());
    myThreadDead.store(true);
    // Let the thread die...
  }
}

/**
 * Check if threads are still running.
 *
 * Enter serial port menu if <break> was received.
 **/
void CSerial::check_state() {
  if (breakHit)
    serial_menu();

  if (myThreadDead.load())
    FAILURE(Thread, "Serial thread has died");
}

void CSerial::serial_menu() {
  fd_set readset;
  unsigned char buffer[FIFO_SIZE + 1];
  ssize_t size;
  struct timeval tv;
  bool exitLoop = false;

  cSystem->stop_threads();

  write("\r\n<BREAK> received. What do you want to do?\r\n");
  write("     0. Continue\r\n");
#if defined(IDB)
  write("     1. End run\r\n");
#else
  write("     1. Exit emulator gracefully\r\n");
  write("     2. Abort emulator (no changes saved)\r\n");
  write("     3. Save state to autosave.axp and continue\r\n");
  write("     4. Load state from autosave.axp and continue\r\n");
#endif
  while (!exitLoop) {
    FD_ZERO(&readset);
    FD_SET(connectSocket, &readset);
    tv.tv_sec = 60;
    tv.tv_usec = 0;
    if (select(connectSocket + 1, &readset, NULL, NULL, &tv) <= 0) {
      write("%SRL-I-TIMEOUT: no timely answer received. Continuing "
            "emulation.\r\n");
      break; // leave loop
    }

#if defined(_WIN32) || defined(__VMS)
    size = recv(connectSocket, (char *)buffer, FIFO_SIZE, 0);
#else
    size = read(connectSocket, &buffer, FIFO_SIZE);
#endif
    switch (buffer[0]) {
    case '0':
      write("%SRL-I-CONTINUE: continuing emulation.\r\n");
      exitLoop = true;
      break;

    case '1':
      write("%SRL-I-EXIT: exiting emulation gracefully.\r\n");
      FAILURE(Graceful, "Graceful exit");
      exitLoop = true;
      break;

    case '2':
      write("%SRL-I-ABORT: aborting emulation.\r\n");
      FAILURE(Abort, "Aborting");
      exitLoop = true;
      break;

    case '3':
      write("%SRL-I-SAVESTATE: Saving state to autosave.axp.\r\n");
      cSystem->SaveState("autosave.axp");
      write("%SRL-I-CONTINUE: continuing emulation.\r\n");
      exitLoop = true;
      break;

    case '4':
      write("%SRL-I-LOADSTATE: Loading state from autosave.axp.\r\n");
      cSystem->RestoreState("autosave.axp");
      write("%SRL-I-CONTINUE: continuing emulation.\r\n");
      exitLoop = true;
      break;

    default:
      write("%SRL-W-INVALID: Not a valid answer.\r\n");
    }
  }

  breakHit = false;
  cSystem->start_threads();
}

void CSerial::execute() {
  fd_set readset;
  unsigned char buffer[FIFO_SIZE + 1];
  unsigned char cbuffer[FIFO_SIZE + 1]; // cooked buffer
  unsigned char *b;

  // cooked buffer
  unsigned char *c;
  ssize_t size;
  struct timeval tv;

  state.serial_cycles++;
  if (state.serial_cycles >= RECV_TICKS) {
    FD_ZERO(&readset);
    FD_SET(connectSocket, &readset);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    if (select(connectSocket + 1, &readset, NULL, NULL, &tv) > 0) {
#if defined(_WIN32) || defined(__VMS)

      // Windows Sockets has no direct equivalent of BSD's read
      size = recv(connectSocket, (char *)buffer, FIFO_SIZE, 0);
#else
      size = read(connectSocket, &buffer, FIFO_SIZE);
#endif

      extern int got_sigint;
      if (size == 0 && !got_sigint) {
        printf("%%SRL-W-DISCONNECT: Write socket closed on other end for "
               "serial port %d.\n",
               state.iNumber);
        printf("-SRL-I-WAITFOR: Waiting for a new connection on port %d.\n",
               listenPort);
        WaitForConnection();
        return;
      }

      buffer[size + 1] = 0; // force null termination.
      b = buffer;
      c = cbuffer;
      while ((ssize_t)(b - buffer) < size) {
        if (*b == 0x0a) {
          b++; // skip LF
          continue;
        }

        if (*b == IAC) {
          if (*(b + 1) == IAC) { // escaped IAC.
            b++;
          } else if (*(b + 1) >= WILL) { // will/won't/do/don't
            b += 3; // skip this byte, and following two. (telnet escape)
            continue;
          } else if (*(b + 1) == SB) { // skip until IAC SE
            b += 2;                    // now we're at start of subnegotiation.
            while (*b != IAC && *(b + 1) != SE)
              b++;
            b += 2;
            continue;
          } else if (*(b + 1) == BREAK) { // break (== halt button?)
            b += 2;
            breakHit = true;
          } else if (*(b + 1) == AYT) { // are you there?
          } else {                      // misc single byte command.
            b += 2;
            continue;
          }
        }

        *c = *b;
        c++;
        b++;
      }

      *c = 0; // null terminate it.
      this->receive((const char *)&cbuffer);
    }

    state.serial_cycles = 0;
  }

  eval_interrupts();
}

static u32 srl_magic1 = 0x5A15A15A;
static u32 srl_magic2 = 0x1A51A51A;

/**
 * Save state to a Virtual Machine State file.
 **/
int CSerial::SaveState(FILE *f) {
  long ss = sizeof(state);

  fwrite(&srl_magic1, sizeof(u32), 1, f);
  fwrite(&ss, sizeof(long), 1, f);
  fwrite(&state, sizeof(state), 1, f);
  fwrite(&srl_magic2, sizeof(u32), 1, f);
  printf("%s: %d bytes saved.\n", devid_string, (int)ss);
  return 0;
}

/**
 * Restore state from a Virtual Machine State file.
 **/
int CSerial::RestoreState(FILE *f) {
  long ss;
  u32 m1;
  u32 m2;
  size_t r;

  r = fread(&m1, sizeof(u32), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (m1 != srl_magic1) {
    printf("%s: MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  r = fread(&ss, sizeof(long), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (ss != sizeof(state)) {
    printf("%s: STRUCT SIZE does not match!\n", devid_string);
    return -1;
  }

  r = fread(&state, sizeof(state), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  r = fread(&m2, sizeof(u32), 1, f);
  if (r != 1) {
    printf("%s: unexpected end of file!\n", devid_string);
    return -1;
  }

  if (m2 != srl_magic2) {
    printf("%s: MAGIC 1 does not match!\n", devid_string);
    return -1;
  }

  printf("%s: %d bytes restored.\n", devid_string, (int)ss);
  return 0;
}

void CSerial::WaitForConnection() {
  struct sockaddr_in Address;
  socklen_t nAddressSize = sizeof(struct sockaddr_in);
  const char *telnet_options = "%c%c%c";
  char buffer[8];
  char s[1000];
  char *nargv = s;
  int i = 0;

#if !defined(LS_SLAVE)
  char s2[200];
  char *argv[20];

  strncpy(s, myCfg->get_text_value("action", ""), 999);
  s[999] = '\0';

  // printf("%s: Specified : %s\n",devid_string,s);
  if (strcmp(s, "")) {

    // spawn external program (telnet client)...
    while (*nargv) {
      argv[i] = nargv;
      if (nargv[0] == '\"')
        nargv = strchr(nargv + 1, '\"');
      if (nargv)
        nargv = strchr(nargv, ' ');
      if (!nargv)
        break;
      *nargv++ = '\0';
      i++;
      argv[i] = NULL;
    }

    argv[i + 1] = NULL;
    strcpy(s2, argv[0]);
    nargv = s2;
    if (nargv[0] == '\"') {
      nargv++;
      *(strchr(nargv, '\"')) = '\0';
    }

    // printf("%s: Starting %s\n", devid_string,nargv);
#if defined(_WIN32)
    if (_spawnvp(_P_NOWAIT, nargv, argv) < 0)
      FAILURE_1(Runtime, "Exec of '%s' has failed.\n", argv[0]);
#elif !defined(__VMS)
    pid_t child;
    int status;
    if (!(child = fork())) {
      execvp(argv[0], argv);
      FAILURE_1(Runtime, "Exec of '%s' failed.\n", argv[0]);
    } else {
      sleep(1);                         // give it a chance to start up.
      waitpid(child, &status, WNOHANG); // reap it, if needed.
      if (kill(child, 0) < 0) {         // uh oh, no kiddo.
        FAILURE_1(Runtime, "Exec of '%s' has failed.\n", argv[0]);
      }
    }
#endif
  }
#endif
  Address.sin_addr.s_addr = INADDR_ANY;
  Address.sin_port = htons((u16)listenPort);
  Address.sin_family = AF_INET;

  //  Wait until we have a connection
  connectSocket = INVALID_SOCKET;
  while (connectSocket == INVALID_SOCKET) {
    acceptingSocket = true;
    connectSocket =
        (int)accept(listenSocket, (struct sockaddr *)&Address, &nAddressSize);
    acceptingSocket = false;
  }

  state.serial_cycles = 0;

  // Send some control characters to the telnet client to handle
  // character-at-a-time mode.
  sprintf(buffer, telnet_options, IAC, DO, TELOPT_ECHO);
  this->write(buffer);

  sprintf(buffer, telnet_options, IAC, DO, TELOPT_NAWS);
  write(buffer);

  sprintf(buffer, telnet_options, IAC, DO, TELOPT_LFLOW);
  this->write(buffer);

  sprintf(buffer, telnet_options, IAC, WILL, TELOPT_ECHO);
  this->write(buffer);

  sprintf(buffer, telnet_options, IAC, WILL, TELOPT_SGA);
  this->write(buffer);

  sprintf(s, "This is serial port #%d on ES40 Emulator\r\n", state.iNumber);
  this->write(s);
}
