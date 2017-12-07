/* alpha_miata_rom.c: Alpha PWS 500au (Miata) default ROM

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
    This is the default Alpha PWS 500au (Miata) Flash ROM. It will be used when
    no .ROM file has been mapped into the Alpha simulator, or when the mapped
    .ROM file does not exist.

    This Miata ROM was dumped from an Alpha PWS 500au in SRM mode using the command
        >>> examine -l -physical -n 300000 ffc000000
    to dump the entire Pyxis-mapped "high" ROM range of F.FC00.0000 - F.FFFF.FFFF,
    then searched to find where the pattern repeated to size the ROM.
    [REF: "Digital Semiconductor 21174 Core Logic Chip Technical Reference Manual",
		  Order Number: EC-R12GC-TE, Section 4.9, Flash ROM space]

	The first longwords at F.FC00.0000 are:
		0x77FF9201
		0x77FF010F
		0x201F0FFC
		0x48031720

    The ROM pattern repeats in 1MB increments at:
        F.FC00.0000
        F.FC10.0000
        F.FC20.0000
        ...
       1F.FFF0.0000

    The ROM was then re-dumped with the instruction translations, using:
        >>> examine -d -n 40000 ffc000000
    and then massaged with a bulk text editor into the current format.

    The Miata contains a full-flash ROM, meaning that it contains both SRM and AlphaBIOS.
    You can switch back and forth between SRM (for OpenVMS and UNIX variants) and
	AlphaBIOS (for Windows NT) using the console.
		In SRM, from the console serial port:
			>>> set os_type NT		! valid os_type values are: {NT|VMS|OpenVMS|OSF|UNIX}
			>>> (power cycle - reboots to AlphaBIOS)
		In AlphaBIOS, from the Keyboard/Video/Mouse (KVM):
			Press F2 during power-up to enter setup
			CMOS Setup > Advanced CMOS Setup > Console Selection > {OpenVMS Console (SRM)|DIGITAL UNIX Console (SRM)}
			Press F10 twice to save
			(power cycle - reboots to SRM)

	References:
	  http://www.tldp.org/HOWTO/SRM-HOWTO/index.html
	  "Digital Personal Workstation au-Series Operating System Dual Boot Installation Guide", EK-ALUNX-OS.C01, July 1998

*/

#include "sim_defs.h"
#include "alpha_sys_defs.h"

extern uint32 miata_default_rom[262144];

t_bool patch_miata_default_rom (t_uint64 addr, uint32 prev_instr, uint32 new_instr)
{
    t_uint64 rom_addr = (addr & (ROMSIZE - 1)) >> 2;
    if (miata_default_rom[rom_addr] == new_instr) {
        return TRUE;    // already patched
    }
    if (miata_default_rom[rom_addr] == prev_instr) {
        miata_default_rom[rom_addr] = new_instr;
        return TRUE;    // patch successful
    }
    printf ("patch_miata_default_rom: Failed to patch location %llx\n", addr);
    return FALSE;
}

void patch_rom (void)
{
#define INS_NOP    0x47FF041F      // BIS  R31,R31,R31
    // Change cycle count multiplier in R16 from 0x2000 to 1 to shorten
    // initial cycle count test delay from 362.0000 cycles to 1B1 cycles
    // LDAH R16, 2(R31) --> LDA R16, 1(R31)
//    patch_miata_default_rom (0xFFC000028, 0x261F0002, 0x221F0001);

    
    //patch_miata_default_rom (0xFFC000070, 0xD340009E, INS_NOP);
    //patch_miata_default_rom (0xFFC000074, 0xD340036A, INS_NOP);

    //patch_miata_default_rom (0xFFC0000CC, 0xD3A00AB9, INS_NOP);
}
