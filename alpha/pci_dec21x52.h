/* pci_dec21x52.h: PCI-PCI Bridge

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
	This PCI Bridge is based on the following chips:
       Digital 21052 and Intel 21152

    Digital Semiconductor sold the intellectual property rights to the 21x52 chip
    line to Intel.

	Documentation:
*/

/*
	History:
	--------
	2016-05-27  DTH  Started
*/

#ifndef __pci_dec21x52_h
#define __pci_dec21x52_h

#include "sim_defs.h"
#include "sim_ether.h"
#include "sim_pci.h"

struct pci_pci_device_t
{
    PCI_BUS*    primary;    // primary bus - NULL if top bus (hose)
    PCI_BUS*    secondary;  // secondary (lower) bus

    PCI_DEV*    pdev;       // device behavior on primary bus
    PCI_DEV*    sdev;       // device behavior for secondary bus

    t_uint64    io_base;
    t_uint64    io_limit;
    t_uint64    mem_base;
    t_uint64    mem_limit;
    t_uint64    pf_mem_base;
    t_uint64    pf_mem_limit;
    int         pbus;
    int         sbus;
    int         sbbus;
};
typedef struct pci_pci_device_t PCI_PBR_DEV;

#define VGA_ADDR_MASK   0xFFFF03FFul

#define BR_CFG1_IOENA   0x00000001  // I/O space enable (primary->secondary forwarding)
#define BR_CFG1_MENA    0x00000002  // Memory space enable (primary->secondary forwarding)
#define BR_CFG1_MSTENA  0x00000004  // Master Enable (secondary->primary forwarding)
#define BR_CFG1_SCENA   0x00000008  // Special Cycle enable
#define BR_CFG1_MWIENA  0x00000010  // Memory Write and Invalidate enable
#define BR_CFG1_VGASNP  0x00000020  // VGA Snoop enable
#define BR_CFG1_PARENA  0x00000040  // Parity Error response enable
#define BR_CFG1_WCYCLE  0x00000080  // Wait Cycle control
#define BR_CFG1_SERREN  0x00000100  // SERR# enable
#define BR_CFG1_FB2B    0x00000200  // Fast back-to-back transaction enable

#define BR_CFG15_PERR   0x00010000  // Parity error response
#define BR_CFG15_SERR   0x00020000  // SERR# enable
#define BR_CFG15_ISAENA 0x00040000  // ISA enable
#define BR_CFG15_VGAMOD 0x00080000  // VGA Mode enable
#define BR_CFG15_MSTABO 0x00200000  // Master Abort Mode
#define BR_CFG15_SBRST  0x00400000  // Secondary Bus Reset
#define BR_CFG15_SFB2B  0x00800000  // Secondary Fast back-to-back enable
#define BR_CFG15_PTMO   0x01000000  // Primary master timeout
#define BR_CFG15_STMO   0x02000000  // Secondary master timeout
#define BR_CFG15_MTMOS  0x04000000  // Master (primary or secondary) timeout status
#define BR_CFG15_MTSERR 0x08000000  // Master timeout SERR# enable

static const uint32 INTEL_21152_CFG_DATA[64] = {
/*00*/  0x00241011, // CFID: vendor + device
/*04*/  0x02800000, // CFCS: command + status
/*08*/  0x06040000, // CFRV: class + revision
/*0C*/  0x00010000, // CFLT: latency timer + cache line size + header type
/*10*/  0x00000000, // BAR0: RESERVED
/*14*/  0x00000000, // BAR1: RESERVED
/*18*/  0x00000000, // Bus numbers and secondary latency timer
/*1C*/  0x02800101, // I/O base/limit and secondary status
/*20*/  0x00000000, // Memory base/limit low <31:20>
/*24*/  0x00010001, // Prefetch memory base/limit addr<31:20>
/*28*/  0x00000000, // Prefetch memory base addr<63:32>
/*2C*/  0x00000000, // Prefetch memory limit addr<63:32>
/*30*/  0x00000000, // I/O base/limit <31:16>

/*34*/  0x00000000, // RESERVED
/*38*/  0x00000000, // RESERVED
/*3C*/  0x281401ff, // CFIT: interrupt configuration
/*40*/  0x00000000, // CFDD: device and driver register
/*44-7C*/   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // not used
/*80-BC*/ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // not used
/*C0-FC*/ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0   // not used
};

static const uint32 INTEL_21152_WMASK[64] = {
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

#endif // __pci_dec21x52_h