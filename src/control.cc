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

#include "control.h"
#include "tools.h"

#include <string.h>
#include <math.h>
#include <poll.h>
#include <assert.h>
#include <iostream>


#define IDENTIFY_MAX_TRIALS 10
#define IDENTIFY_TIMEOUT    2500


// ###### Upload statistics file ############################################
static bool uploadResults(const int          controlSocket,
                          const sctp_assoc_t assocID,
                          FlowSpec*          flowSpec)
{
   char                 messageBuffer[sizeof(NetPerfMeterResults) + NETPERFMETER_RESULTS_MAX_DATA_LENGTH];
   NetPerfMeterResults* resultsMsg = (NetPerfMeterResults*)&messageBuffer;
   
   bool success = flowSpec->finishStatsFile(false);
   if(success) {
      success = sendAcknowledgeToRemoteNode(controlSocket, assocID,
                                            flowSpec->MeasurementID, flowSpec->FlowID, flowSpec->StreamID,
                                            (success == true) ? NETPERFMETER_STATUS_OKAY : NETPERFMETER_STATUS_ERROR);
      if(success) {
         resultsMsg->Header.Type = NETPERFMETER_RESULTS;
               
         sctp_sndrcvinfo sinfo;
         memset(&sinfo, 0, sizeof(sinfo));
         sinfo.sinfo_assoc_id = assocID;
         sinfo.sinfo_ppid     = PPID_NETPERFMETER_CONTROL;

         puts("UPLOAD...");
         do {
            const size_t bytes = fread(&resultsMsg->Data, 1, NETPERFMETER_RESULTS_MAX_DATA_LENGTH, flowSpec->StatsFile);
            resultsMsg->Header.Flags  = feof(flowSpec->StatsFile) ? RHF_EOF : 0x00;
            resultsMsg->Header.Length = htons(bytes);
            printf("   -> b=%d\n",(int)bytes);

            if(sctp_send(controlSocket, &resultsMsg, sizeof(resultsMsg), &sinfo, 0) < 0) {
               std::cerr << "ERROR: Failed to upload results - " << strerror(errno) << "!" << std::endl;
               success = false;
               break;
            }

         } while(!(resultsMsg->Header.Flags & RHF_EOF));
      }
   }
   flowSpec->finishStatsFile(true);
   return(success);
}


// ###### Download statistics file ##########################################
static bool downloadResults(const int          controlSocket,
                            const sctp_assoc_t assocID,
                            FlowSpec*          flowSpec)
{
   
}


// ###### Handle incoming control message ###################################
bool handleControlMessage(MessageReader*           messageReader,
                          std::vector<FlowSpec*>& flowSet,
                          int                      controlSocket)
{
   char            inputBuffer[65536];
   sockaddr_union  from;
   socklen_t       fromlen = sizeof(from);
   int             flags   = 0;
   sctp_sndrcvinfo sinfo;

   const ssize_t received = messageReader->receiveMessage(controlSocket, &inputBuffer, sizeof(inputBuffer),
                                                          &from.sa, &fromlen, &sinfo, &flags);
   if(received == MRRM_PARTIAL_READ) {
      return(true);   // Partial read -> wait for next fragment.
   }
   else if(received < (ssize_t)sizeof(NetPerfMeterHeader)) {
      std::cerr << "ERROR: Control connection is broken!" << std::endl;
      return(false);
   }

   // ====== Received a SCTP notification ===================================
   if(flags & MSG_NOTIFICATION) {
      const sctp_notification* notification = (const sctp_notification*)&inputBuffer;
      if( (notification->sn_header.sn_type == SCTP_ASSOC_CHANGE) &&
          ((notification->sn_assoc_change.sac_state == SCTP_COMM_LOST) ||
           (notification->sn_assoc_change.sac_state == SCTP_SHUTDOWN_COMP)) ) {
         remoteAllFlowsOwnedBy(flowSet, notification->sn_assoc_change.sac_assoc_id);
      }
   }

   // ====== Received a real control message ================================
   else {
      const NetPerfMeterHeader* header = (const NetPerfMeterHeader*)&inputBuffer;
      if(ntohs(header->Length) != received) {
         std::cerr << "ERROR: Received malformed control message!" << std::endl
            << "       expected=" << ntohs(header->Length) << ", received=" << received << std::endl;
         return(false);
      }

      if(header->Type == NETPERFMETER_ADD_FLOW) {
         const NetPerfMeterAddFlowMessage* addFlowMsg = (const NetPerfMeterAddFlowMessage*)&inputBuffer;
         if(received < sizeof(NetPerfMeterAddFlowMessage)) {
            std::cerr << "ERROR: Received malformed NETPERFMETER_ADD_FLOW control message!" << std::endl;
            return(false);
         }
         const uint64_t measurementID  = ntoh64(addFlowMsg->MeasurementID);
         const uint32_t flowID         = ntohl(addFlowMsg->FlowID);
         const uint16_t streamID       = ntohs(addFlowMsg->StreamID);
         const size_t  startStopEvents = ntohs(addFlowMsg->OnOffEvents);
         if(received < sizeof(NetPerfMeterAddFlowMessage) + (startStopEvents * sizeof(unsigned int))) {
            std::cerr << "ERROR: Received malformed NETPERFMETER_ADD_FLOW control message (too few start/stop entries)!" << std::endl;
            return(false);
         }
         char description[sizeof(addFlowMsg->Description) + 1];
         memcpy((char*)&description, (const char*)&addFlowMsg->Description, sizeof(addFlowMsg->Description));
         description[sizeof(addFlowMsg->Description)] = 0x00;
         if(FlowSpec::findFlowSpec(flowSet, measurementID, flowID, streamID)) {
            std::cerr << "ERROR: NETPERFMETER_ADD_FLOW tried to add already-existing flow!" << std::endl;
            return(sendAcknowledgeToRemoteNode(controlSocket, sinfo.sinfo_assoc_id,
                                               measurementID, flowID, streamID,
                                               NETPERFMETER_STATUS_ERROR));
         }
         else {
            FlowSpec* newFlowSpec = createRemoteFlow(addFlowMsg, sinfo.sinfo_assoc_id, (const char*)&description);
            for(size_t i = 0;i < startStopEvents;i++) {
               newFlowSpec->OnOffEvents.insert(ntohl(addFlowMsg->OnOffEvent[i]));
            }
            flowSet.push_back(newFlowSpec);
            return(sendAcknowledgeToRemoteNode(controlSocket, sinfo.sinfo_assoc_id,
                                               measurementID, flowID, streamID, NETPERFMETER_STATUS_OKAY));
         }
      }

      else if(header->Type == NETPERFMETER_REMOVE_FLOW) {
         const NetPerfMeterRemoveFlowMessage* removeFlowMsg = (const NetPerfMeterRemoveFlowMessage*)&inputBuffer;
         if(received < sizeof(NetPerfMeterRemoveFlowMessage)) {
            std::cerr << "ERROR: Received malformed NETPERFMETER_REMOVE_FLOW control message!" << std::endl;
            return(false);
         }
         const uint64_t measurementID = ntoh64(removeFlowMsg->MeasurementID);
         const uint32_t flowID        = ntohl(removeFlowMsg->FlowID);
         const uint16_t streamID      = ntohs(removeFlowMsg->StreamID);
         FlowSpec* flowSpec = FlowSpec::findFlowSpec(flowSet, measurementID, flowID, streamID);
         if(flowSpec == NULL) {
            std::cerr << "ERROR: NETPERFMETER_ADD_REMOVE tried to remove not-existing flow!" << std::endl;
            return(sendAcknowledgeToRemoteNode(controlSocket, sinfo.sinfo_assoc_id,
                                               measurementID, flowID, streamID,
                                               NETPERFMETER_STATUS_ERROR));
         }
         else {
            // ------ Remove FlowSpec from set of flows ---------------------
            for(std::vector<FlowSpec*>::iterator iterator = flowSet.begin();iterator != flowSet.end();iterator++) {
               if(*iterator == flowSpec) {
                  flowSet.erase(iterator);
                  break;
               }
            }
            // ------ Upload statistics file --------------------------------
            uploadResults(controlSocket, sinfo.sinfo_assoc_id, flowSpec);
            delete flowSpec;
         }
      }

      else if(header->Type == NETPERFMETER_START) {
         const NetPerfMeterStartMessage* startMsg = (const NetPerfMeterStartMessage*)&inputBuffer;
         if(received < sizeof(NetPerfMeterStartMessage)) {
            std::cerr << "ERROR: Received malformed NETPERFMETER_START control message!" << std::endl;
            return(false);
         }
         const unsigned long long now = getMicroTime();
         const uint64_t measurementID = ntoh64(startMsg->MeasurementID);
         char measurementIDString[64];
         snprintf((char*)&measurementIDString, sizeof(measurementIDString), "%llx", (unsigned long long)measurementID);

         std::cout << std::endl << "Starting measurement " << measurementIDString << " ..." << std::endl;

         for(std::vector<FlowSpec*>::iterator iterator = flowSet.begin();iterator != flowSet.end();iterator++) {
            FlowSpec* flowSpec = *iterator;
            if(flowSpec->MeasurementID == measurementID) {
               flowSpec->BaseTime = now;
               flowSpec->Status   = (flowSpec->OnOffEvents.size() > 0) ? FlowSpec::Off : FlowSpec::On;
               flowSpec->print(std::cout);
            }
         }

         return(sendAcknowledgeToRemoteNode(controlSocket, sinfo.sinfo_assoc_id,
                                            measurementID, 0, 0,
                                            NETPERFMETER_STATUS_OKAY));
      }

      std::cerr << "ERROR: Received invalid control message of type " << (unsigned int)header->Type << "!" << std::endl;
      return(false);
   }
}


// ###### Tell remote node to add new flow ##################################
bool addFlowToRemoteNode(int controlSocket, const FlowSpec* flowSpec)
{
   // ====== Sent NETPERFMETER_ADD_FLOW to remote node =======================
   char                    addFlowMsgBuffer[1000 + sizeof(NetPerfMeterAddFlowMessage) + (sizeof(unsigned int) * flowSpec->OnOffEvents.size())];
   NetPerfMeterAddFlowMessage* addFlowMsg = (NetPerfMeterAddFlowMessage*)&addFlowMsgBuffer;
   addFlowMsg->Header.Type    = NETPERFMETER_ADD_FLOW;
   addFlowMsg->Header.Flags   = NPAF_COMPRESS_STATS;
   addFlowMsg->Header.Length  = htons(sizeof(NetPerfMeterAddFlowMessage) + (sizeof(unsigned int) * flowSpec->OnOffEvents.size()));
   addFlowMsg->MeasurementID  = hton64(flowSpec->MeasurementID);
   addFlowMsg->FlowID         = htonl(flowSpec->FlowID);
   addFlowMsg->StreamID       = htons(flowSpec->StreamID);
   addFlowMsg->Protocol       = flowSpec->Protocol;
   addFlowMsg->Flags          = 0x00;
   addFlowMsg->FrameRate      = doubleToNetwork(flowSpec->InboundFrameRate);
   addFlowMsg->FrameSize      = doubleToNetwork(flowSpec->InboundFrameSize);
   addFlowMsg->FrameRateRng   = flowSpec->InboundFrameRateRng;
   addFlowMsg->FrameSizeRng   = flowSpec->InboundFrameSizeRng;
   addFlowMsg->ReliableMode   = htonl((uint32_t)rint(flowSpec->ReliableMode * (double)0xffffffff));
   addFlowMsg->OrderedMode    = htonl((uint32_t)rint(flowSpec->OrderedMode * (double)0xffffffff));
   addFlowMsg->OnOffEvents = htons(flowSpec->OnOffEvents.size());
   size_t i = 0;
   for(std::set<unsigned int>::iterator iterator = flowSpec->OnOffEvents.begin();iterator != flowSpec->OnOffEvents.end();iterator++, i++) {
      addFlowMsg->OnOffEvent[i] = htonl(*iterator);
   }
   memset((char*)&addFlowMsg->Description, 0, sizeof(addFlowMsg->Description));
   strncpy((char*)&addFlowMsg->Description, flowSpec->Description.c_str(), std::min(sizeof(addFlowMsg->Description), flowSpec->Description.size()));
   std::cout << "<R1> "; std::cout.flush();
   if(sctp_sendmsg(controlSocket, addFlowMsg, sizeof(NetPerfMeterAddFlowMessage) + (sizeof(unsigned int) * flowSpec->OnOffEvents.size()), NULL, 0, PPID_NETPERFMETER_CONTROL, 0, 0, ~0, 0) <= 0) {
      return(false);
   }
   std::cout << "<R2> "; std::cout.flush();
   if(waitForAcknowledgeFromRemoteNode(controlSocket, flowSpec->MeasurementID, flowSpec->FlowID, flowSpec->StreamID) == false) {
      return(false);
   }

   // ====== Sent NETPERFMETER_IDENTIFY_FLOW to remote node =================
   unsigned int maxTrials = 1;
   if( (flowSpec->Protocol != IPPROTO_SCTP) || (flowSpec->Protocol != IPPROTO_TCP) ) {
      maxTrials = IDENTIFY_MAX_TRIALS;
   }
   for(unsigned int trial = 0;trial < maxTrials;trial++) {
      NetPerfMeterIdentifyMessage identifyMsg;
      identifyMsg.Header.Type   = NETPERFMETER_IDENTIFY_FLOW;
      identifyMsg.Header.Flags  = 0x00;
      identifyMsg.Header.Length = htons(sizeof(identifyMsg));
      identifyMsg.MagicNumber   = hton64(NETPERFMETER_IDENTIFY_FLOW_MAGIC_NUMBER);
      identifyMsg.MeasurementID = hton64(flowSpec->MeasurementID);
      identifyMsg.FlowID        = htonl(flowSpec->FlowID);
      identifyMsg.StreamID      = htons(flowSpec->StreamID);

      std::cout << "<R3> "; std::cout.flush();
      if(flowSpec->Protocol == IPPROTO_SCTP) {
         if(sctp_sendmsg(flowSpec->SocketDescriptor, &identifyMsg, sizeof(identifyMsg), NULL, 0, PPID_NETPERFMETER_CONTROL, 0, flowSpec->StreamID, ~0, 0) <= 0) {
            return(false);
         }
      }
      else {
         if(ext_send(flowSpec->SocketDescriptor, &identifyMsg, sizeof(identifyMsg), 0) <= 0) {
            return(false);
         }
      }
      std::cout << "<R4> "; std::cout.flush();
      if(waitForAcknowledgeFromRemoteNode(controlSocket, flowSpec->MeasurementID, flowSpec->FlowID, flowSpec->StreamID, IDENTIFY_TIMEOUT) == true) {
         return(true);
      }
      std::cout << "<timeout> "; std::cout.flush();
   }
   return(false);
}


// ###### Create FlowSpec for remote flow ###################################
FlowSpec* createRemoteFlow(const NetPerfMeterAddFlowMessage* addFlowMsg,
                           const sctp_assoc_t            controlAssocID,
                           const char*                   description)
{
   FlowSpec* flowSpec = new FlowSpec;
   assert(flowSpec != NULL);

   flowSpec->MeasurementID            = ntoh64(addFlowMsg->MeasurementID);
   flowSpec->FlowID                   = ntohl(addFlowMsg->FlowID);
   flowSpec->StreamID                 = ntohs(addFlowMsg->StreamID);
   flowSpec->Protocol                 = addFlowMsg->Protocol;
   flowSpec->Description              = std::string(description);
   flowSpec->OutboundFrameRate        = networkToDouble(addFlowMsg->FrameRate);
   flowSpec->OutboundFrameSize        = networkToDouble(addFlowMsg->FrameSize);
   flowSpec->OutboundFrameRateRng     = addFlowMsg->FrameRateRng;
   flowSpec->OutboundFrameSizeRng     = addFlowMsg->FrameSizeRng;
   flowSpec->OrderedMode              = ntohl(addFlowMsg->OrderedMode)  / (double)0xffffffff;
   flowSpec->ReliableMode             = ntohl(addFlowMsg->ReliableMode) / (double)0xffffffff;
   flowSpec->RemoteControlAssocID     = controlAssocID;
   flowSpec->scheduleNextTransmissionEvent();
   return(flowSpec);
}


// ###### Tell remote node to remove a flow #################################
bool removeFlowFromRemoteNode(int controlSocket, const FlowSpec* flowSpec)
{
   NetPerfMeterRemoveFlowMessage removeFlowMsg;
   removeFlowMsg.Header.Type   = NETPERFMETER_REMOVE_FLOW;
   removeFlowMsg.Header.Flags  = 0x00;
   removeFlowMsg.Header.Length = htons(sizeof(removeFlowMsg));
   removeFlowMsg.MeasurementID = hton64(flowSpec->MeasurementID);
   removeFlowMsg.FlowID        = htonl(flowSpec->FlowID);
   removeFlowMsg.StreamID      = htons(flowSpec->StreamID);
   if(sctp_sendmsg(controlSocket, &removeFlowMsg, sizeof(removeFlowMsg), NULL, 0, PPID_NETPERFMETER_CONTROL, 0, 0, ~0, 0) <= 0) {
      return(false);
   }
   return(waitForAcknowledgeFromRemoteNode(controlSocket, flowSpec->MeasurementID, flowSpec->FlowID, flowSpec->StreamID));
}


// ###### Delete all flows owned by a given remote node #####################
void remoteAllFlowsOwnedBy(std::vector<FlowSpec*>& flowSet, const sctp_assoc_t assocID)
{
   std::vector<FlowSpec*>::iterator iterator = flowSet.begin();
   while(iterator != flowSet.end()) {
      FlowSpec* flowSpec = *iterator;
      if(flowSpec->RemoteControlAssocID == assocID) {
         std::vector<FlowSpec*>::iterator toBeRemoved = iterator;
         flowSet.erase(toBeRemoved);
         delete flowSpec;
         iterator = flowSet.begin();   // Is there a better solution?
      }
      else {
         iterator++;
      }
   }
}


// ###### Send NETPERFMETER_ACKNOWLEDGE to remote node ##########################
bool sendAcknowledgeToRemoteNode(int            controlSocket,
                                 sctp_assoc_t   assocID,
                                 const uint64_t measurementID,
                                 const uint32_t flowID,
                                 const uint16_t streamID,
                                 const uint32_t status)
{
   NetPerfMeterAcknowledgeMessage ackMsg;
   ackMsg.Header.Type   = NETPERFMETER_ACKNOWLEDGE;
   ackMsg.Header.Flags  = 0x00;
   ackMsg.Header.Length = htons(sizeof(ackMsg));
   ackMsg.MeasurementID = hton64(measurementID);
   ackMsg.FlowID        = htonl(flowID);
   ackMsg.StreamID      = htons(streamID);
   ackMsg.Status        = htonl(status);

   sctp_sndrcvinfo sinfo;
   memset(&sinfo, 0, sizeof(sinfo));
   sinfo.sinfo_assoc_id = assocID;
   sinfo.sinfo_ppid     = PPID_NETPERFMETER_CONTROL;

   return(sctp_send(controlSocket, &ackMsg, sizeof(ackMsg), &sinfo, 0) > 0);
}


// ###### Wait for NETPERFMETER_ACKNOWLEDGE from remote node ################
bool waitForAcknowledgeFromRemoteNode(int            controlSocket,
                                      const uint64_t measurementID,
                                      const uint32_t flowID,
                                      const uint16_t streamID,
                                      const int      timeout)
{
   // ====== Wait until there is something to read or a timeout =============
   struct pollfd pfd;
   pfd.fd      = controlSocket;
   pfd.events  = POLLIN;
   pfd.revents = 0;
   if(ext_poll(&pfd, 1, timeout) < 1) {
      return(false);
   }
   if(!(pfd.revents & POLLIN)) {
      return(false);
   }

   // ====== Read NETPERFMETER_ACKNOWLEDGE message ==========================
   NetPerfMeterAcknowledgeMessage ackMsg;
   if(ext_recv(controlSocket, &ackMsg, sizeof(ackMsg), 0) < sizeof(ackMsg)) {
      return(false);
   }
   if(ackMsg.Header.Type != NETPERFMETER_ACKNOWLEDGE) {
      std::cerr << "ERROR: Received message type " << (unsigned int)ackMsg.Header.Type << " instead of NETPERFMETER_ACKNOWLEDGE!" << std::endl;
      return(false);
   }

   // ====== Check whether NETPERFMETER_ACKNOWLEDGE is okay =================
   if( (ntoh64(ackMsg.MeasurementID) != measurementID) ||
       (ntohl(ackMsg.FlowID) != flowID) ||
       (ntohs(ackMsg.StreamID) != streamID) ) {
      std::cerr << "ERROR: Received NETPERFMETER_ACKNOWLEDGE for wrong measurement/flow/stream!" << std::endl;
      return(false);
   }

   const uint32_t status = ntohl(ackMsg.Status);
   std::cout << "<status=" << status << "> ";
   std::cout.flush();
   return(status == NETPERFMETER_STATUS_OKAY);
}


// ###### Handle NETPERFMETER_IDENTIFY_FLOW message #########################
void handleIdentifyMessage(std::vector<FlowSpec*>&        flowSet,
                           const NetPerfMeterIdentifyMessage* identifyMsg,
                           const int                      sd,
                           const sctp_assoc_t             assocID,
                           const sockaddr_union*          from,
                           const int                      controlSocket)
{
   FlowSpec* flowSpec = FlowSpec::findFlowSpec(flowSet,
                                               ntoh64(identifyMsg->MeasurementID),
                                               ntohl(identifyMsg->FlowID),
                                               ntohs(identifyMsg->StreamID));
   if((flowSpec != NULL) && (flowSpec->RemoteAddressIsValid == false)) {
      flowSpec->SocketDescriptor     = sd;
      flowSpec->RemoteDataAssocID    = assocID;
      flowSpec->RemoteAddress        = *from;
      flowSpec->RemoteAddressIsValid = true;
      const bool success = flowSpec->initializeStatsFile(true);
      sendAcknowledgeToRemoteNode(controlSocket, flowSpec->RemoteControlAssocID,
                                  ntoh64(identifyMsg->MeasurementID),
                                  ntohl(identifyMsg->FlowID),
                                  ntohs(identifyMsg->StreamID),
                                  (success == true) ? NETPERFMETER_STATUS_OKAY : NETPERFMETER_STATUS_ERROR);
   }
   else {
      std::cerr << "WARNING: NETPERFMETER_IDENTIFY_FLOW message for unknown flow!" << std::endl;
   }
}


// ###### Start measurement #################################################
bool startMeasurement(int            controlSocket,
                      const uint64_t measurementID)
{
   NetPerfMeterStartMessage startMsg;
   startMsg.Header.Type   = NETPERFMETER_START;
   startMsg.Header.Flags  = 0x00;
   startMsg.Header.Length = htons(sizeof(startMsg));
   startMsg.MeasurementID = hton64(measurementID);

   sctp_sndrcvinfo sinfo;
   memset(&sinfo, 0, sizeof(sinfo));
   sinfo.sinfo_ppid = PPID_NETPERFMETER_CONTROL;

   std::cout << "Starting measurement ... <S1> "; std::cout.flush();
   if(sctp_send(controlSocket, &startMsg, sizeof(startMsg), &sinfo, 0) < 0) {
      return(false);
   }
   std::cout << "<S2> "; std::cout.flush();
   if(waitForAcknowledgeFromRemoteNode(controlSocket, measurementID, 0, 0) == false) {
      return(false);
   }
   std::cout << "okay" << std::endl << std::endl;
   return(true);
}


// ###### Stop measurement ##################################################
bool stopMeasurement(int            controlSocket,
                     const uint64_t measurementID)
{
   return(true);
}
