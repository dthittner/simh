/* alpha_pyxis.h: Alpha Pyxis 21174 Core logic chip

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

/*
	History:
	--------
    2016-07-16  DTH  Fix INT_REQ__CSR (too many trailing zeros)
	2015-07-21  DTH  Completed register masks
	2015-07-15  DTH  Started
*/

#ifndef __alpha_pyxis_h
#define __alpha_pyxis_h

#include "sim_defs.h"

// Revision Control Register @87.4000.0080 [21174 TRM, Section 5.1.1]
#define PYXIS_REV__CSR			0x8740000080ull
#define PYXIS_REV__PYXIS_ID		0x0000FF00ul
#define PYXIS_REV__PYXIS_REV	0x000000FFul
#define PYXIS_REV__MBZ			0xFFFF0000ul
#define PYXIS_REV__INIT			0x00000101ul

// PCI Latency Register @87.4000.00C0 [21174 TRM, Section 5.1.2]
#define PCI_LAT__CSR			0x87400000c0ull
#define PCI_LAT__MSTR_LAT		0x0000FF00ul
#define PCI_LAT__MSTR_RET		0x000000F0ul
#define PCI_LAT__TRGT_RET		0x0000000Ful
#define PCI_LAT__MBZ			0xFFFF0000ul
#define PCI_LAT__INIT			0x00000000ul

// Control Register @87.4000.0100 [21174 TRM, Section 5.1.3]
#define PYXIS_CTRL__CSR				0x8740000100ull
#define PYXIS_CTRL__RM_USE_HISTORY	0x40000000ul
#define PYXIS_CTRL__RM_TYPE			0x30000000ul
#define PXYIS_CTRL__RL_USE_HISTORY	0x04000000ul
#define PYXIS_CTRL__RL_TYPE			0x03000000ul
#define PYXIS_CTRL__RD_USE_HISTORY	0x00400000ul
#define PYXIS_CTRL__RD_TYPE			0x00300000ul
#define PYXIS_CTRL__ASSERT_IDLE_BC	0x00002000ul
#define PYXIS_CTRL__ECC_CHK_EN		0x00001000ul
#define PYXIS_CTRL__MCHK_ERR_EN		0x00000800ul
#define PYXIS_CTRL__FILL_ERR_EN		0x00000400ul
#define PYXIS_CTRL__PERR_EN			0x00000200ul
#define PYXIS_CTRL__ADDR_PE_EN		0x00000100ul
#define PYXIS_CTRL__PCI_ACK64_EN	0x00000080ul
#define PYXIS_CTRL__PCI_REQ64_EN	0x00000040ul
#define PYXIS_CTRL__PCI_MEM_EN		0x00000020ul
#define PYXIS_CTRL__PCI_MST_EN		0x00000010ul
#define PYXIS_CTRL__FST_BB_EN		0x00000008ul
#define PYXIS_CTRL__PCI_LOOP_EN		0x00000004ul
#define PYXIS_CTRL__PCI_EN			0x00000001ul
#define PYXIS_CTRL__MBZ				0x111F3002ul
#define PYXIS_CTRL__INIT			0x00000000ul

// Control Register 1 @87.4000.0140 [21174 TRM, Section 5.1.4]
#define PYXIS_CTRL1__CSR			0x8740000140ull
#define PYXIS_CTRL1__LW_PAR_MODE	0x00001000ul
#define PYXIS_CTRL1__PCI_LINK_EN	0x00000100ul
#define PYXIS_CTRL1__PCI_MWIN_EN	0x00000010ul
#define PYXIS_CTRL1__IOA_BEN		0x00000001ul
#define PYXIS_CTRL1__MBZ			0xFFFFEEEEul
#define PYXIS_CTRL1__INIT			0x00000000ul

// Flash Control Register @87.4000.0200 [21174 TRM, Section 5.1.5]
#define FLASH_CTRL__CSR					0x8740000200ull
#define FLASH_CTRL__FLASH_HIGH_ENABLE	0x00002000ul
#define FLASH_CTRL__FLASH_LOW_ENABLE	0x00001000ul
#define FLASH_CTRL__FLASH_ACCESS_TIME	0x00000F00ul
#define FLASH_CTRL__FLASH_DISABLE_TIME	0x000000F0ul
#define FLASH_CTRL__FLASH_WP_WIDTH		0x0000000Ful
#define FLASH_CTRL__MBZ					0xFFFFC000ul
#define FLASH_CTRL__INIT				0x00003F7Ful

// Hardware Address Extension Register @87.4000.0400 [21174 TRM, Section 5.1.6]
#define HAE_MEM__CSR				0x8740000400ull
#define HAE_MEM__REGION_1			0xE0000000ul
#define HAE_MEM__REGION_2			0x0000F800ul
#define HAE_MEM__REGION_3			0x000000FCul
#define HAE_MEM__MBZ				0x1FFF0703ul
#define HAE_MEM__INIT				0x00000000ul

// Hardware Address Extension Register @87.4000.0440 [21174 TRM, Section 5.1.7]
#define HAE_IO__CSR					0x8740000440ull
#define HAE_IO__HAE_IO				0xFE000000ul
#define HAE_IO__MBZ					0x01FFFFFFul
#define HAE_IO__INIT				0x00000000ul

// Configuration Type Register @87.4000.0480 [21174 TRM, Section 5.1.8]
#define CFG__CSR					0x8740000480ull
#define CFG__CFG					0x00000003ul
#define CFG__MBZ					0xFFFFFFFCul
#define CFG__INIT					0x00000000ul

// Diagnostic Control Register @87.4000.2000 [21174 TRM,Section 5.2.1]
#define PYXIS_DIAG__CSR				0x8740002000ull
#define PYXIS_DIAG__FPE_TO_EV56		0x80000000ul
#define PYXIS_DIAG__FPE_PCI			0x30000000ul
#define PYXIS_DIAG__USE_CHECK		0x00000002ul
#define PYXIS_DIAG__MBZ				0x4FFFFFFDul
#define PYXIS_DIAG__INIT			0x00000000ul

// Diagnostic Check Register @87.4000.3000 [21174 TRM, Section 5.2.2]
#define DIAG_CHECK__CSR				0x8740003000ull
#define DIAG_CHECK__DIAG_CHECK		0x000000FFul
#define DIAG_CHECK__MBZ				0xFFFFFF00ul
#define DIAG_CHECK__INIT			0x00000000ul

// Performance Monitor Register @87.4000.4000 [21174 TRM, Section 5.3.1]
#define PERF_MONITOR__CSR			0x8740004000ull
#define PERF_MONITOR__HIGH_COUNT	0xFFFF0000ul
#define PERF_MONITOR__LOW_COUNT		0x0000FFFFul
#define PERF_MONITOR__MBZ			0x00000000ul
#define PERF_MONITOR__INIT			0x00000000ul

// Performance Monitor Control Register @87.4000.4040 [21174 TRM, Section 5.3.2]
#define PERF_CONTROL__CSR				0x8740004040ull
#define PERF_CONTROL__HIGH_COUNT_START	0x80000000ul
#define PERF_CONTROL__HIGH_ERR_STOP		0x40000000ul
#define PERF_CONTROL__HIGH_COUNT_CLR	0x20000000ul
#define PERF_CONTROL__HIGH_COUNT_CYCLES	0x10000000ul
#define PERF_CONTROL__HIGH_SELECT		0x00070000ul
#define PERF_CONTROL__LOW_COUNT_START	0x00008000ul
#define PERF_CONTROL__LOW_ERR_STOP		0x00004000ul
#define PERF_CONTROL__LOW_COUNT_CLR		0x00002000ul
#define PERF_CONTROL__LOW_COUNT_CYCLES	0x00001000ul
#define PERF_CONTROL__LOW_SELECT		0x00000007ul
#define PERF_CONTROL__MBZ				0x0FF80FF8ul
#define PERF_CONTROL__INIT				0x00000000ul

// Error Register @87.4000.8200 [21174 TRM, Section 5.4.1]
#define PYXIS_ERR__CSR					0x8740008200ull
#define PYXIS_ERR__ERR_VALID			0x80000000ul
#define PYXIS_ERR__LOST_IOA_TIMEOUT		0x08000000ul
#define PYXIS_ERR__LOST_PA_PTE_INV		0x02000000ul
#define PYXIS_ERR__LOST_RCVD_TAR_ABT	0x01000000ul
#define PYXIS_ERR__LOST_RCVD_MAS_ABT	0x00800000ul
#define PYXIS_ERR__LOST_PCI_ADDR_PE		0x00400000ul
#define PYXIS_ERR__LOST_PERR			0x00200000ul
#define PYXIS_ERR__LOST_MEM_NEM			0x00080000ul
#define PYXIS_ERR__LOST_CPU_PE			0x00040000ul
#define PYXIS_ERR__LOST_UN_CORR_ERR		0x00020000ul
#define PYXIS_ERR__LOST_CORR_ERR		0x00010000ul
#define PYXIS_ERR__IOA_TIMEOUT			0x00000800ul
#define PYXIS_ERR__PA_PTE_INV			0x00000200ul
#define PYXIS_ERR__RCVD_TAR_ABT			0x00000100ul
#define PYXIS_ERR__RCVD_MAS_ABT			0x00000080ul
#define PYXIS_ERR__PCI_ADDR_PE			0x00000040ul
#define PYXIS_ERR__PCI_PERR				0x00000020ul
#define PYXIS_ERR__PCI_SERR				0x00000010ul
#define PYXIS_ERR__MEM_NEM				0x00000008ul
#define PYXIS_ERR__CPU_PE				0x00000004ul
#define PYXIS_ERR__UN_CORR_ERR			0x00000002ul
#define PYXIS_ERR__CORR_ERR				0x00000001ul
#define PYXIS_ERR__W1C					0x00000BFFul
#define PYXIS_ERR__MBZ					0x7410F400ul
#define PYXIS_ERR__INIT					0x00000000ul

// Status Register @87.4000.8240 [21174 TRM, Section 5.4.2]
#define PYXIS_STAT__CSR			0x8740008240ull
#define PYXIS_STAT__TLB_MISS	0x00000800ul
#define PYXIS_STAT__IOA_VALID	0x000000F0ul
#define PYXIS_STAT__PCI_STATUS1	0x00000002ul
#define PYXIS_STAT__PCI_STATUS0	0x00000001ul
#define PYXIS_STAT__MBZ			0xFFFFF70Cul
#define PYXIS_STAT__INIT		0x00000000ul

// Error Mask Register @87.4000.8280 [21174 TRM, Section 5.4.3]
#define ERR_MASK__CSR			0x8740008280ull
#define ERR_MASK__IOA_TIMEOUT	0x00000800ul
#define ERR_MASK__PA_PTE_INV	0x00000200ul
#define ERR_MASK__RCVD_TAR_ABT	0x00000100ul
#define ERR_MASK__RCVD_MAS_ABT	0x00000080ul
#define ERR_MASK__PCI_ADDR_PE	0x00000040ul
#define ERR_MASK__PCI_PERR		0x00000020ul
#define ERR_MASK__PCI_SERR		0x00000010ul
#define ERR_MASK__MEM_NEM		0x00000008ul
#define ERR_MASK__CPU_PE		0x00000004ul
#define ERR_MASK__UN_COR_ERR	0x00000002ul
#define ERR_MASK__COR_ERR		0x00000001ul
#define ERR_MASK__MBZ			0xFFFFF400ul
#define ERR_MASK__INIT			0x00000000ul

// Syndrome Register @87.4000.8300 [21174 TRM, Section 5.4.4]
#define PYXIS_SYN__CSR					0x8740008300ull
#define PYXIS_SYN__UNCORRECTABLE_ERROR1	0x08000000ul
#define PYXIS_SYN__UNCORRECTABLE_ERROR0	0x04000000ul
#define PYXIS_SYN__CORRECTABLE_ERROR1	0x02000000ul
#define PYXIS_SYN__CORRECTABLE_ERROR0	0x01000000ul
#define PYXIS_SYN__RAW_CHECK_BITS		0x00FF0000ul
#define PYXIS_SYN__ERROR_SYNDROME1		0x0000FF00ul
#define PYXIS_SYN__ERROR_SYNDROME0		0x000000FFul
#define PYXIS_SYN__MBZ					0xF0000000ul
#define PYXIS_SYN__INIT					0x00000000ul

// Error Data Register @87.4000.8308 [21174 TRM, Section 5.4.5]
#define PYXIS_ERR_DATA__CSR					0x8740008308ull
#define PYXIS_ERR_DATA__ERROR_DATA_QUADWORD	0xFFFFFFFFFFFFFFFFull
#define PYXIS_ERR_DATA__MBZ					0x0000000000000000ull
#define PYXIS_ERR_DATA__INIT				0x0000000000000000ull

// Memory Error Address Register @87.4000.8400 [21174 TRM, Section 5.4.6]
#define MEAR__CSR			0x8740008400ull
#define MEAR__ERROR_ADDR	0xFFFFFFF0ul
#define MEAR__MBZ			0x0000000Ful
#define MEAR__INIT			0x00000000ul

// Memory Error Status Register @87.4000.8440 [21174 TRM, Section 5.4.7]
#define MESR__CSR				0x8740008440ull
#define MESR__SEQ_STATE			0xFE000000ul
#define MESR__DATA_CYCLE_TYPE	0x01F00000ul
#define MESR__OWORD_INDEX		0x00030000ul
#define MESR__TLBFILL_NXM		0x00008000ul
#define MESR__VICTIM_NXM		0x00004000ul
#define MESR__IO_WR_NXM			0x00002000ul
#define MESR__IO_RD_NXM			0x00001000ul
#define MESR__CPU_WR_NXM		0x00000800ul
#define MESR__CPU_RD_NXM		0x00000400ul
#define MESR__DMA_WR_NXM		0x00000200ul
#define MESR__DMA_RD_NXM		0x00000100ul
#define MESR__ERROR_ADDR		0x000000FFul
#define MESR__RW				0xFE000000ul
#define MESR__MBZ				0x000C0000ul
#define MESR__INIT				0x00000000ul

// PCI Error Register 0 @87.4000.8800 [21174 TRM, Section 5.4.8]
#define PCI_ERR0__CSR			0x8740008800ull
#define PCI_ERR0__PCI_DAC		0x10000000ul
#define PCI_ERR0__PCI_CMD		0x0F000000ul
#define PCI_ERR0__TARGET_STATE	0x00F00000ul
#define PCI_ERR0__MASTER_STATE	0x000F0000ul
#define PCI_ERR0__WINDOW		0x00000F00ul
#define PCI_ERR0__DMA_DAC		0x00000020ul
#define PCI_ERR0__DMA_CMD		0x0000000Ful
#define PCI_ERR0__MBZ			0xE000F0D0ul
#define PCI_ERR0__INIT			0x00000000ul

// PCI Error Register 1 @87.4000.8840 [21174 TRM, Section 5.4.9]
#define PCI_ERR1__CSR			0x8740008840ull
#define PCI_ERR1__DMA_ADDRESS	0xFFFFFFFFul
#define PCI_ERR1__MBZ			0x00000000ul
#define PCI_ERR1__INIT			0x00000000ul

// PCI Error Register 2 @87.4000.8880 [21174 TRM, Section 5.4.10]
#define PCI_ERR2__CSR			0x8740008880ull
#define PCI_ERR2__PCI_ADDRESS	0xFFFFFFFFul
#define PCI_ERR2__MBZ			0x00000000ul
#define PCI_ERR2__INIT			0x00000000ul

// Memory Control Register @87.5000.0000 [21174 TRM, Section 5.5.1]
#define MCR__CSR				0x8750000000ull
#define MCR__DRAM_MODE			0x3FFF0000ul
#define MCR__DRAM_CLK_AUTO		0x00008000ul
#define MCR__CKE_AUTO			0x00004000ul
#define MCR__SEQ_TRACE			0x00002000ul
#define MCR__OVERLAP_DISABLE	0x00001000ul
#define MCR__PIPELINED_BCACHE	0x00000800ul
#define MCR__BCACHE_ENABLE		0x00000400ul
#define MCR__BCACHE_TYPE		0x00000200ul
#define MCR__SERVER_MODE		0x00000100ul
#define MCR__MODE_REQ			0x00000001ul
#define MCR__MBZ				0xC00000FEul
#define MCR__RW					0x3FFFFC01ul
#define MCR__INIT				0x00000000ul

// Memory Clock Mask Register @87.5000.0040 [21174 TRM, Section 5.5.2]
#define MCMR__CSR				0x8750000040ull
#define MCMR__MCMR				0x0000FFFFul
#define MCMR__MBZ				0xFFFF0000ul
#define MCMR__INIT				0x0000fffful

// Global Timing Register @87.5000.0200 [21174 TRM, Section 5.5.3]
#define GTR__CSR				0x8750000200ull
#define GTR__IDLE_BC_WIDTH		0x00000700ul
#define GTR__CAS_LATENCY		0x00000030ul
#define GTR__MIN_RAS_PRECHARGE	0x00000007ul
#define GTR__MBZ				0xFFFFF8C8ul
#define GTR__INIT				0x00000034ul

// Refresh Timing Register @87.5000.0300 [21174 TRM, Section 5.5.4]
#define RTR__CSR				0x8750000300ull
#define RTR__RTR_FORCE_REF		0x00008000ul
#define RTR__REF_INTERVAL		0x00001F80ul
#define RTR__REFRESH_WIDTH		0x00000070ul
#define RTR__MBZ				0xFFFF600Ful
#define RTR__INIT				0x000002E0ul

// Row History Policy Mask Register @87.5000.0400 [21174 TRM, Section 5.5.5]
#define RHPR__CSR				0x8750000400ull
#define RHPR__POLICY_MASK		0x0000FFFFul
#define RHPR__MBZ				0xFFFF0000ul
#define RHPR__INIT				0x0000E809ul	// documented value E8809 is impossible

// Memory Control Debug Register 1 @87.5000.0500 [21174 TRM, Section 5.5.6]
#define MDR1__CSR				0x8750000500ull
#define MDR1__ENABLE			0x80000000ul
#define MDR1__SEL3				0x3F000000ul
#define MDR1__SEL2				0x003F0000ul
#define MDR1__SEL1				0x00003F00ul
#define MDR1__SEL0				0x0000003Ful
#define MDR1__MBZ				0x40C0C0C0ul
#define MDR1__INIT				0x00000000ul

// Memory Control Debug Register 2 @87.5000.0540 [21174 TRM, Section 5.5.7]
#define MDR2__CSR				0x8750000540ull
#define MDR2__ENABLE			0x80000000ul
#define MDR2__SEL3				0x3F000000ul
#define MDR2__SEL2				0x003F0000ul
#define MDR2__SEL1				0x00003F00ul
#define MDR2__SEL0				0x0000003Ful
#define MDR2__MBZ				0x40C0C0C0ul
#define MDR2__INIT				0x00000000ul

// Base Address Registers @87.5000.{0600..07C0} [21174 TRM, Section 5.5.8]
#define BBAR0__CSR				0x8750000600ull
#define BBAR1__CSR				0x8750000640ull
#define BBAR2__CSR				0x8750000680ull
#define BBAR3__CSR				0x87500006C0ull
#define BBAR4__CSR				0x8750000700ull
#define BBAR5__CSR				0x8750000740ull
#define BBAR6__CSR				0x8750000780ull
#define BBAR7__CSR				0x87500007C0ull
#define BBAR__BASEADDR			0x0000FFC0ul
#define BBAR__MBZ				0xFFFF003Ful
#define BBAR__INIT				0x00000000ul

// Bank Configuration Registers @ 87.5000.{0800..09C0} [21174 TRM, Section 5.5.9]
#define BCR0__CSR				0x8750000800ull
#define BCR1__CSR				0x8750000840ull
#define BCR2__CSR				0x8750000880ull
#define BCR3__CSR				0x87500008C0ull
#define BCR4__CSR				0x8750000900ull
#define BCR5__CSR				0x8750000940ull
#define BCR6__CSR				0x8750000980ull
#define BCR7__CSR				0x87500009C0ull
#define BCR__4BANK				0x00000080ul
#define BCR__ROWSEL				0x00000040ul
#define BCR__SUBBANK_ENABLE		0x00000020ul
#define BCR__BANK_SIZE			0x0000001Eul
#define BCR__BANK_SIZE_512MB	0x00000002ul
#define BCR__BANK_SIZE_256MB	0x00000004ul
#define BCR__BANK_SIZE_128MB	0x00000006ul
#define BCR__BANK_SIZE_64MB     0x00000008ul
#define BCR__BANK_SIZE_32MB     0x0000000Aul
#define BCR__BANK_SIZE_16MB		0x0000000Cul
#define BCR__BANK_SIZE_8MB		0x0000000Eul
#define BCR__BANK_ENABLE		0x00000001ul
#define BCR__MBZ				0xFFFFFF00ul
#define BCR__INIT				0x00000000ul

// Bank Timing Registers @ 87.5000.{0A00..0BC0} [21174 TRM, Section 5.5.10]
#define BTR0__CSR				0x8750000A00ull
#define BTR1__CSR				0x8750000A40ull
#define BTR2__CSR				0x8750000A80ull
#define BTR3__CSR				0x8750000AC0ull
#define BTR4__CSR				0x8750000B00ull
#define BTR5__CSR				0x8750000B40ull
#define BTR6__CSR				0x8750000B80ull
#define BTR7__CSR				0x8750000BC0ull
#define BTR__SLOW_PRECHARGE		0x00000020ul
#define BTR__TOSHIBA			0x00000010ul
#define	BTR__ROW_ADDR_HOLD		0x00000007ul
#define BTR__MBZ				0xFFFFFFC8ul
#define BTR__INIT				0x00000000ul

// Cache Valid Map Register @ 87.5000.0C00 [21174 TRM, Section 5.5.11]
#define CVM__CSR				0x8750000C00ull
#define CVM__CACHE_VALID_MAP	0xFFFFFFFFul
#define CVM__MBZ				0x00000000ul
#define CVM__INIT				0x00000000ul	// initial value not defined in manual -
												// might depend on size of Flash?

// Scatter-Gather Translation Buffer Invalidate Register @87.6000.0100 [21174 TRM, Section 5.6.1]
#define TBIA__CSR				0x8760000100ull
#define TBIA__TBIA				0x00000003ul
#define TBIA__MBZ				0xFFFFFFFCul
#define TBIA__INIT				0x00000000ul

// Windows Base Registers @87.6000.{0400,0500,0600,0700} [21174 TRM, Section 5.6.2]
#define W0_BASE__CSR			0x8760000400ull
#define W1_BASE__CSR			0x8760000500ull
#define W2_BASE__CSR			0x8760000600ull
#define W3_BASE__CSR			0x8760000700ull
#define WX_BASE__W_BASE			0xFFF00000ul
#define WX_BASE__DAC_ENABLE		0x00000008ul
#define WX_BASE__MEMCS_EN		0x00000004ul
#define WX_BASE__WX_BASE_SG		0x00000002ul
#define WX_BASE__W_EN			0x00000001ul
#define WX_BASE__MBZ			0xFFFFFFF0ul
#define WX_BASE__INIT			0x00000000ul

// Windows Mask Registers @87.6000.{0440,0540,0640,0740} [21174 TRM, Section 5.6.3]
#define W0_MASK__CSR			0x8760000440ull
#define W1_MASK__CSR			0x8760000540ull
#define W2_MASK__CSR			0x8760000640ull
#define W3_MASK__CSR			0x8760000740ull
#define WX_MASK__W_MASK			0xFFF00000ul
#define WX_MASK__MBZ			0x000FFFFFul
#define WX_MASK__INIT			0x00000000ul

// Translated Base Registers @87.6000.{0480,0580,0680,0780} [21174 TRM, Section 5.6.4]
#define T0_BASE__CSR			0x8760000480ull
#define T1_BASE__CSR			0x8760000580ull
#define T2_BASE__CSR			0x8760000680ull
#define T3_BASE__CSR			0x8760000780ull
#define TX_BASE__T_BASE			0xFFFFFF00ul
#define TX_BASE__MBZ			0x000000FFul
#define TX_BASE__INIT			0x00000000ul

// Window DAC Base Register @87.6000.07C0 [21174 TRM, Section 5.6.5]
#define W_DAC__CSR				0x87600007C0ull
#define W_DAC__DAC_BASE			0x000000FFul
#define W_DAC__MBZ				0xFFFFFF00ul
#define W_DAC__INIT				0x00000000ul

// Lockable Translation Buffer Tag Registers @87.6000.{0800,0840,0880,08C0} [21174 TRM, Section 5.7.1]
#define LTB_TAG0__CSR			0x8760000800ull
#define LTB_TAG1__CSR			0x8760000840ull
#define LTB_TAG2__CSR			0x8760000880ull
#define LTB_TAG3__CSR			0x87600008C0ull
#define LTB_TAGX__TB_TAG		0xFFFF8000ul
#define LTB_TAGX__DAC			0x00000004ul
#define LTB_TAGX__LOCKED		0x00000002ul
#define LTB_TAGX__VALID			0x00000001ul
#define LTB_TAGX__MBZ			0x00007FF8ul
#define LTB_TAGX__INIT			0x00000000ul

// Translation Buffer Tag Registers @87.6000.{0900,0940,0980,09C0} [21174 TRM, Section 5.7.2]
#define TB_TAG4__CSR			0x8760000900ull
#define TB_TAG5__CSR			0x8760000940ull
#define TB_TAG6__CSR			0x8760000980ull
#define TB_TAG7__CSR			0x87600009C0ull
#define TB_TAGX__TB_TAG			0xFFFF8000ul
#define TB_TAGX__DAC			0x00000004ul
#define TB_TAGX__VALID			0x00000001ul
#define TB_TAGX__MBZ			0x00007FFAul
#define TB_TAGX__INIT			0x00000000ul

// Translation Buffer Page Registers @87.6000.{1000..17C0} [21174 TRM, Section 5.7.3]
#define TB0_PAGE0__CSR			0x8760001000ull
#define TB0_PAGE1__CSR			0x8760001040ull
#define TB0_PAGE2__CSR			0x8760001080ull
#define TB0_PAGE3__CSR			0x87600010C0ull
#define TB1_PAGE0__CSR			0x8760001100ull
#define TB1_PAGE1__CSR			0x8760001140ull
#define TB1_PAGE2__CSR			0x8760001180ull
#define TB1_PAGE3__CSR			0x87600011C0ull
#define TB2_PAGE0__CSR			0x8760001200ull
#define TB2_PAGE1__CSR			0x8760001240ull
#define TB2_PAGE2__CSR			0x8760001280ull
#define TB2_PAGE3__CSR			0x87600012C0ull
#define TB3_PAGE0__CSR			0x8760001300ull
#define TB3_PAGE1__CSR			0x8760001340ull
#define TB3_PAGE2__CSR			0x8760001380ull
#define TB3_PAGE3__CSR			0x87600013C0ull
#define TB4_PAGE0__CSR			0x8760001400ull
#define TB4_PAGE1__CSR			0x8760001440ull
#define TB4_PAGE2__CSR			0x8760001480ull
#define TB4_PAGE3__CSR			0x87600014C0ull
#define TB5_PAGE0__CSR			0x8760001500ull
#define TB5_PAGE1__CSR			0x8760001540ull
#define TB5_PAGE2__CSR			0x8760001580ull
#define TB5_PAGE3__CSR			0x87600015C0ull
#define TB6_PAGE0__CSR			0x8760001600ull
#define TB6_PAGE1__CSR			0x8760001640ull
#define TB6_PAGE2__CSR			0x8760001680ull
#define TB6_PAGE3__CSR			0x87600016C0ull
#define TB7_PAGE0__CSR			0x8760001700ull
#define TB7_PAGE1__CSR			0x8760001740ull
#define TB7_PAGE2__CSR			0x8760001780ull
#define TB7_PAGE3__CSR			0x87600017C0ull
#define TBM_PAGEN__PAGE_ADDRESS	0x003FFFFEul
#define TBM_PAGEN__VALID		0x00000001ul
#define TBM_PAGEN__MBZ			0xFFC00000ul
#define TBM_PAGEN__INIT			0x00000000ul

// Clock Control Register @87.8000.0000 [21174 TRM, Section 5.8.1]
#define CCR__CSR				0x8780000000ull
#define CCR__DCLK_DELAY			0xFF000000ul
#define CCR__DCLK_PCSEL			0x00040000ul
#define CCR__DCLK_FORCE			0x00020000ul
#define CCR__DCLK_INV			0x00010000ul
#define CCR__SEL_CONFIG_SRC		0x00001000ul
#define CCR__CSR_LONG_RESET		0x00000400ul
#define CCR__CSR_PLL_RANGE		0x00000300ul
#define CCR__CSR_PCLK_DIVIDE	0x00000070ul
#define	CCR__CSR_CLOCK_DIVIDE	0x00000003ul
#define CCR__MBZ				0x00F8E88Cul
#define CCR__INIT				0x18020631ul

// Clock Status Register @87.8000.0100 [21174 TRM, Section 5.8.2]
#define CLK_STAT__CSR				0x8780000100ull
#define CLK_STAT__DELAY_ELEMENTS	0xFF000000ul
#define CLK_STAT__PU_LONG_RESET		0x00400000ul
#define CLK_STAT__PU_PLL_RANGE		0x00300000ul
#define CLK_STAT__PU_PCLK_DIVIDE	0x00070000ul
#define CLK_STAT__PU_CLK_DIVIDE		0x00003000ul
#define CLK_STAT__LONG_RESET		0x00000400ul
#define CLK_STAT__PLL_RANGE			0x00000300ul
#define CLK_STAT__PCLK_DIVIDE		0x00000070ul
#define CLK_STAT__CLK_DIVIDE		0x00000003ul
#define CLK_STAT__MBZ				0x0088C88Cul
#define CLK_STAT__INIT				0x00000000ul

// Reset Register @87.8000.0900 [21174 TRM, Section 5.8.3]
#define RESET__CSR				0x8780000900ull
#define RESET__RESET			0xFFFFFFFFul
#define RESET__MBZ				0x00000000ul
#define RESET__INIT				0x00000000ul
#define RESET__MAGIC_VALUE		0x0000DEADul

// Interrupt Request Register @87.A000.0000 [21174 TRM, Section 5.9.1]
#define INT_REQ__CSR			0x87A0000000ull
#define INT_REQ__ERROR_INT		0x8000000000000000ull
#define INT_REQ__CLK_INT_PEND	0x4000000000000000ull
#define INT_REQ__INT_REQ		0x3FFFFFFFFFFFFFFFull
#define INT_REQ__W1C			0x7FFFFFFFFFFFFFFFull
#define INT_REQ__MBZ			0x0000000000000000ull
#define INT_REQ__INIT			0x0000000000000000ull

// Interrupt Mask Register @87.A000.0040 [21174 TRM, Section 5.9.2]
#define INT_MASK__CSR			0x87A0000040ull
#define INT_MASK__CLK_INT_EN	0x4000000000000000ull
#define INT_MASK__INT_MASK		0x3FFFFFFFFFFFFFFFull
#define INT_MASK__MBZ			0x8000000000000000ull
#define INT_MASK__INIT			0x0000000000000000ull

// Interrupt High/Low Select Register @87.A000.00C0 [21174 TRM, Section 5.9.3]
#define INT_HILO__CSR			0x87A00000C0ull
#define INT_HILO__INT_HILO		0x00000000000000FFull
#define INT_HILO__MBZ			0xFFFFFFFFFFFFFF00ull
#define INT_HILO__INIT			0x0000000000000000ull

// Interrupt Routine Select Register @87.A000.0140 [21174 TRM, Section 5.9.4]
#define INT_ROUTE__CSR			0x87A0000140ull
#define INT_ROUTE__BIT7			0x0000000000000080ull
#define INT_ROUTE__BIT6			0x0000000000000040ull
#define INT_ROUTE__BIT5			0x0000000000000020ull
#define INT_ROUTE__BIT4			0x0000000000000010ull
#define INT_ROUTE__BIT3			0x0000000000000008ull
#define INT_ROUTE__BIT2			0x0000000000000004ull
#define INT_ROUTE__BIT1			0x0000000000000002ull
#define INT_ROUTE__BIT0			0x0000000000000001ull
#define INT_ROUTE__MBZ			0xFFFFFFFFFFFFFF00ull
#define INT_ROUTE__INIT			0x0000000000000000ull

// General-Purpose Output Register @87.A000.0180 [21174 TRM, Section 5.9.5]
#define GPO__CSR				0x87A0000180ull
#define GPO__GPO				0xFFFFFFFFFFFFFFFFull
#define GPO__MBZ				0x0000000000000000ull
#define GPO__INIT				0x0000000000000000ull

// Interrupt Configuration Register @87.A000.01C0 [21174 TRM, Section 5.9.6]
#define INT_CNFG__CSR				0x87A00001C0ull
#define INT_CNFG__DRIVE_IRQ			0x00010000ul
#define INT_CNFG__IRQ_CFG_DIVISOR	0x00007800ul
#define INT_CNFG__IRQ_CFG_DELAY		0x00000700ul
#define INT_CNFG__IRQ_COUNT			0x00000070ul
#define INT_CNFG__CLOCK_DIVISOR		0x0000000Ful
#define INT_CNFG__MBZ				0xFFFE8080ul
#define INT_CNFG__INIT				0x00000030ul

// Real-Time Counter Register @87.A000.0200 [21174 TRM, Section 5.9.7]
#define RT_COUNT__CSR		0x87A0000200ull
#define RT_COUNT__RT_COUNT	0xFFFFFFFFFFFFFFFFull
#define RT_COUNT__MBZ		0x0000000000000000ull
#define RT_COUNT__INIT		0x0000000000000000ull

// Interrupt Time Register @87.A000.0240 [21174 TRM, Section 5.9.8]
#define INT_TIME__CSR				0x87A0000240ull
#define INT_TIME__INTERRUPT_TIME	0xFFFFFFFFFFFFFFFFull
#define INT_TIME__MBZ				0x0000000000000000ull
#define INT_TIME__INIT				0x0000000000000000ull

// I2C Control Register @87.A000.02C0 [21174 TRM, Section 5.9.9]
#define IIC_CTRL__CSR				0x87A00002C0ull
#define IIC_CTRL__CLK				0x00000020ul
#define IIC_CTRL__CLK_EN			0x00000010ul
#define IIC_CTRL__DATA				0x00000008ul
#define IIC_CTRL__DATA_EN			0x00000004ul
#define IIC_CTRL__READ_CLK			0x00000002ul
#define IIC_CTRL__READ_DATA			0x00000001ul
#define IIC_CTRL__MBZ				0xFFFFFFC0ul
#define IIC_CTRL__INIT				0x00000000ul

// PCI Sparse I/O space mapping
#define PCI_SIO_HAE_IO_MASK         0xFE000000ul    // <31:25> mask
#define PCI_SIO_PA_ADDR_MASK        0x3FFFFF00ul    // addr_h<29:8> mask
#define PCI_SIO_PA_ADDR_V           5
#define PCI_SIO_ADDR_MASK           0x01FFFFFFul    // addr<24:0> mask
#define PCI_SIO_PA_ENCODE_MASK      0x00000078ul    // addr_h<4:3> mask
#define PCI_SIO_PA_ENCODE_V         3
#define PCI_SIO_ALIGN_MASK          0x00000080ul    // addr_h<7> mask
#define PCI_SIO_ALIGN_V             5

struct pyxis_reg_t { // There are 120 registers

	// General registers [21174 TRM, Sections 4.3, 5.1, 5.2, 5.3 and 5.4]
	uint32 pyxis_rev;
	uint32 pci_lat;
	uint32 pyxis_ctrl;
	uint32 pyxis_ctrl1;
	uint32 flash_ctrl;
	uint32 hae_mem;
	uint32 hae_io;
	uint32 cfg;
	uint32 pyxis_diag;
	uint32 diag_check;
	uint32 perf_monitor;
	uint32 perf_control;
	uint32 pyxis_err;
	uint32 pyxis_stat;
	uint32 err_mask;
	uint32 pyxis_syn;
	t_uint64 pyxis_err_data;
	uint32 mear;
	uint32 mesr;
	uint32 pci_err0;
	uint32 pci_err1;
	uint32 pci_err2;

	// Memory Controller Registers [21174 TRM, Sections 4.4 and 5.5]
	uint32 mcr;
	uint32 mcmr;
	uint32 gtr;
	uint32 rtr;
	uint32 rhpr;
	uint32 mdr1;
	uint32 mdr2;
	uint32 bbar0;
	uint32 bbar1;
	uint32 bbar2;
	uint32 bbar3;
	uint32 bbar4;
	uint32 bbar5;
	uint32 bbar6;
	uint32 bbar7;
	uint32 bcr0;
	uint32 bcr1;
	uint32 bcr2;
	uint32 bcr3;
	uint32 bcr4;
	uint32 bcr5;
	uint32 bcr6;
	uint32 bcr7;
	uint32 btr0;
	uint32 btr1;
	uint32 btr2;
	uint32 btr3;
	uint32 btr4;
	uint32 btr5;
	uint32 btr6;
	uint32 btr7;
	uint32 cvm;

	// PCI Window Control Registers [21174 TRM, Sections 4.5 and 5.6]
	uint32 tbia;
	uint32 w0_base;
	uint32 w0_mask;
	uint32 t0_base;
	uint32 w1_base;
	uint32 w1_mask;
	uint32 t1_base;
	uint32 w2_base;
	uint32 w2_mask;
	uint32 t2_base;
	uint32 w3_base;
	uint32 w3_mask;
	uint32 t3_base;
	uint32 w_dac;

	// Scatter-Gather Address Transalation registers [21174 TRM, Sections 4.6 and 5.7]
	uint32 ltb_tag0;
	uint32 ltb_tag1;
	uint32 ltb_tag2;
	uint32 ltb_tag3;
	uint32 tb_tag4;
	uint32 tb_tag5;
	uint32 tb_tag6;
	uint32 tb_tag7;
	uint32 tb0_page0;
	uint32 tb0_page1;
	uint32 tb0_page2;
	uint32 tb0_page3;
	uint32 tb1_page0;
	uint32 tb1_page1;
	uint32 tb1_page2;
	uint32 tb1_page3;
	uint32 tb2_page0;
	uint32 tb2_page1;
	uint32 tb2_page2;
	uint32 tb2_page3;
	uint32 tb3_page0;
	uint32 tb3_page1;
	uint32 tb3_page2;
	uint32 tb3_page3;
	uint32 tb4_page0;
	uint32 tb4_page1;
	uint32 tb4_page2;
	uint32 tb4_page3;
	uint32 tb5_page0;
	uint32 tb5_page1;
	uint32 tb5_page2;
	uint32 tb5_page3;
	uint32 tb6_page0;
	uint32 tb6_page1;
	uint32 tb6_page2;
	uint32 tb6_page3;
	uint32 tb7_page0;
	uint32 tb7_page1;
	uint32 tb7_page2;
	uint32 tb7_page3;

	// Miscellaneous Registers [21174 TRM, Sections 4.7 and 5.8]
	uint32 ccr;
	uint32 clk_stat;
	uint32 reset;

	// Interrupt Control Registers [21174 TRM, Sections 4.8 and 5.9]
	t_uint64 int_req;
	t_uint64 int_mask;
	t_uint64 int_hilo;
	t_uint64 int_route;
	t_uint64 gpo;
	uint32 int_cnfg;
	t_uint64 rt_count;
	t_uint64 int_time;
	uint32 iic_ctrl;
};

// Debug flag
#define DBG_IO   0x0001
#define DBG_WARN 0x0002

#endif // ifndef __alpha_pyxis_h