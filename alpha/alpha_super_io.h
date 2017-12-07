/* alpha_super_io.h: Alpha Sidewinder Lite PC87303VUL logic chip

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
	The PC87303VUL logic chip implements most of the standard ISA PC devices:
		Floppy Disk Controller (FDC)
		Keyboard Controller
		Real-Time Clock
		Dual UARTs (NS16450 and PC16550A compatible)
		IEEE 1284 Parallel port
		IDE interface

	Documentation:
		"National Semiconducter PC87303VUL", RRD-B30M75, February 1995

*/

/*
	History:
	--------
	2015-08-03  DTH  Started
*/

#ifndef __alpha_super_io_h
#define __alpha_super_io_h

#include "sim_defs.h"

#define SUPER_IO__REG0_PARALLEL_ENABLE	0x01
#define SUPER_IO__REG0_UART1_ENABLE		0x02
#define SUPER_IO__REG0_UART2_ENABLE		0x04
#define SUPER_IO__REG0_FDC_ENABLE		0x09
#define SUPER_IO__REG0_FDC4_ENCODE		0x10
#define SUPER_IO__REG0_FDC_SECONDARY	0x20
#define SUPER_IO__REG0_IDE_ENABLE		0x40
#define SUPER_IO__REG0_IDE_SECONDARY	0x80
#define SUPER_IO__REG0_INIT				0x07    // UART1,UART2,LPT		

#define SUPER_IO__REG1_PARALLEL_ADDR	0x03
#define SUPER_IO__REG1_UART1_ADDR		0x0C
#define SUPER_IO__REG1_UART2_ADDR		0x30
#define SUPER_IO__REG1_SEL_COM34		0xC0
#define SUPER_IO__REG1_INIT				0x11    // COM1, COM2, LPTA

#define SUPER_IO__REG2_POWER_DOWN		0x01
#define SUPER_IO__REG2_CLK_POWER_DOWN	0x02
#define SUPER_IO__REG2_CSOUT_SELECT		0x04
#define SUPER_IO__REG2_SELECT_IRQ75		0x08
#define SUPER_IO__REG2_UART1_TEST		0x10
#define SUPER_IO__REG2_UART2_TEST		0x20
#define SUPER_IO__REG2_LOCK_CFG			0x40
#define SUPER_IO__REG2_ECP_SELECT		0x80
#define SUPER_IO__REG2_INIT				0x00

#define SUPER_IO__REG3_MEDIA_SENSE_SEL	0x01
#define SUPER_IO__REG3_IDENT_ACK_SEL	0x02
#define SUPER_IO__REG3_PARALLEL_FLOAT	0x08
#define SUPER_IO__REG3_LOG_DRV_EXCHG	0x10
#define SUPER_IO__REG3_ZERO_WAIT		0x20
#define SUPER_IO__REG3_MBZ				0xC4
#define	SUPER_IO__REG3_INIT				0x00

#define SUPER_IO__REG4_EPP_ENABLE		0x01
#define SUPER_IO__REG4_EPP_VER_SEL		0x02
#define SUPER_IO__REG4_ECP_ENABLE		0x04
#define SUPER_IO__REG4_ECP_CLK_FREEZE	0x08
#define SUPER_IO__REG4_INT_POLARITY		0x20
#define SUPER_IO__REG4_INT_IO_CTRL		0x40
#define SUPER_IO__REG4_RTC_RAM_MASK		0x80
#define SUPER_IO__REG4_MBZ				0x10
#define SUPER_IO__REG4_INIT				0x00

#define SUPER_IO__REG5_KBC_ENABLE		0x01
#define SUPER_IO__REG5_KBC_SPEED_CTL	0x02
#define SUPER_IO__REG5_PROG_ACC_ENABLE	0x04
#define SUPER_IO__REG5_RTC_ENABLE		0x08
#define SUPER_IO__REG5_RTC_CLKTST_SEL	0x10
#define SUPER_IO__REG5_RAWSEL			0x20
#define SUPER_IO__REG5_CHIP_SEL_ENABLE	0x40
#define SUPER_IO__REG5_KBC_CLK_SRC_SEL	0x80
#define SUPER_IO__REG5_INIT				0x01

#define SUPER_IO__REG6_IDE_TRI_CTRL		0x01
#define SUPER_IO__REG6_FDC_TRI_CTRL		0x02
#define SUPER_IO__REG6_UART_TRI_CTRL	0x04
#define SUPER_IO__REG6_FP_TRI_CTRL		0x40
#define SUPER_IO__REG6_MBZ				0xB8
#define SUPER_IO__REG6_INIT				0x00

#define SUPER_IO__REG7_EPP_TMO_INT_ENA	0x04
#define SUPER_IO__REG7_MBZ				0xFB
#define SUPER_IO__REG7_INIT				0x00

#define SUPER_IO__REG8_IDENT			0x30
#define SUPER_IO__REG8_INIT				0x30

#define SUPER_IO__REG9_IRQ5_DRATE_SEL	0x01
#define SUPER_IO__REG9_DRV_23_SEL		0x02
#define SUPER_IO__REG9_ENH_TDR_SUPP		0x04
#define SUPER_IO__REG9_ECP_CNFGA_BIT3	0x20
#define SUPER_IO__REG9_SYS_OP_MODE		0xC0
#define SUPER_IO__REG9_INIT				0xC0

#define SUPER_IO__REGA_LA0				0x01
#define SUPER_IO__REGA_LA1				0x02
#define SUPER_IO__REGA_LA2				0x04
#define SUPER_IO__REGA_LA3				0x08
#define SUPER_IO__REGA_LA4				0x10
#define SUPER_IO__REGA_LA5				0x20
#define SUPER_IO__REGA_LA6				0x40
#define SUPER_IO__REGA_LA7				0x80
#define SUPER_IO__REGA_INIT				0x00

#define SUPER_IO__REGB_HA8				0x01
#define SUPER_IO__REGB_HA9				0x02
#define SUPER_IO__REGB_HA10				0x04
#define SUPER_IO__REGB_ENA_CS0_WRITE	0x10
#define SUPER_IO__REGB_ENA_CS0_READ		0x20
#define SUPER_IO__REGB_ENA_FULL_ADDR	0x40
#define SUPER_IO__REGB_CS0_SEL_PIN		0x80
#define SUPER_IO__REGB_MBZ				0x08
#define SUPER_IO__REGB_INIT				0x00

#define SUPER_IO__REGC_LA0				0x01
#define SUPER_IO__REGC_LA1				0x02
#define SUPER_IO__REGC_LA2				0x04
#define SUPER_IO__REGC_LA3				0x08
#define SUPER_IO__REGC_LA4				0x10
#define SUPER_IO__REGC_LA5				0x20
#define SUPER_IO__REGC_LA6				0x40
#define SUPER_IO__REGC_LA7				0x80
#define SUPER_IO__REGC_INIT				0x00

#define SUPER_IO__REGD_HA8				0x01
#define SUPER_IO__REGD_HA9				0x02
#define SUPER_IO__REGD_HA10				0x04
#define SUPER_IO__REGD_ENA_CS0_WRITE	0x10
#define SUPER_IO__REGD_ENA_CS0_READ		0x20
#define SUPER_IO__REGD_ENA_FULL_ADDR	0x40
#define SUPER_IO__REGD_CS0_SEL_PIN		0x80
#define SUPER_IO__REGD_MBZ				0x08
#define SUPER_IO__REGD_INIT				0x00

struct super_io_t {
	int	index_addr;					// ISA I/O address of index register
	int data_addr;					// ISA I/O address of data register
	int	index_id_read;				// Identification read countdown
	uint8 index;					// index register
	uint8 data;						// data register
	uint8 reg[16];					// registers pointed to by index + 2 extra unused
};

#define LPT__LPTA_ADDRESS           0x3BC
#define LPT__LPTB_ADDRESS           0x378
#define LPT__LPTC_ADDRESS           0x278

#define LPT__DTR_INIT               0x00
#define LPT__STR_INIT               0x07
#define LPT__CTR_INIT               0xC0
#define LPT__ADDR_INIT              0x00
#define LPT__DP0_INIT               0x00
#define LPT__DP1_INIT               0x00
#define LPT__DP2_INIT               0x00
#define LPT__DP3_INIT               0x00

struct lpt_ieee1284_t {
    uint32 address;             // I/O base address
                                // standard registers
    uint8 dtr;                      // data register
    uint8 str;                      // status register
    uint8 ctr;                      // control register
    // LPTA does not have EPP/ECP registers due to 3BC address [ref: 7.5]
                                // EPP registers
    uint8 addr;                     // address register
    uint8 dp0;                      // data port 0
    uint8 dp1;                      // data port 1
    uint8 dp2;                      // data port 2
    uint8 dp3;                      // data port 3
                                // ECP registers
    uint8 fifo;                     // CFIFO, DFIFO, TFIFO, CNFGA shared register
    uint8 cnfgb;                    // Configuration Register B
    uint8 ecr;                      // Extended Control Register (ECR)
};

#define UART__LCR_DLAB 0x80

#define UART__LSR_DR    0x01        // Data Ready

struct uart_ns16550_t {
	struct registers_t {
		uint8	rbr;				// Receive Buffer Register
		uint8	thr;				// Transmit Holding Register
		uint8	ier;				// Interrrupt Enable Register
		uint8	iir;				// Interrupt Intentification Register
		uint8	fcr;				// FIFO Control Register
		uint8	lcr;				// Line COntrol Register
		uint8	mcr;				// Modem Control Register
		uint8	lsr;				// Line Status Register
		uint8	msr;				// Modem Status Register
		uint8	scr;				// Scratch Pad Register
		uint8	dll;				// Divisor Latch LSB
		uint8	dlm;				// Divisor Latch MSB
	} reg;
	struct buffer_t {
		uint8 buffer[256];			// buffer
		uint8 pos;					// buffer position
		uint8 end;					// buffer end
	} ibuff, obuff;					// input and ouput buffers
};
typedef struct uart_ns16550_t UART;

/* debugging bitmaps */
#define DBG_REG  0x0001
#define DBG_WRN  0x0002
#define DBG_IO   0X0004

#endif // __alpha_super_io_h