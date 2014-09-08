/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2009-2014 by Thomas Dreibholz
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact: sebastian.wallat@uni-due.de
 *          dreibh@iem.uni-due.de
 */

#ifndef NETMETERPACKETS_H
#define NETMETERPACKETS_H

#include "tools.h"

#include <sys/types.h>


struct NetPerfMeterHeader
{
   uint8_t  Type;
   uint8_t  Flags;
   uint16_t Length;
} __attribute__((packed));

#define PPID_NETPERFMETER_CONTROL   36           /* Old value: 0x29097605 */
#define PPID_NETPERFMETER_DATA      37           /* Old value: 0x29097606 */
#define SC_NETPERFMETER_DATA        1852861808   /* Old value: 0x29097606 */


#define NETPERFMETER_ACKNOWLEDGE    0x01
#define NETPERFMETER_ADD_FLOW       0x02
#define NETPERFMETER_REMOVE_FLOW    0x03
#define NETPERFMETER_IDENTIFY_FLOW  0x04
#define NETPERFMETER_DATA           0x05
#define NETPERFMETER_START          0x06
#define NETPERFMETER_STOP           0x07
#define NETPERFMETER_RESULTS        0x08


struct NetPerfMeterAcknowledgeMessage
{
   NetPerfMeterHeader Header;

   uint32_t           FlowID;
   uint64_t           MeasurementID;
   uint16_t           StreamID;
   uint16_t           Padding;

   uint32_t           Status;
} __attribute__((packed));

#define NETPERFMETER_STATUS_OKAY  0
#define NETPERFMETER_STATUS_ERROR 1


#define NETPERFMETER_DESCRIPTION_SIZE     32
#define NETPERFMETER_RNG_INPUT_PARAMETERS  4

#define NETPERFMETER_PATHMGR_LENGTH       16
#define NETPERFMETER_CC_LENGTH            16

struct NetPerfMeterOnOffEvent
{
   uint8_t          RandNumGen;
   uint8_t          Flags;
   uint16_t         pad;
   network_double_t ValueArray[NETPERFMETER_RNG_INPUT_PARAMETERS];
};

#define NPOOEF_RELTIME (1 << 0)

struct NetPerfMeterAddFlowMessage
{
   NetPerfMeterHeader     Header;

   uint32_t               FlowID;
   uint64_t               MeasurementID;
   uint16_t               StreamID;
   uint8_t                Protocol;
   uint8_t                pad;

   char                   Description[NETPERFMETER_DESCRIPTION_SIZE];

   uint32_t               OrderedMode;
   uint32_t               ReliableMode;
   uint32_t               RetransmissionTrials;

   network_double_t       FrameRate[NETPERFMETER_RNG_INPUT_PARAMETERS];
   network_double_t       FrameSize[NETPERFMETER_RNG_INPUT_PARAMETERS];
   uint8_t                FrameRateRng;
   uint8_t                FrameSizeRng;

   uint32_t               RcvBufferSize;
   uint32_t               SndBufferSize;

   uint16_t               MaxMsgSize;
   uint8_t                CMT;
   uint8_t                CCID;

   uint16_t               NDiffPorts;
   char                   PathMgr[NETPERFMETER_PATHMGR_LENGTH];
   char                   CongestionControl[NETPERFMETER_CC_LENGTH];

   uint16_t               OnOffEvents;
   NetPerfMeterOnOffEvent OnOffEvent[];
} __attribute__((packed));

#define NPMAFF_DEBUG         (1 << 0)
#define NPMAFF_NODELAY       (1 << 1)
#define NPMAFF_REPEATONOFF   (1 << 2)

// RetransmissionTrials in milliseconds (highest bit of 32-bit value set)
#define NPMAF_RTX_TRIALS_IN_MILLISECONDS (1 << 31)

#define NPAF_PRIMARY_PATH 0x00
#define NPAF_CMT          0x01
#define NPAF_CMTRPv1      0x02
#define NPAF_CMTRPv2      0x03
#define NPAF_LikeMPTCP    0x04


struct NetPerfMeterRemoveFlowMessage
{
   NetPerfMeterHeader Header;

   uint32_t           FlowID;
   uint64_t           MeasurementID;
   uint16_t           StreamID;
} __attribute__((packed));


struct NetPerfMeterIdentifyMessage
{
   NetPerfMeterHeader Header;
   uint32_t           FlowID;
   uint64_t           MagicNumber;
   uint64_t           MeasurementID;
   uint16_t           StreamID;
} __attribute__((packed));

#define NETPERFMETER_IDENTIFY_FLOW_MAGIC_NUMBER 0x4bcdf3aa303c6774ULL

#define NPMIF_COMPRESS_VECTORS (1 << 0)
#define NPMIF_NO_VECTORS       (1 << 1)


struct NetPerfMeterDataMessage
{
   NetPerfMeterHeader Header;

   uint32_t           FlowID;
   uint64_t           MeasurementID;
   uint16_t           StreamID;
   uint16_t           Padding;

   uint32_t           FrameID;
   uint64_t           SeqNumber;
   uint64_t           ByteSeqNumber;
   uint64_t           TimeStamp;

   unsigned char      Payload[];
} __attribute__((packed));

#define NPMDF_FRAME_BEGIN (1 << 0)
#define NPMDF_FRAME_END   (1 << 1)


struct NetPerfMeterStartMessage
{
   NetPerfMeterHeader Header;

   uint32_t           Padding;
   uint64_t           MeasurementID;
} __attribute__((packed));

#define NPMSF_COMPRESS_VECTORS (1 << 0)
#define NPMSF_COMPRESS_SCALARS (1 << 1)
#define NPMSF_NO_VECTORS       (1 << 2)
#define NPMSF_NO_SCALARS       (1 << 3)


struct NetPerfMeterStopMessage
{
   NetPerfMeterHeader Header;

   uint32_t           Padding;
   uint64_t           MeasurementID;
} __attribute__((packed));


struct NetPerfMeterResults
{
   NetPerfMeterHeader Header;
   char               Data[];
} __attribute__((packed));

#define NPMRF_EOF                            (1 << 0)
#define NETPERFMETER_RESULTS_MAX_DATA_LENGTH 1400

#endif
