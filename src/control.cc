/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2024 by Thomas Dreibholz
 * ==========================================================================
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
 * Contact:  dreibh@simula.no
 * Homepage: https://www.nntb.no/~dreibh/netperfmeter/
 */

#include "control.h"
#include "loglevel.h"
#include "tools.h"

#include <string.h>
#include <math.h>
#include <poll.h>
#include <assert.h>

#include <iostream>


unsigned int         gOutputVerbosity = 9;   // FIXME!
extern MessageReader gMessageReader;


// ##########################################################################
// #### Active Side Control                                              ####
// ##########################################################################


#define IDENTIFY_MAX_TRIALS 10
#define IDENTIFY_TIMEOUT    30000


// ###### Download file #####################################################
static bool downloadOutputFile(MessageReader* messageReader,
                               const int      controlSocket,
                               const char*    fileName)
{
   char                 messageBuffer[sizeof(NetPerfMeterResults) +
                                      NETPERFMETER_RESULTS_MAX_DATA_LENGTH];
   NetPerfMeterResults* resultsMsg = (NetPerfMeterResults*)&messageBuffer;

   FILE* fh = fopen(fileName, "w");
   if(fh == nullptr) {
      LOG_ERROR
      stdlog << format("Unable to create file %s: %s!",
                       fileName, strerror(errno)) << "\n";
      LOG_END
      return false;
   }
   bool success = false;
   ssize_t received = gMessageReader.receiveMessage(controlSocket, resultsMsg, sizeof(messageBuffer));
   while( (received == MRRM_PARTIAL_READ) || (received >= (ssize_t)sizeof(NetPerfMeterResults)) ) {
      if(received > 0) {
         const size_t bytes = ntohs(resultsMsg->Header.Length);
         if(resultsMsg->Header.Type != NETPERFMETER_RESULTS) {
            LOG_ERROR
            stdlog << format("Received unexpected message type 0x%02x on socket %d!",
                             (unsigned int)resultsMsg->Header.Type, controlSocket) << "\n";
            LOG_END
            fclose(fh);
            return false;
         }
         if(bytes != (size_t)received) {
            LOG_ERROR
            stdlog << "Received malformed NETPERFMETER_RESULTS message!" << "\n";
            LOG_END
            fclose(fh);
            return false;
         }

         if(bytes > sizeof(NetPerfMeterResults)) {
            if(fwrite((char*)&resultsMsg->Data, bytes - sizeof(NetPerfMeterResults), 1, fh) != 1) {
               LOG_ERROR
               stdlog << format("Unable to write results to file %s: %s!",
                                fileName, strerror(errno)) << "\n";
               LOG_END
               fclose(fh);
               return false;
            }
         }
         else {
            LOG_WARNING
            stdlog << format("No results received for file %s from socket %d!",
                             fileName, controlSocket) << "\n";
            LOG_END
         }

         if(resultsMsg->Header.Flags & NPMRF_EOF) {
            LOG_TRACE
            stdlog << "<done>" << "\n";
            LOG_END
            success = true;
            break;
         }
      }
      received = gMessageReader.receiveMessage(controlSocket, resultsMsg, sizeof(messageBuffer));
   }
   if(!success) {
      LOG_ERROR
      stdlog << format("Unable to download results from socket %d (errno=%s)!",
                       controlSocket, strerror(errno)) << "\n";
      LOG_END
   }
   fclose(fh);
   return success;
}


// ###### Download statistics file ##########################################
static bool downloadResults(MessageReader*     messageReader,
                            const int          controlSocket,
                            const std::string& namePattern,
                            const Flow*        flow)
{
   const std::string outputName =
      Flow::getNodeOutputName(
         namePattern, "passive",
         format("-%08x-%04x", flow->getFlowID(), flow->getStreamID()));

   LOG_INFO
   stdlog << format("Downloading results [%s] from socket %d",
                    outputName.c_str(), controlSocket) << "\n";
   LOG_END
   bool success = awaitNetPerfMeterAcknowledge(messageReader, controlSocket,
                                               flow->getMeasurementID(),
                                               flow->getFlowID(),
                                               flow->getStreamID());
   if(success) {
      LOG_TRACE
      stdlog << "<ack>" << "\n";
      LOG_END
      success = downloadOutputFile(messageReader, controlSocket, outputName.c_str());
   }

   LOG_TRACE
   stdlog << ((success == true) ? "okay": "FAILED") << "\n";
   LOG_END
   return success;
}


// ###### Tell remote node to add new flow ##################################
bool performNetPerfMeterAddFlow(MessageReader* messageReader,
                                int            controlSocket,
                                const Flow*    flow)
{
   // ====== Sent NETPERFMETER_ADD_FLOW to remote node ======================
   const size_t                addFlowMsgSize = sizeof(NetPerfMeterAddFlowMessage) +
                                                   (sizeof(NetPerfMeterOnOffEvent) * flow->getTrafficSpec().OnOffEvents.size());
   char                        addFlowMsgBuffer[addFlowMsgSize];
   NetPerfMeterAddFlowMessage* addFlowMsg = (NetPerfMeterAddFlowMessage*)&addFlowMsgBuffer;
   addFlowMsg->Header.Type   = NETPERFMETER_ADD_FLOW;
   addFlowMsg->Header.Flags  = 0x00;
   if(flow->getTrafficSpec().Debug == true) {
      addFlowMsg->Header.Flags |= NPMAFF_DEBUG;
   }
   if(flow->getTrafficSpec().NoDelay == true) {
      addFlowMsg->Header.Flags |= NPMAFF_NODELAY;
   }
   if(flow->getTrafficSpec().RepeatOnOff == true) {
      addFlowMsg->Header.Flags |= NPMAFF_REPEATONOFF;
   }

   addFlowMsg->Header.Length = htons(addFlowMsgSize);
   addFlowMsg->MeasurementID = hton64(flow->getMeasurementID());
   addFlowMsg->FlowID        = htonl(flow->getFlowID());
   addFlowMsg->StreamID      = htons(flow->getStreamID());
   addFlowMsg->Protocol      = flow->getTrafficSpec().Protocol;
   addFlowMsg->pad           = 0x00;
   for(size_t i = 0;i < NETPERFMETER_RNG_INPUT_PARAMETERS;i++) {
      addFlowMsg->FrameRate[i] = doubleToNetwork(flow->getTrafficSpec().InboundFrameRate[i]);
      addFlowMsg->FrameSize[i] = doubleToNetwork(flow->getTrafficSpec().InboundFrameSize[i]);
   }
   addFlowMsg->FrameRateRng  = flow->getTrafficSpec().InboundFrameRateRng;
   addFlowMsg->FrameSizeRng  = flow->getTrafficSpec().InboundFrameSizeRng;
   addFlowMsg->RcvBufferSize = htonl(flow->getTrafficSpec().RcvBufferSize);
   addFlowMsg->SndBufferSize = htonl(flow->getTrafficSpec().SndBufferSize);
   addFlowMsg->MaxMsgSize    = htons(flow->getTrafficSpec().MaxMsgSize);
   addFlowMsg->OrderedMode   = htonl((uint32_t)((long long)rint(flow->getTrafficSpec().OrderedMode * (double)0xffffffff)));
   addFlowMsg->ReliableMode  = htonl((uint32_t)((long long)rint(flow->getTrafficSpec().ReliableMode * (double)0xffffffff)));
   addFlowMsg->RetransmissionTrials =
      htonl((uint32_t)flow->getTrafficSpec().RetransmissionTrials |
            (uint32_t)(flow->getTrafficSpec().RetransmissionTrialsInMS ? NPMAF_RTX_TRIALS_IN_MILLISECONDS : 0));
   addFlowMsg->CCID          = flow->getTrafficSpec().CCID;
   addFlowMsg->CMT           = flow->getTrafficSpec().CMT;

   addFlowMsg->OnOffEvents   = htons(flow->getTrafficSpec().OnOffEvents.size());
   size_t i = 0;
   for(std::vector<OnOffEvent>::const_iterator iterator = flow->getTrafficSpec().OnOffEvents.begin();
       iterator != flow->getTrafficSpec().OnOffEvents.end();iterator++, i++) {
      addFlowMsg->OnOffEvent[i].RandNumGen = (*iterator).RandNumGen;
      addFlowMsg->OnOffEvent[i].Flags      = 0x00;
      if((*iterator).RelativeTime == true) {
         addFlowMsg->OnOffEvent[i].Flags |= NPOOEF_RELTIME;
      }
      addFlowMsg->OnOffEvent[i].pad = 0x00;
      for(size_t j = 0; j < NETPERFMETER_RNG_INPUT_PARAMETERS; j++) {
         addFlowMsg->OnOffEvent[i].ValueArray[j] = doubleToNetwork((*iterator).ValueArray[j]);
      }
   }
   memset((char*)&addFlowMsg->Description, 0, sizeof(addFlowMsg->Description));
   strncpy((char*)&addFlowMsg->Description, flow->getTrafficSpec().Description.c_str(),
           std::min(sizeof(addFlowMsg->Description), flow->getTrafficSpec().Description.size()));

   memset((char*)&addFlowMsg->CongestionControl, 0, sizeof(addFlowMsg->CongestionControl));
   strncpy((char*)&addFlowMsg->CongestionControl, flow->getTrafficSpec().CongestionControl.c_str(),
           std::min(sizeof(addFlowMsg->CongestionControl), flow->getTrafficSpec().CongestionControl.size()));

   memset((char*)&addFlowMsg->PathMgr, 0, sizeof(addFlowMsg->PathMgr));
   strncpy((char*)&addFlowMsg->PathMgr, flow->getTrafficSpec().PathMgr.c_str(),
           std::min(sizeof(addFlowMsg->PathMgr), flow->getTrafficSpec().PathMgr.size()));

   memset((char*)&addFlowMsg->Scheduler, 0, sizeof(addFlowMsg->Scheduler));
   strncpy((char*)&addFlowMsg->Scheduler, flow->getTrafficSpec().Scheduler.c_str(),
           std::min(sizeof(addFlowMsg->Scheduler), flow->getTrafficSpec().Scheduler.size()));

   addFlowMsg->NDiffPorts = htons(flow->getTrafficSpec().NDiffPorts);

   LOG_TRACE
   stdlog << "<R1>" << "\n";
   LOG_END
   if(ext_send(controlSocket, addFlowMsg, addFlowMsgSize, 0) <= 0) {
      LOG_ERROR
      stdlog << format("Sending message failed on socket %d: %s!",
                       controlSocket, strerror(errno)) << "\n";
      LOG_END
      return false;
   }

   // ====== Wait for NETPERFMETER_ACKNOWLEDGE ==============================
   LOG_TRACE
   stdlog << "<R2>" << "\n";
   LOG_END
   if(awaitNetPerfMeterAcknowledge(messageReader, controlSocket,
                                   flow->getMeasurementID(),
                                   flow->getFlowID(), flow->getStreamID()) == false) {
      LOG_ERROR
      stdlog << format("Receiving message failed on socket %d: %s!",
                       controlSocket, strerror(errno)) << "\n";
      LOG_END
      return false;
   }

   // ======  Let remote identify the new flow ==============================
   return performNetPerfMeterIdentifyFlow(messageReader, controlSocket, flow);
}


// ###### Let remote identify a new flow ####################################
bool performNetPerfMeterIdentifyFlow(MessageReader* messageReader,
                                     int            controlSocket,
                                     const Flow*    flow)
{
   // ====== Sent NETPERFMETER_IDENTIFY_FLOW to remote node =================
   unsigned int maxTrials = 1;
   if( (flow->getTrafficSpec().Protocol != IPPROTO_SCTP) &&
       (flow->getTrafficSpec().Protocol != IPPROTO_TCP) &&
       (flow->getTrafficSpec().Protocol != IPPROTO_MPTCP) ) {
      // SCTP and TCP are reliable transport protocols => no retransmissions
      maxTrials = IDENTIFY_MAX_TRIALS;
   }
   for(unsigned int trial = 0;trial < maxTrials;trial++) {
      NetPerfMeterIdentifyMessage identifyMsg;
      identifyMsg.Header.Type   = NETPERFMETER_IDENTIFY_FLOW;
      identifyMsg.Header.Flags  = 0x00;
      if(flow->getVectorFile().getFormat() == OFF_None) {
         identifyMsg.Header.Flags |= NPMIF_NO_VECTORS;
      }
      else if(flow->getVectorFile().getFormat() == OFF_BZip2) {
         identifyMsg.Header.Flags |= NPMIF_COMPRESS_VECTORS;
      }
      identifyMsg.Header.Length = htons(sizeof(identifyMsg));
      identifyMsg.MagicNumber   = hton64(NETPERFMETER_IDENTIFY_FLOW_MAGIC_NUMBER);
      identifyMsg.MeasurementID = hton64(flow->getMeasurementID());
      identifyMsg.FlowID        = htonl(flow->getFlowID());
      identifyMsg.StreamID      = htons(flow->getStreamID());

      if(gOutputVerbosity >= NPFOV_CONNECTIONS) {
         gOutputMutex.lock();
         std::cerr << "<R3> "; std::cerr.flush();
         gOutputMutex.unlock();
      }
      if(flow->getTrafficSpec().Protocol == IPPROTO_SCTP) {
         sctp_sndrcvinfo sinfo;
         memset(&sinfo, 0, sizeof(sinfo));
         sinfo.sinfo_stream = flow->getStreamID();
         sinfo.sinfo_ppid   = htonl(PPID_NETPERFMETER_CONTROL);
         if(sctp_send(flow->getSocketDescriptor(), &identifyMsg, sizeof(identifyMsg), &sinfo, 0) <= 0) {
            return false;
         }
      }
      else {
         if(ext_send(flow->getSocketDescriptor(), &identifyMsg, sizeof(identifyMsg), 0) <= 0) {
            return false;
         }
      }
      if(gOutputVerbosity >= NPFOV_CONNECTIONS) {
         gOutputMutex.lock();
         std::cerr << "<R4> "; std::cerr.flush();
         gOutputMutex.unlock();
      }
      if(awaitNetPerfMeterAcknowledge(messageReader, controlSocket,
                                      flow->getMeasurementID(),
                                      flow->getFlowID(), flow->getStreamID(),
                                      IDENTIFY_TIMEOUT) == true) {
         return true;
      }
   }
   return false;
}


// ###### Start measurement #################################################
bool performNetPerfMeterStart(MessageReader*         messageReader,
                              int                    controlSocket,
                              const uint64_t         measurementID,
                              const char*            activeNodeName,
                              const char*            passiveNodeName,
                              const char*            configName,
                              const char*            vectorNamePattern,
                              const OutputFileFormat vectorFileFormat,
                              const char*            scalarNamePattern,
                              const OutputFileFormat scalarFileFormat)
{
   // ====== Write config file ==============================================
   FILE* configFile = nullptr;
   if(configName[0] != 0x00) {
      configFile = fopen(configName, "w");
      if(configFile == nullptr) {
         LOG_ERROR
         stdlog << format("Unable to create config file %s: %s!",
                          configName, strerror(errno)) << "\n";
         LOG_END
         return false;
      }

      FlowManager::getFlowManager()->lock();

      fprintf(configFile, "NUM_FLOWS=%u\n", (unsigned int)FlowManager::getFlowManager()->getFlowSet().size());
      fprintf(configFile, "NAME_ACTIVE_NODE=\"%s\"\n", activeNodeName);
      fprintf(configFile, "NAME_PASSIVE_NODE=\"%s\"\n", passiveNodeName);
      fprintf(configFile, "SCALAR_ACTIVE_NODE=\"%s\"\n",
                           Flow::getNodeOutputName(scalarNamePattern, "active").c_str());
      fprintf(configFile, "SCALAR_PASSIVE_NODE=\"%s\"\n",
                           Flow::getNodeOutputName(scalarNamePattern, "passive").c_str());
      fprintf(configFile, "VECTOR_ACTIVE_NODE=\"%s\"\n",
                           Flow::getNodeOutputName(vectorNamePattern, "active").c_str());
      fprintf(configFile, "VECTOR_PASSIVE_NODE=\"%s\"\n\n",
                           Flow::getNodeOutputName(vectorNamePattern, "passive").c_str());

      for(std::vector<Flow*>::iterator iterator = FlowManager::getFlowManager()->getFlowSet().begin();
         iterator != FlowManager::getFlowManager()->getFlowSet().end();
         iterator++) {
         const Flow* flow = *iterator;
         char extension[32];
         snprintf((char*)&extension, sizeof(extension), "-%08x-%04x",        flow->getFlowID(), flow->getStreamID());
         fprintf(configFile, "FLOW%u_DESCRIPTION=\"%s\"\n",                  flow->getFlowID(), flow->getTrafficSpec().Description.c_str());
         fprintf(configFile, "FLOW%u_PROTOCOL=\"%s\"\n",                     flow->getFlowID(), getProtocolName(flow->getTrafficSpec().Protocol));
         for(unsigned int i = 0;i < NETPERFMETER_RNG_INPUT_PARAMETERS;i++) {
            fprintf(configFile, "FLOW%u_OUTBOUND_FRAME_RATE_VALUE%u=%f\n",   flow->getFlowID(), i + 1, flow->getTrafficSpec().OutboundFrameRate[i]);
         }
         fprintf(configFile, "FLOW%u_OUTBOUND_FRAME_RATE_RNG_ID=%u\n",       flow->getFlowID(), flow->getTrafficSpec().OutboundFrameRateRng);
         fprintf(configFile, "FLOW%u_OUTBOUND_FRAME_RATE_RNG_NAME=\"%s\"\n", flow->getFlowID(), getRandomGeneratorName(flow->getTrafficSpec().OutboundFrameRateRng));
         for(unsigned int i = 0;i < NETPERFMETER_RNG_INPUT_PARAMETERS;i++) {
            fprintf(configFile, "FLOW%u_OUTBOUND_FRAME_SIZE_VALUE%u=%f\n",   flow->getFlowID(), i + 1, flow->getTrafficSpec().OutboundFrameSize[i]);
         }
         fprintf(configFile, "FLOW%u_OUTBOUND_FRAME_SIZE_RNG=%u\n",          flow->getFlowID(), flow->getTrafficSpec().OutboundFrameSizeRng);
         for(unsigned int i = 0;i < NETPERFMETER_RNG_INPUT_PARAMETERS;i++) {
            fprintf(configFile, "FLOW%u_INBOUND_FRAME_RATE_VALUE%u=%f\n",   flow->getFlowID(), i + 1, flow->getTrafficSpec().InboundFrameRate[i]);
         }
         fprintf(configFile, "FLOW%u_INBOUND_FRAME_RATE_RNG_ID=%u\n",        flow->getFlowID(), flow->getTrafficSpec().InboundFrameRateRng);
         fprintf(configFile, "FLOW%u_INBOUND_FRAME_RATE_RNG_NAME=\"%s\"\n",  flow->getFlowID(), getRandomGeneratorName(flow->getTrafficSpec().InboundFrameRateRng));
         for(unsigned int i = 0;i < NETPERFMETER_RNG_INPUT_PARAMETERS;i++) {
            fprintf(configFile, "FLOW%u_INBOUND_FRAME_SIZE_VALUE%u=%f\n",   flow->getFlowID(), i + 1, flow->getTrafficSpec().InboundFrameSize[i]);
         }
         fprintf(configFile, "FLOW%u_INBOUND_FRAME_SIZE_RNG=%u\n",           flow->getFlowID(), flow->getTrafficSpec().InboundFrameSizeRng);
         fprintf(configFile, "FLOW%u_RELIABLE=%f\n",                         flow->getFlowID(), flow->getTrafficSpec().ReliableMode);
         fprintf(configFile, "FLOW%u_ORDERED=%f\n",                          flow->getFlowID(), flow->getTrafficSpec().OrderedMode);
         fprintf(configFile, "FLOW%u_NODELAY=\"%s\"\n",                      flow->getFlowID(), (flow->getTrafficSpec().NoDelay == true) ? "on" : "off");
         fprintf(configFile, "FLOW%u_DEBUG=\"%s\"\n",                        flow->getFlowID(), (flow->getTrafficSpec().Debug == true) ? "on" : "off");
         fprintf(configFile, "FLOW%u_CC=\"%s\"\n",                           flow->getFlowID(), flow->getTrafficSpec().CongestionControl.c_str());
         fprintf(configFile, "FLOW%u_PATHMGR=\"%s\"\n",                      flow->getFlowID(), flow->getTrafficSpec().PathMgr.c_str());
         fprintf(configFile, "FLOW%u_SCHEDULER=\"%s\"\n",                    flow->getFlowID(), flow->getTrafficSpec().Scheduler.c_str());
         fprintf(configFile, "FLOW%u_NDIFFPORTS=%u\n",                       flow->getFlowID(), flow->getTrafficSpec().NDiffPorts);
         fprintf(configFile, "FLOW%u_VECTOR_ACTIVE_NODE=\"%s\"\n",           flow->getFlowID(), flow->getVectorFile().getName().c_str());
         fprintf(configFile, "FLOW%u_VECTOR_PASSIVE_NODE=\"%s\"\n\n",        flow->getFlowID(),
                              Flow::getNodeOutputName(vectorNamePattern, "passive",
                                 format("-%08x-%04x", flow->getFlowID(), flow->getStreamID())).c_str());
      }

      FlowManager::getFlowManager()->unlock();

      fclose(configFile);
   }

   // ====== Start flows ====================================================
   const bool success = FlowManager::getFlowManager()->startMeasurement(
                           controlSocket, measurementID, getMicroTime(),
                           vectorNamePattern, vectorFileFormat,
                           scalarNamePattern, scalarFileFormat,
                           (gOutputVerbosity >= NPFOV_FLOWS));
   if(success) {
      // ====== Tell passive node to start measurement ======================
      NetPerfMeterStartMessage startMsg;
      startMsg.Header.Type   = NETPERFMETER_START;
      startMsg.Header.Length = htons(sizeof(startMsg));
      startMsg.Padding       = 0x00000000;
      startMsg.MeasurementID = hton64(measurementID);
      startMsg.Header.Flags  = 0x00;
      if(scalarNamePattern[0] == 0x00) {
         startMsg.Header.Flags |= NPMSF_NO_SCALARS;
      }
      if(hasSuffix(scalarNamePattern, ".bz2")) {
         startMsg.Header.Flags |= NPMSF_COMPRESS_SCALARS;
      }
      if(vectorNamePattern[0] == 0x00) {
         startMsg.Header.Flags |= NPMSF_NO_VECTORS;
      }
      if(hasSuffix(vectorNamePattern, ".bz2")) {
         startMsg.Header.Flags |= NPMSF_COMPRESS_VECTORS;
      }

      LOG_INFO
      stdlog << format("Starting measurement on socket %d ...",
                       controlSocket) << "\n";
      LOG_END
      if(ext_send(controlSocket, &startMsg, sizeof(startMsg), 0) < 0) {
         return false;
      }
      LOG_TRACE
      stdlog << "<wait>";
      LOG_END
      if(awaitNetPerfMeterAcknowledge(messageReader, controlSocket,
                                      measurementID, 0, 0) == false) {
         return false;
      }
      LOG_TRACE
      stdlog << "<okay>" << "\n";
      LOG_END
      return true;
   }
   return false;
}


// ###### Tell remote node to remove a flow #################################
static bool sendNetPerfMeterRemoveFlow(MessageReader* messageReader,
                                       int            controlSocket,
                                       Measurement*   measurement,
                                       Flow*          flow)
{
   flow->getVectorFile().finish(true);

   NetPerfMeterRemoveFlowMessage removeFlowMsg;
   removeFlowMsg.Header.Type   = NETPERFMETER_REMOVE_FLOW;
   removeFlowMsg.Header.Flags  = 0x00;
   removeFlowMsg.Header.Length = htons(sizeof(removeFlowMsg));
   removeFlowMsg.MeasurementID = hton64(flow->getMeasurementID());
   removeFlowMsg.FlowID        = htonl(flow->getFlowID());
   removeFlowMsg.StreamID      = htons(flow->getStreamID());

   LOG_TRACE
   stdlog << format("<flow %u>", flow->getFlowID()) << "\n";
   LOG_END
   if(ext_send(controlSocket, &removeFlowMsg, sizeof(removeFlowMsg), 0) <= 0) {
      return false;
   }
   if(measurement->getVectorNamePattern() != "") {
      return(downloadResults(messageReader, controlSocket,
                             measurement->getVectorNamePattern(), flow));
   }
   return true;
}


// ###### Stop measurement ##################################################
bool performNetPerfMeterStop(MessageReader* messageReader,
                             int            controlSocket,
                             const uint64_t measurementID)
{
   // ====== Stop flows =====================================================
   FlowManager::getFlowManager()->lock();
   FlowManager::getFlowManager()->stopMeasurement(controlSocket, measurementID);
   Measurement* measurement = FlowManager::getFlowManager()->findMeasurement(controlSocket,
                                                                             measurementID);
   assert(measurement != nullptr);
   measurement->writeScalarStatistics(getMicroTime());
   FlowManager::getFlowManager()->unlock();

   // ====== Tell passive node to stop measurement ==========================
   NetPerfMeterStopMessage stopMsg;
   stopMsg.Header.Type   = NETPERFMETER_STOP;
   stopMsg.Header.Flags  = 0x00;
   stopMsg.Header.Length = htons(sizeof(stopMsg));
   stopMsg.Padding       = 0x00000000;
   stopMsg.MeasurementID = hton64(measurementID);

   LOG_INFO
   stdlog << format("Stopping measurement on socket %d ...",
                    controlSocket) << "\n";
   LOG_END
   if(ext_send(controlSocket, &stopMsg, sizeof(stopMsg), 0) < 0) {
      return false;
   }
   LOG_TRACE
   stdlog << "<waiting>" << "\n";
   LOG_END
   if(awaitNetPerfMeterAcknowledge(messageReader, controlSocket,
                                   measurementID, 0, 0) == false) {
      return false;
   }

   // ====== Download passive node's vector file ============================
   if(measurement->getVectorNamePattern() != "") {
      const std::string vectorName = Flow::getNodeOutputName(
         measurement->getVectorNamePattern(), "passive");

      LOG_INFO
      stdlog << format("Downloading vector results [%s] from socket %d",
                       vectorName.c_str(), controlSocket) << "\n";
      LOG_END
      if(downloadOutputFile(messageReader, controlSocket, vectorName.c_str()) == false) {
         delete measurement;
         return false;
      }
      LOG_TRACE
      stdlog << "<okay>" << "\n";
      LOG_END
   }

   // ====== Download passive node's scalar file ============================
   if(measurement->getScalarNamePattern() != "") {
      const std::string scalarName = Flow::getNodeOutputName(
         measurement->getScalarNamePattern(), "passive");
      LOG_INFO
      stdlog << format("Downloading scalar results [%s] from socket %d",
                       scalarName.c_str(), controlSocket) << "\n";
      LOG_END
      if(downloadOutputFile(messageReader, controlSocket, scalarName.c_str()) == false) {
         delete measurement;
         return false;
      }
      LOG_TRACE
      stdlog << "<okay>" << "\n";
      LOG_END
   }

   // ====== Download flow results and remove the flows =====================
   FlowManager::getFlowManager()->lock();
   std::vector<Flow*>::iterator iterator = FlowManager::getFlowManager()->getFlowSet().begin();
   while(iterator != FlowManager::getFlowManager()->getFlowSet().end()) {
      Flow* flow = *iterator;
      if(flow->getMeasurementID() == measurementID) {
         if(sendNetPerfMeterRemoveFlow(messageReader, controlSocket,
                                       measurement, flow) == false) {
            delete measurement;
            return false;
         }
         LOG_INFO
         flow->print(stdlog, true);
         LOG_END
         delete flow;
         // Invalidated iterator. Is there a better solution?
         iterator = FlowManager::getFlowManager()->getFlowSet().begin();
         continue;
      }
      iterator++;
   }
   FlowManager::getFlowManager()->unlock();

   // ====== Remove the Measurement object =================================
   delete measurement;
   return true;
}


// ###### Wait for NETPERFMETER_ACKNOWLEDGE from remote node ################
bool awaitNetPerfMeterAcknowledge(MessageReader* messageReader,
                                  int            controlSocket,
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
   const int result = ext_poll_wrapper(&pfd, 1, timeout);
   if(result < 1) {
      LOG_TRACE
      stdlog << "<timeout>" << "\n";
      LOG_END
      return false;
   }
   if(!(pfd.revents & POLLIN)) {
      LOG_TRACE
      stdlog << "<no answer>" << "\n";
      LOG_END
      return false;
   }

   // ====== Read NETPERFMETER_ACKNOWLEDGE message ==========================
   NetPerfMeterAcknowledgeMessage ackMsg;
   ssize_t                        received;
   do {
      received = gMessageReader.receiveMessage(controlSocket, &ackMsg, sizeof(ackMsg));
   } while(received == MRRM_PARTIAL_READ);
   if(received < (ssize_t)sizeof(ackMsg)) {
      return false;
   }
   if(ackMsg.Header.Type != NETPERFMETER_ACKNOWLEDGE) {
      LOG_WARNING
      stdlog << format("Received message type 0x%02x instead of NETPERFMETER_ACKNOWLEDGE on socket %d!",
                       (unsigned int)ackMsg.Header.Type, controlSocket) << "\n";
      LOG_END
      return false;
   }

   // ====== Check whether NETPERFMETER_ACKNOWLEDGE is okay =================
   if( (ntoh64(ackMsg.MeasurementID) != measurementID) ||
       (ntohl(ackMsg.FlowID) != flowID) ||
       (ntohs(ackMsg.StreamID) != streamID) ) {
      LOG_WARNING
      stdlog << format("Received NETPERFMETER_ACKNOWLEDGE for wrong measurement/flow/stream on socket %d!",
                       controlSocket) << "\n";
      LOG_END
      return false;
   }

   const uint32_t status = ntohl(ackMsg.Status);
   LOG_TRACE
   stdlog << format("<status=%u>", status) << "\n";
   LOG_END
   return status == NETPERFMETER_STATUS_OKAY;
}



// ##########################################################################
// #### Passive Side Control                                             ####
// ##########################################################################


// ###### Upload file #######################################################
static bool uploadOutputFile(const int         controlSocket,
                             const OutputFile& outputFile)
{
   char                 messageBuffer[sizeof(NetPerfMeterResults) +
                                      NETPERFMETER_RESULTS_MAX_DATA_LENGTH];
   NetPerfMeterResults* resultsMsg = (NetPerfMeterResults*)&messageBuffer;

   // ====== Initialize header ==============================================
   resultsMsg->Header.Type = NETPERFMETER_RESULTS;

   // ====== Transmission loop ==============================================
   LOG_INFO
   stdlog << format("Uploading results on socket %d ...", controlSocket) << "\n";
   LOG_END

   bool success = true;
   do {
      // ====== Read chunk from file ========================================
      const size_t bytes = fread(&resultsMsg->Data, 1,
                                 NETPERFMETER_RESULTS_MAX_DATA_LENGTH,
                                 outputFile.getFile());
      resultsMsg->Header.Flags  = feof(outputFile.getFile()) ? NPMRF_EOF : 0x00;
      resultsMsg->Header.Length = htons(sizeof(NetPerfMeterResults) + bytes);
      if(ferror(outputFile.getFile())) {
         LOG_ERROR
         stdlog << format("Failed to read results from %s: %s!",
                          outputFile.getName().c_str(), strerror(errno)) << "\n";
         LOG_END
         success = false;
         break;
      }

      // ====== Transmit chunk ==============================================
      if(ext_send(controlSocket, resultsMsg, sizeof(NetPerfMeterResults) + bytes, 0) < 0) {
         LOG_ERROR
         stdlog << format("Failed to upload results on socket %d: %s!",
                          controlSocket, strerror(errno)) << "\n";
         LOG_END
         success = false;
         break;
      }
   } while(!(resultsMsg->Header.Flags & NPMRF_EOF));

   // ====== Check results ==================================================
   if(!success) {
      LOG_WARNING
      stdlog << format("Unable to upload results on socket %d (errno=%s)!",
                       controlSocket, strerror(errno)) << "\n";
      LOG_END
      ext_shutdown(controlSocket, 2);
   }
   LOG_TRACE
   stdlog << ((success == true) ? "okay": "FAILED") << "\n";
   LOG_END
   return success;
}


// ###### Upload per-flow statistics file ###################################
static bool uploadResults(const int controlSocket,
                          Flow*     flow)
{
   bool success = flow->getVectorFile().finish(false);
   if(success) {
      success = sendNetPerfMeterAcknowledge(
                   controlSocket,
                   flow->getMeasurementID(), flow->getFlowID(), flow->getStreamID(),
                   (success == true) ? NETPERFMETER_STATUS_OKAY :
                                       NETPERFMETER_STATUS_ERROR);
      if(success) {
         if(flow->getVectorFile().exists()) {
            success = uploadOutputFile(controlSocket, flow->getVectorFile());
         }
      }
   }
   flow->getVectorFile().finish(true);
   return success;
}


// ###### Handle NETPERFMETER_ADD_FLOW ######################################
static bool handleNetPerfMeterAddFlow(MessageReader*                    messageReader,
                                      const int                         controlSocket,
                                      const NetPerfMeterAddFlowMessage* addFlowMsg,
                                      const size_t                      received)
{
   if(received < sizeof(NetPerfMeterAddFlowMessage)) {
      LOG_WARNING
      stdlog << format("Received malformed NETPERFMETER_ADD_FLOW control message on socket %d!",
                       controlSocket) << "\n";
      LOG_END
      ext_shutdown(controlSocket, 2);
      return false;
   }
   const uint64_t measurementID   = ntoh64(addFlowMsg->MeasurementID);
   const uint32_t flowID          = ntohl(addFlowMsg->FlowID);
   const uint16_t streamID        = ntohs(addFlowMsg->StreamID);
   const size_t   startStopEvents = ntohs(addFlowMsg->OnOffEvents);
   if(received < sizeof(NetPerfMeterAddFlowMessage) + (startStopEvents * sizeof(NetPerfMeterOnOffEvent))) {
      LOG_WARNING
      stdlog << format("Too few start/stop entries in NETPERFMETER_ADD_FLOW control message on socket %d!",
                       controlSocket) << "\n";
      LOG_END
      ext_shutdown(controlSocket, 2);
      return false;
   }
   char description[sizeof(addFlowMsg->Description) + 1];
   memcpy((char*)&description, (const char*)&addFlowMsg->Description, sizeof(addFlowMsg->Description));
   description[sizeof(addFlowMsg->Description)] = 0x00;
   if(FlowManager::getFlowManager()->findFlow(measurementID, flowID, streamID)) {
      LOG_WARNING
      stdlog << format("NETPERFMETER_ADD_FLOW tried to add already-existing flow on socket %d!",
                       controlSocket) << "\n";
      LOG_END
      return(sendNetPerfMeterAcknowledge(controlSocket,
                                         measurementID, flowID, streamID,
                                         NETPERFMETER_STATUS_ERROR));
   }
   else {
      // ====== Create new flow =============================================
      FlowTrafficSpec trafficSpec;
      trafficSpec.Protocol    = addFlowMsg->Protocol;
      assert(addFlowMsg->Protocol <= 255);
      trafficSpec.Description = std::string(description);
      for(size_t i = 0;i < NETPERFMETER_RNG_INPUT_PARAMETERS;i++) {
         trafficSpec.OutboundFrameRate[i] = networkToDouble(addFlowMsg->FrameRate[i]);
         trafficSpec.OutboundFrameSize[i] = networkToDouble(addFlowMsg->FrameSize[i]);
      }
      trafficSpec.ErrorOnAbort             = false;
      trafficSpec.OutboundFrameRateRng     = addFlowMsg->FrameRateRng;
      trafficSpec.OutboundFrameSizeRng     = addFlowMsg->FrameSizeRng;
      trafficSpec.MaxMsgSize               = ntohs(addFlowMsg->MaxMsgSize);
      trafficSpec.RcvBufferSize            = ntohl(addFlowMsg->RcvBufferSize);
      trafficSpec.SndBufferSize            = ntohl(addFlowMsg->SndBufferSize);
      trafficSpec.OrderedMode              = ntohl(addFlowMsg->OrderedMode)  / (double)0xffffffff;
      trafficSpec.ReliableMode             = ntohl(addFlowMsg->ReliableMode) / (double)0xffffffff;
      trafficSpec.CMT                      = addFlowMsg->CMT;
      trafficSpec.CCID                     = addFlowMsg->CCID;
      trafficSpec.NoDelay                  = (addFlowMsg->Header.Flags & NPMAFF_NODELAY);
      trafficSpec.Debug                    = (addFlowMsg->Header.Flags & NPMAFF_DEBUG);
      trafficSpec.RepeatOnOff              = (addFlowMsg->Header.Flags & NPMAFF_REPEATONOFF);
      trafficSpec.RetransmissionTrials     = ntohl(addFlowMsg->RetransmissionTrials) & ~NPMAF_RTX_TRIALS_IN_MILLISECONDS;
      trafficSpec.RetransmissionTrialsInMS = (ntohl(addFlowMsg->RetransmissionTrials) & NPMAF_RTX_TRIALS_IN_MILLISECONDS);
      if( (trafficSpec.RetransmissionTrialsInMS) && (trafficSpec.RetransmissionTrials == 0x7fffffff) ) {
         trafficSpec.RetransmissionTrials = ~0;
      }

      const NetPerfMeterOnOffEvent* event = (const NetPerfMeterOnOffEvent*)&addFlowMsg->OnOffEvent;
      for(size_t i = 0;i < startStopEvents;i++) {
         OnOffEvent newEvent;
         newEvent.RandNumGen   = event[i].RandNumGen;
         newEvent.RelativeTime = (event[i].Flags & NPOOEF_RELTIME);
         for(size_t j = 0; j < NETPERFMETER_RNG_INPUT_PARAMETERS; j++) {
            newEvent.ValueArray[j] = networkToDouble(event[i].ValueArray[j]);
         }
         trafficSpec.OnOffEvents.push_back(newEvent);
      }

      char congestionControl[sizeof(addFlowMsg->CongestionControl) + 1];
      memcpy((char*)&congestionControl, (const char*)&addFlowMsg->CongestionControl, sizeof(addFlowMsg->CongestionControl));
      congestionControl[sizeof(addFlowMsg->CongestionControl)] = 0x00;
      trafficSpec.CongestionControl = std::string(congestionControl);

      char pathMgr[sizeof(addFlowMsg->PathMgr) + 1];
      memcpy((char*)&pathMgr, (const char*)&addFlowMsg->PathMgr, sizeof(addFlowMsg->PathMgr));
      pathMgr[sizeof(addFlowMsg->PathMgr)] = 0x00;
      trafficSpec.PathMgr = std::string(pathMgr);

      char scheduler[sizeof(addFlowMsg->Scheduler) + 1];
      memcpy((char*)&scheduler, (const char*)&addFlowMsg->Scheduler, sizeof(addFlowMsg->Scheduler));
      scheduler[sizeof(addFlowMsg->Scheduler)] = 0x00;
      trafficSpec.Scheduler = std::string(scheduler);

      trafficSpec.NDiffPorts = ntohs(addFlowMsg->NDiffPorts);

      Flow* flow = new Flow(ntoh64(addFlowMsg->MeasurementID), ntohl(addFlowMsg->FlowID),
                            ntohs(addFlowMsg->StreamID), trafficSpec,
                            controlSocket);
      return(sendNetPerfMeterAcknowledge(controlSocket,
                                         measurementID, flowID, streamID,
                                         (flow != nullptr) ? NETPERFMETER_STATUS_OKAY :
                                                             NETPERFMETER_STATUS_ERROR));
   }
}


// ###### Handle NETPERFMETER_REMOVE_FLOW #####################################
static bool handleNetPerfMeterRemoveFlow(MessageReader*                       messageReader,
                                         const int                            controlSocket,
                                         const NetPerfMeterRemoveFlowMessage* removeFlowMsg,
                                         const size_t                         received)
{
   if(received < sizeof(NetPerfMeterRemoveFlowMessage)) {
      LOG_WARNING
      stdlog << format("Received malformed NETPERFMETER_REMOVE_FLOW control message on socket %d!",
                       controlSocket) << "\n";
      LOG_END
      ext_shutdown(controlSocket, 2);
      return false;
   }
   const uint64_t measurementID = ntoh64(removeFlowMsg->MeasurementID);
   const uint32_t flowID        = ntohl(removeFlowMsg->FlowID);
   const uint16_t streamID      = ntohs(removeFlowMsg->StreamID);
   Flow* flow = FlowManager::getFlowManager()->findFlow(measurementID, flowID, streamID);
   if(flow == nullptr) {
      LOG_WARNING
      stdlog << format("NETPERFMETER_ADD_REMOVE tried to remove not-existing flow on socket %d!",
                       controlSocket) << "\n";
      LOG_END
      return(sendNetPerfMeterAcknowledge(controlSocket,
                                         measurementID, flowID, streamID,
                                         NETPERFMETER_STATUS_ERROR));
   }
   else {
      // ------ Remove Flow from Flow Manager -------------------------
      FlowManager::getFlowManager()->removeFlow(flow);
      // ------ Upload statistics file --------------------------------
      flow->getVectorFile().finish(false);
      if(flow->getVectorFile().exists()) {
         uploadResults(controlSocket, flow);
      }

      delete flow;
   }
   return true;
}


// ###### Handle NETPERFMETER_START #########################################
static bool handleNetPerfMeterStart(MessageReader*                  messageReader,
                                    const int                       controlSocket,
                                    const NetPerfMeterStartMessage* startMsg,
                                    const size_t                    received)
{
   if(received < sizeof(NetPerfMeterStartMessage)) {
      LOG_WARNING
      stdlog << format("Received malformed NETPERFMETER_START control message on socket %d!",
                       controlSocket) << "\n";
      LOG_END
      ext_shutdown(controlSocket, 2);
      return false;
   }
   const uint64_t measurementID = ntoh64(startMsg->MeasurementID);

   if(gOutputVerbosity >= NPFOV_STATUS) {
      gOutputMutex.lock();
      std::cerr << "\n";
      printTimeStamp(std::cerr);
      std::cerr << "Starting measurement "
                << format("$%llx", (unsigned long long)measurementID) << " ...\n";
      gOutputMutex.unlock();
   }

   OutputFileFormat scalarFileFormat = OFF_Plain;
   if(startMsg->Header.Flags & NPMSF_COMPRESS_SCALARS) {
      scalarFileFormat = OFF_BZip2;
   }
   if(startMsg->Header.Flags & NPMSF_NO_SCALARS) {
      scalarFileFormat = OFF_None;
   }
   OutputFileFormat vectorFileFormat = OFF_Plain;
   if(startMsg->Header.Flags & NPMSF_COMPRESS_VECTORS) {
      vectorFileFormat = OFF_BZip2;
   }
   if(startMsg->Header.Flags & NPMSF_NO_VECTORS) {
      vectorFileFormat = OFF_None;
   }

   const unsigned long long now = getMicroTime();
   bool success = FlowManager::getFlowManager()->startMeasurement(
      controlSocket, measurementID, now,
      nullptr, vectorFileFormat,
      nullptr, scalarFileFormat,
      (gOutputVerbosity >= NPFOV_FLOWS));

   return(sendNetPerfMeterAcknowledge(controlSocket,
                                      measurementID, 0, 0,
                                      (success == true) ? NETPERFMETER_STATUS_OKAY :
                                                          NETPERFMETER_STATUS_ERROR));
}


// ###### Handle NETPERFMETER_STOP #########################################
static bool handleNetPerfMeterStop(MessageReader*                 messageReader,
                                   const int                      controlSocket,
                                   const NetPerfMeterStopMessage* stopMsg,
                                   const size_t                   received)
{
   if(received < sizeof(NetPerfMeterStopMessage)) {
      LOG_WARNING
      stdlog << format("Received malformed NETPERFMETER_STOP control message on socket %d!",
                       controlSocket) << "\n";
      LOG_END
      ext_shutdown(controlSocket, 2);
      return false;
   }
   const uint64_t measurementID = ntoh64(stopMsg->MeasurementID);

   if(gOutputVerbosity >= NPFOV_STATUS) {
      gOutputMutex.lock();
      std::cerr << "\n";
      printTimeStamp(std::cerr);
      std::cerr << "Stopping measurement "
                << format("$%llx", (unsigned long long)measurementID)
                << " ...\n";
      gOutputMutex.unlock();
   }

   // ====== Stop flows =====================================================
   FlowManager::getFlowManager()->lock();
   FlowManager::getFlowManager()->stopMeasurement(controlSocket, measurementID, (gOutputVerbosity >= NPFOV_FLOWS));
   bool         success     = false;
   Measurement* measurement =
      FlowManager::getFlowManager()->findMeasurement(controlSocket, measurementID);
   if(measurement) {
      measurement->lock();
      measurement->writeScalarStatistics(getMicroTime());
      success = measurement->finish(false);
      measurement->unlock();
   }

   // ====== Remove pointer to Measurement from its flows ===================
   for(std::vector<Flow*>::iterator iterator = FlowManager::getFlowManager()->getFlowSet().begin();
      iterator != FlowManager::getFlowManager()->getFlowSet().end();
      iterator++) {
      Flow* flow = *iterator;
      if(flow->getMeasurement() == measurement) {
         flow->setMeasurement(nullptr);
      }
   }
   FlowManager::getFlowManager()->unlock();

   // ====== Acknowledge result =============================================
   sendNetPerfMeterAcknowledge(controlSocket,
                               measurementID, 0, 0,
                               (success == true) ? NETPERFMETER_STATUS_OKAY :
                                                   NETPERFMETER_STATUS_ERROR);

   // ====== Upload results =================================================
   if(measurement) {
      if(success) {
         if(measurement->getVectorFile().exists()) {
            uploadOutputFile(controlSocket, measurement->getVectorFile());
         }
         if(measurement->getScalarFile().exists()) {
            uploadOutputFile(controlSocket, measurement->getScalarFile());
         }
      }
      delete measurement;
   }
   return true;
}


// ###### Delete all flows owned by a given remote node #####################
void handleControlAssocShutdown(int controlSocket)
{
   FlowManager::getFlowManager()->removeAllMeasurements(controlSocket);
}


// ###### Handle incoming control message ###################################
bool handleNetPerfMeterControlMessage(MessageReader* messageReader,
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
      return true;   // Partial read -> wait for next fragment.
   }
   else if(received < (ssize_t)sizeof(NetPerfMeterHeader)) {
      if(received < 0) {
         LOG_WARNING
         stdlog << format("Control connection is broken on socket %d!",
                          controlSocket) << "\n";
         LOG_END
         ext_shutdown(controlSocket, 2);
      }
      return false;
   }

   // ====== Received a SCTP notification ===================================
   if(flags & MSG_NOTIFICATION) {
      const sctp_notification* notification = (const sctp_notification*)&inputBuffer;
      if( (notification->sn_header.sn_type == SCTP_ASSOC_CHANGE) &&
          ((notification->sn_assoc_change.sac_state == SCTP_COMM_LOST) ||
           (notification->sn_assoc_change.sac_state == SCTP_SHUTDOWN_COMP)) ) {
        handleControlAssocShutdown(controlSocket);
      }
   }

   // ====== Received a real control message ================================
   else {
      const NetPerfMeterHeader* header = (const NetPerfMeterHeader*)&inputBuffer;
      if(ntohs(header->Length) != received) {
         LOG_WARNING
         stdlog << format("Received malformed control message on socket %d: expected length %u, but received length %u!",
                          controlSocket, (unsigned int)ntohs(header->Length), (unsigned int)received) << "\n";
         LOG_END
         ext_shutdown(controlSocket, 2);
         return false;
      }

      switch(header->Type) {
         case NETPERFMETER_ADD_FLOW:
            return handleNetPerfMeterAddFlow(
                      messageReader, controlSocket,
                      (const NetPerfMeterAddFlowMessage*)&inputBuffer, received);
         case NETPERFMETER_REMOVE_FLOW:
            return handleNetPerfMeterRemoveFlow(
                      messageReader, controlSocket,
                      (const NetPerfMeterRemoveFlowMessage*)&inputBuffer, received);
         case NETPERFMETER_START:
            return handleNetPerfMeterStart(
                      messageReader, controlSocket,
                      (const NetPerfMeterStartMessage*)&inputBuffer, received);
         case NETPERFMETER_STOP:
            return handleNetPerfMeterStop(
                      messageReader, controlSocket,
                      (const NetPerfMeterStopMessage*)&inputBuffer, received);
         default:
            LOG_WARNING
            stdlog << format("Received invalid control message of type 0x%02x!",
                             (unsigned int)header->Type) << "\n";
            LOG_END
            ext_shutdown(controlSocket, 2);
            return false;
      }
   }
   return true;
}


// ###### Handle NETPERFMETER_IDENTIFY_FLOW message #########################
void handleNetPerfMeterIdentify(const NetPerfMeterIdentifyMessage* identifyMsg,
                                const int                          sd,
                                const sockaddr_union*              from)
{
   int   controlSocketDescriptor;
   Flow* flow;

   OutputFileFormat vectorFileFormat = OFF_Plain;
   if(identifyMsg->Header.Flags & NPMIF_COMPRESS_VECTORS) {
      vectorFileFormat = OFF_BZip2;
   }
   if(identifyMsg->Header.Flags & NPMIF_NO_VECTORS) {
      vectorFileFormat = OFF_None;
   }

   flow = FlowManager::getFlowManager()->identifySocket(ntoh64(identifyMsg->MeasurementID),
                                                        ntohl(identifyMsg->FlowID),
                                                        ntohs(identifyMsg->StreamID),
                                                        sd, from,
                                                        vectorFileFormat,
                                                        controlSocketDescriptor);
   if(flow != nullptr) {
      const bool success = flow->configureSocket(sd);
      sendNetPerfMeterAcknowledge(controlSocketDescriptor,
                                  ntoh64(identifyMsg->MeasurementID),
                                  ntohl(identifyMsg->FlowID),
                                  ntohs(identifyMsg->StreamID),
                                  (success == true) ? NETPERFMETER_STATUS_OKAY :
                                                      NETPERFMETER_STATUS_ERROR);
   }
   else {
      // No known flow -> no association to send response to!
      gOutputMutex.lock();
      printTimeStamp(std::cerr);
      std::cerr << "WARNING: NETPERFMETER_IDENTIFY_FLOW message for unknown flow!\n";
      gOutputMutex.unlock();
   }
}


// ###### Send NETPERFMETER_ACKNOWLEDGE to remote node ######################
bool sendNetPerfMeterAcknowledge(int            controlSocket,
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
   ackMsg.Padding       = 0x0000;
   ackMsg.Status        = htonl(status);

   return ext_send(controlSocket, &ackMsg, sizeof(ackMsg), 0) > 0;
}
