/* alpha_pyxis.c: Alpha Pyxis 21174 Core logic chip

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
   THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of the author shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author.

*/

/*
	The 21174 "Pyxis" Core Logic Chip sits between the Alpha CPU and the real world
	and handles all memory and I/O operations for the Alpha CPU.

	Documentation:
		"Digital Semiconductor 21174 Core Logic Chip Technical Reference Manual",
		 Order Number: EC-R12GC-TE

	The Pyxis chip has the following features: [ref: TRM, 1.1]
		Synchronous dynamic RAM (DRAM) memory controller
		Optional Bcache (level 3 cache)
		64-bit PCI bus at 33 MHz
		64 interrupts through external shift register
		32 general-purpose inputs through external shift register
		32 general-purpose outputs through external shift register
		3.3V design
		Quadword ECC support, longword parity, or no parity on system and memory buses
		Onchip phase-locked loop (PLL)
		Direct attachment of flash ROM
		Startup from flash ROM
		Compact design, complete interface in single 474-pin ball grid array (BGA)
		1000 MB/s peak memory bandwidth
		Glueless workstation memory controller

*/
/* Interrupt documentation hints:
    http://lxr.free-electrons.com/source/arch/alpha/kernel/sys_miata.c
    General Miata MX5/Pyxis vs. MiataGL/Pyxis notes:
    http://www.freebsd.org/releases/6.3R/hardware-alpha.html#AEN187
*/

/*
	History:
	--------
    2016-07-09  DTH  Removed upper 1MB from DIB to allow CPU to claim CBU I/O registers
	2015-07-24  DTH  Completed register read/write baseline
	2015-07-21  DTH  Completed register masks
	2015-07-15  DTH  Started
*/
#include "sim_defs.h"
#include "alpha_defs.h"
#include "alpha_sys_defs.h"
#include "alpha_pyxis.h"
#include "sim_pci.h"

// required external references
extern t_uint64 *M;		// main memory array (in alpha_cpu.c)
extern t_uint64 PC_Global;
extern UNIT cpu_unit;	// main memory size in bytes is in cpu_unit.capac (in alpha_cpu.c)

/* forwards */
t_uint64 pyxis_readPA (t_uint64 pa, int bytes);
t_uint64 pyxis_read_csr (t_uint64 pa, uint32 lnth);
t_bool pyxis_write_csr (t_uint64 pa, t_uint64 value, uint32 lnth);
t_stat pyxis_reset (DEVICE* dev);
t_bool rom_rd (t_uint64 pa, t_uint64 *val, uint32 lnt);

/*
    This encodes the [ByteOffset##Size] into a cbez mask. [HRM, Table 6-6]
*/
const uint8 pyxis_sparse_encode_cbez[16] = {
            // Offset   // Type         // Lanes 7..0
    0xFE,   // 00       // Byte(00)     // 0000 000X
    0xFC,   // 00       // Word(01)     // 0000 00XX
    0xF8,   // 00       // Tribyte(10)  // 0000 0XXX
    0xF0,   // 00       // Longword(11) // 0000 XXXX

    0xFD,   // 01       // Byte(00)     // 0000 00X0
    0xF9,   // 01       // Word(01)     // 0000 0XX0
    0xF1,   // 01       // Tribyte(10)  // 0000 XXX0
    0xF0,   // 01       // Illegal(11)  // 0000 XXXX

    0xFB,   // 10       // Byte(00)     // 0000 0X00
    0xF3,   // 10       // Word(01)     // 0000 XX00
    0xF0,   // 10       // Illegal(10)  // 0000 XXXX
    0xF0,   // 10       // Illegal(11)  // 0000 XXXX

    0xF7,   // 11       // Byte(00)     // 0000 X000
    0xF0,   // 11       // Illegal(01)  // 0000 XXXX
    0xF0,   // 11       // Illegal(10)  // 0000 XXXX
    0x00,   // 11       // Quadword(11) // XXXX XXXX
};

UNIT pyxis_unit[] = {
    { NULL }
};

// Pxyis registers
REG pyxis_sim_reg[] = {
    { NULL }
    };

struct pyxis_reg_t pyxis_reg;  // There are 120 CSR registers

// PCI bus attached to the Pyxis
struct pci_bus_t pyxis_pci64 = {
	"PYXIS 64-bit PCI",		// name
	{0}						// registered
};

// function forwards for DIB
t_bool pyxis_read  (t_uint64 pa, t_uint64* value, uint32 size);
t_bool pyxis_write (t_uint64 pa, t_uint64  value, uint32 size);

DEBTAB pyxis_debug[] = {
  {"IO",   DBG_IO,    "watch I/O address read/write"},
  {"WARN", DBG_WARN,  "display warnings"},
  {0}
};

// Almost all I/O is routed through the Pyxis chip, so the DIB claims
// all 40-bit address space that is not main memory (0-1F.FFFF.FFFF)
// except for the upper 1MB (FF.FFF0.0000-FF.FFFF.FFFF) which
// is reservered for external CPU registers
DIB pyxis_dib = {
    0x0200000000ull,        // low  - 02.0000.0000
    0xFFFFEFFFFFull,        // high - FF.FFEF.FFFF (not FF.FFFF.FFFF !)
    &pyxis_read,            // read
    &pyxis_write,           // write
    0                       // ipl
};

DEVICE pyxis_dev = {
    "PYXIS", pyxis_unit, pyxis_sim_reg, NULL/*pyxis_mod*/,
    1, 0, 8, 4, 0, 32,
    NULL, NULL, pyxis_reset/*&clk_reset*/,
    NULL, NULL, NULL,
    &pyxis_dib/*dib*/, DEV_DEBUG | DEV_DIB,
    DBG_IO|DBG_WARN/*0*/,   // debug control (debug flags enabled)
    pyxis_debug,            // debug flags
    NULL, NULL, NULL/*&clk_help*/, NULL, NULL, 
    NULL/*&clk_description*/
    };


void pyxis_pci_reset (void)
{
	pci_bus_reset (&pyxis_pci64);	// This resets all devices and error conditions on this PCI bus
}

void pyxis_pci_sparse_io_encode (t_uint64 pa, t_uint64* pci_addr, uint8* cbez)
{
    // pa = raw addr_h<39:0>
    // pci_addr = target pci address, needs final longword/quadword alignment
    // pci_cbe = pci byte enable mask, encoding byte/word/tribyte/longword/quadword alignment
    //          .. note that pci_cbe is really pci_cbe# (bit reversed; 0= enable, 1=disable)
    uint8 offset_len = (pa & PCI_SIO_PA_ENCODE_MASK) >> PCI_SIO_PA_ENCODE_V;  // pci_encode =  pa<6:3> >> 3

    // Alignment - sparse addresses are longword aligned, except for the single case of quadword alignment
    // ..for longword alignment, copy addr_h<7> to pci_addr<2> and clear pci_addr<1:0>
    // ..for quadword alignment fixup (later) clear pci_addr<2:0>
    *pci_addr = (pa & 0x3FFFFF80ul) >> 5;
    if (offset_len == 0xF) {    // if Quadword
        *pci_addr &= ~0x4;      // clear pci_addr<2> to align quadword
    }
    if ((pa >= 0x85C0000000) && (pa <= 0x85FFFFFFFF)) { // If Sparse IO B region add HAE_MEM offset
        *pci_addr |= pyxis_reg.hae_mem;
    }
    *cbez = pyxis_sparse_encode_cbez[offset_len];
}

t_bool pyxis_read (t_uint64 pa, t_uint64* value, uint32 lnth)
{
    t_uint64    pci_addr;
    PCI_STAT    pci_resl;
    uint8       cbez;    // PCI cbe# (inverted byte encoding bits; 0=enable, 1=disable)
    uint8       pci_encode;
    uint8       pci_byte_offset;

    // debug I/O dispatch
    if (pyxis_dev.dctrl & DBG_WARN) {
        if (pa > 0xFFFFFFFFFFull) {
            sim_debug (DBG_WARN, &pyxis_dev, "pyxis_read: Warning: PA is > 0xFF.FFFF.FFFF\n");
        }
    }
    sim_debug (DBG_IO, &pyxis_dev, "pyxis_read: @PC(%llx) addr=%llx, len=%d\n", (PC-4), pa, lnth);
    
    // --------------- main I/O dispatch code ------------

    *value = 0;         // default
	// System Address Space - dispatch table (21174 TRM, Section 6.1, Address Map)
	//	.. unrecognized addresses return 0 .. where is this rule stated ??
	
    // Table 6-1 Physical Address Map (Byte/Word Mode Disabled)
	if (pa <= 0x01FFFFFFFF) {												// Main memory space (8GB)
		// Flash mapped at address 0? (21174 TRM, Section 4.9 Flash ROM Address Space)
		if (((pyxis_reg.flash_ctrl & FLASH_CTRL__FLASH_LOW_ENABLE) != 0) && (pa <= 0x0000FFFFFF)) {
            t_uint64 rom_pa = (pa & (ROMSIZE - 1)) + ROMBASE;
            return rom_rd (rom_pa, value, lnth);
        } else if (pa <= MEMSIZE) {	// in physically allocated memory space?
			*value = M[pa >> 3];		// yes, get from memory array
            return TRUE;
		} else {					// not in physically allocated memory space
            *value = 0;
			return FALSE;	// non-existant memory reference; read zero, might generate a NEM interrupt
		}
	} else if ((pa >= 0x0E00000000ULL) && (pa <= 0x0EFFFFFFFFULL)) {	// Dummy memory region
        *value = 0;
        return TRUE;
    } else if ((pa >= 0x0FFC000000ull) && (pa <= 0x1FFFFFFFFFull)) {    // High flash region f.fc00.000-1f.ffff.ffff
        // if high flash region is enabled, fixup address and send to ROM, else nxm
        if (pyxis_reg.flash_ctrl & FLASH_CTRL__FLASH_HIGH_ENABLE) {
            // Revector through ROM routines -
            // Since ROM repeats through region, trim PA down to ROMSIZE and shift up to ROMBASE
            t_uint64 rom_pa = (pa & (ROMSIZE - 1)) + ROMBASE;
            return rom_rd (rom_pa, value, lnth);
        } else {
            *value = 0;
            sim_debug (DBG_WARN, &pyxis_dev, "pyxis_read: invalid high flash read @ %x\n", pa);
            return FALSE;
        }
	} else if ((pa >= 0x8000000000ULL) && (pa <= 0x83FFFFFFFFULL)) {	// PCI sparse memory region 0, 512MB
        sim_debug (DBG_WARN, &pyxis_dev, "alpha_pyxis.c/pyxis_read(): Unimplemented PCI sparse memory region 0 (%llx)\n", pa);
	} else if ((pa >= 0x8400000000ULL) && (pa <= 0x84FFFFFFFFULL)) {	// PCI sparse memory region 1, 128MB
        sim_debug (DBG_WARN, &pyxis_dev, "alpha_pyxis.c/pyxis_read(): Unimplemented PCI sparse memory region 1 (%llx)\n", pa);
	} else if ((pa >= 0x8500000000ULL) && (pa <= 0x857FFFFFFFULL)) {	// PCI sparse memory region 2,  64MB
        sim_debug (DBG_WARN, &pyxis_dev, "alpha_pyxis.c/pyxis_read(): Unimplemented PCI sparse memory region 2 (%llx)\n", pa);
	} else if ((pa >= 0x8580000000ULL) && (pa <= 0x85BFFFFFFFULL)) {	// PCI sparse I/O space region A, 32MB [TRM 6.8]
        sim_debug (DBG_WARN, &pyxis_dev, "alpha_pyxis.c/pyxis_read(): Unverified PCI sparse I/O region A pa(%llx)\n", pa);
        pci_addr = (pa & PCI_SIO_PA_ADDR_MASK) >> PCI_SIO_PA_ADDR_V;        // pci_addr = pa<29:8> >> 5
        pyxis_pci_sparse_io_encode (pa, &pci_addr, &cbez);
        pci_resl = pci_bus_io_read (&pyxis_pci64, pci_addr, lnth, cbez, (uint32*) value);
        sim_debug (DBG_IO, &pyxis_dev, "pci_sparse_io_A_read: @PC(%llx) pa(%llx) lnth(%d) cbez(0x%02x)[%s] value(0x%08x)\n",
                PC-4, pci_addr, lnth, cbez, CBEZ_LANES[cbez], (uint32*)*value);
        return TRUE;
    } else if ((pa >= 0x85C0000000ULL) && (pa <= 0x85FFFFFFFFULL)) {	// PCI sparse I/O space region B, 32MB [TRM 6.8]
        sim_debug (DBG_WARN, &pyxis_dev, "alpha_pyxis.c/pyxis_read(): Unverifieded PCI sparse I/O region B pa(%llx)\n", pa);
        pci_addr = (pyxis_reg.hae_io & PCI_SIO_HAE_IO_MASK) | (pa & PCI_SIO_ADDR_MASK); // pci_addr = hae_io<31:25> | pa<24:0>
        pyxis_pci_sparse_io_encode (pa, &pci_addr, &cbez);
        pci_resl = pci_bus_io_read (&pyxis_pci64, pci_addr, lnth, cbez, (uint32*) value);
        sim_debug (DBG_IO, &pyxis_dev, "pci_sparse_io_B_read: @PC(%llx) pa(%llx) lnth(%d) cbez(0x%02x)[%s] value(0x%08x)\n",
                PC-4, pci_addr, lnth, cbez, CBEZ_LANES[cbez], (uint32*)*value);
        return TRUE;
    } else if ((pa >= 0x8600000000ULL) && (pa <= 0x86FFFFFFFFULL)) {	// PCI dense memory
        sim_debug (DBG_WARN, &pyxis_dev, "alpha_pyxis.c/pyxis_read(): Unimplemented PCI dense memory region (%llx)\n", pa);
	} else if ((pa >= 0x8700000000ULL) && (pa <= 0x871FFFFFFFULL)) {	// PCI sparse configuration space [ref: TRM 6.9]
		t_stat  result;
		int bus;
		int	device;
		int	function;
		int	register_;
		int type;
        int offset_len;
        uint8 cbez = 0;
		bus =       ((pa & 0x000000001FE00000ull) >> 21);	// PCI (heirarchial) bus number (nonzero if type = 1)
		device =    ((pa & 0x00000000001F0000ull) >> 16);	// PCI device (slot number)
		function =  ((pa & 0x000000000000E000ull) >> 13);	// PCI function
		register_ = ((pa & 0x0000000000001F80ull) >> 5);	// PCI register offset<7:2>, <1:0> are always zero
		offset_len =((pa & 0x0000000000000078ull) >> 3);	// offset + len [ref: TRM Table 6-10]
		type =      pyxis_reg.cfg & CFG__CFG;
        cbez =      pyxis_sparse_encode_cbez[offset_len];
        if (lnth == 3) {    // Quadword read to Configuration space!
            sim_printf ("pyxis_read: Quadword PCI Configuration read violates PCI 2.1 spec\n");
        }
        if (type == 0) {
		    result = pci_bus_cfg_read (&pyxis_pci64, device, function, register_, cbez, (uint32*)value);
            sim_debug (DBG_IO, &pyxis_dev, "pci_sparse_cfg_read: @PC(%llx) bus(%d) slot(%d) func(%d) reg(0x%02x) cbez(0x%02x)[%s] value(0x%08x)\n",
                PC-4, bus, device, function, register_, cbez, CBEZ_LANES[cbez], (uint32*)*value);
        } else {
            result = pci_bus_cfg_read1 (&pyxis_pci64, bus, device, function, register_, cbez, (uint32*)value);
            sim_debug (DBG_IO, &pyxis_dev, "pci_sparse_cfg_read1: @PC(%llx) bus(%d) slot(%d) func(%d) reg(0x%02x) cbez(0x%02x)[%s] value(0x%08x)\n",
                PC-4, bus, device, function, register_, cbez, CBEZ_LANES[cbez], (uint32*)*value);
        }

		if (result == SCPE_NXM) {	// This is the place to return an NEM(NXM) interrupt if requred
									// pci_config_read will set value appropriately if NXM
		}
		return TRUE;
	} else if ((pa >= 0x8720000000ULL) && (pa <= 0x873FFFFFFFULL)) {	// PCI special/interrupt acknowledge
        sim_debug (DBG_WARN, &pyxis_dev, "alpha_pyxis.c/pyxis_read(): Unimplemented PCI special/interrupt acknowledge (%llx)\n", pa);
	} else if ((pa >= 0x8740000000ULL) && (pa <= 0x87AFFFFFFFULL)) {	// 21174 main CSRs
		*value = pyxis_read_csr(pa, lnth);
        return TRUE;
	} else {
		// Is Byte/Word Mode Enabled for high memory accesses? If so, dispatch..
		if (((pyxis_reg.pyxis_ctrl1 & PYXIS_CTRL1__IOA_BEN) != 0) && (pa >= 0x8800000000ull)) {
            if        ((pa >= 0x8800000000ull) && (pa <= 0x88FFFFFFFFull)) {    // PCI memory space INT8
            } else if ((pa >= 0x9800000000ull) && (pa <= 0x98FFFFFFFFull)) {    // PCI memory space INT4
            } else if ((pa >= 0xA800000000ull) && (pa <= 0xA8FFFFFFFFull)) {    // PCI memory space INT2
            } else if ((pa >= 0xB800000000ull) && (pa <= 0xB8FFFFFFFFull)) {    // PCI memory space INT1
            } else if ((pa >= 0x8900000000ull) && (pa <= 0x89FFFFFFFFull)) {    // PCI I/O space INT8
            } else if ((pa >= 0x9900000000ull) && (pa <= 0x99FFFFFFFFull)) {    // PCI I/O space INT4
            } else if ((pa >= 0xA900000000ull) && (pa <= 0xA9FFFFFFFFull)) {    // PCI I/O space INT2
            } else if ((pa >= 0xB900000000ull) && (pa <= 0xB9FFFFFFFFull)) {    // PCI I/O space INT1
            } else if ((pa >= 0x8A00000000ull) && (pa <= 0x8AFFFFFFFFull)) {    // PCI configuration space, type 0, INT8
            } else if ((pa >= 0x9A00000000ull) && (pa <= 0x9AFFFFFFFFull)) {    // PCI configuration space, type 0, INT4
            } else if ((pa >= 0xAA00000000ull) && (pa <= 0xAAFFFFFFFFull)) {    // PCI configuration space, type 0, INT2
            } else if ((pa >= 0xBA00000000ull) && (pa <= 0xBAFFFFFFFFull)) {    // PCI configuration space, type 0, INT1
            } else if ((pa >= 0x8B00000000ull) && (pa <= 0x8BFFFFFFFFull)) {    // PCI configuration space, type 1, INT8
            } else if ((pa >= 0x9B00000000ull) && (pa <= 0x9BFFFFFFFFull)) {    // PCI configuration space, type 1, INT4
            } else if ((pa >= 0xAB00000000ull) && (pa <= 0xABFFFFFFFFull)) {    // PCI configuration space, type 1, INT2
            } else if ((pa >= 0xBB00000000ull) && (pa <= 0xBBFFFFFFFFull)) {    // PCI configuration space, type 1, INT1
            } else if ((pa >= 0xC7C0000000ull) && (pa <= 0xC7FFFFFFFFull)) {    // Flash ROM read/write space
            } // IOA_BEN mode dispatch
		} // if IOA_BEN
	}
    sim_debug (DBG_IO, &pyxis_dev, "pyxis_read: undispatched IO address = %llx\n", pa);
	return FALSE;  // Unknown/unmapped address; this can generate an NEM (Non-Existant Memory) error
}

t_bool pyxis_write (t_uint64 pa, t_uint64 value, uint32 lnth)
{
    PCI_STAT    pci_resl;
    t_bool retval = FALSE;
    uint8       cbez;    // PCI cbe# (inverted byte encoding bits; 0=enable, 1=disable)

    // System Address Space - dispatch table (21174 TRM, Section 6.1, Address Map)
	//	.. unrecognized addresses return 0 .. where is this rule stated ??
	
    if (pyxis_dev.dctrl & DBG_WARN) {
        if (pa > 0xFFFFFFFFFFull) {
            sim_debug (DBG_WARN, &pyxis_dev, "pyxis_write: Warning: PA is > 0xFF.FFFF.FFFF\n");
        }
    }
    sim_debug (DBG_IO, &pyxis_dev, "pyxis_write: @PC(%llx) addr=%llx, len=%d, value=%llx\n", (PC-4), pa, lnth, value);

	// Table 6-1 Physical Address Map (Byte/Word Mode Disabled)
	if        ((pa >= 0x0E00000000ULL) && (pa <= 0x0EFFFFFFFFULL)) {	// Dummy memory region
    } else if ((pa >= 0x0FFC000000ull) && (pa <= 0x1FFFFFFFFFull)) {    // High flash region
        sim_debug (DBG_WARN, &pyxis_dev, "pyxis_write: invalid high flash write @ %x\n", pa);
        return FALSE;
	} else if ((pa >= 0x8000000000ULL) && (pa <= 0x83FFFFFFFFULL)) {	// PCI sparse memory region 0, 512MB
	} else if ((pa >= 0x8400000000ULL) && (pa <= 0x84FFFFFFFFULL)) {	// PCI sparse memory region 1, 128MB
	} else if ((pa >= 0x8500000000ULL) && (pa <= 0x857FFFFFFFULL)) {	// PCI sparse memory region 2,  64MB
	} else if ((pa >= 0x8580000000ULL) && (pa <= 0x85BFFFFFFFULL)) {	// PCI sparse I/O space region A, 32MB [TRM 6.8]
        t_uint64 pci_addr = pa & PCI_SIO_ADDR_MASK;                     // pci_addr = pa<24:0>
        pyxis_pci_sparse_io_encode (pa, &pci_addr, &cbez);
        pci_resl = pci_bus_io_write (&pyxis_pci64, pci_addr, lnth, cbez, value);
        sim_debug (DBG_IO, &pyxis_dev, "pci_sparse_io_A_write: @PC(%llx) pa(%llx) lnth(%d) cbez(0x%02x)[%s] value(0x%08x)\n",
                PC-4, pci_addr, lnth, cbez, CBEZ_LANES[cbez], (uint32)value);
        return TRUE;
	} else if ((pa >= 0x85C0000000ULL) && (pa <= 0x85FFFFFFFFULL)) {	// PCI sparse I/O space region B, 32MB [TRM 6.8]
        t_uint64 pci_addr = (pyxis_reg.hae_io & PCI_SIO_HAE_IO_MASK)    // pci_addr = hae_io<31:25> | pa<24:0>
                          | (pa & PCI_SIO_ADDR_MASK);
        pyxis_pci_sparse_io_encode (pa, &pci_addr, &cbez);
        pci_resl = pci_bus_io_write (&pyxis_pci64, pci_addr, lnth, cbez, value);
        sim_debug (DBG_IO, &pyxis_dev, "pci_sparse_io_B_write: @PC(%llx) pa(%llx) lnth(%d) cbez(0x%02x)[%s] value(0x%08x)\n",
                PC-4, pci_addr, lnth, cbez, CBEZ_LANES[cbez], (uint32)value);
        return TRUE;
	} else if ((pa >= 0x8600000000ULL) && (pa <= 0x86FFFFFFFFULL)) {	// PCI dense memory
	} else if ((pa >= 0x8700000000ULL) && (pa <= 0x871FFFFFFFULL)) {	// PCI sparse configuration space [ref: TRM 6.9]
		t_stat  result;
		int bus;
		int	device;
		int	function;
		int	register_;
		int	offset_len;
        uint8 cbez = 0;
        int type;
		bus =       ((pa & 0x000000001FE00000ull) >> 21);	// PCI (heirarchial) bus number
		device =    ((pa & 0x00000000001F0000ull) >> 16);	// PCI device (slot number)
		function =  ((pa & 0x000000000000E000ull) >> 13);	// PCI function
		register_ = ((pa & 0x0000000000001F80ull) >> 5);	// PCI register offset<7:2>, <1:0> are always zero
		offset_len =((pa & 0x0000000000000078ull) >> 3);	// offset + len [ref: TRM Table 6-10]
		type =      pyxis_reg.cfg & CFG__CFG;
        cbez =      pyxis_sparse_encode_cbez[offset_len];
        if (lnth == 3) {    // Quadword write to Configuration space!
            sim_printf ("pyxis_write: Quadword PCI Configuration write violates PCI 2.1 spec\n");
        }        //		result = pci_config_write (&pyxis_pci64, bus, device, function, register_, offset, length, type, value);
        if (type == 0) {
		    result = pci_bus_cfg_write (&pyxis_pci64, device, function, register_, cbez, (uint32)value);
            sim_debug (DBG_IO, &pyxis_dev, "pci_sparse_cfg_write: @PC(%llx) bus(%d) slot(%d) func(%d) reg(0x%02x) cbez(0x%02x)[%s] value(0x%08x)\n",
                PC-4, bus, device, function, register_, cbez, CBEZ_LANES[cbez], (uint32)value);
        } else {
            result = pci_bus_cfg_write1 (&pyxis_pci64, bus, device, function, register_, cbez, (uint32)value);
            sim_debug (DBG_IO, &pyxis_dev, "pci_sparse_cfg_write1: @PC(%llx) bus(%d) slot(%d) func(%d) reg(0x%02x) cbez(0x%02x)[%s] value(0x%08x)\n",
                PC-4, bus, device, function, register_, cbez, CBEZ_LANES[cbez], (uint32)value);        }
        if (result == SCPE_NXM) {	// This is the place to return an NEM(NXM) interrupt if requred
									// pci_config_read will set value appropriately if NXM
		}
		return TRUE;
	} else if ((pa >= 0x8720000000ULL) && (pa <= 0x873FFFFFFFULL)) {	// PCI special/interrupt acknowledge
	} else if ((pa >= 0x8740000000ULL) && (pa <= 0x87AFFFFFFFULL)) {	// 21174 main CSRs
		return pyxis_write_csr(pa, value, lnth);
	} else {
		// Is Byte/Word Mode Enabled for high memory accesses? If so, dispatch..
		if (((pyxis_reg.pyxis_ctrl1 & PYXIS_CTRL1__IOA_BEN) != 0) && (pa >= 0x8800000000ULL)) {
            if        ((pa >= 0x8800000000ull) && (pa <= 0x88FFFFFFFFull)) {    // PCI memory space INT8
            } else if ((pa >= 0x9800000000ull) && (pa <= 0x98FFFFFFFFull)) {    // PCI memory space INT4
            } else if ((pa >= 0xA800000000ull) && (pa <= 0xA8FFFFFFFFull)) {    // PCI memory space INT2
            } else if ((pa >= 0xB800000000ull) && (pa <= 0xB8FFFFFFFFull)) {    // PCI memory space INT1
            } else if ((pa >= 0x8900000000ull) && (pa <= 0x89FFFFFFFFull)) {    // PCI I/O space INT8
            } else if ((pa >= 0x9900000000ull) && (pa <= 0x99FFFFFFFFull)) {    // PCI I/O space INT4
            } else if ((pa >= 0xA900000000ull) && (pa <= 0xA9FFFFFFFFull)) {    // PCI I/O space INT2
            } else if ((pa >= 0xB900000000ull) && (pa <= 0xB9FFFFFFFFull)) {    // PCI I/O space INT1
            } else if ((pa >= 0x8A00000000ull) && (pa <= 0x8AFFFFFFFFull)) {    // PCI configuration space, type 0, INT8
            } else if ((pa >= 0x9A00000000ull) && (pa <= 0x9AFFFFFFFFull)) {    // PCI configuration space, type 0, INT4
            } else if ((pa >= 0xAA00000000ull) && (pa <= 0xAAFFFFFFFFull)) {    // PCI configuration space, type 0, INT2
            } else if ((pa >= 0xBA00000000ull) && (pa <= 0xBAFFFFFFFFull)) {    // PCI configuration space, type 0, INT1
            } else if ((pa >= 0x8B00000000ull) && (pa <= 0x8BFFFFFFFFull)) {    // PCI configuration space, type 1, INT8
            } else if ((pa >= 0x9B00000000ull) && (pa <= 0x9BFFFFFFFFull)) {    // PCI configuration space, type 1, INT4
            } else if ((pa >= 0xAB00000000ull) && (pa <= 0xABFFFFFFFFull)) {    // PCI configuration space, type 1, INT2
            } else if ((pa >= 0xBB00000000ull) && (pa <= 0xBBFFFFFFFFull)) {    // PCI configuration space, type 1, INT1
            } else if ((pa >= 0xC7C0000000ull) && (pa <= 0xC7FFFFFFFFull)) {    // Flash ROM read/write space
            } // IOA_BEN mode dispatch
        } // if IOA_BEN
	}
    sim_debug (DBG_IO, &pyxis_dev, "pyxis_write: undispatched IO address = %llx\n", pa);
	return FALSE;  // Unknown/unmapped address; this can generate an NEM (Non-Existant Memory) error
}

const char* pyxis_regid(t_uint64 pa)
{
	switch (pa) {
	// General Registers [21174 TRM, Section 4.3]
	case PYXIS_REV__CSR:
         return "PYXIS_REV";
	case PCI_LAT__CSR:
		return "PYXIS_LAT";
	case PYXIS_CTRL__CSR:
		return "PYXIS_CTRL";
	case PYXIS_CTRL1__CSR:
		return "PYXIS_CTRL1";
	case FLASH_CTRL__CSR:
		return "FLASH_CTRL";
	case HAE_MEM__CSR:
		return "HAE_MEM";
	case HAE_IO__CSR:
		return "HAE_IO";
	case CFG__CSR:
		return "CFG";
	case PYXIS_DIAG__CSR:
		return "PYXIS_DIAG";
	case DIAG_CHECK__CSR:
		return "DIAG_CHECK";
	case PERF_MONITOR__CSR:
		return "PERF_MONITOR";
	case PERF_CONTROL__CSR:
		return "PERF_CONTROL";
	case PYXIS_ERR__CSR:
		return "PYXIS_ERR";
	case PYXIS_STAT__CSR:
		return "PYXIS_STAT";
	case ERR_MASK__CSR:
		return "ERR_MASK";
	case PYXIS_SYN__CSR:
		return "PYXIS_SYN";
	case PYXIS_ERR_DATA__CSR:
		return "PYXIS_ERR_DATA";
	case MEAR__CSR:
		return "MEAR";
	case MESR__CSR:
		return "MESR";
	case PCI_ERR0__CSR:
		return "PCI_ERR0";
	case PCI_ERR1__CSR:
		return "PCI_ERR1";
	case PCI_ERR2__CSR:
		return "PCI_ERR2";

	// Memory Controller Registers [21174 TRM, Section 4.4]
	case MCR__CSR:
		return "MCR";
	case MCMR__CSR:
		return "MCMR";
	case GTR__CSR:
		return "GTR";
	case RTR__CSR:
		return "RTR";
	case RHPR__CSR:
		return "RHPR";
	case MDR1__CSR:
		return "MDR1";
	case MDR2__CSR:
		return "MDR2";
	case BBAR0__CSR:
		return "BBAR0";
	case BBAR1__CSR:
		return "BBAR1";
	case BBAR2__CSR:
		return "BBAR2";
	case BBAR3__CSR:
		return "BBAR3";
	case BBAR4__CSR:
		return "BBAR4";
	case BBAR5__CSR:
		return "BBAR5";
	case BBAR6__CSR:
		return "BBAR6";
	case BBAR7__CSR:
		return "BBAR7";
	case BCR0__CSR:
		return "BCR0";
	case BCR1__CSR:
		return "BCR1";
	case BCR2__CSR:
		return "BCR2";
	case BCR3__CSR:
		return "BCR3";
	case BCR4__CSR:
		return "BCR4";
    case BCR5__CSR:
		return "BCR5";
	case BCR6__CSR:
		return "BCR6";
	case BCR7__CSR:
		return "BCR7";
	case BTR0__CSR:
		return "BTR0";
	case BTR1__CSR:
		return "BTR1";
	case BTR2__CSR:
		return "BTR2";
	case BTR3__CSR:
		return "BTR3";
	case BTR4__CSR:
		return "BTR4";
	case BTR5__CSR:
		return "BTR5";
	case BTR6__CSR:
		return "BTR6";
	case BTR7__CSR:
		return "BTR7";
	case CVM__CSR:
		return "CVM";

	// PCI Window Control Registers [21174 TRM, Section 4.5]
	case TBIA__CSR:
		return "TBIA";
	case W0_BASE__CSR:
		return "W0_BASE";
	case W0_MASK__CSR:
		return "W0_MASK";
	case T0_BASE__CSR:
		return "T0_BASE";
	case W1_BASE__CSR:
		return "W1_BASE";
	case W1_MASK__CSR:
		return "W1_MASK";
	case T1_BASE__CSR:
		return "T1_BASE";
	case W2_BASE__CSR:
		return "W2_BASE";
	case W2_MASK__CSR:
		return "W2_MASK";
	case T2_BASE__CSR:
		return "T2_BASE";
	case W3_BASE__CSR:
		return "W3_BASE";
	case W3_MASK__CSR:
		return "W3_MASK";
	case T3_BASE__CSR:
		return "T3_BASE";
	case W_DAC__CSR:
		return "W_DAC";

	// Scatter-Gather Address Translation Registers [21174 TRM, Section 4.6]
	case LTB_TAG0__CSR:
		return "LTB_TAG0";
	case LTB_TAG1__CSR:
		return "LTB_TAG1";
	case LTB_TAG2__CSR:
		return "LTB_TAG2";
	case LTB_TAG3__CSR:
		return "LTB_TAG3";
	case TB_TAG4__CSR:
		return "TB_TAG4";
	case TB_TAG5__CSR:
		return "TB_TAG5";
	case TB_TAG6__CSR:
		return "TB_TAG6";
	case TB_TAG7__CSR:
		return "TB_TAG7";
	case TB0_PAGE0__CSR:
		return "TB0_PAG0";
	case TB0_PAGE1__CSR:
		return "TB0_PAGE1";
	case TB0_PAGE2__CSR:
		return "TB0_PAGE2";
	case TB0_PAGE3__CSR:
		return "TB0_PAGE3";
	case TB1_PAGE0__CSR:
		return "TB1_PAGE0";
	case TB1_PAGE1__CSR:
		return "TB1_PAGE1";
	case TB1_PAGE2__CSR:
		return "TB1_PAGE2";
	case TB1_PAGE3__CSR:
		return "TB1_PAGE3";
	case TB2_PAGE0__CSR:
		return "TB2_PAGE0";
	case TB2_PAGE1__CSR:
		return "TB2_PAGE1";
	case TB2_PAGE2__CSR:
		return "TB2_PAGE2";
	case TB2_PAGE3__CSR:
		return "TB2_PAGE3";
	case TB3_PAGE0__CSR:
		return "TB3_PAGE0";
	case TB3_PAGE1__CSR:
		return "TB3_PAGE1";
	case TB3_PAGE2__CSR:
		return "TB3_PAGE2";
	case TB3_PAGE3__CSR:
		return "TB3_PAGE3";
	case TB4_PAGE0__CSR:
		return "TB4_PAGE0";
	case TB4_PAGE1__CSR:
		return "TB4_PAGE1";
	case TB4_PAGE2__CSR:
		return "TB4_PAGE2";
	case TB4_PAGE3__CSR:
		return "TB4_PAGE3";
	case TB5_PAGE0__CSR:
		return "TB5_PAGE0";
	case TB5_PAGE1__CSR:
		return "TB5_PAGE1";
	case TB5_PAGE2__CSR:
		return "TB5_PAGE2";
	case TB5_PAGE3__CSR:
		return "TB5_PAGE3";
	case TB6_PAGE0__CSR:
		return "TB6_PAGE0";
	case TB6_PAGE1__CSR:
		return "TB6_PAGE1";
	case TB6_PAGE2__CSR:
		return "TB6_PAGE2";
	case TB6_PAGE3__CSR:
		return "TB6_PAGE3";
	case TB7_PAGE0__CSR:
		return "TB7_PAGE0";
	case TB7_PAGE1__CSR:
		return "TB7_PAGE1";
	case TB7_PAGE2__CSR:
		return "TB7_PAGE2";
	case TB7_PAGE3__CSR:
		return "TB7_PAGE3";

	// Miscellaneous Registers [21174 TRM, Section 4.7]
	case CCR__CSR:
		return "CCR";
	case CLK_STAT__CSR:
		return "CLK_STAT";
	case RESET__CSR:	// technically, register is WO  [TRM, Section 5.8.3]
		return "RESET";

	// Interrupt Control Registers [21174 TRM, Section 4.8]
	case INT_REQ__CSR:
		return "INT_REQ";
	case INT_MASK__CSR:
		return "INT_MASK";
	case INT_HILO__CSR:
		return "INT_HILO";
	case INT_ROUTE__CSR:
		return "INT_ROUTE";
	case GPO__CSR:
		return "GPO";
	case INT_CNFG__CSR:
		return "INT_CNFG";
    case RT_COUNT__CSR:
		return "RT_COUNT";
	case INT_TIME__CSR:
		return "INT_TIME";
	case IIC_CTRL__CSR:
		return "IIC_CTRL";

    default:
        return "--UNKNOWN--";
	} // switch (pa)
}

t_uint64 pyxis_read_csr (t_uint64 pa, uint32 lnth)
{
    t_uint64 value;

	switch (pa) {

	// General Registers [21174 TRM, Section 4.3]
	case PYXIS_REV__CSR:
		value = pyxis_reg.pyxis_rev;
        break;
	case PCI_LAT__CSR:
		value = pyxis_reg.pci_lat;
        break;
	case PYXIS_CTRL__CSR:
		value = pyxis_reg.pyxis_ctrl;
        break;
	case PYXIS_CTRL1__CSR:
		value = pyxis_reg.pyxis_ctrl1;
        break;
	case FLASH_CTRL__CSR:
		value = pyxis_reg.flash_ctrl;
        break;
	case HAE_MEM__CSR:
		value = pyxis_reg.hae_mem;
        break;
	case HAE_IO__CSR:
		value = pyxis_reg.hae_io;
        break;
	case CFG__CSR:
		value = pyxis_reg.cfg;
        break;
	case PYXIS_DIAG__CSR:
		value = pyxis_reg.pyxis_diag;
        break;
	case DIAG_CHECK__CSR:
		value = pyxis_reg.diag_check;
        break;
	case PERF_MONITOR__CSR:
		value = pyxis_reg.perf_monitor;
        break;
	case PERF_CONTROL__CSR:
		value = pyxis_reg.perf_control;
        break;
	case PYXIS_ERR__CSR:
		value = pyxis_reg.pyxis_err;
        break;
	case PYXIS_STAT__CSR:
		value = pyxis_reg.pyxis_stat;
        break;
	case ERR_MASK__CSR:
		value = pyxis_reg.err_mask;
        break;
	case PYXIS_SYN__CSR:
		value = pyxis_reg.pyxis_syn;
        break;
	case PYXIS_ERR_DATA__CSR:
		value = pyxis_reg.pyxis_err_data;
        break;
	case MEAR__CSR:
		value = pyxis_reg.mear;
        break;
	case MESR__CSR:
		value = pyxis_reg.mesr;
        break;
	case PCI_ERR0__CSR:
		value = pyxis_reg.pci_err0;
        break;
	case PCI_ERR1__CSR:
		value = pyxis_reg.pci_err1;
        break;
	case PCI_ERR2__CSR:
		value = pyxis_reg.pci_err2;
        break;

	// Memory Controller Registers [21174 TRM, Section 4.4]
	case MCR__CSR:
		value = pyxis_reg.mcr;
        break;
	case MCMR__CSR:
		value = pyxis_reg.mcmr;
        break;
	case GTR__CSR:
		value = pyxis_reg.gtr;
        break;
	case RTR__CSR:
		value = pyxis_reg.rtr;
        break;
	case RHPR__CSR:
		value = pyxis_reg.rhpr;
        break;
	case MDR1__CSR:
		value = pyxis_reg.mdr1;
        break;
	case MDR2__CSR:
		value = pyxis_reg.mdr2;
        break;
	case BBAR0__CSR:
		value = pyxis_reg.bbar0;
        break;
	case BBAR1__CSR:
		value = pyxis_reg.bbar1;
        break;
	case BBAR2__CSR:
		value = pyxis_reg.bbar2;
        break;
	case BBAR3__CSR:
		value = pyxis_reg.bbar3;
        break;
	case BBAR4__CSR:
		value = pyxis_reg.bbar4;
        break;
	case BBAR5__CSR:
		value = pyxis_reg.bbar5;
        break;
	case BBAR6__CSR:
		value = pyxis_reg.bbar6;
        break;
	case BBAR7__CSR:
		value = pyxis_reg.bbar7;
        break;
	case BCR0__CSR:
		value = pyxis_reg.bcr0;
        break;
	case BCR1__CSR:
		value = pyxis_reg.bcr1;
        break;
	case BCR2__CSR:
		value = pyxis_reg.bcr2;
        break;
	case BCR3__CSR:
		value = pyxis_reg.bcr3;
        break;
	case BCR4__CSR:
		value = pyxis_reg.bcr4;
        break;
	case BCR5__CSR:
		value = pyxis_reg.bcr5;
        break;
	case BCR6__CSR:
		value = pyxis_reg.bcr6;
        break;
	case BCR7__CSR:
		value = pyxis_reg.bcr7;
        break;
	case BTR0__CSR:
		value = pyxis_reg.btr0;
        break;
	case BTR1__CSR:
		value = pyxis_reg.btr1;
        break;
	case BTR2__CSR:
		value = pyxis_reg.btr2;
        break;
	case BTR3__CSR:
		value = pyxis_reg.btr3;
        break;
	case BTR4__CSR:
		value = pyxis_reg.btr4;
        break;
	case BTR5__CSR:
		value = pyxis_reg.btr5;
        break;
	case BTR6__CSR:
		value = pyxis_reg.btr6;
        break;
	case BTR7__CSR:
		value = pyxis_reg.btr7;
        break;
	case CVM__CSR:
		value = pyxis_reg.cvm;
        break;

	// PCI Window Control Registers [21174 TRM, Section 4.5]
	case TBIA__CSR:
		value = pyxis_reg.tbia;
        break;
	case W0_BASE__CSR:
		value = pyxis_reg.w0_base;
        break;
	case W0_MASK__CSR:
		value = pyxis_reg.w0_mask;
        break;
	case T0_BASE__CSR:
		value = pyxis_reg.t0_base;
        break;
	case W1_BASE__CSR:
		value = pyxis_reg.w1_base;
        break;
	case W1_MASK__CSR:
		value = pyxis_reg.w1_mask;
        break;
	case T1_BASE__CSR:
		value = pyxis_reg.t1_base;
        break;
	case W2_BASE__CSR:
		value = pyxis_reg.w2_base;
        break;
	case W2_MASK__CSR:
		value = pyxis_reg.w2_mask;
        break;
	case T2_BASE__CSR:
		value = pyxis_reg.t2_base;
        break;
	case W3_BASE__CSR:
		value = pyxis_reg.w3_base;
        break;
	case W3_MASK__CSR:
		value = pyxis_reg.w3_mask;
        break;
	case T3_BASE__CSR:
		value = pyxis_reg.t3_base;
        break;
	case W_DAC__CSR:
		value = pyxis_reg.w_dac;
        break;

	// Scatter-Gather Address Translation Registers [21174 TRM, Section 4.6]
	case LTB_TAG0__CSR:
		value = pyxis_reg.ltb_tag0;
        break;
	case LTB_TAG1__CSR:
		value = pyxis_reg.ltb_tag1;
        break;
	case LTB_TAG2__CSR:
		value = pyxis_reg.ltb_tag2;
        break;
	case LTB_TAG3__CSR:
		value = pyxis_reg.ltb_tag3;
        break;
	case TB_TAG4__CSR:
		value = pyxis_reg.tb_tag4;
        break;
	case TB_TAG5__CSR:
		value = pyxis_reg.tb_tag5;
        break;
	case TB_TAG6__CSR:
		value = pyxis_reg.tb_tag6;
        break;
	case TB_TAG7__CSR:
		value = pyxis_reg.tb_tag7;
        break;
	case TB0_PAGE0__CSR:
		value = pyxis_reg.tb0_page0;
        break;
	case TB0_PAGE1__CSR:
		value = pyxis_reg.tb0_page1;
        break;
	case TB0_PAGE2__CSR:
		value = pyxis_reg.tb0_page2;
        break;
	case TB0_PAGE3__CSR:
		value = pyxis_reg.tb0_page3;
        break;
	case TB1_PAGE0__CSR:
		value = pyxis_reg.tb1_page0;
        break;
	case TB1_PAGE1__CSR:
		value = pyxis_reg.tb1_page1;
        break;
	case TB1_PAGE2__CSR:
		value = pyxis_reg.tb1_page2;
        break;
	case TB1_PAGE3__CSR:
		value = pyxis_reg.tb1_page3;
        break;
	case TB2_PAGE0__CSR:
		value = pyxis_reg.tb2_page0;
        break;
	case TB2_PAGE1__CSR:
		value = pyxis_reg.tb2_page1;
        break;
	case TB2_PAGE2__CSR:
		value = pyxis_reg.tb2_page2;
        break;
	case TB2_PAGE3__CSR:
		value = pyxis_reg.tb2_page3;
        break;
	case TB3_PAGE0__CSR:
		value = pyxis_reg.tb3_page0;
        break;
	case TB3_PAGE1__CSR:
		value = pyxis_reg.tb3_page1;
        break;
	case TB3_PAGE2__CSR:
		value = pyxis_reg.tb3_page2;
        break;
	case TB3_PAGE3__CSR:
		value = pyxis_reg.tb3_page3;
        break;
	case TB4_PAGE0__CSR:
		value = pyxis_reg.tb4_page0;
        break;
	case TB4_PAGE1__CSR:
		value = pyxis_reg.tb4_page1;
        break;
	case TB4_PAGE2__CSR:
		value = pyxis_reg.tb4_page2;
        break;
	case TB4_PAGE3__CSR:
		value = pyxis_reg.tb4_page3;
        break;
	case TB5_PAGE0__CSR:
		value = pyxis_reg.tb5_page0;
        break;
	case TB5_PAGE1__CSR:
		value = pyxis_reg.tb5_page1;
        break;
	case TB5_PAGE2__CSR:
		value = pyxis_reg.tb5_page2;
        break;
	case TB5_PAGE3__CSR:
		value = pyxis_reg.tb5_page3;
        break;
	case TB6_PAGE0__CSR:
		value = pyxis_reg.tb6_page0;
        break;
	case TB6_PAGE1__CSR:
		value = pyxis_reg.tb6_page1;
        break;
	case TB6_PAGE2__CSR:
		value = pyxis_reg.tb6_page2;
        break;
	case TB6_PAGE3__CSR:
		value = pyxis_reg.tb6_page3;
        break;
	case TB7_PAGE0__CSR:
		value = pyxis_reg.tb7_page0;
        break;
	case TB7_PAGE1__CSR:
		value = pyxis_reg.tb7_page1;
        break;
	case TB7_PAGE2__CSR:
		value = pyxis_reg.tb7_page2;
        break;
	case TB7_PAGE3__CSR:
		value = pyxis_reg.tb7_page3;
        break;

	// Miscellaneous Registers [21174 TRM, Section 4.7]
	case CCR__CSR:
		value = pyxis_reg.ccr;
        break;
	case CLK_STAT__CSR:
		value = pyxis_reg.clk_stat;
        break;
	case RESET__CSR:	// technically, register is WO  [TRM, Section 5.8.3]
		value = pyxis_reg.reset;
        break;

	// Interrupt Control Registers [21174 TRM, Section 4.8]
	case INT_REQ__CSR:
        // Miata C01 system identification, from Draft MiataGL System Specification, Table 2-1
        pyxis_reg.int_req |= 0xFF00000030u; //GPI<39:32> = 0xFF, GPI<5:4> = 0x3
		value = pyxis_reg.int_req;
        break;
	case INT_MASK__CSR:
		value = pyxis_reg.int_mask;
        break;
	case INT_HILO__CSR:
		value = pyxis_reg.int_hilo;
        break;
	case INT_ROUTE__CSR:
		value = pyxis_reg.int_route;
        break;
	case GPO__CSR:
		value = pyxis_reg.gpo;
        break;
	case INT_CNFG__CSR:
		value = pyxis_reg.int_cnfg;
        break;
	case RT_COUNT__CSR:
		value = pyxis_reg.rt_count;
        break;
	case INT_TIME__CSR:
		value = pyxis_reg.int_time;
        break;
	case IIC_CTRL__CSR:
		value = pyxis_reg.iic_ctrl;
        break;

    default:
        value = 0;
        break;
	} // switch (pa)

    sim_debug (DBG_IO, &pyxis_dev,"pyxis_reg_read: @PC(%llx), pa(%llx)[%s], lnth(%d), value(0x%llx)\n",
        PC-4, pa, pyxis_regid(pa), lnth, value);
	return value;
} // pyxis_read_csr (pa)

t_stat pyxis_write_csr (t_uint64 pa, t_uint64 value, uint32 lnth)
{
    sim_debug (DBG_IO, &pyxis_dev,"pyxis_reg_write: @PC(%llx), pa(%llx)[%s], lnth(%d), value(0x%llx)\n",
        PC-4, pa, pyxis_regid(pa), lnth,value);
	switch (pa) {

	// General Registers [ref: TRM, 5.1]
	case PYXIS_REV__CSR:		// [ref: TRM, 5.1.1]
		break;					// Register is RO
	case PCI_LAT__CSR:			// [ref: TRM, 5.1.2]
		pyxis_reg.pci_lat = value & ~PCI_LAT__MBZ;
		break;
	case PYXIS_CTRL__CSR:		// [ref: TRM, 5.1.3]
		pyxis_reg.pyxis_ctrl = value & ~PYXIS_CTRL__MBZ;
		break;
	case PYXIS_CTRL1__CSR:		// [ref: TRM, 5.1.4]
		pyxis_reg.pyxis_ctrl1 = value & ~PYXIS_CTRL1__MBZ;
		break;
	case FLASH_CTRL__CSR:		// [ref: TRM, 5.1.5]
		pyxis_reg.flash_ctrl = value & ~FLASH_CTRL__MBZ;
		break;
	case HAE_MEM__CSR:			// [ref: TRM, 5.1.6]
		pyxis_reg.hae_mem = value & ~HAE_MEM__MBZ;
		break;
	case HAE_IO__CSR:			// [ref: TRM, 5.1.7]
		pyxis_reg.hae_io = value & ~HAE_IO__MBZ;
		break;
	case CFG__CSR:				// [ref: TRM, 5.1.8]
		pyxis_reg.cfg = value & ~CFG__MBZ;
		break;

	// Diagnostic Registers [ref: TRM, 5.2]
	case PYXIS_DIAG__CSR:		// [ref: TRM, 5.2.1]
		pyxis_reg.pyxis_diag = value & ~PYXIS_DIAG__MBZ;
		break;
	case DIAG_CHECK__CSR:		// [ref: TRM, 5.2.2]
		pyxis_reg.diag_check = value & ~DIAG_CHECK__MBZ;
		break;

	// Performance Monitor Registers [ref: TRM, 5.3]
	case PERF_MONITOR__CSR:		// [ref: TRM, 5.3.1]
		break;					// Register is RO
	case PERF_CONTROL__CSR:		// [ref: TRM, 5.3.2]
		pyxis_reg.perf_control = value & ~PERF_CONTROL__MBZ;
		// Clear perf_monitor.low_count?
		if ((pyxis_reg.perf_control & PERF_CONTROL__LOW_COUNT_CLR) != 0) {
			pyxis_reg.perf_monitor &= ~PERF_MONITOR__LOW_COUNT;
			pyxis_reg.perf_control &= ~PERF_CONTROL__LOW_COUNT_CLR;
		}
		// Clear perf_monitor.high_count?
		if ((pyxis_reg.perf_control & PERF_CONTROL__HIGH_COUNT_CLR) != 0) {
			pyxis_reg.perf_monitor &= ~PERF_MONITOR__HIGH_COUNT;
			pyxis_reg.perf_control &= ~PERF_CONTROL__HIGH_COUNT_CLR;
		}
		break;

	// Error Registers [ref:TRM, 5.4]
	case PYXIS_ERR__CSR:		// [ref: TRM, 5.4.1]
		pyxis_reg.pyxis_err &= ~(pyxis_reg.pyxis_err & value & PYXIS_ERR__W1C); // Clear W1C bits
		break;
	case PYXIS_STAT__CSR:		// [ref: TRM, 5.4.2]
		break;					// Register is RO
	case ERR_MASK__CSR:			// [ref: TRM, 5.4.3]
		pyxis_reg.err_mask = value & ~ERR_MASK__MBZ;
		break;
	case PYXIS_SYN__CSR:		// [ref: TRM, 5.4.4]
		break;					// Register is RO
	case PYXIS_ERR_DATA__CSR:	// [ref: TRM, 5.4.5]
		break;					// Register is RO
	case MEAR__CSR:				// [ref: TRM, 5.4.6]
		break;					// Register is RO
	case MESR__CSR:				// [ref: TRM, 5.4.7]
		pyxis_reg.mesr = (value & MESR__RW) | (pyxis_reg.mesr & ~MESR__RW); // Set RW bits
		break;
	case PCI_ERR0__CSR:			// [ref: TRM, 5.4.8]
		break;					// Register is RO
	case PCI_ERR1__CSR:			// [ref: TRM, 5.4.9]
		break;					// Register is RO
	case PCI_ERR2__CSR:			// [ref: TRM, 5.4.10]
		break;					// Register is RO

	// Memory Controller Registers [ref: TRM, 5.5]
	case MCR__CSR:				// [ref: TRM, 5.5.1]
		pyxis_reg.mcr = (value & MCR__RW) | (pyxis_reg.mcr & ~MCR__RW);	// Set RW bits
		break;
	case MCMR__CSR:				// [ref: TRM, 5.5.2]
		pyxis_reg.mcmr = value & ~MCMR__MBZ;
		break;
	case GTR__CSR:				// [ref: TRM, 5.5.3]
		pyxis_reg.gtr = value & ~GTR__MBZ;
		break;
	case RTR__CSR:				// [ref: TRM, 5.5.4]
		pyxis_reg.rtr = value & ~RTR__MBZ;
		break;
	case RHPR__CSR:				// [ref: TRM, 5.5.5]
		pyxis_reg.rhpr = value & ~RHPR__MBZ;
		break;
	case MDR1__CSR:				// [ref: TRM, 5.5.6]
		pyxis_reg.mdr1 = value & ~MDR1__MBZ;
		break;
	case MDR2__CSR:				// [ref: TRM, 5.5.7]
		pyxis_reg.mdr2 = value & ~MDR2__MBZ;
		break;
	case BBAR0__CSR:			// [ref: TRM, 5.5.8]
		pyxis_reg.bbar0 = value & ~BBAR__MBZ;
		break;
	case BBAR1__CSR:			// [ref: TRM, 5.5.8]
		pyxis_reg.bbar1 = value & ~BBAR__MBZ;
		break;
	case BBAR2__CSR:			// [ref: TRM, 5.5.8]
		pyxis_reg.bbar2 = value & ~BBAR__MBZ;
		break;
	case BBAR3__CSR:			// [ref: TRM, 5.5.8]
		pyxis_reg.bbar3 = value & ~BBAR__MBZ;
		break;
	case BBAR4__CSR:			// [ref: TRM, 5.5.8]
		pyxis_reg.bbar4 = value & ~BBAR__MBZ;
		break;
	case BBAR5__CSR:			// [ref: TRM, 5.5.8]
		pyxis_reg.bbar5 = value & ~BBAR__MBZ;
		break;
	case BBAR6__CSR:			// [ref: TRM, 5.5.8]
		pyxis_reg.bbar6 = value & ~BBAR__MBZ;
		break;
	case BBAR7__CSR:			// [ref: TRM, 5.5.8]
		pyxis_reg.bbar7 = value & ~BBAR__MBZ;
		break;
	case BCR0__CSR:				// [ref: TRM, 5.5.9]
		pyxis_reg.bcr0 = value & ~BCR__MBZ;
		break;
	case BCR1__CSR:				// [ref: TRM, 5.5.9]
		pyxis_reg.bcr1 = value & ~BCR__MBZ;
		break;
	case BCR2__CSR:				// [ref: TRM, 5.5.9]
		pyxis_reg.bcr2 = value & ~BCR__MBZ;
		break;
	case BCR3__CSR:				// [ref: TRM, 5.5.9]
		pyxis_reg.bcr3 = value & ~BCR__MBZ;
		break;
	case BCR4__CSR:				// [ref: TRM, 5.5.9]
		pyxis_reg.bcr4 = value & ~BCR__MBZ;
		break;
	case BCR5__CSR:				// [ref: TRM, 5.5.9]
		pyxis_reg.bcr5 = value & ~BCR__MBZ;
		break;
	case BCR6__CSR:				// [ref: TRM, 5.5.9]
		pyxis_reg.bcr6 = value & ~BCR__MBZ;
		break;
	case BCR7__CSR:				// [ref: TRM, 5.5.9]
		pyxis_reg.bcr7 = value & ~BCR__MBZ;
		break;
	case BTR0__CSR:				// [ref: TRM, 5.5.10]
		pyxis_reg.btr0 = value & ~BTR__MBZ;
		break;
	case BTR1__CSR:				// [ref: TRM, 5.5.10]
		pyxis_reg.btr1 = value & ~BTR__MBZ;
		break;
	case BTR2__CSR:				// [ref: TRM, 5.5.10]
		pyxis_reg.btr2 = value & ~BTR__MBZ;
		break;
	case BTR3__CSR:				// [ref: TRM, 5.5.10]
		pyxis_reg.btr3 = value & ~BTR__MBZ;
		break;
	case BTR4__CSR:				// [ref: TRM, 5.5.10]
		pyxis_reg.btr4 = value & ~BTR__MBZ;
		break;
	case BTR5__CSR:				// [ref: TRM, 5.5.10]
		pyxis_reg.btr5 = value & ~BTR__MBZ;
		break;
	case BTR6__CSR:				// [ref: TRM, 5.5.10]
		pyxis_reg.btr6 = value & ~BTR__MBZ;
		break;
	case BTR7__CSR:				// [ref: TRM, 5.5.10]
		pyxis_reg.btr7 = value & ~BTR__MBZ;
		break;
	case CVM__CSR:				// [ref: TRM, 5.5.11]
		pyxis_reg.cvm &= ~(pyxis_reg.cvm & value & CVM__CACHE_VALID_MAP);	// Set W1C bits
		break;

	// PCI Window Control Registers [ref: TRM, 5.6]
	case TBIA__CSR:				// [ref: TRM, 5.6.1]
		// This is not really a register write, it is an action command.
		switch (value & TBIA__TBIA)
		{
		case 0:		// no operation
			break;
		case 1:		// invalidate/unlock locked tags
			if ((pyxis_reg.ltb_tag0 & LTB_TAGX__LOCKED) != 0) {
				pyxis_reg.ltb_tag0 &= ~(LTB_TAGX__LOCKED | LTB_TAGX__VALID);
			}
			if ((pyxis_reg.ltb_tag1 & LTB_TAGX__LOCKED) != 0) {
				pyxis_reg.ltb_tag1 &= ~(LTB_TAGX__LOCKED | LTB_TAGX__VALID);
			}
			if ((pyxis_reg.ltb_tag2 & LTB_TAGX__LOCKED) != 0) {
				pyxis_reg.ltb_tag2 &= ~(LTB_TAGX__LOCKED | LTB_TAGX__VALID);
			}
			if ((pyxis_reg.ltb_tag3 & LTB_TAGX__LOCKED) != 0) {
				pyxis_reg.ltb_tag3 &= ~(LTB_TAGX__LOCKED | LTB_TAGX__VALID);
			}
			break;
		case 2:		// invalidate unlocked tags
			// Invalidate unlocked lockable tags
			if ((pyxis_reg.ltb_tag0 & LTB_TAGX__LOCKED) == 0) {
				pyxis_reg.ltb_tag0 &= ~LTB_TAGX__VALID;
			}
			if ((pyxis_reg.ltb_tag1 & LTB_TAGX__LOCKED) == 0) {
				pyxis_reg.ltb_tag1 &= ~LTB_TAGX__VALID;
			}
			if ((pyxis_reg.ltb_tag2 & LTB_TAGX__LOCKED) == 0) {
				pyxis_reg.ltb_tag2 &= ~LTB_TAGX__VALID;
			}
			if ((pyxis_reg.ltb_tag3 & LTB_TAGX__LOCKED) == 0) {
				pyxis_reg.ltb_tag3 &= ~LTB_TAGX__VALID;
			}
			// Invalidate non-lockable tags ?? should this be done ??
			pyxis_reg.tb_tag4 &= ~TB_TAGX__VALID;
			pyxis_reg.tb_tag5 &= ~TB_TAGX__VALID;
			pyxis_reg.tb_tag6 &= ~TB_TAGX__VALID;
			pyxis_reg.tb_tag7 &= ~TB_TAGX__VALID;
			break;
		case 3:		// invalidate and unlock all tags
			// Invalidate/unlock lockable tags
			pyxis_reg.ltb_tag0 &= ~(LTB_TAGX__LOCKED | LTB_TAGX__VALID);
			pyxis_reg.ltb_tag1 &= ~(LTB_TAGX__LOCKED | LTB_TAGX__VALID);
			pyxis_reg.ltb_tag2 &= ~(LTB_TAGX__LOCKED | LTB_TAGX__VALID);
			pyxis_reg.ltb_tag3 &= ~(LTB_TAGX__LOCKED | LTB_TAGX__VALID);
			// Invalidate non-lockable tags  ?? should this be done ??
			pyxis_reg.tb_tag4 &= ~(TB_TAGX__VALID);
			pyxis_reg.tb_tag5 &= ~(TB_TAGX__VALID);
			pyxis_reg.tb_tag6 &= ~(TB_TAGX__VALID);
			pyxis_reg.tb_tag7 &= ~(TB_TAGX__VALID);
			break;
		}; // switch (value & TBIA__TBIA)
		break;
	case W0_BASE__CSR:	// [ref: TRM, 5.6.2]
		pyxis_reg.w0_base = value & ~WX_BASE__MBZ;
		break;
	case W0_MASK__CSR:	// [ref: TRM, 5.6.3]
		pyxis_reg.w0_mask = value & ~WX_MASK__MBZ;
		break;
	case T0_BASE__CSR:	// [ref: TRM, 5.6.4]
		pyxis_reg.t0_base = value & ~TX_BASE__MBZ;
		break;
	case W1_BASE__CSR:	// [ref: TRM, 5.6.2]
		pyxis_reg.w1_base = value & ~WX_BASE__MBZ;
		break;
	case W1_MASK__CSR:	// [ref: TRM, 5.6.3]
		pyxis_reg.w1_mask = value & ~WX_MASK__MBZ;
		break;
	case T1_BASE__CSR:	// [ref: TRM, 5.6.4]
		pyxis_reg.t1_base = value & ~TX_BASE__MBZ;
		break;
	case W2_BASE__CSR:	// [ref: TRM, 5.6.2]
		pyxis_reg.w2_base = value & ~WX_BASE__MBZ;
		break;
	case W2_MASK__CSR:	// [ref: TRM, 5.6.3]
		pyxis_reg.w2_mask = value & ~WX_MASK__MBZ;
		break;
	case T2_BASE__CSR:	// [ref: TRM, 5.6.4]
		pyxis_reg.t2_base = value & ~TX_BASE__MBZ;
		break;
	case W3_BASE__CSR:	// [ref: TRM, 5.6.2]
		pyxis_reg.w3_base = value & ~WX_BASE__MBZ;
		break;
	case W3_MASK__CSR:	// [ref: TRM, 5.6.3]
		pyxis_reg.w3_mask = value & ~WX_MASK__MBZ;
		break;
	case T3_BASE__CSR:	// [ref: TRM, 5.6.4]
		pyxis_reg.t3_base = value & ~TX_BASE__MBZ;
		break;
	case W_DAC__CSR:
		pyxis_reg.w_dac = value & ~W_DAC__MBZ;

	// Scatter-Gather Address Translation Registers [ref: TRM, 5.7]
	case LTB_TAG0__CSR:	// [ref: TRM, 5.7.1]
		pyxis_reg.ltb_tag0 = value & ~LTB_TAGX__MBZ;
		break;
	case LTB_TAG1__CSR:	// [ref: TRM, 5.7.1]
		pyxis_reg.ltb_tag1 = value & ~LTB_TAGX__MBZ;
		break;
	case LTB_TAG2__CSR:	// [ref: TRM, 5.7.1]
		pyxis_reg.ltb_tag2 = value & ~LTB_TAGX__MBZ;
		break;
	case LTB_TAG3__CSR:	// [ref: TRM, 5.7.1]
		pyxis_reg.ltb_tag3 = value & ~LTB_TAGX__MBZ;
		break;
	case TB_TAG4__CSR:	// [ref: TRM, 5.7.2]
		return pyxis_reg.tb_tag4 = value & ~TB_TAGX__MBZ;
		break;
	case TB_TAG5__CSR:	// [ref: TRM, 5.7.2]
		return pyxis_reg.tb_tag5 = value & ~TB_TAGX__MBZ;
		break;
	case TB_TAG6__CSR:	// [ref: TRM, 5.7.2]
		return pyxis_reg.tb_tag6 = value & ~TB_TAGX__MBZ;
		break;
	case TB_TAG7__CSR:	// [ref: TRM, 5.7.2]
		return pyxis_reg.tb_tag7 = value & ~TB_TAGX__MBZ;
		break;
	case TB0_PAGE0__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb0_page0 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB0_PAGE1__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb0_page1 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB0_PAGE2__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb0_page2 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB0_PAGE3__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb0_page3 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB1_PAGE0__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb1_page0 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB1_PAGE1__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb1_page1 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB1_PAGE2__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb1_page2 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB1_PAGE3__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb1_page3 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB2_PAGE0__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb2_page0 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB2_PAGE1__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb2_page1 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB2_PAGE2__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb2_page2 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB2_PAGE3__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb2_page3 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB3_PAGE0__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb3_page0 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB3_PAGE1__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb3_page1 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB3_PAGE2__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb3_page2 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB3_PAGE3__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb3_page3 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB4_PAGE0__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb4_page0 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB4_PAGE1__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb4_page1 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB4_PAGE2__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb4_page2 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB4_PAGE3__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb4_page3 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB5_PAGE0__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb5_page0 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB5_PAGE1__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb5_page1 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB5_PAGE2__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb5_page2 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB5_PAGE3__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb5_page3 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB6_PAGE0__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb6_page0 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB6_PAGE1__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb6_page1 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB6_PAGE2__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb6_page2 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB6_PAGE3__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb6_page3 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB7_PAGE0__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb7_page0 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB7_PAGE1__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb7_page1 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB7_PAGE2__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb7_page2 = value & ~TBM_PAGEN__MBZ;
		break;
	case TB7_PAGE3__CSR:	// [ref: TRM, 5.7.3]
		pyxis_reg.tb7_page3 = value & ~TBM_PAGEN__MBZ;
		break;

	// Miscellaneous Registers [ref: TRM, 5.8]
	case CCR__CSR:			// [ref: TRM, 5.8.1]
		pyxis_reg.ccr = value & ~CCR__MBZ;
		break;
	case CLK_STAT__CSR:		// [ref: TRM, 5.8.2]
		break;				// register is RO
	case RESET__CSR:		// [ref: TRM, 5.8.3]
		if (value == RESET__MAGIC_VALUE) {
			pci_bus_reset (&pyxis_pci64);	// reset PCI bus & devices
			//pyxis_reset ();			// reset pyxis to power-on settings
		}
		break;

	// Interrupt Control Registers [ref: TRM, 5.9]
	case INT_REQ__CSR:	// [ref: TRM, 5.9.1]
		pyxis_reg.int_req &= ~(pyxis_reg.int_req & value & INT_REQ__W1C);	// Set W1C bits
		break;
	case INT_MASK__CSR:	// [ref: TRM, 5.9.2]
		pyxis_reg.int_mask = value & ~INT_MASK__MBZ;
		break;
	case INT_HILO__CSR:	// [ref: TRM, 5.9.3]
		pyxis_reg.int_hilo = value & ~INT_HILO__MBZ;
		break;
	case INT_ROUTE__CSR:// [ref: TRM, 5.9.4]
		pyxis_reg.int_route = value & ~INT_ROUTE__MBZ;
		break;
	case GPO__CSR:		// [ref: TRM, 5.9.5]
        //printf("pyxis: GPO write, value[%x]\n", (int)value);
		pyxis_reg.gpo = value & ~GPO__MBZ;
		break;
	case INT_CNFG__CSR:	// [ref: TRM, 5.9.6]
		pyxis_reg.int_cnfg = value & ~INT_CNFG__MBZ;
		break;
	case RT_COUNT__CSR:	// [ref: TRM, 5.9.7]
		pyxis_reg.rt_count = value & ~RT_COUNT__MBZ;
		break;
	case INT_TIME__CSR:	// [ref: TRM, 5.9.8]
		pyxis_reg.int_time = value & ~INT_TIME__MBZ;
		break;
	case IIC_CTRL__CSR:	// [ref: TRM, 5.9.9]
		break;			// Not currently implementing I2C register;
						// will require skipping this register's read/write
						// cycles to size memory modules.

	default:			// this would be where a non-existant memory violation would go
		return SCPE_NXREG;
		break;
	} // switch (pa)
	return SCPE_OK;
} // pyxis_write_csr (pa, value)

void pyxis_size_bank (t_uint64* unsized, t_uint64* sized, uint32* bbar, uint32* bcr)
{
#define MEM_512MB 0x2000000ull
#define MEM_256MB 0x1000000ull
#define MEM_128MB 0x0800000ull
#define MEM_64MB  0x0400000ull
#define MEM_32MB  0x0200000ull
#define MEM_16MB  0x0100000ull
#define MEM_8MB	  0x0080000ull
	// Fill banks from largest to smallest sticks, which is the recommended hardware method.
	// NOTE: The real Alpha PWS 500au only supported 3 banks of up to 512MB each.
	// This sizing routine will support 8 banks of 512MB each, for a total of 4GB.
	if (*unsized !=0) {
		if (*unsized >= MEM_512MB) {
			*bcr  = BCR__BANK_SIZE_512MB | BCR__BANK_ENABLE;	// bank size
			*bbar = (*sized >> 18) & BBAR__BASEADDR;			// bank offset
			*sized   += MEM_512MB;
			*unsized -= MEM_512MB;
			return;
		}
		if (*unsized >= MEM_256MB) {
			*bcr  = BCR__BANK_SIZE_256MB | BCR__BANK_ENABLE;	// bank size
			*bbar = (*sized >> 18) & BBAR__BASEADDR;			// bank offset
			*sized   += MEM_256MB;
			*unsized -= MEM_256MB;
			return;
		}
		if (*unsized >= MEM_128MB) {
			*bcr  = BCR__BANK_SIZE_128MB | BCR__BANK_ENABLE;	// bank size
			*bbar = (*sized >> 18) & BBAR__BASEADDR;			// bank offset
			*sized   += MEM_128MB;
			*unsized -= MEM_128MB;
			return;
		}
		if (*unsized >= MEM_64MB) {
			*bcr  = BCR__BANK_SIZE_64MB | BCR__BANK_ENABLE;		// bank size
			*bbar = (*sized >> 18) & BBAR__BASEADDR;			// bank offset
			*sized   += MEM_64MB;
			*unsized -= MEM_64MB;
			return;
		}
		if (*unsized >= MEM_32MB) {
			*bcr  = BCR__BANK_SIZE_32MB | BCR__BANK_ENABLE;		// bank size
			*bbar = (*sized >> 18) & BBAR__BASEADDR;			// bank offset
			*sized   += MEM_32MB;
			*unsized -= MEM_32MB;
			return;
		}
		if (*unsized >= MEM_16MB) {
			*bcr  = BCR__BANK_SIZE_16MB | BCR__BANK_ENABLE;		// bank size
			*bbar = (*sized >> 18) & BBAR__BASEADDR;			// bank offset
			*sized   += MEM_16MB;
			*unsized -= MEM_16MB;
			return;
		}
		if (*unsized >= MEM_8MB) {
			*bcr  = BCR__BANK_SIZE_8MB | BCR__BANK_ENABLE;		// bank size
			*bbar = (*sized >> 18) & BBAR__BASEADDR;			// bank offset
			*sized   += MEM_8MB;
			*unsized -= MEM_8MB;
			return;
		}
	}
}

t_stat pyxis_reset (DEVICE* dev)
{
// initialize all registers
	// General Registers [21174 TRM, Section 4.3]
	pyxis_reg.pyxis_rev		= PYXIS_REV__INIT;
	pyxis_reg.pci_lat		= PCI_LAT__INIT;
	pyxis_reg.pyxis_ctrl	= PYXIS_CTRL__INIT;
	pyxis_reg.pyxis_ctrl1	= PYXIS_CTRL1__INIT;
	pyxis_reg.flash_ctrl	= FLASH_CTRL__INIT;
	pyxis_reg.hae_mem		= HAE_MEM__INIT;
	pyxis_reg.hae_io		= HAE_IO__INIT;
	pyxis_reg.cfg			= CFG__INIT;
	pyxis_reg.pyxis_diag	= PYXIS_DIAG__INIT;
	pyxis_reg.diag_check	= DIAG_CHECK__INIT;
	pyxis_reg.perf_monitor  = PERF_MONITOR__INIT;
	pyxis_reg.perf_control	= PERF_CONTROL__INIT;
	pyxis_reg.pyxis_err		= PYXIS_ERR__INIT;
	pyxis_reg.pyxis_stat	= PYXIS_STAT__INIT;
	pyxis_reg.err_mask		= ERR_MASK__INIT;
	pyxis_reg.pyxis_syn		= PYXIS_SYN__INIT;
	pyxis_reg.pyxis_err_data= PYXIS_ERR__INIT;
	pyxis_reg.mear			= MEAR__INIT;
	pyxis_reg.mesr			= MESR__INIT;
	pyxis_reg.pci_err0		= PCI_ERR0__INIT;
	pyxis_reg.pci_err1		= PCI_ERR1__INIT;
	pyxis_reg.pci_err2		= PCI_ERR2__INIT;

	// Memory Controller Registers [21174 TRM, Section 4.4]
	pyxis_reg.mcr			= MCR__INIT;
	pyxis_reg.mcmr			= MCMR__INIT;
	pyxis_reg.gtr			= GTR__INIT;
	pyxis_reg.rtr			= RTR__INIT;
	pyxis_reg.rhpr			= RHPR__INIT;
	pyxis_reg.mdr1			= MDR1__INIT;
	pyxis_reg.mdr2			= MDR2__INIT;
	pyxis_reg.bbar0			= BBAR__INIT;
	pyxis_reg.bbar1			= BBAR__INIT;
	pyxis_reg.bbar2			= BBAR__INIT;
	pyxis_reg.bbar3			= BBAR__INIT;
	pyxis_reg.bbar4			= BBAR__INIT;
	pyxis_reg.bbar5			= BBAR__INIT;
	pyxis_reg.bbar6			= BBAR__INIT;
	pyxis_reg.bbar7			= BBAR__INIT;
	pyxis_reg.bcr0			= BCR__INIT;
	pyxis_reg.bcr1			= BCR__INIT;
	pyxis_reg.bcr2			= BCR__INIT;
	pyxis_reg.bcr3			= BCR__INIT;
	pyxis_reg.bcr4			= BCR__INIT;
	pyxis_reg.bcr5			= BCR__INIT;
	pyxis_reg.bcr6			= BCR__INIT;
	pyxis_reg.bcr7			= BCR__INIT;
	pyxis_reg.btr0			= BTR__INIT;
	pyxis_reg.btr1			= BTR__INIT;
	pyxis_reg.btr2			= BTR__INIT;
	pyxis_reg.btr3			= BTR__INIT;
	pyxis_reg.btr4			= BTR__INIT;
	pyxis_reg.btr5			= BTR__INIT;
	pyxis_reg.btr6			= BTR__INIT;
	pyxis_reg.btr7			= BTR__INIT;
	pyxis_reg.cvm			= CVM__INIT;

	// PCI Window Control Registers [21174 TRM, Section 4.5]
	pyxis_reg.tbia			= TBIA__INIT;
	pyxis_reg.w0_base		= WX_BASE__INIT;
	pyxis_reg.w0_mask		= WX_MASK__INIT;
	pyxis_reg.t0_base		= TX_BASE__INIT;
	pyxis_reg.w1_base		= WX_BASE__INIT;
	pyxis_reg.w1_mask		= WX_MASK__INIT;
	pyxis_reg.t1_base		= TX_BASE__INIT;
	pyxis_reg.w2_base		= WX_BASE__INIT;
	pyxis_reg.w2_mask		= WX_MASK__INIT;
	pyxis_reg.t2_base		= TX_BASE__INIT;
	pyxis_reg.w3_base		= WX_BASE__INIT;
	pyxis_reg.w3_mask		= WX_MASK__INIT;
	pyxis_reg.t3_base		= TX_BASE__INIT;
	pyxis_reg.w_dac			= W_DAC__INIT;

	// Scatter-Gather Address Translation Registers [21174 TRM, Section 4.6]
	pyxis_reg.ltb_tag0	= LTB_TAGX__INIT;
	pyxis_reg.ltb_tag1	= LTB_TAGX__INIT;
	pyxis_reg.ltb_tag2	= LTB_TAGX__INIT;
	pyxis_reg.ltb_tag3	= LTB_TAGX__INIT;
	pyxis_reg.tb_tag4	= TB_TAGX__INIT;
	pyxis_reg.tb_tag5	= TB_TAGX__INIT;
	pyxis_reg.tb_tag6	= TB_TAGX__INIT;
	pyxis_reg.tb_tag7	= TB_TAGX__INIT;
	pyxis_reg.tb0_page0	= TBM_PAGEN__INIT;
	pyxis_reg.tb0_page1	= TBM_PAGEN__INIT;
	pyxis_reg.tb0_page2	= TBM_PAGEN__INIT;
	pyxis_reg.tb0_page3	= TBM_PAGEN__INIT;
	pyxis_reg.tb1_page0	= TBM_PAGEN__INIT;
	pyxis_reg.tb1_page1	= TBM_PAGEN__INIT;
	pyxis_reg.tb1_page2	= TBM_PAGEN__INIT;
	pyxis_reg.tb1_page3	= TBM_PAGEN__INIT;
	pyxis_reg.tb2_page0	= TBM_PAGEN__INIT;
	pyxis_reg.tb2_page1	= TBM_PAGEN__INIT;
	pyxis_reg.tb2_page2	= TBM_PAGEN__INIT;
	pyxis_reg.tb2_page3	= TBM_PAGEN__INIT;
	pyxis_reg.tb3_page0	= TBM_PAGEN__INIT;
	pyxis_reg.tb3_page1	= TBM_PAGEN__INIT;
	pyxis_reg.tb3_page2	= TBM_PAGEN__INIT;
	pyxis_reg.tb3_page3	= TBM_PAGEN__INIT;
	pyxis_reg.tb4_page0	= TBM_PAGEN__INIT;
	pyxis_reg.tb4_page1	= TBM_PAGEN__INIT;
	pyxis_reg.tb4_page2	= TBM_PAGEN__INIT;
	pyxis_reg.tb4_page3	= TBM_PAGEN__INIT;
	pyxis_reg.tb5_page0	= TBM_PAGEN__INIT;
	pyxis_reg.tb5_page1	= TBM_PAGEN__INIT;
	pyxis_reg.tb5_page2	= TBM_PAGEN__INIT;
	pyxis_reg.tb5_page3	= TBM_PAGEN__INIT;
	pyxis_reg.tb6_page0	= TBM_PAGEN__INIT;
	pyxis_reg.tb6_page1	= TBM_PAGEN__INIT;
	pyxis_reg.tb6_page2	= TBM_PAGEN__INIT;
	pyxis_reg.tb6_page3	= TBM_PAGEN__INIT;
	pyxis_reg.tb7_page0	= TBM_PAGEN__INIT;
	pyxis_reg.tb7_page1	= TBM_PAGEN__INIT;
	pyxis_reg.tb7_page2	= TBM_PAGEN__INIT;
	pyxis_reg.tb7_page3	= TBM_PAGEN__INIT;

	// Miscellaneous Registers [21174 TRM, Section 4.7]
	pyxis_reg.ccr		= CCR__INIT;
	pyxis_reg.clk_stat	= CLK_STAT__INIT;
	pyxis_reg.reset		= RESET__INIT;

	// Interrupt Control Registers [21174 TRM, Section 4.8]
	pyxis_reg.int_req	= INT_REQ__INIT;
	pyxis_reg.int_mask	= INT_MASK__INIT;
	pyxis_reg.int_hilo	= INT_HILO__INIT;
	pyxis_reg.int_route	= INT_ROUTE__INIT;
	pyxis_reg.gpo		= GPO__INIT;
	pyxis_reg.int_cnfg	= INT_CNFG__INIT;
	pyxis_reg.rt_count	= RT_COUNT__INIT;
	pyxis_reg.int_time	= INT_TIME__INIT;
	pyxis_reg.iic_ctrl	= IIC_CTRL__INIT;

// size memory banks
	{
	t_uint64	unsized = MEMSIZE;
	t_uint64	sized   = 0;
	pyxis_size_bank (&unsized, &sized, &pyxis_reg.bbar0, &pyxis_reg.bcr0);
	pyxis_size_bank (&unsized, &sized, &pyxis_reg.bbar1, &pyxis_reg.bcr1);
	pyxis_size_bank (&unsized, &sized, &pyxis_reg.bbar2, &pyxis_reg.bcr2);
	pyxis_size_bank (&unsized, &sized, &pyxis_reg.bbar3, &pyxis_reg.bcr3);
	pyxis_size_bank (&unsized, &sized, &pyxis_reg.bbar4, &pyxis_reg.bcr4);
	pyxis_size_bank (&unsized, &sized, &pyxis_reg.bbar5, &pyxis_reg.bcr5);
	pyxis_size_bank (&unsized, &sized, &pyxis_reg.bbar6, &pyxis_reg.bcr6);
	pyxis_size_bank (&unsized, &sized, &pyxis_reg.bbar7, &pyxis_reg.bcr7);
	}

// reset attached PCI bus (hose 0)
	pyxis_pci_reset ();

	return SCPE_OK;
}