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
#include "statisticswriter.h"
#include "t3.h"

#include <string.h>
#include <math.h>
#include <poll.h>
#include <assert.h>
#include <iostream>


#define IDENTIFY_MAX_TRIALS 10
#define IDENTIFY_TIMEOUT    2500


static bool download(const int   controlSocket,
                     const char* fileName);


// ###### Tell remote node to add new flow ##################################
bool performNetPerfMeterAddFlow(int controlSocket, const Flow* flow)
{
   // ====== Sent NETPERFMETER_ADD_FLOW to remote node ======================
   const size_t                addFlowMsgSize = sizeof(NetPerfMeterAddFlowMessage) +
                                                (sizeof(unsigned int) * flow->getTrafficSpec().OnOffEvents.size());
   char                        addFlowMsgBuffer[addFlowMsgSize];
   NetPerfMeterAddFlowMessage* addFlowMsg = (NetPerfMeterAddFlowMessage*)&addFlowMsgBuffer;
   addFlowMsg->Header.Type   = NETPERFMETER_ADD_FLOW;
   addFlowMsg->Header.Flags  = 0x00;
   addFlowMsg->Header.Length = htons(addFlowMsgSize);
   addFlowMsg->MeasurementID = hton64(flow->getMeasurementID());
   addFlowMsg->FlowID        = htonl(flow->getFlowID());
   addFlowMsg->StreamID      = htons(flow->getStreamID());
   addFlowMsg->Protocol      = flow->getTrafficSpec().Protocol;
   addFlowMsg->Flags         = 0x00;
   addFlowMsg->FrameRate     = doubleToNetwork(flow->getTrafficSpec().InboundFrameRate);
   addFlowMsg->FrameSize     = doubleToNetwork(flow->getTrafficSpec().InboundFrameSize);
   addFlowMsg->FrameRateRng  = flow->getTrafficSpec().InboundFrameRateRng;
   addFlowMsg->FrameSizeRng  = flow->getTrafficSpec().InboundFrameSizeRng;
   addFlowMsg->ReliableMode  = htonl((uint32_t)rint(flow->getTrafficSpec().ReliableMode * (double)0xffffffff));
   addFlowMsg->OrderedMode   = htonl((uint32_t)rint(flow->getTrafficSpec().OrderedMode * (double)0xffffffff));
   addFlowMsg->OnOffEvents   = htons(flow->getTrafficSpec().OnOffEvents.size());
   size_t i = 0;
   for(std::set<unsigned int>::iterator iterator = flow->getTrafficSpec().OnOffEvents.begin();
       iterator != flow->getTrafficSpec().OnOffEvents.end();iterator++, i++) {
      addFlowMsg->OnOffEvent[i] = htonl(*iterator);
   }
   memset((char*)&addFlowMsg->Description, 0, sizeof(addFlowMsg->Description));
   strncpy((char*)&addFlowMsg->Description, flow->getTrafficSpec().Description.c_str(),
           std::min(sizeof(addFlowMsg->Description), flow->getTrafficSpec().Description.size()));
           
   sctp_sndrcvinfo sinfo;
   memset(&sinfo, 0, sizeof(sinfo));
   sinfo.sinfo_ppid = PPID_NETPERFMETER_CONTROL;
           
   std::cout << "<R1> "; std::cout.flush();
   if(sctp_send(controlSocket, addFlowMsg, addFlowMsgSize, &sinfo, 0) <= 0) {
      return(false);
   }
   
   // ====== Wait for NETPERFMETER_ACKNOWLEDGE ==============================
   std::cout << "<R2> "; std::cout.flush();
   if(awaitNetPerfMeterAcknowledge(controlSocket, flow->getMeasurementID(),
                                   flow->getFlowID(), flow->getStreamID()) == false) {
      return(false);
   }

   // ======  Let remote identify the new flow ==============================
   return(performNetPerfMeterIdentifyFlow(controlSocket, flow));
}


// ###### Let remote identify a new flow ####################################
bool performNetPerfMeterIdentifyFlow(int controlSocket, const Flow* flow)
{
   // ====== Sent NETPERFMETER_IDENTIFY_FLOW to remote node =================
   unsigned int maxTrials = 1;
   if( (flow->getTrafficSpec().Protocol != IPPROTO_SCTP) ||
       (flow->getTrafficSpec().Protocol != IPPROTO_TCP) ) {
      // SCTP and TCP are reliable transport protocols => no retransmissions
      maxTrials = IDENTIFY_MAX_TRIALS;
   }
   const StatisticsWriter* statisticsWriter =
      StatisticsWriter::getStatisticsWriter(flow->getMeasurementID());
   assert(statisticsWriter != NULL);
   for(unsigned int trial = 0;trial < maxTrials;trial++) {
      NetPerfMeterIdentifyMessage identifyMsg;
      identifyMsg.Header.Type   = NETPERFMETER_IDENTIFY_FLOW;
      identifyMsg.Header.Flags  = hasSuffix(statisticsWriter->VectorSuffix, ".bz2") ? NPIF_COMPRESS_STATS : 0x00;
      identifyMsg.Header.Length = htons(sizeof(identifyMsg));
      identifyMsg.MagicNumber   = hton64(NETPERFMETER_IDENTIFY_FLOW_MAGIC_NUMBER);
      identifyMsg.MeasurementID = hton64(flow->getMeasurementID());
      identifyMsg.FlowID        = htonl(flow->getFlowID());
      identifyMsg.StreamID      = htons(flow->getStreamID());
      
      std::cout << "<R3> "; std::cout.flush();
      if(flow->getTrafficSpec().Protocol == IPPROTO_SCTP) {
         sctp_sndrcvinfo sinfo;
         memset(&sinfo, 0, sizeof(sinfo));
         sinfo.sinfo_stream = flow->getStreamID();
         sinfo.sinfo_ppid   = PPID_NETPERFMETER_CONTROL;
         if(sctp_send(flow->getSocketDescriptor(), &identifyMsg, sizeof(identifyMsg), &sinfo, 0) <= 0) {
            return(false);
         }
      }
      else {
         if(ext_send(flow->getSocketDescriptor(), &identifyMsg, sizeof(identifyMsg), 0) <= 0) {
            return(false);
         }
      }
      std::cout << "<R4> "; std::cout.flush();
      if(awaitNetPerfMeterAcknowledge(controlSocket, flow->getMeasurementID(),
                                          flow->getFlowID(), flow->getStreamID(),
                                          IDENTIFY_TIMEOUT) == true) {
         return(true);
      }
      std::cout << "<timeout> "; std::cout.flush();
   }
   return(false);
}


// ###### Start measurement #################################################
bool performNetPerfMeterStart(int            controlSocket,
                              const uint64_t measurementID)
{
   StatisticsWriter* statisticsWriter = StatisticsWriter::getStatisticsWriter();
   if(statisticsWriter->initializeOutputFiles() == false) {
      return(false);
   }

   // ====== Start flows ====================================================
   FlowManager::getFlowManager()->startMeasurement(measurementID);
      
   // ====== Tell passive node to start measurement =========================
   NetPerfMeterStartMessage startMsg;
   startMsg.Header.Type   = NETPERFMETER_START;
   startMsg.Header.Flags  = hasSuffix(statisticsWriter->VectorSuffix, ".bz2") ? NPSF_COMPRESS_STATS : 0x00;
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
   if(awaitNetPerfMeterAcknowledge(controlSocket, measurementID, 0, 0) == false) {
      return(false);
   }
   std::cout << "okay" << std::endl << std::endl;
   return(true);
}


// ###### Stop measurement ##################################################
bool performNetPerfMeterStop(int            controlSocket,
                             const uint64_t measurementID)
{
   // ====== Start flows ====================================================
   FlowManager::getFlowManager()->stopMeasurement(measurementID);

   StatisticsWriter* statisticsWriter = StatisticsWriter::getStatisticsWriter();
   if(statisticsWriter->finishOutputFiles() == false) {
      return(false);
   }

   // ====== Tell passive node to stop measurement ==========================
   NetPerfMeterStopMessage stopMsg;
   stopMsg.Header.Type   = NETPERFMETER_STOP;
   stopMsg.Header.Flags  = 0x00;
   stopMsg.Header.Length = htons(sizeof(stopMsg));
   stopMsg.MeasurementID = hton64(measurementID);

   sctp_sndrcvinfo sinfo;
   memset(&sinfo, 0, sizeof(sinfo));
   sinfo.sinfo_ppid = PPID_NETPERFMETER_CONTROL;

   std::cout << "Stopping measurement ... <S1> "; std::cout.flush();
   if(sctp_send(controlSocket, &stopMsg, sizeof(stopMsg), &sinfo, 0) < 0) {
      return(false);
   }
   std::cout << "<S2> "; std::cout.flush();
   if(awaitNetPerfMeterAcknowledge(controlSocket, measurementID, 0, 0) == false) {
      return(false);
   }

   // ====== Download passive node's vector file ============================
   const std::string vectorName = StatisticsWriter::getPassivNodeFilename(
                                     statisticsWriter->VectorPrefix,
                                     statisticsWriter->VectorSuffix);
   std::cout << "Downloading results [" << vectorName << "] ";
   std::cout.flush();
   if(download(controlSocket, vectorName.c_str()) == false) {
      return(false);
   }
   std::cout << " ";

   // ====== Download passive node's scalar file ============================
   const std::string scalarName = StatisticsWriter::getPassivNodeFilename(
                                     statisticsWriter->ScalarPrefix,
                                     statisticsWriter->ScalarSuffix);
   std::cout << "Downloading results [" << scalarName << "] ";
   std::cout.flush();
   if(download(controlSocket, scalarName.c_str()) == false) {
      return(false);
   }
   std::cout << " okay" << std::endl << std::endl;
   
   return(true);
}


// ###### Send NETPERFMETER_ACKNOWLEDGE to remote node ######################
bool sendNetPerfMeterAcknowledge(int            controlSocket,
                                 sctp_assoc_t   assocID,   // ????? notwendig ???
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
bool awaitNetPerfMeterAcknowledge(int            controlSocket,
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
      std::cerr << "ERROR: Received message type " << (unsigned int)ackMsg.Header.Type
                << " instead of NETPERFMETER_ACKNOWLEDGE!" << std::endl;
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






// ###### Download file #####################################################
static bool download(const int       controlSocket,
                     const char*     fileName)
{
   char                 messageBuffer[sizeof(NetPerfMeterResults) + NETPERFMETER_RESULTS_MAX_DATA_LENGTH];
   NetPerfMeterResults* resultsMsg = (NetPerfMeterResults*)&messageBuffer;

   FILE* fh = fopen(fileName, "w");
   if(fh == NULL) {
      std::cerr << "ERROR: Unable to create file " << fileName
               << " - " << strerror(errno) << "!" << std::endl;
      return(false);
   }
   bool success = false;
   ssize_t received = ext_recv(controlSocket, resultsMsg, sizeof(messageBuffer), 0);
   while(received >= sizeof(NetPerfMeterResults)) {
      const size_t bytes = ntohs(resultsMsg->Header.Length);
      if(resultsMsg->Header.Type != NETPERFMETER_RESULTS) {
         std::cerr << "ERROR: Received unexpected message type "
                   << (unsigned int)resultsMsg->Header.Type <<  "!" << std::endl;
         exit(1);
      }
      if(bytes + sizeof(NetPerfMeterResults) > received) {
         std::cerr << "ERROR: Received malformed NETPERFMETER_RESULTS message!" << std::endl;
         // printf("%u + %u > %u\n", bytes, sizeof(NetPerfMeterResults), received);
         exit(1);
      }
      std::cout << ".";

      if(fwrite((char*)&resultsMsg->Data, bytes, 1, fh) != 1) {
         std::cerr << "ERROR: Unable to write results to file " << fileName
                  << " - " << strerror(errno) << "!" << std::endl;
         exit(1);
      }
   
      if(resultsMsg->Header.Flags & RHF_EOF) {
         success = true;
         break;
      }
      received = ext_recv(controlSocket, resultsMsg, sizeof(messageBuffer), 0);
   }
   fclose(fh);
   return(success);
}


// ###### Download statistics file ##########################################
static bool downloadResults(const int       controlSocket,
                            const FlowSpec* flowSpec)
{
   const StatisticsWriter* statisticsWriter =
      StatisticsWriter::getStatisticsWriter(flowSpec->MeasurementID);
   assert(statisticsWriter != NULL);

   char extension[32];
   snprintf((char*)&extension, sizeof(extension), "-%08x-%04x",
            flowSpec->FlowID, flowSpec->StreamID);
   const std::string vectorName = StatisticsWriter::getPassivNodeFilename(
                                     statisticsWriter->VectorPrefix,
                                     statisticsWriter->VectorSuffix,
                                     extension);

   std::cout << "Downloading results [" << vectorName << "] ";
   std::cout.flush();
   bool success = awaitNetPerfMeterAcknowledge(controlSocket, flowSpec->MeasurementID,
                                               flowSpec->FlowID, flowSpec->StreamID);
   if(success) {
      success = download(controlSocket, vectorName.c_str());
   }
   std::cout << " " << ((success == true) ? "okay": "FAILED") << std::endl;
   return(success);
}





// ?????? passive side CTRL!


// ###### Upload file #######################################################
static bool uploadOutputFile(const int          controlSocket,
                             const sctp_assoc_t assocID,
                             const OutputFile&  outputFile)
{
   char                 messageBuffer[sizeof(NetPerfMeterResults) +
                                      NETPERFMETER_RESULTS_MAX_DATA_LENGTH];
   NetPerfMeterResults* resultsMsg = (NetPerfMeterResults*)&messageBuffer;

   // ====== Initialize header ==============================================
   sctp_sndrcvinfo sinfo;
   memset(&sinfo, 0, sizeof(sinfo));
   sinfo.sinfo_assoc_id    = assocID;
   sinfo.sinfo_ppid        = PPID_NETPERFMETER_CONTROL;
   resultsMsg->Header.Type = NETPERFMETER_RESULTS;

   std::cout << std::endl << "Uploading results [" << outputFile.getName() << "] ";
   std::cout.flush();

   // ====== Transmission loop ==============================================
   bool success = true;
   setBlocking(controlSocket);
   do {
      // ====== Read chunk from file ========================================
      const size_t bytes = fread(&resultsMsg->Data, 1,
                                 NETPERFMETER_RESULTS_MAX_DATA_LENGTH,
                                 outputFile.getFile());
      resultsMsg->Header.Flags  = feof(outputFile.getFile()) ? RHF_EOF : 0x00;
      resultsMsg->Header.Length = htons(bytes);
      // printf("   -> b=%d   snd=%d\n",(int)bytes, (int)(sizeof(NetPerfMeterResults) + bytes));
      if(ferror(outputFile.getFile())) {
         std::cerr << "ERROR: Failed to read results from "
                   << outputFile.getName() << " - "
                   << strerror(errno) << "!" << std::endl;
         success = false;
         break;
      }

      // ====== Transmit chunk ==============================================
      if(sctp_send(controlSocket, resultsMsg, sizeof(NetPerfMeterResults) + bytes, &sinfo, 0) < 0) {
         std::cerr << "ERROR: Failed to upload results - " << strerror(errno) << "!" << std::endl;
         success = false;
         break;
      }
      std::cout << ".";
      std::cout.flush();
   } while(!(resultsMsg->Header.Flags & RHF_EOF));

   // ====== Check results ==================================================
   if(!success) {
      sendAbort(controlSocket, assocID);
   }
   setNonBlocking(controlSocket);
   std::cout << " " << ((success == true) ? "okay": "FAILED") << std::endl;
   return(success);
}


// ###### Upload per-flow statistics file ###################################
static bool uploadResults(const int          controlSocket,
                          const sctp_assoc_t assocID,
                          Flow*              flow)
{
   bool success = flow->getVectorFile().finish(false);
   if(success) {
      success = sendNetPerfMeterAcknowledge(
                   controlSocket, assocID,
                   flow->getMeasurementID(), flow->getFlowID(), flow->getStreamID(),
                   (success == true) ? NETPERFMETER_STATUS_OKAY :
                                       NETPERFMETER_STATUS_ERROR);
      if(success) {
         success = uploadOutputFile(controlSocket, assocID, flow->getVectorFile());
      }
   }
   flow->getVectorFile().finish(true);
   return(success);
}


// ###### Handle NETPERFMETER_ADD_FLOW ######################################
static bool handleNetPerfMeterAddFlow(const NetPerfMeterAddFlowMessage* addFlowMsg,
                                      const int                         controlSocket,
                                      const sctp_assoc_t                assocID,
                                      const size_t                      received)
{
   if(received < sizeof(NetPerfMeterAddFlowMessage)) {
      std::cerr << "ERROR: Received malformed NETPERFMETER_ADD_FLOW control message!"
                << std::endl;
      return(false);
   }
   const uint64_t measurementID   = ntoh64(addFlowMsg->MeasurementID);
   const uint32_t flowID          = ntohl(addFlowMsg->FlowID);
   const uint16_t streamID        = ntohs(addFlowMsg->StreamID);
   const size_t   startStopEvents = ntohs(addFlowMsg->OnOffEvents);
   if(received < sizeof(NetPerfMeterAddFlowMessage) + (startStopEvents * sizeof(unsigned int))) {
      std::cerr << "ERROR: Received malformed NETPERFMETER_ADD_FLOW control message "
                   "(too few start/stop entries)!" << std::endl;
      return(false);
   }
   char description[sizeof(addFlowMsg->Description) + 1];
   memcpy((char*)&description, (const char*)&addFlowMsg->Description, sizeof(addFlowMsg->Description));
   description[sizeof(addFlowMsg->Description)] = 0x00;
   if(FlowManager::getFlowManager()->findFlow(measurementID, flowID, streamID)) {
      std::cerr << "ERROR: NETPERFMETER_ADD_FLOW tried to add already-existing flow!"
                << std::endl;
      return(sendNetPerfMeterAcknowledge(controlSocket, assocID,
                                          measurementID, flowID, streamID,
                                          NETPERFMETER_STATUS_ERROR));
   }
   else {
      // ====== Create new flow =============================================
      TrafficSpec trafficSpec;
      trafficSpec.Protocol             = addFlowMsg->Protocol;
      trafficSpec.Description          = std::string(description);
      trafficSpec.OutboundFrameRate    = networkToDouble(addFlowMsg->FrameRate);
      trafficSpec.OutboundFrameSize    = networkToDouble(addFlowMsg->FrameSize);
      trafficSpec.OutboundFrameRateRng = addFlowMsg->FrameRateRng;
      trafficSpec.OutboundFrameSizeRng = addFlowMsg->FrameSizeRng;
      trafficSpec.OrderedMode          = ntohl(addFlowMsg->OrderedMode)  / (double)0xffffffff;
      trafficSpec.ReliableMode         = ntohl(addFlowMsg->ReliableMode) / (double)0xffffffff;
      for(size_t i = 0;i < startStopEvents;i++) {
         trafficSpec.OnOffEvents.insert(ntohl(addFlowMsg->OnOffEvent[i]));
      }
      
      Flow* flow = new Flow(ntoh64(addFlowMsg->MeasurementID), ntohl(addFlowMsg->FlowID),
                            ntohs(addFlowMsg->StreamID), trafficSpec,
                            assocID);  // <- AssocID!!!!! ???????????
      return(sendNetPerfMeterAcknowledge(controlSocket, assocID,
                                         measurementID, flowID, streamID,
                                         (flow != NULL) ? NETPERFMETER_STATUS_OKAY : NETPERFMETER_STATUS_ERROR));
   }
}


// ###### Handle NETPERFMETER_REMOVE_FLOW #####################################
static bool handleNetPerfMeterRemoveFlow(const NetPerfMeterRemoveFlowMessage* removeFlowMsg,
                                         const int                            controlSocket,
                                         const sctp_assoc_t                   assocID,
                                         const size_t                         received)
{
   if(received < sizeof(NetPerfMeterRemoveFlowMessage)) {
      std::cerr << "ERROR: Received malformed NETPERFMETER_REMOVE_FLOW control message!"
                << std::endl;
      return(false);
   }
   const uint64_t measurementID = ntoh64(removeFlowMsg->MeasurementID);
   const uint32_t flowID        = ntohl(removeFlowMsg->FlowID);
   const uint16_t streamID      = ntohs(removeFlowMsg->StreamID);
   Flow* flow = FlowManager::getFlowManager()->findFlow(measurementID, flowID, streamID);
   if(flow == NULL) {
      std::cerr << "ERROR: NETPERFMETER_ADD_REMOVE tried to remove not-existing flow!"
                << std::endl;
      return(sendNetPerfMeterAcknowledge(controlSocket, assocID,
                                          measurementID, flowID, streamID,
                                          NETPERFMETER_STATUS_ERROR));
   }
   else {
      // ------ Remove Flow from Flow Manager -------------------------
      FlowManager::getFlowManager()->removeFlow(flow);
      // ------ Upload statistics file --------------------------------
      uploadResults(controlSocket, assocID, flow);
      delete flow;
   }
   return(true);
}


// ###### Handle NETPERFMETER_START #########################################
static bool handleNetPerfMeterStart(const NetPerfMeterStartMessage* startMsg,
                                    const int                       controlSocket,
                                    const sctp_assoc_t              assocID,
                                    const size_t                    received)
{
   if(received < sizeof(NetPerfMeterStartMessage)) {
      std::cerr << "ERROR: Received malformed NETPERFMETER_START control message!" << std::endl;
      return(false);
   }
   const uint64_t measurementID = ntoh64(startMsg->MeasurementID);

   std::cout << std::endl << "Starting measurement "
               << format("%llx", (unsigned long long)measurementID) << " ..." << std::endl;

   // ???????? "now" notwendig??????
   const unsigned long long now = getMicroTime();
   FlowManager::getFlowManager()->startMeasurement(measurementID, now, true);

   bool success = false;
   StatisticsWriter* statisticsWriter =
      StatisticsWriter::addMeasurement(now, measurementID,
                                       (startMsg->Header.Flags & NPSF_COMPRESS_STATS) ? true : false);
   if(statisticsWriter) {
      success = statisticsWriter->initializeOutputFiles();
   }
   
   return(sendNetPerfMeterAcknowledge(controlSocket, assocID,
                                       measurementID, 0, 0,
                                       (success == true) ? NETPERFMETER_STATUS_OKAY : NETPERFMETER_STATUS_ERROR));
}


// ###### Handle NETPERFMETER_STOP #########################################
static bool handleNetPerfMeterStop(const NetPerfMeterStopMessage* stopMsg,
                                   const int                      controlSocket,
                                   const sctp_assoc_t             assocID,
                                   const size_t                   received)
{
   if(received < sizeof(NetPerfMeterStopMessage)) {
      std::cerr << "ERROR: Received malformed NETPERFMETER_STOP control message!" << std::endl;
      return(false);
   }
   const unsigned long long now = getMicroTime();
   const uint64_t measurementID = ntoh64(stopMsg->MeasurementID);
   char measurementIDString[64];
   snprintf((char*)&measurementIDString, sizeof(measurementIDString),
            "%llx", (unsigned long long)measurementID);

   std::cout << std::endl << "Stopping measurement "
               << measurementIDString << " ..." << std::endl;

   bool outputReady = false;
   StatisticsWriter* statisticsWriter = StatisticsWriter::findMeasurement(measurementID);
   if(statisticsWriter) {
//       outputReady = statisticsWriter->writeAllScalarStatistics(now, flowSet, measurementID);
outputReady=true; // ???
      if(outputReady) {
         outputReady = statisticsWriter->finishOutputFiles(false);
      }
   }
   
   sendNetPerfMeterAcknowledge(controlSocket, assocID,
                               measurementID, 0, 0,
                               (outputReady == true) ? NETPERFMETER_STATUS_OKAY : NETPERFMETER_STATUS_ERROR);
   if(outputReady) {
/*      uploadOutput(controlSocket, assocID,
             statisticsWriter->VectorName.c_str(), statisticsWriter->VectorFile);
      upload(controlSocket, assocID,
             statisticsWriter->ScalarName.c_str(), statisticsWriter->ScalarFile);*/
   }
   if(statisticsWriter)  {
      statisticsWriter->finishOutputFiles(true);
      StatisticsWriter::removeMeasurement(measurementID);
   }
   return(true);
}   



// ###### Handle incoming control message ###################################
bool handleControlMessage(MessageReader* messageReader,
                          int            controlSocket)
{
   char            inputBuffer[65536];
   sockaddr_union  from;
   socklen_t       fromlen = sizeof(from);
   int             flags   = 0;
   sctp_sndrcvinfo sinfo;

   // ====== Read message (or fragment) =====================================
   const ssize_t received =
      messageReader->receiveMessage(controlSocket, &inputBuffer, sizeof(inputBuffer),
                                    &from.sa, &fromlen, &sinfo, &flags);
   if(received == MRRM_PARTIAL_READ) {
      return(true);   // Partial read -> wait for next fragment.
   }
   else if(received < (ssize_t)sizeof(NetPerfMeterHeader)) {
      std::cerr << "ERROR: Control connection is broken!" << std::endl;
      sendAbort(controlSocket, sinfo.sinfo_assoc_id);
      return(false);
   }

   // ====== Received a SCTP notification ===================================
   if(flags & MSG_NOTIFICATION) {
      const sctp_notification* notification = (const sctp_notification*)&inputBuffer;
      if( (notification->sn_header.sn_type == SCTP_ASSOC_CHANGE) &&
          ((notification->sn_assoc_change.sac_state == SCTP_COMM_LOST) ||
           (notification->sn_assoc_change.sac_state == SCTP_SHUTDOWN_COMP)) ) {
         
//         remoteAllFlowsOwnedBy(notification->sn_assoc_change.sac_assoc_id);
puts("---- REMOVE ALL ....");
      }
   }

   // ====== Received a real control message ================================
   else {
      const NetPerfMeterHeader* header = (const NetPerfMeterHeader*)&inputBuffer;
      if(ntohs(header->Length) != received) {
         std::cerr << "ERROR: Received malformed control message!" << std::endl
            << "       expected=" << ntohs(header->Length)
            << ", received="<< received << std::endl;
         return(false);
      }

      switch(header->Type) {
         case NETPERFMETER_ADD_FLOW:
            return(handleNetPerfMeterAddFlow(
                      (const NetPerfMeterAddFlowMessage*)&inputBuffer,
                      controlSocket, sinfo.sinfo_assoc_id, received));
          break;
         case NETPERFMETER_REMOVE_FLOW:
            return(handleNetPerfMeterRemoveFlow(
                      (const NetPerfMeterRemoveFlowMessage*)&inputBuffer,
                      controlSocket, sinfo.sinfo_assoc_id, received));
          break;
         case NETPERFMETER_START:
            return(handleNetPerfMeterStart(
                      (const NetPerfMeterStartMessage*)&inputBuffer,
                      controlSocket, sinfo.sinfo_assoc_id, received));
          break;
         case NETPERFMETER_STOP:
            return(handleNetPerfMeterStop(
                      (const NetPerfMeterStopMessage*)&inputBuffer,
                      controlSocket, sinfo.sinfo_assoc_id, received));
          break;
         default:
            std::cerr << "ERROR: Received invalid control message of type "
                     << (unsigned int)header->Type << "!" << std::endl;
            sendAbort(controlSocket, sinfo.sinfo_assoc_id);
            return(false);
          break;
      }
   }
}


// ###### Tell remote node to remove a flow #################################
bool removeFlowFromRemoteNode(int controlSocket, FlowSpec* flowSpec)
{
   flowSpec->finishVectorFile(true);
   
   NetPerfMeterRemoveFlowMessage removeFlowMsg;
   removeFlowMsg.Header.Type   = NETPERFMETER_REMOVE_FLOW;
   removeFlowMsg.Header.Flags  = 0x00;
   removeFlowMsg.Header.Length = htons(sizeof(removeFlowMsg));
   removeFlowMsg.MeasurementID = hton64(flowSpec->MeasurementID);
   removeFlowMsg.FlowID        = htonl(flowSpec->FlowID);
   removeFlowMsg.StreamID      = htons(flowSpec->StreamID);

   sctp_sndrcvinfo sinfo;
   memset(&sinfo, 0, sizeof(sinfo));
   sinfo.sinfo_ppid = PPID_NETPERFMETER_CONTROL;

   if(sctp_send(controlSocket, &removeFlowMsg, sizeof(removeFlowMsg), &sinfo, 0) <= 0) {
      return(false);
   }
   return(downloadResults(controlSocket, flowSpec));
}


// ###### Handle NETPERFMETER_IDENTIFY_FLOW message #########################
void handleIdentifyMessage(const NetPerfMeterIdentifyMessage* identifyMsg,
                           const int                          sd,
                           const sockaddr_union*              from,
                           const sctp_assoc_t                 assocID,
                           const int                          controlSocket)
{
   const bool success =
      FlowManager::getFlowManager()->identifyFlow(ntoh64(identifyMsg->MeasurementID),
                                                  ntohl(identifyMsg->FlowID),
                                                  ntohs(identifyMsg->StreamID));
   if(success == false) {
      std::cerr << "WARNING: Failed to identify flow on NETPERFMETER_IDENTIFY_FLOW!" << std::endl;
   }
   sendNetPerfMeterAcknowledge(controlSocket, flow->RemoteControlAssocID,
                                 ntoh64(identifyMsg->MeasurementID),
                                 ntohl(identifyMsg->FlowID),
                                 ntohs(identifyMsg->StreamID),
                                 (success == true) ? NETPERFMETER_STATUS_OKAY : NETPERFMETER_STATUS_ERROR);
   
   ?????????ßß
   Flow* flow = Flow::findFlow(flowSet,
                                               ntoh64(identifyMsg->MeasurementID),
                                               ntohl(identifyMsg->FlowID),
                                               ntohs(identifyMsg->StreamID));
   if((flow != NULL) && (flow->RemoteAddressIsValid == false)) {
      flow->setSocketDescriptor(
      flow->SocketDescriptor     = sd;
// ??????      flow->RemoteDataAssocID    = assocID;
      
      flow->RemoteAddress        = *from;
      flow->RemoteAddressIsValid = true;
      const bool success = flow->initializeVectorFile((identifyMsg->Header.Flags & NPIF_COMPRESS_STATS) ? true : false);
      sendNetPerfMeterAcknowledge(controlSocket, flow->RemoteControlAssocID,
                                  ntoh64(identifyMsg->MeasurementID),
                                  ntohl(identifyMsg->FlowID),
                                  ntohs(identifyMsg->StreamID),
                                  (success == true) ? NETPERFMETER_STATUS_OKAY : NETPERFMETER_STATUS_ERROR);
   }
   else {
      std::cerr << "WARNING: NETPERFMETER_IDENTIFY_FLOW message for unknown flow!" << std::endl;
   }
}


// ###### Delete all flows owned by a given remote node #####################
void remoteAllFlowsOwnedBy(std::vector<FlowSpec*>& flowSet,
                           const sctp_assoc_t assocID)
{
   std::vector<FlowSpec*>::iterator iterator = flowSet.begin();
   while(iterator != flowSet.end()) {
      FlowSpec* flowSpec = *iterator;
      if(flowSpec->RemoteControlAssocID == assocID) {
         if(flowSpec->RemoteAddressIsValid) {
            if(flowSpec->Protocol == IPPROTO_SCTP) {
               sendAbort(flowSpec->SocketDescriptor, flowSpec->RemoteDataAssocID);
            }
            else if(flowSpec->Protocol == IPPROTO_UDP) {
               // Nothing to do for UDP
            }
            else {
               // TCP and DCCP
               ext_close(flowSpec->SocketDescriptor);
            }
         }
         StatisticsWriter::removeMeasurement(flowSpec->MeasurementID);

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
