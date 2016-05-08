/*************************************************************************
 *                                                                       *
 * $Id: s100_adcs6.c 1970 2008-06-26 06:01:27Z hharte $                  *
 *                                                                       *
 * Copyright (c) 2007-2008 Howard M. Harte.                              *
 * http://www.hartetec.com                                               *
 *                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining *
 * a copy of this software and associated documentation files (the       *
 * "Software"), to deal in the Software without restriction, including   *
 * without limitation the rights to use, copy, modify, merge, publish,   *
 * distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to *
 * the following conditions:                                             *
 *                                                                       *
 * The above copyright notice and this permission notice shall be        *
 * included in all copies or substantial portions of the Software.       *
 *                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       *
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND                 *
 * NONINFRINGEMENT. IN NO EVENT SHALL HOWARD M. HARTE BE LIABLE FOR ANY  *
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  *
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     *
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                *
 *                                                                       *
 * Except as contained in this notice, the name of Howard M. Harte shall *
 * not be used in advertising or otherwise to promote the sale, use or   *
 * other dealings in this Software without prior written authorization   *
 * Howard M. Harte.                                                      *
 *                                                                       *
 * SIMH Interface based on altairz80_hdsk.c, by Peter Schorn.            *
 *                                                                       *
 * Module Description:                                                   *
 *     Advanced Digital Corporation (ADC) Super-Six CPU Board            *
 * module for SIMH.                                                      *
 *                                                                       *
 * This module is a wrapper around the wd179x FDC module, and adds the   *
 * ADC-specific registers as well as the Digitex Monitor Boot ROM.       *
 *                                                                       *
 * Environment:                                                          *
 *     User mode only                                                    *
 *                                                                       *
 *************************************************************************/

/*#define DBG_MSG */
#include "altairz80_defs.h"

#if defined (_WIN32)
#include <windows.h>
#endif

#include "sim_defs.h"   /* simulator definitions */
#include "wd179x.h"

#ifdef DBG_MSG
#define DBG_PRINT(args) sim_printf args
#else
#define DBG_PRINT(args)
#endif

/* Debug flags */
#define ERROR_MSG   (1 << 0)
#define DRIVE_MSG   (1 << 1)
#define VERBOSE_MSG (1 << 2)
#define DMA_MSG     (1 << 3)

#define ADCS6_MAX_DRIVES    4
#define ADCS6_ROM_SIZE      (2 * 1024)
#define ADCS6_ADDR_MASK     (ADCS6_ROM_SIZE - 1)

typedef struct {
    PNP_INFO    pnp;    /* Plug and Play */
    uint32 dma_addr;    /* DMA Transfer Address */
    uint8 rom_disabled; /* TRUE if ROM has been disabled */
    uint8 head_sel;
    uint8 autowait;
    uint8 rtc;
    uint8 imask;        /* Interrupt Mask Register */
    uint8 ipend;        /* Interrupt Pending Register */
    uint8 s100_addr_u;  /* A23:16 of S-100 bus */
} ADCS6_INFO;

extern WD179X_INFO_PUB *wd179x_infop;

static ADCS6_INFO adcs6_info_data = { { 0xF000, ADCS6_ROM_SIZE, 0x3, 2 } };
static ADCS6_INFO *adcs6_info = &adcs6_info_data;

extern t_stat set_membase(UNIT *uptr, int32 val, char *cptr, void *desc);
extern t_stat show_membase(FILE *st, UNIT *uptr, int32 val, void *desc);
extern t_stat set_iobase(UNIT *uptr, int32 val, char *cptr, void *desc);
extern t_stat show_iobase(FILE *st, UNIT *uptr, int32 val, void *desc);
extern uint32 sim_map_resource(uint32 baseaddr, uint32 size, uint32 resource_type,
        int32 (*routine)(const int32, const int32, const int32), uint8 unmap);

static t_stat adcs6_svc (UNIT *uptr);

extern uint32 PCX;      /* external view of PC  */

#define UNIT_V_ADCS6_ROM        (UNIT_V_UF + 2) /* boot ROM enabled                         */
#define UNIT_ADCS6_ROM          (1 << UNIT_V_ADCS6_ROM)
#define ADCS6_CAPACITY          (77*2*16*256)   /* Default Micropolis Disk Capacity         */

#define MOTOR_TO_LIMIT          128

static t_stat adcs6_reset(DEVICE *adcs6_dev);
static t_stat adcs6_boot(int32 unitno, DEVICE *dptr);
static t_stat adcs6_attach(UNIT *uptr, char *cptr);
static t_stat adcs6_detach(UNIT *uptr);

static int32 adcs6_dma(const int32 port, const int32 io, const int32 data);
static int32 adcs6_timer(const int32 port, const int32 io, const int32 data);
static int32 adcs6_control(const int32 port, const int32 io, const int32 data);
static int32 adcs6_banksel(const int32 port, const int32 io, const int32 data);
static int32 adcs6rom(const int32 port, const int32 io, const int32 data);
static const char* adcs6_description(DEVICE *dptr);

static int32 dipswitch      = 0x00;     /* 5-position DIP switch on 64FDC card */

/* Disk Control/Flags Register, 0x34 (IN) */
#define ADCS6_FLAG_DRQ      (1 << 7)    /* DRQ (All controllers) */
#define ADCS6_FLAG_BOOT     (1 << 6)    /* boot# jumper (active low) (All controllers) */
#define ADCS6_FLAG_SEL_REQ  (1 << 5)    /* Head Load (4FDC, 16FDC) / Select Request (64FDC) */
#define ADCS6_FLAG_INH_INIT (1 << 4)    /* Unassigned (4FDC) / Inhibit_Init# (16FDC, 64FDC) */
#define ADCS6_FLAG_MTRON    (1 << 3)    /* Unassigned (4FDC) / Motor On (16FDC, 64FDC) */
#define ADCS6_FLAG_MTO      (1 << 2)    /* Unassigned (4FDC) / Motor Timeout (16FDC, 64FDC) */
#define ADCS6_FLAG_ATO      (1 << 1)    /* Unassigned (4FDC) / Autowait Timeout (16FDC, 64FDC) */
#define ADCS6_FLAG_EOJ      (1 << 0)    /* End of Job (INTRQ) (All Controllers) (16FDC, 64FDC) */

/* Disk Control/Flags Register, 0x34 (OUT) */
#define ADCS6_CTRL_AUTOWAIT (1 << 7)    /* Auto Wait Enable (All controllers) */
#define ADCS6_CTRL_DDENS    (1 << 3)    /* Unassigned (4FDC) / Double Density (16FDC, 64FDC) */
#define ADCS6_CTRL_HDS      (1 << 2)    /* Motor On (All controllers) */
#define ADCS6_CTRL_MINI     (1 << 4)    /* Mini (5.25") (All controllers) */

/* 64FDC Auxiliary Disk Command, 0x04 (OUT) */
#define ADCS6_AUX_RESERVED0 (1 << 0)    /* Unused (All Controllers) */
#define ADCS6_AUX_CMD_SIDE  (1 << 1)    /* 16FDC, 64FDC: Side Select* Low=Side 1, High=Side 0. */
#define ADCS6_AUX_CTRL_OUT  (1 << 2)    /* Control Out* (All Controllers) */
#define ADCS6_AUX_RESTORE   (1 << 3)    /* 4FDC, 16FDC Restore* / 64FDC Unused */
#define ADCS6_AUX_FAST_SEEK (1 << 4)    /* 4FDC, 16FDC Fast Seek* / 64FDC Unused */
#define ADCS6_AUX_SEL_OVERRIDE (1 << 5) /* 4FDC Eject Right* / 16FDC, 64FDC Drive Select Override */
#define ADCS6_AUX_EJECT     (1 << 6)    /* 4FDC Eject Left* / 16FDC Eject*, 64FDC Unused */
#define ADCS6_AUX_RESERVED7 (1 << 7)    /* Unused (All Controllers) */

/* 64FDC Interrupt Mask Register, 0x03 (OUT) */
#define ADCS6_IRQ_TIMER1        (1 << 0)    /* Timer1 Interrupt Mask */
#define ADCS6_IRQ_TIMER2        (1 << 1)    /* Timer2 Interrupt Mask */
#define ADCS6_IRQ_EOJ           (1 << 2)    /* End of Job Interrupt Mask */
#define ADCS6_IRQ_TIMER3        (1 << 3)    /* Timer3 Interrupt Mask */
#define ADCS6_IRQ_RDA           (1 << 4)    /* Read Data Available Interrupt Mask */
#define ADCS6_IRQ_TBE           (1 << 5)    /* Transmit Buffer Empty Interrupt Mask */
#define ADCS6_IRQ_TIMER4        (1 << 6)    /* Timer4 Interrupt Mask */
#define ADCS6_IRQ_TIMER5        (1 << 7)    /* Timer5 Interrupt Mask */

#define ADCS6_TIMER1_RST        0xC7        /* RST0 - 0xC7 */
#define ADCS6_TIMER2_RST        0xCF        /* RST8 - 0xCF */
#define ADCS6_EOJ_RST           0xD7        /* RST10 - 0xD7 */
#define ADCS6_TIMER3_RST        0xDF        /* RST18 - 0xDF */
#define ADCS6_RDA_RST           0xE7        /* RST20 - 0xE7 */
#define ADCS6_TBE_RST           0xEF        /* RST28 - 0xEF */
#define ADCS6_TIMER4_RST        0xF7        /* RST30 - 0xF7 */
#define ADCS6_TIMER5_RST        0xFF        /* RST38 - 0xFF */

#define RST_OPCODE_TO_VECTOR(x) (x & 0x38)

/* The ADCS6 does not really have RAM associated with it, but for ease of integration with the
 * SIMH/AltairZ80 Resource Mapping Scheme, rather than Map and Unmap the ROM, simply implement our
 * own RAM that can be swapped in when the ADCS6 Boot ROM is disabled.
 */
static uint8 adcs6ram[ADCS6_ROM_SIZE];

static UNIT adcs6_unit[] = {
    { UDATA (&adcs6_svc, UNIT_FIX + UNIT_ATTABLE + UNIT_DISABLE + UNIT_ROABLE + UNIT_ADCS6_ROM, ADCS6_CAPACITY), 1024 },
    { UDATA (&adcs6_svc, UNIT_FIX + UNIT_ATTABLE + UNIT_DISABLE + UNIT_ROABLE, ADCS6_CAPACITY) },
    { UDATA (&adcs6_svc, UNIT_FIX + UNIT_ATTABLE + UNIT_DISABLE + UNIT_ROABLE, ADCS6_CAPACITY) },
    { UDATA (&adcs6_svc, UNIT_FIX + UNIT_ATTABLE + UNIT_DISABLE + UNIT_ROABLE, ADCS6_CAPACITY) }
};

static REG adcs6_reg[] = {
    { HRDATAD (J7,           dipswitch,             8, "5-position DIP switch on 64FDC card"), },
    { NULL }
};

#define ADCS6_NAME  "ADC Super-Six"

static const char* adcs6_description(DEVICE *dptr) {
    return ADCS6_NAME;
}

static MTAB adcs6_mod[] = {
    { MTAB_XTD|MTAB_VDV,    0,                  "MEMBASE",  "MEMBASE",
        &set_membase, &show_membase, NULL, "Sets disk controller memory base address"   },
    { MTAB_XTD|MTAB_VDV,    0,                  "IOBASE",   "IOBASE",
        &set_iobase, &show_iobase, NULL, "Sets disk controller I/O base address"        },
    /* quiet, no warning messages       */
    { UNIT_ADCS6_ROM,      0,                   "NOROM",    "NOROM",
        NULL, NULL, NULL, "Disables boot ROM for unit " ADCS6_NAME "n"                  },
    { UNIT_ADCS6_ROM,      UNIT_ADCS6_ROM,      "ROM",      "ROM",
        NULL, NULL, NULL, "Enables boot ROM for unit " ADCS6_NAME "n"                   },
    { 0 }
};

/* Debug Flags */
static DEBTAB adcs6_dt[] = {
    { "ERROR",      ERROR_MSG,      "Error messages"    },
    { "DRIVE",      DRIVE_MSG,      "Drive messages"    },
    { "VERBOSE",    VERBOSE_MSG,    "Verbose messages"  },
    { "DMA",        DMA_MSG,        "DMA messages"      },
    { NULL,         0                                   }
};

DEVICE adcs6_dev = {
    "ADCS6", adcs6_unit, adcs6_reg, adcs6_mod,
    ADCS6_MAX_DRIVES, 10, 31, 1, ADCS6_MAX_DRIVES, ADCS6_MAX_DRIVES,
    NULL, NULL, &adcs6_reset,
    &adcs6_boot, &adcs6_attach, &adcs6_detach,
    &adcs6_info_data, (DEV_DISABLE | DEV_DIS | DEV_DEBUG), ERROR_MSG,
    adcs6_dt, NULL, NULL, NULL, NULL, NULL, &adcs6_description
};

/* This is the DIGITEX Monitor version 1.2.A -- 10/06/83
 *
 * MONITOR COMMANDS :
 * B               = LOAD DISK BOOT LOADER
 * DSSSS,QQQQ      = DUMP MEMORY IN HEX FROM S TO Q
 * FSSSS,QQQQ,BB   = FILL MEMORY FROM S TO Q WITH B
 * GAAAA           = GO TO ADDRESS A
 * IPP             = INPUT FROM PORT P
 * LAAAA           = LOAD MEMORY STARTING AT A
 * MSSSS,QQQQ,DDDD = MOVE STARTING AT S TO Q TO ADDR. D
 * OPP,DD          = OUTPUT DATA D TO PORT P
 * ESC WILL TERMINATE ANY COMMAND
 */
static uint8 adcs6_rom[ADCS6_ROM_SIZE] = {
    0xC3, 0x3C, 0xF0, 0xC3, 0xA4, 0xF0, 0xC3, 0xB6, 0xF0, 0xC3, 0xAF, 0xF0, 0xC3, 0xC9, 0xF0, 0xC3,
    0xE1, 0xF0, 0xC3, 0xF0, 0xF0, 0xC3, 0x06, 0xF1, 0xC3, 0x14, 0xF1, 0xC3, 0x0B, 0xF1, 0xC3, 0x22,
    0xF1, 0xC3, 0x2D, 0xF1, 0xC3, 0x4A, 0xF1, 0xC3, 0x77, 0xF1, 0xC3, 0xA1, 0xF1, 0xC3, 0xFA, 0xF1,
    0xC3, 0xBC, 0xF3, 0xC3, 0x53, 0xF3, 0xC3, 0x8E, 0xF2, 0xC3, 0xA0, 0xF2, 0xDB, 0x15, 0xD3, 0x18,
    0xCB, 0x77, 0x28, 0x10, 0xAF, 0xD3, 0x15, 0xD3, 0x40, 0xD3, 0x17, 0x3E, 0x40, 0xD3, 0x16, 0x21,
    0x3E, 0x60, 0x18, 0x0A, 0xAF, 0xD3, 0x17, 0x3E, 0x4F, 0xD3, 0x16, 0x21, 0x3E, 0x6F, 0x22, 0x04,
    0xEE, 0xAF, 0x32, 0x01, 0xEE, 0x31, 0x64, 0xEE, 0x21, 0xCA, 0xF3, 0x01, 0x01, 0x08, 0xED, 0xB3,
    0x21, 0xD2, 0xF3, 0xCD, 0xE1, 0xF0, 0xC3, 0xB2, 0xF2, 0x31, 0x64, 0xEE, 0x21, 0x4D, 0xF4, 0xCD,
    0xE1, 0xF0, 0xCD, 0xC9, 0xF0, 0x47, 0x21, 0xAF, 0xF6, 0x7E, 0xFE, 0xFF, 0x28, 0x08, 0xB8, 0x28,
    0x0D, 0x23, 0x23, 0x23, 0x18, 0xF3, 0x21, 0x52, 0xF4, 0xCD, 0xE1, 0xF0, 0x18, 0xDB, 0x23, 0x5E,
    0x23, 0x56, 0xEB, 0xE9, 0xF5, 0xDB, 0x01, 0xE6, 0x04, 0x28, 0xFA, 0xF1, 0xD3, 0x00, 0xC9, 0xDB,
    0x01, 0xE6, 0x01, 0xC8, 0x18, 0x06, 0xDB, 0x01, 0xE6, 0x01, 0x28, 0xFA, 0xDB, 0x00, 0xE6, 0x7F,
    0xFE, 0x61, 0xD8, 0xFE, 0x7B, 0xD0, 0xE6, 0x5F, 0xC9, 0x3E, 0xFF, 0x32, 0x00, 0xEE, 0xCD, 0xB6,
    0xF0, 0xF5, 0x3A, 0x00, 0xEE, 0xA7, 0x20, 0x02, 0xF1, 0xC9, 0xF1, 0xFE, 0x20, 0xD4, 0xA4, 0xF0,
    0xC9, 0xF5, 0xE5, 0x7E, 0xB7, 0x28, 0x06, 0xCD, 0xA4, 0xF0, 0x23, 0x18, 0xF6, 0xE1, 0xF1, 0xC9,
    0xE5, 0x21, 0x14, 0xF5, 0xCD, 0xE1, 0xF0, 0xE1, 0xC9, 0xCD, 0xAF, 0xF0, 0xFE, 0x1B, 0xCA, 0x79,
    0xF0, 0xFE, 0x08, 0xD0, 0x18, 0xF3, 0xF5, 0x3E, 0x20, 0x18, 0x12, 0xF5, 0x0F, 0x0F, 0x0F, 0x0F,
    0xCD, 0x14, 0xF1, 0xF1, 0xF5, 0xE6, 0x0F, 0xC6, 0x90, 0x27, 0xCE, 0x40, 0x27, 0xCD, 0xA4, 0xF0,
    0xF1, 0xC9, 0xF5, 0x7C, 0xCD, 0x0B, 0xF1, 0x7D, 0xCD, 0x0B, 0xF1, 0xF1, 0xC9, 0xCD, 0xC9, 0xF0,
    0xFE, 0x2C, 0xC8, 0xFE, 0x20, 0xC8, 0xFE, 0x30, 0xD8, 0xFE, 0x3A, 0xDA, 0x47, 0xF1, 0xFE, 0x41,
    0xD8, 0xFE, 0x47, 0x3F, 0xD8, 0xD6, 0x07, 0xD6, 0x30, 0xC9, 0xC5, 0xD5, 0x0E, 0x00, 0x1E, 0x00,
    0xCD, 0x2D, 0xF1, 0x30, 0x0E, 0xFE, 0x0D, 0x37, 0x20, 0x1A, 0x7B, 0xB7, 0x20, 0x15, 0x37, 0x3E,
    0x0D, 0x18, 0x11, 0xFE, 0x10, 0x30, 0x0C, 0x1C, 0x47, 0x79, 0x87, 0x87, 0x87, 0x87, 0x80, 0x4F,
    0xC3, 0x50, 0xF1, 0x79, 0xD1, 0xC1, 0xC9, 0xD5, 0x21, 0x00, 0x00, 0x37, 0x3F, 0xF5, 0xCD, 0x2D,
    0xF1, 0x30, 0x0D, 0xFE, 0x0D, 0x20, 0x05, 0xCD, 0x06, 0xF1, 0x18, 0x12, 0xF1, 0x37, 0xD1, 0xC9,
    0xFE, 0x10, 0x30, 0x0A, 0x29, 0x29, 0x29, 0x29, 0x5F, 0x16, 0x00, 0x19, 0x18, 0xE0, 0xF1, 0xD1,
    0xC9, 0x77, 0xBE, 0xC8, 0xE5, 0x21, 0x63, 0xF4, 0xCD, 0xE1, 0xF0, 0xE1, 0xCD, 0x22, 0xF1, 0xC3,
    0x79, 0xF0, 0xCD, 0x77, 0xF1, 0xD2, 0xBE, 0xF1, 0x21, 0x5D, 0xF4, 0xC3, 0x99, 0xF0, 0x22, 0x02,
    0xEE, 0xCD, 0xF0, 0xF0, 0x2A, 0x02, 0xEE, 0xCD, 0x22, 0xF1, 0xCD, 0x06, 0xF1, 0x7E, 0xCD, 0x0B,
    0xF1, 0xCD, 0x06, 0xF1, 0xCD, 0x4A, 0xF1, 0xDA, 0xE4, 0xF1, 0xCD, 0xA1, 0xF1, 0x2A, 0x02, 0xEE,
    0x23, 0xC3, 0xBE, 0xF1, 0xFE, 0x0D, 0xCA, 0x79, 0xF0, 0xFE, 0x20, 0xCA, 0xDD, 0xF1, 0xFE, 0x2D,
    0xC2, 0xB8, 0xF1, 0x2A, 0x02, 0xEE, 0x2B, 0xC3, 0xBE, 0xF1, 0xF5, 0x7A, 0x2F, 0x57, 0x7B, 0x2F,
    0x5F, 0x13, 0xF1, 0xC9, 0xCD, 0x7E, 0xF2, 0xCD, 0x7E, 0xF2, 0xCD, 0xFA, 0xF1, 0xCD, 0xF0, 0xF0,
    0xCD, 0x22, 0xF1, 0xCD, 0x06, 0xF1, 0xCD, 0x06, 0xF1, 0x7E, 0xCD, 0x0B, 0xF1, 0xCD, 0x3F, 0xF2,
    0xCD, 0x86, 0xF2, 0xFE, 0x13, 0xCC, 0xF9, 0xF0, 0x7D, 0xE6, 0x0F, 0xCA, 0x0D, 0xF2, 0xC3, 0x16,
    0xF2, 0xCD, 0x7E, 0xF2, 0xEB, 0xE9, 0x21, 0x17, 0xF5, 0xCD, 0xE1, 0xF0, 0xC3, 0x79, 0xF0, 0xE5,
    0x19, 0xDA, 0x79, 0xF0, 0xE1, 0x23, 0xC9, 0xCD, 0x7E, 0xF2, 0xD5, 0xCD, 0x7E, 0xF2, 0xCD, 0x7E,
    0xF2, 0xEB, 0xE3, 0x8D, 0xFA, 0xF1, 0x7E, 0xE3, 0xCD, 0xA1, 0xF1, 0x23, 0xE3, 0xCD, 0x3F, 0xF2,
    0xCD, 0x86, 0xF2, 0xC3, 0x56, 0xF2, 0xCD, 0x7E, 0xF2, 0xCD, 0x7E, 0xF2, 0xCD, 0xFA, 0xF1, 0xCD,
    0x4A, 0xF1, 0xDA, 0xB8, 0xF1, 0xCD, 0xA1, 0xF1, 0xCD, 0x3F, 0xF2, 0xC3, 0x75, 0xF2, 0xCD, 0x77,
    0xF1, 0xDA, 0xB8, 0xF1, 0xEB, 0xC9, 0xCD, 0xAF, 0xF0, 0xB7, 0xC2, 0x79, 0xF0, 0xC9, 0xCD, 0x4A,
    0xF1, 0xDA, 0xB8, 0xF1, 0x4F, 0xED, 0x78, 0xCD, 0xF0, 0xF0, 0xCD, 0x0B, 0xF1, 0xC3, 0x79, 0xF0,
    0xCD, 0x4A, 0xF1, 0xDA, 0xB8, 0xF1, 0x4F, 0xCD, 0x4A, 0xF1, 0xDA, 0xB8, 0xF1, 0xED, 0x79, 0xC3,
    0x79, 0xF0, 0xCD, 0x86, 0xF2, 0xCD, 0xC6, 0xF2, 0xCD, 0x6F, 0xF3, 0xCD, 0x86, 0xF2, 0xCD, 0xE8,
    0xF2, 0xCD, 0x8F, 0xF3, 0x18, 0xF5, 0xDB, 0x15, 0x47, 0x3E, 0x18, 0xCB, 0x60, 0x20, 0x01, 0xAF,
    0x32, 0x06, 0xEE, 0xD3, 0x14, 0x3E, 0x0B, 0xD3, 0x0C, 0x00, 0xDB, 0x14, 0xDB, 0x0C, 0xE6, 0x80,
    0xC8, 0xAF, 0x32, 0x06, 0xEE, 0xD3, 0x14, 0xC9, 0xDB, 0x0C, 0x17, 0xD8, 0x21, 0xE8, 0x03, 0xDB,
    0x0C, 0xE6, 0x02, 0x28, 0x06, 0x2B, 0x7D, 0xB4, 0x20, 0xF5, 0xC9, 0x06, 0x0A, 0x21, 0x80, 0x3E,
    0xDB, 0x0C, 0xE6, 0x02, 0x20, 0x08, 0x2B, 0x7D, 0xB4, 0x20, 0xF5, 0x10, 0xF0, 0xC9, 0x3E, 0xFF,
    0x32, 0x01, 0xEE, 0x3E, 0x01, 0x21, 0x00, 0xC0, 0x32, 0x08, 0xEE, 0xD3, 0x0E, 0x3E, 0x8C, 0xD3,
    0x0C, 0x00, 0xDB, 0x14, 0xB7, 0xF2, 0x2E, 0xF3, 0xDB, 0x0F, 0x77, 0x23, 0x18, 0xF4, 0xDB, 0x0C,
    0xB7, 0x20, 0x19, 0x3A, 0x08, 0xEE, 0x3C, 0xFE, 0x04, 0x20, 0xDD, 0x2A, 0x04, 0xEE, 0x22, 0xFC,
    0xBF, 0x2A, 0x4A, 0xF3, 0x22, 0xFE, 0xBF, 0xC3, 0xFC, 0xBF, 0xD3, 0x16, 0xF5, 0x3A, 0x01, 0xEE,
    0xB7, 0x28, 0x0D, 0x21, 0xB8, 0xF4, 0xCD, 0xE1, 0xF0, 0xF1, 0xCD, 0x0B, 0xF1, 0xC3, 0x79, 0xF0,
    0xF1, 0x21, 0xD4, 0xF4, 0xCD, 0xE1, 0xF0, 0x3E, 0xFF, 0x32, 0x01, 0xEE, 0xC3, 0x79, 0xF0, 0xAF,
    0xD3, 0xE6, 0x3E, 0x08, 0xD3, 0xE6, 0x3E, 0x10, 0xD3, 0xE6, 0x3E, 0x18, 0xD3, 0xE6, 0xAF, 0xD3,
    0xE6, 0xD3, 0xE3, 0xD3, 0xE4, 0xD3, 0xE5, 0x3C, 0xD3, 0xE2, 0x3E, 0x70, 0xD3, 0xE7, 0xC9, 0xDB,
    0xE7, 0xE6, 0x50, 0xFE, 0x50, 0xC0, 0xDB, 0xE7, 0xB7, 0xF8, 0xAF, 0xD3, 0xE6, 0x3E, 0x20, 0xD3,
    0xE7, 0xDB, 0xE7, 0xB7, 0xFA, 0xA1, 0xF3, 0xE6, 0x01, 0x20, 0x11, 0x21, 0x00, 0x80, 0x01, 0xE0,
    0x00, 0xED, 0xB2, 0x2A, 0x04, 0xEE, 0x22, 0x00, 0x80, 0xC3, 0x00, 0x80, 0x21, 0xF4, 0xF4, 0xCD,
    0xE1, 0xF0, 0xDB, 0xE1, 0xCD, 0x0B, 0xF1, 0xC3, 0x79, 0xF0, 0x18, 0x04, 0x44, 0x03, 0xC1, 0x05,
    0xEA, 0x00, 0x0D, 0x0A, 0x0A, 0x0A, 0x44, 0x49, 0x47, 0x49, 0x54, 0x45, 0x58, 0x20, 0x4D, 0x6F,
    0x6E, 0x69, 0x74, 0x6F, 0x72, 0x20, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6F, 0x6E, 0x20, 0x31, 0x2E,
    0x32, 0x2E, 0x41, 0x20, 0x2D, 0x2D, 0x20, 0x31, 0x30, 0x2F, 0x30, 0x36, 0x2F, 0x38, 0x33, 0x0D,
    0x0A, 0x0A, 0x50, 0x72, 0x65, 0x73, 0x73, 0x20, 0x22, 0x48, 0x22, 0x20, 0x66, 0x6F, 0x72, 0x20,
    0x48, 0x65, 0x6C, 0x70, 0x0D, 0x0A, 0x0A, 0x41, 0x74, 0x74, 0x65, 0x6D, 0x70, 0x74, 0x69, 0x6E,
    0x67, 0x20, 0x74, 0x6F, 0x20, 0x62, 0x6F, 0x6F, 0x74, 0x2E, 0x2E, 0x2E, 0x0D, 0x0A, 0x50, 0x72,
    0x65, 0x73, 0x73, 0x20, 0x61, 0x6E, 0x79, 0x20, 0x6B, 0x65, 0x79, 0x20, 0x74, 0x6F, 0x20, 0x61,
    0x62, 0x6F, 0x72, 0x74, 0x20, 0x62, 0x6F, 0x6F, 0x74, 0x2E, 0x0D, 0x0A, 0x00, 0x0D, 0x0A, 0x20,
    0x3E, 0x00, 0x20, 0x55, 0x4E, 0x44, 0x45, 0x46, 0x49, 0x4E, 0x45, 0x44, 0x00, 0x20, 0x3F, 0x3F,
    0x3F, 0x3F, 0x00, 0x0D, 0x0D, 0x0A, 0x4D, 0x45, 0x4D, 0x4F, 0x52, 0x59, 0x20, 0x57, 0x52, 0x49,
    0x54, 0x45, 0x20, 0x45, 0x52, 0x52, 0x4F, 0x52, 0x20, 0x41, 0x54, 0x20, 0x00, 0x45, 0x52, 0x52,
    0x4F, 0x52, 0x00, 0x20, 0x50, 0x41, 0x55, 0x53, 0x45, 0x00, 0x3F, 0x20, 0x00, 0x20, 0x41, 0x42,
    0x4F, 0x52, 0x54, 0x45, 0x44, 0x00, 0x53, 0x54, 0x41, 0x52, 0x54, 0x49, 0x4E, 0x47, 0x20, 0x41,
    0x44, 0x44, 0x52, 0x45, 0x53, 0x53, 0x3A, 0x00, 0x45, 0x4E, 0x44, 0x49, 0x4E, 0x47, 0x20, 0x41,
    0x44, 0x44, 0x52, 0x45, 0x53, 0x53, 0x3A, 0x00, 0x0D, 0x0A, 0x46, 0x44, 0x43, 0x20, 0x43, 0x4F,
    0x4C, 0x44, 0x20, 0x42, 0x4F, 0x4F, 0x54, 0x20, 0x45, 0x52, 0x52, 0x4F, 0x52, 0x20, 0x43, 0x4F,
    0x44, 0x45, 0x20, 0x00, 0x0D, 0x0A, 0x49, 0x4E, 0x53, 0x45, 0x52, 0x54, 0x20, 0x44, 0x49, 0x53,
    0x4B, 0x20, 0x26, 0x20, 0x50, 0x52, 0x45, 0x53, 0x53, 0x20, 0x42, 0x20, 0x54, 0x4F, 0x20, 0x42,
    0x4F, 0x4F, 0x54, 0x00, 0x0D, 0x0A, 0x48, 0x44, 0x43, 0x31, 0x30, 0x30, 0x31, 0x20, 0x43, 0x4F,
    0x4C, 0x44, 0x20, 0x42, 0x4F, 0x4F, 0x54, 0x20, 0x45, 0x52, 0x52, 0x4F, 0x52, 0x20, 0x43, 0x4F,
    0x44, 0x45, 0x20, 0x00, 0x0D, 0x0A, 0x00, 0x0D, 0x0A, 0x4D, 0x4F, 0x4E, 0x49, 0x54, 0x4F, 0x52,
    0x20, 0x43, 0x4F, 0x4D, 0x4D, 0x41, 0x4E, 0x44, 0x53, 0x20, 0x3A, 0x0D, 0x0A, 0xC2, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xBD, 0x20, 0x4C,
    0x4F, 0x41, 0x44, 0x20, 0x44, 0x49, 0x53, 0x4B, 0x20, 0x42, 0x4F, 0x4F, 0x54, 0x20, 0x4C, 0x4F,
    0x41, 0x44, 0x45, 0x52, 0x0D, 0x0A, 0x44, 0x53, 0x53, 0x53, 0x53, 0x2C, 0x51, 0x51, 0x51, 0x51,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3D, 0x20, 0x44, 0x55, 0x4D, 0x50, 0x20, 0x4D, 0x45, 0x4D,
    0x4F, 0x52, 0x59, 0x20, 0x49, 0x4E, 0x20, 0x48, 0x45, 0x58, 0x20, 0x46, 0x52, 0x4F, 0x4D, 0x20,
    0x53, 0x20, 0x54, 0x4F, 0x20, 0x51, 0x0D, 0x0A, 0x46, 0x53, 0x53, 0x53, 0x53, 0x2C, 0x51, 0x51,
    0x51, 0x51, 0x2C, 0x42, 0x42, 0x20, 0x20, 0x20, 0x3D, 0x20, 0x46, 0x49, 0x4C, 0x4C, 0x20, 0x4D,
    0x45, 0x4D, 0x4F, 0x52, 0x59, 0x20, 0x46, 0x52, 0x4F, 0x4D, 0x20, 0x53, 0x20, 0x54, 0x4F, 0x20,
    0x51, 0x20, 0x57, 0x49, 0x54, 0x48, 0x20, 0x42, 0x0D, 0x0A, 0x47, 0x41, 0x41, 0x41, 0x41, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3D, 0x20, 0x47, 0x4F, 0x20, 0x54,
    0x4F, 0x20, 0x41, 0x44, 0x44, 0x52, 0x45, 0x53, 0x53, 0x20, 0x41, 0x0D, 0x0A, 0x49, 0x50, 0x50,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3D, 0x20, 0x49,
    0x4E, 0x50, 0x55, 0x54, 0x20, 0x46, 0x52, 0x4F, 0x4D, 0x20, 0x50, 0x4F, 0x52, 0x54, 0x20, 0x50,
    0x0D, 0x0A, 0x4C, 0x41, 0x41, 0x41, 0x41, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x3D, 0x20, 0x4C, 0x4F, 0x41, 0x44, 0x20, 0x4D, 0x45, 0x4D, 0x4F, 0x52, 0x59, 0x20,
    0x53, 0x54, 0x41, 0x52, 0x54, 0x49, 0x4E, 0x47, 0x20, 0x41, 0x54, 0x20, 0x41, 0x0D, 0x0A, 0x4D,
    0x53, 0x53, 0x53, 0x53, 0x2C, 0x51, 0x51, 0x51, 0x51, 0x2C, 0x44, 0x44, 0x44, 0x44, 0x20, 0x3D,
    0x20, 0x4D, 0x4F, 0x56, 0x45, 0x20, 0x53, 0x54, 0x41, 0x52, 0x54, 0x49, 0x4E, 0x47, 0x20, 0x41,
    0x54, 0x20, 0x53, 0x20, 0x54, 0x4F, 0x20, 0x51, 0x20, 0x54, 0x4F, 0x20, 0x41, 0x44, 0x44, 0x52,
    0x2E, 0x20, 0x44, 0x0D, 0x0A, 0x4F, 0x50, 0x50, 0x2C, 0x44, 0x44, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x3D, 0x20, 0x4F, 0x55, 0x54, 0x50, 0x55, 0x54, 0x20, 0x44, 0x41,
    0x54, 0x41, 0x20, 0x44, 0x20, 0x54, 0x4F, 0x20, 0x50, 0x4F, 0x52, 0x54, 0x20, 0x50, 0x0D, 0x0A,
    0x45, 0x53, 0x43, 0x20, 0x57, 0x49, 0x4C, 0x4C, 0x20, 0x54, 0x45, 0x52, 0x4D, 0x49, 0x4E, 0x41,
    0x54, 0x45, 0x20, 0x41, 0x4E, 0x59, 0x20, 0x43, 0x4F, 0x4D, 0x4D, 0x41, 0x4E, 0x44, 0x00, 0x4C,
    0xB2, 0xF1, 0x0D, 0x79, 0xF0, 0x2E, 0xC1, 0xF1, 0x2D, 0xF3, 0xF1, 0x44, 0x04, 0xF2, 0x49, 0x8E,
    0xF2, 0x4F, 0xA0, 0xF2, 0x46, 0x66, 0xF2, 0x47, 0x31, 0xF2, 0x4D, 0x47, 0xF2, 0x48, 0x36, 0xF2,
    0x42, 0xB2, 0xF2, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x7F, 0x01, 0x00, 0x04, 0x00, 0x07, 0x00, 0x0A, 0x00, 0x0D, 0x00, 0x10, 0x00, 0x13, 0x00, 0x16,
    0x00, 0x19, 0x00, 0x1C, 0x00, 0x1F, 0x00, 0x22, 0x00, 0x25, 0x00, 0x28, 0x00, 0x2B, 0x00, 0x2E,
    0x00, 0x31, 0x00, 0x34, 0x00, 0x37, 0x00, 0x3A, 0x00, 0x69, 0x00, 0x71, 0x00, 0x74, 0x00, 0x77,
    0x00, 0x7D, 0x00, 0x80, 0x00, 0x83, 0x00, 0x87, 0x00, 0x97, 0x00, 0x9A, 0x00, 0xCF, 0x00, 0xDE,
    0x00, 0xE8, 0x00, 0xF2, 0x00, 0xF5, 0x00, 0xFA, 0x00, 0xFF, 0x00, 0x11, 0x01, 0x1E, 0x01, 0x25,
    0x01, 0x29, 0x01, 0x2E, 0x01, 0x3C, 0x01, 0x51, 0x01, 0x71, 0x01, 0x7F, 0x01, 0x88, 0x01, 0xA6,
    0x01, 0xA9, 0x01, 0xAD, 0x01, 0xB0, 0x01, 0xB3, 0x01, 0xB6, 0x01, 0xB9, 0x01, 0xBC, 0x01, 0xC2,
    0x01, 0xC8, 0x01, 0xCB, 0x01, 0xCF, 0x01, 0xD2, 0x01, 0xD5, 0x01, 0xD8, 0x01, 0xDB, 0x01, 0xE2,
    0x01, 0xE7, 0x01, 0xEC, 0x01, 0xF1, 0x01, 0xF8, 0x01, 0x05, 0x02, 0x08, 0x02, 0x0B, 0x02, 0x0E,
    0x02, 0x11, 0x02, 0x14, 0x02, 0x17, 0x02, 0x1B, 0x02, 0x1E, 0x02, 0x21, 0x02, 0x26, 0x02, 0x2C,
    0x02, 0x2F, 0x02, 0x32, 0x02, 0x37, 0x02, 0x3A, 0x02, 0x3D, 0x02, 0x42, 0x02, 0x48, 0x02, 0x4C,
    0x02, 0x4F, 0x02, 0x54, 0x02, 0x59, 0x02, 0x5E, 0x02, 0x61, 0x02, 0x64, 0x02, 0x67, 0x02, 0x6A,
    0x02, 0x6D, 0x02, 0x70, 0x02, 0x73, 0x02, 0x76, 0x02, 0x79, 0x02, 0x7C, 0x02, 0x7F, 0x02, 0x82,
    0x02, 0x87, 0x02, 0x8B, 0x02, 0x8F, 0x02, 0x92, 0x02, 0x98, 0x02, 0x9B, 0x02, 0x9E, 0x02, 0xA1,
    0x02, 0xA4, 0x02, 0xA8, 0x02, 0xAB, 0x02, 0xB0, 0x02, 0xB3, 0x02, 0xB6, 0x02, 0xB9, 0x02, 0xBC,
    0x02, 0xBF, 0x02, 0xC2, 0x02, 0x26, 0x03, 0x42, 0x03, 0x54, 0x03, 0x57, 0x03, 0x5B, 0x03, 0x00
};

/* returns TRUE iff there exists a disk with VERBOSE */
static int32 adcs6_hasProperty(uint32 property) {
    int32 i;
    for (i = 0; i < ADCS6_MAX_DRIVES; i++)
        if (adcs6_dev.units[i].flags & property)
            return TRUE;
    return FALSE;
}

static uint8 motor_timeout = 0;

/* Unit service routine */
static t_stat adcs6_svc (UNIT *uptr)
{

    if(adcs6_info->head_sel == 1) {
        motor_timeout ++;
        if(motor_timeout == MOTOR_TO_LIMIT) {
            adcs6_info->head_sel = 0;
            sim_debug(DRIVE_MSG, &adcs6_dev, "ADCS6: Motor OFF\n");
        }
    }

    adcs6_info->rtc ++;

    sim_printf("Timer IRQ\n");
    adcs6_info->ipend |= ADCS6_IRQ_TIMER3;

/*  sim_activate (adcs6_unit, adcs6_unit->wait); */ /* requeue! */

    return SCPE_OK;
}


/* Reset routine */
static t_stat adcs6_reset(DEVICE *dptr)
{
    PNP_INFO *pnp = (PNP_INFO *)dptr->ctxt;

    if(dptr->flags & DEV_DIS) { /* Disconnect ROM and I/O Ports */
        if (adcs6_hasProperty(UNIT_ADCS6_ROM)) {
            sim_map_resource(pnp->mem_base, pnp->mem_size, RESOURCE_TYPE_MEMORY, &adcs6rom, TRUE);
        }
        /* Unmap I/O Ports (0x3-4,0x5-9,0x34,0x40 */
        sim_map_resource(0x10, 4, RESOURCE_TYPE_IO, &adcs6_dma, TRUE);
        sim_map_resource(0x04, 8, RESOURCE_TYPE_IO, &adcs6_timer, TRUE);
        sim_map_resource(0x14, 1, RESOURCE_TYPE_IO, &adcs6_control, TRUE);
        sim_map_resource(0x15, 7, RESOURCE_TYPE_IO, &adcs6_banksel, TRUE);
    } else {
        /* Connect ADCS6 ROM at base address */
        if (adcs6_hasProperty(UNIT_ADCS6_ROM)) {
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: ROM Enabled.\n");
            if(sim_map_resource(pnp->mem_base, pnp->mem_size, RESOURCE_TYPE_MEMORY, &adcs6rom, FALSE) != 0) {
                sim_printf("%s: error mapping MEM resource at 0x%04x\n", __FUNCTION__, pnp->io_base);
                return SCPE_ARG;
            }
            adcs6_info->rom_disabled = FALSE;
        } else {
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: ROM Disabled.\n");
            adcs6_info->rom_disabled = TRUE;
        }

        /* Connect ADCS6 FDC Synchronization / Drive / Density Register */
        if(sim_map_resource(0x14, 0x01, RESOURCE_TYPE_IO, &adcs6_control, FALSE) != 0) {
            sim_printf("%s: error mapping I/O resource at 0x%04x\n", __FUNCTION__, pnp->io_base);
            return SCPE_ARG;
        }
/*#define ADCS6 */
#ifdef ADCS6
        /* Connect ADCS6 Interrupt, and Aux Disk Registers */

        if(sim_map_resource(0x10, 0x04, RESOURCE_TYPE_IO, &adcs6_dma, FALSE) != 0) {
            sim_printf("%s: error mapping I/O resource at 0x%04x\n", __FUNCTION__, pnp->io_base);
            return SCPE_ARG;
        }

        /* Connect ADCS6 Timer Registers */
        if(sim_map_resource(0x04, 0x08, RESOURCE_TYPE_IO, &adcs6_timer, FALSE) != 0) {
            sim_printf("%s: error mapping I/O resource at 0x%04x\n", __FUNCTION__, pnp->io_base);
            return SCPE_ARG;
        }
#endif
        /* Connect ADCS6 Memory Management / Bank Select Register */
        if(sim_map_resource(0x15, 0x7, RESOURCE_TYPE_IO, &adcs6_banksel, FALSE) != 0) {
            sim_printf("%s: error mapping I/O resource at 0x%04x\n", __FUNCTION__, pnp->io_base);
            return SCPE_ARG;
        }
    }

/*  sim_activate (adcs6_unit, adcs6_unit->wait); */ /* requeue! */
    return SCPE_OK;
}

static t_stat adcs6_boot(int32 unitno, DEVICE *dptr)
{
    DBG_PRINT(("Booting ADCS6 Controller" NLP));

    /* Re-enable the ROM in case it was disabled */
    adcs6_info->rom_disabled = FALSE;

    /* Set the PC to 0, and go. */
    *((int32 *) sim_PC->loc) = 0xF000;
    return SCPE_OK;
}

/* Attach routine */
static t_stat adcs6_attach(UNIT *uptr, char *cptr)
{
    t_stat r;
    r = wd179x_attach(uptr, cptr);

    return r;
}

/* Detach routine */
static t_stat adcs6_detach(UNIT *uptr)
{
    t_stat r;

    r = wd179x_detach(uptr);

    return r;
}

static int32 adcs6rom(const int32 Addr, const int32 write, const int32 data)
{
/*  DBG_PRINT(("ADCS6: ROM %s, Addr %04x" NLP, write ? "WR" : "RD", Addr)); */
    if(write) {
        if(adcs6_info->rom_disabled == FALSE) {
            sim_debug(ERROR_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " Cannot write to ROM.\n", PCX);
        } else {
            adcs6ram[Addr & ADCS6_ADDR_MASK] = data;
        }
        return 0;
    } else {
        if(adcs6_info->rom_disabled == FALSE) {
            return(adcs6_rom[Addr & ADCS6_ADDR_MASK]);
        } else {
            return(adcs6ram[Addr & ADCS6_ADDR_MASK]);
        }
    }
}

/* Disk Control/Flags Register, 0x14 */
static int32 adcs6_control(const int32 port, const int32 io, const int32 data)
{
    int32 result = 0;
    if(io) { /* I/O Write */
        wd179x_infop->sel_drive = data & 0x03;

        if(data & ADCS6_CTRL_MINI) {
            wd179x_infop->drivetype = 5;
        } else {
            wd179x_infop->drivetype = 8;
        }

        if(data & ADCS6_CTRL_HDS) {
            adcs6_info->head_sel = 1;
            wd179x_infop->fdc_head = 1;
        } else {
            adcs6_info->head_sel = 0;
            wd179x_infop->fdc_head = 0;
        }

        if(data & ADCS6_CTRL_DDENS) {
            wd179x_infop->ddens = 1;
        } else {
            wd179x_infop->ddens = 0;
        }
        if(data & ADCS6_CTRL_AUTOWAIT) {
            adcs6_info->autowait = 1;
        } else {
            adcs6_info->autowait = 0;
        }

        sim_debug(DRIVE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                  " WR CTRL: sel_drive=%d, drivetype=%d, head_sel=%d, dens=%d, aw=%d\n",
                  PCX, wd179x_infop->sel_drive,
                  wd179x_infop->drivetype, adcs6_info->head_sel,
                  wd179x_infop->ddens, adcs6_info->autowait);
    } else { /* I/O Read */
        result = wd179x_infop->drq ? 0xFF : 0;
        if (wd179x_infop->intrq)
            result &= 0x7F;
    }

    return result;
}

/* ADC Super Six DMA (Z80-DMA) */
static int32 adcs6_dma(const int32 port, const int32 io, const int32 data)
{
    int32 result = 0xff;
    if(io) { /* I/O Write */
        sim_debug(DMA_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                  " WR DMA: 0x%02x\n", PCX, data & 0xFF);
    } else { /* I/O Read */
        result = 0xFF;
        sim_debug(DMA_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                  " RD DMA: 0x%02x\n", PCX, result);
    }
    return result;
}

/* ADC Super-Six PIO and CTC ports */
static int32 adcs6_timer(const int32 port, const int32 io, const int32 data)
{
    static int32 result = 0xFF;
    if(io) { /* Write */
        switch(port) {
        case 0x04:
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " WR PIOA DATA=0x%02x\n", PCX, data);
            break;
        case 0x05:
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " WR PIOB DATA=0x%02x\n", PCX, data);
            break;
        case 0x06:
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " WR PIOA CTRL=0x%02x\n", PCX, data);
            break;
        case 0x07:
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " WR PIOB CTRL=0x%02x\n", PCX, data);
            break;
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " WR CTC%d: 0x%02x\n", PCX, port - 8, data);
            break;
        default:
            sim_debug(ERROR_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " WR Unhandled Port: 0x%02x=0x%02x\n", PCX, port, data);
            break;
        }
    } else { /* Read */
        result = 0xFF;
        switch(port) {
        case 0x04:
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " RD PIOA DATA=0x%02x\n", PCX, result);
            break;
        case 0x05:
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " RD PIOB DATA=0x%02x\n", PCX, result);
            break;
        case 0x06:
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " RD PIOA CTRL=0x%02x\n", PCX, result);
            break;
        case 0x07:
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " RD PIOB CTRL=0x%02x\n", PCX, result);
            break;
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " RD CTC%d: 0x%02x\n", PCX, port - 8, data);
            break;
        default:
            sim_debug(ERROR_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " RD Unhandled Port: 0x%02x=0x%02x\n", PCX, port, data);
            break;
        }
    }
    return result;
}

/* 64FDC Bank Select (Write Disables boot ROM) */
static int32 adcs6_banksel(const int32 port, const int32 io, const int32 data)
{
    int32 result;
    if(io) {    /* Write */
        switch(port) {
        case 0x15:
            adcs6_info->s100_addr_u = data & 0xFF;
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " WR S100 A[23:16]=0x%02x\n", PCX, data);
            break;
        case 0x16:
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " WR MCTRL0: 0x%02x\n", PCX, data);
            adcs6_info->rom_disabled = (data & 0x20) ? TRUE : FALSE; /* Unmap Boot ROM */
            break;
        case 0x17:
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " WR MCTRL1: 0x%02x\n", PCX, data);
            break;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " WR BAUD RATE=0x%02x\n", PCX, data);
            break;
        default:
            sim_debug(ERROR_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " WR Unhandled Port: 0x%02x=0x%02x\n", PCX, port, data);
            break;
        }
        result = 0;
    } else {    /* Read */
        result = 0xFF;
        switch(port) {
        case 0x15:
            /* These are the Jumpers at J7.
             * Bit 7=0 = double-sided disk, bit 7=1 = single sided.
             * Bit 6=0 = use on-board RAM, Bit 6=1 = use S-100 RAM cards.
             * Bit 5:0 = "Baud Rate"
             */
            result = dipswitch;
            sim_debug(VERBOSE_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " RD BAUD RATE=0x%02x\n", PCX, result);
            break;
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
        default:
            sim_debug(ERROR_MSG, &adcs6_dev, "ADCS6: " ADDRESS_FORMAT
                      " RD attempt from write-only 0x%02x=0x%02x\n", PCX, port, result);
            break;
        }
    }
    return result;
}
