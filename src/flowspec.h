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

#ifndef FLOWSPEC_H
#define FLOWSPEC_H

#include <sys/types.h>

#include <bzlib.h>
#include <iostream>
#include <vector>
#include <set>
#include <string>

#include "tools.h"


class FlowSpec
{
   public:
   FlowSpec();
   ~FlowSpec();

   void statusChangeEvent(const unsigned long long now);
   unsigned long long scheduleNextStatusChangeEvent(const unsigned long long now);
   unsigned long long scheduleNextTransmissionEvent();

   bool initializeStatsFile(const bool compressed = true);
   bool finishStatsFile(const bool closeFile);
   
   void resetStatistics();
   void print(std::ostream& os, const bool printStatistics = false) const;


   static FlowSpec* findFlowSpec(std::vector<FlowSpec*>& flowSet,
                                 const uint64_t          measurementID,
                                 const uint32_t          flowID,
                                 const uint16_t          streamID);
   static FlowSpec* findFlowSpec(std::vector<FlowSpec*>& flowSet,
                                 int                     sd,
                                 uint16_t                streamID);
   static FlowSpec* findFlowSpec(std::vector<FlowSpec*>& flowSet,
                                 const sctp_assoc_t      assocID);
   static FlowSpec* findFlowSpec(std::vector<FlowSpec*>& flowSet,
                                 const struct sockaddr*  from);
   static void printFlows(std::ostream&           os,
                          std::vector<FlowSpec*>& flowSet,
                          const bool              printStatistics = false);


   // ====== Attributes =====================================================
   public:
   FlowSpec*               NextFlow;

   std::string             Description;
   uint64_t                MeasurementID;
   uint32_t                FlowID;
   uint16_t                StreamID;
   uint8_t                 Protocol;

   double                  ReliableMode;
   double                  OrderedMode;

   double                  OutboundFrameRate;
   double                  OutboundFrameSize;
   double                  InboundFrameRate;
   double                  InboundFrameSize;
   uint8_t                 OutboundFrameRateRng;
   uint8_t                 OutboundFrameSizeRng;
   uint8_t                 InboundFrameRateRng;
   uint8_t                 InboundFrameSizeRng;


   // ====== Socket Descriptor ==============================================
   int                     SocketDescriptor;
   bool                    OriginalSocketDescriptor;


   // ====== Sequence Numbers ===============================================
   uint64_t                LastOutboundSeqNumber;
   uint32_t                LastOutboundFrameID;

   // ====== Timing =========================================================
   unsigned long long      FirstTransmission;
   unsigned long long      FirstReception;
   unsigned long long      LastTransmission;
   unsigned long long      LastReception;


   // ====== Current statistics =============================================
   unsigned long long      TransmittedBytes;
   unsigned long long      TransmittedPackets;
   unsigned long long      TransmittedFrames;

   unsigned long long      ReceivedBytes;
   unsigned long long      ReceivedPackets;
   unsigned long long      ReceivedFrames;
   
   double                  Jitter;
   unsigned long long      LostBytes;
   unsigned long long      LostPackets;
   unsigned long long      LostFrames;


   // ====== Last-time statistics (for bandwidth calculations) ==============
   unsigned long long      LastTransmittedBytes;
   unsigned long long      LastTransmittedPackets;
   unsigned long long      LastTransmittedFrames;

   unsigned long long      LastReceivedBytes;
   unsigned long long      LastReceivedPackets;
   unsigned long long      LastReceivedFrames;

   unsigned long long      LastLostBytes;
   unsigned long long      LastLostPackets;
   unsigned long long      LastLostFrames;

   double                  LastTransitTime;

   // ====== Start/stop control =============================================
   enum FlowStatus {
      WaitingForStartup = 1,
      On                = 2,
      Off               = 3
   };
   FlowStatus              Status;
   unsigned long long      NextStatusChangeEvent;
   std::set<unsigned int>  OnOffEvents;
   unsigned long long      BaseTime;


   // ====== Variables for event loop =======================================
   int                     Index;
   unsigned long long      NextTransmissionEvent;

   sctp_assoc_t            RemoteControlAssocID;
   sctp_assoc_t            RemoteDataAssocID;
   sockaddr_union          RemoteAddress;
   bool                    RemoteAddressIsValid;
   
   // ====== Statistics file handling =======================================
   unsigned long long      StatsLine;
   char                    StatsFileName[256];
   FILE*                   StatsFile;
   BZFILE*                 StatsBZFile;
};

#endif
