/* pci_dec21x4x.h: PCI Ethernet adapter, using DEC Tulip chips

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

*/

/*
	This Ethernet PCI adapter is based on the following chips:
       DEC21140 (DE435) and Intel21143 (DE500)

    All of the chips in the 21x4x series belong to the "Tulip" family, and
    behave similarly to the ethernet technology developed on the QBus Ethernet
    controller of the PDP11 and VAX lines. If you need to see the OS behavior,
    look for source files mentioning "Tulip".

    DEC sold the intellectual property rights to the 21x4x Ethernet chip
    line to Intel after the 21041 chip, so the 2104x were labelled
    DEC and the 2114x chips were labelled Intel.

	Documentation:
*/

/*
	History:
	--------
    2016-05-10  DTH  Added all EW_CSRx_* bitmasks
	2016-04-11  DTH  Started
*/

#ifndef __pci_dec21x4x_h
#define __pci_dec21x4x_h

#include "sim_defs.h"
#include "sim_ether.h"
#include "sim_pci.h"

//+ Hacks to get pdp11_xq.c to compile

/* Qbus I/O page layout - see pdp11_io_lib.c for address layout details */
#define IOBA_AUTO       (0)                             /* Assigned by Auto Configure */
#define VEC_Q           0x200                           /* Qbus vector offset */
#define SET_INT(dv)     0
#define CLR_INT(dv)     0
//- Hacks to get pdp11-xq.c to compile

#include "alpha_defs.h"
#define XQ_RDX          16
#define XQ_WID          32
extern int32 PSL;                                       /* PSL */
extern int32 fault_PC;                                  /* fault PC */
extern int32 int_req[IPL_HLVL];

#define XQ_QUE_MAX           500                        /* read queue size in packets */
#define XQ_FILTER_MAX         14                        /* number of filters allowed */
#if defined(SIM_ASYNCH_IO) && defined(USE_READER_THREAD)
#define XQ_SERVICE_INTERVAL  0                          /* polling interval - No Polling with Asynch I/O */
#else
#define XQ_SERVICE_INTERVAL  100                        /* polling interval - X per second */
#endif
#define XQ_SYSTEM_ID_SECS    540                        /* seconds before system ID timer expires */
#define XQ_HW_SANITY_SECS    240                        /* seconds before HW sanity timer expires */
#define XQ_MAX_CONTROLLERS     2                        /* maximum controllers allowed */

#define XQ_MAX_RCV_PACKET   1600                        /* Maximum receive packet data */

enum ew_type {EW_T_DE435, EW_T_DE500A, EW_T_DE500B, XQ_T_DEQNA, XQ_T_DELQA, XQ_T_DELQA_PLUS};

struct ew_sanity {
  int       enabled;                                    /* sanity timer enabled? 2=HW, 1=SW, 0=off */
  int       quarter_secs;                               /* sanity timer value in 1/4 seconds */
  int       timer;                                      /* countdown timer */
};

struct ew_setup {
  int               valid;                              /* is the setup block valid? */
  int               promiscuous;                        /* promiscuous mode enabled */
  int               multicast;                          /* enable all multicast addresses */
  int               l1;                                 /* first  diagnostic led state */
  int               l2;                                 /* second diagnostic led state */
  int               l3;                                 /* third  diagnostic led state */
  int               sanity_timer;                       /* sanity timer value (encoded) */
  ETH_MAC           macs[XQ_FILTER_MAX];                /* MAC addresses to respond to */
};

struct ew_turbo_init_block { /* DELQA-T Initialization Block */
  uint16            mode;
#define XQ_IN_MO_PRO 0x8000               /* Promiscuous Mode */
#define XQ_IN_MO_INT 0x0040               /* Internal Loopback Mode */
#define XQ_IN_MO_DRT 0x0020               /* Disable Retry */
#define XQ_IN_MO_DTC 0x0008               /* Disable Transmit CRC */
#define XQ_IN_MO_LOP 0x0004               /* Loopback */
  ETH_MAC           phys;                 /* Physical MAC Address */
  ETH_MULTIHASH     hash_filter;          /* 64bit LANCE Hash Filter for Multicast Address selection */
  uint16            rdra_l;
  uint16            rdra_h;
  uint16            tdra_l;
  uint16            tdra_h;
  uint16            options;
#define XQ_IN_OP_HIT 0x0002               /* Host Inactivity Timer Enable Flag */
#define XQ_IN_OP_INT 0x0001               /* Interrupt Enable Flag*/
  uint16            vector;               /* Interrupt Vector */
  uint16            hit_timeout;          /* Host Inactivity Timer Timeout Value */
  uint8             bootpassword[6];      /* MOP Console Boot Password */
};

/* DELQA-T Mode - Transmit Buffer Descriptor */
struct transmit_buffer_descriptor {
    uint16         tmd0;
#define XQ_TMD0_ERR1  0x4000    /* Error Summary. The OR of TMD1 (LC0, LCA, and RTR) */
#define XQ_TMD0_MOR   0x1000    /* More than one retry on transmit */
#define XQ_TMD0_ONE   0x0800    /* One retry on transmit */
#define XQ_TMD0_DEF   0x0400    /* Deferral during transmit */
    uint16         tmd1;
#define XQ_TMD1_LCO   0x1000    /* Late collision on transmit - packet not transmitted */
#define XQ_TMD1_LCA   0x0800    /* Loss of carrier on transmit - packet not transmitted */
#define XQ_TMD1_RTR   0x0400    /* Retry error on transmit - packet not transmitted */
#define XQ_TMD1_TDR   0x03FF    /* Time Domain Reflectometry value */
    uint16         tmd2;
#define XQ_TMD2_ERR2  0x8000    /* Error Summary. The OR of TMD2 (BBL, CER, and MIS)  */
#define XQ_TMD2_BBL   0x4000    /* Babble error on transmit */
#define XQ_TMD2_CER   0x2000    /* Collision error on transmit */
#define XQ_TMD2_MIS   0x1000    /* Packet lost on receive */
#define XQ_TMD2_EOR   0x0800    /* End Of Receive Ring Reached */
#define XQ_TMD2_RON   0x0020    /* Receiver On */
#define XQ_TMD2_TON   0x0010    /* Transmitter On */
    uint16         tmd3;
#define XQ_TMD3_OWN   0x8000    /* Ownership field. 0 = DELQA-T, 1 = Host Driver */
#define XQ_TMD3_FOT   0x4000    /* First Of Two flag. 1 = first in chained, 0 = no chain or last in chain */
#define XQ_TMD3_BCT   0x0FFF    /* Byte Count */
    uint16         ladr;        /* Low 16bits of Buffer Address */
    uint16         hadr;        /* Most significant bits of the Buffer Address */
    uint16         hostuse1;
    uint16         hostuse2;
};
#define XQ_TURBO_XM_BCNT  12    /* Transmit Buffer Descriptor Count */

struct receive_buffer_descriptor {
    uint16         rmd0;
#define XQ_RMD0_ERR3  0x4000    /* Error Summary. The OR of FRA, CRC, OFL and BUF */
#define XQ_RMD0_FRA   0x2000    /* Framing error on receive */
#define XQ_RMD0_OFL   0x1000    /* Overflow error on receive (Giant packet) */
#define XQ_RMD0_CRC   0x0800    /* CRC error on receive */
#define XQ_RMD0_BUF   0x0400    /* Internal device buffer error. Part of Giant packet lost */
#define XQ_RMD0_STP   0x0200    /* Start of Packet Flag */
#define XQ_RMD0_ENP   0x0100    /* End of Packet Flag */
    uint16         rmd1;
#define XQ_RMD1_MCNT  0x0FFF    /* Message byte count (including CRC) */
    uint16         rmd2;
#define XQ_RMD2_ERR4  0x8000    /* Error Summary. The OR of RMD2 (RBL, CER, and MIS)  */
#define XQ_RMD2_BBL   0x4000    /* Babble error on transmit */
#define XQ_RMD2_CER   0x2000    /* Collision error on transmit */
#define XQ_RMD2_MIS   0x1000    /* Packet lost on receive */
#define XQ_RMD2_EOR   0x0800    /* End Of Receive Ring Reached */
#define XQ_RMD2_RON   0x0020    /* Receiver On */
#define XQ_RMD2_TON   0x0010    /* Transmitter On */
    uint16         rmd3;
#define XQ_RMD3_OWN   0x8000    /* Ownership field. 0 = DELQA-T, 1 = Host Driver */
    uint16         ladr;        /* Low 16bits of Buffer Address */
    uint16         hadr;        /* Most significant bits of the Buffer Address */
    uint16         hostuse1;
    uint16         hostuse2;
};
#define XQ_TURBO_RC_BCNT  32    /* Receive Buffer Descriptor Count */

struct ew_stats {
  int               recv;                               /* received packets */
  int               dropped;                            /* received packets dropped */
  int               xmit;                               /* transmitted packets */
  int               fail;                               /* transmit failed */
  int               runt;                               /* runts */
  int               reset;                              /* reset count */
  int               giant;                              /* oversize packets */
  int               setup;                              /* setup packets */
  int               loop;                               /* loopback packets */
};

struct ew_meb {                                         /* MEB block */
  uint8   type;
  uint8   add_lo;
  uint8   add_mi;
  uint8   add_hi;
  uint8   siz_lo;
  uint8   siz_hi;
};

enum ew_state {STATE_STOPPED, STATE_RUNNING, STATE_SUSPENDED};

#define EW_RDES0_OWN  0x80000000    // Own bit (0=Host owned, 1=Controller owned)
#define EW_RDES0_FF   0x40000000    // Filtering Fail
#define EW_RDES0_FL   0x3FFF0000    // Frame Length (received)
#define EW_RDES0_ES   0x00008000    // Error Summary
#define EW_RDES0_DE   0x00004000    // Descriptor Error
#define EW_RDES0_DT   0x00003000    // Data Type (Frame type: wire, internal loopback, external loopback)
#define EW_RDES0_DT_WIRE 0x00000000 // Data Type (Frame=wire)
#define EW_RDES0_DT_INT  0x00001000 // Data Type (Frame=internal loopback)
#define EW_RDES0_DT_EXT  0x00002000 // Data Type (Frame=external loopback)
#define EW_RDES0_DT_RES  0x00003000 // Data Type (Frame=reserved)
#define EW_RDES0_RF    0x00000800   // Runt Frame
#define EW_RDES0_MF    0x00000400   // Multicast Frame
#define EW_RDES0_FS    0x00000200   // First Descriptor (of frame)
#define EW_RDES0_LS    0x00000100   // Last Descriptor (of frame)
#define EW_RDES0_TL    0x00000080   // Frame Too Long (giant)
#define EW_RDES0_CS    0x00000040   // Collision Seen
#define EW_RDES0_FT    0x00000020   // Frame Type (1=frame>1500 bytes, 0=IEEE 802.3)
#define EW_RDES0_RW    0x00000010   // Receive Watchdog
#define EW_RDES0_RE    0x00000008   // Report on MII Error
#define EW_RDES0_DB    0x00000004   // Dribbling Bit
#define EW_RDES0_CE    0x00000002   // CRC Error
#define EW_RDES0_ZERO  0x00000001   // Zero (always zero for a packet of legal length)

#define EW_RDES1_RER    0x02000000      // Receive End-of-Ring
#define EW_RDES1_RCH    0X01000000      // Second Address Chained (RER has precedence)
#define EW_RDES1_RBS2   0x003FF800      // Buffer 2 Size (0=unused buffer)
#define EW_RDES1_RBS2_V 11              // Buffer 2 shift
#define EW_RDES1_RBS1   0x000007FF      // Buffer 1 Size (0=unused buffer)

#define EW_TDES0_OWN    0x80000000      // Own Bit
#define EW_TDES0_ES     0x00008000      // Error Summary
#define EW_TDES0_TO     0x00004000      // Transmit Jabber Timeout
#define EW_TDES0_LO     0x00000800      // Loss Of Carrier
#define EW_TDES0_NC     0x00000400      // No Carrier
#define EW_TDES0_LC     0x00000200      // Late Collision
#define EW_TDES0_EC     0x00000100      // Excessive Collisions
#define EW_TDES0_HF     0x00000080      // Heartbeat Fail
#define EW_TDES0_CC     0x00000078      // Collision Count
#define EW_TDES0_LF     0x00000004      // Link Fail Report
#define EW_TDES0_UF     0x00000002      // Underflow Error
#define EW_TDES0_DE     0x00000001      // Deferred

#define EW_TDES1_IC     0x80000000      // Interrupt on Completion
#define EW_TDES1_LS     0x40000000      // Last Segment
#define EW_TDES1_FS     0x20000000      // First Segment
#define EW_TDES1_FT1    0x10000000      // Filtering Type 1
#define EW_TDES1_SET    0x08000000      // Setup Packet
#define EW_TDES1_AC     0x04000000      // Add CRC Disable
#define EW_TDES1_TER    0x02000000      // Transmit End of Ring
#define EW_TDES1_TCH    0x01000000      // Transmit Second Address Chained
#define EW_TDES1_DPD    0x00800000      // Disabled Padding
#define EW_TDES1_FT0    0x00400000      // Filtering Type 0
#define EW_TDES1_TBS2   0x003FF800      // Buffer 2 Size
#define EW_TDES1_TBS2_V 11              // Buffer 2 shift
#define EW_TDES1_TBS1   0X000007FF      // Buffer 1 Size

// CFG definitions

#define EW_CFCS_MSA     0x00000002ul    // Memory Space Access (1=enable, 0=disable)
#define EW_CFCS_IOSA    0x00000001ul    // I/O Space access (1=enable, 0=disable)



//  CSR definitions

#define EW_CSR0_EON     0x04000000      // Enable OnNow registers [21143/Rev4 only]
#define EW_CSR0_WIE     0x01000000      // Write and Invalidate Enable
#define EW_CSR0_RLE     0x00800000      // Read Line Enable
#define EW_CSR0_RME     0x00200000      // Read Multiple Enable
#define EW_CSR0_DBO     0x00100000      // Descriptor Byte Ordering [0=Little-Endian, 1=Big-Endian]
#define EW_CSR0_TAP     0x000E0000      // Transmit Auto Polling
#define EW_CSR0_CAL     0x0000C000      // Cache Alignment
#define EW_CSR0_PBL     0x00003F00      // Programmable Burst Length
#define EW_CSR0_BLE     0x00000080      // Big/Little Endian [data buffers]
#define EW_CSR0_DSL     0x0000007C      // Descriptor Skip Length
#define EW_CSR0_DSL_V   2
#define EW_CSR0_BAR     0x00000002      // Bus Arbitration [1=round robin, 0=receive priority]
#define EW_CSR0_SWR     0x00000001      // Software Reset

#define EW_CSR1_TPD     0xFFFFFFFF      // Transmit Poll Demand

#define EW_CSR2_RPD     0xFFFFFFFF      // Receive Poll Demand

#define EW_CSR3_SRL     0xFFFFFFFC      // Start of Receive List

#define EW_CSR4_STL     0xFFFFFFFC      // Start of Transmit List

#define EW_CSR5_LC      0x08000000      // Link Changed
#define EW_CSR5_GPI     0x04000000      // General-purpose Port Interrupt
#define EW_CSR5_EB      0x03800000      // Error Bits
#define EW_CSR5_TS      0x00700000      // Transmission process State
#define EW_CSR5_RS      0x000E0000      // Receive process State
#define EW_CSR5_NIS     0x00010000      // Normal Interrupt Summary
#define EW_CSR5_NIS_SUM 0x00004845      // NIS summarizes these bits
#define EW_CSR5_AIS     0x00008000      // Abnormal Interrupt Summary
#define EW_CSR5_AIS_SUM 0x0C003F3A      // AIS summarizes these bits
#define EW_CSR5_ERI     0x00004000      // Early Receive Interrupt
#define EW_CSR5_FBE     0x00002000      // Fatal Bus Error
#define EW_CSR5_LNF     0x00001000      // Link Fail
#define EW_CSR5_GTE     0x00000800      // General-purpose Timer Expires
#define EW_CSR5_ETI     0x00000400      // Early Transmit Interrupt
#define EW_CSR5_RWT     0x00000200      // Receive Watchdog Timeout
#define EW_CSR5_RPS     0x00000100      // Receive Process Stopped
#define EW_CSR5_RU      0x00000080      // Receive buffer Unavailable
#define EW_CSR5_RI      0x00000040      // Receive Interrupt
#define EW_CSR5_UNF     0x00000020      // Transmit Underflow
#define EW_CSR5_LNPANC  0x00000010      // Link Pass or Autonegotiation Complete
#define EW_CSR5_TJT     0x00000008      // Transmit Jabber Timeout
#define EW_CSR5_TU      0x00000004      // Transmit buffer Unavailable
#define EW_CSR5_TPS     0x00000002      // Transmit Process Stopped
#define EW_CSR5_TI      0x00000001      // Transmit Interrupt

#define EW_CSR6_SC      0x80000000      // Special Capture effect enable
#define EW_CSR6_RA      0x40000000      // Receive All
#define EW_CSR6_IDAMSB  0x04000000      // Ignore Destination Address MSB
#define EW_CSR6_MB1     0x02000000      // Must Be 1
#define EW_CSR6_SCR     0x01000000      // Scrambler Mode
#define EW_CSR6_PCS     0x00800000      // PCS function
#define EW_CSR6_TTM     0x00400000      // Transmit Threshold Mode
#define EW_CSR6_SF      0x00200000      // Store and Forward
#define EW_CSR6_HBD     0x00080000      // Heartbeat Disable
#define EW_CSR6_PS      0x00040000      // Port Select
#define EW_CSR6_CA      0x00020000      // Capture Affect enable
#define EW_CSR6_TR      0x0000C000      // Threshold Control Bits
#define EW_CSR6_ST      0x00002000      // Start/Stop Transmission command
#define EW_CSR6_FC      0x00001000      // Force Collision Mode
#define EW_CSR6_OM      0x00000C00      // Operating Mode
#define EW_CSR6_FD      0x00000200      // Full-Duplex Mode
#define EW_CSR6_PM      0x00000080      // Pass All Multicast
#define EW_CSR6_PR      0x00000040      // Promiscuous Mode
#define EW_CSR6_SB      0x00000020      // Start/Stop Backoff Counter
#define EW_CSR6_IF      0x00000010      // Inverse Filtering
#define EW_CSR6_PB      0x00000008      // Pass Bad Frames
#define EW_CSR6_HO      0x00000004      // Hash-Only Filtering Mode
#define EW_CSR6_SR      0x00000002      // Start/Stop Receive
#define EW_CSR6_HP      0x00000001      // Hash/Perfect Receive Filtering Mode (1=Hash, 0=Perfect)

#define EW_CSR7_LCE     0x08000000      // Link Change Enable
#define EW_CSR7_GPE     0x04000000      // General-purpose Port Enable
#define EW_CSR7_NIE     0x00010000      // Normal Interrupt Summary Enable
#define EW_CSR7_AIE     0x00008000      // Abnormal Interrupt Summary Enable
#define EW_CSR7_ERE     0x00004000      // Early Receive Interrupt Enable
#define EW_CSR7_FBE     0x00002000      // Fatal Bus Error Enable
#define EW_CSR7_LFE     0x00001000      // Link Fail Enable
#define EW_CSR7_GTE     0x00000800      // General-purpose Timer Enable
#define EW_CSR7_ETE     0x00000400      // Early Transmit Interrupt Enable
#define EW_CSR7_RWE     0x00000200      // Receive Watchdog Timeout Enable
#define EW_CSR7_RSE     0x00000100      // Receive Process Stopped Enable
#define EW_CSR7_RUE     0x00000080      // Receive buffer Unavailable Enable
#define EW_CSR7_RIE     0x00000040      // Receive Interrupt Enable
#define EW_CSR7_UNE     0x00000020      // Transmit Underflow Enable
#define EW_CSR7_LNEANE  0x00000010      // Link Pass Enable or Autonegotiation Completed Enable
#define EW_CSR7_TJE     0x00000008      // Transmit Jabber Timeout Enable
#define EW_CSR7_TUE     0x00000004      // Transmit buffer Unavailable Enable
#define EW_CSR7_TSE     0x00000002      // Transmit Process Stopped Enable
#define EW_CSR7_TIE     0x00000001      // Transmit Interrupt Enable

#define EW_CSR8_OCO     0x10000000      // Overflow Counter Overflow
#define EW_CSR8_FOC     0x0FFE0000      // FIFO Overflow Counter
#define EW_CSR8_MFO     0x00010000      // Missed Frame Overflow
#define EW_CSR8_MFC     0x0000FFFF      // Missed Frame Counter

#define EW_CSR9_MDI     0x00080000      // MII Management Data_In
#define EW_CSR9_MII     0x00040000      // MII Management Operation Mode
#define EW_CSR9_MDO     0x00020000      // MII Management Write Data
#define EW_CSR9_MDC     0x00010000      // MII Management Clock
#define EW_CSR9_RD      0x00004000      // ROM Read Operation
#define EW_CSR9_WR      0x00002000      // ROM Write Operation
#define EW_CSR9_BR      0x00001000      // Boot ROM Select
#define EW_CSR9_SR      0x00000800      // Serial ROM Select
#define EW_CSR9_REG     0x00000400      // External Register Select
#define EW_CSR9_DATA    0x000000FF      // Boot ROM or Serial ROM Control

#define EW_CSR10_BRA    0x0003FFFF      // Boot ROM Address

#define EW_CSR11_CSZ    0x80000000      // Cycle Size
#define EW_CSR11_TT     0x78000000      // Transmit Timer
#define EW_CSR11_NTP    0x07000000      // Number of Transmit Packets
#define EW_CSR11_RT     0x00F00000      // Receive Timer
#define EW_CSR11_NRP    0x000E0000      // Number of Receive Packets
#define EW_CSR11_CON    0x00010000      // Continuous Mode
#define EW_CSR11_TV     0x0000FFFF      // Timer Value

#define EW_CSR12_LPC    0xFFFF0000      // Link Partner's Link Code Word
#define EW_CSR12_LPN    0x00008000      // Link Partner Negotiable
#define EW_CSR12_ANS    0x00007000      // Autonegotiation Arbitration State
#define EW_CSR12_TRF    0x00000800      // Transmit Remote Fault
#define EW_CSR12_NSN    0x00000400      // Non-Stable NLPs Detected
#define EW_CSR12_TRA    0x00000200      // 10base-T Receive Port Activity
#define EW_CSR12_ARA    0x00000100      // AUI Receive Port Activity
#define EW_CSR12_APS    0x00000008      // Autopolarity State
#define EW_CSR12_LS10   0x00000004      // 10-Mb/s Link Status
#define EW_CSR12_LS100  0x00000002      // 100-Mb/s Link Status
#define EW_CSR12_MRA    0x00000001      // MII Receive Port Activity

#define EW_CSR13_AUI    0x00000008      // 10Base-T or AUI
#define EW_CSR13_RST    0x00000001      // SIA Reset

#define EW_CSR14_T4     0x00040000      // 100Base-T4
#define EW_CSR14_TXF    0x00020000      // 100Base-TX Full-duplex
#define EW_CSR14_TXH    0x00010000      // 100Base-TX Half-duplex
#define EW_CSR14_TAS    0x00008000      // 10Base-T/AUI Autosensing Enable
#define EW_CSR14_SPP    0x00004000      // Set Polarity Plus
#define EW_CSR14_APE    0x00002000      // Autopolarity Enable
#define EW_CSR14_LTE    0x00001000      // Link Test Enable
#define EW_CSR14_SQE    0x00000800      // Signal Quality (heartbeat) Generate Enable
#define EW_CSR14_CLD    0x00000400      // Collision Detect Enable
#define EW_CSR14_CSQ    0x00000200      // Collision Squelch Enable
#define EW_CSR14_RSQ    0x00000100      // Receive Squelch Enable
#define EW_CSR14_ANE    0x00000080      // Autonegotiation Enable
#define EW_CSR14_TH     0x00000040      // 10Base-T Half-duplex Enable
#define EW_CSR14_CPEN   0x00000030      // Compensation Enable
#define EW_CSR14_LSE    0x00000008      // Link Pulse Send Enable
#define EW_CSR14_DREN   0x00000004      // Driver Enable
#define EW_CSR14_LBK    0x00000002      // Loopback Enable
#define EW_CSR14_ECEN   0x00000001      // Encoder Enable

#define EW_CSR15_RMI    0x40000000      // Receive Match Interrupt
#define EW_CSR15_GI1    0x20000000      // General Port Interrupt 1
#define EW_CSR15_GI0    0x10000000      // General Port Interrupt 0
#define EW_CSR15_CWE    0x08000000      // Control Write Enable
#define EW_CSR15_RME    0x04000000      // Receive Match Enable
#define EW_CSR15_GEI1   0x02000000      // GEP Interrupt Enable on Port 1
#define EW_CSR15_GEI0   0x01000000      // GEP Interrupt Enable on Port 0
#define EW_CSR15_LGS3   0x00800000      // LED/GEP 3 Select
#define EW_CSR15_LGS2   0x00400000      // LED/GEP 2 Select
#define EW_CSR15_LGS1   0x00200000      // LED/GEP 1 Select
#define EW_CSR15_LGS0   0x00100000      // LED/GEP 0 Select
#define EW_CSR15_MD     0x000F0000      // General-Purpose Mode and Data
#define EW_CSR15_HCKR   0x00008000      // Hacker
#define EW_CSR15_RMP    0x00004000      // Received Magic Packet
#define EW_CSR15_LEE    0x00000800      // Link Extend Enable
#define EW_CSR15_RWR    0x00000020      // Receive Watchdog Release
#define EW_CSR15_RWD    0x00000010      // Receive Watchdog Disable
#define EW_CSR15_ABM    0x00000008      // AUI/BNC Mode
#define EW_CSR15_JCK    0x00000004      // Jabber Check
#define EW_CSR15_HUJ    0x00000002      // Host Unjab
#define EW_CSR15_JBD    0x00000001      // Jabber Disable

struct ew_device {
                                                        /*+ initialized values - DO NOT MOVE */
  ETH_PCALLBACK     rcallback;                          /* read callback routine */
  ETH_PCALLBACK     wcallback;                          /* write callback routine */
  ETH_MAC           mac;                                /* Hardware MAC address */
  enum ew_type      type;                               /* controller type */
  enum ew_type      mode;                               /* controller operating mode */
  uint32            poll;                               /* configured poll ethernet times/sec for receive */
  uint32            coalesce_latency;                   /* microseconds to hold-off interrupts when not polling */
  uint32            coalesce_latency_ticks;             /* instructions in coalesce_latency microseconds */
  struct ew_sanity  sanity;                             /* sanity timer information */
  t_bool            lockmode;                           /* DEQNA-Lock mode */
  uint32            throttle_time;                      /* ms burst time window */
  uint32            throttle_burst;                     /* packets passed with throttle_time which trigger throttling */
  uint32            throttle_delay;                     /* ms to delay when throttling.  0 disables throttling */
                                                        /*- initialized values - DO NOT MOVE */

                                                        /* I/O register storage */

  uint16            rbdl[2];                            /* Receive Buffer Descriptor List */
  uint16            xbdl[2];                            /* Transmit Buffer Descriptor List */
  uint16            var;                                /* Vector Address Register */
  uint16            csr;                                /* Control and Status Register */

  uint16            srr;                                /* Status and Response Register - DELQA-T only */
  uint16            srqr;                               /* Synchronous Request Register - DELQA-T only */
  uint32            iba;                                /* Init Block Address Register - DELQA-T only */
  uint16            icr;                                /* Interrupt Request Register - DELQA-T only */
  uint16            pending_interrupt;                  /* Pending Interrupt - DELQA-T only */
  struct ew_turbo_init_block
                    init;
  struct transmit_buffer_descriptor
                    xring[XQ_TURBO_XM_BCNT];            /* Transmit Buffer Ring */
  uint32            tbindx;                             /* Transmit Buffer Ring Index */
  struct receive_buffer_descriptor
                    rring[XQ_TURBO_RC_BCNT];            /* Receive Buffer Ring */
  uint32            rbindx;                             /* Receive Buffer Ring Index */

  uint32            irq;                                /* interrupt request flag */

                                                        /* buffers, etc. */
  struct ew_setup   setup;
  struct ew_stats   stats;
  uint8             mac_checksum[2];
  uint16            rbdl_buf[6];
  uint16            xbdl_buf[6];
  uint32            rbdl_ba;
  uint32            xbdl_ba;
  ETH_DEV*          etherface;
  ETH_PACK          read_buffer;
  ETH_PACK          write_buffer;
  ETH_QUE           ReadQ;
  int32             idtmr;                              /* countdown for ID Timer */
  uint32            must_poll;                          /* receiver must poll instead of counting on asynch polls */

  uint32            cfg_reg[64];                        // current configuration registers (256 bytes)
  uint32            csrs[18];                           // CSRs,            mapped at 0H-79H
  uint32*           csrs_wmask[18];                     // Address of array of CSR write masks
  uint32*           csrs_w1mask[18];                    // Address of array of CSR write-1-to-clear masks
  uint32            cardbus[4];                         // Cardbus change,  mapped at 80H-8FH
  uint32            rom[128];                           // Serial ROM,      mapped at 200H-3FFH

  // receive state
  enum ew_state     rx_state;                           // stopped, running, suspended
  uint32            rx_curr_base;                       // address of current receive descriptor
  uint32            rx_rdes[4];                         // current receive descriptor

  // transmit state
  enum ew_state     tx_state;                           // stopped, running, suspended
  uint32            tx_curr_base;                       // address of current transmit descriptor
  uint32            tx_tdes[4];                         // current transmit descriptor

};

struct ew_controller {
  DEVICE*           dev;                                /* device block */
  UNIT*             unit;                               /* unit block */
  DIB*              dib;                                /* device interface block */
  struct ew_device* var;                                /* controller-specific variables */
  struct pci_device_t*     pci;                         /* pci device, used to find controller variables */
};

typedef struct ew_controller CTLR;

#define EW_CFID_IDX     0
#define EW_CFCS_IDX     1
#define EW_CFRV_IDX     2
#define EW_CFLT_IDX     3
#define EW_CBIO_IDX     4
#define EW_CBMA_IDX     5
#define EW_CCIS_IDX     10
#define EW_CSID_IDX     11
#define EW_CBER_IDX     12
#define EW_CCAP_IDX     13      // Capabilities Pointer [21143v4 only]
#define EW_CFIT_IDX     15
#define EW_CFDD_IDX     16
#define EW_CWUA0_IDX    17
#define EW_CWUA1_IDX    18
#define EW_SOP0_IDX     19
#define EW_SOP1_IDX     20
#define EW_CWUC_IDX     21
#define EW_CCID_IDX     55      // Capability ID [21143v4 only]
#define EW_CPMC_IDX     56      // Power Management Control and Status [21143v4 only]

#define XQ_CSR_RI 0x8000                                /* Receive Interrupt Request     (RI) [RO/W1] */
#define XQ_CSR_PE 0x4000                                /* Parity Error in Host Memory   (PE) [RO] */
#define XQ_CSR_CA 0x2000                                /* Carrier from Receiver Enabled (CA) [RO] */
#define XQ_CSR_OK 0x1000                                /* Ethernet Transceiver Power    (OK) [RO] */
#define XQ_CSR_RR 0x0800                                /* Reserved : Set to Zero        (RR) [RO] */
#define XQ_CSR_SE 0x0400                                /* Sanity Timer Enable           (SE) [RW] */
#define XQ_CSR_EL 0x0200                                /* External Loopback             (EL) [RW] */
#define XQ_CSR_IL 0x0100                                /* Internal Loopback             (IL) [RW] */
#define XQ_CSR_XI 0x0080                                /* Transmit Interrupt Request    (XI) [RO/W1] */
#define XQ_CSR_IE 0x0040                                /* Interrupt Enable              (IE) [RW] */
#define XQ_CSR_RL 0x0020                                /* Receive List Invalid/Empty    (RL) [RO] */
#define XQ_CSR_XL 0x0010                                /* Transmit List Invalid/Empty   (XL) [RO] */
#define XQ_CSR_BD 0x0008                                /* Boot/Diagnostic ROM Load      (BD) [RW] */
#define XQ_CSR_NI 0x0004                                /* NonExistant Memory Timeout   (NXM) [RO] */
#define XQ_CSR_SR 0x0002                                /* Software Reset                (SR) [RW] */
#define XQ_CSR_RE 0x0001                                /* Receiver Enable               (RE) [RW] */

/* special access bitmaps */
#define XQ_CSR_RO   0xF8B4                              /* Read-Only bits */
#define XQ_CSR_RW   0x074B                              /* Read/Write bits */
#define XQ_CSR_W1   0x8080                              /* Write-one-to-clear bits */
#define XQ_CSR_BP   0x0208                              /* Boot PDP diagnostic ROM */
#define XQ_CSR_XIRI 0X8080                              /* Transmit & Receive Interrupts */

#define XQ_VEC_MS 0x8000                                /* Mode Select                   (MO) [RW]  */
#define XQ_VEC_OS 0x4000                                /* Option Switch Setting         (OS) [RO]  */
#define XQ_VEC_RS 0x2000                                /* Request Self-Test             (RS) [RW]  */
#define XQ_VEC_S3 0x1000                                /* Self-Test Status              (S3) [RO]  */
#define XQ_VEC_S2 0x0800                                /* Self-Test Status              (S2) [RO]  */
#define XQ_VEC_S1 0x0400                                /* Self-Test Status              (S1) [RO]  */
#define XQ_VEC_ST 0x1C00                                /* Self-Test (S1 + S2 + S3)           [RO]  */
#define XQ_VEC_IV 0x03FC                                /* Interrupt Vector              (IV) [RW]  */
#define XQ_VEC_RR 0x0002                                /* Reserved                      (RR) [RO]  */
#define XQ_VEC_ID 0x0001                                /* Identity Test Bit             (ID) [RW]  */

/* special access bitmaps */
#define XQ_VEC_RO 0x5C02                                /* Read-Only bits */
#define XQ_VEC_RW 0xA3FD                                /* Read/Write bits */

/* DEQNA - DELQA Normal Mode Buffer Descriptors */
#define XQ_DSC_V  0x8000                                /* Valid bit */
#define XQ_DSC_C  0x4000                                /* Chain bit */
#define XQ_DSC_E  0x2000                                /* End of Message bit       [Transmit only] */
#define XQ_DSC_S  0x1000                                /* Setup bit                [Transmit only] */
#define XQ_DSC_L  0x0080                                /* Low Byte Termination bit [Transmit only] */
#define XQ_DSC_H  0x0040                                /* High Byte Start bit      [Transmit only] */

/* DEQNA - DELQA Receive Status Word 1 */
#define XQ_RST_UNUSED    0x8000                         /* Unused buffer */
#define XQ_RST_LASTNOT   0xC000                         /* Used but Not Last segment */
#define XQ_RST_LASTERR   0x4000                         /* Used, Last segment, with errors */
#define XQ_RST_LASTNOERR 0x0000                         /* Used, Last segment, without errors */
#define XQ_RST_RUNT      0x4800                         /* Runt packet, internal loopback unsuccessful */
#define XQ_RST_ESETUP    0x2000                         /* Setup packet, internal loopback or external loopback packet */
#define XQ_RST_DISCARD   0x1000                         /* Runt packet, internal loopback unsuccessful */
#define XQ_RST_RUNT      0x4800                         /* Runt packet, internal loopback unsuccessful */
#define XQ_RST_FRAMEERR  0x5006                         /* Framing Error in packet */
#define XQ_RST_CRCERR    0x5002                         /* CRC Error in packet */
#define XQ_RST_OVERFLOW  0x0001                         /* Receiver overflowed, packet(s) lost */

/* DEQNA - DELQA Transmit Status Word 1 */
#define XQ_XMT_UNUSED    0x8000                         /* Unused buffer */
#define XQ_XMT_LASTNOT   0xC000                         /* Used but Not Last segment */
#define XQ_XMT_LASTERR   0x4000                         /* Used, Last segment, with errors */
#define XQ_XMT_LASTNOERR 0x0000                         /* Used, Last segment, without errors */
#define XQ_XMT_LOSS      0x5000                         /* Carrier Loss during transmission error */
#define XQ_XMT_NOCARRIER 0x4800                         /* No Carrier during transmission error */
#define XQ_XMT_STE16     0x0400                         /* Sanity timer enabled with timeout of 4 minutes */
#define XQ_XMT_ABORT     0x4200                         /* Transmission aborted due to excessive collisions */
#define XQ_XMT_FAIL      0x0100                         /* Heartbeat collision check failure */

#define XQ_LONG_PACKET   0x0600                         /* DEQNA Long Packet Limit (1536 bytes) */

/* DEQNA - DELQA Normal Mode Setup Packet Flags */
#define XQ_SETUP_MC 0x0001                              /* multicast bit */
#define XQ_SETUP_PM 0x0002                              /* promiscuous bit */
#define XQ_SETUP_LD 0x000C                              /* led bits */
#define XQ_SETUP_ST 0x0070                              /* sanity timer bits */

/* DELQA-T Mode - Status and Response Register (SRR) */
#define XQ_SRR_FES  0x8000                              /* Fatal Error Summary                [RO]  */
#define XQ_SRR_CHN  0x4000                              /* Chaining Error                     [RO]  */
#define XQ_SRR_NXM  0x1000                              /* Non-Existant Memory Error          [RO]  */
#define XQ_SRR_PAR  0x0800                              /* Parity Error (Qbus)                [RO]  */
#define XQ_SRR_IME  0x0400                              /* Internal Memory Error              [RO]  */
#define XQ_SRR_TBL  0x0200                              /* Transmit Buffer Too Long Error     [RO]  */
#define XQ_SRR_RESP 0x0003                              /* Synchronous Response Field         [RO]  */
#define XQ_SRR_TRBO 0x0001                              /* Select Turbo Response              [RO]  */
#define XQ_SRR_STRT 0x0002                              /* Start Device Response              [RO]  */
#define XQ_SRR_STOP 0x0003                              /* Stop Device Response               [RO]  */

/* DELQA-T Mode - Synchronous Request Register (SRQR) */
#define XQ_SRQR_STRT 0x0002                             /* Start Device Request               [WO]  */
#define XQ_SRQR_STOP 0x0003                             /* Stop Device Request                [WO]  */
#define XQ_SRQR_RW   0x0003                             /* Writable Bits in SRQR              [WO]  */

/* DELQA-T Mode - Asynchronous Request Register (ARQR) */
#define XQ_ARQR_TRQ 0x8000                             /* Transmit Request                    [WO]  */
#define XQ_ARQR_RRQ 0x0080                             /* Receieve Request                    [WO]  */
#define XQ_ARQR_SR  0x0002                             /* Software Reset Request              [WO]  */

/* DELQA-T Mode - Interrupt Control Register (ICR) */
#define XQ_ICR_ENA 0x0001                              /* Interrupt Enabled                   [WO]  */



static const uint32 INTEL_21140_CFG_DATA[64] = {
/*00*/  0x00091011, // CFID: vendor + device
/*04*/  0x02800000, // CFCS: command + status
/*08*/  0x02000022, // CFRV: class + revision	// 22 = 21140-AE or -AF (DE500-AA)
/*0C*/  0x00000000, // CFLT: latency timer + cache line size
/*10*/  0x00000001, // BAR0: CBIO
/*14*/  0x00000000, // BAR1: CBMA
/*18*/  0x00000000, // BAR2: RESERVED
/*1C*/  0x00000000, // BAR3: RESERVED
/*20*/  0x00000000, // BAR4: RESERVED
/*24*/  0x00000000, // BAR5: RESERVED
/*28*/  0x00000000, // RESERVED
/*2C*/  0x500a1011, // CSID: subsystem + vendor
/*30*/  0x00000000, // BAR6: expansion rom base
/*34*/  0x00000000, // RESERVED
/*38*/  0x00000000, // RESERVED
/*3C*/  0x281401ff, // CFIT: interrupt configuration
/*40*/  0x00000000, // CFDD: device and driver register
/*44-7C*/   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // not used
/*80-BC*/ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // not used
/*C0-FC*/ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0   // not used
};

static const uint32 INTEL_21140_WMASK[64] = {
/*00*/  0x00000000, // CFID: vendor + device
/*04*/  0x00000147, // CFCS: command + status
/*08*/  0x00000000, // CFRV: class + revision
/*0c*/  0x0000ff00, // CFLT: latency timer
/*10*/  0xffffff80, // BAR0: CBIO
/*14*/  0xffffff80, // BAR1: CBMA
/*18*/  0x00000000, // BAR2: RESERVED
/*1c*/  0x00000000, // BAR3: RESERVED
/*20*/  0x00000000, // BAR4: RESERVED 
/*24*/  0x00000000, // BAR5: RESERVED
/*28*/  0x00000000, // RESERVED
/*2c*/  0x00000000, // RESERVED
/*30*/  0x00000000, // RESERVED
/*34*/  0x00000000, // RESERVED
/*38*/  0x00000000, // RESERVED
/*3c*/  0x0000ffff, // CFIT: interrupt configuration
/*40*/  0xc000ff00, // CFDA: configuration driver area
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const uint32 INTEL_21140_CSR_DATA[32] = {
/*00*/  0xFE000000, // Bus Mode Register CSR0
        0x00000000, // -
/*08*/  0xFFFFFFFF, // Transmit Poll Demand CSR1
        0x00000000, // -
/*10*/  0xFFFFFFFF, // Receive Poll Demand CSR2
        0x00000000, // -
/*18*/  0x00000000, // Receive List Base Address CSR3 [unpredictable]
        0x00000000, // -
/*20*/  0x00000000, // Transmit List Base Address CSR4 [unpredictable]
        0x00000000, // -
/*28*/  0xFC000000, // Status Register CSR5
        0x00000000, // -
/*30*/  0x32000040, // Operation Mode Register CSR6
        0x00000000, // -
/*38*/  0xFFFE0000, // Inerrupt Enable Register CSR7
        0x00000000, // -
/*40*/  0xE0000000, // Missed Frames and Overflow Counter CSR8
        0x00000000, // -
/*48*/  0xFFF483FF, // Boot ROM, Serial ROM, and MII Management Register CSR9
        0x00000000, // -
/*50*/  0x00000000, // Boot ROM Programming Address CSR10 [unpredictable]
        0x00000000, // -
/*58*/  0xFFFE0000, // General-Purpose Timer Register CSR11
        0x00000000, // -
/*60*/  0xFFFFFE00, // General-Purpose Port Register CSR12
        0x00000000, // -
/*68*/  0x00000000, // Reserved CSR13
        0x00000000, // -
/*70*/  0x00000000, // Reserved CSR14
        0x00000000, // -
/*78*/  0xFFFFFEC8, // Watchdog Timer Register CSR15
        0x00000000
};

#endif // __pci_dec21x4x_h