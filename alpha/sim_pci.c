/* sim_pci.c: PCI bus simulator

   Copyright (c) 2015, David T. Hittner

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
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of the author shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author.

*/

/*
	This implements the PCI Local Bus that is used for device I/O in latter-day systems.

	Documentation:
		PCI SIG, "PCI Local Bus Specificaton Revision 3.0", February 3, 2004

	Features:
		64-bit PCI Address and Data implementation, without using DAC (Dual-Access Cycles)
	
	Features not implemented:
		Parity failure
*/

/*
    Data Transfer Behavior
    ----------------------
    PCI bus accesses are centered around a 32-bit or 64-bit data transfer.

    On a 32-bit bus, you can transfer 1..4 bytes at a time. The meaningful
    bytes transferred is controlled with the CBE#[3..0] signals, which is implemented
    as the low nibble of the CBEZ variable - CBE stands for Command/ByteEnable, and the Z
    replaces the # sign, indicating that the meaningful bits are negated (zeros).
    So a CBEZ value of 1110b means that only the least significant byte lane is meaningful.
    On a 64-bit bus, CBE#[7..4] enables the high bytes as the high nibble of CBEZ. 

    The PCI Address passed is little-endian, per the PCI specification. This means
    that it contains the address of the least significant byte of the transfer, which
    will be aligned in byte lane 0. CBEZ_MASK[CBEZ] can be applied to mask out
    bytes which should not need to be transferred.

    Alpha software tends to big-or-little-endian align the 1..3 byte PCI data transfers
    by shifting the PCI address and the CBE# signals, so as to avoid having to shift the
    transferred data after it has been moved to/from registers or memory.

    Configuration register read/writes are longword (DWORD) aligned since only the register
    index is passed - address bits <1:0> are forcibly set to 00 or 01 to indicate
    a Type 0 or Type 1 configuration access respectively.

    I/O addresses are designed for compatibility with unaligned ISA addresses, and have
    no particular restrictions, but are designed for single (32-bit) or double (64-bit) transfers.

    Memory Addresses are designed for high performance DMA transfers and SHOULD be longword
    (DWORD) aligned, although the PCI specification does not require it for a single (32-bit)
    or a double (64-bit) transfer. When using the Memory Read Multiple, Memory Read Line, and the 
    Memory Write and Invalidate commands, the expectation is that only the first 32-bit
    transfer can be unaligned - after that, the rest of the transfers are aligned.
*/

/*
	History:
	--------
    2016-07-13  DTH  Enabled CBEZ masking for default configuration read/write routines
	2015-07-26  DTH  Started
*/

#include "sim_defs.h"
#include "sim_pci.h"

const char* CBEZ_LANES[256] = {
/*00-03*/ "Q76543210", "invalid", "invalid", "invalid",
/*04-07*/ "invalid", "invalid", "invalid", "invalid",
/*08-0B*/ "invalid", "invalid", "invalid", "invalid",
/*0C-0F*/ "invalid", "invalid", "invalid", "invalid",
/*10-13*/ "invalid", "invalid", "invalid", "invalid",
/*14-17*/ "invalid", "invalid", "invalid", "invalid",
/*18-1B*/ "invalid", "invalid", "invalid", "invalid",
/*1C-1F*/ "invalid", "invalid", "invalid", "invalid",
/*20-23*/ "invalid", "invalid", "invalid", "invalid",
/*24-27*/ "invalid", "invalid", "invalid", "invalid",
/*28-2B*/ "invalid", "invalid", "invalid", "invalid",
/*2C-2F*/ "invalid", "invalid", "invalid", "invalid",
/*30-33*/ "invalid", "invalid", "invalid", "invalid",
/*34-37*/ "invalid", "invalid", "invalid", "invalid",
/*38-3B*/ "invalid", "invalid", "invalid", "invalid",
/*3C-3F*/ "invalid", "invalid", "invalid", "invalid",
/*40-43*/ "invalid", "invalid", "invalid", "invalid",
/*44-47*/ "invalid", "invalid", "invalid", "invalid",
/*48-4B*/ "invalid", "invalid", "invalid", "invalid",
/*4C-4F*/ "invalid", "invalid", "invalid", "invalid",
/*50-53*/ "invalid", "invalid", "invalid", "invalid",
/*54-57*/ "invalid", "invalid", "invalid", "invalid",
/*58-5B*/ "invalid", "invalid", "invalid", "invalid",
/*5C-5F*/ "invalid", "invalid", "invalid", "invalid",
/*60-63*/ "invalid", "invalid", "invalid", "invalid",
/*64-67*/ "invalid", "invalid", "invalid", "invalid",
/*68-6B*/ "invalid", "invalid", "invalid", "invalid",
/*6C-6F*/ "invalid", "invalid", "invalid", "invalid",
/*70-73*/ "invalid", "invalid", "invalid", "invalid",
/*74-77*/ "invalid", "invalid", "invalid", "invalid",
/*78-7B*/ "invalid", "invalid", "invalid", "invalid",
/*7C-7F*/ "invalid", "invalid", "invalid", "invalid",
/*80-83*/ "invalid", "invalid", "invalid", "invalid",
/*84-87*/ "invalid", "invalid", "invalid", "invalid",
/*88-8B*/ "invalid", "invalid", "invalid", "invalid",
/*8C-8F*/ "invalid", "invalid", "invalid", "invalid",
/*90-93*/ "invalid", "invalid", "invalid", "invalid",
/*94-97*/ "invalid", "invalid", "invalid", "invalid",
/*98-9B*/ "invalid", "invalid", "invalid", "invalid",
/*9C-9F*/ "invalid", "invalid", "invalid", "invalid",
/*A0-A3*/ "invalid", "invalid", "invalid", "invalid",
/*A4-A7*/ "invalid", "invalid", "invalid", "invalid",
/*A8-AB*/ "invalid", "invalid", "invalid", "invalid",
/*AC-AF*/ "invalid", "invalid", "invalid", "invalid",
/*B0-B3*/ "invalid", "invalid", "invalid", "invalid",
/*B4-B7*/ "invalid", "invalid", "invalid", "invalid",
/*B8-BB*/ "invalid", "invalid", "invalid", "invalid",
/*BC-BF*/ "invalid", "invalid", "invalid", "invalid",
/*C0-C3*/ "invalid", "invalid", "invalid", "invalid",
/*C4-C7*/ "invalid", "invalid", "invalid", "invalid",
/*C8-CB*/ "invalid", "invalid", "invalid", "invalid",
/*CC-CF*/ "invalid", "invalid", "invalid", "invalid",
/*D0-D3*/ "invalid", "invalid", "invalid", "invalid",
/*D4-D7*/ "invalid", "invalid", "invalid", "invalid",
/*D8-DB*/ "invalid", "invalid", "invalid", "invalid",
/*DC-DF*/ "invalid", "invalid", "invalid", "invalid",
/*E0-E3*/ "invalid", "invalid", "invalid", "invalid",
/*E4-E7*/ "invalid", "invalid", "invalid", "invalid",
/*E8-EB*/ "invalid", "invalid", "invalid", "invalid",
/*EC-EF*/ "invalid", "invalid", "invalid", "invalid",
/*F0-F3*/ "L3210", "T321", "invalid", "invalid",
/*F4-F7*/ "invalid", "invalid", "invalid", "B3",
/*F8-FB*/ "T210", "W21", "invalid", "B2",
/*FC-FF*/ "W10", "B1", "B0", "invalid"
};

const t_uint64 CBEZ_MASK[256] = {
/*00-03*/ 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFF00ull, 0xFFFFFFFFFFFF00FFull, 0xFFFFFFFFFFFF0000ull,
/*04-07*/ 0xFFFFFFFFFF00FFFFull, 0xFFFFFFFFFF00FF00ull, 0xFFFFFFFFFF0000FFull, 0xFFFFFFFFFF000000ull,
/*08-0B*/ 0xFFFFFFFF00FFFFFFull, 0xFFFFFFFF00FFFF00ull, 0xFFFFFFFF00FF00FFull, 0xFFFFFFFF00FF0000ull,
/*0C-0F*/ 0xFFFFFFFF0000FFFFull, 0xFFFFFFFF0000FF00ull, 0xFFFFFFFF000000FFull, 0xFFFFFFFF00000000ull,
/*10-13*/ 0xFFFFFF00FFFFFFFFull, 0xFFFFFF00FFFFFF00ull, 0xFFFFFF00FFFF00FFull, 0xFFFFFF00FFFF0000ull,
/*14-17*/ 0xFFFFFF00FF00FFFFull, 0xFFFFFF00FF00FF00ull, 0xFFFFFF00FF0000FFull, 0xFFFFFF00FF000000ull,
/*18-1B*/ 0xFFFFFF0000FFFFFFull, 0xFFFFFF0000FFFF00ull, 0xFFFFFF0000FF00FFull, 0xFFFFFF0000FF0000ull,
/*1C-1F*/ 0xFFFFFF000000FFFFull, 0xFFFFFF000000FF00ull, 0xFFFFFF00000000FFull, 0xFFFFFF0000000000ull,
/*20-23*/ 0xFFFF00FFFFFFFFFFull, 0xFFFF00FFFFFFFF00ull, 0xFFFF00FFFFFF00FFull, 0xFFFF00FFFFFF0000ull,
/*24-27*/ 0xFFFF00FFFF00FFFFull, 0xFFFF00FFFF00FF00ull, 0xFFFF00FFFF0000FFull, 0xFFFF00FFFF000000ull,
/*28-2B*/ 0xFFFF00FF00FFFFFFull, 0xFFFF00FF00FFFF00ull, 0xFFFF00FF00FF00FFull, 0xFFFF00FF00FF0000ull,
/*2C-2F*/ 0xFFFF00FF0000FFFFull, 0xFFFF00FF0000FF00ull, 0xFFFF00FF000000FFull, 0xFFFF00FF00000000ull,
/*30-33*/ 0xFFFF0000FFFFFFFFull, 0xFFFF0000FFFFFF00ull, 0xFFFF0000FFFF00FFull, 0xFFFFFFFFFFFF0000ull,
/*34-37*/ 0xFFFF0000FF00FFFFull, 0xFFFF0000FF00FF00ull, 0xFFFF0000FF0000FFull, 0xFFFFFFFFFF000000ull,
/*38-3B*/ 0xFFFF000000FFFFFFull, 0xFFFF000000FFFF00ull, 0xFFFF000000FF00FFull, 0xFFFF000000FF0000ull,
/*3C-3F*/ 0xFFFF00000000FFFFull, 0xFFFF00000000FF00ull, 0xFFFF0000000000FFull, 0xFFFF000000000000ull,
/*40-43*/ 0xFF00FFFFFFFFFFFFull, 0xFF00FFFFFFFFFF00ull, 0xFF00FFFFFFFF00FFull, 0xFF00FFFFFFFF0000ull,
/*44-47*/ 0xFF00FFFFFF00FFFFull, 0xFF00FFFFFF00FF00ull, 0xFF00FFFFFF0000FFull, 0xFF00FFFFFF000000ull,
/*48-4B*/ 0xFF00FFFF00FFFFFFull, 0xFF00FFFF00FFFF00ull, 0xFF00FFFF00FF00FFull, 0xFF00FFFF00FF0000ull,
/*4C-4F*/ 0xFF00FFFF0000FFFFull, 0xFF00FFFF0000FF00ull, 0xFF00FFFF000000FFull, 0xFF00FFFF00000000ull,
/*50-53*/ 0xFF00FF00FFFFFFFFull, 0xFF00FF00FFFFFF00ull, 0xFF00FF00FFFF00FFull, 0xFF00FF00FFFF0000ull,
/*54-57*/ 0xFF00FF00FF00FFFFull, 0xFF00FF00FF00FF00ull, 0xFF00FF00FF0000FFull, 0xFF00FF00FF000000ull,
/*58-5B*/ 0xFF00FF0000FFFFFFull, 0xFF00FF0000FFFF00ull, 0xFF00FF0000FF00FFull, 0xFF00FF0000FF0000ull,
/*5C-5F*/ 0xFF00FF000000FFFFull, 0xFF00FF000000FF00ull, 0xFF00FF00000000FFull, 0xFF00FF0000000000ull,
/*60-63*/ 0xFF0000FFFFFFFFFFull, 0xFF0000FFFFFFFF00ull, 0xFF0000FFFFFF00FFull, 0xFF0000FFFFFF0000ull,
/*64-67*/ 0xFF0000FFFF00FFFFull, 0xFF0000FFFF00FF00ull, 0xFF0000FFFF0000FFull, 0xFF0000FFFF000000ull,
/*68-6B*/ 0xFF0000FF00FFFFFFull, 0xFF0000FF00FFFF00ull, 0xFF0000FF00FF00FFull, 0xFF0000FF00FF0000ull,
/*6C-6F*/ 0xFF0000FF0000FFFFull, 0xFF0000FF0000FF00ull, 0xFF0000FF000000FFull, 0xFF0000FF00000000ull,
/*70-73*/ 0xFF000000FFFFFFFFull, 0xFF000000FFFFFF00ull, 0xFF000000FFFF00FFull, 0xFF00FFFFFFFF0000ull,
/*74-77*/ 0xFF000000FF00FFFFull, 0xFF000000FF00FF00ull, 0xFF000000FF0000FFull, 0xFF00FFFFFF000000ull,
/*78-7B*/ 0xFF00000000FFFFFFull, 0xFF00000000FFFF00ull, 0xFF00000000FF00FFull, 0xFF00000000FF0000ull,
/*7C-7F*/ 0xFF0000000000FFFFull, 0xFF0000000000FF00ull, 0xFF000000000000FFull, 0xFF00000000000000ull,
/*80-83*/ 0x00FFFFFFFFFFFFFFull, 0x00FFFFFFFFFFFF00ull, 0x00FFFFFFFFFF00FFull, 0x00FFFFFFFFFF0000ull,
/*84-87*/ 0x00FFFFFFFF00FFFFull, 0x00FFFFFFFF00FF00ull, 0x00FFFFFFFF0000FFull, 0x00FFFFFFFF000000ull,
/*88-8B*/ 0x00FFFFFF00FFFFFFull, 0x00FFFFFF00FFFF00ull, 0x00FFFFFF00FF00FFull, 0x00FFFFFF00FF0000ull,
/*8C-8F*/ 0x00FFFFFF0000FFFFull, 0x00FFFFFF0000FF00ull, 0x00FFFFFF000000FFull, 0x00FFFFFF00000000ull,
/*90-93*/ 0x00FFFF00FFFFFFFFull, 0x00FFFF00FFFFFF00ull, 0x00FFFF00FFFF00FFull, 0x00FFFF00FFFF0000ull,
/*94-97*/ 0x00FFFF00FF00FFFFull, 0x00FFFF00FF00FF00ull, 0x00FFFF00FF0000FFull, 0x00FFFF00FF000000ull,
/*98-9B*/ 0x00FFFF0000FFFFFFull, 0x00FFFF0000FFFF00ull, 0x00FFFF0000FF00FFull, 0x00FFFF0000FF0000ull,
/*9C-9F*/ 0x00FFFF000000FFFFull, 0x00FFFF000000FF00ull, 0x00FFFF00000000FFull, 0x00FFFF0000000000ull,
/*A0-A3*/ 0x00FF00FFFFFFFFFFull, 0x00FF00FFFFFFFF00ull, 0x00FF00FFFFFF00FFull, 0x00FF00FFFFFF0000ull,
/*A4-A7*/ 0x00FF00FFFF00FFFFull, 0x00FF00FFFF00FF00ull, 0x00FF00FFFF0000FFull, 0x00FF00FFFF000000ull,
/*A8-AB*/ 0x00FF00FF00FFFFFFull, 0x00FF00FF00FFFF00ull, 0x00FF00FF00FF00FFull, 0x00FF00FF00FF0000ull,
/*AC-AF*/ 0x00FF00FF0000FFFFull, 0x00FF00FF0000FF00ull, 0x00FF00FF000000FFull, 0x00FF00FF00000000ull,
/*B0-B3*/ 0x00FF0000FFFFFFFFull, 0x00FF0000FFFFFF00ull, 0x00FF0000FFFF00FFull, 0x00FFFFFFFFFF0000ull,
/*B4-B7*/ 0x00FF0000FF00FFFFull, 0x00FF0000FF00FF00ull, 0x00FF0000FF0000FFull, 0x00FFFFFFFF000000ull,
/*B8-BB*/ 0x00FF000000FFFFFFull, 0x00FF000000FFFF00ull, 0x00FF000000FF00FFull, 0x00FF000000FF0000ull,
/*BC-BF*/ 0x00FF00000000FFFFull, 0x00FF00000000FF00ull, 0x00FF0000000000FFull, 0x00FF000000000000ull,
/*C0-C3*/ 0x0000FFFFFFFFFFFFull, 0x0000FFFFFFFFFF00ull, 0x0000FFFFFFFF00FFull, 0x0000FFFFFFFF0000ull,
/*C4-C7*/ 0x0000FFFFFF00FFFFull, 0x0000FFFFFF00FF00ull, 0x0000FFFFFF0000FFull, 0x0000FFFFFF000000ull,
/*C8-CB*/ 0x0000FFFF00FFFFFFull, 0x0000FFFF00FFFF00ull, 0x0000FFFF00FF00FFull, 0x0000FFFF00FF0000ull,
/*CC-CF*/ 0x0000FFFF0000FFFFull, 0x0000FFFF0000FF00ull, 0x0000FFFF000000FFull, 0x0000FFFF00000000ull,
/*D0-D3*/ 0x0000FF00FFFFFFFFull, 0x0000FF00FFFFFF00ull, 0x0000FF00FFFF00FFull, 0x0000FF00FFFF0000ull,
/*D4-D7*/ 0x0000FF00FF00FFFFull, 0x0000FF00FF00FF00ull, 0x0000FF00FF0000FFull, 0x0000FF00FF000000ull,
/*D8-DB*/ 0x0000FF0000FFFFFFull, 0x0000FF0000FFFF00ull, 0x0000FF0000FF00FFull, 0x0000FF0000FF0000ull,
/*DC-DF*/ 0x0000FF000000FFFFull, 0x0000FF000000FF00ull, 0x0000FF00000000FFull, 0x0000FF0000000000ull,
/*E0-E3*/ 0x000000FFFFFFFFFFull, 0x000000FFFFFFFF00ull, 0x000000FFFFFF00FFull, 0x000000FFFFFF0000ull,
/*E4-E7*/ 0x000000FFFF00FFFFull, 0x000000FFFF00FF00ull, 0x000000FFFF0000FFull, 0x000000FFFF000000ull,
/*E8-EB*/ 0x000000FF00FFFFFFull, 0x000000FF00FFFF00ull, 0x000000FF00FF00FFull, 0x000000FF00FF0000ull,
/*EC-EF*/ 0x000000FF0000FFFFull, 0x000000FF0000FF00ull, 0x000000FF000000FFull, 0x000000FF00000000ull,
/*F0-F3*/ 0x00000000FFFFFFFFull, 0x00000000FFFFFF00ull, 0x00000000FFFF00FFull, 0x0000FFFFFFFF0000ull,
/*F4-F7*/ 0x00000000FF00FFFFull, 0x00000000FF00FF00ull, 0x00000000FF0000FFull, 0x0000FFFFFF000000ull,
/*F8-FB*/ 0x0000000000FFFFFFull, 0x0000000000FFFF00ull, 0x0000000000FF00FFull, 0x0000000000FF0000ull,
/*FC-FF*/ 0x000000000000FFFFull, 0x000000000000FF00ull, 0x00000000000000FFull, 0x0000000000000000ull
};


/*
    This is the "default" cfg_read routine for a PCI device, to be called if the PCI device
    did not register a (*cfg_read) callback routine. This will read the value from the
    cfg_reg[function].csr[] configuration register.
    
    If the specified function does not exist, and the call is reading register 0 (csr[0]]),
    then the routine returns all 1's, which is the value that would have been generated
    on a real PCI by the master-abort caused by the device not responding to the invalid function.
    
    If a register is reserved or not implemented by the device, it should have initialized the
    register with 0, which is the value to be returned for an unimplemented or unused register.

    If there are any side effects of the read, this default routine cannot be used effectively,
    unless the device can detect and address the side effects later during normal operation.
*/
PCI_STAT pci_cfg_read_default (PCI_DEV* dev, int slot, int function, int reg_indx, uint8 cbez, uint32* value)
{
    uint32 result;

    // If the PCI device configuration registers aren't set up correctly, we can't provide a default behavior
    if (dev->cfg_reg == NULL) {
        sim_printf("pci_cfg_read_default: Broken PCI Device: (%s) PCI_DEV.(*cfg_read) and PCI_DEV.cfg_reg are both NULL\n", dev->name);
        return PCI_NOT_ME;
    }

    // If the function exceeds the max_func, do not respond
    if ((function >= dev->functions) && (reg_indx == 0)) {
        return PCI_NOT_ME;
    }

    // read configuration register
    result = dev->cfg_reg[function].csr[reg_indx];  // 32-bit register value
    result = result & CBEZ_MASK[cbez & 0x0F];       // mask out 32-bit bytes we don't want
    *value = result;                                // return masked value
    return PCI_OK;
}

PCI_STAT pci_bus_cfg_read (PCI_BUS* bus, int slot, int function, int register_, uint8 cbez, uint32* value)
{
    if (bus->dev[slot] != NULL) {
        PCI_STAT result;
		if (bus->dev[slot]->cfg_read != NULL) {
			result = (*bus->dev[slot]->cfg_read) (bus->dev[slot], slot, function, register_, cbez, value);
		} else {
            result = pci_cfg_read_default (bus->dev[slot], slot, function, register_, cbez, value);
        }
        if (result != PCI_NOT_ME) {
            return result;
        }
    }

    // No device or function in the specified slot; return master-abort value
    *value = PCI_CONFIG_NX_READ_VALUE;
    return PCI_OK;
}

PCI_STAT pci_bus_cfg_read1 (PCI_BUS* bus, int bus_num, int dev_num, int function, int register_, uint8 cbez, uint32* value)
{
    int slot;
    // dispatch Type 1 read to subordinate buses, since a type 1 config is not addressed to THIS bus.
    // Subordinate buses must convert the Type 1 to a Type 0 based on their assigned bus numbers.
    // Devices will return something other than a PCI_NOT_ME if the request was picked up.
	for (slot=0; slot<PCI_MAX_DEV; slot++) {
		if (bus->dev[slot] != NULL) {
            if (bus->dev[slot]->cfg_read1 != NULL) {	// found device with cfg_read1 bridge function
				PCI_STAT result = (*bus->dev[slot]->cfg_read1) (bus->dev[slot], bus_num, dev_num, function, register_, cbez, value);
				if (result != PCI_NOT_ME) {
					return result;
				}
			}
		}
    }

	// Did not find the requesd bus/device/function/register
	*value = PCI_CONFIG_NX_READ_VALUE;
    return PCI_OK;
}

/*
    This is the "default" cfg_write routine for a PCI device, to be called if the PCI device
    did not register a (*cfg_write) callback routine. This will write the value into the
    cfg_reg[function].csr[] configuration register based on the cfg_wmask[function].csr[] write mask.

    If there are any side effects of the write, this default routine cannot be used effectively,
    unless the device can detect and address the side effects later during normal operation.
*/
PCI_STAT pci_cfg_write_default (PCI_DEV* dev, int slot, int function, int reg_indx, uint8 cbez, uint32 value)
{
    uint32 temp, wmask, *reg;

    // If the function does not exist, there's nothing to do. This should not normally
    // happen on a write, since the cfg_read enumeration should have spotted that there is no such
    // function number to write. Or the PCI device simulation has a bug. :-(
    if (function >= dev->functions) {
        return PCI_OK;  // illegal writes do not have to do anything
    }

    // If the PCI device configuration registers/masks aren't set up correctly, we can't provide default behavior
    if ((dev->cfg_reg == NULL) || (dev->cfg_wmask == NULL)) {
        sim_printf("pci_cfg_write_default: (%s) PCI_DEV.cfg_reg or PCI_DEV.cfg_wmask missing\n", dev->name);
        return PCI_SETUP_ERR;
    }

    // use local variables simplify write logic below
    reg    = &dev->cfg_reg[function].csr[reg_indx];                             // point to register
    wmask =  dev->cfg_wmask[function].csr[reg_indx] & CBEZ_MASK[cbez & 0x0F];   // mask write bits
    temp = *reg;                                                                // save old value

    // write masked value into the configuration register
    *reg = (temp & ~(wmask)) /* save unmasked bits */ | (value & wmask) /* set masked bits */;
    return PCI_OK;
}

PCI_STAT pci_bus_cfg_write (PCI_BUS* pci_bus, int slot, int function, int register_, uint8 cbez, uint32 value)
{
    if (pci_bus->dev[slot] == NULL) {			// No PCI device exists in that slot.
        // This situation should never occur unless the user disabled the device while the simulation was running
        // or the OS/firmware PCI enumeration/configuration routine is buggy
		return PCI_OK;                          // just return, illegal writes do not have to do anything
    } else {
	    if (pci_bus->dev[slot]->cfg_write != NULL) {   // (*cfg_write) defined?
    		return (*pci_bus->dev[slot]->cfg_write) (pci_bus->dev[slot], slot, function, register_, cbez, value);
	    } else {
            // (*cfg_write) is not defined, so call default routine instead
            return pci_cfg_write_default (pci_bus->dev[slot], slot, function, register_, cbez, value);
        }
    }
    return PCI_OK;  // illegal writes do not have to do anything
}

PCI_STAT pci_bus_cfg_write1 (PCI_BUS* pci_bus, int bus, int slot, int function, int register_, uint8 cbez, uint32 value)
{
    PCI_STAT status;
	int i;

    // Subordinate PCI bus write - route to all bridges until bus/device found (does not return PCI_NOT_ME)

    // Subordinate bridges should look at their high/low bus definitions to see if they can claim the bus number.
    // If they can, they should either pass the type 1 on to their subordinate bridges, or convert the type 1 
    // to a type 0 access if the bus number matches exactly.

    // search device list for subordinate bridges
    for (i=0; i<PCI_MAX_DEV; i++) {
		if ((pci_bus->dev[i] != NULL) && (pci_bus->dev[i]->cfg_write1 != NULL)) { // bridge detected
            status = (*pci_bus->dev[i]->cfg_write1) (pci_bus->dev[i], bus, slot, function, register_, cbez, value);
            if (status != PCI_NOT_ME) {
                return status;
            }
        }
    }

    // If we get here, the (recursive) bus was not found; this situation should not happen
    // unless the user disabled a bridge while the simulation was running, or unless
    // the OS/firmware PCI enumeration/configuration is buggy.
    // .. or unless the bridge simulation is implemented incorrectly. :-(
    return PCI_NOT_ME;
}



PCI_STAT pci_bus_io_read (PCI_BUS* bus, t_uint64 pci_address, int size, uint8 cbez, uint32* value)
{
    PCI_STAT status;
    int i;

    // Dispatch read to target device for the pci_address provided. This will only be invoked from the top (hose) bus[?].
    // On a real PCI, the address is broadcast to everyone, and the device responsible would assert DEVSEL#

    // So dispatch read to each device in turn; device will either return PCI_NOT_ME if it's not responsible for the pci_address,
    // or it will return PCI_OK or some PCI error which indicates that it has claimed the pci_address.
    // This simulates the entire PCI subsystem more accurately, where the pci_address is broadcast to everybody,
    // and the device that is responsible responds with a DEVSEL#. If we had simulated DEVSEL# with some form of (*select) routine,
    // the device would have to figure out the BAR/function mapping once for the (*select), and then have to figure it out again
    // when the operation was actually dispatched to them. This method ensures that the dispatch test will only occur once per device.
    // This method also prevents this routine from having to know the intimate details of every device.
    // Bridges must pass this request on to subordinate buses, and respond accordingly if/when the target is found.

    // Dispatch operation to all PCI devices and subordinate bridges and devices
    for (i=0; i<PCI_MAX_DEV; i++) {                 // for each slot
        if (bus->dev[i] != NULL) {
            if (bus->dev[i]->io_read != NULL) {   // if device defined the routine, call it
                status = (*bus->dev[i]->io_read) (bus->dev[i], pci_address, size, cbez, value);
                if (status != PCI_NOT_ME) {         // device claimed io address
                    return status;
                }
            }
        }
    }

    // If we get here, no device has claimed the pci_address;
    // proper response depends upon if this is the master bus (hose) or a subordinate bridge

    if (bus->parent == NULL) {  // master bus response
        *value = 0;             // operation was not claimed
        return PCI_OK;          // that's the best we could do!
    }
    return PCI_NOT_ME;          // subordinate bus response
}

PCI_STAT pci_bus_io_write (PCI_BUS* bus, t_uint64 pci_address, int size, uint8 cbez, uint32 value)
{
    PCI_STAT status;
    int i;

    // Dispatch write to target device for the pci_address provided. This will only be invoked from the top (hose) bus[?].
    // On a real PCI, the address is broadcast to everyone, and the device responsible would assert DEVSEL#

    // So dispatch to each device in turn; device will either return PCI_NOT_ME if it's not responsible for the pci_address,
    // or it will return PCI_OK or some PCI error which indicates that it has claimed the pci_address.
    // This simulates the entire PCI subsystem more accurately, where the pci_address is broadcast to everybody,
    // and the device that is responsible responds with a DEVSEL#. If we had simulated DEVSEL# with some form of (*select) routine,
    // the device would have to figure out the BAR/function mapping once for the (*select), and then have to figure it out again
    // when the operation was actually dispatched to them. This method ensures that the dispatch test will only occur once per device.
    // This method also prevents this routine from having to know the intimate details of every device.
    // Bridges must pass this request on to subordinate buses, and respond accordingly if/when the target is found.

    // Dispatch operation to all PCI devices and subordinate bridges and devices
    for (i=0; i<PCI_MAX_DEV; i++) {                 // for each slot
        if (bus->dev[i] != NULL) {
            if (bus->dev[i]->io_read != NULL) {   // if device defined the routine, call it
                status = (*bus->dev[i]->io_write) (bus->dev[i], pci_address, size, cbez, value);
                if (status != PCI_NOT_ME) {         // device claimed io address
                    return status;
                }
            }
        }
    }

    // If we get here, no device has claimed the pci_address;
    // proper response depends upon if this is the master bus (hose) or a subordinate bridge

    if (bus->parent == NULL) {  // master bus response; operation was not claimed
        return PCI_OK;          // that's the best we could do!
    }
    return PCI_NOT_ME;          // subordinate bus response
}

PCI_STAT pci_hose_mem_read (PCI_BUS* bus, t_uint64 pci_address, int size, uint8 cbez, uint32* value)
{
    PCI_STAT status;
    int slot;

    // Dispatch read to target device for the pci_address provided.
    // On a real PCI, the address is broadcast to everyone, and the device responsible would assert DEVSEL#

    // So dispatch read to each device in turn; device will either return PCI_NOT_ME if it's not responsible for the pci_address,
    // or it will return PCI_OK or some PCI error which indicates that it has claimed the pci_address.
    // This simulates the entire PCI subsystem more accurately, where the pci_address is broadcast to everybody,
    // and the device that is responsible responds with a DEVSEL#. If we had simulated DEVSEL# with some form of (*select) routine,
    // the device would have to figure out the BAR/function mapping once for the (*select), and then have to figure it out again
    // when the operation was actually dispatched to them. This method ensures that the dispatch test will only occur once per device.
    // This method also prevents this routine from having to know the intimate details of every device.
    // Bridges must pass this request on to subordinate buses, and respond accordingly if/when the target is found.

    // Dispatch operation to all PCI devices and subordinate bridges and devices
    for (slot=0; slot<PCI_MAX_DEV; slot++) {                 // for each slot
        if (bus->dev[slot] != NULL) {
            if (bus->dev[slot]->mem_read != NULL) {   // if device defined the routine, call it
                status = (*bus->dev[slot]->mem_read) (bus->dev[slot], pci_address, size, cbez, value);
                if (status != PCI_NOT_ME) {         // device claimed address
                    return status;
                }
            }
        }
    }

    // If we get here, no device has claimed the pci_address;
    // proper response depends upon if this is the master bus (hose) or a subordinate bridge

    if (bus->bus_num == 0) {    // master bus response
        *value = 0;             // operation was not claimed
        return PCI_OK;          // that's the best we could do!
    }
    return PCI_NOT_ME;          // subordinate bus response
}

PCI_STAT pci_bus_mem_read (PCI_BUS* bus, t_uint64 pci_address, int size, uint8 cbez, uint32* value)
{
    // If this bus has parent(s), re-dispatch this request to the highest parent
    // to make the unidirectional pci bus broadcast logic simpler (can ignore upwards traversal).
    PCI_BUS* current = bus;
    for (;;) {
        if (current->parent != NULL) {
            current = current->parent;
        }
    }
    // execute mem_read from top of bus heirarchy (hose)
    return pci_hose_mem_read (current, pci_address, size, cbez, value);
}

/*
	Reset (unjam) all devices on this PCI bus and all subordinate PCI buses.
    Note: Reset does NOT re-initialize configuration registers to power-up state!
*/
PCI_STAT pci_bus_reset (PCI_BUS* bus)
{
	PCI_STAT result;
    int i;

	for (i=0; i<PCI_MAX_DEV; i++) {                             // for each slot
		if (bus->dev[i] != NULL) {                              // device plugged into slot?
            if (bus->dev[i]->reset != NULL) {                   // does device have a (*reset) function?
			    result = (*bus->dev[i]->reset) (bus->dev[i]);   // call device (*reset) function if exists
            }                                                   // but don't really care what the result is..                                                  
		}
	}
    return PCI_OK;
}


/*
    Register "plugs" the PCI device into a bus slot.

    This should only be executed BEFORE the simulator starts executing.
    Hot-plugging plugging a device into the PCI bus is not likely to be recognized by the runnning OS.

    Verify slot number before inserting a device. Not all slots are connected to the bus in all simulators,
    so the OS (or firmware) enumeration may not be able to see the PCI device.

    Return values:
        PCI_OK  - request successfully executed
        PCI_
*/
PCI_STAT pci_bus_register (PCI_BUS* bus, PCI_DEV* device, int slot)
{
	int i;

	if ((bus == NULL)  || (device == NULL) || (slot >= PCI_MAX_DEV)) {		// Bad parameter?
		return PCI_ARG_ERR;
	}
	if (bus->dev[slot] != NULL) {				// Slot already occupied?
		if (bus->dev[slot] == device) {
			return PCI_OK;
		} else {
			printf ("pci_register: device '%s' cannot use slot %02d occupied by '%s'\n", device->name, slot, bus->dev[slot]->name);
			return PCI_ARG_ERR;
		}
	}

	// de-register device if it is in a different slot
	for (i=0; i<PCI_MAX_DEV; i++) {
		if (bus->dev[i] == device) {
			bus->dev[i] = NULL;
			break;
		}
	}
	// register device in the requested slot
	bus->dev[slot] = device;
	return PCI_OK;
}

/*
    Un-registering a device should only be required if the device is moving
    to a different bus or is being disabled
 */
PCI_STAT pci_bus_unregister (PCI_BUS* bus, PCI_DEV* device)
{
	int i;

	if ((bus == NULL)  || (device == NULL)) {		// Bad parameter?
		return PCI_ARG_ERR;
	}
	// de-register device
	for (i=0; i<PCI_MAX_DEV; i++) {
		if (bus->dev[i] == device) {
			bus->dev[i] = NULL;
			break;
		}
	}
	return PCI_OK;
}

PCI_STAT pci_walk_mem_read_64 (PCI_BUS* bus, t_uint64 pci_address, int size, uint8 cbez, t_uint64* value)
{
    PCI_STAT status;
    int slot;

    // Dispatch read to target device for the pci_address provided.
    // On a real PCI, the address is broadcast to everyone, and the device responsible would assert DEVSEL#

    // So dispatch operation to each device in turn; device will either return PCI_NOT_ME if it's not responsible for the pci_address,
    // or it will return PCI_OK or some PCI error which indicates that it has claimed the pci_address.
    // This simulates the entire PCI subsystem more accurately, where the pci_address is broadcast to everybody,
    // and the device that is responsible responds with a DEVSEL#. If we had simulated DEVSEL# with some form of (*select) callback,
    // the device would have to figure out the BAR/function mapping once during the (*select), and then have to figure it out again
    // when the operation was actually dispatched to them. This method ensures that the mapping test will only occur once per device.
    // This method also prevents this routine from having to know the intimate details of every device.
    // Bridges must pass this request on to subordinate buses, and respond accordingly if/when the target is found.

    // Dispatch operation to all PCI devices and subordinate bridges and devices
    for (slot=0; slot<PCI_MAX_DEV; slot++) {                // for each slot
        if (bus->dev[slot] != NULL) {
            if (bus->dev[slot]->mem_read_64 != NULL) {      // 64-bit mem_read_64 supported?
                status = (*bus->dev[slot]->mem_read_64) (bus->dev[slot], pci_address, size, cbez, value);
                if (status != PCI_NOT_ME) {                 // device claimed address
                    return status;
                }
            } else if (bus->dev[slot]->mem_read != NULL) {  // 32-bit mem_read supported?
                uint32 low = 0;
                status = (*bus->dev[slot]->mem_read) (bus->dev[slot], pci_address, size, cbez, &low);
                if (status != PCI_NOT_ME) {
                    if (size > 4) {                         // read high dword, then merge with low dword
                        uint32 high = 0;
                        status = (*bus->dev[slot]->mem_read) (bus->dev[slot], pci_address, size-4, cbez>>4, &high);
                        *value = ((t_uint64) high << 32) | low;
                    } else {
                        *value = low;
                    }
                    return status;
                }
            }
        }
    }

    // If we get here, no device has claimed the pci_address;
    // proper response depends upon if this is the master bus (hose) or a subordinate bridge

    if (bus->parent == NULL) {  // master bus response
        *value = 0;             // operation was not claimed
        return PCI_OK;          // that's the best we could do!
    }
    return PCI_NOT_ME;          // subordinate bus response
}

PCI_STAT pci_bus_mem_read_64 (PCI_BUS* bus, t_uint64 pci_address, int size, uint8 cbez, t_uint64* value)
{
    // If this bus has parent(s), re-dispatch this request to the highest parent (hose)
    // to make the unidirectional pci bus broadcast logic simpler (can ignore upwards traversal).
    PCI_BUS* current = bus;
    for (;;) {
        if (current->parent != NULL) {
            current = current->parent;
        }
    }
    // execute mem_read from top of bus heirarchy (hose)
    return pci_walk_mem_read_64 (current, pci_address, size, cbez, value);
}

PCI_STAT pci_register (PCI_BUS* pci, PCI_DEV* device, int slot)
{
    if ((pci->dev[slot] == NULL) || (pci->dev[slot] == device)) {
        pci->dev[slot] = device;
        return PCI_OK;
    } else {
        sim_printf ("pci_register: Cannot register Device(%s), Bus(%s) Slot (%d) loaded with another device! \n",
            device->name, pci->name, slot);
        return PCI_SETUP_ERR;
    }
}

PCI_STAT pci_unregister (PCI_BUS* pci, PCI_DEV* device, int slot)
{
    if ((pci->dev[slot] == NULL) || (pci->dev[slot] == device)) {
        pci->dev[slot] = NULL;
        return PCI_OK;
    } else {
        sim_printf ("pci_unregister: Cannot unregister Device(%s), Bus(%s) Slot (%d) loaded with another device! \n",
            device->name, pci->name, slot);
        return PCI_SETUP_ERR;
    }
}

PCI_STAT pci_bus_mem_readm  (PCI_BUS* bus, t_uint64 pci_src_address, uint32* lcl_dst_address, int lw_repeat)
{
    return PCI_NOFNC;   // Not implemented yet
}

PCI_STAT pci_bus_mem_readl  (PCI_BUS* bus, t_uint64 pci_src_address, uint32* lcl_dst_address, int lw_repeat)
{
    return PCI_NOFNC;   // Not implemented yet
}

PCI_STAT pci_bus_mem_read64   (PCI_BUS* bus, t_uint64 pci_src_address, int size, uint8 cbez, t_uint64* value)
{
    return PCI_NOFNC;   // Not implemented yet
}

PCI_STAT pci_bus_mem_write64  (PCI_BUS* bus, t_uint64 pci_dst_address, int size, uint8 cbez, t_uint64  value)
{
    return PCI_NOFNC;   // Not implemented yet
}

PCI_STAT pci_bus_mem_writei (PCI_BUS* bus, t_uint64 pci_dst_address, uint32* lcl_src_address, int lw_repeat)
{
    return PCI_NOFNC;   // Not implemented yet
}

PCI_STAT pci_bus_mem_write  (PCI_BUS* bus, t_uint64 pci_dst_address, int size, uint8 cbez, uint32 value)
{
    return PCI_NOFNC;   // Not implemented yet
}

PCI_STAT pci_bus_special    (PCI_BUS* bus, uint32 value)
{
    return PCI_NOFNC;   // Not implemented yet
}
