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

#include "flowspec.h"
#include "statisticswriter.h"

#include <assert.h>
#include <string.h>
#include <math.h>


// ###### Constructor #######################################################
FlowSpec::FlowSpec()
{
   NextFlow                 = NULL;
   MeasurementID            = ~0;
   FlowID                   = ~0;
   Protocol                 = ~0;
   OrderedMode              = 1.0;
   ReliableMode             = 1.0;
   OutboundFrameRate        = 0.0;
   OutboundFrameSize        = 0.0;
   OutboundFrameRateRng     = RANDOM_CONSTANT;
   OutboundFrameSizeRng     = RANDOM_CONSTANT;
   InboundFrameRate         = 0.0;
   InboundFrameSize         = 0.0;
   InboundFrameRateRng      = RANDOM_CONSTANT;
   InboundFrameSizeRng      = RANDOM_CONSTANT;
   LastOutboundSeqNumber    = 0;
   LastOutboundFrameID      = 0;
   Index                    = -1;
   RemoteControlAssocID     = 0;
   RemoteDataAssocID        = 0;
   RemoteAddressIsValid     = false;
   SocketDescriptor         = -1;
   OriginalSocketDescriptor = false;
   Status                   = WaitingForStartup;
   NextStatusChangeEvent    = ~0;
   BaseTime                 = getMicroTime();
   StatsFileName[0]         = 0x00;
   StatsLine                = 0;
   StatsFile                = NULL;
   StatsBZFile              = NULL;

   resetStatistics();
}


// ###### Destructor ########################################################
FlowSpec::~FlowSpec()
{
}


// ###### Status change event ###############################################
void FlowSpec::statusChangeEvent(const unsigned long long now)
{
   if(NextStatusChangeEvent <= now) {
      std::set<unsigned int>::iterator first = OnOffEvents.begin();
      assert(first != OnOffEvents.end());

      if(Status == Off) {
         Status = On;
      }
      else if(Status == On) {
         Status = Off;
      }
      else {
         assert(false);
      }

      OnOffEvents.erase(first);
   }
   scheduleNextStatusChangeEvent(now);
}


// ###### Schedule next status change event #################################
unsigned long long FlowSpec::scheduleNextStatusChangeEvent(const unsigned long long now)
{
   std::set<unsigned int>::iterator first = OnOffEvents.begin();
   if((Status != WaitingForStartup) && (first != OnOffEvents.end())) {
      const unsigned int relNextEvent = *first;
      const unsigned long long absNextEvent = BaseTime + (1000ULL * relNextEvent);
      NextStatusChangeEvent = absNextEvent;
   }
   else {
      NextStatusChangeEvent = ~0;
   }
   return(NextStatusChangeEvent);
}


// ###### Schedule next transmission event (non-saturated sender) ###########
unsigned long long FlowSpec::scheduleNextTransmissionEvent()
{
   if(Status == On) {
      const double nextFrameRate = getRandomValue(OutboundFrameRate, OutboundFrameRateRng);
      NextTransmissionEvent = LastTransmission + (unsigned long long)rint(1000000.0 / nextFrameRate);
   }
   else {
      NextTransmissionEvent = ~0;
   }
   return(NextTransmissionEvent);
}


// ###### Reset flow statistics #############################################
void FlowSpec::resetStatistics()
{
   FirstTransmission      = 0;
   LastTransmission       = 0;
   FirstReception         = 0;
   LastReception          = 0;

   TransmittedBytes       = 0;
   TransmittedPackets     = 0;
   TransmittedFrames      = 0;
   ReceivedBytes          = 0;
   ReceivedPackets        = 0;
   ReceivedFrames         = 0;
   
   Jitter                 = 0.0;
   LostBytes              = 0;
   LostPackets            = 0;
   LostFrames             = 0;

   LastTransmittedBytes   = 0;
   LastTransmittedPackets = 0;
   LastTransmittedFrames  = 0;
   LastReceivedBytes      = 0;
   LastReceivedPackets    = 0;
   LastReceivedFrames     = 0;
   LastTransitTime        = 0.0;
   LastLostBytes          = 0;
   LastLostPackets        = 0;
   LastLostFrames         = 0;
}


// ###### Show configuration entry (value + random number generator) ########
static void showEntry(std::ostream& os, const double value, const uint8_t rng)
{
   char str[256];

   const char* rngName = "unknown?!";
   switch(rng) {
      case RANDOM_CONSTANT:
         rngName = "constant";
       break;
      case RANDOM_EXPONENTIAL:
         rngName = "negative exponential";
       break;
   }

   snprintf((char*)&str, sizeof(str), "%1.6lf (%s)", value, rngName);
   os << str;
}


// ###### Print FlowSpec ####################################################
void FlowSpec::print(std::ostream& os, const bool printStatistics) const
{
   if((OriginalSocketDescriptor) || (RemoteControlAssocID != 0)) {
      if(Protocol != IPPROTO_SCTP) {
         os << "+ " << getProtocolName(Protocol) << " Flow (Flow ID #" << FlowID << " \"" << Description << "\"):" << std::endl;
      }
      else {
         os << "+ " << getProtocolName(Protocol) << " Flow:" << std::endl;
      }
   }
   if(Protocol == IPPROTO_SCTP) {
      os << "   o Stream #" << StreamID << " \"" << Description << "\":" << std::endl;
   }

   os << "      - Outbound Frame Rate: ";
   showEntry(os, OutboundFrameRate, OutboundFrameRateRng);
   os << std::endl;
   os << "      - Outbound Frame Size: ";
   showEntry(os, OutboundFrameSize, OutboundFrameSizeRng);
   os << std::endl;
   os << "      - Inbound Frame Rate:  ";
   showEntry(os, InboundFrameRate, InboundFrameRateRng);
   os << std::endl;
   os << "      - Inbound Frame Size:  ";
   showEntry(os, InboundFrameSize, InboundFrameSizeRng);
   os << std::endl;
   if(Protocol == IPPROTO_SCTP) {
      char ordered[32];
      char reliable[32];
      snprintf((char*)&ordered, sizeof(ordered), "%1.2f%%", 100.0 * OrderedMode);
      snprintf((char*)&reliable, sizeof(reliable), "%1.2f%%", 100.0 * ReliableMode);
      os << "      - Ordered Mode:        " << ordered  << std::endl;
      os << "      - Reliable Mode:       " << reliable << std::endl;
   }
   os << "      - Start/Stop:          { ";
   if(OnOffEvents.size() > 0) {
      bool start = true;
      for(std::set<unsigned int>::iterator iterator = OnOffEvents.begin();iterator != OnOffEvents.end();iterator++) {
         if(start) {
            os << "*" << (*iterator / 1000.0) << " ";
         }
         else {
            os << "~" << (*iterator / 1000.0) << " ";
         }
         start = !start;
      }
   }
   else {
      os << "*0 ";
   }
   os << "}" << std::endl;

   if(printStatistics) {
      const double transmissionDuration  = (LastTransmission - FirstTransmission) / 1000000.0;
      const double transmittedBits       = 8 * TransmittedBytes;
      const double transmittedBitRate    = transmittedBits / transmissionDuration;
      const double transmittedBytes      = TransmittedBytes;
      const double transmittedByteRate   = transmittedBytes / transmissionDuration;
      const double transmittedPackets    = TransmittedPackets;
      const double transmittedPacketRate = transmittedPackets / transmissionDuration;
      const double transmittedFrames     = TransmittedFrames;
      const double transmittedFrameRate  = transmittedFrames / transmissionDuration;

      const double receptionDuration    = (LastReception - FirstReception) / 1000000.0;
      const double receivedBits         = 8 * ReceivedBytes;
      const double receivedBitRate      = receivedBits / receptionDuration;
      const double receivedBytes        = ReceivedBytes;
      const double receivedByteRate     = receivedBytes / receptionDuration;
      const double receivedPackets      = ReceivedPackets;
      const double receivedPacketRate   = receivedPackets / receptionDuration;
      const double receivedFrames       = ReceivedFrames;
      const double receivedFrameRate    = receivedFrames / receptionDuration;

      os << "      - Transmission:        " << std::endl
           << "         * Duration:         " << transmissionDuration << " s" << std::endl
           << "         * Bytes:            " << transmittedBytes << " B\t-> " << transmittedByteRate << " B/s" << std::endl
           << "         * Bits:             " << transmittedBits << " bit\t-> " << transmittedBitRate << " bit/s" << std::endl
           << "         * Packets:          " << transmittedPackets << " packets\t-> " << transmittedPacketRate << " packets/s" << std::endl
           << "         * Frames:           " << transmittedFrames << " frames\t-> " << transmittedFrameRate << " frames/s" << std::endl;
      os << "      - Reception:           " << std::endl
           << "         * Duration:         " << receptionDuration << "s" << std::endl
           << "         * Bytes:            " << receivedBytes << " B\t-> " << receivedByteRate << " B/s" << std::endl
           << "         * Bits:             " << receivedBits << " bit\t-> " << receivedBitRate << " bit/s" << std::endl
           << "         * Packets:          " << receivedPackets << " packets\t-> " << receivedPacketRate << " packets/s" << std::endl
           << "         * Frames:           " << receivedFrames << " frames\t-> " << receivedFrameRate << " frames/s" << std::endl;
   }
}


// ###### Find FlowSpec by Measurement ID/Flow ID/Stream ID #################
FlowSpec* FlowSpec::findFlowSpec(std::vector<FlowSpec*>& flowSet,
                                 const uint64_t          measurementID,
                                 const uint32_t          flowID,
                                 const uint16_t          streamID)
{
   for(std::vector<FlowSpec*>::iterator iterator = flowSet.begin();iterator != flowSet.end();iterator++) {
      FlowSpec* flowSpec = *iterator;
      if( (flowSpec->MeasurementID == measurementID) &&
          (flowSpec->FlowID == flowID) &&
          (flowSpec->StreamID == streamID) ) {
         return(flowSpec);
      }
   }
   return(NULL);
}


// ###### Find FlowSpec by socket descriptor and Stream ID ##################
FlowSpec* FlowSpec::findFlowSpec(std::vector<FlowSpec*>& flowSet,
                                 int                     sd,
                                 uint16_t                streamID)
{
   for(std::vector<FlowSpec*>::iterator iterator = flowSet.begin();iterator != flowSet.end();iterator++) {
      FlowSpec* flowSpec = *iterator;
      if(flowSpec->SocketDescriptor == sd) {
         if(flowSpec->StreamID == streamID) {
            return(flowSpec);
         }
      }
   }
   return(NULL);
}


// ###### Find FlowSpec by association ID ###################################
FlowSpec* FlowSpec::findFlowSpec(std::vector<FlowSpec*>& flowSet,
                                 const sctp_assoc_t      assocID)
{
   for(std::vector<FlowSpec*>::iterator iterator = flowSet.begin();iterator != flowSet.end();iterator++) {
      FlowSpec* flowSpec = *iterator;
      if( (flowSpec->RemoteAddressIsValid) && (flowSpec->RemoteDataAssocID == assocID) ) {
         return(flowSpec);
      }
   }
   return(NULL);
}


// ###### Find FlowSpec by source address ###################################
FlowSpec* FlowSpec::findFlowSpec(std::vector<FlowSpec*>& flowSet,
                                 const struct sockaddr*  from)
{
   for(std::vector<FlowSpec*>::iterator iterator = flowSet.begin();iterator != flowSet.end();iterator++) {
      FlowSpec* flowSpec = *iterator;
      if( (flowSpec->RemoteAddressIsValid) &&
          (addresscmp(from, &flowSpec->RemoteAddress.sa, true) == 0) ) {
         return(flowSpec);
      }
   }
   return(NULL);
}


// ###### Print all flows ###################################################
void FlowSpec::printFlows(std::ostream&           os,
                          std::vector<FlowSpec*>& flowSet,
                          const bool              printStatistics)
{
   for(std::vector<FlowSpec*>::iterator iterator = flowSet.begin();iterator != flowSet.end();iterator++) {
      const FlowSpec* flowSpec = *iterator;
      flowSpec->print(os, printStatistics);
   }
}


// ###### Initialize statistics file ########################################
bool FlowSpec::initializeStatsFile(const bool compressed)
{
   finishStatsFile(true);

   const char* prefix="out0";   // ????????????????????
   
   if(!RemoteAddressIsValid) {
      snprintf((char*)&StatsFileName, sizeof(StatsFileName), "%s-local-%08x-%04x", prefix, FlowID, StreamID);
   }
   else {
      snprintf((char*)&StatsFileName, sizeof(StatsFileName), "/tmp/temp-%016llx-%08x-%04x.data",
               (unsigned long long)MeasurementID, FlowID, StreamID);
   }
   StatsFile = fopen(StatsFileName, "w+b");
   if(StatsFile == NULL) {
      std::cerr << "ERROR: Unable to create output file " << StatsFileName << " - " << strerror(errno) << "!" << std::endl;
      return(false);
   }

printf("STRING: %s\n",StatsFileName);

   if(compressed) {
      int bzerror;
      StatsBZFile = BZ2_bzWriteOpen(&bzerror, StatsFile, 9, 0, 30);
      if(bzerror != BZ_OK) {
         std::cerr << "ERROR: Unable to initialize BZip2 compression for file " << StatsFileName << "!" << std::endl
              << "Reason: " << BZ2_bzerror(StatsBZFile, &bzerror) << std::endl;
         BZ2_bzWriteClose(&bzerror, StatsBZFile, 0, NULL, NULL);
         StatsBZFile = NULL;
         fclose(StatsFile);
         StatsFile = NULL;
         return(false);
      }
   }
   
   StatsLine = 1;
   return(StatisticsWriter::writeString((const char*)&StatsFileName, StatsFile, StatsBZFile,
                                        "AbsTime RelTime SeqNumber Delay PrevPacketDelayDiff Jitter\n"));
}


// ###### Finish writing of statistics file #################################
bool FlowSpec::finishStatsFile(const bool closeFile)
{
   bool result = true;
   if(StatsBZFile) {
      int bzerror;
      BZ2_bzWriteClose(&bzerror, StatsBZFile, 0, NULL, NULL);
      if(bzerror != BZ_OK) {
         std::cerr << "ERROR: Unable to finish BZip2 compression for file " << StatsFileName << "!" << std::endl
              << "Reason: " << BZ2_bzerror(StatsBZFile, &bzerror) << std::endl;
         result = false;
      }
      StatsBZFile = NULL; 
   }
   if(StatsFile) {
      if(closeFile) {
         fclose(StatsFile);
         StatsFile        = NULL;
         StatsFileName[0] = 0x00;
      }
      else {
         rewind(StatsFile);
      }
   }
   return(result);
}
