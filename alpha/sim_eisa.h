/* sim_eisa.h: (E)ISA bus simulator

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
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of the author shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author.

*/

/*
	This implements a 8/16/32 bit (E)ISA Local Bus.

    The original ISA bus was 8-bit. A 16-bit extension was later added to double the bandwidth.
    Even later, the ISA bus was viewed as a system limit due to the low bandwidth, and the proprietary
    MicroChannel (MCA) bus was created by IBM to solve the bandwidth problem, which required royalty
    payments to IBM to license. In reponse, multiple competing vendors joined together to create
    a 32-bit compatible extension to the ISA bus called the EISA bus to compete with the MCA bus.
    
    This simulation is designed to accept both ISA and EISA transactions.

	Documentation:

	Features:
	
	Features not implemented:

*/

/*
	History:
	--------
	2016-08-03  DTH  Started
*/

#ifndef __sim_eisa_h
#define __sim_eisa_h

#include "sim_defs.h"

typedef int EISA_STAT;
#define EISA_OK           0  // Success
#define EISA_NOT_ME       1  // EISA no response from this device (normal)
#define EISA_NOFNC        2  // EISA Function Not Implemented
#define EISA_SETUP_ERR   10  // EISA setup error
#define EISA_ARG_ERR     11  // EISA routine argument error

typedef struct eisa_bus_t           EISA_BUS;
typedef struct eisa_device_t        EISA_DEV;
typedef struct eisa_registration_t  EISA_REG;

struct eisa_device_t {
	char*	  name;				// name of device
    DEVICE*   dev;              // SIMH device
	EISA_STAT (*reset)     ();	// reset routine
	EISA_STAT (*read)	   (uint32 eisa_address, uint8 size, uint32* value);
	EISA_STAT (*write)     (uint32 eisa_address, uint8 size, uint32* value);
	EISA_STAT (*dma_read)  (uint32 eisa_address, uint8 size, uint32* value, int repeat);
	EISA_STAT (*dma_write) (uint32 eisa_address, uint8 size, uint32* value, int repeat);
};

struct eisa_registration_t {
	EISA_DEV*   device;
    uint32      low;
    uint32      high;
};

#define EISA_MAX_REG    32
struct eisa_bus_t {
	char*		name;
    EISA_REG    registration[EISA_MAX_REG];
    int         registered;
};

// Bus Transaction prototypes
EISA_STAT eisa_bus_reset     (EISA_BUS* eisa);
EISA_STAT eisa_bus_read      (EISA_BUS* eisa, uint32 eisa_address, uint8 size, uint32* value);
EISA_STAT eisa_bus_write     (EISA_BUS* eisa, uint32 eisa_address, uint8 size, uint32* value);
EISA_STAT eisa_bus_dma_read  (EISA_BUS* eisa, uint32 eisa_address, uint8 size, uint32* value, int repeat);
EISA_STAT eisa_bus_dma_write (EISA_BUS* eisa, uint32 eisa_address, uint8 size, uint32* value, int repeat);

// Register Device I/O ranges for I/O dispatch
EISA_STAT eisa_register   (EISA_BUS* eisa, EISA_DEV* edev, uint32 low, uint32 high);
EISA_STAT eisa_unregister (EISA_BUS* eisa, EISA_DEV* edev, uint32 low, uint32 high);

#endif // ifndef __sim_eisa_h