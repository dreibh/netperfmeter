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


struct NetMeterHeader
{
   uint8_t  Type;
   uint8_t  Flags;
   uint16_t Length;
} __attribute__((packed));

#define PPID_NETMETER_CONTROL   0x29097605
#define PPID_NETMETER_DATA      0x29097606


#define NETMETER_ACKNOWLEDGE    0x01
#define NETMETER_ADD_FLOW       0x02
#define NETMETER_REMOVE_FLOW    0x03
#define NETMETER_IDENTIFY_FLOW  0x04
#define NETMETER_DATA           0x05
#define NETMETER_START          0x06
#define NETMETER_STOP           0x07


struct NetMeterAcknowledgeMessage
{
   NetMeterHeader Header;

   uint32_t       FlowID;
   uint64_t       MeasurementID;
   uint16_t       StreamID;

   uint32_t       Status;
} __attribute__((packed));

#define NETMETER_STATUS_OKAY  0
#define NETMETER_STATUS_ERROR 1


struct NetMeterAddFlowMessage
{
   NetMeterHeader   Header;

   uint32_t         FlowID;
   uint64_t         MeasurementID;
   uint16_t         StreamID;
   uint8_t          Protocol;
   uint8_t          Flags;

   char             Description[32];

   uint32_t         ReliableMode;
   uint32_t         OrderedMode;

   network_double_t FrameRate;
   network_double_t FrameSize;
   uint8_t          FrameRateRng;
   uint8_t          FrameSizeRng;

   uint16_t         OnOffEvents;
   unsigned int     OnOffEvent[];
} __attribute__((packed));


struct NetMeterRemoveFlowMessage
{
   NetMeterHeader Header;

   uint32_t       FlowID;
   uint64_t       MeasurementID;
   uint16_t       StreamID;
} __attribute__((packed));


struct NetMeterIdentifyMessage
{
   NetMeterHeader Header;
   uint32_t       FlowID;
   uint64_t       MagicNumber;
   uint64_t       MeasurementID;
   uint16_t       StreamID;
} __attribute__((packed));

#define NETMETER_IDENTIFY_FLOW_MAGIC_NUMBER 0x4bcdf3aa303c6774ULL


struct NetMeterDataMessage
{
   NetMeterHeader Header;

   uint32_t       FlowID;
   uint64_t       MeasurementID;
   uint16_t       StreamID;
   uint16_t       Padding;

   uint64_t       SeqNumber;
   uint64_t       TimeStamp;

   char           Payload[];
} __attribute__((packed));


struct NetMeterStartMessage
{
   NetMeterHeader Header;

   uint64_t       MeasurementID;
} __attribute__((packed));


struct NetMeterStopMessage
{
   NetMeterHeader Header;

   uint64_t       MeasurementID;
} __attribute__((packed));

#endif
