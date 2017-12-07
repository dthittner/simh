/* sim_scsi.h: SCSI Bus and Protocol definitions

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
    This SCSI simulation is designed to emulate SCSI-2 and SCSI-3 protocols.

    It also is the place where SCSI Pass-Through (SPTI) [Windows] and SCSI Generic (sg) [Linux]
    connections to a local SCSI device are implemented.

    SCSI devices typically contain just one LUN, which is usually LUN 0 or infrequently LUN 1.
    Some tape devices contain 2 LUNs, one of which is the physical tape device, and the other 
    is a virtual robotics changer LUN, although some tapes drives implement the
    robotics changer as an entirely separate SCSI ID. Large scale device farms such as
    RAID controllers will display as many LUNs to the host as the user defines,
    up to the LUN limit of the supported SCSI protocol.
    There is even a Nakamichi MJ-5.16SI SCSI CD changer which identifies the physical drive as LUN 0,
    and LUNs 1-5 are the internal CD slots that hold CDs that will be mounted in LUN 0.

*/

#ifndef __sim_scsi_h
#define __sim_scsi_h

#include "sim_defs.h"

#define SCSI2_MAX_ID     16     // SCSI-2 = 16, priority order 7..0,15..8
#define SCSI2_MAX_LUNS   32     // SCSI-2 = 32

#define SCSI3_MAX_ID     32     // SCSI-3 (fiber channel) = 32, priority order 7..0,15..8,23..16,31..24
#define SCSI3_MAX_LUNS  256     // SCSI-3 (fiber channel) = 256

typedef int SCSI_STAT;

typedef SCSI_STAT (*SCSI_RESET)(void);        // resets scsi device

struct scsi_bus_t;                      // forward
typedef struct scsi_bus_t SCSI_BUS;     // forward

/* SCSI Vital Product Data (VPD) */
struct scsi_vpd_t {         // VPD information is not required, but it helps to identify the unit to the FW/OS
    int     len;            // length of VPD character data (since it can contain nulls)
    char*   data;           // VPD character data [0...n bytes]
};
    

// This describes the particular SCSI unit
struct scsi_lun_t {
    char*               name;
    struct scsi_vpd_t*  vpd;
    t_uint64            max_lbn;        // maximum logical block number [0..N] on unit
    struct geometry_t {                 // Arbitrary disk C/H/S geometry data; only used on disk units
                                        // . not really needed if using LBN adressing
                                        // .. but unix may need it to set disk partition boundries
        int             cylinders;      // Cylinders
        int             heads;          // Heads
        int             sectors;        // Sectors/Track
    } chs;

};
typedef struct scsi_lun_t SCSI_LUN;

struct scsi_device_t {
    char*               name;
    SCSI_RESET          reset;
    int                 scsi_id;
    int                 lun;
    SCSI_LUN*           lun_info;
    int                 scsi_device_class;  // disk, tape, cdrom, cdrw, etc.
    int                 scsi_device_id;     // select from list of known scsi device_ids;
    SCSI_BUS*           bus;
};
typedef struct scsi_device SCSI_DEV;

enum scsi_phase_t {
    bus_free,
    arbitration,
    selection,
    reselection,
    message_out,
    command,
    data_out,
    data_in,
    status,
    message_in
};
typedef enum scsi_phase_t SCSI_PHASE;

struct scsi_bus_t {
    char*               name;
    SCSI_PHASE          phase;
    SCSI_DEV*           initiator;
    SCSI_DEV*           target;
    SCSI_DEV*           attached[SCSI3_MAX_ID][SCSI3_MAX_LUNS];
};

#endif  // __sim_scsi_h