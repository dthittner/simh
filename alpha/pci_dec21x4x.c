/* pci_dec21x4x.c: PCI Ethernet adapter, based on DEC/Intel 21x4x "Tulip" chips
  ------------------------------------------------------------------------------

   Copyright (c) 2016, David T. Hittner

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of the author shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author.

  ------------------------------------------------------------------------------

  The Digital/Intel 21x4x "Tulip" PCI Ethernet boards were designed by Digital,
  and the intellectual property was sold to Intel at some point during the life.
  Digital-designed/produced chips are the 2104x series, and the Intel-
  designed/produced chips are the 2114x series.

  The overall behavior of the 21x4x chips was based upon the "perfected"
  behavior of Digital's DEQNA/DELQA/DELQA-T Qbus boards, which incorporate
  an almost identical register structure (other than the PCI interface),
  and an almost identical memory transmit/receive ring behavior.

  The DEQNA/DELQA/DELQA-T simulation (in PDP11/pdp11_xq.{h,c}) is based on:
    Digital DELQA Users Guide, Part# EK-DELQA-UG-002
    Digital DEQNA Users Guide, Part# EK-DEQNA-UG-001
    Digital DELQA-Plus Addendum to DELQA Users Guide, Part# EK-DELQP-UG-001_Sep89.pdf 
  Those manuals can be found online at:
    http://www.bitsavers.org/pdf/dec/qbus

  The 21x4x simulation is based on:
    21040 manual [name and citation needed]
        DE425
        DE434
        DE435
    21041 manual [name and citation needed]
        DE450
    21140 manual  [name and citation needed]
        DE500-XA = rev 02000011 and 02000012 (21140) (media autosense, no duplex autosense)
        DE500-AA = rev 02000020 and 02000022 (21140A) (full autosense)
    21142/21143 manual [name and citation needed]
        DE500-BA = rev 02000030 and 02000041

  Some DExxx variation documentation:
    http://hoffmanlabs.org/vmsfaq/vmsfaq_025.html, Sec 14.23
    http://h20564.www2.hpe.com/hpsc/doc/public/display?docId=emr_na-c01676993&sp4ts.oid=3251162

  Certain adaptations have been made because this is an emulation:
    Ethernet transceiver power flag CSR<12> is ON when attached.
    External Loopback does not go out to the physical adapter, it is
      implemented more like an extended Internal Loopback
    Time Domain Reflectometry (TDR) numbers are faked
    The 10-second approx. hardware/software reset delay does not exist
    Some physical ethernet receive events like Runts, Overruns, etc. are
      never reported back, since the host's packet-level driver discards
      them and never passes them on to the simulation

  Certain advantages are derived from this emulation:
    If the host ethernet controller is faster than 10Mbit/sec, the speed is
      seen by the simulated cpu since there are no minimum response times.

  Regression Tests to be run:
    Alpha:  1. Console show (>>>SHOW DEVICE) [xxxx/xx/xx]
            2. VMS v8.4 boots/initializes/shows device [xxxx/xx/xx]
            3. VMS DECNET - SET HOST and COPY tests [xxxx/xx/xx]
            4. VMS TCPIP  - SET HOST/TELNET and FTP tests [xxxx/xx/xx]
            5. VMS LAT    - SET HOST/LAT tests [xxxx/xx/xx]
            6. VMS Cluster - SHOW CLUSTER, SHOW DEVICE, and cluster COPY tests [xxxx/xx/xx]
            7. Console boot into VMSCluster (>>>B EWAO) [xxxx/xx/xx]

  ------------------------------------------------------------------------------

  Modification history:

  2016-04-20  DTH  Changed XQ to EW, changed/tested default MAC, tested show ew eth
  2016-04-11  DTH  Copied/Modified from PDP11_XQ.C, started diff analysis

  ------------------------------------------------------------------------------
*/

#include <assert.h>
#include "alpha_defs.h"
#include "sim_pci.h"
#include "pci_dec21x4x.h"

//#include "pdp11_xq.h"
//#include "pdp11_ew_bootrom.h"


/* debugging bitmaps */
#define DBG_TRC  0x0001                                 /* trace routine calls */
#define DBG_REG  0x0002                                 /* trace read/write registers */
#define DBG_CSR  0x0004                                 /* watch CSR */
#define DBG_VAR  0x0008                                 /* watch VAR */
#define DBG_WRN  0x0010                                 /* display warnings */
#define DBG_SAN  0x0020                                 /* display sanity timer info */
#define DBG_SET  0x0040                                 /* display setup info */
#define DBG_PCK  0x0080                                 /* display packet headers */
#define DBG_DAT  0x0100                                 /* display packet data */
#define DBG_ETH  0x8000                                 /* debug ethernet device */

extern int32 tmxr_poll;
extern int32 tmr_poll, clk_tps;
extern char* read_line (char *ptr, int32 size, FILE *stream);

/* forward declarations */
t_stat ew_rd(int32* data, int32 PA, int32 access);
t_stat ew_wr(int32  data, int32 PA, int32 access);
t_stat ew_svc(UNIT * uptr);
t_stat ew_tmrsvc(UNIT * uptr);
t_stat ew_reset (DEVICE * dptr);
t_stat ew_attach (UNIT * uptr, char * cptr);
t_stat ew_detach (UNIT * uptr);
t_stat ew_showmac (FILE* st, UNIT* uptr, int32 val, void* desc);
t_stat ew_setmac  (UNIT* uptr, int32 val, char* cptr, void* desc);
t_stat ew_show_filters (FILE* st, UNIT* uptr, int32 val, void* desc);
t_stat ew_show_stats (FILE* st, UNIT* uptr, int32 val, void* desc);
t_stat ew_set_stats  (UNIT* uptr, int32 val, char* cptr, void* desc);
t_stat ew_show_type (FILE* st, UNIT* uptr, int32 val, void* desc);
t_stat ew_set_type (UNIT* uptr, int32 val, char* cptr, void* desc);
t_stat ew_show_sanity (FILE* st, UNIT* uptr, int32 val, void* desc);
t_stat ew_set_sanity (UNIT* uptr, int32 val, char* cptr, void* desc);
t_stat ew_show_throttle (FILE* st, UNIT* uptr, int32 val, void* desc);
t_stat ew_set_throttle (UNIT* uptr, int32 val, char* cptr, void* desc);
t_stat ew_show_lockmode (FILE* st, UNIT* uptr, int32 val, void* desc);
t_stat ew_set_lockmode (UNIT* uptr, int32 val, char* cptr, void* desc);
t_stat ew_show_poll (FILE* st, UNIT* uptr, int32 val, void* desc);
t_stat ew_set_poll (UNIT* uptr, int32 val, char* cptr, void* desc);
t_stat ew_show_leds (FILE* st, UNIT* uptr, int32 val, void* desc);
t_stat ew_process_xbdl(CTLR* xq);
t_stat ew_dispatch_xbdl(CTLR* xq);
t_stat ew_process_turbo_rbdl(CTLR* xq);
t_stat ew_process_turbo_xbdl(CTLR* xq);
void ew_start_receiver(CTLR* xq);
void ew_stop_receiver(CTLR* xq);
void ew_sw_reset(CTLR* xq);
t_stat ew_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw);
t_stat ew_dep (t_value val, t_addr addr, UNIT *uptr, int32 sw);
void ew_reset_santmr(CTLR* xq);
t_stat ew_boot_host(CTLR* xq);
t_stat ew_system_id(CTLR* xq, const ETH_MAC dst, uint16 receipt_id);
void ewa_read_callback(int status);
void ewb_read_callback(int status);
void ewa_write_callback(int status);
void ewb_write_callback(int status);
void ew_setint (CTLR* xq);
void ew_clrint (CTLR* xq);
int32 ew_int (void);
void ew_csr_set_clr(CTLR* xq, uint16 set_bits, uint16 clear_bits);
void ew_show_debug_bdl(CTLR* xq, uint32 bdl_ba);
t_stat ew_boot (int32 unitno, DEVICE *dptr);
t_stat ew_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
const char *ew_description (DEVICE *dptr);

struct ew_device    ewa = {
  ewa_read_callback,                        /* read callback routine */
  ewa_write_callback,                       /* write callback routine */
  {0x00, 0x00, 0xF8, 0xDE, 0x50, 0x0A},     /* mac */
  EW_T_DE500A,                              /* type */
  EW_T_DE500A,                              /* mode */
  XQ_SERVICE_INTERVAL,                      /* poll */
  0, 0,                                     /* coalesce */
  {0},                                      /* sanity */
  0,                                        /* DEQNA-Lock mode */
  ETH_THROT_DEFAULT_TIME,                   /* ms throttle window */
  ETH_THROT_DEFAULT_BURST,                  /* packet packet burst in throttle window */
  ETH_THROT_DISABLED_DELAY                  /* throttle disabled */
  };

struct ew_device    ewb = {
  ewb_read_callback,                        /* read callback routine */
  ewb_write_callback,                       /* write callback routine */
  {0x00, 0x00, 0xF8, 0xDE, 0x50, 0x0B},     /* mac */
  EW_T_DE500A,                              /* type */
  EW_T_DE500A,                              /* mode */
  XQ_SERVICE_INTERVAL,                      /* poll */
  0, 0,                                     /* coalesce */
  {0},                                      /* sanity */
  0,                                        /* DEQNA-Lock mode */
  ETH_THROT_DEFAULT_TIME,                   /* ms throttle window */
  ETH_THROT_DEFAULT_BURST,                  /* packet packet burst in throttle window */
  ETH_THROT_DISABLED_DELAY                  /* throttle disabled */
  };

/* SIMH device structures */

#define IOLN_XQ         020

DIB ewa_dib = { 0, 0, NULL, NULL,0};

UNIT ewa_unit[] = {
 { UDATA (&ew_svc, UNIT_IDLE|UNIT_ATTABLE|UNIT_DISABLE, 2047) },  /* receive timer */
 { UDATA (&ew_tmrsvc, UNIT_IDLE|UNIT_DIS, 0) },
};

BITFIELD ew_csr_bits[] = {
  BIT(RE), BIT(SR), BIT(NI), BIT(BD), BIT(XL), BIT(RL), BIT(IE), BIT(XI),
  BIT(IL), BIT(EL), BIT(SE), BIT(RR), BIT(OK), BIT(CA), BIT(PE), BIT(RI),
  ENDBITS
};

BITFIELD ew_var_bits[] = {
  BIT(ID), BIT(RR), BIT(V0), BIT(V1), BIT(V2), BIT(V3), BIT(V4), BIT(V5),
  BIT(V6), BIT(V7), BIT(S1), BIT(S2), BIT(S3), BIT(RS), BIT(OS), BIT(MS),
  ENDBITS
};

BITFIELD ew_srr_bits[] = {
  BIT(RS0), BIT(RS1), BITNC,    BITNC,    BITNC,    BITNC,    BITNC,    BITNC,
  BITNC,    BIT(TBL), BIT(IME), BIT(PAR), BIT(NXM), BITNC, BIT(CHN), BIT(FES),
  ENDBITS
};

REG ewa_reg[] = {
  { GRDATA ( SA0,  ewa.mac[0], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA ( SA1,  ewa.mac[1], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA ( SA2,  ewa.mac[2], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA ( SA3,  ewa.mac[3], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA ( SA4,  ewa.mac[4], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA ( SA5,  ewa.mac[5], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA ( MX0,  ewa.mac_checksum[0], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA ( MX1,  ewa.mac_checksum[1], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATAD ( RBDL, ewa.rbdl[0], XQ_RDX, 16, 0, "Receive BDL Address(low)"), REG_FIT },
  { GRDATAD ( RBDH, ewa.rbdl[1], XQ_RDX, 16, 0, "Receive BDL Address(high)"), REG_FIT },
  { GRDATAD ( XBDL, ewa.xbdl[0], XQ_RDX, 16, 0, "Transmit BDL Address(low)"), REG_FIT },
  { GRDATAD ( XBDH, ewa.xbdl[1], XQ_RDX, 16, 0, "Transmit BDL Address(high)"), REG_FIT },
  { GRDATADF ( VAR,  ewa.var,  XQ_RDX, 16, 0, "Vector Address Register ", ew_var_bits), REG_FIT },
  { GRDATADF ( CSR,  ewa.csr,  XQ_RDX, 16, 0, "Control and Status Register", ew_csr_bits), REG_FIT },
  { FLDATA ( INT,  ewa.irq, 0) },
  { GRDATA ( TYPE,  ewa.type,  XQ_RDX, 32, 0), REG_FIT },
  { GRDATA ( MODE,  ewa.mode,  XQ_RDX, 32, 0), REG_FIT },
  { GRDATA ( POLL, ewa.poll, XQ_RDX, 16, 0), REG_HRO},
  { GRDATA ( CLAT, ewa.coalesce_latency, XQ_RDX, 16, 0), REG_HRO},
  { GRDATA ( CLATT, ewa.coalesce_latency_ticks, XQ_RDX, 16, 0), REG_HRO},
  { GRDATA ( RBDL_BA, ewa.rbdl_ba, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( XBDL_BA, ewa.xbdl_ba, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SETUP_PRM, ewa.setup.promiscuous, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SETUP_MLT, ewa.setup.multicast, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SETUP_L1, ewa.setup.l1, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SETUP_L2, ewa.setup.l2, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SETUP_L3, ewa.setup.l3, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SETUP_SAN, ewa.setup.sanity_timer, XQ_RDX, 32, 0), REG_HRO},
  { BRDATA ( SETUP_MACS, &ewa.setup.macs, XQ_RDX, 8, sizeof(ewa.setup.macs)), REG_HRO},
  { BRDATA ( STATS, &ewa.stats, XQ_RDX, 8, sizeof(ewa.stats)), REG_HRO},
  { BRDATA ( TURBO_INIT, &ewa.init, XQ_RDX, 8, sizeof(ewa.init)), REG_HRO},
  { GRDATADF ( SRR,  ewa.srr,  XQ_RDX, 16, 0, "Status and Response Register", ew_srr_bits), REG_FIT },
  { GRDATAD ( SRQR,  ewa.srqr,  XQ_RDX, 16, 0, "Synchronous Request Register"), REG_FIT },
  { GRDATAD ( IBA,  ewa.iba,  XQ_RDX, 32, 0, "Init Block Address Register"), REG_FIT },
  { GRDATAD ( ICR,  ewa.icr,  XQ_RDX, 16, 0, "Interrupt Request Register"), REG_FIT },
  { GRDATA ( IPEND,  ewa.pending_interrupt,  XQ_RDX, 16, 0), REG_FIT },
  { GRDATA ( TBINDX, ewa.tbindx, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( RBINDX, ewa.rbindx, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( IDTMR, ewa.idtmr, XQ_RDX, 32, 0), REG_HRO},
//  { GRDATA ( VECTOR, ewa_dib.vec, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( MUST_POLL, ewa.must_poll, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SANT_ENAB, ewa.sanity.enabled, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SANT_QSECS, ewa.sanity.quarter_secs, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SANT_TIMR, ewa.sanity.timer, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( LOCKMODE, ewa.lockmode, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( THR_TIME, ewa.throttle_time, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( THR_BURST, ewa.throttle_burst, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( THR_DELAY, ewa.throttle_delay, XQ_RDX, 32, 0), REG_HRO},
  { NULL },
};

DIB ewb_dib = { 0, 0, NULL, NULL, 0 };

UNIT ewb_unit[] = {
 { UDATA (&ew_svc, UNIT_IDLE|UNIT_ATTABLE|UNIT_DISABLE, 2047) },  /* receive timer */
 { UDATA (&ew_tmrsvc, UNIT_IDLE|UNIT_DIS, 0) },
};

REG ewb_reg[] = {
  { GRDATA ( SA0,  ewb.mac[0], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA ( SA1,  ewb.mac[1], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA ( SA2,  ewb.mac[2], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA ( SA3,  ewb.mac[3], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA ( SA4,  ewb.mac[4], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA ( SA5,  ewb.mac[5], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA ( MX0,  ewb.mac_checksum[0], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA ( MX1,  ewb.mac_checksum[1], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATAD ( RBDL, ewb.rbdl[0], XQ_RDX, 16, 0, "Receive BDL Address(low)"), REG_FIT },
  { GRDATAD ( RBDH, ewb.rbdl[1], XQ_RDX, 16, 0, "Receive BDL Address(high)"), REG_FIT },
  { GRDATAD ( XBDL, ewb.xbdl[0], XQ_RDX, 16, 0, "Transmit BDL Address(low)"), REG_FIT },
  { GRDATAD ( XBDH, ewb.xbdl[1], XQ_RDX, 16, 0, "Transmit BDL Address(high)"), REG_FIT },
  { GRDATADF ( VAR,  ewb.var,  XQ_RDX, 16, 0, "Vector Address Register", ew_var_bits), REG_FIT },
  { GRDATADF ( CSR,  ewb.csr,  XQ_RDX, 16, 0, "Control and Status Register", ew_csr_bits), REG_FIT },
  { FLDATA ( INT,  ewb.irq, 0) },
  { GRDATA ( TYPE,  ewb.type,  XQ_RDX, 32, 0), REG_FIT },
  { GRDATA ( MODE,  ewb.mode,  XQ_RDX, 32, 0), REG_FIT },
  { GRDATA ( POLL, ewb.poll, XQ_RDX, 16, 0), REG_HRO},
  { GRDATA ( CLAT, ewb.coalesce_latency, XQ_RDX, 16, 0), REG_HRO},
  { GRDATA ( CLATT, ewb.coalesce_latency_ticks, XQ_RDX, 16, 0), REG_HRO},
  { GRDATA ( RBDL_BA, ewb.rbdl_ba, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( XBDL_BA, ewb.xbdl_ba, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SETUP_PRM, ewb.setup.promiscuous, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SETUP_MLT, ewb.setup.multicast, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SETUP_L1, ewb.setup.l1, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SETUP_L2, ewb.setup.l2, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SETUP_L3, ewb.setup.l3, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SETUP_SAN, ewb.setup.sanity_timer, XQ_RDX, 32, 0), REG_HRO},
  { BRDATA ( SETUP_MACS, &ewb.setup.macs, XQ_RDX, 8, sizeof(ewb.setup.macs)), REG_HRO},
  { BRDATA ( STATS, &ewb.stats, XQ_RDX, 8, sizeof(ewb.stats)), REG_HRO},
  { BRDATA ( TURBO_INIT, &ewb.init, XQ_RDX, 8, sizeof(ewb.init)), REG_HRO},
  { GRDATADF ( SRR,  ewb.srr,  XQ_RDX, 16, 0, "Status and Response Register", ew_srr_bits), REG_FIT },
  { GRDATAD ( SRQR,  ewb.srqr,  XQ_RDX, 16, 0, "Synchronous Request Register"), REG_FIT },
  { GRDATAD ( IBA,  ewb.iba,  XQ_RDX, 32, 0, "Init Block Address Register"), REG_FIT },
  { GRDATAD ( ICR,  ewb.icr,  XQ_RDX, 16, 0, "Interrupt Request Register"), REG_FIT },
  { GRDATA ( IPEND,  ewb.pending_interrupt,  XQ_RDX, 16, 0), REG_FIT },
  { GRDATA ( TBINDX, ewb.tbindx, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( RBINDX, ewb.rbindx, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( IDTMR, ewb.idtmr, XQ_RDX, 32, 0), REG_HRO},
//  { GRDATA ( VECTOR, ewb_dib.vec, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( MUST_POLL, ewb.must_poll, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SANT_ENAB, ewb.sanity.enabled, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SANT_QSECS, ewb.sanity.quarter_secs, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( SANT_TIMR, ewb.sanity.timer, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( LOCKMODE, ewb.lockmode, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( THR_TIME, ewb.throttle_time, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( THR_BURST, ewb.throttle_burst, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA ( THR_DELAY, ewb.throttle_delay, XQ_RDX, 32, 0), REG_HRO},
  { NULL },
};

MTAB ew_mod[] = {
//  { MTAB_XTD|MTAB_VDV, 0, "ADDRESS", NULL,
//    NULL, &show_addr, NULL, "Qbus address" },
//  { MTAB_XTD|MTAB_VDV, 0, "VECTOR", NULL,
//    NULL, &show_vec, NULL,  "Interrupt vector" },
  { MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "MAC", "MAC=xx:xx:xx:xx:xx:xx",
    &ew_setmac, &ew_showmac, NULL, "MAC address" },
  { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "ETH", NULL,
    NULL, &eth_show, NULL, "Display attachable devices" },
  { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "FILTERS", NULL,
    NULL, &ew_show_filters, NULL, "Display address filters" },
  { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "STATS", "STATS",
    &ew_set_stats, &ew_show_stats, NULL, "Display or reset statistics" },
  { MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "TYPE", "TYPE={DEQNA|DELQA|DELQA-T}",
    &ew_set_type, &ew_show_type, NULL, "Display current device type being simulated" },
#ifdef USE_READER_THREAD
  { MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "POLL", "POLL={DEFAULT|DISABLED|4..2500|DELAY=nnn}",
    &ew_set_poll, &ew_show_poll, NULL, "Display the current polling mode" },
#else
  { MTAB_XTD|MTAB_VDV, 0, "POLL", "POLL={DEFAULT|DISABLED|4..2500}",
    &ew_set_poll, &ew_show_poll, NULL, "Display the current polling mode" },
#endif
//  { MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "SANITY", "SANITY={ON|OFF}",
//    &ew_set_sanity, &ew_show_sanity, NULL, "Sanity timer" },
  { MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "THROTTLE", "THROTTLE=DISABLED|TIME=n{;BURST=n{;DELAY=n}}",
    &ew_set_throttle, &ew_show_throttle, NULL, "Display transmit throttle configuration" },
//  { MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "DEQNALOCK", "DEQNALOCK={ON|OFF}",
//    &ew_set_lockmode, &ew_show_lockmode, NULL, "DEQNA-Lock mode" },
//  { MTAB_XTD|MTAB_VDV,           0, "LEDS", NULL,
//    NULL, &ew_show_leds, NULL, "Display status LEDs" },
  { 0 },
};

DEBTAB ew_debug[] = {
  {"TRACE",  DBG_TRC,   "trace routine calls"},
  {"CSR",    DBG_CSR,   "watch CSR"},
  {"VAR",    DBG_VAR,   "watch VAR"},
  {"WARN",   DBG_WRN,   "display warnings"},
  {"SETUP",  DBG_SET,   "display setup info"},
  {"SANITY", DBG_SAN,   "display sanity timer info"},
  {"REG",    DBG_REG,   "trace read/write registers"},
  {"PACKET", DBG_PCK,   "display packet headers"},
  {"DATA",   DBG_DAT,   "display packet data"},
  {"ETH",    DBG_ETH,   "debug ethernet device"},
  {0}
};

DEVICE ewa_dev = {
  "EWA", ewa_unit, ewa_reg, ew_mod,
  2, XQ_RDX, 11, 1, XQ_RDX, 16,
  &ew_ex, &ew_dep, &ew_reset,
  &ew_boot, &ew_attach, &ew_detach,
  &ewa_dib, DEV_DISABLE | DEV_PCI | DEV_DEBUG | DEV_ETHER,
  DBG_WRN, ew_debug, NULL, NULL, &ew_help, NULL, NULL, 
  &ew_description
};

struct pci_config_t ewa_cfg_reg[1];
struct pci_config_t ewa_cfg_wmask[1];
extern PCI_BUS pyxis_pci64;

PCI_STAT ew_pci_reset (PCI_DEV* dev);
PCI_STAT ew_io_read   (PCI_DEV* dev, t_uint64 pci_addr, int size_b, uint8 cbez, uint32* value); // forward
PCI_STAT ew_io_write  (PCI_DEV* dev, t_uint64 pci_addr, int size_b, uint8 cbez, uint32  value); // forward
PCI_STAT ew_mem_read  (PCI_DEV* dev, t_uint64 pci_addr, int size_b, uint8 cbez, uint32* value); // forward
PCI_STAT ew_mem_write (PCI_DEV* dev, t_uint64 pci_addr, int size_b, uint8 cbez, uint32  value); // forward
PCI_STAT ew_cfg_read  (PCI_DEV* dev, int slot, int func, int register_, uint8 cbez, uint32* value);  // forward
PCI_STAT ew_cfg_write (PCI_DEV* dev, int slot, int func, int register_, uint8 cbez, uint32  value);  // forward

PCI_DEV ewa_pci_dev = {
    "EWA_PCI",          // name
    &ewa_dev,           // DEV*
    &pyxis_pci64,       // PCI_BUS*
    3,                  // pci slot
    1,                  // functions
    ewa_cfg_reg,        // cfg_reg[func]
    ewa_cfg_wmask,      // cfg_wmask[func]
    &ew_pci_reset,      // (*reset)
    NULL,               // (*int_ack)
    NULL,               // (*special)
    0,//&ew_io_read,        // (*io_read)
    0,//&ew_io_write,       // (*io_write)
    0,//&ew_mem_read,       // (*mem_read)
    0,//&ew_mem_write,      // (*mem_write)
    0,//&ew_cfg_read,       // (*cfg_read)
    0,//&ew_cfg_write,      // (*cfg_write)
    NULL,               // (*mem_readm)
    NULL,               // (*dac)
    NULL,               // (*mem_readl)
    NULL,               // (*mem_writei)
    NULL,               // (*cfg_read1)
    NULL                // (*cfg_write1)
};

DEVICE ewb_dev = {
  "EWB", ewb_unit, ewb_reg, ew_mod,
  2, XQ_RDX, 11, 1, XQ_RDX, 16,
  &ew_ex, &ew_dep, &ew_reset,
  &ew_boot, &ew_attach, &ew_detach,
  &ewb_dib, DEV_DISABLE | DEV_DIS | DEV_PCI | DEV_DEBUG | DEV_ETHER,
  0, ew_debug, NULL, NULL, NULL, NULL, NULL, 
  &ew_description
};

struct pci_config_t ewb_cfg_reg[1];
struct pci_config_t ewb_cfg_wmask[1];

PCI_DEV ewb_pci_dev = {
    "EWB_PCI",          // name
    &ewb_dev,           // DEV*
    &pyxis_pci64,       // PCI_BUS*
    11,                 // pci slot
    1,                  // functions
    ewb_cfg_reg,        // cfg_reg[func]
    ewb_cfg_wmask,      // cfg_wmask[func]
    &ew_pci_reset,      // (*reset)
    NULL,               // (*int_ack)
    NULL,               // (*special)
    0,//&ew_io_read,        // (*io_read)
    0,//&ew_io_write,       // (*io_write)
    0,//&ew_mem_read,       // (*mem_read)
    0,//&ew_mem_write,      // (*mem_write)
    0,//&ew_cfg_read,       // (*cfg_read)
    0,//&ew_cfg_write,      // (*cfg_write)
    NULL,               // (*mem_readm)
    NULL,               // (*dac)
    NULL,               // (*mem_readl)
    NULL                // (*mem_writei)
};

CTLR ew_ctrl[] = {
  {&ewa_dev,  ewa_unit, &ewa_dib, &ewa, &ewa_pci_dev},    /* EWA controller */
  {&ewb_dev, ewb_unit, &ewb_dib, &ewb, &ewb_pci_dev}      /* EWB controller */
};

const char* const ew_recv_regnames[] = {
  "MAC0", "MAC1", "MAC2", "MAC3", "MAC4", "MAC5", "VAR", "CSR"
};

const char* const xqt_recv_regnames[] = {
  "MAC0", "MAC1", "MAC2", "MAC3", "MAC4", "MAC5", "SRR", ""
};

const char* const ew_xmit_regnames[] = {
  "XCR0", "XCR1", "RBDL-Lo", "RBDL-Hi", "XBDL-Lo", "XBDL-Hi", "VAR", "CSR"
};

const char* const xqt_xmit_regnames[] = {
  "IBAL", "IBAH", "ICR", "", "SRQR", "", "", "ARQR"
};

/* internal debugging routines */
void ew_debug_setup(CTLR* xq);
void ew_debug_turbo_setup(CTLR* xq);

/*============================================================================*/

/* Multicontroller support */

CTLR* ew_unit2ctlr(UNIT* uptr)
{
  unsigned int i,j;
  for (i=0; i<XQ_MAX_CONTROLLERS; i++)
    for (j=0; j<ew_ctrl[i].dev->numunits; j++)
      if (&ew_ctrl[i].unit[j] == uptr)
        return &ew_ctrl[i];
  /* not found */
  return 0;
}

CTLR* ewa_dev2ctlr(DEVICE* dptr)
{
  int i;
  for (i=0; i<XQ_MAX_CONTROLLERS; i++)
    if (ew_ctrl[i].dev == dptr)
      return &ew_ctrl[i];
  /* not found */
  return 0;
}

CTLR* ew_pa2ctlr (uint32 PA)
{
/*
    int i;

    for (i=0; i<XQ_MAX_CONTROLLERS; i++)
    if ((PA >= ew_ctrl[i].dib->ba) && (PA < (ew_ctrl[i].dib->ba + ew_ctrl[i].dib->lnt)))
      return &ew_ctrl[i];
 */
  /* not found */
  return 0;
}

CTLR* ew_pci2ctlr (struct pci_device_t* pci)
{
  int i;
  for (i=0; i<XQ_MAX_CONTROLLERS; i++)
    if (pci == ew_ctrl[i].pci)
      return &ew_ctrl[i];
  /* not found */
  return 0;
}

/*============================================================================*/

/* stop simh from reading non-existant unit data stream */
t_stat ew_ex (t_value* vptr, t_addr addr, UNIT* uptr, int32 sw)
{
  /* on PDP-11, allow EX command to look at bootrom */
  // On Alpha, might want to examine expansion ROM?
#ifdef VM_PDP11
  CTLR* xq = ew_unit2ctlr(uptr);
  uint16 *bootrom = NULL;

  if (xq->var->type == XQ_T_DEQNA)
    bootrom = ew_bootrom_deqna;
  else
    if (xq->var->type == XQ_T_DELQA)
      bootrom = ew_bootrom_delqa;
    else
      if (xq->var->type == XQ_T_DELQA_PLUS)
        bootrom = ew_bootrom_delqat;
  if (addr <= sizeof(ew_bootrom_delqa)/2)
    *vptr = bootrom[addr];
  else
    *vptr = 0;
  return SCPE_OK;
#else
  return SCPE_NOFNC;
#endif
}

/* stop simh from writing non-existant unit data stream */
t_stat ew_dep (t_value val, t_addr addr, UNIT* uptr, int32 sw)
{
  return SCPE_NOFNC;
}

t_stat ew_showmac (FILE* st, UNIT* uptr, int32 val, void* desc)
{
  CTLR* xq = ew_unit2ctlr(uptr);
  char  buffer[20];

  eth_mac_fmt((ETH_MAC*)xq->var->mac, buffer);
  fprintf(st, "MAC=%s", buffer);
  return SCPE_OK;
}

void ew_make_checksum(CTLR* xq)
{
  /* checksum calculation routine detailed in vaxboot.zip/xqbtdrivr.mar */
  uint32  checksum = 0;
  const uint32 wmask = 0xFFFF;
  size_t i;

  for (i = 0; i < sizeof(ETH_MAC); i += 2) {
    checksum <<= 1;
    if (checksum > wmask)
      checksum -= wmask;
    checksum += (xq->var->mac[i] << 8) | xq->var->mac[i+1];
    if (checksum > wmask)
      checksum -= wmask;
  }
  if (checksum == wmask)
    checksum = 0;

  /* set checksum bytes */
  xq->var->mac_checksum[0] = (uint8)(checksum);
  xq->var->mac_checksum[1] = (uint8)(checksum >> 8);
}

t_stat ew_setmac (UNIT* uptr, int32 val, char* cptr, void* desc)
{
  t_stat status;
  CTLR* xq = ew_unit2ctlr(uptr);

  if (!cptr) return SCPE_IERR;
  if (uptr->flags & UNIT_ATT) return SCPE_ALATT;
  status = eth_mac_scan(&xq->var->mac, cptr);
  if (status != SCPE_OK)
    return status;

  /* calculate mac checksum */
  ew_make_checksum(xq);
  return SCPE_OK;
}

t_stat ew_set_stats (UNIT* uptr, int32 val, char* cptr, void* desc)
{
  /* this sets all ints in the stats structure to the integer passed */
  CTLR* xq = ew_unit2ctlr(uptr);

  if (cptr) {
    /* set individual stats to passed parameter value */
    int init = atoi(cptr);
    int* stat_array = (int*) &xq->var->stats;
    int elements = sizeof(struct ew_stats)/sizeof(int);
    int i;
    for (i=0; i<elements; i++)
      stat_array[i] = init;
  } else {
    /* set stats to zero */
    memset(&xq->var->stats, 0, sizeof(struct ew_stats));
  }
  return SCPE_OK;
}

t_stat ew_show_stats (FILE* st, UNIT* uptr, int32 val, void* desc)
{
  const char* fmt = "  %-15s%d\n";
  CTLR* xq = ew_unit2ctlr(uptr);

  fprintf(st, "XQ Ethernet statistics:\n");
  fprintf(st, fmt, "Recv:",        xq->var->stats.recv);
  fprintf(st, fmt, "Dropped:",     xq->var->stats.dropped + xq->var->ReadQ.loss);
  fprintf(st, fmt, "Xmit:",        xq->var->stats.xmit);
  fprintf(st, fmt, "Xmit Fail:",   xq->var->stats.fail);
  fprintf(st, fmt, "Runts:",       xq->var->stats.runt);
  fprintf(st, fmt, "Oversize:",    xq->var->stats.giant);
  fprintf(st, fmt, "SW Reset:",    xq->var->stats.reset);
  fprintf(st, fmt, "Setup:",       xq->var->stats.setup);
  fprintf(st, fmt, "Loopback:",    xq->var->stats.loop);
  fprintf(st, fmt, "ReadQ count:", xq->var->ReadQ.count);
  fprintf(st, fmt, "ReadQ high:",  xq->var->ReadQ.high);
  eth_show_dev(st, xq->var->etherface);
  return SCPE_OK;
}

t_stat ew_show_filters (FILE* st, UNIT* uptr, int32 val, void* desc)
{
  CTLR* xq = ew_unit2ctlr(uptr);
  char  buffer[20];
  size_t i;

  if (xq->var->mode == XQ_T_DELQA_PLUS) {
    eth_mac_fmt(&xq->var->init.phys, buffer);
    fprintf(st, "Physical Address=%s\n", buffer);
    if (xq->var->etherface->hash_filter) {
      fprintf(st, "Multicast Hash: ");
      for (i=0; i<sizeof(xq->var->etherface->hash); ++i)
        fprintf(st, "%02X ", xq->var->etherface->hash[i]);
      fprintf(st, "\n");
    }
    if (xq->var->init.mode & XQ_IN_MO_PRO)
      fprintf(st, "Promiscuous Receive Mode\n");
  } else {
    fprintf(st, "Filters:\n");
    for (i=0; i<XQ_FILTER_MAX; i++) {
      eth_mac_fmt((ETH_MAC*)xq->var->setup.macs[i], buffer);
      fprintf(st, "  [%2d]: %s\n", (int)i, buffer);
    }
    if (xq->var->setup.multicast)
      fprintf(st, "All Multicast Receive Mode\n");
    if (xq->var->setup.promiscuous)
      fprintf(st, "Promiscuous Receive Mode\n");
  }
  return SCPE_OK;
}

t_stat ew_show_type (FILE* st, UNIT* uptr, int32 val, void* desc)
{
  CTLR* ew = ew_unit2ctlr(uptr);
  fprintf(st, "type=");
  switch (ew->var->type) {
    case  EW_T_DE435:   fprintf(st, "DE435");   break;
    case  EW_T_DE500A:  fprintf(st, "DE500-AA"); break;
    case  EW_T_DE500B:  fprintf(st, "DE500-BA"); break;
  }
  return SCPE_OK;
}

t_stat ew_set_type (UNIT* uptr, int32 val, char* cptr, void* desc)
{
  CTLR* ew = ew_unit2ctlr(uptr);
  if (!cptr) return SCPE_IERR;
  if (uptr->flags & UNIT_ATT) return SCPE_ALATT;

  /* this assumes that the parameter has already been upcased */
  if      (!strcmp(cptr, "DE435"))      ew->var->type = EW_T_DE435;
  else if (!strcmp(cptr, "DE500-AA"))   ew->var->type = EW_T_DE500A;
  else if (!strcmp(cptr, "DE500-BA"))   ew->var->type = EW_T_DE500B;
  else return SCPE_ARG;

  return SCPE_OK;
}

t_stat ew_show_poll (FILE* st, UNIT* uptr, int32 val, void* desc)
{
  CTLR* xq = ew_unit2ctlr(uptr);
  if (xq->var->poll)
    fprintf(st, "poll=%d", xq->var->poll);
  else {
    fprintf(st, "polling=disabled");
    if (xq->var->coalesce_latency)
      fprintf(st, ",latency=%d", xq->var->coalesce_latency);
  }
  return SCPE_OK;
}

t_stat ew_set_poll (UNIT* uptr, int32 val, char* cptr, void* desc)
{
  CTLR* xq = ew_unit2ctlr(uptr);
  if (!cptr) return SCPE_IERR;
  if (uptr->flags & UNIT_ATT) return SCPE_ALATT;

  /* this assumes that the parameter has already been upcased */
  if (!strcmp(cptr, "DEFAULT"))
    xq->var->poll = XQ_SERVICE_INTERVAL;
  else if ((!strcmp(cptr, "DISABLED")) || (!strncmp(cptr, "DELAY=", 6))) {
    xq->var->poll = 0;
    if (!strncmp(cptr, "DELAY=", 6)) {
      int delay = 0;
      if (1 != sscanf(cptr+6, "%d", &delay))
        return SCPE_ARG;
      xq->var->coalesce_latency = delay;
      xq->var->coalesce_latency_ticks = (tmr_poll * clk_tps * xq->var->coalesce_latency) / 1000000;
      }
    }
  else {
    int newpoll = 0;
    if (1 != sscanf(cptr, "%d", &newpoll))
      return SCPE_ARG;
    if ((newpoll == 0) ||
        ((!sim_idle_enab) && (newpoll >= 4) && (newpoll <= 2500)))
      xq->var->poll = newpoll;
    else
      return SCPE_ARG;
  }

  return SCPE_OK;
}

t_stat ew_show_throttle (FILE* st, UNIT* uptr, int32 val, void* desc)
{
  CTLR* xq = ew_unit2ctlr(uptr);

  if (xq->var->throttle_delay == ETH_THROT_DISABLED_DELAY)
    fprintf(st, "throttle=disabled");
  else
    fprintf(st, "throttle=time=%d;burst=%d;delay=%d", xq->var->throttle_time, xq->var->throttle_burst, xq->var->throttle_delay);
  return SCPE_OK;
}

t_stat ew_set_throttle (UNIT* uptr, int32 val, char* cptr, void* desc)
{
  CTLR* xq = ew_unit2ctlr(uptr);
  char tbuf[CBUFSIZE], gbuf[CBUFSIZE];
  char *tptr = cptr;
  uint32 newval;
  uint32 set_time = xq->var->throttle_time;
  uint32 set_burst = xq->var->throttle_burst;
  uint32 set_delay = xq->var->throttle_delay;
  t_stat r = SCPE_OK;

  if (!cptr) {
    xq->var->throttle_delay = ETH_THROT_DEFAULT_DELAY;
    eth_set_throttle (xq->var->etherface, xq->var->throttle_time, xq->var->throttle_burst, xq->var->throttle_delay);
    return SCPE_OK;
    }

  /* this assumes that the parameter has already been upcased */
  if ((!strcmp (cptr, "ON")) ||
      (!strcmp (cptr, "ENABLED")))
    xq->var->throttle_delay = ETH_THROT_DEFAULT_DELAY;
  else
    if ((!strcmp (cptr, "OFF")) ||
        (!strcmp (cptr, "DISABLED")))
      xq->var->throttle_delay = ETH_THROT_DISABLED_DELAY;
    else {
      if (set_delay == ETH_THROT_DISABLED_DELAY)
        set_delay = ETH_THROT_DEFAULT_DELAY;
      while (*tptr) {
        tptr = get_glyph_nc (tptr, tbuf, ';');
        cptr = tbuf;
        cptr = get_glyph (cptr, gbuf, '=');
        if ((NULL == cptr) || ('\0' == *cptr))
          return SCPE_ARG;
        newval = (uint32)get_uint (cptr, 10, 100, &r);
        if (r != SCPE_OK)
          return SCPE_ARG;
        if (!MATCH_CMD(gbuf, "TIME")) {
          set_time = newval;
          }
        else
          if (!MATCH_CMD(gbuf, "BURST")) {
            if (newval > 30)
               return SCPE_ARG;
            set_burst = newval;
            }
          else
            if (!MATCH_CMD(gbuf, "DELAY")) {
              set_delay = newval;
              }
            else
              return SCPE_ARG;
        }
      xq->var->throttle_time = set_time;
      xq->var->throttle_burst = set_burst;
      xq->var->throttle_delay = set_delay;
      }
  eth_set_throttle (xq->var->etherface, xq->var->throttle_time, xq->var->throttle_burst, xq->var->throttle_delay);
  return SCPE_OK;
}

/*============================================================================*/

t_stat ew_nxm_error(CTLR* xq)
{
  const uint16 set_bits = XQ_CSR_NI | XQ_CSR_XI | XQ_CSR_XL | XQ_CSR_RL;
  sim_debug(DBG_WRN, xq->dev, "Non Existent Memory Error!\n");

  if (xq->var->mode == XQ_T_DELQA_PLUS) {
    /* set NXM and associated bits in SRR */
    xq->var->srr |= (XQ_SRR_FES | XQ_SRR_NXM);
    ew_setint(xq);
  } else
    /* set NXM and associated bits in CSR */
    ew_csr_set_clr(xq, set_bits , 0);
  return SCPE_OK;
}

/*
** write callback
*/
void ew_write_callback (CTLR* ew, int status)
{
  PCI_STAT wstatus;
  sim_debug(DBG_TRC, ew->dev, "ew_write_callback\n");
    
  // collect statistics
  ew->var->stats.xmit += 1;

  /* update write status words */
  if (status == 0) { /* success */
    if (DBG_PCK & ew->dev->dctrl)
      eth_packet_trace_ex(ew->var->etherface, ew->var->write_buffer.msg, ew->var->write_buffer.len, "xq-write", DBG_DAT & ew->dev->dctrl, DBG_PCK);

  } else { /* failure */
    sim_debug(DBG_WRN, ew->dev, "Packet Write Error!\n");
    ew->var->stats.fail += 1;
    //wstatus = Map_WriteW(ew->var->xbdl_ba + 8, 4, write_failure);
  }
  wstatus = pci_bus_mem_write(ew->pci->bus, ew->var->tx_curr_base, 4, PCI_CBE_DWORD_LO, ew->var->tx_tdes[0]);
  if (wstatus != PCI_OK) {
    ew_nxm_error(ew);
    return;
  }

  /* update csr */
  ew_csr_set_clr(ew, XQ_CSR_XI, 0);

  /* clear write buffer */
  ew->var->write_buffer.len = 0;

}

void ewa_write_callback (int status)
{
  ew_write_callback(&ew_ctrl[0], status);
}

void ewb_write_callback (int status)
{
  ew_write_callback(&ew_ctrl[1], status);
}

/* read registers: */
t_stat ew_rd(int32* data, int32 PA, int32 access)
{
  CTLR* xq = ew_pa2ctlr(PA);
  int index = (PA >> 1) & 07;   /* word index */

  sim_debug(DBG_REG, xq->dev, "ew_rd(PA=0x%08X [%s], access=%d)\n", PA, ((xq->var->mode == XQ_T_DELQA_PLUS) ? xqt_recv_regnames[index] : ew_recv_regnames[index]), access);
  switch (index) {
    case 0:
    case 1:
      /* return checksum in external loopback mode */
      if (xq->var->csr & XQ_CSR_EL)
        *data = 0xFF00 | xq->var->mac_checksum[index];
      else
        *data = 0xFF00 | xq->var->mac[index];
      sim_debug(DBG_REG, xq->dev, "   data=0x%X\n", *data);
      break;
    case 2:
    case 3:
    case 4:
    case 5:
      *data = 0xFF00 | xq->var->mac[index];
      sim_debug(DBG_REG, xq->dev, "   data=0x%X\n", *data);
      break;
    case 6:
      if (xq->var->mode != XQ_T_DELQA_PLUS) {
        sim_debug_bits(DBG_VAR, xq->dev, ew_var_bits, xq->var->var, xq->var->var, 0);
        sim_debug     (DBG_VAR, xq->dev, ", vec = 0%o\n", (xq->var->var & XQ_VEC_IV));
        *data = xq->var->var;
      } else {
        sim_debug_bits(DBG_VAR, xq->dev, ew_srr_bits, xq->var->srr, xq->var->srr, 1);
        *data = xq->var->srr;
      }
      break;
    case 7:
      sim_debug_bits(DBG_CSR, xq->dev, ew_csr_bits, xq->var->csr, xq->var->csr, 1);
      *data = xq->var->csr;
      break;
  }
  return SCPE_OK;
}


/* dispatch ethernet read request
   procedure documented in sec. 3.2.2 */

t_stat ew_process_rbdl(CTLR* ew)
{

  ETH_ITEM* item;

  int   rbs1, rbs2;
  int    bite_size;
  uint8* bite_address;
  PCI_STAT  rstatus, wstatus;
  t_uint64  pci_dst_address;

  sim_debug(DBG_TRC, ew->dev, "ew_process_rdbl\n");

  if (ew->var->rx_state == STATE_STOPPED)   // receiver not running - can't process packets
      return SCPE_OK;

  /* process buffer descriptors */
  while(1) {

    if (!ew->var->ReadQ.count) break;           // No packets waiting in queue to process

    ew->var->rx_state = STATE_RUNNING;
    item = &ew->var->ReadQ.item[ew->var->ReadQ.head];   // point to 1st queued packet

    // Attempt to acquire current receive descriptor
    // .. a more accurate simulation would use multiple pci_bus_mem_read()s if CSR0.RME is not set
    rstatus = pci_bus_mem_readm (ew->pci->bus, ew->var->rx_curr_base, ew->var->rx_rdes, 4);
    if (rstatus != PCI_OK) {
        return ew_nxm_error(ew);
    }
    
    // Does descriptor belong to controller? If not, suspend processing and signal condition.
    if ((ew->var->rx_rdes[0] & EW_RDES0_OWN) == 0) {    // controller does not own buffer; suspend receive process
        ew->var->rx_state = STATE_SUSPENDED;
        ew_csr_set_clr (ew, EW_CSR5_RU, 0);             // Set no receive buffers bit and interrupt if needed
    }

    // Descriptor belongs to controller, start filling it in
    ew->var->rx_rdes[0] = 0;                                // clear ownership and status bits
    if (item->packet.used == 0) {
        ew->var->rx_rdes[0] |= EW_RDES0_FS;                 // Mark as first descriptor of frame
    }

    // Determine available buffer sizes
    rbs1 = ew->var->rx_rdes[1] & EW_RDES1_RBS1;                       // Buffer 1 size
    rbs2 = (ew->var->rx_rdes[1] & EW_RDES1_RBS2) >> EW_RDES1_RBS2_V;  // Buffer 2 size

    // fill first buffer if length non-zero
    if (rbs1 > 0) {
        bite_size = item->packet.crc_len - item->packet.used;
        if (bite_size > rbs1) bite_size = rbs1;
        bite_address = &item->packet.msg[item->packet.used];
        pci_dst_address = ew->var->rx_rdes[2];
        // write packet buffer to PCI memory
        // .. a more accurate simulation would use multiple pci_bus_mem_write()s if CSR0.WIE not set
        wstatus = pci_bus_mem_writei (ew->pci->bus, pci_dst_address, (uint32*) bite_address, (int)((bite_size+3)/4));
        if (wstatus != PCI_OK) return ew_nxm_error(ew);
        item->packet.used += bite_size;                             // adjust used size
    }

    // fill second buffer if length non_zero and not a chained buffer address and more packet
    if (item->packet.used < item->packet.crc_len)  {
        if (((ew->var->rx_rdes[1] & EW_RDES1_RCH) == 0) && (rbs2 > 0)){
            bite_size = item->packet.crc_len - item->packet.used;
            if (bite_size > rbs2) bite_size = rbs2;
            bite_address = &item->packet.msg[item->packet.used];
            pci_dst_address = ew->var->rx_rdes[3];
            // write packet buffer to PCI memory
            // .. a more accurate simulation would use multiple pci_bus_mem_write()s if CSR0.WIE not set
            wstatus = pci_bus_mem_writei (ew->pci->bus, pci_dst_address, (uint32*) bite_address, (int)((bite_size+3)/4));
            if (wstatus != PCI_OK) return ew_nxm_error(ew);
            item->packet.used += bite_size;                             // adjust used size
        }
    }

    // Fill in RDES0 status bits, we're done with this descriptor..
    if (item->packet.used >= item->packet.crc_len) {
        ew->var->rx_rdes[0] |= item->packet.crc_len << 16;      // set frame length
        ew->var->rx_rdes[0] |= EW_RDES0_LS;                     // set last descriptor (of frame) indicator
        if (item->packet.crc_len > 1500) {
            ew->var->rx_rdes[0] |= EW_RDES0_FT;                 // set packet length > 1500
        }
        if ((item->packet.msg[0] & 0x1) != 0) {                 // if dst[0].lsb set, multicast address
            ew->var->rx_rdes[0] |= EW_RDES0_MF;                 // set multicast frame
        }
        ew->var->rx_rdes[0] |= ((ew->var->csrs[6] & EW_CSR6_OM) << 2);  // set data type RDES0<13:12> = operating mode CSR6<11:10>
        if (item->packet.crc_len < ETH_MIN_PACKET) {            // runt packet?
            sim_debug(DBG_WRN, ew->dev, "ew_process_rbdl: Runt detected, size = %d\n", item->packet.len);
            ew->var->rx_rdes[0] |= EW_RDES0_ES | EW_RDES0_RF;   // set runt indicators
        }
    }

    // Write back RDES0 (other RDES descriptor array elements have not changed)
    wstatus = pci_bus_mem_write (ew->pci->bus, ew->var->rx_curr_base, 4, PCI_CBE_DWORD_LO, ew->var->rx_rdes[0]);
    if (wstatus != PCI_OK) return ew_nxm_error(ew);

    // Figure out where the next descriptor is
    if ((ew->var->rx_rdes[1] & EW_RDES1_RER) != 0) {        // end-of-ring marker takes priority, 
        ew->var->rx_curr_base = ew->var->csrs[3];           // .. return to ring base address
    } else if ((ew->var->rx_rdes[1] & EW_RDES1_RCH) != 0) { // explicit chain, address is in RDES3
        ew->var->rx_curr_base = ew->var->rx_rdes[3];
    } else {                                                // unchained descriptor
        ew->var->rx_curr_base += 16;                        // set to next contiguous descriptor address
        ew->var->rx_curr_base += ((ew->var->csrs[0] & EW_CSR0_DSL) >> EW_CSR0_DSL_V);   // add descriptor skip length
    }

    // Remove packet from queue and signal reception complete
    if (item->packet.used >= item->packet.len) {
        ethq_remove(&ew->var->ReadQ);

        // signal reception complete
        ew_csr_set_clr(ew, XQ_CSR_RI, 0);
    }

  } /* while */
  return SCPE_OK;
}


t_stat ew_process_setup(CTLR* xq)
{
  int i,j;
  int count = 0;
  float secs = 0;
  uint32 saved_debug = xq->dev->dctrl;
  ETH_MAC zeros = {0, 0, 0, 0, 0, 0};
  ETH_MAC filters[XQ_FILTER_MAX + 1];

  sim_debug(DBG_TRC, xq->dev, "ew_process_setup()\n");

  /* temporarily turn on Ethernet debugging if setup debugging is enabled */
  if (xq->dev->dctrl & DBG_SET)
    xq->dev->dctrl |= DBG_ETH;

  /* extract filter addresses from setup packet */
  memset(xq->var->setup.macs, '\0', sizeof(xq->var->setup.macs));
  for (i = 0; i < 7; i++)
    for (j = 0; j < 6; j++) {
      if ((uint32)((i +   01) + (j * 8)) >= xq->var->write_buffer.len)
        continue;
      xq->var->setup.macs[i]  [j] = xq->var->write_buffer.msg[(i +   01) + (j * 8)];
      if ((uint32)((i + 0101) + (j * 8)) >= xq->var->write_buffer.len)
        continue;
      xq->var->setup.macs[i+7][j] = xq->var->write_buffer.msg[(i + 0101) + (j * 8)];
    }

  /*
     Under VMS the setup packet that is passed to turn promiscuous 
     off after it has been on doesn't seem to follow the rules documented 
     in both the DEQNA and DELQA manuals.
     These rules seem to say that setup packets less than 128 should only 
     modify the address filter set and probably not the All-Multicast and 
     Promiscuous modes, however, VMS V5-5 and V7.3 seem to send a 127 byte 
     packet to turn this functionality off.  I'm not sure how real hardware 
     behaves in this case, since the only consequence is extra interrupt 
     load.  To realize and retain the benefits of the newly added BPF 
     functionality in sim_ether, I've modified the logic implemented here
     to disable Promiscuous mode when a "small" setup packet is processed.
     I'm deliberately not modifying the All-Multicast mode the same way
     since I don't have an observable case of its behavior.  These two 
     different modes come from very different usage situations:
        1) Promiscuous mode is usually entered for relatively short periods 
           of time due to the needs of a specific application program which
           is doing some sort of management/monitoring function (i.e. tcpdump)
        2) All-Multicast mode is only entered by the OS Kernel Port Driver
           when it happens to have clients (usually network stacks or service 
           programs) which as a group need to listen to more multicast ethernet
           addresses than the 12 (or so) which the hardware supports directly.
     so, I believe that the All-Multicast mode, is first rarely used, and if 
     it ever is used, once set, it will probably be set either forever or for 
     long periods of time, and the additional interrupt processing load to
     deal with the distinctly lower multicast traffic set is clearly lower than
     that of the promiscuous mode.
  */
  xq->var->setup.promiscuous = 0;
  /* process high byte count */
  if (xq->var->write_buffer.len > 128) {
    uint16 len = (uint16)xq->var->write_buffer.len;
    uint16 led, san;

    xq->var->setup.multicast = (0 != (len & XQ_SETUP_MC));
    xq->var->setup.promiscuous = (0 != (len & XQ_SETUP_PM));
    if ((led = (len & XQ_SETUP_LD) >> 2)) {
      switch (led) {
        case 1: xq->var->setup.l1 = 0; break;
        case 2: xq->var->setup.l2 = 0; break;
        case 3: xq->var->setup.l3 = 0; break;
      } /* switch */
    } /* if led */

    /* set sanity timer timeout */
    san = (len & XQ_SETUP_ST) >> 4;
    switch(san) {
      case 0: secs = 0.25;    break;  /* 1/4 second  */
      case 1: secs = 1;       break;  /*   1 second  */
      case 2: secs = 4;       break;  /*   4 seconds */
      case 3: secs = 16;      break;  /*  16 seconds */
      case 4: secs =  1 * 60; break;  /*   1 minute  */
      case 5: secs =  4 * 60; break;  /*   4 minutes */
      case 6: secs = 16 * 60; break;  /*  16 minutes */
      case 7: secs = 64 * 60; break;  /*  64 minutes */
    }
    xq->var->sanity.quarter_secs = (int) (secs * 4);
  }

  /* finalize sanity timer state */
  if (xq->var->sanity.enabled != 2) {
    if (xq->var->csr & XQ_CSR_SE)
      xq->var->sanity.enabled = 1;
    else
      xq->var->sanity.enabled = 0;
  }
  ew_reset_santmr(xq);

  /* set ethernet filter */
  /* memcpy (filters[count++], xq->mac, sizeof(ETH_MAC)); */
  for (i = 0; i < XQ_FILTER_MAX; i++)
    if (memcmp(zeros, &xq->var->setup.macs[i], sizeof(ETH_MAC)))
      memcpy (filters[count++], xq->var->setup.macs[i], sizeof(ETH_MAC));
  eth_filter (xq->var->etherface, count, filters, xq->var->setup.multicast, xq->var->setup.promiscuous);


  /* mark setup block valid */
  xq->var->setup.valid = 1;

  ew_debug_setup(xq);

  xq->dev->dctrl = saved_debug; /* restore original debugging */

  return SCPE_OK;
}

/*
  Dispatch Write Operation

  The DELQA manual does not explicitly state whether or not multiple packets
  can be written in one transmit operation, so a maximum of 1 packet is assumed.
  
  MP: Hmmm... Figure 3-1 on page 3-3 step 6 says that descriptors will be processed
  until the end of the list is found.

*/
t_stat ew_process_xbdl(CTLR* xq)
{
  const uint16  implicit_chain_status[2] = {XQ_DSC_V | XQ_DSC_C, 1};
  uint16  write_success[2] = {0x2000 /* Bit 13 Always Set */, 1 /*Non-Zero TDR*/};
  uint16 b_length, w_length;
  int32 rstatus = 0;
  int32 wstatus = 0;
  uint32 address;
  t_stat status;

  sim_debug(DBG_TRC, xq->dev, "ew_process_xbdl()\n");

  /* clear write buffer */
  xq->var->write_buffer.len = 0;
  free (xq->var->write_buffer.oversize);
  xq->var->write_buffer.oversize = NULL;

  /* process buffer descriptors until not valid */
  while (1) {

    /* Get transmit bdl from memory */
    //rstatus = Map_ReadW (xq->var->xbdl_ba,    12, &xq->var->xbdl_buf[0]);
    xq->var->xbdl_buf[0] = 0xFFFF;
    //wstatus = Map_WriteW(xq->var->xbdl_ba,     2, &xq->var->xbdl_buf[0]);
    if (rstatus || wstatus) return ew_nxm_error(xq);

    /* compute host memory address */
    address = ((xq->var->xbdl_buf[1] & 0x3F) << 16) | xq->var->xbdl_buf[2];

    /* explicit chain buffer? */
    if (xq->var->xbdl_buf[1] & XQ_DSC_C) {
      xq->var->xbdl_ba = address;
      sim_debug(DBG_WRN, xq->dev, "XBDL chaining to buffer descriptor at: 0x%X\n", address);
      continue;
    }

    /* invalid buffer? */
    if (~xq->var->xbdl_buf[1] & XQ_DSC_V) {
      ew_csr_set_clr(xq, XQ_CSR_XL, 0);
      sim_debug(DBG_WRN, xq->dev, "XBDL List empty\n");
      return SCPE_OK;
    }

    /* decode buffer length - two's complement (in words) */
    w_length = ~xq->var->xbdl_buf[3] + 1;
    b_length = w_length * 2;
    if (xq->var->xbdl_buf[1] & XQ_DSC_H) {
      b_length -= 1;
      address += 1;
    }
    if (xq->var->xbdl_buf[1] & XQ_DSC_L) b_length -= 1;

    /* add to transmit buffer, making sure it's not too big */
    if ((xq->var->write_buffer.len + b_length) > sizeof(xq->var->write_buffer.msg)) {
      xq->var->write_buffer.oversize = (uint8*)realloc (xq->var->write_buffer.oversize, xq->var->write_buffer.len + b_length);
      if (xq->var->write_buffer.len <= sizeof(xq->var->write_buffer.msg))
        memcpy (xq->var->write_buffer.oversize, xq->var->write_buffer.msg, xq->var->write_buffer.len);
      }
    //rstatus = Map_ReadB(address, b_length, xq->var->write_buffer.oversize ? &xq->var->write_buffer.oversize[xq->var->write_buffer.len] : &xq->var->write_buffer.msg[xq->var->write_buffer.len]);
    if (rstatus) return ew_nxm_error(xq);
    xq->var->write_buffer.len += b_length;

    /* end of message? */
    if (xq->var->xbdl_buf[1] & XQ_DSC_E) {
      if (((~xq->var->csr & XQ_CSR_IL) || (xq->var->csr & XQ_CSR_EL)) ||  /* loopback */
           (xq->var->xbdl_buf[1] & XQ_DSC_S)) { /* or setup packet (forces loopback regardless of state) */
        if (xq->var->xbdl_buf[1] & XQ_DSC_S) { /* setup packet */
          status = ew_process_setup(xq);
          ethq_insert (&xq->var->ReadQ, 0, &xq->var->write_buffer, status);/* put packet in read buffer */
        } else { /* loopback */
          if (((~xq->var->csr & XQ_CSR_RL) &&        /* If a buffer descriptor list is good */
               (xq->var->rbdl_buf[1] & XQ_DSC_V)) || /* AND the descriptor is valid */
              (xq->var->csr & XQ_CSR_EL))            /* OR External Loopback */
            ethq_insert (&xq->var->ReadQ, 1, &xq->var->write_buffer, 0);
          if ((DBG_PCK & xq->dev->dctrl) && xq->var->etherface)
            eth_packet_trace_ex(xq->var->etherface, xq->var->write_buffer.msg, xq->var->write_buffer.len, "xq-write-loopback", DBG_DAT & xq->dev->dctrl, DBG_PCK);
          write_success[0] |= XQ_XMT_FAIL;
        }

        /* update write status */
        //wstatus = Map_WriteW(xq->var->xbdl_ba + 8, 4, (uint16*) write_success);
        if (wstatus) return ew_nxm_error(xq);

        /* clear write buffer */
        xq->var->write_buffer.len = 0;
        free (xq->var->write_buffer.oversize);
        xq->var->write_buffer.oversize = NULL;

        /* reset sanity timer */
        ew_reset_santmr(xq);

        /* signal transmission complete */
        ew_csr_set_clr(xq, XQ_CSR_XI, 0);

        /* now trigger "read" of setup or loopback packet */
        if (~xq->var->csr & XQ_CSR_RL)
          status = ew_process_rbdl(xq);

      } else { /* not loopback */

        status = eth_write(xq->var->etherface, &xq->var->write_buffer, xq->var->wcallback);
        if (status != SCPE_OK)           /* not implemented or unattached */
          ew_write_callback(xq, 1);      /* fake failure */
        else {
          if (xq->var->coalesce_latency == 0)
            ew_svc(&xq->unit[0]);        /* service any received data */
      }
        sim_debug(DBG_WRN, xq->dev, "XBDL completed processing write\n");

      } /* loopback/non-loopback */

    } else { /* not at end-of-message */

      sim_debug(DBG_WRN, xq->dev, "XBDL implicitly chaining to buffer descriptor at: 0x%X\n", xq->var->xbdl_ba+12);
      /* update bdl status words */
      //wstatus = Map_WriteW(xq->var->xbdl_ba + 8, 4, (uint16*) implicit_chain_status);
      if(wstatus) return ew_nxm_error(xq);
    }

    /* set to next bdl (implicit chain) */
    xq->var->xbdl_ba += 12;

  } /* while */
}

void ew_show_debug_bdl(CTLR* xq, uint32 bdl_ba)
{
    uint16 bdl_buf[6] = {0};
  uint16 b_length, w_length;
  uint32 address, initial_bdl_ba = bdl_ba;
  int32 rstatus = 0;

  if ((!sim_deb) || (!(xq->dev->dctrl & DBG_TRC)))/* Do nothing if not debugging */
      return;

  sim_debug(DBG_TRC, xq->dev, "  Descriptor list at: 0x%X\n", bdl_ba);

  while (1) {

    /* get the beginning of the buffer descriptor */
    //rstatus = Map_ReadW (bdl_ba, 6, &bdl_buf[0]);
    if (rstatus) return;

    /* explicit chain buffer? */
    if (bdl_buf[1] & XQ_DSC_C) {
      sim_debug(DBG_TRC, xq->dev, "    descriptor=0x%X, flags=0x%04X, bits=0x%04X, chain=0x%X\n", bdl_ba, bdl_buf[0], bdl_buf[1] & 0xFFC0, ((bdl_buf[1] & 0x3F) << 16) | bdl_buf[2]);
      bdl_ba = ((bdl_buf[1] & 0x3F) << 16) | bdl_buf[2];
      if (initial_bdl_ba == bdl_ba)
          break;
      continue;
      }

    /* invalid buffer? */
    if (~bdl_buf[1] & XQ_DSC_V)
      break;

    /* get the rest of the buffer descriptor */
    //rstatus = Map_ReadW (bdl_ba + 6, 6, &bdl_buf[3]);
    if (rstatus) return;

    /* get host memory address */
    address = ((bdl_buf[1] & 0x3F) << 16) | bdl_buf[2];

    /* decode buffer length - two's complement (in words) */
    w_length = ~bdl_buf[3] + 1;
    b_length = w_length * 2;
    if (bdl_buf[1] & XQ_DSC_H) {
      b_length -= 1;
      address += 1;
    }
    if (bdl_buf[1] & XQ_DSC_L) b_length -= 1;

    sim_debug(DBG_TRC, xq->dev, "    descriptor=0x%X, flags=0x%04X, bits=0x%04X, addr=0x%X, len=0x%X, st1=0x%04X, st2=0x%04X\n", 
                                              bdl_ba, bdl_buf[0], bdl_buf[1] & 0xFFC0, address, b_length, bdl_buf[4], bdl_buf[5]);

    bdl_ba += 12;
    }

  sim_debug(DBG_TRC, xq->dev, "    descriptor=0x%X, flags=0x%04X, bits=0x%04X\n", bdl_ba, bdl_buf[0], bdl_buf[1] & 0xFFC0);
}

t_stat ew_dispatch_rbdl(CTLR* ew)
{
  int i;
  int32 rstatus = 0;
  int32 wstatus = 0;

  sim_debug(DBG_TRC, ew->dev, "ew_dispatch_rbdl()\n");

  /* mark receive bdl valid */
  ew_csr_set_clr(ew, 0, XQ_CSR_RL);

  /* init receive bdl buffer */
  for (i=0; i<6; i++)
    ew->var->rbdl_buf[i] = 0;

  /* get address of first receive buffer */
  ew->var->rbdl_ba = ((ew->var->rbdl[1] & 0x3F) << 16) | (ew->var->rbdl[0] & ~01);

  /* When debugging, walk and display the buffer descriptor list */
  ew_show_debug_bdl(ew, ew->var->rbdl_ba);

  /* get first receive buffer */
  ew->var->rbdl_buf[0] = 0xFFFF;
  //wstatus = Map_WriteW(ew->var->rbdl_ba,     2, &ew->var->rbdl_buf[0]);
  //rstatus = Map_ReadW (ew->var->rbdl_ba + 2, 6, &ew->var->rbdl_buf[1]);
  if (rstatus || wstatus) return ew_nxm_error(ew);

  /* is buffer valid? */
  if (~ew->var->rbdl_buf[1] & XQ_DSC_V) {
    ew_csr_set_clr(ew, XQ_CSR_RL, 0);
    return SCPE_OK;
    }

  /* process any waiting packets in receive queue */
  if (ew->var->ReadQ.count)
    ew_process_rbdl(ew);

  return SCPE_OK;
}

t_stat ew_dispatch_xbdl(CTLR* xq)
{
  int i;
  t_stat status;

  sim_debug(DBG_TRC, xq->dev, "ew_dispatch_xbdl()\n");

  /* mark transmit bdl valid */
  ew_csr_set_clr(xq, 0, XQ_CSR_XL);

  /* initialize transmit bdl buffers */
  for (i=0; i<6; i++)
    xq->var->xbdl_buf[i] = 0;

  /* clear transmit buffer */
  xq->var->write_buffer.len = 0;
  free (xq->var->write_buffer.oversize);
  xq->var->write_buffer.oversize = NULL;

  /* get base address of first transmit descriptor */
  xq->var->xbdl_ba = ((xq->var->xbdl[1] & 0x3F) << 16) | (xq->var->xbdl[0] & ~01);

  /* When debugging, walk and display the buffer descriptor list */
  ew_show_debug_bdl(xq, xq->var->xbdl_ba);

  /* process xbdl */
  status = ew_process_xbdl(xq);

  return status;
}

t_stat ew_process_turbo_rbdl(CTLR* xq)
{
  int i;
  t_stat status = 0;
  int descriptors_consumed = 0;
  uint32 rdra = (xq->var->init.rdra_h << 16) | xq->var->init.rdra_l;

  sim_debug(DBG_TRC, xq->dev, "ew_process_turbo_rbdl()\n");

  if ((xq->var->srr & XQ_SRR_RESP) != XQ_SRR_STRT)
    return SCPE_OK;

  /* Process descriptors in the receive ring while the're available and we have packets */
  do {
    uint32 address;
    uint16 b_length, rbl;
    ETH_ITEM* item;
    uint8* rbuf;

    /* stop processing when nothing in read queue */
    if (!xq->var->ReadQ.count)
        break;

    i = xq->var->rbindx;

    /* Get receive descriptor from memory */
    //status = Map_ReadW (rdra+i*sizeof(xq->var->rring[i]), sizeof(xq->var->rring[i]), (uint16 *)&xq->var->rring[i]);
    if (status != SCPE_OK)
        return ew_nxm_error(xq);

    /* Done if Buffer not Owned */
    if (xq->var->rring[i].rmd3 & XQ_TMD3_OWN)
        break;

    ++descriptors_consumed;

    /* Update ring index */
    xq->var->rbindx = (xq->var->rbindx + 1) % XQ_TURBO_RC_BCNT;

    address = ((xq->var->rring[i].hadr & 0x3F ) << 16) | xq->var->rring[i].ladr;
    b_length = ETH_FRAME_SIZE;

    item = &xq->var->ReadQ.item[xq->var->ReadQ.head];
    rbl = (uint16)(item->packet.len + ETH_CRC_SIZE);
    rbuf = item->packet.msg;

    /* see if packet must be size-adjusted or is splitting */
    if (item->packet.used) {
      uint16 used = (uint16)item->packet.used;
      rbl -= used;
      rbuf = &item->packet.msg[used];
    } else {
      /* adjust non loopback runt packets */
      if ((item->type != ETH_ITM_LOOPBACK) && (rbl < ETH_MIN_PACKET)) {
        xq->var->stats.runt += 1;
        sim_debug(DBG_WRN, xq->dev, "Runt detected, size = %d\n", rbl);
        /* pad runts with zeros up to minimum size - this allows "legal" (size - 60)
           processing of those weird short ARP packets that seem to occur occasionally */
        memset(&item->packet.msg[rbl], 0, ETH_MIN_PACKET-rbl);
        rbl = ETH_MIN_PACKET;
      };

      /* adjust oversized non-loopback packets */
      if ((item->type != ETH_ITM_LOOPBACK) && (rbl > ETH_FRAME_SIZE)) {
        xq->var->stats.giant += 1;
        sim_debug(DBG_WRN, xq->dev, "Giant detected, size=%d\n", rbl);
        /* trim giants down to maximum size - no documentation on how to handle the data loss */
        item->packet.len = ETH_MAX_PACKET;
        rbl = ETH_FRAME_SIZE;
      };
    };

    /* make sure entire packet fits in buffer - if not, will need to split into multiple buffers */
    if (rbl > b_length)
      rbl = b_length;
    item->packet.used += rbl;
    
    /* send data to host */
    //status = Map_WriteB(address, rbl, rbuf);
    if (status != SCPE_OK)
      return ew_nxm_error(xq);

    /* set receive size into RBL - RBL<10:8> maps into Status1<10:8>,
       RBL<7:0> maps into Status2<7:0>, and Status2<15:8> (copy) */
    xq->var->rring[i].rmd0 = 0;
    xq->var->rring[i].rmd1 = rbl;
    xq->var->rring[i].rmd2 = XQ_RMD2_RON | XQ_RMD2_TON;
    if (0 == (item->packet.used - rbl))
      xq->var->rring[i].rmd0 |= XQ_RMD0_STP; /* Start of Packet */
    if (item->packet.used == (item->packet.len + ETH_CRC_SIZE))
      xq->var->rring[i].rmd0 |= XQ_RMD0_ENP; /* End of Packet */
    
    if (xq->var->ReadQ.loss) {
      xq->var->rring[i].rmd2 |= XQ_RMD2_MIS; 
      sim_debug(DBG_WRN, xq->dev, "ReadQ overflow!\n");
      xq->var->stats.dropped += xq->var->ReadQ.loss;
      xq->var->ReadQ.loss = 0;          /* reset loss counter */
    }

    //Map_ReadW (rdra+(uint32)(((char *)(&xq->var->rring[xq->var->rbindx].rmd3))-((char *)&xq->var->rring)), sizeof(xq->var->rring[xq->var->rbindx].rmd3), (uint16 *)&xq->var->rring[xq->var->rbindx].rmd3);
    if (xq->var->rring[xq->var->rbindx].rmd3 & XQ_RMD3_OWN)
      xq->var->rring[i].rmd2 |= XQ_RMD2_EOR;

    /* Update receive descriptor in memory (only done after we've processed the contents) */
    /* Note: We're updating all but the end of the descriptor (which we never change) */
    /*       AND the driver will be allowed to change once the changed tmd3 (ownership) */
    /*       is noted so we avoid walking on its changes */
    xq->var->rring[i].rmd3 |= XQ_TMD3_OWN; /* Return Descriptor to Driver */
    //status = Map_WriteW (rdra+i*sizeof(xq->var->rring[i]), sizeof(xq->var->rring[i])-8, (uint16 *)&xq->var->rring[i]);
    if (status != SCPE_OK)
      return ew_nxm_error(xq);

    /* remove packet from queue */
    if (item->packet.used >= item->packet.len)
      ethq_remove(&xq->var->ReadQ);
  } while (0 == (xq->var->rring[xq->var->rbindx].rmd3 & XQ_RMD3_OWN));

  if (xq->var->rring[xq->var->rbindx].rmd3 & XQ_RMD3_OWN) {
      sim_debug(DBG_WRN, xq->dev, "ew_process_turbo_rbdl() - receive ring full\n");
  }

  if (descriptors_consumed)
    /* Interrupt for Packet Reception Completion */
    ew_setint(xq);

  return SCPE_OK;
}

t_stat ew_process_turbo_xbdl(CTLR* xq)
{
  int i;
  t_stat status = 0;
  int descriptors_consumed  = 0;
  uint32 tdra = (xq->var->init.tdra_h << 16) | xq->var->init.tdra_l;

  sim_debug(DBG_TRC, xq->dev, "ew_process_turbo_xbdl()\n");

  if ((xq->var->srr & XQ_SRR_RESP) != XQ_SRR_STRT)
    return SCPE_OK;

  /* clear transmit buffer */
  xq->var->write_buffer.len = 0;
  free (xq->var->write_buffer.oversize);
  xq->var->write_buffer.oversize = NULL;

  /* Process each descriptor in the transmit ring */
  do {
    uint32 address;
    uint16 b_length;

    i = xq->var->tbindx;

    /* Get transmit descriptor from memory */
    //status = Map_ReadW (tdra+i*sizeof(xq->var->xring[i]), sizeof(xq->var->xring[i]), (uint16 *)&xq->var->xring[i]);
    if (status != SCPE_OK)
      return ew_nxm_error(xq);

    if (xq->var->xring[i].tmd3 & XQ_TMD3_OWN)
        break;

    /* Update ring index */
    xq->var->tbindx = (xq->var->tbindx + 1) % XQ_TURBO_XM_BCNT;

    ++descriptors_consumed;
    address = ((xq->var->xring[i].hadr & 0x3F ) << 16) | xq->var->xring[i].ladr;
    b_length = (xq->var->xring[i].tmd3 & XQ_TMD3_BCT);

    /* add to transmit buffer, accomodating it if it is too big */
    if ((xq->var->write_buffer.len + b_length) > sizeof(xq->var->write_buffer.msg)) {
      xq->var->write_buffer.oversize = (uint8*)realloc (xq->var->write_buffer.oversize, xq->var->write_buffer.len + b_length);
      if (xq->var->write_buffer.len <= sizeof(xq->var->write_buffer.msg))
        memcpy (xq->var->write_buffer.oversize, xq->var->write_buffer.msg, xq->var->write_buffer.len);
      }
    //status = Map_ReadB(address, b_length, xq->var->write_buffer.oversize ? &xq->var->write_buffer.oversize[xq->var->write_buffer.len] : &xq->var->write_buffer.msg[xq->var->write_buffer.len]);
    if (status != SCPE_OK)
      return ew_nxm_error(xq);

    xq->var->write_buffer.len += b_length;
    if (!(xq->var->xring[i].tmd3 & XQ_TMD3_FOT)) {
      /* Process Loopback if in Loopback mode */
      if (xq->var->init.mode & XQ_IN_MO_LOP) {
        if ((xq->var->init.mode & XQ_IN_MO_INT) || (xq->var->etherface)) {
          /* put packet in read buffer */
          ethq_insert (&xq->var->ReadQ, 1, &xq->var->write_buffer, 0);
          status = SCPE_OK;
        } else {
          /* External loopback fails when not connected */
          status = SCPE_NOFNC;
        }
      } else
        status = eth_write(xq->var->etherface, &xq->var->write_buffer, NULL);

      xq->var->stats.xmit += 1;
      if (status != SCPE_OK) {         /* not implemented or unattached */
        sim_debug(DBG_WRN, xq->dev, "Packet Write Error!\n");
        xq->var->stats.fail += 1;
        xq->var->xring[i].tmd0 = XQ_TMD0_ERR1;
        xq->var->xring[i].tmd1 = (uint16)(100 + xq->var->write_buffer.len * 8); /* arbitrary value */
        xq->var->xring[i].tmd1 |= XQ_TMD1_LCA;
      } else {
        if (DBG_PCK & xq->dev->dctrl)
          eth_packet_trace_ex(xq->var->etherface, xq->var->write_buffer.msg, xq->var->write_buffer.len, "xq-write", DBG_DAT & xq->dev->dctrl, DBG_PCK);
        xq->var->xring[i].tmd0 = 0;
        xq->var->xring[i].tmd1 = (uint16)(100 + xq->var->write_buffer.len * 8); /* arbitrary value */
      }
      sim_debug(DBG_WRN, xq->dev, "XBDL completed processing write\n");
      /* clear transmit buffer */
      xq->var->write_buffer.len = 0;
      xq->var->xring[i].tmd2 = XQ_TMD2_RON | XQ_TMD2_TON;
    }

    //Map_ReadW (tdra+(uint32)(((char *)(&xq->var->xring[xq->var->tbindx].tmd3))-((char *)&xq->var->xring)), sizeof(xq->var->xring[xq->var->tbindx].tmd3), (uint16 *)&xq->var->xring[xq->var->tbindx].tmd3);
    if (xq->var->xring[xq->var->tbindx].tmd3 & XQ_TMD3_OWN)
      xq->var->xring[i].tmd2 |= XQ_TMD2_EOR;

    /* Update transmit descriptor in memory (only done after we've processed the contents) */
    /* Note: We're updating all but the end of the descriptor (which we never change) */
    /*       AND the driver will be allowed to change once the changed tmd3 (ownership) */
    /*       is noted so we avoid walking on its changes */
    xq->var->xring[i].tmd3 |= XQ_TMD3_OWN; /* Return Descriptor to Driver */
    //status = Map_WriteW (tdra+i*sizeof(xq->var->xring[i]), sizeof(xq->var->xring[i])-8, (uint16 *)&xq->var->xring[i]);
    if (status != SCPE_OK)
      return ew_nxm_error(xq);

  } while (0 == (xq->var->xring[xq->var->tbindx].tmd3 & XQ_TMD3_OWN));

  if (descriptors_consumed) {

    /* Interrupt for Packet Transmission Completion */
    ew_setint(xq);

    if (xq->var->coalesce_latency == 0)
      ew_svc(&xq->unit[0]);        /* service any received data */
  } else {
    /* There appears to be a bug in the VMS SCS/XQ driver when it uses chained
       buffers to transmit a packet.  It updates the transmit buffer ring in the
       correct order (i.e. clearing the ownership on the last packet segment 
       first), but it writes a transmit request to the ARQR register after adjusting
       the ownership of EACH buffer piece.  This results in us being awakened once
       and finding nothing to do.  We ignore this and the next write the ARQR will
       properly cause the packet transmission.
     */
    sim_debug(DBG_WRN, xq->dev, "ew_process_turbo_xbdl() - Nothing to Transmit\n");
  }

  return status;
}

t_stat ew_process_loopback(CTLR* xq, ETH_PACK* pack)
{
  ETH_PACK  response;
  ETH_MAC   *physical_address;
  t_stat    status;
  int offset   = 16 + (pack->msg[14] | (pack->msg[15] << 8));
  int function = pack->msg[offset] | (pack->msg[offset+1] << 8);

  sim_debug(DBG_TRC, xq->dev, "ew_process_loopback()\n");

  if (function != 2 /*forward*/)
    return SCPE_NOFNC;

  /* create forward response packet */
  memcpy (&response, pack, sizeof(ETH_PACK));
  if (xq->var->mode == XQ_T_DELQA_PLUS)
    physical_address = &xq->var->init.phys;
  else
      if (xq->var->setup.valid)
        physical_address = &xq->var->setup.macs[0];
      else
        physical_address = &xq->var->mac;

  /* The only packets we should be responding to are ones which 
     we received due to them being directed to our physical MAC address, 
     OR the Broadcast address OR to a Multicast address we're listening to 
     (we may receive others if we're in promiscuous mode, but shouldn't 
     respond to them) */
  if ((0 == (pack->msg[0]&1)) &&           /* Multicast or Broadcast */
      (0 != memcmp(physical_address, pack->msg, sizeof(ETH_MAC))))
      return SCPE_NOFNC;

  memcpy (&response.msg[0], &response.msg[offset+2], sizeof(ETH_MAC));
  memcpy (&response.msg[6], physical_address, sizeof(ETH_MAC));
  offset += 8 - 16; /* Account for the Ethernet Header and Offset value in this number  */
  response.msg[14] = offset & 0xFF;
  response.msg[15] = (offset >> 8) & 0xFF;

  /* send response packet */
  status = eth_write(xq->var->etherface, &response, NULL);
  ++xq->var->stats.loop;

  if (DBG_PCK & xq->dev->dctrl)
      eth_packet_trace_ex(xq->var->etherface, response.msg, response.len, ((function == 1) ? "xq-loopbackreply" : "xq-loopbackforward"), DBG_DAT & xq->dev->dctrl, DBG_PCK);

  return status;
}

t_stat ew_process_remote_console (CTLR* xq, ETH_PACK* pack)
{
  t_stat status;
  ETH_MAC source;
  uint16 receipt;
  int code = pack->msg[16];

  sim_debug(DBG_TRC, xq->dev, "ew_process_remote_console()\n");

  switch (code) {
    case 0x05: /* request id */
      receipt = pack->msg[18] | (pack->msg[19] << 8);
      memcpy(source, &pack->msg[6], sizeof(ETH_MAC));

      /* send system id to requestor */
      status = ew_system_id (xq, source, receipt);
      return status;
      break;
    case 0x06:  /* boot */
      /*
      NOTE: the verification field should be checked here against the
      verification value established in the setup packet. If they match the
      reboot should occur, otherwise nothing happens, and the packet
      is passed on to the host.

      Verification is not implemented, since the setup packet processing code
      isn't complete yet.

      Various values are also passed: processor, control, and software id.
      These control the various boot parameters, however SIMH does not
      have a mechanism to pass these to the host, so just reboot.
      */

      //status = ew_boot_host(xq);
      return status;
      break;
  } /* switch */

  return SCPE_NOFNC;
}

t_stat ew_process_local (CTLR* xq, ETH_PACK* pack)
{
  /* returns SCPE_OK if local processing occurred,
     otherwise returns SCPE_NOFNC or some other code */
  int protocol;

  sim_debug(DBG_TRC, xq->dev, "ew_process_local()\n");
  /* DEQNA's have no local processing capability */
  if (xq->var->type == XQ_T_DEQNA)
    return SCPE_NOFNC;

  protocol = pack->msg[12] | (pack->msg[13] << 8);
  switch (protocol) {
    case 0x0090:  /* ethernet loopback */
      return ew_process_loopback(xq, pack);
      break;
    case 0x0260:  /* MOP remote console */
      return ew_process_remote_console(xq, pack);
      break;
  }
  return SCPE_NOFNC;
}

void ew_read_callback(CTLR* xq, int status)
{
  xq->var->stats.recv += 1;

  if (DBG_PCK & xq->dev->dctrl)
    eth_packet_trace_ex(xq->var->etherface, xq->var->read_buffer.msg, xq->var->read_buffer.len, "xq-recvd", DBG_DAT & xq->dev->dctrl, DBG_PCK);

  xq->var->read_buffer.used = 0;  /* none processed yet */

  if ((xq->var->csr & XQ_CSR_RE) || (xq->var->mode == XQ_T_DELQA_PLUS)) { /* receiver enabled */
    /* process any packets locally that can be */
    t_stat status = ew_process_local (xq, &xq->var->read_buffer);

    /* add packet to read queue */
    if (status != SCPE_OK)
      ethq_insert(&xq->var->ReadQ, 2, &xq->var->read_buffer, status);
  } else {
    xq->var->stats.dropped += 1;
    sim_debug(DBG_WRN, xq->dev, "packet received with receiver disabled\n");
  }
}

void ewa_read_callback(int status)
{
  ew_read_callback(&ew_ctrl[0], status);
}

void ewb_read_callback(int status)
{
  ew_read_callback(&ew_ctrl[1], status);
}

void ew_sw_reset(CTLR* xq)
{
  uint16 set_bits = XQ_CSR_XL | XQ_CSR_RL;
  int i;

  sim_debug(DBG_TRC, xq->dev, "ew_sw_reset()\n");
  ++xq->var->stats.reset;

  /* Return DELQA-T to DELQA Normal mode */
  if (xq->var->type == XQ_T_DELQA_PLUS) {
    xq->var->mode = XQ_T_DELQA;
    xq->var->iba = xq->var->srr = 0;
  }

  /* Old DEQNA firmware also enabled interrupts and */
  /* the Ultrix 1.X driver counts on that behavior */  
  //if ((xq->var->type == XQ_T_DEQNA) && xq->dib->vec && (ULTRIX1X))
  //  set_bits |= XQ_CSR_IE;

  /* reset csr bits */
  ew_csr_set_clr(xq, set_bits, (uint16) ~set_bits);

  if (xq->var->etherface)
    ew_csr_set_clr(xq, XQ_CSR_OK, 0);

  /* clear interrupt unconditionally */
  ew_clrint(xq);

  /* flush read queue */
  ethq_clear(&xq->var->ReadQ);

  /* clear setup info */
  xq->var->setup.multicast = 0;
  xq->var->setup.promiscuous = 0;
  if (xq->var->etherface) {
    int count = 0;
    ETH_MAC zeros = {0, 0, 0, 0, 0, 0};
    ETH_MAC filters[XQ_FILTER_MAX + 1];

    /* set ethernet filter */
    /* memcpy (filters[count++], xq->mac, sizeof(ETH_MAC)); */
    for (i = 0; i < XQ_FILTER_MAX; i++)
      if (memcmp(zeros, &xq->var->setup.macs[i], sizeof(ETH_MAC)))
        memcpy (filters[count++], xq->var->setup.macs[i], sizeof(ETH_MAC));
    eth_filter (xq->var->etherface, count, filters, xq->var->setup.multicast, xq->var->setup.promiscuous);
    }

  /* Stop receive polling until the receiver is reenabled */
  ew_stop_receiver(xq);

}

/* write registers: */

t_stat ew_wr_var(CTLR* xq, int32 data)
{
  uint16 save_var = xq->var->var;
  sim_debug(DBG_REG, xq->dev, "ew_wr_var(data= 0x%08X)\n", data);
  


  return SCPE_OK;
}

t_stat ew_wr_csr(CTLR* xq, int32 data)
{
  uint16 set_bits = data & XQ_CSR_RW;                      /* set RW set bits */
  uint16 clr_bits = ((data ^ XQ_CSR_RW) & XQ_CSR_RW)       /* clear RW cleared bits */
                  |  (data & XQ_CSR_W1)                    /* write 1 to clear bits */
                  | ((data & XQ_CSR_XI) ? XQ_CSR_NI : 0);  /* clearing XI clears NI */

  sim_debug(DBG_REG, xq->dev, "ew_wr_csr(data=0x%08X)\n", data);

  /* reset controller when SR transitions to cleared */
  if (xq->var->csr & XQ_CSR_SR & ~data) {
    ew_sw_reset(xq);
    return SCPE_OK;
  }

  /* start receiver when RE transitions to set */
  if (~xq->var->csr & XQ_CSR_RE & data) {
    sim_debug(DBG_REG, xq->dev, "ew_wr_csr(data=0x%08X) - receiver started\n", data);

    /* start the read service timer or enable asynch reading as appropriate */
    ew_start_receiver(xq);
  }

  /* stop receiver when RE transitions to clear */
  if (xq->var->csr & XQ_CSR_RE & ~data) {
    sim_debug(DBG_REG, xq->dev, "ew_wr_csr(data=0x%08X) - receiver stopped\n", data);

    /* stop the read service timer or disable asynch reading as appropriate */
    ew_stop_receiver(xq);
  }

  /* update CSR bits */
  ew_csr_set_clr (xq, set_bits, clr_bits);

  return SCPE_OK;
}

void ew_start_receiver(CTLR* xq)
{
  if (!xq->var->etherface)
    return;

  /* start the read service timer or enable asynch reading as appropriate */
  if (xq->var->must_poll) {
    if (sim_idle_enab)
      sim_clock_coschedule(xq->unit, tmxr_poll);
    else
      sim_activate(xq->unit, (tmr_poll*clk_tps)/xq->var->poll);
    }
  else
    if ((xq->var->poll == 0) || (xq->var->mode == XQ_T_DELQA_PLUS))
      eth_set_async(xq->var->etherface, xq->var->coalesce_latency_ticks);
    else
      if (sim_idle_enab)
        sim_clock_coschedule(xq->unit, tmxr_poll);
      else
        sim_activate(xq->unit, (tmr_poll*clk_tps)/xq->var->poll);
}

void ew_stop_receiver(CTLR* xq)
{
  sim_cancel(&xq->unit[0]); /* Stop Receiving */
  if (xq->var->etherface)
    eth_clr_async(xq->var->etherface);
}

t_stat ew_wr_srqr(CTLR* xq, int32 data)
{
  uint16 set_bits = data & XQ_SRQR_RW;                     /* set RW set bits */

  sim_debug(DBG_REG, xq->dev, "ew_wr_srqr(data=0x%08X)\n", data);



  return SCPE_OK;
}

t_stat ew_wr_arqr(CTLR* xq, int32 data)
{
  sim_debug(DBG_REG, xq->dev, "ew_wr_arqr(data=0x%08X)\n", data);



  return SCPE_OK;
}

t_stat ew_wr_icr(CTLR* xq, int32 data)
{
  uint16 old_icr = xq->var->icr;

  sim_debug(DBG_REG, xq->dev, "ew_wr_icr(data=0x%08X)\n", data);

  xq->var->icr = data & XQ_ICR_ENA;

  if (xq->var->icr && !old_icr && xq->var->pending_interrupt)
    ew_setint(xq);

  return SCPE_OK;
}

t_stat ew_wr(int32 ldata, int32 PA, int32 access)
{
  CTLR* xq = ew_pa2ctlr(PA);
  int index = (PA >> 1) & 07;   /* word index */
  uint16 data = (uint16)ldata;

  sim_debug(DBG_REG, xq->dev, "ew_wr(data=0x%08X, PA=0x%08X[%s], access=%d)\n", data, PA, ((xq->var->mode == XQ_T_DELQA_PLUS) ? xqt_xmit_regnames[index] : ew_xmit_regnames[index]), access);

  switch (xq->var->mode) {
    case XQ_T_DELQA_PLUS:
      switch (index) {
        case 0:   /* IBAL */
          xq->var->iba = (xq->var->iba & 0xFFFF0000) | (data & 0xFFFF);
          break;
        case 1:   /* IBAH */
          xq->var->iba = (xq->var->iba & 0xFFFF) | ((data & 0xFFFF) << 16);
          break;
        case 2:   /* ICR */
          ew_wr_icr(xq, data);
          break;
        case 3:
          break;
        case 4:   /* SRQR */
          ew_wr_srqr(xq, data);
          break;
        case 5:
          break;
        case 6:
          break;
        case 7:   /* ARQR */
          ew_wr_arqr(xq, data);
          break;
      }
      break;
    default: /* DEQNA, DELQA Normal */
      switch (index) {
        case 0:   /* IBAL/XCR0 */ /* these should only be written on a DELQA-T */
          if (xq->var->type == XQ_T_DELQA_PLUS)
            xq->var->iba = (xq->var->iba & 0xFFFF0000) | (data & 0xFFFF);
          break;
        case 1:   /* IBAH/XCR1 */
          if (xq->var->type == XQ_T_DELQA_PLUS) {
            if (((xq->var->iba & 0xFFFF) == 0x0BAF) && (data == 0xFF00)) {
              xq->var->mode = XQ_T_DELQA_PLUS;
              xq->var->srr = XQ_SRR_TRBO;
              sim_cancel(xq->unit); /* Turn off receive processing until explicitly enabled */
              eth_clr_async(xq->var->etherface);
            }
            xq->var->iba = (xq->var->iba & 0xFFFF) | ((data & 0xFFFF) << 16);
          }
          break;
        case 2:   /* receive bdl low bits */
          xq->var->rbdl[0] = data;
          break;
        case 3:   /* receive bdl high bits */
          xq->var->rbdl[1] = data;
          ew_csr_set_clr(xq, 0, XQ_CSR_RL);
          ew_dispatch_rbdl(xq); /* start receive operation */
          break;
        case 4:   /* transmit bdl low bits */
          xq->var->xbdl[0] = data;
          break;
        case 5:   /* transmit bdl high bits */
          xq->var->xbdl[1] = data;
          ew_csr_set_clr(xq, 0, XQ_CSR_XL);
          ew_dispatch_xbdl(xq); /* start transmit operation */
          break;
        case 6:   /* vector address register */
          ew_wr_var(xq, data);
          break;
        case 7:   /* control and status register */
          ew_wr_csr(xq, data);
          break;
      }
      break;
  }
  return SCPE_OK;
}


/* reset device */
t_stat ew_reset(DEVICE* dptr)
{
  t_stat status;
  CTLR* ew = ewa_dev2ctlr(dptr);
  const uint16 set_bits = XQ_CSR_RL | XQ_CSR_XL;

  sim_debug(DBG_TRC, ew->dev, "ew_reset()\n");

  /* calculate MAC checksum */
  ew_make_checksum(ew);

  /* init vector address register */
  switch (ew->var->type) {
    case XQ_T_DEQNA:
      ew->var->var = 0;
      ew->var->mode = XQ_T_DEQNA;
      break;
    case XQ_T_DELQA:
    case XQ_T_DELQA_PLUS:
      ew->var->var = (ew->var->lockmode ? 0 : XQ_VEC_MS) | ((ew->var->sanity.enabled == 2) ? XQ_VEC_OS : 0);
      ew->var->mode = (ew->var->lockmode ? XQ_T_DEQNA : XQ_T_DELQA);
      break;
  }


  /* init control status register */
  ew_csr_set_clr(ew, set_bits, (uint16) ~set_bits);

  /* clear interrupts unconditionally */
  ew_clrint(ew);

  /* init read queue (first time only) */
  status = ethq_init(&ew->var->ReadQ, XQ_QUE_MAX);
  if (status != SCPE_OK)
    return status;

  /* clear read queue */
  ethq_clear(&ew->var->ReadQ);

  /* reset ethernet interface */
  if (ew->var->etherface) {
    /* restore filter on ROM mac address */
    status = eth_filter (ew->var->etherface, 1, &ew->var->mac, 0, 0);
    ew_csr_set_clr(ew, XQ_CSR_OK, 0);

    /* start service timer */
    sim_activate_after(&ew->unit[1], 250000);

    /* stop the receiver */
    eth_clr_async(ew->var->etherface);
  }

  /* stop the receiver */
  sim_cancel(ew->unit);

  /* set hardware sanity controls */
  if (ew->var->sanity.enabled) {
    ew->var->sanity.quarter_secs = XQ_HW_SANITY_SECS * 4/*qsec*/;
  }

  if (sim_switches & SWMASK ('P')) { /* Powerup? */
    memset (&ew->var->setup, 0, sizeof(ew->var->setup));
    /* Turn on all 3 DEQNA Leds */
    ew->var->setup.l1 = 1;
    ew->var->setup.l2 = 1;
    ew->var->setup.l3 = 1;
    }

  // Register/Deregister device with PCI bus.
  // This enables/disables ability of device to participate in PCI I/O dispatch.
  if ((ew->dev->flags & DEV_DIS) == 0) {    // device enabled
      pci_register (ew->pci->bus, ew->pci, ew->pci->slot_num);
  } else {
      pci_unregister(ew->pci->bus, ew->pci, ew->pci->slot_num);
  }

  return SCPE_OK;
}

void ew_reset_santmr(CTLR* xq)
{
  sim_debug(DBG_TRC, xq->dev, "ew_reset_santmr(enable=%d, qsecs=%d)\n", (xq->var->sanity.enabled ? 1 : 0), xq->var->sanity.quarter_secs);
  if (xq->var->sanity.enabled) {
    sim_debug(DBG_SAN, xq->dev, "SANITY TIMER RESETTING, qsecs: %d\n", xq->var->sanity.quarter_secs);

    /* reset sanity countdown timer to max count */
    xq->var->sanity.timer = xq->var->sanity.quarter_secs;
  }
}


t_stat ew_system_id (CTLR* xq, const ETH_MAC dest, uint16 receipt_id)
{
  static uint16 receipt = 0;
  ETH_PACK system_id;
  uint8* const msg = &system_id.msg[0];
  t_stat status;

  sim_debug(DBG_TRC, xq->dev, "ew_system_id()\n");

  /* reset system ID counter for next event */
  xq->var->idtmr = XQ_SYSTEM_ID_SECS * 4;

  if (xq->var->coalesce_latency) {
    /* Adjust latency ticks based on calibrated timer values */
    xq->var->coalesce_latency_ticks = (tmr_poll * clk_tps * xq->var->coalesce_latency) / 1000000;
    }

  if (xq->var->type == XQ_T_DEQNA) /* DELQA-only function */
    return SCPE_NOFNC;  

  memset (&system_id, 0, sizeof(system_id));
  memcpy (&msg[0], dest, sizeof(ETH_MAC));
  memcpy (&msg[6], xq->var->setup.valid ? xq->var->setup.macs[0] : xq->var->mac, sizeof(ETH_MAC));
  msg[12] = 0x60;                         /* type */
  msg[13] = 0x02;                         /* type */
  msg[14] = 0x1C;                         /* character count */
  msg[15] = 0x00;                         /* character count */
  msg[16] = 0x07;                         /* code */
  msg[17] = 0x00;                         /* zero pad */
  if (receipt_id) {
    msg[18] = receipt_id & 0xFF;          /* receipt number */
    msg[19] = (receipt_id >> 8) & 0xFF;   /* receipt number */
  } else {
    msg[18] = receipt & 0xFF;             /* receipt number */
    msg[19] = (receipt++ >> 8) & 0xFF;    /* receipt number */
  }

                                          /* MOP VERSION */
  msg[20] = 0x01;                         /* type */
  msg[21] = 0x00;                         /* type */
  msg[22] = 0x03;                         /* length */
  msg[23] = 0x03;                         /* version */
  msg[24] = 0x01;                         /* eco */
  msg[25] = 0x00;                         /* user eco */

                                          /* FUNCTION */
  msg[26] = 0x02;                         /* type */
  msg[27] = 0x00;                         /* type */
  msg[28] = 0x02;                         /* length */
  msg[29] = 0x00;                         /* value 1 ??? */
  msg[30] = 0x00;                         /* value 2 */

                                          /* HARDWARE ADDRESS */
  msg[31] = 0x07;                         /* type */
  msg[32] = 0x00;                         /* type */
  msg[33] = 0x06;                         /* length */
  memcpy (&msg[34], xq->var->mac, sizeof(ETH_MAC)); /* ROM address */

                                          /* DEVICE TYPE */
  msg[40] = 37;                           /* type */
  msg[41] = 0x00;                         /* type */
  msg[42] = 0x01;                         /* length */
  msg[43] = 0x11;                         /* value (0x11=DELQA) */
  if (xq->var->type == XQ_T_DELQA_PLUS)   /* DELQA-T has different Device ID */
    msg[43] = 0x4B;                       /* value (0x4B(75)=DELQA-T) */

  /* write system id */
  system_id.len = 60;
  status = eth_write(xq->var->etherface, &system_id, NULL);

  if (DBG_PCK & xq->dev->dctrl)
    eth_packet_trace_ex(xq->var->etherface, system_id.msg, system_id.len, "xq-systemid", DBG_DAT & xq->dev->dctrl, DBG_PCK);

  return status;
}

/*
** service routine - used for ethernet reading loop
*/
t_stat ew_svc(UNIT* uptr)
{
  CTLR* xq = ew_unit2ctlr(uptr);

  /* if the receiver is enabled */
  if ((xq->var->mode == XQ_T_DELQA_PLUS) || (xq->var->csr & XQ_CSR_RE)) {
    t_stat status;

    /* First pump any queued packets into the system */
    if ((xq->var->ReadQ.count > 0) && ((xq->var->mode == XQ_T_DELQA_PLUS) || (~xq->var->csr & XQ_CSR_RL)))
      ew_process_rbdl(xq);

    /* Now read and queue packets that have arrived */
    /* This is repeated as long as they are available */
    do {
      /* read a packet from the ethernet - processing is via the callback */
      status = eth_read (xq->var->etherface, &xq->var->read_buffer, xq->var->rcallback);
    } while (status);

    /* Now pump any still queued packets into the system */
    if ((xq->var->ReadQ.count > 0) && ((xq->var->mode == XQ_T_DELQA_PLUS) || (~xq->var->csr & XQ_CSR_RL)))
      ew_process_rbdl(xq);
  }

  /* resubmit service timer */
  if ((xq->var->must_poll) || (xq->var->poll && (xq->var->mode != XQ_T_DELQA_PLUS))) {
    if (sim_idle_enab)
      sim_clock_coschedule(uptr, tmxr_poll);
    else
      sim_activate(uptr, (tmr_poll*clk_tps)/xq->var->poll);
    }

  return SCPE_OK;
}

/*
** service routine - used for timer based activities
*/
t_stat ew_tmrsvc(UNIT* uptr)
{
  CTLR* xq = ew_unit2ctlr(uptr);

  /* has sanity timer expired? if so, reboot */
  if (xq->var->sanity.enabled)
    if (--xq->var->sanity.timer <= 0) {
      if (xq->var->mode != XQ_T_DELQA_PLUS) {
        //return ew_boot_host(xq);
      } else { /* DELQA-T Host Inactivity Timer expiration means switch out of DELQA-T mode */
        sim_debug(DBG_TRC, xq->dev, "ew_tmrsvc(DELQA-PLUS Host Inactivity Expired\n");
        xq->var->mode = XQ_T_DELQA;
        xq->var->iba = xq->var->srr = 0;
        xq->var->var = (xq->var->lockmode ? 0 : XQ_VEC_MS) | ((xq->var->sanity.enabled == 2) ? XQ_VEC_OS : 0);
      }
    }

  /* has system id timer expired? if so, do system id */
  if (--xq->var->idtmr <= 0) {
    const ETH_MAC mop_multicast = {0xAB, 0x00, 0x00, 0x02, 0x00, 0x00};
    ew_system_id(xq, mop_multicast, 0);
  }

  /* resubmit service timer */
  sim_activate_after(uptr, 250000);

  return SCPE_OK;
}


/* attach device: */
t_stat ew_attach(UNIT* uptr, char* cptr)
{
  t_stat status;
  char* tptr;
  CTLR* xq = ew_unit2ctlr(uptr);
  char buffer[80];                                          /* buffer for runtime input */

  sim_debug(DBG_TRC, xq->dev, "ew_attach(cptr=%s)\n", cptr);

  /* runtime selection of ethernet port? */
  if (*cptr == '?') {                                       /* I/O style derived from main() */
    memset (buffer, 0, sizeof(buffer));                     /* clear read buffer */
    eth_show (stdout, uptr, 0, NULL);                       /* show ETH devices */
    printf ("Select device (ethX or <device_name>)? ");     /* prompt for device */
    cptr = read_line (buffer, sizeof(buffer), stdin);       /* read command line */
    if (cptr == NULL) return SCPE_ARG;                      /* ignore EOF */
    if (*cptr == 0) return SCPE_ARG;                        /* ignore blank */
  }                                                         /* resume attaching */

  tptr = (char *) malloc(strlen(cptr) + 1);
  if (tptr == NULL) return SCPE_MEM;
  strcpy(tptr, cptr);

  xq->var->etherface = (ETH_DEV *) malloc(sizeof(ETH_DEV));
  if (!xq->var->etherface) {
    free(tptr);
    return SCPE_MEM;
    }

  status = eth_open(xq->var->etherface, cptr, xq->dev, DBG_ETH);
  if (status != SCPE_OK) {
    free(tptr);
    free(xq->var->etherface);
    xq->var->etherface = NULL;
    return status;
  }
  eth_set_throttle (xq->var->etherface, xq->var->throttle_time, xq->var->throttle_burst, xq->var->throttle_delay);
  if (xq->var->poll == 0) {
    status = eth_set_async(xq->var->etherface, xq->var->coalesce_latency_ticks);
    if (status != SCPE_OK) {
      eth_close(xq->var->etherface);
      free(tptr);
      free(xq->var->etherface);
      xq->var->etherface = NULL;
      return status;
    }
    xq->var->must_poll = 0;
  } else {
    xq->var->must_poll = (SCPE_OK != eth_clr_async(xq->var->etherface));
  }
  if (SCPE_OK != eth_check_address_conflict (xq->var->etherface, &xq->var->mac)) {
    char buf[32];

    eth_mac_fmt(&xq->var->mac, buf);     /* format ethernet mac address */
    sim_printf("%s: MAC Address Conflict on LAN for address %s, change the MAC address to a unique value\n", xq->dev->name, buf);
    eth_close(xq->var->etherface);
    free(tptr);
    free(xq->var->etherface);
    xq->var->etherface = NULL;
    return SCPE_NOATT;
  }
  uptr->filename = tptr;
  uptr->flags |= UNIT_ATT;

  /* turn on transceiver power indicator */
  ew_csr_set_clr(xq, XQ_CSR_OK, 0);

  /* init read queue (first time only) */
  status = ethq_init(&xq->var->ReadQ, XQ_QUE_MAX);
  if (status != SCPE_OK) {
    eth_close(xq->var->etherface);
    free(tptr);
    free(xq->var->etherface);
    xq->var->etherface = NULL;
    return status;
    }

  if (xq->var->mode == XQ_T_DELQA_PLUS)
    eth_filter_hash (xq->var->etherface, 1, &xq->var->init.phys, 0, xq->var->init.mode & XQ_IN_MO_PRO, &xq->var->init.hash_filter);
  else
    if (xq->var->setup.valid) {
      int i, count = 0;
      ETH_MAC zeros = {0, 0, 0, 0, 0, 0};
      ETH_MAC filters[XQ_FILTER_MAX + 1];

      for (i = 0; i < XQ_FILTER_MAX; i++)
        if (memcmp(zeros, &xq->var->setup.macs[i], sizeof(ETH_MAC)))
          memcpy (filters[count++], xq->var->setup.macs[i], sizeof(ETH_MAC));
      eth_filter (xq->var->etherface, count, filters, xq->var->setup.multicast, xq->var->setup.promiscuous);
      }
    else
      /* reset the device with the new attach info */
      ew_reset(xq->dev);

  return SCPE_OK;
}

/* detach device: */

t_stat ew_detach(UNIT* uptr)
{
  CTLR* xq = ew_unit2ctlr(uptr);
  sim_debug(DBG_TRC, xq->dev, "ew_detach()\n");

  if (uptr->flags & UNIT_ATT) {
    eth_close (xq->var->etherface);
    free(xq->var->etherface);
    xq->var->etherface = NULL;
    free(uptr->filename);
    uptr->filename = NULL;
    uptr->flags &= ~UNIT_ATT;
    /* cancel service timers */
    sim_cancel(&xq->unit[0]);
    sim_cancel(&xq->unit[1]);
  }

  /* turn off transceiver power indicator */
  ew_csr_set_clr(xq, 0, XQ_CSR_OK);

  return SCPE_OK;
}

void ew_setint(CTLR* xq)
{
  if (xq->var->mode == XQ_T_DELQA_PLUS) {
    if (!xq->var->icr) {
      xq->var->pending_interrupt = 1;
      return;
    }
    xq->var->pending_interrupt = 0;
  }

  sim_debug(DBG_TRC, xq->dev, "ew_setint() - Generate Interrupt\n");

  xq->var->irq = 1;
  SET_INT(XQ);
  return;
}

void ew_clrint(CTLR* xq)
{
  int i;
  xq->var->irq = 0;                               /* set controller irq off */
  /* clear master interrupt? */
  for (i=0; i<XQ_MAX_CONTROLLERS; i++)            /* check all controllers.. */
    if (ew_ctrl[i].var->irq) {                    /* if any irqs enabled */
      SET_INT(XQ);                                /* set master interrupt on */
      return;
    }
  CLR_INT(XQ);                                    /* clear master interrupt */
  return;
}

int32 ew_int (void)
{
#if 0
  int i;
  for (i=0; i<XQ_MAX_CONTROLLERS; i++) {
    CTLR* xq = &ew_ctrl[i];
    if (xq->var->irq) {                           /* if interrupt pending */
      ew_clrint(xq);                              /* clear interrupt */
      return xq->dib->vec;                        /* return vector */
    }
  }
#endif
  return 0;                                       /* no interrupt request active */
}

void ew_csr_set_clr (CTLR* xq, uint16 set_bits, uint16 clear_bits)
{
  uint16 saved_csr = xq->var->csr;

  /* set the bits in the csr */
  xq->var->csr = (xq->var->csr | set_bits) & ~clear_bits;

  sim_debug_bits(DBG_CSR, xq->dev, ew_csr_bits, saved_csr, xq->var->csr, 1);

  /* check and correct the state of controller interrupt */

  /* if IE is transitioning, process it */
  if ((saved_csr ^ xq->var->csr) & XQ_CSR_IE) {

    /* if IE transitioning low and interrupt set, clear interrupt */
    if ((clear_bits & XQ_CSR_IE) && xq->var->irq)
      ew_clrint(xq);

    /* if IE transitioning high, and XI or RI is high,
       set interrupt if interrupt is off */
    if ((set_bits & XQ_CSR_IE) && (xq->var->csr & XQ_CSR_XIRI) && !xq->var->irq)
      ew_setint(xq);

  } else { /* IE is not transitioning */

    /* if interrupts are enabled */
    if (xq->var->csr & XQ_CSR_IE) {

      /* if XI or RI transitioning high and interrupt off, set interrupt */
      if (((saved_csr ^ xq->var->csr) & (set_bits & XQ_CSR_XIRI)) && !xq->var->irq) {
        ew_setint(xq);

      } else {

        /* if XI or RI transitioning low, and both XI and RI are now low,
           clear interrupt if interrupt is on */
        if (((saved_csr ^ xq->var->csr) & (clear_bits & XQ_CSR_XIRI))
         && !(xq->var->csr & XQ_CSR_XIRI)
         && xq->var->irq)
          ew_clrint(xq);
      }

    } /* IE enabled */

  } /* IE transitioning */
}

/*==============================================================================
/                               debugging routines
/=============================================================================*/


void ew_debug_setup(CTLR* xq)
{
  int   i;
  char  buffer[20];

  if (!(sim_deb && (xq->dev->dctrl & DBG_SET)))
    return;

  if (xq->var->write_buffer.msg[0]) {
    sim_debug(DBG_SET, xq->dev, "%s: setup> MOP info present!\n", xq->dev->name);
    }

  for (i = 0; i < XQ_FILTER_MAX; i++) {
    eth_mac_fmt(&xq->var->setup.macs[i], buffer);
    sim_debug(DBG_SET, xq->dev, "%s: setup> set addr[%d]: %s\n", xq->dev->name, i, buffer);
  }

  if (xq->var->write_buffer.len > 128) {
    char buffer[20] = {0};
    uint16 len = (uint16)xq->var->write_buffer.len;
    if (len & XQ_SETUP_MC) strcat(buffer, "MC ");
    if (len & XQ_SETUP_PM) strcat(buffer, "PM ");
    if (len & XQ_SETUP_LD) strcat(buffer, "LD ");
    if (len & XQ_SETUP_ST) strcat(buffer, "ST ");
    sim_debug(DBG_SET, xq->dev, "%s: setup> Length [%d =0x%X, LD:%d, ST:%d] info: %s\n",
      xq->dev->name, len, len, (len & XQ_SETUP_LD) >> 2, (len & XQ_SETUP_ST) >> 4, buffer);
  }
}

void ew_debug_turbo_setup(CTLR* xq)
{
  size_t i;
  char  buffer[64] = "";

  if (!(sim_deb && (xq->dev->dctrl & DBG_SET)))
    return;

  sim_debug(DBG_SET, xq->dev, "%s: setup> Turbo Initialization Block!\n", xq->dev->name);

  if (xq->var->init.mode & XQ_IN_MO_PRO) strcat(buffer, "PRO ");
  if (xq->var->init.mode & XQ_IN_MO_INT) strcat(buffer, "INT ");
  if (xq->var->init.mode & XQ_IN_MO_DRT) strcat(buffer, "DRC ");
  if (xq->var->init.mode & XQ_IN_MO_DTC) strcat(buffer, "DTC ");
  if (xq->var->init.mode & XQ_IN_MO_LOP) strcat(buffer, "LOP ");
  sim_debug(DBG_SET, xq->dev, "%s: setup> set Mode: %s\n", xq->dev->name, buffer);

  eth_mac_fmt(&xq->var->init.phys, buffer);
  sim_debug(DBG_SET, xq->dev, "%s: setup> set Physical MAC Address: %s\n", xq->dev->name, buffer);

  buffer[0] = '\0';
  for (i = 0; i < sizeof(xq->var->init.hash_filter); i++) 
    sprintf(&buffer[strlen(buffer)], "%02X ", xq->var->init.hash_filter[i]);
  sim_debug(DBG_SET, xq->dev, "%s: setup> set Multicast Hash: %s\n", xq->dev->name, buffer);

  buffer[0] = '\0';
  if (xq->var->init.options & XQ_IN_OP_HIT) strcat(buffer, "HIT ");
  if (xq->var->init.options & XQ_IN_OP_INT) strcat(buffer, "INT ");
  sim_debug(DBG_SET, xq->dev, "%s: setup> set Options: %s\n", xq->dev->name, buffer);

  sim_debug(DBG_SET, xq->dev, "%s: setup> set Vector: %d =0x%X\n", 
            xq->dev->name, xq->var->init.vector, xq->var->init.vector);

  sim_debug(DBG_SET, xq->dev, "%s: setup> set Host Inactivity Timeout: %d seconds\n", 
            xq->dev->name, xq->var->init.hit_timeout);

  buffer[0] = '\0';
  for (i = 0; i < sizeof(xq->var->init.bootpassword); i++) 
    sprintf(&buffer[strlen(buffer)], "%02X ", xq->var->init.bootpassword[i]);

  sim_debug(DBG_SET, xq->dev, "%s: setup> set Boot Password: %s\n", xq->dev->name, buffer);

  sim_debug(DBG_SET, xq->dev, "%s: setup> set Receive Ring Buffer Address:  %02X%04X\n", 
            xq->dev->name, xq->var->init.rdra_h, xq->var->init.rdra_l);
  sim_debug(DBG_SET, xq->dev, "%s: setup> set Transmit Ring Buffer Address: %02X%04X\n", 
            xq->dev->name, xq->var->init.tdra_h, xq->var->init.tdra_l);
}

t_stat ew_boot (int32 unitno, DEVICE *dptr)
{
#ifdef VM_PDP11
size_t i;
DIB *dib = (DIB *)dptr->ctxt;
CTLR *xq = ew_unit2ctlr(&dptr->units[unitno]);
uint16 *bootrom = NULL;
extern int32 REGFILE[6][2];                 /* R0-R5, two sets */
extern uint16 *M;                           /* Memory */

if (xq->var->type == XQ_T_DEQNA)
  bootrom = ew_bootrom_deqna;
else
  if (xq->var->type == XQ_T_DELQA)
    bootrom = ew_bootrom_delqa;
  else
    if (xq->var->type == XQ_T_DELQA_PLUS)
      bootrom = ew_bootrom_delqat;

for (i = 0; i < BOOT_LEN; i++)
    M[(BOOT_START >> 1) + i] = bootrom[i];
cpu_set_boot (BOOT_ENTRY);
REGFILE[0][0] = 0;
REGFILE[1][0] = dib->ba;
return SCPE_OK;
#else
return SCPE_NOFNC;
#endif
}

t_stat ew_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
const char helpString[] =
 /* The '*'s in the next line represent the standard text width of a help line */
     /****************************************************************************/
//  " The %1s is a communication subsystem which consists of a microprocessor\n"
    " The DELQA-T/DELQA/DEQNA Qbus devices interface the %S processors\n"
    " to an Ethernet Local Area Network (LAN).\n"
    "\n"
#ifdef VM_PDP11
    " The controllers are compatible with both 18- and 22-bit Qbus backplanes.\n"
    "\n"
#endif
    " The simulator implements two DELQA-T/DELQA/DEQNA Qbus Ethernet controllers\n"
    " (XQ, XQB).  Initially, XQ is enabled, and XQB is disabled.\n"
    "1 Hardware Description\n"
    " The %D conforms to the Ethernet 2.0 specification performing the\n"
    " data link layer functions, and part of the physical layer functions.\n"
    "2 Models\n"
    "3 DEQNA\n"
    " A M7504 Qbus Module.  The DELQA module is a dual-height module which\n"
    " plugs directly into the Qbus backplane.\n"
    "3 DELQA\n"
    " A M7516 Qbus Module.  The DELQA module is a dual-height module which\n"
    " plugs directly into the Qbus backplane.\n"
    "3 DELQA-T\n"
    " A M7516-YM Qbus Module.  The DELQA-T, also known as the DELQA-PLUS,\n"
    " is a dual-height module which plugs directly into the Qbus backplane.\n"
    "\n"
    " The DELQA-T device has an extended register programming interface\n"
    " which is more efficient than the initial DEQNA and DELQA model.\n"
    "2 $Registers\n"
    "\n"
    " These registers contain the emulated state of the device.  These values\n"
    " don't necessarily relate to any detail of the original device being\n"
    " emulated but are merely internal details of the emulation.\n"
    "1 Configuration\n"
    " A %D device is configured with various SET and ATTACH commands\n"
     /****************************************************************************/
    "2 $Set commands\n"
    "3 MAC\n"
    " The MAC address of the controller is the Hardware MAC address which on\n"
    " real hardware is uniquely assigned by the factory.  Each LAN device on a\n"
    " network must have unique MAC addresses for proper operation.\n"
    "\n"
    "+sim> SET %D MAC=<mac-address>\n"
    "\n"
    " A Valid MAC address is comprised of 6 pairs of hex digits delimited by\n"
    " dashes, colons or period characters.\n"
    "\n"
    " The default MAC address for the XQ device is 08-00-2B-AA-BB-CC.  The\n"
    " default MAC address for the XQB device is 08-00-2B-BB-CC-DD.\n"
    "\n"
    " The SET MAC command must be done before the %D device is attached to a\n"
    " network.\n"
    "3 Type\n"
    " The type of device being emulated can be changed with the following\n"
    " command:\n"
    "\n"
    "+sim> SET %D TYPE={DEQNA|DELQA|DELQA-T}\n"
    "\n"
    " A SET TYPE command should be entered before the device is attached.\n"
     /****************************************************************************/
    "3 SANITY\n"
    " The sanity timer exists to make sure that the simulated operating system\n"
    " software is up and running.  The sanity timer is also known as the host\n"
    " inactivity timer.\n"
    " The timer is reset by the operating system device driver interacting with\n"
    " the device.  If the timer expires, the device negates the Qbus DCOK signal\n"
    " which causes the system to reboot.\n"
    "\n"
    " The initial state of the sanity timer on real DEQNA hardware is configured\n"
    " with the switch W4 and is switch S4 on DELQA boards.  The SET %D SANITY\n"
    " command exists to reflect the setting of this switch.\n"
    "3 DEQNALOCK\n"
    " Setting DEQNALock mode causes a DELQA or DELQA-T device to behaves exactly\n"
    " like a DEQNA, except for the operation of the VAR and MOP processing.\n"
    "3 POLL\n"
#if defined(USE_READER_THREAD) && defined(SIM_ASYNCH_IO)
    " The SET %D POLL command changes the service polling timer.  Scheduled\n"
    " service polling is unnecessary and inefficient when asynchronous I/O is\n"
    " available, therefore the default setting is disabled.\n"
#else /* !(defined(USE_READER_THREAD) && defined(SIM_ASYNCH_IO)) */
    " The SET %D POLL command changes the service polling timer.  The polling\n"
    " timer is calibrated to run the service thread on each simulated system clock\n"
    " tick.  This should be sufficient for most situations, however if desired more\n"
    " frequent polling can be specified.  Polling too frequent can seriously impact\n"
    " the simulator's ability to execute instructions efficiently.\n"
#endif /* defined(USE_READER_THREAD) && defined(SIM_ASYNCH_IO) */
     /****************************************************************************/
    "3 THROTTLE\n"
    " The faster network operation of a simulated DELQA-T/DELQA/DEQNA device\n"
    " might be too fast to interact with real PDP11 or VAX systems running on\n"
    " the same LAN.\n"
    " Traffic from the simulated device can easily push the real hardware\n"
    " harder than it ever would have seen historically.  The net result can\n"
    " be excessive packet loss due to various over-run conditions.  To support\n"
    " interoperation of simulated systems with legacy hardware, the simulated\n"
    " system can explictly be configured to throttle back the traffic it puts\n"
    " on the wire.\n"
    "\n"
    " Throttling is configured with the SET XQ THROTTLE commands:\n"
    "\n"
    "+sim> SET XQ THROTTLE=DISABLE\n"
    "+sim> SET XQ THROTTLE=ON\n"
    "+sim> SET XQ THROTTLE=TIME=n;BURST=p;DELAY=t\n"
    "\n"
    " TIME specifies the number of milliseconds between successive packet\n"
    " transmissions which will trigger throttling.\n"
    " BURST specifies the number of successive packets which each are less than\n"
    " the TIME gap that will cause a delay in sending subsequent packets.\n"
    " DELAY specifies the number of milliseconds which a throttled packet will\n"
    " be delayed prior to its transmission.\n"
    "\n"
     /****************************************************************************/
    "2 Attach\n"
    " The device must be attached to a LAN device to communicate with systems\n"
    " on that LAN\n"
    "\n"
    "+sim> SHOW %D ETH\n"
    "+ETH devices:\n"
#if defined(_WIN32)
    "+ eth0   \\Device\\NPF_{A6F81789-B849-4220-B09B-19760D401A38} (Local Area Connection)\n"
    "+ eth1   udp:sourceport:remotehost:remoteport               (Integrated UDP bridge support)\n"
    "+sim> ATTACH %D eth0\n"
#else
    "+ eth0   en0      (No description available)\n"
    "+ eth1   tap:tapN (Integrated Tun/Tap support)\n"
    "+ eth2   udp:sourceport:remotehost:remoteport               (Integrated UDP bridge support)\n"
    "+sim> ATTACH %D eth0\n"
    "+sim> ATTACH %D en0\n"
#endif
    "+sim> ATTACH %D udp:1234:remote.host.com:1234\n"
    "\n"
    "2 Examples\n"
    " To configure two simulators to talk to each other use the following\n"
    " example:\n"
    " \n"
    " Machine 1\n"
    "+sim> SET %D ENABLE\n"
    "+sim> SET %D PEER=LOCALHOST:2222\n"
    "+sim> ATTACH %D 1111\n"
    " \n"
    " Machine 2\n"
    "+sim> SET %D ENABLE\n"
    "+sim> SET %U PEER=LOCALHOST:1111\n"
    "+sim> ATTACH %U 2222\n"
    "\n"
    "1 Monitoring\n"
    " The %D device configuration and state can be displayed with one of the\n"
    " available show commands.\n"
    "2 $Show commands\n"
    "3 LEDs\n"
    " The %D devices have on-board LEDS which are used by the operating system,\n"
    " boot code, and diagnostics to indicate the state of the device.  The LED\n"
    " state is visible with the SHOW %D LEDS command.\n"
    "1 Boot Support\n"
#ifdef VM_PDP11
    " The %D device is bootable using the on-board ROM code in the PDP-11\n"
    " simulator.\n"
#else
    " The %D device is bootable via the processor boot ROM on all MicroVAX\n"
    " systems.\n"
#endif
    "1 Debugging\n"
    " The simulator has a number of debug options, these are:\n"
    "\n"
    "++TRACE   Shows detailed routine calls.\n"
    "++CSR     Shows activities affecting the CSR.\n"
    "++VAR     Shows activities affecting the VAR.\n"
    "++WARN    Shows warnings.\n"
    "++SETUP   Shows setup info.\n"
    "++SANITY  Shows sanity timer info.\n"
    "++REG     Shows all device register programatic read/write activity\n"
    "++PACKET  Shows packet headers.\n"
    "++DATA    Shows packet data.\n"
    "++ETH     Shows ethernet device details.\n"
    "\n"
    " To get a full trace use\n"
    "\n"
    "+sim> SET %D DEBUG\n"
    "\n"
     /****************************************************************************/
    "1 Dependencies\n"
#if defined(_WIN32)
    " The WinPcap package must be installed in order to enable\n"
    " communication with other computers on the local LAN.\n"
    "\n"
    " The WinPcap package is available from http://www.winpcap.org/\n"
#else
    " To build simulators with the ability to communicate to other computers\n"
    " on the local LAN, the libpcap development package must be installed on\n"
    " the system which builds the simulator.\n"
    "\n"
#if defined(__APPLE__)
#else
#if defined(__linux__)
#else
#endif
#endif
#endif
    "1 Privileges Required\n"
#if defined(_WIN32)
    " Windows systems can attach the simulated %D device to the local LAN\n"
    " network interface without any special privileges as long as the\n"
    " WinPcap package has been previously installed on the host system.\n"
#else
#endif
    "1 Host Computer Communications\n"
#if defined(_WIN32)
    " On Windows using the WinPcap interface, the simulated %D device\n"
    " can be used to communicate with the host computer on the same LAN\n"
    " which it is attached to.\n"
#else
#endif
     /****************************************************************************/
    "1 Performance\n"
    " On modern host systems and networks, the simulated DEQNA/DELQA/DELQA-T\n"
    " device can easily move data at more than 20Mbits per second.\n"
    " Real DEQNA/DELQA hardware rarely exceeded more than 1.5Mbits/second\n"
    "\n"
    " Due to this significant speed mismatch, there can be issues when\n"
    " simulated systems attempt to communicate with real PDP11 and VAX systems\n"
    " on the LAN.  See SET %D THROTTLE to help accommodate such communications.\n"
    "1 Related Devices\n"
    " The %D can facilitate communication with other simh simulators which\n"
    " have emulated Ethernet devices available as well as real systems that\n"
    " are directly connected to the LAN.\n"
    "\n"
    " The other simulated Ethernet devices include:\n"
    "\n"
    "++DEUNA/DELUA  Unibus PDP11 and VAX simulators\n"
    "\n"
    ;
return scp_help (st, dptr, uptr, flag, helpString, cptr);
}

const char *ew_description (DEVICE *dptr)
{
return (dptr == &ewa_dev) ? "DELQA/DEQNA Ethernet controller"
                         : "Second DELQA/DEQNA Ethernet controller";
}

void ew_software_reset(CTLR* EW)
{
    //TODO
}

void ew_receive_poll_demand (CTLR* ew)
{
    //TODO
}

void ew_transmit_poll_demand (CTLR* ew)
{
    //TODO
}

void ew_raise_interrupt(CTLR* ew)
{
   // PCI_STAT pci_resl;
    uint16 int_pin  = (ew->pci->cfg_reg[0].csr[EW_CFIT_IDX] & PCI_CFG15_INT_PIN)  >> PCI_CFG15_INT_PIN_V;
    uint16 int_line = (ew->pci->cfg_reg[0].csr[EW_CFIT_IDX] & PCI_CFG15_INT_LINE) >> PCI_CFG15_INT_LINE_V;
    //TODO - This behavior is a guess based on the contents of the CFIT (CFG15) registeer
    sim_debug(DBG_WRN, ew->dev, "ew_raise_interrupt: cfg15: INT_PIN(INT%H), INT_LINE(%H)\n", int_pin, int_line);
    //pci_resl = pci_hose_interrupt(int_pin, int_line);
}

void ew_recalculate_interrupt(CTLR* ew)
{
    // Reset/Set summary if there are any unmasked bits that need to be summarized
    if ((ew->var->csrs[5] & ~ew->var->csrs[7]/*unmasked*/  & EW_CSR5_NIS_SUM/*summarized bits*/) == 0) {
        ew->var->csrs[5] &= ~EW_CSR5_NIS;   // Reset summary
    } else {
        ew->var->csrs[5] |= EW_CSR5_NIS;    // Set summary
    }
    if ((ew->var->csrs[5] & ~ew->var->csrs[7]/*unmasked*/ & EW_CSR5_AIS_SUM)/*summarized bits*/ == 0) {
        ew->var->csrs[5] &= ~EW_CSR5_AIS;   // Reset summary
    } else {
        ew->var->csrs[5] |= EW_CSR5_AIS;    // Set summary
    }

    // Do we need to raise interrupt?
    if ((ew->var->csrs[5] & ew->var->csrs[7]) != 0) {
        ew_raise_interrupt(ew);
    }
}


PCI_STAT ew_pci_reset (struct pci_device_t* This)
{
    // Find which controller this is
    CTLR* ew = ew_pci2ctlr (This);

    sim_debug (DBG_TRC, ew->dev, "ew_pci_reset: hard reset\n");

    // reset config registers to default values
    memcpy(ew->var->cfg_reg, INTEL_21140_CFG_DATA, sizeof(INTEL_21140_CFG_DATA));

    // reset CSRs to default values
    memcpy(ew->var->csrs, INTEL_21140_CSR_DATA, sizeof(INTEL_21140_CSR_DATA));

    return PCI_OK;
}

PCI_STAT ew_pci_cfg_read (struct pci_device_t* This, int type, int bus, int slot, int func, int reg, int lnth, int byt_off, t_uint64* value)
{
    CTLR* ew = ew_pci2ctlr (This);          // Find affected controller
    int    index = (reg & 0xFF) >> 2;       // index of 32-bit configuration array[64]
    uint32 result = ew->var->cfg_reg[index];   // temporary result

    sim_debug (DBG_TRC, ew->dev, "ew_pci_cfg_read: \n");

    // Type 1 read is passthrough to inferior bus on bridge; this isn't a bridge so ignore
    // This is only a single function (external port) card, so ignore non-zero function reads
    if ((type == 1) || (func != 0)) {
        *value = 0;
        return PCI_OK;
    }
    if ((lnth !=4) || (byt_off !=0)) {
        // this is a warning; we may need to modify result with a byte shift or bit mask.
        // Generally, config reads are 32 bits at a time (lnth 4), but it is possible that
        // byte-offset and byte masking or a 64-bit read attempt has occurred.
        // It *is* possible to have a 64-bit configuration register, as shown by the
        // memory base address register "type" field decode of 0x10 = locate anywhere in 64-bit
        // address space (PCI 2.1, sec 6.2.5.1)
        sim_debug(DBG_WRN, ew->dev, "ew_pci_cfg_read: unaligned read!\n");
    }

    // put result into return value; may need to bit shift or byte mask
    *value = result;
    return PCI_OK;
}

PCI_STAT ew_pci_cfg_write (struct pci_device_t* This, int type, int bus, int slot, int func, int reg, int lnth, int byt_off, t_uint64 value)
{
    CTLR* ew = ew_pci2ctlr (This);          // Find affected controller
    int    index = (reg & 0xFF) >> 2;       // index of 32-bit configuration array[64]
    // uint32 old_value = ew->var->cfg_reg[index];   // save old register value for comparison later

    // Type 1 read is passthrough to inferior bus on bridge; this isn't a bridge so ignore
    // This is only a single function (external port) card, so ignore non-zero function reads
    if ((type == 1) || (func != 0)) {
        return PCI_OK;
    }
    if ((lnth !=4) || (byt_off !=0)) {
        // this is a warning; we may need to modify result with a byte shift or bit mask.
        // Generally, config writes are 32 bits at a time (lnth 4), but it is possible that
        // byte-offset and byte masking or a 64-bit write attempt has occurred.
        // It *is* possible to have a 64-bit configuration register, as shown by the
        // memory base address register "type" field decode of 0x10 = locate anywhere in 64-bit
        // address space (PCI 2.1, sec 6.2.5.1)
        sim_debug(DBG_WRN, ew->dev, "ew_pci_cfg_write: unaligned write!\n");
    }

    // write register
    ew->var->cfg_reg[index] = value & INTEL_21140_WMASK[index];
    
    return PCI_OK;
}

PCI_STAT ew_csr_read (CTLR* ew, int offset, int lnth, t_uint64* value)
{
    int index = offset >> 2;                // index into csrs

    switch (index) {
    case 0:     // CSR0 - Bus Mode 00H
    case 1:     // CSR1 - Transmit Poll Demand 08H
    case 2:     // CSR2 - Receive Poll Demand 10H
    case 3:     // CSR3 - Receive Descriptor List Base Address 18H
    case 4:     // CSR4 - Transmit Descriptor List Base Address 20H
    case 5:     // CSR5 - Status Register 28H
    case 6:     // CSR6 - Operation Mode 30H
    case 7:     // CSR7 - Interrupt Enable 38H
    case 9:     // CSR9 - Boot/Serial ROM and MII 48H
    case 10:    // CSR10 - Boot ROM Programming Address 50H
    case 11:    // CSR11 - Genral Puropse Timer and Interrupt Mitigation Control 58H
    case 12:    // CSR12 - SIA Status 60H
    case 13:    // CSR13 - SIA Connectivity 68H
    case 14:    // CSR14 - SIA Transmit and Receive 70H
    case 15:    // CSR15 - SIA and General-Purpose Port 78H
        *value = ew->var->csrs[index];
        break;
    case 8:     // CSR8 - Missed Frames and Overflow Counter 40H
        *value = ew->var->csrs[index];
        ew->var->csrs[index] = 0;       // All counters cleared on read
        break;
    default:
        *value = 0;     // I/O and Memory reads to unused/reserved addresses return zero
        break;
    }
    return PCI_OK;
}

PCI_STAT ew_csr_write (CTLR* ew, int offset, int lnth, t_uint64 value)
{
    int index = offset >> 2;                    // index into csrs
    uint32  old_value = ew->var->csrs[index];   // save old CSR value
    uint32  new_value;

    // Define masks to simplify CSR write logic -
    //   ROmask, RWmask, and W1mask must cover all register bits!
    uint32 wmask =  *ew->var->csrs_wmask[index];
    uint32 w1mask = *ew->var->csrs_w1mask[index];
    uint32 romask = ~(wmask | w1mask);

    // Calculate new CSR value. Logic:
    //   1> Save old RO and W1C bits
    //   2> Add new RW bits
    //   3> Clear old W1C bits that are set in new W1C
    new_value = ((old_value & (romask | w1mask)) /*1*/ | (value & wmask) /*2*/)
              & ~(value & w1mask) /*3*/;

    // Apply the write operation - some CSRs have side effects
    switch (index) {
    case 0:         // CSR0 - Bus Mode Register 00H
        ew->var->csrs[index] = new_value;
        if ((new_value & EW_CSR0_SWR) != 0) {
            ew_software_reset(ew);
        }
        break;
    case 1:         // CSR1 - Transmit Poll Demand 08H
        ew->var->csrs[index] = new_value;
        ew_transmit_poll_demand(ew);
        break;
    case 2:         // CSR2 - Receive Poll Demand 10H
        ew->var->csrs[index] = new_value;
        ew_receive_poll_demand(ew);
        break;
    case 3:         // CSR3 - Receive Descriptor List Base 18H
        if (ew->var->rx_state == STATE_STOPPED) {
            ew->var->csrs[index] = new_value & 0xFFFFFFFCul;    // longword align
            ew->var->rx_curr_base = ew->var->csrs[index];       // Set current receive descriptor
        } else {
            sim_debug(DBG_WRN, ew->dev, "ew_csr_write: Attempt to write CSR3 when receive process is not stopped.\n");
        }
        break;
    case 4:         // CSR4 - Transmit Descriptor List Base 20H
        if (ew->var->tx_state == STATE_STOPPED) {
            ew->var->csrs[index] = new_value & 0xFFFFFFFCul;    // longword align
            ew->var->tx_curr_base = ew->var->csrs[index];       // Set current transmit descriptor
        } else {
            sim_debug(DBG_WRN, ew->dev, "ew_csr_write: Attempt to write CSR4 when transmit process is not stopped.\n");
        }
        break;
    case 5:         // CSR5 - Status Register 28H
        ew->var->csrs[index] = new_value;
        break;
    case 6:         // CSR6 - Operation Mode Register 30H
        {
            uint32 changed = old_value ^ new_value;
            uint32 went_low = changed & ~new_value;
            uint32 went_high = changed & new_value;

            // write new value into register
            ew->var->csrs[index] = new_value;

            // deal with the changed values
            //TODO
        }
        break;
    case 7:         // CSR7 - Inerrupt Enable Register 38H
        ew->var->csrs[index] = new_value;
        ew_recalculate_interrupt(ew);
        break;
    case 8:         // CSR8 - Missed Frames and Overflow Counter Register 40H
        // This is a read-only register!! Idiot ...
        sim_debug(DBG_WRN, ew->dev, "ew_csr_write: Attempt to write to read-only CSR8!\n");
        break;
    case 9:         // CSR9 - Boot/Serial ROM, and MII Management Register 48H
        //TODO
        break;
    case 10:        // CSR10 - Boot ROM Programming Address 50H
        // Boot ROM is only for Intel systems - not implemented at this time.
        ew->var->csrs[index] = new_value;
        break;
    case 11:        // CSR11 - General-Purpose Timer and Interrupt Mitigation Control Register 58H
        ew->var->csrs[index] = new_value;
        break;
    case 12:        // CSR12 - SIA Status Register 60H
        ew->var->csrs[index] = new_value;
        break;
    case 13:        // CSR13 - SIA Connctivity Register 68H
        ew->var->csrs[index] = new_value;
        break;
    case 14:        // CSR14 - SIA Transmit and Receive Register 70H
        ew->var->csrs[index] = new_value;
        break;
    case 15:        // CSR15 - SAI and General-Purpose Port Register 78H
        ew->var->csrs[index] = new_value;
        break;
    default:
        break;     // May ignore writes unused/reserved addresses
    };
    return PCI_OK;
}

PCI_STAT ew_pci_io_read (PCI_DEV* _this, t_uint64 pci_addr, int lnth, t_uint64* value)
{
    CTLR* ew = ew_pci2ctlr (_this);                     // Affected controller
    int offset;                                         // Offset from Base Address Register (BAR)

    // I/O space access enabled? If not, return PCI_NOT_ME
    if ((ew->pci->cfg_reg[0].csr[EW_CFCS_IDX] & EW_CFCS_IOSA) == 0) {
        return PCI_NOT_ME;
    }
    if (((pci_addr ^ ew->pci->cfg_reg[0].csr[EW_CBIO_IDX]) & ew->pci->cfg_wmask[0].csr[EW_CBIO_IDX]) != 0) {
        return PCI_NOT_ME;
    }

    // IO operations can only access the CSRs, offset 0H-7FH, per 21143 Manual, Registers, Sec 3.0
    offset = (int)(pci_addr - ew->pci->cfg_reg[0].csr[EW_CBIO_IDX]);  // Get offset from base address

    // dispatch CSR operation
    if ((offset >= 0x00) && (offset < 0x80)) {
        return ew_csr_read (ew, offset, lnth, value);
    } else {
        // ignore invalid offsets
        sim_debug(DBG_WRN, ew->dev, "ew_pci_io_read: %S invalid IO register access (%x)\n", ew->dev->name, offset);
        *value = 0;
        return PCI_OK;
    }
}

PCI_STAT ew_pci_io_write (PCI_DEV* _this, t_uint64 pci_addr, int lnth, t_uint64 value)
{
    CTLR* ew = ew_pci2ctlr (_this);                     // Affected controller
    int offset;                                         // Offset from Base Address Register (BAR)

    // I/O space access enabled? If not, return PCI_NOT_ME
    if ((ew->pci->cfg_reg[0].csr[EW_CFCS_IDX] & EW_CFCS_IOSA) == 0) {
        return PCI_NOT_ME;
    }
    // Does the controller need to respond to this access? If not, return PCI_NOT_ME
    if (((pci_addr ^ ew->pci->cfg_reg[0].csr[EW_CBIO_IDX]) & ew->pci->cfg_wmask[0].csr[EW_CBIO_IDX]) != 0) {
        return PCI_NOT_ME;
    }

    // IO operations can only access the CSRs, offset 0H-7FH, per 21143 Manual, Registers, Sec 3.0
    offset = (int)(pci_addr - ew->pci->cfg_reg[0].csr[EW_CBIO_IDX]);  // Get offset from base address

    // dispatch CSR operation
    if ((offset >= 0x00) && (offset < 0x80)) {
        return ew_csr_write (ew, offset, lnth, value);
    } else {
        // ignore invalid offsets
        sim_debug(DBG_WRN, ew->dev, "ew_pci_io_write: %S invalid IO register access (%x)\n", ew->dev->name, offset);
        return PCI_OK;
    }
}

PCI_STAT ew_pci_mem_read (PCI_DEV* _this, t_uint64 pci_addr, int lnth, t_uint64* value)
{
    CTLR* ew = ew_pci2ctlr (_this);                     // Affected controller
    int offset;                                         // Offset from Base Address Register (BAR)

    // Memory space access enabled? If not, return PCI_NOT_ME
    if ((ew->pci->cfg_reg[0].csr[EW_CFCS_IDX] & EW_CFCS_MSA) == 0) {
        return PCI_NOT_ME;
    }
    // Does the controller need to respond to this access? If not, return PCI_NOT_ME
    if (((pci_addr ^ ew->pci->cfg_reg[0].csr[EW_CBMA_IDX]) & ew->pci->cfg_wmask[0].csr[EW_CBMA_IDX]) != 0) {
        return PCI_NOT_ME;
    }

    // PCI Memory reads can access the following offsets, per 21143 Manual, Sec 3, Registers
    //   0H- 79H  CSRs
    //  80H- 8FH  Cardbus Status Changed Registers
    //  90H-1FFH  Reserved
    // 200H-3FFH  Serial ROM [21143v4 only]
    offset = (int)(pci_addr - ew->pci->cfg_reg[0].csr[EW_CBMA_IDX]); // Get offset from base memory address
     
    if ((offset >= 0x00) && (offset < 0x80)) {          // dispatch CSR
        return ew_csr_read (ew, offset, lnth, value);
    } else if ((offset >= 0x80) && (offset < 0x90)) {   // dispatch Cardbus registers
        int index = (offset - 0x80) >> 2;
        *value = ew->var->cardbus[index];
        return PCI_OK;
    } else if ((offset >= 0x90) && (offset < 0x200)) {  // dispatch Reserved (ignored)
        *value = 0;
        return PCI_OK;
    } else if ((offset >=0x200) && (offset < 0x400)) {  // dispatch Serial ROM
        int index = (offset - 0x200) >> 2;
        *value = ew->var->rom[index];
        return PCI_OK;
    } else {                                            // ignore invalid offsets
        *value = 0;
        return PCI_OK;
    }
}

PCI_STAT ew_pci_mem_write (PCI_DEV* _this, t_uint64 pci_addr, int lnth, int offset_b, t_uint64 value)
{
    CTLR* ew = ew_pci2ctlr (_this);                     // Affected controller
    int offset;                                         // Offset from Base Address Register (BAR)

    // Memory space access enabled? If not, return PCI_NOT_ME
    if ((ew->pci->cfg_reg[0].csr[EW_CFCS_IDX] & EW_CFCS_MSA) == 0) {
        return PCI_NOT_ME;
    }
    // Does the controller need to respond to this access? If not, return PCI_NOT_ME
    if (((pci_addr ^ ew->pci->cfg_reg[0].csr[EW_CBMA_IDX]) & ew->pci->cfg_wmask[0].csr[EW_CBMA_IDX]) != 0) {
        return PCI_NOT_ME;
    }

    // PCI Memory reads can access the following offsets, per 21143 Manual, Sec 3, Registers
    //   0H- 79H  CSRs
    //  80H- 8FH  Cardbus Status Changed Registers
    //  90H-1FFH  Reserved
    // 200H-3FFH  Serial ROM
    offset = (int)(pci_addr - ew->pci->cfg_reg[0].csr[EW_CBMA_IDX]); // Get offset from base address

    if ((offset >= 0x00) && (offset < 0x80)) {          // dispatch CSR
        return ew_csr_write (ew, offset, lnth, value);
    } else if ((offset >= 0x80) && (offset < 0x90)) {   // dispatch Cardbus registers
        int index = (offset - 0x80) >> 2;
        ew->var->cardbus[index] = (value & 0xFFFF);
        return PCI_OK;
    } else if ((offset >= 0x90) && (offset < 0x200)) {  // dispatch Reserved (ignored)
        return PCI_OK;
    } else if ((offset >=0x200) && (offset < 0x400)) {  // dispatch Serial ROM
        int index = (offset - 0x200) >> 2;
        ew->var->rom[index] = (value & 0xFFFF);
        return PCI_OK;
    } else {                                            // ignore invlid offsets
        return PCI_OK;
    }
}

