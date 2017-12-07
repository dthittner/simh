/* pci_dec21x52.c: PCI-PCI Bridge
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
  Documentation:
    Intel 21152 PCI-to-PCI Bridge Preliminary Datasheet October 1998, 278060-001
  ------------------------------------------------------------------------------

  This simulation of a 21152 bridge implements:
    1) Upstream and downstream PCI forwarding
    2) Type 1 to Type 0 conversion
    3) Type 1 to Special Cycle conversion
    4) ISA mode
    5) VGA mode
    6) VGA Snoop Mode

  This simulation does not currently implement:
    1) Transaction Buffering
    2) Cycle Count data transfer limit
    3) Prefetch read-ahead

  ------------------------------------------------------------------------------

  Modification history:

  2016-06-01  DTH  Added ISA, VGA, VGA Snoop modes; Type 1 conversions
  2016-05-31  DTH  Corrected range inclusion/exclusion logic
  2016-05-27  DTH  Started

  ------------------------------------------------------------------------------
*/

#include "alpha_defs.h"
#include "sim_defs.h"
#include "sim_pci.h"
#include "pci_dec21x52.h"

// forwards
extern PCI_BUS pyxis_pci64;
extern DEVICE ppbr0_dev;
t_stat ppbr0_reset (DEVICE* dev);

PCI_STAT ppbr0_pri_reset      (PCI_DEV* _this);
PCI_STAT ppbr0_pri_io_read    (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32* value);
PCI_STAT ppbr0_pri_io_write   (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32 value);
PCI_STAT ppbr0_pri_mem_read   (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32* value);
PCI_STAT ppbr0_pri_mem_write  (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32 value);
PCI_STAT ppbr0_pri_cfg_write  (PCI_DEV* _this, int slot, int function, int register_, uint8 cbez, uint32 value);
PCI_STAT ppbr0_pri_mem_readm  (PCI_DEV* _this, t_uint64 pci_src_address, uint32* lcl_dst_address, int repeat);
PCI_STAT ppbr0_pri_mem_readl  (PCI_DEV* _this, t_uint64 pci_src_address, uint32* lcl_dst_address, int repeat);
PCI_STAT ppbr0_pri_mem_writei (PCI_DEV* _this, t_uint64 pci_dst_address, uint32* lcl_src_address, int repeat);
PCI_STAT ppbr0_pri_cfg_read1  (PCI_DEV* _this, int bus, int device, int function, int register_, uint8 cbez, uint32* value);
PCI_STAT ppbr0_pri_cfg_write1 (PCI_DEV* _this, int bus, int device, int function, int register_, uint8 cbez, uint32 value);
PCI_STAT ppbr0_pri_mem_read64 (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, t_uint64* value);
PCI_STAT ppbr0_pri_mem_write64(PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, t_uint64 value);

PCI_STAT ppbr0_sec_io_read (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32* value);
PCI_STAT ppbr0_sec_io_write (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32 value);
PCI_STAT ppbr0_sec_cfg_read (PCI_DEV* _this, int slot, int function, int register_, uint8 cbez, uint32* value);
PCI_STAT ppbr0_sec_cfg_write (PCI_DEV* _this, int slot, int function, int register_, uint8 cbez, uint32 value);
PCI_STAT ppbr0_sec_cfg_read1 (PCI_DEV* _this, int bus, int slot, int function, int register_, uint8 cbez, uint32* value);
PCI_STAT ppbr0_sec_cfg_write1 (PCI_DEV* _this, int bus, int slot, int function, int register_, uint8 cbez, uint32 value);

PCI_BUS ppbr0_bus = {
    "PCI 21152 BUS0",
    {0}
};

PCI_CFG ppbr0_cfg_reg;

PCI_DEV ppbr0_pri = {
    "PPBR0_PRI",    // name
    &ppbr0_dev,     // SIMH device
    &pyxis_pci64,   // bus
    20,             // slot_num
    1,              // functions
    &ppbr0_cfg_reg,          // cfg_reg
    NULL,           // cfg_wmask
    NULL,           // (*reset)
    NULL,           // (*int_ack)
    NULL,           // (*special)
    &ppbr0_pri_io_read,     // (*io_read)
    &ppbr0_pri_io_write,    // (*io_write)
    &ppbr0_pri_mem_read,    // (*mem_read)
    &ppbr0_pri_mem_write,   // (*mem_write)
    NULL,                   // (*cfg_read)  // use cfg_read_default
    &ppbr0_pri_cfg_write,   // (*cfg_write)
    &ppbr0_pri_mem_readm,   // (*mem_readm)
    &ppbr0_pri_mem_readl,   // (*mem_readl)
    &ppbr0_pri_mem_writei,  // (*mem_writei)
    &ppbr0_pri_cfg_read1,   // (*cfg_read1)
    &ppbr0_pri_cfg_write1,  // (*cfg_write1)
    &ppbr0_pri_mem_read64,  // (*mem_read64)
    &ppbr0_pri_mem_write64, // (*mem_write64)
    NULL,           // (*mem_readm64)
    NULL,           // (*mem_readl64)
    NULL            // (*mem_writei64)
};

PCI_DEV ppbr0_sec = {
    "PPBR0_SEC",    // name
    &ppbr0_dev,     // SIMH device
    &ppbr0_bus,     // bus
    0,              // slot_num     // slot zero passes traffic upstream back to CPU faster
    1,              // functions
    NULL,           // cfg_reg
    NULL,           // cfg_wmask
    &ppbr0_pri_reset,       // (*reset)
    NULL,           // (*int_ack)
    NULL,           // (*special)
    &ppbr0_sec_io_read,     // (*io_read)
    &ppbr0_sec_io_write,    // (*io_write)
    NULL,           // (*mem_read)
    NULL,           // (*mem_write)
    &ppbr0_sec_cfg_read,    // (*cfg_read)
    &ppbr0_sec_cfg_write,   // (*cfg_write)
    NULL,           // (*mem_readm)
    NULL,           // (*mem_readl)
    NULL,           // (*mem_writei)
    &ppbr0_sec_cfg_read1,   // (*cfg_read1)
    &ppbr0_sec_cfg_write1,  // (*cfg_write1)
    NULL,           // (*mem_read64)
    NULL,           // (*mem_write64)
    NULL,           // (*mem_readm64)
    NULL,           // (*mem_readl64)
    NULL            // (*mem_writei64)
};

PCI_PBR_DEV bridge0 = {
    &pyxis_pci64,   // primary
    &ppbr0_bus,     // secondary
    &ppbr0_pri,     // primary device
    &ppbr0_sec,     // secondary device
    {0}             // don't fill in remainder of fields
};

DEVICE ppbr0_dev = {
  "PCI_BR0",        // *name
  NULL,             // *units
  NULL,             // *registers
  NULL,             // *modifiers
  2,                // numunits
  16,               // aradix
  11,               // awidth
  1,                // aincr
  16,               // dradix
  16,               // dwidth
  NULL,             // (*examine)
  NULL,             // (*deposit)
  &ppbr0_reset,     // (*reset)
  NULL,             // (*boot)
  NULL,             // (*attach)
  NULL,             // (*detach)
  NULL,             // *ctxt (DIB)
  DEV_DEBUG,        // flags
  0,                // dctrl
  NULL,             // *debflags
  NULL,             // (*msize)
  NULL,             // *lname
  NULL,             // (*help)
  NULL,             // (*attach_help)
  NULL,             // *help_ctx 
  NULL              // *(*help_description)
};

t_bool forward_mem_down (PCI_DEV* _this, t_uint64 pci_address)
{
    if ((_this->cfg_reg[0].csr[1] & BR_CFG1_MENA) != 0) {   // Memory forward enabled test
        if (pci_address > PCI_SAC_MAX_ADDRESS) {    // Dual Address Cycle [DAC]
            // DAC uses prefetch memory range
            return ((pci_address >= bridge0.pf_mem_base) && (pci_address <= bridge0.pf_mem_limit));
        } else {                                    // Single Address Cycle [SAC]
            // SAC uses standard memory range
            return ((pci_address >= bridge0.mem_base) && (pci_address <= bridge0.mem_limit));
        }
        if ( ((_this->cfg_reg[0].csr[15] & BR_CFG15_VGAMOD) != 0) &&
              ((pci_address >= 0xA0000) && (pci_address <= 0xBFFFF)) ) {
                  // VGA Mode enabled and matches VGA memory addresses
                  return TRUE;
        }
    }
    return FALSE;
}

t_bool forward_mem_up (PCI_DEV* _this, t_uint64 pci_address)
{
    if ((_this->cfg_reg[0].csr[1] & BR_CFG1_MSTENA) != 0) { // I/O and Memory Master enabled (upstream forwarding)
        if (pci_address > PCI_SAC_MAX_ADDRESS) {    // Dual Address Cycle (DAC)
            return (!((pci_address >= bridge0.pf_mem_base) && (pci_address <= bridge0.pf_mem_limit)));
        } else {
            return (!((pci_address >= bridge0.mem_base) && (pci_address <= bridge0.mem_limit)));
        }
    }
    return FALSE;
}

t_stat ppbr0_reset (DEVICE* dev)
{
    PCI_STAT status;

    // Register bridge devices with primary and secondary PCI buses
    status = pci_register (bridge0.primary,   bridge0.pdev, bridge0.pdev->slot_num);
    status = pci_register (bridge0.secondary, bridge0.sdev, bridge0.sdev->slot_num);

    // Initialize PCI configuration registers to power-up defaults
    memcpy(&ppbr0_cfg_reg, INTEL_21152_CFG_DATA, sizeof(INTEL_21152_CFG_DATA));

    // Initialize state data
    bridge0.io_base = 0;
    bridge0.io_limit = 0;
    bridge0.mem_base = 0;
    bridge0.mem_limit = 0;
    bridge0.pf_mem_base = 0;
    bridge0.pf_mem_limit = 0;
    bridge0.pbus = 0;
    bridge0.sbus = 0;
    bridge0.sbbus = 0;
}

PCI_STAT ppbr0_pri_reset (PCI_DEV* _this)
{
    PCI_STAT status;
    t_stat retval;

    status = pci_bus_reset(bridge0.secondary);  // reset secondary bus
    retval = ppbr0_reset(_this->dev);           // reset device back to power-on
    return PCI_OK;
}

PCI_STAT ppbr0_pri_cfg_write (PCI_DEV* _this, int slot, int function, int register_, uint8 cbez, uint32 value)
{
    PCI_STAT result;

    // why reinvent the wheel? let the default cfg writer do the register bit twaddling
    result = pci_cfg_write_default (_this, slot, function, register_, cbez, value);

    switch (register_) {
    case 6:
        // Update PCI bus numbers
        bridge0.pbus  = ((_this->cfg_reg[0].csr[6] & PCI_CFG_H1R6_PBUS_M)  >> PCI_CFG_H1R6_PBUS_V);
        bridge0.sbus  = ((_this->cfg_reg[0].csr[6] & PCI_CFG_H1R6_SBUS_M)  >> PCI_CFG_H1R6_SBUS_V);
        bridge0.sbbus = ((_this->cfg_reg[0].csr[6] & PCI_CFG_H1R6_SBBUS_M) >> PCI_CFG_H1R6_SBBUS_V);
        bridge0.primary->bus_num = bridge0.pbus;
        bridge0.secondary->bus_num = bridge0.sbus;
        break;

    case 7:
    case 12:
        // Update I/O base and limit
        bridge0.io_base  = (((_this->cfg_reg[0].csr[7]  & PCI_CFG_H1R7_IOB_M)  >> PCI_CFG_H1R7_IOB_V)
                         |  ((_this->cfg_reg[0].csr[12] & PCI_CFG_H1R12_IOB_M) >> PCI_CFG_H1R12_IOB_V)) << 12;

        bridge0.io_limit = ((((_this->cfg_reg[0].csr[7]  & PCI_CFG_H1R7_IOL_M)  >> PCI_CFG_H1R7_IOL_V)
                         |   ((_this->cfg_reg[0].csr[12] & PCI_CFG_H1R12_IOL_M) >> PCI_CFG_H1R12_IOL_V)) << 12)
                         | 0xFFF;
        break;

    case 8:
        // Update (32-bit) Memory base and limit
        bridge0.mem_base  = ((_this->cfg_reg[0].csr[8] & PCI_CFG_H1R8_MEB_M) >> PCI_CFG_H1R8_MEB_V) << 20;

        bridge0.mem_limit = (((_this->cfg_reg[0].csr[8] & PCI_CFG_H1R8_MEL_M) >> PCI_CFG_H1R8_MEL_V) << 20)
                          | 0xFFFFF;
        break;

    case 9:
    case 10:
    case 11:
        // Update Prefetch (64-bit) Memory base and limit
        bridge0.pf_mem_base = ((_this->cfg_reg[0].csr[9] & PCI_CFG_H1R9_PMB_M) >> PCI_CFG_H1R9_PMB_V) << 20
                            | (_this->cfg_reg[0].csr[10] << 31);

        bridge0.pf_mem_limit = ((_this->cfg_reg[0].csr[9] & PCI_CFG_H1R9_PML_M) >> PCI_CFG_H1R9_PML_V) << 20
                             | (_this->cfg_reg[0].csr[11] << 31)
                             | 0xFFFFF;
        break;
    }
    return PCI_OK;
}

//====================================================================
//              Primary PCI Device Callbacks
//====================================================================

PCI_STAT ppbr0_pri_io_read (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32* value)
{
    if ((_this->cfg_reg[0].csr[1] & BR_CFG1_IOENA) == 0) {  // I/O forward enabled test
        // I/O forwarding (downstream) disabled, bail out
        return PCI_NOT_ME;
    } else if ((pci_address >= bridge0.io_base) && (pci_address <= bridge0.io_limit)) { // I/O Range test
        // I/O range matched, is ISA mode range modifier enabled?
        if ( ((_this->cfg_reg[0].csr[15] & BR_CFG15_ISAENA) != 0) && ((pci_address & 0xFFFF0000) == 0) ) {
            // ISA mode enabled for 1st 64KB (<31:16>=0) - modified I/O forwards
            if ((pci_address & 0x300) == 0) {
                // Lower 256 bytes of each 1K block should be forwarded down in ISA mode
                return pci_bus_io_read (bridge0.secondary, pci_address, size, cbez, value);
            } else {
                // Upper 768 bytes of each 1K block should NOT be forwarded down in ISA mode
                return PCI_NOT_ME;
            }
        } else {
            // ISA mode disabled or address not in 1st 64KB - forward I/O range match
            return pci_bus_io_read (bridge0.secondary, pci_address, size, cbez, value);
        }
    } else if ((_this->cfg_reg[0].csr[15] & BR_CFG15_VGAMOD) != 0) {    // VGA mode test
        // VGA Mode enabled, forward any matching VGA I/O addresses
        uint32 pci_target = pci_address & 0xFFFF03FF; // <31:16>=0,<15:10>ignored,<9:0>used
        if ( ((pci_target >= 0x3B0) && (pci_target <= 0x3BB)) ||
             ((pci_target >= 0x3C0) && (pci_target <= 0x3DF)) ) {
            return pci_bus_io_read (bridge0.secondary, pci_address, size, cbez, value);
        }
    }
    return PCI_NOT_ME;
}

PCI_STAT ppbr0_pri_io_write (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32 value)
{
    if ((_this->cfg_reg[0].csr[1] & BR_CFG1_IOENA) == 0) {  // I/O forward enabled test
        // I/O forwarding (downstream) disabled, bail out
        return PCI_NOT_ME;
    } else if ((pci_address >= bridge0.io_base) && (pci_address <= bridge0.io_limit)) { // I/O Range test
        // I/O range matched, is ISA mode range modifier enabled?
        if ( ((_this->cfg_reg[0].csr[15] & BR_CFG15_ISAENA) != 0) && ((pci_address & 0xFFFF0000) == 0) ) {
            // ISA mode enabled for 1st 64KB (<31:16>=0) - modified I/O forwards
            if ((pci_address & 0x300) == 0) {
                // Lower 256 bytes of each 1K block should be forwarded down in ISA mode
                return pci_bus_io_write (bridge0.secondary, pci_address, size, cbez, value);
            } else {
                // Upper 768 bytes of each 1K block should NOT be forwarded down in ISA mode
                return PCI_NOT_ME;
            }
        } else {
            // ISA mode disabled or address not in 1st 64KB - forward I/O range match
            return pci_bus_io_write (bridge0.secondary, pci_address, size, cbez, value);
        }
    } else if ((_this->cfg_reg[0].csr[15] & BR_CFG15_VGAMOD) != 0) {    // VGA mode test
        // VGA Mode enabled, forward any matching VGA I/O addresses
        uint32 pci_target = pci_address & 0xFFFF03FF; // <31:16>=0,<15:10>ignored,<9:0>used
        if ( ((pci_target >= 0x3B0) && (pci_target <= 0x3BB)) ||
             ((pci_target >= 0x3C0) && (pci_target <= 0x3DF)) ) {
            return pci_bus_io_write (bridge0.secondary, pci_address, size, cbez, value);
        }
    } else if ((_this->cfg_reg[0].csr[1] & BR_CFG1_VGASNP) != 0) {      // VGA Snoop Mode test
        // VGA Snoop Mode enabled, forward any matching VGA Snoop I/O addresses
        uint32 pci_target = pci_address & 0xFFFF03FF; // <31:16>=0,<15:10>ignored,<9:0>used
        if ( (pci_target == 0x3C6) || (pci_target == 0x3C8) ||(pci_target == 0x3C9) ) {
            return pci_bus_io_write (bridge0.secondary, pci_address, size, cbez, value);
        }
    }

    return PCI_NOT_ME;
}

PCI_STAT ppbr0_pri_mem_read (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32* value)
{
    if (forward_mem_down(_this, pci_address)) {
        return pci_bus_mem_read (bridge0.secondary, pci_address, size, cbez, value);
    } else {
        return PCI_NOT_ME;
    }
}

PCI_STAT ppbr0_pri_mem_write (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32 value)
{
    if (forward_mem_down(_this, pci_address)) {
        return pci_bus_mem_write (bridge0.secondary, pci_address, size, cbez, value);
    } else {
        return PCI_NOT_ME;
    }
}

PCI_STAT ppbr0_pri_mem_readm (PCI_DEV* _this, t_uint64 pci_src_address, uint32* lcl_dst_address, int repeat)
{
    if (forward_mem_down(_this, pci_src_address)) {
        return pci_bus_mem_readm (bridge0.secondary, pci_src_address, lcl_dst_address, repeat);
    } else {
        return PCI_NOT_ME;
    }
}

PCI_STAT ppbr0_pri_mem_readl (PCI_DEV* _this, t_uint64 pci_src_address, uint32* lcl_dst_address, int repeat)
{
    if (forward_mem_down(_this, pci_src_address)) {
        return pci_bus_mem_readl (bridge0.secondary, pci_src_address, lcl_dst_address, repeat);
    } else {
        return PCI_NOT_ME;
    }
}

PCI_STAT ppbr0_pri_mem_writei (PCI_DEV* _this, t_uint64 pci_dst_address, uint32* lcl_src_address, int repeat)
{
    if (forward_mem_down(_this, pci_dst_address)) {
        return pci_bus_mem_writei (bridge0.secondary, pci_dst_address, lcl_src_address, repeat);
    } else {
        return PCI_NOT_ME;
    }
}

PCI_STAT ppbr0_pri_cfg_read1 (PCI_DEV* _this, int bus, int device, int function, int register_, uint8 cbez, uint32* value)
{
    if (bus == bridge0.sbus) {
        // Bus matches secondary bus number, convert Type 1 to Type 0 on secondary bus
        return pci_bus_cfg_read(bridge0.secondary, device, function, register_, cbez, value);
    } else if ((bus > bridge0.sbus) && (bus <= bridge0.sbbus)) {
        // Bus matches a subordinate bus below secondary bus, pass on Type 1 to secondary bus
        return pci_bus_cfg_read1(bridge0.secondary, bus, device, function, register_, cbez, value);
    } else {
        return PCI_NOT_ME;
    }
}

PCI_STAT ppbr0_pri_cfg_write1 (PCI_DEV* _this, int bus, int device, int function, int register_, uint8 cbez, uint32 value)
{
    if (bus == bridge0.sbus) {
        // Bus matches secondary bus number, convert to Type 0 or Special Cycle on secondary bus
        if ((device == 0x1f) && (function == 0x7) && (register_ == 0)) {
            // Convert to Special Cycle on secondary bus
            PCI_STAT status = pci_bus_special (bridge0.secondary, value);
            // Special cycles always return PCI_NOT_ME, we want to claim/complete this transaction
            return PCI_OK;  
        } else {
            // Convert to Type 0 on secondary bus
            return pci_bus_cfg_write(bridge0.secondary, device, function, register_, cbez, value);
        }
    } else if ((bus > bridge0.sbus) && (bus <= bridge0.sbbus)) {
        // Bus matches a subordinate bus below secondary bus, pass on Type 1 to secondary bus
        return pci_bus_cfg_write1(bridge0.secondary, bus, device, function, register_, cbez, value);
    } else {
        return PCI_NOT_ME;
    }
}

PCI_STAT ppbr0_pri_mem_read64 (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, t_uint64* value)
{
    if (forward_mem_down(_this, pci_address)) {
        return pci_bus_mem_read64 (bridge0.secondary, pci_address, size, cbez, value);
    } else {
        return PCI_NOT_ME;
    }
}

PCI_STAT ppbr0_pri_mem_write64 (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, t_uint64 value)
{
    if (forward_mem_down(_this, pci_address)) {
        return pci_bus_mem_write64 (bridge0.secondary, pci_address, size, cbez, value);
    } else {
        return PCI_NOT_ME;
    }
}

//====================================================================
//              Secondary PCI Device Callbacks
//====================================================================


PCI_STAT ppbr0_sec_cfg_read (PCI_DEV* _this, int slot, int function, int register_, uint8 cbez, uint32* value)
{
    // Bridge does not respond to Type 0 configuration reads on secondary bus!
    return PCI_NOT_ME;
}

PCI_STAT ppbr0_sec_cfg_write (PCI_DEV* _this, int slot, int function, int register_, uint8 cbez, uint32 value)
{
    // Bridge does not respond to Type 0 configuration writes on secondary bus!
    return PCI_NOT_ME;
}

PCI_STAT ppbr0_sec_io_read (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32* value)
{
    if ((_this->cfg_reg[0].csr[1] & BR_CFG1_MSTENA) == 0) {  // I/O Master enabled test (upstream forward)
        // I/O forwarding (upstream) disabled, bail out
        return PCI_NOT_ME;
    } else if ((pci_address >= bridge0.io_base) && (pci_address <= bridge0.io_limit)) { // I/O Range test
        // I/O range matched, is ISA mode range modifier enabled?
        if ( ((_this->cfg_reg[0].csr[15] & BR_CFG15_ISAENA) != 0) && ((pci_address & 0xFFFF0000) == 0) ) {
            // ISA mode enabled for 1st 64KB (<31:16>=0) - modified I/O forwards
            if ((pci_address & 0x300) == 0) {
                // Lower 256 bytes of each 1K block should NOT be forwarded up in ISA mode
                return PCI_NOT_ME;
            } else {
                // Upper 768 bytes of each 1K block should be forwarded up in ISA mode
                return pci_bus_io_read (bridge0.primary, pci_address, size, cbez, value);
            }
        } else {
            // ISA mode disabled or address not in 1st 64KB - do not forward I/O range match
            return PCI_NOT_ME;
        }
    } else {
        // Outside of I/O range, forward upstream
        return pci_bus_io_read (bridge0.primary, pci_address, size, cbez, value);
    }
    return PCI_NOT_ME;
}

PCI_STAT ppbr0_sec_io_write (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32 value)
{
    if ((_this->cfg_reg[0].csr[1] & BR_CFG1_MSTENA) == 0) {  // I/O Master enabled test (upstream forward)
        // I/O forwarding (upstream) disabled, bail out
        return PCI_NOT_ME;
    } else if ((pci_address >= bridge0.io_base) && (pci_address <= bridge0.io_limit)) { // I/O Range test
        // I/O range matched, is ISA mode range modifier enabled?
        if ( ((_this->cfg_reg[0].csr[15] & BR_CFG15_ISAENA) != 0) && ((pci_address & 0xFFFF0000) == 0) ) {
            // ISA mode enabled for 1st 64KB (<31:16>=0) - modified I/O forwards
            if ((pci_address & 0x300) == 0) {
                // Lower 256 bytes of each 1K block should NOT be forwarded up in ISA mode
                return PCI_NOT_ME;
            } else {
                // Upper 768 bytes of each 1K block should be forwarded up in ISA mode
                return pci_bus_io_write (bridge0.primary, pci_address, size, cbez, value);
            }
        } else {
            // ISA mode disabled or address not in 1st 64KB - do not forward I/O range match
            return PCI_NOT_ME;
        }
    } else {
        // Outside of I/O range, forward upstream
        return pci_bus_io_write (bridge0.primary, pci_address, size, cbez, value);
    }
    return PCI_NOT_ME;
}

PCI_STAT ppbr0_sec_cfg_read1 (PCI_DEV* _this, int bus, int device, int function, int register_, uint8 cbez, uint32* value)
{
    if ((bus < bridge0.sbus) || (bus > bridge0.sbbus)) {
        // Bus does not match consecutive subordinate bus range, so forward upstream.
        // Convert to Type 0 or Special Cycle if bus matches primary bus number
        if (bus == bridge0.pbus) {
            return pci_bus_cfg_read(bridge0.primary, device, function, register_, cbez, value);
        } else {
            // Doesn't match primary bus number, forward Type 1 transaction upstream
            return pci_bus_cfg_read1(bridge0.primary, bus, device, function, register_, cbez, value);
        }
    } else {
        return PCI_NOT_ME;
    }
}

PCI_STAT ppbr0_sec_cfg_write1 (PCI_DEV* _this, int bus, int device, int function, int register_, uint8 cbez, uint32 value)
{
    if ((bus < bridge0.sbus) || (bus > bridge0.sbbus)) {
        // Bus does not match consecutive subordinate bus range, so forward upstream.
        // Convert to Type 0 or Special Cycle if bus matches primary bus number
        if (bus == bridge0.pbus) {
            if ((device == 0x1F) && (function == 0x7) && (register_ == 0)) {
                // Convert to Special Cycle
                PCI_STAT status = pci_bus_special (bridge0.primary, value);
                // Special cycles always return PCI_NOT_ME, we want to claim/complete this transaction
                return PCI_OK;
            } else {
                // Convert to Type 0
                return pci_bus_cfg_write(bridge0.primary, device, function, register_, cbez, value);
            }
        } else {
            // Doesn't match primary bus number, forward Type 1 transaction upstream
            return pci_bus_cfg_write1(bridge0.primary, bus, device, function, register_, cbez, value);
        }
    } else {
        return PCI_NOT_ME;
    }
}
