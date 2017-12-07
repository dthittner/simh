/* pci_sym53c8xxx.c: PCI SCSI adapter, using SYMBIOS 53C8xx chips

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
From Digital Storage Adapters V2.1aa 18-Feb-1993 QB00EBP8:
    KZPAA-AA minimum OS versions
        Windows NT      3.5
        Digital UNIX    3.0
        OpenVMS         6.1 (SCSI Cluster 6.2)  
    KZPCA-AA minimum OS versions
        Windows NT      4.0 +SP3
        Tru64 UNIX      4.0F
        OpenVMS         7.1-2
*/
/* VPD information can be emitted from a SCSI device with sg_vpd(8) and sq_inq(8).
   sg3_utils(8) may also be of interest. scsi_readcap(8), sg_format(8), sg_luns(8) */
/*
	Documentation:
        Digital SCSI Disk sizes: http://www.ultimate.com/phil/pdp10/dec.disks
*/

/*
	History:
	--------
	2016-04-16  DTH  Core SIMH code copied from PDP11/pdp11_rq.c
*/

#include "alpha_defs.h"
#include "alpha_sys_defs.h"
#include "sim_pci.h"
#include "pci_sym53c8xx.h"

//#include "pdp11_uqssp.h"
//#include "pdp11_mscp.h"
//#include "sim_disk.h"

/* debugging bitmaps */
#define DBG_TRC  0x0001                                 /* trace routine calls */
#define DBG_INI  0x0002                                 /* display setup/init sequence info */
#define DBG_REG  0x0004                                 /* trace read/write registers */
#define DBG_REQ  0x0008                                 /* display transfer requests */
#define DBG_DSK  0x0010                                 /* display sim_disk activities */
#define DBG_DAT  0x0020                                 /* display transfer data */
#define DBG_PCI  0x0040                                 /* debug PCI transfers */
#define DBG_SCSI 0x0080                                 /* debug SCSI transactions */

/*
SCSI disks (Winchesters unless noted)
=====================================
disk	cap.	sec/trk	trk/cyl	cyl	notes

=== 5.25" FH

RZ55	325M				Micropolis 1578-15
					or maxtor xt-4380sb2?
RZ55L
RZ56	650M	54	15	1632	Micropolis 1588-15
RZ56L
RZ57	1.01G	71	15	1925	Micropolis 1598-15
RZ57I
RZ57L
RZ58	1.3G	85~	15	2117	5400rpm? Micropolis 1908?
RZ59	8.9G	193	18	5111

=== 5.25" FH?

RZ72
RZ73	2.0G	71	21	2621
RZ74	3.57G	67~	25	4165

=== 3.5" HH?

RZ22	51M	33	4	776	Conner CP350
RZ23	102M	33	8	776	Conner CP3100-1
RZ23L	118M	39~	4	1524
RZ24	205M	38	8	1348	Conner CP3200; 3500rpm?
RZ24L	240M	66~	8	1818	Quantum LPS-240S?
RZ25	416M	62	9	1492
RZ25L	523M	79~	8	1891
RZ26	1.05G	57	14	2570	5400rpm?
RZ27	1.6G	143~	16	1366
RZ28	2.1G	99~	16	2595	DEC manufactured (ST32550N???)
RZ28B	2.1G	82~	19	2626	Seagate ST12400N; 5411 rpm; seek 10/2/22
RZ29	4.2G	113~	20	3720	Quantum?

================ 5" HH?

RZ31
RZ33	??
RZ35	832M	57	14	2086

================ 5" FH??

RZ55	332M
RZ56	635M
RZ57	1G
RZ58	1.3G

================ 5" FH

RZ73	2G
RZ74	3.57G

================ CD-ROM (IDE/ATAPI)

RRD20?					PHILLIPS 2X IDE/ATAPI CDROM
RRD32?					32X ATAPI CD-ROM
RRD37					Toshiba XM-5201B

================ CD-ROM (SCSI)

RRD40					Laser Magnetics LMS CM 210 (no audio)
RRD42					1X Sony CDU-541
RRD43					2X Toshiba XM-4101B "1/3 height"
RRD44					2X Toshiba XM-3401B "high performance"
RRD45					4X Toshiba XM-5401B
RRD46					12x Toshiba XM-6302B (or XM-5701B?)
RRD47					32x Toshiba XM-6201B
RRD50					Philips/LMSI CM100 (no audio)

================ Optical WORM

RWZ01	288M	31	1	18751	Erasable Optical 5.25"
					(Sony) EDM-1DA0/1DA1/650/600

RWZ21					WORM 3.5" (MO)

RV20	6GB?				Optical WORM 12"
RSV20					Optical WORM
RV60					12"
RV64					Jukebox (RV20 based)

RWZ52	1.2G				5.25"; rewritable; 600MB/side
					HC: acc 36ms; r 1.6MB/s; w 0.53MB/s
					LC: acc 38ms; r 1.0MB/s; w 0.33MB/s

RVZ72	6.55G				tabletop 12" write once; SCSI
					access 600ms; read 900KB/s; w 400 KB/s
RV720	78GB				deskside jukebox w/ 1 drive, 12 disks
RV730ZB	438GB				datacenter jukebox w/ 2 drives; 67 d.
RV730ZD	308GB				datacenter jukebox w/ 4 drives; 47 d.
					

Decimage Express v2?			LMS 5.25"/12" WORM 

Solid-state disks (SCSI?)
=========================

"disk"	cap.	sec/trk	trk/cyl	cyl	notes

EZ51	104M	33	9	776
EZ54	418M	62	10	1492
EZ58	835M	20	10	8353
*/

#if 0
#define UF_MSK          (UF_CMR|UF_CMW)                 /* settable flags */

#define PK_SH_MAX       24                              /* max display wds */
#define PK_SH_PPL       8                               /* wds per line */
#define PK_SH_DPL       4                               /* desc per line */
#define PK_SH_RI        001                             /* show rings */
#define PK_SH_FR        002                             /* show free q */
#define PK_SH_RS        004                             /* show resp q */
#define PK_SH_UN        010                             /* show unit q's */
#define PK_SH_ALL       017                             /* show all */

#define PK_CLASS        1                               /* PK class */
#define PKU_UQPM        6                               /* UB port model */
#define PKQ_UQPM        19                              /* QB port model */
#define PK_UQPM         (UNIBUS? PKU_UQPM: PKQ_UQPM)
#define PKU_MODEL       6                               /* UB MSCP ctrl model (UDA50A) */
#define PKQ_MODEL       19                              /* QB MSCP ctrl model (PKDX3) */
#define PK_MODEL        (UNIBUS? PKU_MODEL: PKQ_MODEL)
#define PK_HVER         1                               /* hardware version */
#define PK_SVER         3                               /* software version */
#define PK_DHTMO        60                              /* def host timeout */
#define PK_DCTMO        120                             /* def ctrl timeout */
#define PK_NUMDR        4                               /* # drives */
#define PK_NUMBY        512                             /* bytes per block */
#define PK_MAXFR        (1 << 16)                       /* max xfer */
#define PK_MAPXFER      (1u << 31)                      /* mapped xfer */
#define PK_M_PFN        0x1FFFFF                        /* map entry PFN */

#define UNIT_V_ONL      (UNIT_V_UF + 0)                 /* online */
#define UNIT_V_WLK      (UNIT_V_UF + 1)                 /* hwre write lock */
#define UNIT_V_ATP      (UNIT_V_UF + 2)                 /* attn pending */
#define UNIT_V_DTYPE    (UNIT_V_UF + 3)                 /* drive type */
#define UNIT_M_DTYPE    0x1F
#define UNIT_V_NOAUTO   (UNIT_V_UF + 8)                 /* noautosize */
#define UNIT_ONL        (1 << UNIT_V_ONL)
#define UNIT_WLK        (1 << UNIT_V_WLK)
#define UNIT_ATP        (1 << UNIT_V_ATP)
#define UNIT_NOAUTO     (1 << UNIT_V_NOAUTO)
#define UNIT_DTYPE      (UNIT_M_DTYPE << UNIT_V_DTYPE)
#define GET_DTYPE(x)    (((x) >> UNIT_V_DTYPE) & UNIT_M_DTYPE)
#define cpkt            u3                              /* current packet */
#define pktq            u4                              /* packet queue */
#define uf              buf                             /* settable unit flags */
#define cnum            wait                            /* controller index */
#define io_status       u5                              /* io status from callback */
#define io_complete     u6                              /* io completion flag */
#define rqxb            filebuf                         /* xfer buffer */
#define UNIT_WPRT       (UNIT_WLK | UNIT_RO)            /* write prot */
#define PK_RMV(u)       ((drv_tab[GET_DTYPE (u->flags)].flgs & PKDF_RMV)? \
                        UF_RMV: 0)
#define PK_WPH(u)       (((drv_tab[GET_DTYPE (u->flags)].flgs & PKDF_RO) || \
                        (u->flags & UNIT_WPRT) || sim_disk_wrp (u))? UF_WPH: 0)

#define CST_S1          0                               /* init stage 1 */
#define CST_S1_WR       1                               /* stage 1 wrap */
#define CST_S2          2                               /* init stage 2 */
#define CST_S3          3                               /* init stage 3 */
#define CST_S3_PPA      4                               /* stage 3 sa wait */
#define CST_S3_PPB      5                               /* stage 3 ip wait */
#define CST_S4          6                               /* stage 4 */
#define CST_UP          7                               /* online */
#define CST_DEAD        8                               /* fatal error */

#define ERR             0                               /* must be SCPE_OK! */
#define OK              1

#define PK_TIMER        (PK_NUMDR)
#define PK_QUEUE        (PK_TIMER + 1)

/* Internal packet management.  The real PKDX3 manages its packets as true
   linked lists.  However, use of actual addresses in structures won't work
   with save/restore.  Accordingly, the packets are an arrayed structure,
   and links are actually subscripts.  To minimize complexity, packet[0]
   is not used (0 = end of list), and the number of packets must be a power
   of two.
*/

#define PK_NPKTS        32                              /* # packets (pwr of 2) */
#define PK_M_NPKTS      (PK_NPKTS - 1)                  /* mask */
#define PK_PKT_SIZE_W   32                              /* payload size (wds) */
#define PK_PKT_SIZE     (PK_PKT_SIZE_W * sizeof (int16))

struct rqpkt {
    uint16      link;                                   /* link to next */
    uint16      d[PK_PKT_SIZE_W];                       /* data */
    };

/* Packet payload extraction and insertion; cp defines controller */

#define GETP(p,w,f)     ((cp->pak[p].d[w] >> w##_V_##f) & w##_M_##f)
#define GETP32(p,w)     (((uint32) cp->pak[p].d[w]) | \
                        (((uint32) cp->pak[p].d[(w)+1]) << 16))
#define PUTP32(p,w,x)   cp->pak[p].d[w] = (x) & 0xFFFF; \
                        cp->pak[p].d[(w)+1] = ((x) >> 16) & 0xFFFF

/* Disk formats.  A SCSI disk contains:

   LBNs         Logical Block Numbers - blocks of 512 bytes, numbered from 0...N

*/

#define RCT_OVHD        2                               /* #ovhd blks */
#define RCT_ENTB        128                             /* entries/blk */
#define RCT_END         0x80000000                      /* marks RCT end */

/* In general, SCSI controllers support any device which supports the mandatory
SCSI-2 command set. Certain controllers and OS's have higher requirements depending
on the capability required from the device.

SCSI buses are considered to be a "universal"  bus in that  any SCSI devicecan be attached
to the controller as long as the SCSI bus length, speed, and termination
are correctly observed.

Digital's SCSI devices used the "Z" designator in the name to indicate that the device had a SCSI connector.
There were the RZxx (Rotating scuZzy) disk drives, the TZxx (Tape scuZzy) tape drives,
and the RRZxx (Read-only Rotating scuZzy) CDROM drives.

Digital's OpenVMS and Microsoft Windows NT aren't picky about the geometry of the disk drive, and only address the device by LBNs.
Digital (Tru64) Unix, OSF1, xBSD, and Linux care about the geometry as they carve the device up into multiple partitions.

SCSI devices have a device indentification built into them which can be queried with a SCSI read command.

   type sec     surf    cyl     tpg     gpc     RCT     LBNs        Size
        
   RX50 10      1       80      5       16      -       800
   RZ25 62      9       1492    ?       ?       -       832536      406MB

   Each device can be a different type.  The drive field in the
   unit flags specified the device type and thus, indirectly,
   the drive size.
*/


struct drvtyp {
    uint16      sect;                                   /* sectors */
    int32       surf;                                   /* surfaces */
    int32       cyl;                                    /* cylinders */
    uint16      tpg;                                    /* trk/grp */
    uint16      gpc;                                    /* grp/cyl */
    int32       xbn;                                    /* XBN size */
    int32       dbn;                                    /* DBN size */
    uint32      lbn;                                    /* LBN size */
    uint16      rcts;                                   /* RCT size */
    int32       rctc;                                   /* RCT copies */
    int32       rbn;                                    /* RBNs */
    uint16      mod;                                    /* MSCP model */
    int32       med;                                    /* MSCP media */
    int32       flgs;                                   /* flags */
    const char  *name;                                  /* name */
    };

#define RRD40_DTYPE     8
#define RRD40_SECT      128
#define RRD40_SURF      1
#define RRD40_CYL       10400
#define RRD40_TPG       RRD40_SURF
#define RRD40_GPC       1
#define RRD40_XBN       0
#define RRD40_DBN       0
#define RRD40_LBN       1331200
#define RRD40_RCTS      0
#define RRD40_RCTC      0
#define RRD40_RBN       0
#define RRD40_MOD       26
#define RRD40_MED       0x25652228
#define RRD40_FLGS      (RQDF_RMV | RQDF_RO)

#define RZ25_DTYPE      9
#define RZ25_SECT       62
#define RZ25_SURF       9
#define RZ25_CYL        191492
#define RZ25_TPG        0
#define RZ25_GPC        0
#define RZ25_XBN        2080
#define RZ25_DBN        2080
#define RZ25_LBN        832536  // 406MB
#define RZ25_RCTS       0
#define RZ25_RCTC       0
#define RZ25_RBN        0
#define RZ25_MOD        0
#define RZ25_MED        0
#define RZ25_FLGS       0


#define PK_DRV(d) \
  { d##_SECT, d##_SURF, d##_CYL,  d##_TPG, \
    d##_GPC,  d##_XBN,  d##_DBN,  d##_LBN, \
    d##_RCTS, d##_RCTC, d##_RBN,  d##_MOD, \
    d##_MED, d##_FLGS, #d }
#define PK_SIZE(d)      d##_LBN

static struct drvtyp drv_tab[] = {
//    PK_DRV (RA73),
    { 0 }
    };



DEBTAB sym_debug[] = {
  {"TRACE",  DBG_TRC},
  {"INIT",   DBG_INI},
  {"REG",    DBG_REG},
  {"REQ",    DBG_REQ},
  {"DISK",   DBG_DSK},
  {"DATA",   DBG_DAT},
  {0}
};


t_stat sym_rd (int32 *data, int32 PA, int32 access);
t_stat sym_wr (int32 data, int32 PA, int32 access);
t_stat sym_svc (UNIT *uptr);
t_stat sym_tmrsvc (UNIT *uptr);
t_stat sym_quesvc (UNIT *uptr);
t_stat sym_reset (DEVICE *dptr);
t_stat sym_attach (UNIT *uptr, char *cptr);
t_stat sym_detach (UNIT *uptr);
t_stat sym_boot (int32 unitno, DEVICE *dptr);
t_stat sym_set_wlk (UNIT *uptr, int32 val, char *cptr, void *desc);
t_stat sym_set_type (UNIT *uptr, int32 val, char *cptr, void *desc);
t_stat sym_set_ctype (UNIT *uptr, int32 val, char *cptr, void *desc);
t_stat sym_show_type (FILE *st, UNIT *uptr, int32 val, void *desc);
t_stat sym_show_ctype (FILE *st, UNIT *uptr, int32 val, void *desc);
t_stat sym_show_wlk (FILE *st, UNIT *uptr, int32 val, void *desc);
t_stat sym_show_ctrl (FILE *st, UNIT *uptr, int32 val, void *desc);
t_stat sym_show_unitq (FILE *st, UNIT *uptr, int32 val, void *desc);
t_stat sym_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
const char *sym_description (DEVICE *dptr);
#endif

#if 0
t_bool sym_step4 (MSC *cp);
t_bool sym_mscp (MSC *cp, uint16 pkt, t_bool q);
t_bool sym_abo (MSC *cp, uint16 pkt, t_bool q);
t_bool sym_avl (MSC *cp, uint16 pkt, t_bool q);
t_bool sym_fmt (MSC *cp, uint16 pkt, t_bool q);
t_bool sym_gcs (MSC *cp, uint16 pkt, t_bool q);
t_bool sym_gus (MSC *cp, uint16 pkt, t_bool q);
t_bool sym_onl (MSC *cp, uint16 pkt, t_bool q);
t_bool sym_rw (MSC *cp, uint16 pkt, t_bool q);
t_bool sym_scc (MSC *cp, uint16 pkt, t_bool q);
t_bool sym_suc (MSC *cp, uint16 pkt, t_bool q);
t_bool sym_plf (MSC *cp, uint16 err);
t_bool sym_dte (MSC *cp, UNIT *uptr, uint16 err);
t_bool sym_hbe (MSC *cp, UNIT *uptr);
t_bool sym_una (MSC *cp, uint16 un);
t_bool sym_deqf (MSC *cp, uint16 *pkt);
uint16 sym_deqh (MSC *cp, uint16 *lh);
void sym_enqh (MSC *cp, uint16 *lh, uint16 pkt);
void sym_enqt (MSC *cp, uint16 *lh, uint16 pkt);
t_bool sym_getpkt (MSC *cp, uint16 *pkt);
t_bool sym_putpkt (MSC *cp, uint16 pkt, t_bool qt);
t_bool sym_getdesc (MSC *cp, struct uq_ring *ring, uint32 *desc);
t_bool sym_putdesc (MSC *cp, struct uq_ring *ring, uint32 desc);
uint16 sym_rw_valid (MSC *cp, uint16 pkt, UNIT *uptr, uint16 cmd);
t_bool sym_rw_end (MSC *cp, UNIT *uptr, uint16 flg, uint16 sts);
uint32 sym_map_ba (uint32 ba, uint32 ma);
int32 sym_readb (uint32 ba, int32 bc, uint32 ma, uint8 *buf);
int32 sym_readw (uint32 ba, int32 bc, uint32 ma, uint16 *buf);
int32 sym_writew (uint32 ba, int32 bc, uint32 ma, uint16 *buf);
void sym_putr (MSC *cp, uint16 pkt, uint16 cmd, uint16 flg,
    uint16 sts, uint16 lnt, uint16 typ);
void sym_putr_unit (MSC *cp, uint16 pkt, UNIT *uptr, uint16 lu, t_bool all);
void sym_setf_unit (MSC *cp, uint16 pkt, UNIT *uptr);
void sym_init_int (MSC *cp);
void sym_ring_int (MSC *cp, struct uq_ring *ring);
t_bool sym_fatal (MSC *cp, uint16 err);
UNIT *sym_getucb (MSC *cp, uint16 lu);
int32 sym_map_pa (uint32 pa);
void sym_setint (MSC *cp);
void sym_clrint (MSC *cp);
int32 sym_inta (void);
#endif

/* SYM data structures

   sym_dev       SYM device descriptor
   sym_unit      SYM unit list
   sym_reg       SYM register list
   sym_mod       SYM modifier list
*/

#if 0
MSC sym_ctx = { 0 };

#define IOLN_PK         004

DIB sym_dib = {
    IOBA_AUTO, IOLN_PK, &sym_rd, &sym_wr,
    1, IVCL (PK), 0, { &sym_inta }, IOLN_PK
    };

UNIT sym_unit[] = {
    { UDATA (&sym_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (RD54_DTYPE << UNIT_V_DTYPE), PK_SIZE (RD54)) },
    { UDATA (&sym_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (RD54_DTYPE << UNIT_V_DTYPE), PK_SIZE (RD54)) },
    { UDATA (&sym_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (RD54_DTYPE << UNIT_V_DTYPE), PK_SIZE (RD54)) },
    { UDATA (&sym_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+UNIT_ROABLE+
            (RX50_DTYPE << UNIT_V_DTYPE), PK_SIZE (RX50)) },
    { UDATA (&sym_tmrsvc, UNIT_IDLE|UNIT_DIS, 0) },
    { UDATA (&sym_quesvc, UNIT_DIS, 0) }
    };

REG sym_reg[] = {
    { GRDATAD (UBASE,   sym_ctx.ubase,   DEV_RDX,  8, 0, "unit base"), REG_RO },
    { GRDATAD (SA,      sym_ctx.sa,      DEV_RDX, 16, 0, "status/address register") },
    { GRDATAD (SAW,     sym_ctx.saw,     DEV_RDX, 16, 0, "written data") },
    { GRDATAD (S1DAT,   sym_ctx.s1dat,   DEV_RDX, 16, 0, "step 1 init host data") },
    { GRDATAD (COMM,    sym_ctx.comm,    DEV_RDX, 22, 0, "comm region") },
    { GRDATAD (CQIOFF,  sym_ctx.cq.ioff, DEV_RDX, 32, 0, "command queue intr offset") },
    { GRDATAD (CQBA,    sym_ctx.cq.ba,   DEV_RDX, 22, 0, "command queue base address") },
    { GRDATAD (CQLNT,   sym_ctx.cq.lnt,  DEV_RDX, 32, 2, "command queue length"), REG_NZ },
    { GRDATAD (CQIDX,   sym_ctx.cq.idx,  DEV_RDX,  8, 2, "command queue index") },
    { GRDATAD (PKIOFF,  sym_ctx.rq.ioff, DEV_RDX, 32, 0, "request queue intr offset") },
    { GRDATAD (PKBA,    sym_ctx.rq.ba,   DEV_RDX, 22, 0, "request queue base address") },
    { GRDATAD (PKLNT,   sym_ctx.rq.lnt,  DEV_RDX, 32, 2, "request queue length"), REG_NZ },
    { GRDATAD (PKIDX,   sym_ctx.rq.idx,  DEV_RDX,  8, 2, "request queue index") },
    { DRDATAD (FREE,    sym_ctx.freq,                 5, "head of free packet list") },
    { DRDATAD (RESP,    sym_ctx.rspq,                 5, "head of response packet list") },
    { DRDATAD (PBSY,    sym_ctx.pbsy,                 5, "number of busy packets") },
    { GRDATAD (CFLGS,   sym_ctx.cflgs,   DEV_RDX, 16, 0, "controller flags") },
    { GRDATAD (CSTA,    sym_ctx.csta,    DEV_RDX,  4, 0, "controller state") },
    { GRDATAD (PERR,    sym_ctx.perr,    DEV_RDX,  9, 0, "port error number") },
    { DRDATAD (CRED,    sym_ctx.credits,              5, "host credits") },
    { DRDATAD (HAT,     sym_ctx.hat,                 17, "host available timer") },
    { DRDATAD (HTMO,    sym_ctx.htmo,                17, "host timeout value") },
    { FLDATA  (PRGI,    sym_ctx.prgi,                 0), REG_HIDDEN },
    { FLDATA  (PIP,     sym_ctx.pip,                  0), REG_HIDDEN },
    { FLDATA  (CTYPE,   sym_ctx.ctype,               32), REG_HIDDEN  },
    { DRDATAD (ITIME,   sym_itime,                   24, "init time delay, except stage 4"), PV_LEFT + REG_NZ },
    { DRDATAD (I4TIME,  sym_itime4,                  24, "init stage 4 delay"), PV_LEFT + REG_NZ },
    { DRDATAD (QTIME,   sym_qtime,                   24, "response time for 'immediate' packets"), PV_LEFT + REG_NZ },
    { DRDATAD (XTIME,   sym_xtime,                   24, "response time for data transfers"), PV_LEFT + REG_NZ },
    { BRDATAD (PKTS,    sym_ctx.pak,     DEV_RDX,    16, sizeof(sym_ctx.pak)/2, "packet buffers, 33W each, 32 entries") },
    { URDATAD (CPKT,    sym_unit[0].cpkt, 10, 5, 0, PK_NUMDR, 0, "current packet, units 0 to 3") },
    { URDATAD (UCNUM,   sym_unit[0].cnum, 10, 5, 0, PK_NUMDR, 0, "ctrl number, units 0 to 3") },
    { URDATAD (PKTQ,    sym_unit[0].pktq, 10, 5, 0, PK_NUMDR, 0, "packet queue, units 0 to 3") },
    { URDATAD (UFLG,    sym_unit[0].uf,  DEV_RDX, 16, 0, PK_NUMDR, 0, "unit flags, units 0 to 3") },
    { URDATA  (CAPAC,   sym_unit[0].capac, 10, T_ADDR_W, 0, PK_NUMDR, PV_LEFT | REG_HRO) },
    { GRDATA  (DEVADDR, sym_dib.ba,      DEV_RDX, 32, 0), REG_HRO },
    { GRDATA  (DEVVEC,  sym_dib.vec,     DEV_RDX, 16, 0), REG_HRO },
    { DRDATA  (DEVLBN,  drv_tab[RA8U_DTYPE].lbn, 22), REG_HRO },
    { NULL }
    };

MTAB sym_mod[] = {
    { UNIT_WLK,                 0,  NULL, "WRITEENABLED", 
        &sym_set_wlk, NULL, NULL, "Write enable disk drive" },
    { UNIT_WLK,          UNIT_WLK,  NULL, "LOCKED", 
        &sym_set_wlk, NULL, NULL, "Write lock disk drive"  },
    { MTAB_XTD|MTAB_VUN, 0, "WRITE", NULL,
      NULL, &sym_show_wlk, NULL,  "Display drive writelock status" },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, PK_SH_RI, "RINGS", NULL,
      NULL, &sym_show_ctrl, NULL, "Display command and response rings" },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, PK_SH_FR, "FREEQ", NULL,
      NULL, &sym_show_ctrl, NULL, "Display free queue" },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, PK_SH_RS, "RESPQ", NULL,
      NULL, &sym_show_ctrl, NULL, "Display response queue" },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, PK_SH_UN, "UNITQ", NULL,
      NULL, &sym_show_ctrl, NULL, "Display all unit queues" },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, PK_SH_ALL, "ALL", NULL,
      NULL, &sym_show_ctrl, NULL, "Display complete controller state" },
    { MTAB_XTD|MTAB_VDV, PKDX3_CTYPE, NULL, "PKDX3",
      &sym_set_ctype, NULL, NULL, "Set PKDX3 (QBUS RX50/RDnn) Controller Type" },
    { MTAB_XTD|MTAB_VDV, UDA50_CTYPE, NULL, "UDA50",
      &sym_set_ctype, NULL, NULL, "Set UDA50 (UNIBUS SDI RAnn) Controller Type" },
    { MTAB_XTD|MTAB_VDV, KDA50_CTYPE, NULL, "KDA50",
      &sym_set_ctype, NULL, NULL, "Set KDA50 (QBUS SDI RAnn) Controller Type" },
    { MTAB_XTD|MTAB_VDV, KPK50_CTYPE, NULL, "KPK50",
      &sym_set_ctype, NULL, NULL, "Set KPK50 (QBUS CDROM) Controller Type" },
    { MTAB_XTD|MTAB_VDV, KRU50_CTYPE, NULL, "KRU50",
      &sym_set_ctype, NULL, NULL, "Set KRU50 (UNIBUS CDROM) Controller Type" },
    { MTAB_XTD|MTAB_VDV, KLESI_CTYPE, NULL, "KLESI",
      &sym_set_ctype, NULL, NULL, "Set KLESI (RC25) Controller Type"  },
    { MTAB_XTD|MTAB_VDV, RUX50_CTYPE, NULL, "RUX50",
      &sym_set_ctype, NULL, NULL, "Set RUX50 (UNIBUS RX50) Controller Type" },
    { MTAB_XTD|MTAB_VUN|MTAB_NMO, 0, "UNITQ", NULL,
      NULL, &sym_show_unitq, NULL, "Display unit queue" },
    { MTAB_XTD|MTAB_VUN, RX50_DTYPE, NULL, "RX50",
      &sym_set_type, NULL, NULL, "Set RX50 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RX33_DTYPE, NULL, "RX33",
      &sym_set_type, NULL, NULL, "Set RX33 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RD31_DTYPE, NULL, "RD31",
      &sym_set_type, NULL, NULL, "Set RD31 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RD32_DTYPE, NULL, "RD32",
      &sym_set_type, NULL, NULL, "Set RD32 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RD51_DTYPE, NULL, "RD51",
      &sym_set_type, NULL, NULL, "Set RD51 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RD52_DTYPE, NULL, "RD52",
      &sym_set_type, NULL, NULL, "Set RD52 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RD53_DTYPE, NULL, "RD53",
      &sym_set_type, NULL, NULL, "Set RD53 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RD54_DTYPE, NULL, "RD54",
      &sym_set_type, NULL, NULL, "Set RD54 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA60_DTYPE, NULL, "RA60",
      &sym_set_type, NULL, NULL, "Set RA60 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA81_DTYPE, NULL, "RA81",
      &sym_set_type, NULL, NULL, "Set RA81 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA82_DTYPE, NULL, "RA82",
      &sym_set_type, NULL, NULL, "Set RA82 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RRD40_DTYPE, NULL, "RRD40",
      &sym_set_type, NULL, NULL, "Set RRD40 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RRD40_DTYPE, NULL, "CDROM",
      &sym_set_type, NULL, NULL, "Set CDROM Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA70_DTYPE, NULL, "RA70",
      &sym_set_type, NULL, NULL, "Set RA70 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA71_DTYPE, NULL, "RA71",
      &sym_set_type, NULL, NULL, "Set RA71 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RZ25_DTYPE, NULL, "RZ25",
      &sym_set_type, NULL, NULL, "Set RZ25 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA73_DTYPE, NULL, "RA73",
      &sym_set_type, NULL, NULL, "Set RA73 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA90_DTYPE, NULL, "RA90",
      &sym_set_type, NULL, NULL, "Set RA90 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA92_DTYPE, NULL, "RA92",
      &sym_set_type, NULL, NULL, "Set RA92 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RC25_DTYPE, NULL, "RC25",
      &sym_set_type, NULL, NULL, "Set RC25 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RCF25_DTYPE, NULL, "RCF25",
      &sym_set_type, NULL, NULL, "Set RCF25 Disk Type" },
    { MTAB_XTD|MTAB_VUN, RA80_DTYPE, NULL, "RA80",
      &sym_set_type, NULL, NULL, "Set RA80 Disk Type" },
    { MTAB_XTD|MTAB_VUN|MTAB_VALR, RA8U_DTYPE, NULL, "RAUSER=SIZE",
      &sym_set_type, NULL, NULL, "Set RAUSER=size Disk Type" },
    { MTAB_XTD|MTAB_VUN, 0, "TYPE", NULL,
      NULL, &sym_show_type, NULL, "Display device type" },
    { UNIT_NOAUTO, UNIT_NOAUTO, "noautosize", "NOAUTOSIZE", NULL, NULL, NULL, "Disables disk autosize on attach" },
    { UNIT_NOAUTO,           0, "autosize",   "AUTOSIZE",   NULL, NULL, NULL, "Enables disk autosize on attach" },
    { MTAB_XTD|MTAB_VUN|MTAB_VALR, 0, "FORMAT", "FORMAT={SIMH|VHD|RAW}",
      &sim_disk_set_fmt, &sim_disk_show_fmt, NULL, "Display disk format" },
#if defined (VM_PDP11)
    { MTAB_XTD|MTAB_VDV|MTAB_VALR, 004, "ADDRESS", "ADDRESS",
      &set_addr, &show_addr, NULL, "Bus address" },
    { MTAB_XTD | MTAB_VDV, 0, NULL, "AUTOCONFIGURE",
      &set_addr_flt, NULL, NULL, "Enable autoconfiguration of address & vector" },
#else
    { MTAB_XTD|MTAB_VDV, 004, "ADDRESS", NULL,
      NULL, &show_addr, NULL, "Bus address" },
#endif
    { MTAB_XTD|MTAB_VDV, 0, "VECTOR", NULL,
      NULL, &show_vec, NULL, "Interrupt vector" },
    { MTAB_XTD | MTAB_VDV, 0, "TYPE", NULL,
      NULL, &sym_show_ctype, NULL, "Display controller type" },
    { 0 }
    };
#endif

UNIT pka_unit[] = {
    { NULL }
};

t_stat pka_reset (DEVICE* dev)
{
    // unfinished?
    return SCPE_OK;
}

DEVICE pka_dev = {
    "PKA",
    pka_unit,
    0/*sym_reg*/,
    0/*sym_mod*/,
    1,
    16,
    T_ADDR_W,
    2,
    16,
    16,
    NULL,
    NULL,
    pka_reset,
    0/*&sym_boot*/,
    0/*&sym_attach*/,
    0/*&sym_detach*/,
    0/*&sym_dib*/,
    DEV_DISABLE | DEV_PCI | DEV_DEBUG | DEV_DISK | DEV_SECTORS,
    0,
    0/*sym_debug*/,
    NULL,
    NULL,
    0/*&sym_help*/,
    NULL,
    NULL,
    0/*&sym_description*/
    };

#if 0
static DEVICE *sym_devmap[PK_NUMCT] = {
    &sym_dev, &rqb_dev, &rqc_dev, &rqd_dev
    };

static MSC *sym_ctxmap[PK_NUMCT] = {
    &sym_ctx, &rqb_ctx, &rqc_ctx, &rqd_ctx
    };

/* I/O dispatch routines, I/O addresses 17772150 - 17772152

   base + 0     IP      read/write
   base + 2     SA      read/write
*/

t_stat sym_rd (int32 *data, int32 PA, int32 access)
{
return SCPE_NOFNC;  // not implemented - accessed through pci routines
}

t_stat sym_wr (int32 data, int32 PA, int32 access)
{
return SCPE_NOFNC;  // not implemented - accessed through pci routines
}

#endif

// Called at power-on
t_stat sym_reset (DEVICE* dptr)
{
    // If there are multiple controllers, determine which controller is being addressed,
    // and if device is enabled. if device is disabled, then exit.

    // Initialize local variables
    
    // Initialize/attach PCI configuration registers

    // Register PCI device with PCI bus

    // Initialize operating registers

    // Initialize SCSI bus


}

// pci_reset() stops PCI activity - it does not reset the PCI configuration registers
PCI_STAT sym_pci_reset (PCI_DEV* _this)
{
}

PCI_STAT sym_pci_io_read (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32* value)
{
    char* const op_reg = (char*)&_this->cfg_reg[0].csr[32];
    uint32 wmask = _this->cfg_wmask[0].csr[4];
    uint32 io_enabled = _this->cfg_reg[0].csr[1] & SYM_CFG1_IOENA;
    uint32 io_range_match = ((pci_address & wmask) == (_this->cfg_reg[0].csr[4] & wmask));

    sim_debug (DBG_TRC|DBG_PCI, _this->dev, "sym_pci_io_read(): addr(%llx), size(%d), mask(%x), value*(%x)\n",
                pci_address, size, cbez, value);

    if (io_enabled && io_range_match) {
        // I/O is enabled and the I/O range matches
        uint8  offset = pci_address & 0x7F;
        uint32 accum = 0;
        
        switch (size) {
        case 8:
            accum |= op_reg[offset+7] << 24;
            accum |= op_reg[offset+6] << 16;
            accum |= op_reg[offset+5] << 8;
            accum |= op_reg[offset+4];
            accum &= CBEZ_MASK[cbez >> 4];      // mask out unwanted bytes
            *(value+1) = accum;
            accum = 0;
        case 4:
            accum |= op_reg[offset+3] << 24;
            accum |= op_reg[offset+2] << 16;
        case 2:
            accum |= op_reg[offset+1] << 8;
        case 1:
            accum |= op_reg[offset];
            accum &= CBEZ_MASK[cbez & 0x0F];    // mask out unwanted bytes
            *value = accum;
            break;

        default:
            sim_printf("sym_pci_io_read: unexpected read size (%d) @PCI(%llx)\n", size, pci_address);
            return PCI_ARG_ERR;
            break;
        }
        return PCI_OK;
    } else {
        // I/O is disabled or the I/O range does not match
        return PCI_NOT_ME;
    }
}

PCI_STAT sym_pci_io_write (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32 value)
{
}

PCI_STAT sym_pci_mem_read (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32* value)
{
}

PCI_STAT sym_pci_mem_write (PCI_DEV* _this, t_uint64 pci_address, int size, uint8 cbez, uint32 value)
{
}

PCI_STAT sym_pci_cfg_read (PCI_DEV* _this, int slot, int function, int register_, uint8 cbez, uint32* value)
{
    // Checking Function beyond single provided? (Seen during system PCI configuration phase)
    if (function > 0) {
        *value = PCI_CONFIG_NX_READ_VALUE;
        return PCI_OK;
    }

    // Dispatch configuration register read
    switch (register_) {

    }

}

PCI_STAT sym_pci_cfg_write (PCI_DEV* _this, int slot, int function, int register_, uint8 cbez, uint32  value)
{
    return PCI_NOFNC;
}

PCI_STAT sym_pci_mem_readm (PCI_DEV* _this, t_uint64 pci_src_address, uint32* lcl_dst_address, int count)
{
    return PCI_NOFNC;
}

PCI_STAT sym_pci_mem_readl (PCI_DEV* _this, t_uint64 pci_src_sddress, uint32* lcl_dst_address, int count)
{
    return PCI_NOFNC;
}

PCI_STAT sym_pci_mem_writei (PCI_DEV* _this, t_uint64 pci_dst_address, uint32* lcl_src_address, int count)
{
    return PCI_NOFNC;
}
