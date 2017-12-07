/* sim_pci.h: PCI bus simulator

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
	This implements a 64-bit PCI Local Bus.

	Documentation:
		PCI SIG, "PCI Local Bus Specificaton Revision 3.0", February 3, 2004
		 Order Number: EC-R12GC-TE

	Features:
		64-bit PCI Address and Data implementation, without use of DAC (dual-acces cycles)
	
	Features not implemented:
		Parity failure

*/

/*
	History:
	--------
    2016-05-18  DTH  Added pci_bus_t.valid_slots for slot validation of register function
	2015-07-26  DTH  Started
*/

#ifndef __sim_pci_h 
#define __sim_pci_h

#include "sim_defs.h"

#define PCI_DAC_ADDRESS_M         0xFFFFFFFF00000000ull
#define PCI_SAC_MAX_ADDRESS     0x00000000FFFFFFFFull

#define PCI_CONFIG_NX_READ_VALUE 0xFFFFFFFFul

#define PCI_CONFIG__BUS			0x00FF0000ul
#define PCI_CONFIG__BUS_V		16
#define PCI_CONFIG__DEVICE		0x0000F800ul
#define	PCI_CONFIG__DEVICE_V	11
#define PCI_CONFIG__FUNCTION	0x00000700ul
#define PCI_CONFIG__FUNCTION_V	8
#define PCI_CONFIG__REGISTER	0x000000FCul
#define PCI_CONFIG__REGISTER_V	2
#define PCI_CONFIG__TYPE		0X00000003ul
#define PCI_CONFIG__TYPE_V		0

#define PCI_CONFIG__FAIL_NO_DEVICE	0xFFFFFFFFul;	// Failed to read CSR0
#define PCI_CONFIG__FAIL_REG_READ	0x00000000ul;	// Failed to read other registers

#define PCI_CSR0__VENDOR_ID	0x0000FFFFul
#define PCI_CSR0__DEVICE_ID	0xFFFF0000ul
#define PCI_CSR0__RW		0x00000000ul

#define PCI_CSR1__COMMAND	0x0000FFFFul
#define PCI_CSR1__STATUS	0xFFFF0000ul
#define PCI_CSR1__RW		0x0000FFFFul
#define PCI_CSR1__W1C		0xFFFF0000ul

#define PCI_CSR2__REVISION_ID	0x000000FFul
#define PCI_CSR2__CLASS_CODE	0xFFFFFF00ul
#define PCI_CSR2__RW			0x00000000UL

#define PCI_CSR3__CACHELINE_SIZE	0x000000FFul
#define PCI_CSR3__LATENCY_TIMER		0x0000FF00ul
#define PCI_CSR3__HEADER_TYPE		0x00FF0000ul
#define PCI_CSR3__BIST				0xFF000000ul

#define PCI_CFG15_MAX_LAT           0xFF000000ul
#define PCI_CFG15_MAX_LAT_V         24
#define PCI_CFG15_MIN_GNT           0x00FF0000ul
#define PCI_CFG15_MIN_GNT_V         16
#define PCI_CFG15_INT_PIN           0x0000FF00ul
#define PCI_CFG15_INT_PIN_V         8
#define PCI_CFG15_INT_LINE          0x000000FFul
#define PCI_CFG15_INT_LINE_V        0

#define PCI_CFG_H1R6_PBUS_M         0x000000FFul    // Primary Bus number
#define PCI_CFG_H1R6_PBUS_V         0
#define PCI_CFG_H1R6_SBUS_M         0x0000FF00ul    // Secondary Bus number
#define PCI_CFG_H1R6_SBUS_V         8
#define PCI_CFG_H1R6_SBBUS_M        0x00FF0000ul    // Subordinate Bus number
#define PCI_CFG_H1R6_SBBUS_V        16
#define PCI_CFG_H1R6_SLAT_M         0xFF000000ul    // Secondary Latency Timer
#define PCI_CFG_H1R6_SLAT_V         24

#define PCI_CFG_H1R7_IOBT_M         0x0000000Ful
#define PCI_CFG_H1R7_IOBT_V         0
#define PCI_CFG_H1R7_IOB_M          0x000000F0ul
#define PCI_CFG_H1R7_IOB_V          4
#define PCI_CFG_H1R7_IOLT_M         0x00000F00ul
#define PCI_CFG_H1R7_IOLT_V         8
#define PCI_CFG_H1R7_IOL_M          0x0000F000ul
#define PCI_CFG_H1R7_IOL_V          12
#define PCI_CFG_H1R7_SSTAT_M        0xFFFF0000ul
#define PCI_CFG_H1R7_SSTAT_V        16

#define PCI_CFG_H1R8_MEBT_M         0x0000000Ful
#define PCI_CFG_H1R8_MEBT_V         0
#define PCI_CFG_H1R8_MEB_M          0x0000FFF0ul
#define PCI_CFG_H1R8_MEB_V          4
#define PCI_CFG_H1R8_MELT_M         0x000F0000ul
#define PCI_CFG_H1R8_MELT_V         16
#define PCI_CFG_H1R8_MEL_M          0xFFF00000ul
#define PCI_CFG_H1R8_MEL_V          20

#define PCI_CFG_H1R9_PMBT_M         0x0000000Ful
#define PCI_CFG_H1R9_PMBT_V         0
#define PCI_CFG_H1R9_PMB_M          0x0000FFF0ul
#define PCI_CFG_H1R9_PMB_V          4
#define PCI_CFG_H1R9_PMLT_M         0x000F000Ful
#define PCI_CFG_H1R9_PMLT_V         16
#define PCI_CFG_H1R9_PML_M          0xFFF00000ul
#define PCI_CFG_H1R9_PML_V          20

#define PCI_CFG_H1R12_IOB_M          0x0000FFFFul
#define PCI_CFG_H1R12_IOB_V          0
#define PCI_CFG_H1R12_IOL_M          0xFFFF0000ul
#define PCI_CFG_H1R12_IOL_V          16


#define PCI_CBE_QWORD               0x00            // QWORD (quadword) 64-bit aligned
#define PCI_CBE_DWORD_HI            0x0F            // DWORD (longword) 64-bit left-aligned
#define PCI_CBE_DWORD_LO            0xF0            // DWORD (longword) 64-bit right-aligned or 32-bit aligned
#define PCI_CBE_WORD_LO             0xFC            // WORD (word) 32-bit right-aligned
#define PCI_CBE_WORD_HI             0xF3            // WORD (word) 32-bit left-aligned

/*
	Each PCI device must define one configuration space per function implemented by the card.

	Most PCI cards only implement one function; examples of multi-function devices are:
		4-port Ethernet cards
		ethernet + scsi combo card
		
	A card can implement up to 8 functions [0..7], which will be accessed during the configuration cycle.
    Multifunction cards must provide sequential functions; the first unimplemented function ends the probing.
*/


struct pci_config_t {
    uint32 csr[64];
	//uint32	csr0;	// 00h - Vendor ID and Device ID
	//uint32	csr1;	// 04h - Command and Status
	//uint32	csr2;	// 08h - Revision ID and Class Code
	//uint32	csr3;	// 0Ch - Cachline Size, Latency Timer, Header Type, and BIST
	//uint32	csr4;	// 10h - Base Address Register 0 (BAR0)
	//uint32	csr5;	// 14h - Base Address Register 1 (BAR1)
	//uint32	csr6;	// 18h - Base Address Register 2 (BAR2)
	//uint32	csr7;	// 1Ch - Base Address Register 3 (BAR3)
	//uint32	csr8;	// 20h - Base Address Register 4 (BAR4)
	//uint32	csr9;	// 14h - Base Address Register 5 (BAR5)
	//uint32	csr10;	// 28h - Cardbus CIS Pointer
	//uint32	csr11;  // 2Ch - Subsystem Vendor ID and Susbsystem ID
	//uint32	csr12;	// 30h - Expansion ROM Base Address (BARx)
	//uint32	csr13;	// 34h - Capabilities Pointer and Reserved
	//uint32	csr14;	// 38h - Reserved
	//uint32	csr15;	// 3Ch - Interrupt Line, Interrupt Pin, Min_Gnt, and Max_Lat
 //   uint32  csr16;  // 40h - Card-specific
 //   uint32  csr17;  // 44h - Card-specific
 //   uint32  csr18;  // 48h - Card-specific
	};
typedef struct pci_config_t PCI_CFG;

typedef int PCI_STAT;
#define PCI_OK           0  // Success
#define PCI_NOT_ME       1  // PCI no response from this device (normal)
#define PCI_NOFNC        2  // PCI Function Not Implemented
#define PCI_SETUP_ERR   10  // PCI setup error
#define PCI_ARG_ERR     11  // PCI routine argument error
                            // PCI errors that really shouldn't happen in simulation;
                            // .. there may be some special reason to issue/detect them
                            // .. such as a forced parity error to test parity behavior
#define PCI_PAR_ERROR   12  // PCI Parity Error (PERR#)
#define PCI_SYS_ERROR   13  // PCI System Error (SERR#)
#define PCI_TAR_RETRY   14  // PCI Target not ready; Retry
#define PCI_TAR_ABORT   15  // PCI Target Abort
#define PCI_TAR_DISC    16  // PCI Target Disconnect
#define PCI_MAS_ABORT   17  // PCI Master Abort


struct pci_bus_t;       // forward
typedef struct pci_bus_t PCI_BUS;
struct pci_device_t;    // forward
typedef struct pci_device_t PCI_DEV;

struct pci_device_t {
	char*		    name;		// name of device
    DEVICE*         dev;        // pointer back to the SIMH device; saves tedious lookups
    PCI_BUS*        bus;        // bus device is plugged into [hardwired by init]
    int             slot_num;   // pci slot device is plugged into [hardwired by init]
    int             functions;  // functions supported by this controller - 1 is normal
    
    PCI_CFG*        cfg_reg;      // cfg_reg[];     MANDATORY if using default cfg_ callbacks
    PCI_CFG*        cfg_wmask;    // cfg_wmask[];   MANDATORY if using default cfg_ callbacks

    // PCI device command callbacks
	PCI_STAT (*reset)      (PCI_DEV* _this);	// reset routine, mimics RST# signal

    // 32-bit PCI device command callbacks - devices only need to implement callbacks supported by the device
    // DAC is simulated by giving all 32-bit bus commands 64-bit pci addresses [PCI, 3.10.1]
    PCI_STAT (*int_ack)    (PCI_DEV* _this, int vector);    // interrupt acknowledge (optional)
    PCI_STAT (*special)    (PCI_DEV* _this, uint32 value);   // special cycle (optional)
    PCI_STAT (*io_read)	   (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32* value);
    PCI_STAT (*io_write)   (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32  value);
    PCI_STAT (*mem_read)   (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32* value);
    PCI_STAT (*mem_write)  (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32  value);
    PCI_STAT (*cfg_read)   (PCI_DEV* _this, int slot, int function, int register_, uint8 cbez, uint32* value);   // Type 0
    PCI_STAT (*cfg_write)  (PCI_DEV* _this, int slot, int function, int register_, uint8 cbez, uint32  value);	  // Type 0
    PCI_STAT (*mem_readm)  (PCI_DEV* _this, t_uint64 pci_src_address, uint32* lcl_dst_address, int count); // Memory Read Multiple
    PCI_STAT (*mem_readl)  (PCI_DEV* _this, t_uint64 pci_src_sddress, uint32* lcl_dst_address, int count);  // Memory Read Line
    PCI_STAT (*mem_writei) (PCI_DEV* _this, t_uint64 pci_dst_address, uint32* lcl_src_address, int count);  // Memory Write and Invalidate

    // 32-bit Type 1 Configuration is only used by bridge devices
    PCI_STAT (*cfg_read1)  (PCI_DEV* _this, int bus, int slot, int function, int register_, uint8 cbez, uint32* value); // Type 1
    PCI_STAT (*cfg_write1) (PCI_DEV* _this, int bus, int slot, int function, int register_, uint8 cbez, uint32  value);	// Type 1

    // 64-bit PCI bus callbacks are only used for memory transfers [PCI, 3.10, para 4]
    // "The bandwidth requirements for I/O and Configuration commands cannot justify the
    //  added complexity and, therefore, only Memory commands support 64-bit data transfers."
    PCI_STAT (*mem_read_64)   (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, t_uint64* value);
    PCI_STAT (*mem_write_64)  (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, t_uint64 value);
    PCI_STAT (*mem_readm_64)  (PCI_DEV* _this, t_uint64 pci_src_address, t_uint64* lcl_dst_address, int count);
    PCI_STAT (*mem_readl_64)  (PCI_DEV* _this, t_uint64 pci_src_address, t_uint64* lcl_dst_address, int count);
    PCI_STAT (*mem_writei_64) (PCI_DEV* _this, t_uint64 pci_dst_address, t_uint64* lcl_src_address, int count);
};

#define PCI_MAX_BUS		255		// Maximum PCI bus segments attached to a PCI bus
#define PCI_MAX_DEV		32		// Maximum PCI devices per PCI bus segment [0..31];
#define PCI_MAX_FUNC    7       // Maximum numbers of functions on a PCI controller is 7, but this can be reduced to save space

struct pci_bus_t {
	char*		name;           // name of bus for debugging convenience
    uint32      valid_slots;    // Bit map <31:0> of PCI slot numbers valid on this bus -
                                // this can make a difference during enumeration
                                // as most systems do not connect all slots, and
                                // frequently won't scan slots they don't understand. While
                                // there can be up to 32 possible slot numbers per bus,
                                // only 21 devices can be distinguished on a type 0 configuration
                                // due to addr<31:11> IDSEL# mapping.
                                // In practice, most buses only allow 3-9 slots due to bus
                                // electrical loading. constraints.
    PCI_BUS*    parent;         // Higher-level PCI bus, or NULL if top-level bus
	PCI_DEV*    dev[PCI_MAX_DEV];
    int         bus_num;        // Number of PCI bus - this is determined during the enumeration/configuration pass
};

// Since the CBE# (Command/Byte Enable) signal uses zeros to mark the enabled bytes, the CBEZ_MASK
// reverses the standard bit-mask enable logic to match the cbez field.
extern const char* CBEZ_LANES[256];
extern const t_uint64 CBEZ_MASK[256];

// function prototypes
PCI_STAT pci_bus_reset		(PCI_BUS* bus);
PCI_STAT pci_bus_special    (PCI_BUS* bus, uint32 value);
PCI_STAT pci_bus_cfg_read   (PCI_BUS* bus, int device, int function, int register_, uint8 cbez, uint32* value);
PCI_STAT pci_bus_cfg_write  (PCI_BUS* bus, int device, int function, int register_, uint8 cbez, uint32  value);
PCI_STAT pci_bus_io_read    (PCI_BUS* bus, t_uint64 address, int size, uint8 cbez, uint32* value);
PCI_STAT pci_bus_io_write   (PCI_BUS* pci, t_uint64 address, int size, uint8 cbez, uint32 value);
PCI_STAT pci_bus_mem_read   (PCI_BUS* bus, t_uint64 pci_src_address, int size, uint8 cbez, uint32* lcl_dst_address);
PCI_STAT pci_bus_mem_readm  (PCI_BUS* bus, t_uint64 pci_src_address, uint32* lcl_dst_address, int lw_repeat);
PCI_STAT pci_bus_mem_readl  (PCI_BUS* bus, t_uint64 pci_src_address, uint32* lcl_dst_address, int lw_repeat);
PCI_STAT pci_bus_mem_writei (PCI_BUS* bus, t_uint64 pci_dst_address, uint32* lcl_src_address, int lw_repeat);
PCI_STAT pci_bus_mem_write  (PCI_BUS* bus, t_uint64 pci_dst_address, int size, uint8 cbez, uint32 value);
PCI_STAT pci_bus_cfg_read1  (PCI_BUS* bus, int bus_num, int device, int function, int register_, uint8 cbez, uint32* value);
PCI_STAT pci_bus_cfg_write1 (PCI_BUS* bus, int bus_num, int device, int function, int register_, uint8 cbez, uint32  value);
PCI_STAT pci_bus_mem_read64   (PCI_BUS* bus, t_uint64 pci_src_address, int size, uint8 cbez, t_uint64* value);
PCI_STAT pci_bus_mem_write64  (PCI_BUS* bus, t_uint64 pci_dst_address, int size, uint8 cbez, t_uint64  value);

PCI_STAT pci_register       (PCI_BUS* pci, PCI_DEV* device, int slot);
PCI_STAT pci_unregister     (PCI_BUS* pci, PCI_DEV* device, int slot);

PCI_STAT pci_cfg_read_default  (PCI_DEV* dev, int slot, int function, int register_, uint8 cbez, uint32* value);
PCI_STAT pci_cfg_write_default (PCI_DEV* dev, int slot, int function, int register_, uint8 cbez, uint32 value);

#endif // ifndef __sim_pci_h