/* pci_sym53c8xx.h: PCI SCSI adapter, using SYMBIOS 53C8xx chips

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
   THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of the author shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author.

*/

/*
	This SCSI PCI adapter is based on the following chips:
       Symbios SYM53C810 (KZPAA-AA) and SYM53C895 (KZPCA-AA)

	Documentation:
		"Symbios Logic PCI-SCSI I/O Processors Programming Guide", Version 2.1, J259721
        "SYM53C810A PCI-SCSI I/O Processor Data Manual", Version 2.0, T079621
        "SYM53C895 PCI-Ultra2 SCSI I/O Processor with LVDlink Universal Transceivers Data Manual", Version 2.0, T599721
        "Symbios SCSI SCRIPTS Processors Programming Guide", Version 2.2, S14044
*/

/*
	History:
	--------
	2016-04-11  DTH  Started
*/

// #include protection wrapper
#ifndef __pci_sym53c8xx_h
#define __pci_sym53c8xx_h

#include "sim_defs.h"

#define SYM_CFG0_DEVID  0xFFFF0000ul    // Device ID register
#define SYM_CFG0_VENID  0x0000FFFFul    // Vendor ID register

#define SYM_CFG1_PERR   0x80000000ul    // Parity Error Detected (not necessarily Reported)
#define SYM_CFG1_SERR   0x40000000ul    // SERR asserted
#define SYM_CFG1_MABORT 0x20000000ul    // Master Abort
#define SYM_CFG1_TABORT 0x10000000ul    // Target Abort
#define SYM_CFG1_DEVTIM 0x06000000ul    // DEVSEL/Timing
#define SYM_CFG1_PARREP 0x01000000ul    // Parity Error Reported
#define SYM_CFG1_ESERR  0x00000100ul    // Enable SERR
#define SYM_CFG1_EPERR  0x00000040ul    // Enable Parity Error response
#define SYM_CFG1_WIM    0x00000010ul    // Write and Invalidate Mode
#define SYM_CFG1_BMENA  0x00000004ul    // Enable Bus Mastering
#define SYM_CFG1_MENA   0x00000002ul    // Memory Enable
#define SYM_CFG1_IOENA  0x00000001ul    // I/O Enable
#define SYM_CFG1_STATUS  0xFFFF0000ul   // Status register
#define SYM_CFG1_COMMAND 0x0000FFFFul   // Command register

// Controller type
#define SYM53CUND 0   // undefined
#define SYM53C810 1   // sym53c810, 8-bit narrow scsi-2 controller (DEC KZPAA)
#define SYM53C895 2   // sym53c895, 16-bit wide ultra-2 controller (DEC KZPCA)

// Register 00: SCNTL0: SCSI Control 0
#define   R_SCNTL0 0x00;
#define   R_SCNTL0_ARB1 0x80;     /**< ARB1 */
#define   R_SCNTL0_ARB0 0x40;     /**< ARB0 */
#define   R_SCNTL0_START 0x20;    /**< START */
#define   R_SCNTL0_WATN 0x10;     /**< WATN */
#define   R_SCNTL0_EPC 0x08;      /**< EPC */
#define   R_SCNTL0_AAP 0x02;      /**< AAP */
#define   R_SCNTL0_TRG 0x01;      /**< TRG */
#define   SCNTL0_MASK 0xFB;       /**< Writeable bits */

// Register 01: SCNTL1: SCSI Control 1
#define   R_SCNTL1 0x01;
#define   R_SCNTL1_ADB 0x40;
#define   R_SCNTL1_CON 0x10;      /**< CON */
#define   R_SCNTL1_RST 0x08;      /**< RST */
#define   R_SCNTL1_AESP 0x04;
#define   R_SCNTL1_IARB 0x02;     /**< IARB */
#define   R_SCNTL1_SST 0x01;

// Register 02: SCNTL2: SCSI Control 2
#define   R_SCNTL2 0x02;
#define   R_SCNTL2_SDU 0x80;      /**< SDU */
#define   R_SCNTL2_CHM 0x40;      /**< CHM */
#define   R_SCNTL2_SLPMD 0x20;    /**< SLPMD */
#define   R_SCNTL2_SLPHBEN 0x10;  /**< SLPHBEN */
#define   R_SCNTL2_WSS 0x08;      /**< WSS */
#define   R_SCNTL2_VUE0 0x04;     /**< VUE0 */
#define   R_SCNTL2_VUE1 0x02;     /**< VUE1 */
#define   R_SCNTL2_WSR 0x01;      /**< WSR */
#define   A_SCNTL2_MASK 0x80;     /**< Writeable bits on 810 chip */
#define   B_SCNTL2_MASK 0xF2;     /**< Writeable bits on 895 chip */
#define   A_SCNTL2_W1C 0x00;      /**< Write-1-to-clear bits on 810 chip */
#define   B_SCNTL2_W1C 0x09;      /**< Write-1-to-clear bits on 895 chip */

// Register 03: SCNTL3: SCSI Control 3
#define   R_SCNTL3 0x03;
#define   R_SCNTL3_EWS 0x08;      /**< EWS */
#define   A_SCNTL3_MASK 0x77;     /**< Writeable bits on 810 chip */
#define   B_SCNTL3_MASK 0xFF;     /**< Writeable bits on 895 chip */

// Register 04: SCID: SCSI Chip ID
#define   R_SCID 0x04;
#define   A_R_SCID_ID 0x07;       /**< ID on 810 chip */
#define   A_SCID_MASK 0x67;       /**< Writeable bits on 810 chip */
#define   B_R_SCID_ID 0x0F;       /**< ID on 895 chip */
#define   B_SCID_MASK 0x6F;       /**< Writeable bits on 895 chip */
#define   R_SCID_RRE 0x40;

// Register 05: SXFER: SCSI Transfer
#define   R_SXFER 0x05;

// Register 06: SDID: SCSI Destination ID
#define   R_SDID 0x06;
#define   A_R_SDID_ID 0x07;       /**< ID on 810 chip */
#define   A_SDID_MASK 0x07;       /**< Writeable bits on 810 chip */
#define   B_R_SDID_ID 0x0F;       /**< ID on 895 chip */
#define   B_SDID_MASK 0x0F;       /**< Writeable bits on 895 chip */

// Register 07: GPREG: General Purpose
#define   R_GPREG 0x07;
#define   A_GPREG_MASK 0x03;      /**< Writeable bits on 810 chip */
#define   B_GPREG_MASK 0x1F;      /**< Writeable bits on 895 chip */

// Register 08: SFBR: SCSI First Byte REceived
#define   R_SFBR 0x08;

// Register 09: SOCL: SCSI Output Control Latch
#define   R_SOCL 0x09;
#define   R_SOCL_ACK 0x40;        /**< ACK */
#define   R_SOCL_ATN 0x20;        /**< ATN */

// Register 0A: SSID: SCSI Selector ID
#define   R_SSID 0x0A;
#define   R_SSID_VAL 0x80;        /**< VAL */
#define   A_R_SSID_ID 0x07;       /**< ID on 810 chip */
#define   B_R_SSID_ID 0x0F;       /**< ID on 895 chip */

// Register 0B: SBCL: SCSI Bus Control Lines
#define   R_SBCL 0x0B;
#define   R_SBCL_REQ 0x80;        /**< REQ */
#define   R_SBCL_ACK 0x40;        /**< ACK */
#define   R_SBCL_BSY 0x20;        /**< BSY */
#define   R_SBCL_SEL 0x10;        /**< SEL */
#define   R_SBCL_ATN 0x08;        /**< ATN */
#define   R_SBCL_MSG 0x04;        /**< MSG */
#define   R_SBCL_CD 0x02;         /**< CD */
#define   R_SBCL_IO 0x01;         /**< IO */
#define   R_SBCL_PHASE 0x07;      /**< PHASE */

// Register 0C: DSTAT: DMA Status
#define   R_DSTAT 0x0C;
#define   R_DSTAT_DFE 0x80;       /**< DFE */
#define   R_DSTAT_MDPE 0x40;      /**< MDPE */
#define   R_DSTAT_BF 0x20;        /**< BF */
#define   R_DSTAT_ABRT 0x10;      /**< ABRT */
#define   R_DSTAT_SSI 0x08;       /**< SSI */
#define   R_DSTAT_SIR 0x04;       /**< SIR */
#define   R_DSTAT_IID 0x01;       /**< IID */
#define   DSTAT_RC 0x7D;          /**< Read-to-clear bits */
#define   DSTAT_FATAL 0x7D;       /**< Fatal interrupt bits */

// Register 0D: SSTAT0: SCSI Status 0
#define   R_SSTAT0 0x0D;
#define   R_SSTAT0_RST 0x02;      /**< RST */
#define   R_SSTAT0_SDP0 0x01;     /**< SDP0 */

// Register 0E: SSTAT1: SCSI Status 1
#define   R_SSTAT1 0x0E;
#define   R_SSTAT1_SDP1 0x01;     /**< SDP1 */

// Register 0F: SSTAT2: SCSI Status 2
#define   R_SSTAT2 0x0F;
#define   R_SSTAT2_LDSC 0x02;     /**< LDSC */

// Register 10..13: DSA: Data Structure Address
#define   R_DSA 0x10;             /**< DSA */

// Register 14: ISTAT: Interrupt Status
#define   R_ISTAT 0x14;
#define   R_ISTAT_ABRT 0x80;      /**< ABRT */
#define   R_ISTAT_SRST 0x40;      /**< SRST */
#define   R_ISTAT_SIGP 0x20;      /**< SIGP */
#define   R_ISTAT_SEM 0x10;       /**< SEM */
#define   R_ISTAT_CON 0x08;       /**< CON */
#define   R_ISTAT_INTF 0x04;      /**< INTF */
#define   R_ISTAT_SIP 0x02;       /**< SIP */
#define   R_ISTAT_DIP 0x01;       /**< DIP */
#define   ISTAT_MASK 0xF0;        /**< Writeable bits */
#define   ISTAT_W1C 0x04;         /**< Write-1-to-clear bits */

// Register 18: CTEST0: Chip Test 0
#define   R_CTEST0 0x18;

// Register 19: CTEST1: Chip Test 1
#define   R_CTEST1 0x19;
#define   R_CTEST1_FMT 0xF0;      /**< FMT */
#define   R_CTEST1_FFL 0x0F;      /**< FFL */

// Register 1A: CTEST2: Chip Test 2
#define   R_CTEST2 0x1A;
#define   R_CTEST2_DDIR 0x80;     /**< DDIR */
#define   R_CTEST2_SIGP 0x40;     /**< SIGP */
#define   R_CTEST2_CIO 0x20;      /**< CIO */
#define   R_CTEST2_CM 0x10;       /**< CM */
#define   R_CTEST2_SRTCH 0x08;    /**< SRTCH */
#define   R_CTEST2_TEOP 0x04;     /**< TEOP */
#define   R_CTEST2_DREQ 0x02;     /**< DREQ */
#define   R_CTEST2_DACK 0x01;     /**< DACK */
#define   A_CTEST2_MASK 0x00;     /**< Writeable bits on 810 chip */
#define   B_CTEST2_MASK 0x08;     /**< Writeable bits on 895 chip */

// Register 1B: CTEST3: Chip Test 3
#define   R_CTEST3 0x1B;
#define   R_CTEST3_REV 0xF0;      /**< REV */
#define   R_CTEST3_FLF 0x08;      /**< FLF */
#define   R_CTEST3_CLF 0x04;      /**< CLF */
#define   R_CTEST3_FM 0x02;       /**< FM */
#define   CTEST3_MASK 0x0B;       /**< Writeable bits */

// Register 1C..1F: TEMP: Temporary
#define   R_TEMP 0x1C;

// Register 20: DFIFO: DMA FIFO
#define   R_DFIFO 0x20;

// Register 21: CTEST4: Chip Test 4
#define   R_CTEST4 0x21;
#define   R_CTEST4_SRTM 0x10;
#define   R_CTEST4_ZSD  0x20;
#define   R_CTEST4_ZMOD 0x40;

// Register 22: CTEST5: Chip Test 5
#define   R_CTEST5 0x22;
#define   R_CTEST5_ADCK 0x80;     /**< ADCK */
#define   R_CTEST5_BBCK 0x40;     /**< BBCK */
#define   A_CTEST5_MASK 0x18;     /**< Writeable bits on 810 chip */
#define   B_CTEST5_MASK 0x3F;     /**< Writeable bits on 895 chip */

// Register 23: CTEST6: Chip Test 6
#define   R_CTEST6 0x23;

// Register 24..26: DBC: DMA Byte Counter
#define   R_DBC 0x24;

// Register 27: DCMD: DMA Command
#define   R_DCMD 0x27;

// Register 28..2B: DNAD: DMA Next Address 
#define   R_DNAD 0x28;

// Register 2C..2F: DSP: DMA SCRIPTS Pointer
#define   R_DSP 0x2C;

// Register 30..33: DSPS: DMA SCRIPTS Pointer Save
#define   R_DSPS 0x30;

// Register 34..37: SCRATCHA: Scratch Register A
#define   R_SCRATCHA 0x34;

// Register 38: DMODE: DMA Mode
#define   R_DMODE 0x38;
#define   R_DMODE_MAN 0x01;       /**< MAN */
#define   R_DMODE_DIOM 0x10;
#define   R_DMODE_SIOM 0x20;

// Register 39: DIEN: DMA Interrupt Enable
#define   R_DIEN 0x39;
#define   DIEN_MASK 0x7D;         /**< Writeable bits */

// Register 3A: SBR: Scratch Byte Register
#define   R_SBR 0x3A;

// Register 3B: DCNTL: DMA Control
#define   R_DCNTL 0x3B;
#define   R_DCNTL_SSM 0x10;       /**< SSM */
#define   R_DCNTL_STD 0x04;       /**< STD */
#define   R_DCNTL_IRQD 0x02;      /**< IRQD */
#define   R_DCNTL_COM 0x01;       /**< COM */
#define   DCNTL_MASK 0xFB;        /**< Writeable bits */

// Register 3C..37: ADDER: Adder Sum Output
#define   R_ADDER 0x3C;

// Register 40: SIEN0: SCSI Interrupt Enable 0
#define   R_SIEN0 0x40;
#define   SIEN0_MASK 0xFF;        /**< Writeable bits */

// Register 41: SIEN1: SCSI Interrupt Enable 1
#define   R_SIEN1 0x41;
#define   A_SIEN1_MASK 0x07;      /**< Writeable bits on 810 chip */
#define   B_SIEN1_MASK 0x17;      /**< Writeable bits on 895 chip */

// Register 42: SIST0: SCSI Interrupt Status 0
#define   R_SIST0 0x42;
#define   R_SIST0_MA 0x80;        /**< MA */
#define   R_SIST0_CMP 0x40;       /**< CMP */
#define   R_SIST0_SEL 0x20;       /**< SEL */
#define   R_SIST0_RSL 0x10;       /**< RSL */
#define   R_SIST0_SGE 0x08;       /**< SGE */
#define   R_SIST0_UDC 0x04;       /**< UDC */
#define   R_SIST0_RST 0x02;       /**< RST */
#define   R_SIST0_PAR 0x01;       /**< PAR */
#define   SIST0_RC 0xFF;          /**< Read-to-clear bits */
#define   SIST0_FATAL 0x8F;       /**< Fatal interrupt bits */

// Register 43: SIST1: SCSI Interrupt Status 1
#define   R_SIST1 0x43;
#define   R_SIST1_SBMC 0x10;      /**< SBMC */
#define   R_SIST1_STO 0x04;       /**< STO */
#define   R_SIST1_GEN 0x02;       /**< GEN */
#define   R_SIST1_HTH 0x01;       /**< HTH */
#define   A_SIST1_RC 0x07;        /**< read-to-clear bits on 810 chip */
#define   A_SIST1_FATAL 0x04;     /**< Fatal interrupt bits on 810 chip*/
#define   B_SIST1_RC 0x17;        /**< read-to-clear bits on 895 chip */
#define   B_SIST1_FATAL 0x14;     /**< Fatal interrupt bits on 895 chip*/

// Register 44: SLPAR: SCSI Longitudinal Parity
#define   R_SLPAR 0x44;

// Register 45: SWIDE: SCSI Wide Residue
#define   R_SWIDE 0x45;

// Register 46: MACNTL: Memory Access Control
#define   R_MACNTL 0x46;
#define   R_MACNTL_SCPTS 0x01;
#define   R_MACNTL_PSCPT 0x02;
#define   R_MACNTL_DRD 0x04;
#define   R_MACNTL_DWR 0x08;
#define   MACNTL_MASK 0x0F;       /**< Writeable bits */

// Register 47: GPCNTL: General Purpose Pin Control
#define   R_GPCNTL 0x47;

// Register 48: STIME0: SCSI Timer 0
#define   R_STIME0 0x48;

// Register 49: STIME1: SCSI Timer 1
#define   R_STIME1 0x49;
#define   R_STIME1_GEN 0x0F;      /**< GEN */
#define   A_STIME1_MASK 0x0F;     /**< Writeable bits on 810 chip */
#define   B_STIME1_MASK 0x7F;     /**< Writeable bits on 895 chip */

// Register 4A: RESPID: SCSI Response ID
#define   R_RESPID 0x4A;

// Register 4C: STEST0: SCSI Test 0
#define   R_STEST0 0x4C;

// Register 4D: STEST1: SCSI Test 1
#define   R_STEST1 0x4D;
#define   R_STEST1_SISO 0x40;
#define   A_STEST1_MASK 0xC0;     /**< Writeable bits on 810 chip */
#define   B_STEST1_MASK 0xCC;     /**< Writeable bits on 895 chip */

// Register 4E: STEST2: SCSI Test 2
#define   R_STEST2 0x4E;
#define   R_STEST2_SCE 0x80;      /**< SCE */
#define   R_STEST2_ROF 0x40;      /**< ROF */
#define   R_STEST2_DIF 0x20;      /**< DIF */
#define   R_STEST2_SLB 0x10;      /**< SLB */
#define   R_STEST2_SZM 0x08;      /**< SZM */
#define   R_STEST2_AWS 0x04;      /**< AWS */
#define   R_STEST2_EXT 0x02;      /**< EXT */
#define   R_STEST2_LOW 0x01;      /**< LOW */
#define   A_STEST2_MASK 0x9B;     /**< Writeable bits on 810 chip */
#define   B_STEST2_MASK 0xBF;     /**< Writeable bits on 895 chip */

// Register 4F: STEST3: SCSI Test 3
#define   R_STEST3 0x4F;
#define   R_STEST3_TE 0x80;       /**< TE */
#define   R_STEST3_STR 0x40;      /**< STR */
#define   R_STEST3_HSC 0x20;      /**< HSC */
#define   R_STEST3_DSI 0x10;      /**< DSI */
#define   R_STEST3_S16 0x08;      /**< S16 */
#define   R_STEST3_TTM 0x04;      /**< TTM */
#define   R_STEST3_CSF 0x02;      /**< CSF */
#define   R_STEST3_STW 0x01;      /**< STW */
#define   A_STEST3_MASK 0xF7;     /**< Writeable bits on 810 chip */
#define   B_STEST3_MASK 0xFF;     /**< Writeable bits on 895 chip */

// Register 50..51: SIDL: SCSI Input Data Latch
#define   R_SIDL 0x50;

// Register 52: STEST4: SCSI Test 4
#define   R_STEST4 0x52;

// Register 54..55: SODL: SCSI Output Data Latch
#define   R_SODL 0x54;

// Register 58: SBDL: SCSI Bus Data Lines
#define   R_SBDL 0x58;

// Registers 5C..5F: SCRATCHB: Scratch Register B
#define   R_SCRATCHB 0x5C;

// Register 60..7F: SCRATCHC..SCRATCHJ: Scratch Register C..J
#define   R_SCRATCHC 0x60;

#endif // __pci_sym53c8xx_h