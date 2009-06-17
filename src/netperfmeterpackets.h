/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2009 by Thomas Dreibholz
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
 * Contact: dreibh@iem.uni-due.de
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

#define PPID_NETPERFMETER_CONTROL   0x29097605
#define PPID_NETPERFMETER_DATA      0x29097606


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

   uint32_t           Status;
} __attribute__((packed));

#define NETPERFMETER_STATUS_OKAY  0
#define NETPERFMETER_STATUS_ERROR 1


struct NetPerfMeterAddFlowMessage
{
   NetPerfMeterHeader   Header;

   uint32_t             FlowID;
   uint64_t             MeasurementID;
   uint16_t             StreamID;
   uint8_t              Protocol;
   uint8_t              Flags;

   char                 Description[32];

   uint32_t             ReliableMode;
   uint32_t             OrderedMode;

   network_double_t     FrameRate;
   network_double_t     FrameSize;
   uint8_t              FrameRateRng;
   uint8_t              FrameSizeRng;

   uint16_t             OnOffEvents;
   unsigned int         OnOffEvent[];
} __attribute__((packed));

#define NPAF_COMPRESS_STATS (1 << 0)


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

   char               Payload[];
} __attribute__((packed));

#define DHF_FRAME_BEGIN (1 << 0)
#define DHF_FRAME_END (1 << 1)


struct NetPerfMeterStartMessage
{
   NetPerfMeterHeader Header;

   uint64_t           MeasurementID;
} __attribute__((packed));


struct NetPerfMeterStopMessage
{
   NetPerfMeterHeader Header;

   uint64_t           MeasurementID;
} __attribute__((packed));


struct NetPerfMeterResults
{
   NetPerfMeterHeader Header;
   char               Data[];
} __attribute__((packed));

#define RHF_EOF                              (1 << 0)
#define NETPERFMETER_RESULTS_MAX_DATA_LENGTH 1024

#endif
