/* sim_eisa.c: (E)ISA bus simulator

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

#include "sim_defs.h"
#include "sim_eisa.h"


EISA_STAT eisa_register (EISA_BUS* eisa, EISA_DEV* edev, uint32 low, uint32 high)
{
    int indx;

    // ignore if device I/O range is already registered
    for (indx=0; indx<eisa->registered; indx++) {
        if ((eisa->registration[indx].device == edev) &&
            (eisa->registration[indx].low == low) &&
            (eisa->registration[indx].high == high)) {
                return EISA_OK;
        }
    }

    // make sure there's no I/O range overlap with existing devices
    for (indx=0; indx<eisa->registered; indx++) {
        if ( (low >= eisa->registration[indx].low) && (low <= eisa->registration[indx].high) ||
            (high >= eisa->registration[indx].low) && (high <= eisa->registration[indx].high) ) {
                sim_printf("EISA Device(%s) I/O range overlaps another EISA Device(%s)!\n",
                    edev->name, eisa->registration[indx].device->name);
                return EISA_ARG_ERR;
        }
    }

    // register device I/O range
    indx = eisa->registered++;
    eisa->registration[indx].device = edev;
    eisa->registration[indx].low = low;
    eisa->registration[indx].high = high;
    return EISA_OK;
}

EISA_STAT eisa_unregister (EISA_BUS* eisa, EISA_DEV* edev, uint32 low, uint32 high)
{
    int indx;
    int matched = 0;

    // find device to unregister
    for (indx=0; indx<eisa->registered; indx++) {
        if (eisa->registration[indx].device == edev) {
            if ((eisa->registration[indx].low == low) && (eisa->registration[indx].high == high)) {
                // matches exactly, erase item and bail
                matched = 1;
                eisa->registration[indx].device = NULL;
                eisa->registration[indx].low = 0;
                eisa->registration[indx].high = 0;
                break;
            } else if ((low == 0) && (high == 0)) {
                // matches zero wildcard, erase and continue
                matched = 1;
                eisa->registration[indx].device = NULL;
                eisa->registration[indx].low = 0;
                eisa->registration[indx].high = 0;
            }
        }
    }

    // return correct status
    if (matched == 1) {
        return EISA_OK;
    } else {
        return EISA_ARG_ERR;
    }
}

EISA_STAT eisa_bus_read (EISA_BUS* eisa, uint32 eisa_address, uint8 size, uint32* value)
{
    int indx;

    // dispatch I/O to registered device
    for (indx=0; indx<eisa->registered; indx++) {
        if ((eisa_address >= eisa->registration[indx].low) && (eisa_address <= eisa->registration[indx].high)) {
            return (*eisa->registration[indx].device->read) (eisa_address, size, value);
        }
    }

    // did not find a device claiming I/O address - this may need to return an NMI or SERR error instead
    *value = 0;
    return EISA_NOT_ME;
}

EISA_STAT eisa_bus_write (EISA_BUS* eisa, uint32 eisa_address, uint8 size, uint32* value)
{
    int indx;

    // dispatch I/O to registered device
    for (indx=0; indx<eisa->registered; indx++) {
        if ((eisa_address >= eisa->registration[indx].low) && (eisa_address <= eisa->registration[indx].high)) {
            return (*eisa->registration[indx].device->write) (eisa_address, size, value);
        }
    }

    // did not find a device claiming I/O address - this may need to return an NMI or SERR error instead
    return EISA_NOT_ME;
}

EISA_STAT eisa_bus_dma_read (EISA_BUS* eisa, uint32 eisa_address, uint8 size, uint32* value, int repeat)
{
    int indx;

    // dispatch I/O to registered device
    for (indx=0; indx<eisa->registered; indx++) {
        if ((eisa_address >= eisa->registration[indx].low) && (eisa_address <= eisa->registration[indx].high)) {
            return (*eisa->registration[indx].device->dma_read) (eisa_address, size, value, repeat);
        }
    }

    // did not find a device claiming I/O address - this may need to return an NMI or SERR error instead
    *value = 0;
    return EISA_NOT_ME;
}

EISA_STAT eisa_bus_dma_write (EISA_BUS* eisa, uint32 eisa_address, uint8 size, uint32* value, int repeat)
{
    int indx;

    // dispatch I/O to registered device
    for (indx=0; indx<eisa->registered; indx++) {
        if ((eisa_address >= eisa->registration[indx].low) && (eisa_address <= eisa->registration[indx].high)) {
            return (*eisa->registration[indx].device->dma_write) (eisa_address, size, value, repeat);
        }
    }

    // did not find a device claiming I/O address - this may need to return an NMI or SERR error instead
    return EISA_NOT_ME;
}
