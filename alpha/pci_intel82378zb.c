/* pci_intel82378zb.c: PCI-ISA bridge

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
    Description:

    The Intel 82378ZB(IB) SIO is a PCI-to-ISA bridge chip used in the Miata MK.
*/

/*
	Documentation:

*/

/*
	History:
	--------
    2016-07-13  DTH  Added Configuration power up and write mask values
	2016-07-06  DTH  Started
*/

#include "alpha_defs.h"
#include "alpha_sys_defs.h"
#include "sim_pci.h"
#include "pci_intel82378zb.h"

extern PCI_BUS pyxis_pci64;
t_stat pci2isa_reset (DEVICE * dptr);

UNIT pci2isa_units[] = {
    { NULL },
};

REG pci2isa_reg[] = {
  { NULL },
};

MTAB pci2isa_mod[] = {
  { 0 },
};

DEBTAB pci2isa_debug[] = {
//  {"WARN",   DBG_WRN,   "display warnings"},
  {0}
};

DEVICE pci2isa_dev = {
    "ISA",                      /* name */
    pci2isa_units,              /* units */
    pci2isa_reg,                /* registers */
    pci2isa_mod,                /* modifiers */
    1,                          /* #units */
    16,                         /* address radix */
    64,                         /* address width */
    0,                          /* addr increment */
    16,                         /* data radix */
    32,                         /* data width */
    NULL,                       /* examine routine */
    NULL,                       /* deposit routine */
    &pci2isa_reset,              /* reset routine */
    NULL,                       /* boot routine */
    NULL,                       /* attach routine */
    NULL,                       /* detach routine */
    NULL,                       /* context */
    0,                          /* flags */
    0,                          /* debug control */
    pci2isa_debug,              /* debug flags */
    NULL,                       /* mem size routine */
    NULL,                       /* logical name */
    NULL,                       /* help */
    NULL,                       /* attach help */
    NULL,                       /* Context available to help routines */
    NULL                        /* Device Description */
    };

PCI_CFG pci2isa_cfg_current;

/*
    Speculation:

    The real Miata MK system has an Intel SIO with a revision register (RID) of 0x43; this revision
    is not listed in the SIO/SIO.A manual. It is assumed to be a later revison of the 0x03 ZB(SIO) chip,
    and that the functionality will be the same as the revision 0x03 chip, with the likely
    extension that it is more power friendly like most of the rest of the chips
    implemented during this era. SRM-based Operating Systems probably don't care about the
    power-on/power-save features, but the ARC-based Windows NT might.
*/
const PCI_CFG pci2isa_cfg_powerup = {
/*00*/  0x04848086ul,   // DID/VID
/*04*/  0x02000007ul,   // DS/COM
/*08*/  0x00000043ul,   // --/RID
/*0C*/  0x00000000ul,   // reserved
/*10*/  0x00000000ul,   // reserved
/*14*/  0x00000000ul,   // reserved
/*18*/  0x00000000ul,   // reserved
/*1C*/  0x00000000ul,   // reserved
/*20*/  0x00000000ul,   // reserved
/*24*/  0x00000000ul,   // reserved
/*28*/  0x00000000ul,   // reserved
/*2C*/  0x00000000ul,   // reserved
/*30*/  0x00000000ul,   // reserved
/*34*/  0x00000000ul,   // reserved
/*38*/  0x00000000ul,   // reserved
/*3C*/  0x00000000ul,   // reserved
/*40*/  0x00040020ul,   // ARBPRIX/PAPC/PAC/PCICON
/*44*/  0x000F1000ul,   // MCSTOM/MCSTOH/MCSBOH/MCSCON
/*48*/  0x0F100001ul,   // IADTOH/IADBOH/IADRBE/IADCON
/*4C*/  0x4F074056ul,   // UBCSB/UBCSA/ICD/ICRT
/*50*/  0x00000000ul,   // reserved
/*54*/  0x00000000ul,   // --/MAR3/MAR2/MAR1
/*58*/  0x00000000ul,   // reserved
/*5C*/  0x00000000ul,   // reserved
/*60*/  0x80808080ul,   // PIRQ3/PIRQ2/PIRQ1/PIRQ0
/*64*/  0x00000000ul,   // reserved
/*68*/  0x00000000ul,   // reserved
/*6C*/  0x00000000ul,   // reserved
/*70*/  0x00000000ul,   // reserved
/*74*/  0x00000000ul,   // reserved
/*78*/  0x00000000ul,   // reserved
/*7C*/  0x00000000ul,   // reserved
/*80*/  0x00000078ul,   // --/BIOST
/*84*/  0x00000000ul,   // reserved
/*88*/  0x00000000ul,   // reserved
/*8C*/  0x00000000ul,   // reserved
/*90*/  0x00000000ul,   // reserved
/*94*/  0x00000000ul,   // reserved
/*98*/  0x00000000ul,   // reserved
/*9C*/  0x00000000ul,   // reserved
/*A0*/  0x00000008ul,   // SMIEN/--/SMICNTL
/*A4*/  0x00000000ul,   // SEE
/*A8*/  0x0000000Ful,   // SMIREQ/--/FTMR
/*AC*/  0x00000000ul,   // --/CTLTMRH/--/CTLTMR
/*B0-BF*/   0,0,0,0,    // reserved
/*C0-CF*/   0,0,0,0,    // reserved
/*D0-DF*/   0,0,0,0,    // reserved
/*E0-EF*/   0,0,0,0,    // reserved
/*F0-FF*/   0,0,0,0     // reserved
};

PCI_CFG pci2isa_cfg_wmask = {
/*00*/  0x00000000ul,   // DID/VID
/*04*/  0x37000008ul,   // DS/COM  // DS<MA|RTA> is actually W1C, maybe DS<STA> too
/*08*/  0x00000000ul,   // --/RID
/*0C*/  0x00000000ul,   // reserved
/*10*/  0x00000000ul,   // reserved
/*14*/  0x00000000ul,   // reserved
/*18*/  0x00000000ul,   // reserved
/*1C*/  0x00000000ul,   // reserved
/*20*/  0x00000000ul,   // reserved
/*24*/  0x00000000ul,   // reserved
/*28*/  0x00000000ul,   // reserved
/*2C*/  0x00000000ul,   // reserved
/*30*/  0x00000000ul,   // reserved
/*34*/  0x00000000ul,   // reserved
/*38*/  0x00000000ul,   // reserved
/*3C*/  0x00000000ul,   // reserved
/*40*/  0x01FF1F7Ful,   // ARBPRIX/PAPC/PAC/PCICON
/*44*/  0xFFFFFF0Ful,   // MCSTOM/MCSTOH/MCSBOH/MCSCON
/*48*/  0xFFFFFFFFul,   // IADTOH/IADBOH/IADRBE/IADCON
/*4C*/  0xFFFF7F7Ful,   // UBCSB/UBCSA/ICD/ICRT
/*50*/  0x00000000ul,   // reserved
/*54*/  0x00FFFFFFul,   // --/MAR3/MAR2/MAR1
/*58*/  0x00000000ul,   // reserved
/*5C*/  0x00000000ul,   // reserved
/*60*/  0x8F8F8F9Ful,   // PIRQ3/PIRQ2/PIRQ1/PIRQ0
/*64*/  0x00000000ul,   // reserved
/*68*/  0x00000000ul,   // reserved
/*6C*/  0x00000000ul,   // reserved
/*70*/  0x00000000ul,   // reserved
/*74*/  0x00000000ul,   // reserved
/*78*/  0x00000000ul,   // reserved
/*7C*/  0x00000000ul,   // reserved
/*80*/  0x0000FFFDul,   // --/BIOST
/*84*/  0x00000000ul,   // reserved
/*88*/  0x00000000ul,   // reserved
/*8C*/  0x00000000ul,   // reserved
/*90*/  0x00000000ul,   // reserved
/*94*/  0x00000000ul,   // reserved
/*98*/  0x00000000ul,   // reserved
/*9C*/  0x00000000ul,   // reserved
/*A0*/  0x00FF000Ful,   // SMIEN/reserved/SMICNTL
/*A4*/  0xA000FFFBul,   // SEE
/*A8*/  0x00FF00FFul,   // SMIREQ/--/FTMR
/*AC*/  0x00FF00FFul,   // --/CTLTMRH/--/CTLTMR
/*B0-BF*/   0,0,0,0,    // reserved
/*C0-CF*/   0,0,0,0,    // reserved
/*D0-DF*/   0,0,0,0,    // reserved
/*E0-EF*/   0,0,0,0,    // reserved
/*F0-FF*/   0,0,0,0     // reserved
};


PCI_DEV pci2isa_pci_dev = {
    "ISA_PCI",          // name
    &pci2isa_dev,       // DEV*
    &pyxis_pci64,       // PCI_BUS*
    7,                  // pci slot
    1,                  // functions
    &pci2isa_cfg_current,   // cfg_reg[func]
    &pci2isa_cfg_wmask,     // cfg_wmask[func]
    0,//&ew_pci_reset,      // (*reset)
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

t_stat pci2isa_reset (DEVICE* dp)
{
    // Unconditionally register PCI device with PCI bus. This DEVICE cannot be disabled!
    pci_register (pci2isa_pci_dev.bus, &pci2isa_pci_dev, pci2isa_pci_dev.slot_num);

    // Copy PCI configuration register powerup values to current values
    pci2isa_cfg_current = pci2isa_cfg_powerup;

    return PCI_OK;
}
