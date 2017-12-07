/* alpha_super_io.c: Alpha Sidewinder Lite PC87303VUL logic chip

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

#include "alpha_defs.h"
#include "alpha_sys_defs.h"
//#include "sim_isa.h"
#include "alpha_super_io.h"

struct super_io_t super_io;

// The Super IO has no units - all chip components are individual devices
UNIT super_io_unit[] = {
	{ NULL }
    };

// Super I/O registers
REG super_io_reg[] = {
	{ HRDATAD (INDX,   super_io.index,      8, "Index (byte offset) containing internal register number") },
	{ HRDATAD (DATA,   super_io.data,       8, "Data to be read or written to the indexed register") },
	{ HRDATAD (INDX_A, super_io.index_addr, 16, "Index Register address {[398],26E,15C,2E}" ) },
	{ HRDATAD (DATA_A, super_io.data_addr,  16, "Data  Register address {[399],26F,15D,2F}") },
	{ HRDATAD (FER,    super_io.reg[0x0],	8, "Function Enable Register (FER)") },
	{ HRDATAD (FAR,    super_io.reg[0x1],	8, "Function Address Register (FAR)") },
	{ HRDATAD (PTR,    super_io.reg[0x2],	8, "Power & Test Register (PTR)") },
	{ HRDATAD (FCR,    super_io.reg[0x3],	8, "Function Control Register (FCR)") },
	{ HRDATAD (PCR,    super_io.reg[0x4],	8, "Printer Control Register (PCR)") },
	{ HRDATAD (KRR,    super_io.reg[0x5],	8, "Keyboard and RTC Control Register (KRR)") },
	{ HRDATAD (PMC,    super_io.reg[0x6],	8, "Power management Control Register (PMC)") },
	{ HRDATAD (TUP,    super_io.reg[0x7],	8, "Tape, UARTs & Parallel Port Register (TUP)") },
	{ HRDATAD (SID,    super_io.reg[0x8],	8, "Super I/O Identification (SID)") },
	{ HRDATAD (ASC,    super_io.reg[0x9],	8, "Advanced SIO Configuration Register (ASC)") },
	{ HRDATAD (CS0CF0, super_io.reg[0xA],	8, "Chip Select 0 Configuration Register 0 (CS0CF0)") },
	{ HRDATAD (CS0CF1, super_io.reg[0xB],	8, "Chip Select 0 Configuration Register 1 (CS0CF1)") },
	{ HRDATAD (CS1CF0, super_io.reg[0xC],	8, "Chip Select 1 Configuration Register 0 (CS1CF0)") },
	{ HRDATAD (CS1CF1, super_io.reg[0xD],	8, "Chip Select 1 Configuration Register 1 (CS1CF1)") },
    { NULL }
    };

// DEVICE function forwards
t_stat super_io_reset (DEVICE*);

// Super I/O device
DEVICE super_io_dev = {
    "SIO", super_io_unit, super_io_reg, NULL,
    1, 16, 32, 1, 16, 8,
    NULL, NULL, &super_io_reset,
    NULL, NULL, NULL,
    NULL/*dib*/, 0, 0, NULL, NULL, NULL, NULL/*&clk_help*/, NULL, NULL, 
    NULL/*&clk_description*/
};

t_stat super_io_reset (DEVICE* dptr)
{
	// set index/data address
	super_io.index_addr = 0x398;	// primary I/O address [table 2-2]
	super_io.data_addr =  0x399;	// primary I/O address [table 2-2]
	// initialize configuration registers
	super_io.reg[0x0] = SUPER_IO__REG0_INIT;
	super_io.reg[0x1] = SUPER_IO__REG1_INIT;
	super_io.reg[0x2] = SUPER_IO__REG2_INIT;
	super_io.reg[0x3] = SUPER_IO__REG3_INIT;
	super_io.reg[0x4] = SUPER_IO__REG4_INIT;
	super_io.reg[0x5] = SUPER_IO__REG5_INIT;
	super_io.reg[0x6] = SUPER_IO__REG6_INIT;
	super_io.reg[0x7] = SUPER_IO__REG7_INIT;
	super_io.reg[0x8] = SUPER_IO__REG8_INIT;
	super_io.reg[0x9] = SUPER_IO__REG9_INIT;
	super_io.reg[0xA] = SUPER_IO__REGA_INIT;
	super_io.reg[0xB] = SUPER_IO__REGB_INIT;
	super_io.reg[0xC] = SUPER_IO__REGC_INIT;
	super_io.reg[0xD] = SUPER_IO__REGD_INIT;
	// reset index pre-read identifier count; this is used for index and data register identification
	super_io.index_id_read = 2;

	// Register (E)ISA I/O addresses used by this controller for I/O dispatching:
	//   register_eisa (&eisa_bus, &eisa_device, io_range_low, io_range_high)

	//register_eisa (&eisa_bus, &super_io_eisa,	super_io.index_addr,		super_io.data_addr);
	//register_eisa (&eisa_bus, &fdc_eisa,		super_io.fdc_addr_low,		super_io.fdc_addr_high);
	//register_eisa (&eisa_bus, &uart1_eisa,	super_io.uart1_addr_low,	super_io.uart1_addr_high);
	//register_eisa (&eisa_bus, &uart2_eisa,	super_io.uart2_addr_low,	super_io.uart2_addr_high);
	//register_eisa (&eisa_bus, &lpt_eisa,		super_io.lpt_addr_low,		super_io.lpt_addr_high);

	return SCPE_OK;
}

t_stat super_io_readPA (t_uint64 pa, int size, t_uint64* data)
{
	if (pa == super_io.index_addr) {
		switch (super_io.index_id_read) {
		case 2:
			*data = 0x88;	// identification read #1 [sec 2.2, step 1]
			super_io.index_id_read -= 1;
			break;
		case 1:
			*data = 0x00;	// identification read #2 [sec 2.2, step 1]
			super_io.index_id_read -= 1;
			break;
		case 0:				// normal register read
			*data = super_io.index;
			break;
		} // switch (super_io.index_id_read)
	} else if (pa == super_io.data_addr) {
		*data = super_io.data;
	}
    sim_debug (DBG_REG, &super_io_dev, "super_io_readPA: addr=%x, value=%x\n", pa, *data);
	return SCPE_OK;
}

t_stat super_io_writePA (t_uint64 pa, int size, t_uint64 data)
{
    sim_debug (DBG_REG, &super_io_dev, "super_io_writePA: addr=%x, value=%x\n", pa, data);
	if (pa == super_io.index_addr) {
		super_io.index = (data & 0xFF);					// write index register
		super_io.data = super_io.reg[super_io.index];	// fetch matching data register for next data read
		return SCPE_OK;
	}
	if (pa == super_io.data_addr) {
		int old_value = super_io.reg[super_io.index];	// Save old value of indexed register
		super_io.reg[super_io.index] = (data & 0xFF);
		switch (super_io.index) {
		// may need to dispatch side effects of indexed register writes here
		default:
			break;
		} // switch (super_io.index)
		return SCPE_OK;
	}
	return SCPE_INCOMP;		// register not found
}

//==============================================================================

UNIT null_units[] = {
    { NULL }
};

// uart function forwards
t_bool uart_readPA  (UART* uart, t_uint64 pa, t_uint64* value, uint32 size);
t_bool uart_writePA (UART* uart, t_uint64 pa, t_uint64  value, uint32 size);

DEBTAB uart_debug[] = {
  {"REG",  DBG_REG,   "watch registers"},
  {"IO",   DBG_IO,    "watch I/O address read/write"},
  {"WARN", DBG_WRN,   "display warnings"},
  {0}
};

UART uart1;

t_bool uart1_readPA (t_uint64 pa, t_uint64* value, uint32 size)
{
    return uart_readPA (&uart1, pa, value, size);
}

t_bool uart1_writePA (t_uint64 pa, t_uint64 value, uint32 size)
{
    return uart_writePA (&uart1, pa, value, size);
}

REG uart1_reg[] = {
    { HRDATAD (RBR, uart1.reg.rbr, 8, "Receiver Buffer Register (Read Only)") },
    { HRDATAD (THR, uart1.reg.thr, 8, "Transmitter Holding Register (Write Only)") },
    { HRDATAD (IER, uart1.reg.ier, 8, "Interrupt Enable register") },
    { HRDATAD (IIR, uart1.reg.iir, 8, "Interrupt Indentification Register (Read Only)") },
    { HRDATAD (FCR, uart1.reg.fcr, 8, "FIFO COntrol Register (Write Only)") },
    { HRDATAD (LCR, uart1.reg.lcr, 8, "Line Control Register") },
    { HRDATAD (MCR, uart1.reg.mcr, 8, "Modem Control Register") },
    { HRDATAD (LSR, uart1.reg.lsr, 8, "Line Status register") },
    { HRDATAD (MSR, uart1.reg.msr, 8, "Modem Status Register") },
    { HRDATAD (SCR, uart1.reg.scr, 8, "Scratch Pad Register") },
    { HRDATAD (DLL, uart1.reg.dll, 8, "Divisor Latch (LSB)") },
    { HRDATAD (DLM, uart1.reg.dlm, 8, "Divisor Latch (MSB)") },
    { NULL }
};

DIB uart1_dib = {
    0x3F8,          // low
    0x3FF,          // high
    &uart1_readPA,  // read
    &uart1_writePA, // write
    IPL_HMIN        // ipl
};

// UART1 -> COM1 (serial console)
DEVICE uart1_dev = {
    "UART1",    // name
    null_units, // units
    uart1_reg,  // registers
    NULL,       // modifiers
    1,          // numunits
    16,         // aradix
    T_ADDR_W,   // awidth
    2,          // aincr
    16,         // dradix
    16,         // dwidth
    NULL,       // examine
    NULL,       // deposit
    NULL,       // reset
    NULL,       // boot
    NULL,       // attach
    NULL,       // detach
    &uart1_dib, // context
    DEV_EISA | DEV_DEBUG,   // flags
    0,          // dctrl
    uart_debug, // debflags
    NULL,       // msize
    NULL,       // lname
    NULL,       // help
    NULL,       // attach_help
    NULL,       // help_ctx
    NULL        // description
};

UART uart2;

t_bool uart2_readPA (t_uint64 pa, t_uint64* value, uint32 size)
{
    return uart_readPA (&uart2, pa, value, size);
}

t_bool uart2_writePA (t_uint64 pa, t_uint64 value, uint32 size)
{
    return uart_writePA (&uart2, pa, value, size);
}

REG uart2_reg[] = {
    { HRDATAD (RBR, uart2.reg.rbr, 8, "Receiver Buffer Register (Read Only)") },
    { HRDATAD (THR, uart2.reg.thr, 8, "Transmitter Holding Register (Write Only)") },
    { HRDATAD (IER, uart2.reg.ier, 8, "Interrupt Enable register") },
    { HRDATAD (IIR, uart2.reg.iir, 8, "Interrupt Indentification Register (Read Only)") },
    { HRDATAD (FCR, uart2.reg.fcr, 8, "FIFO COntrol Register (Write Only)") },
    { HRDATAD (LCR, uart2.reg.lcr, 8, "Line Control Register") },
    { HRDATAD (MCR, uart2.reg.mcr, 8, "Modem Control Register") },
    { HRDATAD (LSR, uart2.reg.lsr, 8, "Line Status register") },
    { HRDATAD (MSR, uart2.reg.msr, 8, "Modem Status Register") },
    { HRDATAD (SCR, uart2.reg.scr, 8, "Scratch Pad Register") },
    { HRDATAD (DLL, uart2.reg.dll, 8, "Divisor Latch (LSB)") },
    { HRDATAD (DLM, uart2.reg.dlm, 8, "Divisor Latch (MSB)") },
    { NULL }
};

// UART2 -> COM2
DEVICE uart2_dev = {
    "UART2",    // name
    null_units, // units
    uart2_reg,  // registers
    NULL,       // modifiers
    1,          // numunits
    16,         // aradix
    T_ADDR_W,   // awidth
    2,          // aincr
    16,         // dradix
    16,         // dwidth
    NULL,       // examine
    NULL,       // deposit
    NULL,       // reset
    NULL,       // boot
    NULL,       // attach
    NULL,       // detach
    NULL,       // context
    DEV_EISA /*| DEV_DISABLE */| DEV_DIS | DEV_DEBUG ,          // flags
    0,          // dctrl
    NULL,       // debflags
    NULL,       // msize
    NULL,       // lname
    NULL,       // help
    NULL,       // attach_help
    NULL,       // help_ctx
    NULL        // description
    };

t_bool uart_readPA (UART* uart, t_uint64 pa, t_uint64* value, uint32 size)
{
	uint8 offset = pa & 0x7;		// lowest three bits determines register number
	switch (offset) {
	case 0:
		if (uart->reg.lcr & UART__LCR_DLAB) {	// DLAB set
			*value = uart->reg.dll;	// divisor latch LSB
		} else {
			if (uart->ibuff.pos == uart->ibuff.end) {
				*value = 0;
			} else {
				*value = uart->ibuff.buffer[uart->ibuff.pos++];	// get curent char
                uart->reg.lsr |= ~ UART__LSR_DR;                // clear Data Ready bit

				if (uart->ibuff.pos != uart->ibuff.end) {       // more characters waiting?
					// more characters to process; set DR, and interrupt flag, and interrupt
				}
			}
		}
		break;
	case 1:
		if (uart->reg.lcr & UART__LCR_DLAB) {
			*value = uart->reg.dlm;	// divisor latch MSB
		} else {
			*value = uart->reg.ier;
		}
		break;
	case 2:
		*value = uart->reg.iir;
		break;
	case 3:
		*value = uart->reg.fcr;
		break;
    case 4:
        *value = uart->reg.mcr;
        break;
    case 5:
        *value = uart->reg.lsr;
        break;
    case 6:
        *value = uart->reg.msr;
        break;
    case 7:
        *value = uart->reg.scr;
        break;
    default:    // can't happen; entire range is covered
        break;
	}
//    sim_debug (DBG_REG, dev, "uart_readPA:: device=%s, read reg[%s], value=%x\n", dev->name, uart_regname[offset], *value);
    return TRUE;
}

t_stat uart_writePA (UART* uart, t_uint64 pa, t_uint64 value, uint32 size)
{
	uint8 offset = pa & 0x7;		// lowest three bits determines register number
	switch (offset) {
    case 0:
        if (uart->reg.lcr & UART__LCR_DLAB) {
            uart->reg.dll = value & 0xFF;
        } else {
            int32 c = value & 0xFF;
            t_stat r;
            uart->reg.thr = c;  // write char to register to aid debugging
            // need to write the byte to the terminal here
            //>>> schedule via sim_timer() or direct output? direct output for now..
            uart->obuff.buffer[++uart->obuff.end] = c; // buffer output character
            r = sim_putchar_s (c);  // direct output character to terminal
        }
        break;
    case 1:
        if (uart->reg.lcr & UART__LCR_DLAB) {
            uart->reg.dlm = value & 0xFF;
        } else {
            uart->reg.ier = value & 0x0F;
        }
        break;
    case 2:
        uart->reg.fcr = value & 0xFF;
        break;
    case 3:
        uart->reg.lcr = value & 0xFF;
        break;
    case 4:
        uart->reg.mcr = value & 0xFF;
        break;
    case 5:
        uart->reg.lsr = value & 0xFF;
        break;
    case 6:
        uart->reg.msr = value & 0xFF;
        break;
    case 7:
        uart->reg.scr = value & 0xFF;
        break;
    default:    // can't happen; entire range is covered
        break;
    }
   return TRUE;
}




//==============================================================================
//                          IEEE 1284 Parallel Port
//==============================================================================

struct lpt_ieee1284_t lpt;

REG lpt_reg[] = {
    { NULL }
};

t_stat lpt_reset (DEVICE* dev)
{
    lpt.address = 0x3BC;        // I/O base address (3bc = LPTA)
    lpt.dtr = LPT__DTR_INIT;
    lpt.str = LPT__STR_INIT;
    lpt.ctr = LPT__CTR_INIT;
    lpt.addr = LPT__ADDR_INIT;
    lpt.dp0 = LPT__DP0_INIT;
    lpt.dp1 = LPT__DP1_INIT;
    lpt.dp2 = LPT__DP2_INIT;
    lpt.dp3 = LPT__DP3_INIT;

    // unfinished?

    return SCPE_OK;
}

t_bool lpt_read (DEVICE* dev, t_uint64 pa, t_uint64* value, uint32 len)
{
    if ( ((lpt.address == LPT__LPTA_ADDRESS) && ((pa & ~0x3ull) == lpt.address)) ||
         ((lpt.address != LPT__LPTA_ADDRESS) && ((pa & ~0x7ull) == lpt.address)) )
    {
        switch (pa & 0x7) {
        case 0:
            *value = lpt.dtr;
            break;
        case 1:
            *value = lpt.str;
            break;
        case 2:
            *value = lpt.ctr;
            break;
        case 3:
            *value = lpt.addr;
            break;
        case 4:
            *value = lpt.dp0;
            break;
        case 5:
            *value = lpt.dp1;
            break;
        case 6:
            *value = lpt.dp2;
            break;
        case 7:
            *value = lpt.dp3;
            break;
        default:
            break;
        } // switch (pa & 0x7)
        sim_debug (DBG_REG, dev, "lpt_read: device %s register[%x] = %x\n", dev->name, (pa & 0x7), (int)*value);
    } else {
        sim_debug (DBG_IO, dev, "lpt_read: device %s dispatching invalid address %x\n", dev->name, (int) pa);
        return FALSE;
    }
    return TRUE;
}

t_bool lpt_write (DEVICE* dev, t_uint64 pa, t_uint64 value, uint32 len)
{
    if ((pa & ~0x7ull) == lpt.address) {
        switch (pa & 0x7) {
        case 0:
            lpt.dtr = value & 0x7;
            break;
        case 1:
            //lpt.str = value & 0x7; // STR is Read Only
            break;
        case 2:
            lpt.ctr = value & 0x7;
            break;
        case 3:
            lpt.addr = value & 0x7;
            break;
        case 4:
            lpt.dp0 = value & 0x7;
            break;
        case 5:
            lpt.dp1 = value & 0x7;
            break;
        case 6:
            lpt.dp2 = value & 0x7;
            break;
        case 7:
            lpt.dp3 = value & 0x7;
            break;
        default:
            break;
        } // switch (pa & 0x7)
        sim_debug (DBG_REG, dev, "lpt_write: device %s register[%x] = %x\n", dev->name, (pa & 0x7), (value & 0x7));
    } else {
        sim_debug (DBG_IO, dev, "lpt_write: device %s dispatching invalid address %x\n", dev->name, (int) pa);
        return FALSE;
    }
    return TRUE;
}

// LPT -> LPTA
DEVICE lpt_dev = {
    "LPT",      // name
    null_units, // units
    NULL,       // registers
    NULL,       // modifiers
    1,          // numunits
    16,         // aradix
    64,         // awidth
    2,          // aincr
    16,         // dradix
    16,         // dwidth
    NULL,       // examine routine
    NULL,       // deposit routine
    lpt_reset,  // reset routine
    NULL,       // boot routine
    NULL,       // attach routine
    NULL,       // detach routine
    0,          // ctxt
    DEV_DIS,    // flags
    0,          // dctrl
    NULL,       // debflags
    NULL,       // msize routine
    NULL,       // lname
    NULL,       // help routine
    NULL,       // attach help routine
    NULL,       // help ctx
    NULL        // description routine
};

/*
================================================================================
                      Real Time Clock (RTC)
================================================================================
*/

struct rtc_ds12887_t {
    uint8   indx;
    uint8   data;
    uint8   reg[256];
} rtc;

DEBTAB rtc_debug[] = {
  {"REG",  DBG_REG,   "watch registers"},
  {"IO",   DBG_IO,    "watch I/O address read/write"},
  {"WARN", DBG_WRN,   "display warnings"},
  {0}
};

/* forwards */
t_stat rtc_reset (DEVICE* dev);
t_bool rtc_read  (t_uint64 pa, t_uint64* data, uint32 size);
t_bool rtc_write (t_uint64 pa, t_uint64  data, uint32 size);

DIB rtc_dib = {
    0x70,       /* low */
    0x71,       /* high */
    rtc_read,   /* read routine */
    rtc_write,  /* write routine */
    0           /* ipl */
};

DEVICE rtc_dev = {
    "RTC",      // name
    null_units, // units
    NULL,       // registers
    NULL,       // modifiers
    1,          // numunits
    16,         // aradix
    64,         // awidth
    2,          // aincr
    16,         // dradix
    16,         // dwidth
    NULL,       // examine routine
    NULL,       // deposit routine
    rtc_reset,  // reset routine
    NULL,       // boot routine
    NULL,       // attach routine
    NULL,       // detach routine
    &rtc_dib,    // ctxt
    DEV_DEBUG,  // flags
    0,          // dctrl
    rtc_debug,  // debflags
    NULL,       // msize routine
    NULL,       // lname
    NULL,       // help routine
    NULL,       // attach help routine
    NULL,       // help ctx
    NULL        // description routine
};

t_stat rtc_reset (DEVICE* dev)
{
    //eisa_register (&eisa_bus, dev, 0x70, 0x71);
    return SCPE_OK;
}

t_bool rtc_read (t_uint64 pa, t_uint64* value, uint32 size)
{
    sim_debug (DBG_IO, &rtc_dev, "rtc_readPA: read I/O address %x\n",pa);
    switch (pa) {
    case 0x70:
        *value = rtc.indx;
        break;
    case 0x71:
        *value = rtc.data;
        sim_debug (DBG_REG, &rtc_dev, "rtc_readPA: register[%x]=%x\n", rtc.indx, *value);
        break;
    default:
        sim_debug (DBG_WRN, &rtc_dev, "rtc_readPA: dispatching invalid i/o address %x\n", pa);
        *value = 0;
        return FALSE;
        break;
    };
    return TRUE;
}

t_bool rtc_write (t_uint64 pa, t_uint64 value, uint32 size)
{
    uint8 old;
    sim_debug (DBG_IO, &rtc_dev, "rtc_writePA: write I/O address %x\n",pa);
    
    switch (pa) {
    case 0x70:
        rtc.indx = value & 0xFF;
        rtc.data = rtc.reg[rtc.indx];   // fetch data for this index;
        break;
    case 0x71:
        old = rtc.reg[rtc.indx];
        switch (rtc.indx) {
        case 0x0A:
            // bit 7 is RO
            rtc.reg[0x0A] = (rtc.reg[0x0A] & 0x80) | (value & 0x7F);
            break;
        case 0x0B:
            // bit 3 is RO
            rtc.reg[0x0B] = (rtc.reg[0x0B] & 0x08) | (value & 0xF7);
            break;
        case 0x0C:
        case 0x0D:
            // 0x0C and 0x0D are read-only registers
            break;
        default:
            rtc.data = value & 0xFF;
            break;
        }; // switch (rtc.indx)
        rtc.data = rtc.reg[rtc.indx];   // reset data value
        sim_debug (DBG_REG, &rtc_dev, "rtc_writePA: register[%x] changed from %x to %x\n", rtc.indx, old, rtc.data);
        break;
    default:
        sim_debug (DBG_WRN, &rtc_dev, "rtc_writePA: dispatching invalid i/o address %x\n", pa);
        return FALSE;
        break;
    };
    return TRUE;
}

