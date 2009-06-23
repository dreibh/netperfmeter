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

#include "transfer.h"
#include "control.h"
#include "tools.h"
#include "netperfmeterpackets.h"

#include <string.h>
#include <assert.h>
#include <math.h>
#include <iostream>


static void updateStatistics(StatisticsWriter*              statsWriter,
                             FlowSpec*                      flowSpec,
                             const unsigned long long       now,
                             const NetPerfMeterDataMessage* dataMsg,
                             const size_t                   received);


#define MAXIMUM_MESSAGE_SIZE (size_t)65536
#define MAXIMUM_PAYLOAD_SIZE (MAXIMUM_MESSAGE_SIZE - sizeof(NetPerfMeterDataMessage))



// ###### Transmit data frame ###############################################
ssize_t transmitFrame(StatisticsWriter*        statsWriter,
                      FlowSpec*                flowSpec,
                      const unsigned long long now,
                      const size_t             maxMsgSize)
{
   static char              outputBuffer[MAXIMUM_MESSAGE_SIZE];
   static bool              outputBufferInitialized = false;
   NetPerfMeterDataMessage* dataMsg                 = (NetPerfMeterDataMessage*)&outputBuffer;

   // ====== Create printable data pattern ==================================
   if(outputBufferInitialized == false) {
      unsigned char c = 30;
      for(size_t i = 0;i < MAXIMUM_PAYLOAD_SIZE;i++) {
         dataMsg->Payload[i] = (char)c;
         c++;
         if(c > 127) {
            c = 30;
         }
      }
      outputBufferInitialized = true;
   }


   // ====== Obtain length of data to send ==================================
   size_t bytesToSend = (size_t)rint(getRandomValue(flowSpec->OutboundFrameSize,
                                                    flowSpec->OutboundFrameSizeRng));
   if(bytesToSend == 0) {
      // On POLLOUT, we generate a maximum-sized message. If there is still space
      // in the buffer, POLLOUT will be set again ...
      bytesToSend = std::min(maxMsgSize, MAXIMUM_MESSAGE_SIZE);
   }
   ssize_t bytesSent = 0;

   while(bytesSent < bytesToSend) {
      // ====== Prepare NETPERFMETER_DATA message ===============================
      size_t chunkSize = std::min(bytesToSend, std::min(maxMsgSize, MAXIMUM_MESSAGE_SIZE));
      if(chunkSize < sizeof(NetPerfMeterDataMessage)) {
         chunkSize = sizeof(NetPerfMeterDataMessage);
      }
      dataMsg->Header.Type   = NETPERFMETER_DATA;
      dataMsg->Header.Flags  = 0x00;
      if(bytesSent == 0) {
         dataMsg->Header.Flags |= DHF_FRAME_BEGIN;
      }
      if(bytesSent + chunkSize >= bytesToSend) {
         dataMsg->Header.Flags |= DHF_FRAME_END;
      }
      dataMsg->Header.Length = htons(chunkSize);
      dataMsg->MeasurementID = hton64(flowSpec->MeasurementID);
      dataMsg->FlowID        = htonl(flowSpec->FlowID);
      dataMsg->StreamID      = htons(flowSpec->StreamID);
      dataMsg->Padding       = 0x0000;
      dataMsg->FrameID       = htonl(flowSpec->LastOutboundFrameID++);
      dataMsg->SeqNumber     = hton64(flowSpec->LastOutboundSeqNumber++);
      dataMsg->ByteSeqNumber = hton64(flowSpec->TransmittedBytes);
      dataMsg->TimeStamp     = hton64(now);

      // ====== Send NETPERFMETER_DATA message ==================================
      ssize_t sent;
      if(flowSpec->Protocol == IPPROTO_SCTP) {
         sctp_sndrcvinfo sinfo;
         memset(&sinfo, 0, sizeof(sinfo));
         sinfo.sinfo_assoc_id = (flowSpec->RemoteAddressIsValid) ? flowSpec->RemoteDataAssocID : 0;
         sinfo.sinfo_stream   = flowSpec->StreamID;
         sinfo.sinfo_ppid     = htonl(PPID_NETPERFMETER_DATA);
         if(flowSpec->ReliableMode < 1.0) {
            const bool sendUnreliable = (randomDouble() < flowSpec->ReliableMode);
            if(sendUnreliable) {
               sinfo.sinfo_timetolive = 1.0;
            }
         }
         if(flowSpec->OrderedMode < 1.0) {
            const bool sendUnordered = (randomDouble() < flowSpec->OrderedMode);
            if(sendUnordered) {
               sinfo.sinfo_flags |= SCTP_UNORDERED;
            }
         }
         sent = sctp_send(flowSpec->SocketDescriptor,
                          (char*)&outputBuffer, chunkSize,
                          &sinfo, MSG_NOSIGNAL);
      }
      else if(flowSpec->Protocol == IPPROTO_UDP) {
         sent = ext_sendto(flowSpec->SocketDescriptor,
                           (char*)&outputBuffer, chunkSize, MSG_NOSIGNAL,
                           &flowSpec->RemoteAddress.sa,
                           getSocklen(&flowSpec->RemoteAddress.sa));
      }
      else {
         sent = ext_send(flowSpec->SocketDescriptor, (char*)&outputBuffer, chunkSize, MSG_NOSIGNAL);
      }

      // ====== Update statistics ===========================================
      if(sent > 0) {
         bytesSent += sent;
         flowSpec->LastTransmission = now;
         if(flowSpec->FirstTransmission == 0) {
            flowSpec->FirstTransmission = now;
         }
         flowSpec->TransmittedPackets++;
         flowSpec->TransmittedBytes += (unsigned long long)sent;
         statsWriter->TotalTransmittedBytes += sent;
         statsWriter->TotalTransmittedPackets++;
      }
      else {
         printf("Overload for Flow ID #%u - %s!\n", flowSpec->FlowID,strerror(errno));
         break;
      }
   }
   flowSpec->TransmittedFrames++;
   statsWriter->TotalTransmittedFrames++;
   return(bytesSent);
}


// ###### Handle data message ###############################################
ssize_t handleDataMessage(const bool               activeMode,
                          MessageReader*           messageReader,
                          StatisticsWriter*        statsWriter,
                          std::vector<FlowSpec*>&  flowSet,
                          const unsigned long long now,
                          const int                sd,
                          const int                protocol,
                          const int                controlSocket)
{
   char            inputBuffer[65536];
   sockaddr_union  from;
   socklen_t       fromlen = sizeof(from);
   int             flags   = 0;
   sctp_sndrcvinfo sinfo;

   const ssize_t received =
      messageReader->receiveMessage(sd, &inputBuffer, sizeof(inputBuffer),
                                    &from.sa, &fromlen, &sinfo, &flags);
   if( (received > 0) && (!(flags & MSG_NOTIFICATION)) ) {
      const NetPerfMeterDataMessage*     dataMsg     =
         (const NetPerfMeterDataMessage*)&inputBuffer;
      const NetPerfMeterIdentifyMessage* identifyMsg =
         (const NetPerfMeterIdentifyMessage*)&inputBuffer;

      // ====== Handle NETPERFMETER_IDENTIFY_FLOW message ===================
      if( (received >= sizeof(NetPerfMeterIdentifyMessage)) &&
          (identifyMsg->Header.Type == NETPERFMETER_IDENTIFY_FLOW) &&
          (ntoh64(identifyMsg->MagicNumber) == NETPERFMETER_IDENTIFY_FLOW_MAGIC_NUMBER) ) {
         handleIdentifyMessage(flowSet, identifyMsg, sd, sinfo.sinfo_assoc_id,
                               &from, controlSocket);
      }

      // ====== Handle NETPERFMETER_DATA message ============================
      else if( (received >= sizeof(NetPerfMeterDataMessage)) &&
               (dataMsg->Header.Type == NETPERFMETER_DATA) ) {
         // ====== Identifiy flow ===========================================
         FlowSpec* flowSpec;
         if(activeMode) {
            flowSpec = FlowSpec::findFlowSpec(flowSet, sd,
                                              (protocol == IPPROTO_SCTP) ? sinfo.sinfo_stream : 0);
         }
         else {
            if(protocol == IPPROTO_SCTP) {
               flowSpec = FlowSpec::findFlowSpec(flowSet, sinfo.sinfo_assoc_id);
            }
            else if(protocol == IPPROTO_UDP) {
               flowSpec = FlowSpec::findFlowSpec(flowSet, &from.sa);
            }
            else {
               flowSpec = FlowSpec::findFlowSpec(flowSet, sd, 0);
            }
         }
         if(flowSpec) {
            //Update flow statistics by received NETPERFMETER_DATA message.
            updateStatistics(statsWriter, flowSpec, now, dataMsg, received);
         }
         else {
            std::cout << "WARNING: Received data for unknown flow!" << std::endl;
         }
      }
      else {
         std::cout << "WARNING: Received garbage!" << std::endl;
      }
   }

   return(received);
}


// ###### Update flow statistics with incoming NETPERFMETER_DATA message ####
static void updateStatistics(StatisticsWriter*              statsWriter,
                             FlowSpec*                      flowSpec,
                             const unsigned long long       now,
                             const NetPerfMeterDataMessage* dataMsg,
                             const size_t                   received)
{
   // ====== Update reception time ==========================================
   flowSpec->LastReception = now;
   if(flowSpec->FirstReception == 0) {
      flowSpec->FirstReception = now;
   }


   // ====== Update bandwidth statistics ====================================
   flowSpec->ReceivedPackets++;
   flowSpec->ReceivedBytes += (unsigned long long)received;

   statsWriter->TotalReceivedPackets++;
   statsWriter->TotalReceivedBytes += (unsigned long long)received;


   // ====== Update QoS statistics ==========================================
   const uint64_t seqNumber   = ntoh64(dataMsg->SeqNumber);
   const uint64_t timeStamp   = ntoh64(dataMsg->TimeStamp);
   const double   transitTime = ((double)now - (double)timeStamp) / 1000.0;

   // ------ Jitter calculation according to RFC 3550 -----------------------
   /* From RFC 3550:
      int transit = arrival - r->ts;
      int d = transit - s->transit;
      s->transit = transit;
      if (d < 0) d = -d;
      s->jitter += (1./16.) * ((double)d - s->jitter);
   */
   const double diff = transitTime - flowSpec->LastTransitTime;
   flowSpec->LastTransitTime = transitTime;
   flowSpec->Jitter += (1.0/16.0) * (fabs(diff) - flowSpec->Jitter);

   // ------ Loss calculation -----------------------------------------------
    
   // printf("%llu: d=%1.3f ms   j=%1.3f ms\n",seqNumber,transitTime,flowSpec->Jitter);

   flowSpec->ReceivedFrames++;   // ??? To be implemented ???
   statsWriter->TotalReceivedFrames++;   // ??? To be implemented ???
   

   // ------ Write statistics -----------------------------------------------
   const StatisticsWriter* statisticsWriter =
      StatisticsWriter::getStatisticsWriter(flowSpec->MeasurementID);
   assert(statisticsWriter != NULL);
   char str[512];
   snprintf((char*)&str, sizeof(str),
            "%06llu %llu %1.6f\t"
            "%llu %1.3f %1.3f\n",
            flowSpec->StatsLine++, now, (double)(now - statisticsWriter->FirstStatisticsEvent) / 1000000.0,
            (unsigned long long)seqNumber, transitTime, diff, flowSpec->Jitter);
   
   StatisticsWriter::writeString((const char*)&flowSpec->StatsFileName,
                                 flowSpec->StatsFile, flowSpec->StatsBZFile,
                                 (const char*)&str);
}
