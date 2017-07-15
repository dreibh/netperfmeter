/*
 * Network Performance Meter
 * Copyright (C) 2009-2017 by Thomas Dreibholz
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


static void updateStatistics(Flow*                          flowSpec,
                             const unsigned long long       now,
                             const NetPerfMeterDataMessage* dataMsg,
                             const size_t                   received);

extern unsigned int gOutputVerbosity;

#define MAXIMUM_MESSAGE_SIZE (size_t)65536
#define MAXIMUM_PAYLOAD_SIZE (MAXIMUM_MESSAGE_SIZE - sizeof(NetPerfMeterDataMessage))


// ###### Generate payload pattern ##########################################
static void fillPayload(unsigned char* payload, const size_t length)
{
   unsigned char c = 30;
   for(size_t i = 0;i < length;i++) {
      *payload++ = c++;
      if(c > 127) {
         c = 30;
      }
   }
}


// ###### Send NETPERFMETER_DATA message ####################################
ssize_t sendNetPerfMeterData(Flow*                    flow,
                             const uint32_t           frameID,
                             const bool               isFrameBegin,
                             const bool               isFrameEnd,
                             const unsigned long long now,
                             size_t                   bytesToSend)
{
   char                     outputBuffer[MAXIMUM_MESSAGE_SIZE];
   NetPerfMeterDataMessage* dataMsg = (NetPerfMeterDataMessage*)&outputBuffer;

   if(bytesToSend < sizeof(NetPerfMeterDataMessage)) {
      bytesToSend = sizeof(NetPerfMeterDataMessage);
   }

   // ====== Prepare NETPERFMETER_DATA message ==============================
   // ------ Create header --------------------------------
   dataMsg->Header.Type   = NETPERFMETER_DATA;
   dataMsg->Header.Flags  = 0x00;
   if(isFrameBegin) {
      dataMsg->Header.Flags |= NPMDF_FRAME_BEGIN;
   }
   if(isFrameEnd) {
      dataMsg->Header.Flags |= NPMDF_FRAME_END;
   }
   dataMsg->Header.Length = htons(bytesToSend);
   dataMsg->MeasurementID = hton64(flow->getMeasurementID());
   dataMsg->FlowID        = htonl(flow->getFlowID());
   dataMsg->StreamID      = htons(flow->getStreamID());
   dataMsg->Padding       = 0x0000;
   dataMsg->FrameID       = htonl(frameID);
   dataMsg->SeqNumber     = hton64(flow->nextOutboundSeqNumber());
   dataMsg->ByteSeqNumber = hton64(flow->getCurrentBandwidthStats().TransmittedBytes);
   dataMsg->TimeStamp     = hton64(now);

   // ------ Create payload data pattern ------------------
   fillPayload((unsigned char*)&dataMsg->Payload,
               bytesToSend - sizeof(NetPerfMeterDataMessage));

   // ====== Send NETPERFMETER_DATA message =================================
   ssize_t sent;
   if(flow->getTrafficSpec().Protocol == IPPROTO_SCTP) {
      sctp_sndrcvinfo sinfo;
      memset(&sinfo, 0, sizeof(sinfo));
      sinfo.sinfo_stream   = flow->getStreamID();
      sinfo.sinfo_ppid     = htonl(PPID_NETPERFMETER_DATA);
      if(flow->getTrafficSpec().ReliableMode < 1.0) {
         const bool sendUnreliable = (randomDouble() > flow->getTrafficSpec().ReliableMode);
         if(sendUnreliable) {
            sinfo.sinfo_timetolive = flow->getTrafficSpec().RetransmissionTrials;
#if defined __FreeBSD__ || defined __APPLE__
            /* This is implementation specific and should be changed
             * to use either a SCTP_PRINFO cmsg or sctp_sendv(). However,
             * these are currently also only available in FreeBSD and Mac OS X
             */
            if(flow->getTrafficSpec().RetransmissionTrialsInMS) {
               sinfo.sinfo_flags |= SCTP_PR_SCTP_TTL;
            }
            else {
               sinfo.sinfo_flags |= SCTP_PR_SCTP_RTX;
            }
#else
#warning The SCTP API does not support multiple PR-SCTP policies! Using SCTP_PR_SCTP_TTL in unreliable mode.
#endif
         }
      }
      if(flow->getTrafficSpec().OrderedMode < 1.0) {
         const bool sendUnordered = (randomDouble() > flow->getTrafficSpec().OrderedMode);
         if(sendUnordered) {
            sinfo.sinfo_flags |= SCTP_UNORDERED;
         }
      }
#ifndef WITH_NEAT
      sent = sctp_send(flow->getSocketDescriptor(),
                       (char*)&outputBuffer, bytesToSend,
                       &sinfo, 0);
#else
      sent = nsa_send(flow->getSocketDescriptor(),
                      (char*)&outputBuffer, bytesToSend, 0);
#endif
   }
   else if(flow->getTrafficSpec().Protocol == IPPROTO_UDP) {
      if(flow->isRemoteAddressValid()) {
#ifndef WITH_NEAT
         sent = ext_sendto(flow->getSocketDescriptor(),
                           (char*)&outputBuffer, bytesToSend, 0,
                           flow->getRemoteAddress(),
                           getSocklen(flow->getRemoteAddress()));
#else
         sent = nsa_sendto(flow->getSocketDescriptor(),
                           (char*)&outputBuffer, bytesToSend, 0,
                           flow->getRemoteAddress(),
                           getSocklen(flow->getRemoteAddress()));
#endif
      }
      else {
#ifndef WITH_NEAT
         sent = ext_send(flow->getSocketDescriptor(),
                         (char*)&outputBuffer, bytesToSend, 0);
#else
         sent = nsa_send(flow->getSocketDescriptor(),
                         (char*)&outputBuffer, bytesToSend, 0);
#endif
      }
   }
   else {
#ifndef WITH_NEAT
      sent = ext_send(flow->getSocketDescriptor(), (char*)&outputBuffer, bytesToSend, 0);
#else
      sent = nsa_send(flow->getSocketDescriptor(), (char*)&outputBuffer, bytesToSend, 0);
#endif
   }

   // ====== Check, whether flow has been aborted unintentionally ===========
   if((sent < 0) &&
      (errno != EAGAIN) &&
      (!flow->isAcceptedIncomingFlow()) &&
      (flow->getTrafficSpec().ErrorOnAbort) &&
      (flow->getOutputStatus() == Flow::On)) {
      std::cerr << "ERROR: Flow #" << flow->getFlowID() << " has been aborted - "
                  << strerror(errno) << "!" << std::endl;
      exit(1);
   }

   return(sent);
}


// ###### Transmit data frame ###############################################
ssize_t transmitFrame(Flow*                    flow,
                      const unsigned long long now)
{
   // ====== Obtain length of data to send ==================================
   size_t bytesToSend =
      (size_t)rint(getRandomValue((const double*)&flow->getTrafficSpec().OutboundFrameSize,
                                  flow->getTrafficSpec().OutboundFrameSizeRng));
   if(bytesToSend == 0) {
      // On POLLOUT, we generate a maximum-sized message. If there is still space
      // in the buffer, POLLOUT will be set again ...
      bytesToSend = std::min((size_t)flow->getTrafficSpec().MaxMsgSize, MAXIMUM_MESSAGE_SIZE);
   }
   ssize_t bytesSent   = 0;
   size_t  packetsSent = 0;

   const uint32_t frameID = flow->nextOutboundFrameID();
   while(bytesSent < (ssize_t)bytesToSend) {
      // ====== Send message ================================================
      size_t chunkSize = std::min(bytesToSend, std::min((size_t)flow->getTrafficSpec().MaxMsgSize,
                                                        MAXIMUM_MESSAGE_SIZE));
      const ssize_t sent =
         sendNetPerfMeterData(flow, frameID,
                              (bytesSent == 0),                       // Is frame begin?
                              (bytesSent + chunkSize >= bytesToSend), // Is frame end?
                              now, chunkSize);

      // ====== Update statistics ===========================================
      if(sent > 0) {
         bytesSent += sent;
         packetsSent++;
      }
      else {
         // Transmission error -> stop sending.
         break;
      }
   }

   flow->updateTransmissionStatistics(now, 1, packetsSent, bytesSent);
   return(bytesSent);
}


// ###### Handle data message ###############################################
ssize_t handleNetPerfMeterData(const bool               isActiveMode,
                               const unsigned long long now,
                               const int                protocol,
                               const int                sd)
{
   char            inputBuffer[65536];
   sockaddr_union  from;
   socklen_t       fromlen = sizeof(from);
   int             flags   = 0;
   sctp_sndrcvinfo sinfo;

   sinfo.sinfo_stream = 0;
   const ssize_t received =
      FlowManager::getFlowManager()->getMessageReader()->receiveMessage(
         sd, &inputBuffer, sizeof(inputBuffer), &from.sa, &fromlen, &sinfo, &flags);

   if( (received > 0) && (!(flags & MSG_NOTIFICATION)) ) {
      const NetPerfMeterDataMessage*     dataMsg     =
         (const NetPerfMeterDataMessage*)&inputBuffer;
      const NetPerfMeterIdentifyMessage* identifyMsg =
         (const NetPerfMeterIdentifyMessage*)&inputBuffer;

      // ====== Handle NETPERFMETER_IDENTIFY_FLOW message ===================
      if( (received >= (ssize_t)sizeof(NetPerfMeterIdentifyMessage)) &&
          (identifyMsg->Header.Type == NETPERFMETER_IDENTIFY_FLOW) &&
          (ntoh64(identifyMsg->MagicNumber) == NETPERFMETER_IDENTIFY_FLOW_MAGIC_NUMBER) ) {
          handleNetPerfMeterIdentify(identifyMsg, sd, &from);
      }

      // ====== Handle NETPERFMETER_DATA message ============================
      else if( (received >= (ssize_t)sizeof(NetPerfMeterDataMessage)) &&
               (dataMsg->Header.Type == NETPERFMETER_DATA) ) {
         // ====== Identifiy flow ===========================================
         Flow* flow;
         if(( protocol == IPPROTO_UDP) && (!isActiveMode) ) {
            flow = FlowManager::getFlowManager()->findFlow(&from.sa);
         }
         else {
            flow = FlowManager::getFlowManager()->findFlow(sd, sinfo.sinfo_stream);
         }
         if(flow) {
            // Update flow statistics by received NETPERFMETER_DATA message.
            updateStatistics(flow, now, dataMsg, received);
         }
         else {
            std::cout << "WARNING: Received data for unknown flow!" << std::endl;
         }
      }
      else {
         std::cout << "WARNING: Received garbage!" << std::endl;
      }
   }

   else if( (received <= 0) && (received != MRRM_PARTIAL_READ) ) {
      Flow* flow = FlowManager::getFlowManager()->findFlow(sd, sinfo.sinfo_stream);
      if(flow) {
         if(gOutputVerbosity >= NPFOV_CONNECTIONS) {
            std::cout << "End of input for flow " <<  flow->getFlowID() << std::endl;
         }
         flow->endOfInput();
      }
   }

   return(received);
}


// ###### Update flow statistics with incoming NETPERFMETER_DATA message ####
static void updateStatistics(Flow*                          flow,
                             const unsigned long long       now,
                             const NetPerfMeterDataMessage* dataMsg,
                             const size_t                   receivedBytes)
{
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
   const double diff   = transitTime - flow->getDelay();
   const double jitter = flow->getJitter() + (1.0/16.0) * (fabs(diff) - flow->getJitter());

   // ------ Loss calculation -----------------------------------------------
   flow->getDefragmenter()->addFragment(now, dataMsg);
   size_t receivedFrames;
   size_t lostFrames;
   size_t lostPackets;
   size_t lostBytes;
   flow->getDefragmenter()->purge(now, flow->getTrafficSpec().DefragmentTimeout,
                                  receivedFrames, lostFrames, lostPackets, lostBytes);

   flow->updateReceptionStatistics(
      now, receivedFrames, receivedBytes,
      lostFrames, lostPackets, lostBytes,
      (unsigned long long)seqNumber, transitTime, diff, jitter);
}
