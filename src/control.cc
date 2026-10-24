/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2026 by Thomas Dreibholz
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

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact:  dreibh@simula.no
 * Homepage: https://www.nntb.no/~dreibh/netperfmeter/
 */

#include "assure.h"
#include "control.h"
#include "fileio.h"
#include "loglevel.h"
#include "tools.h"
#include "outputfile.h"

#include <cmath>
#include <cstdint>
#include <cstring>


// ##########################################################################
// #### Active Side Control                                              ####
// ##########################################################################


#define UNRELIABLE_IDENTIFY_MAX_TRIALS     5   // UDP, DCCP
#define UNRELIABLE_IDENTIFY_TIMEOUT     2000   // UDP, DCCP
#define RELIABLE_IDENTIFY_MAX_TRIALS       1   // TCP+MPTCP, SCTP
#define RELIABLE_IDENTIFY_TIMEOUT      10000   // TCP+MPTCP, SCTP


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
      // TRANSLATORS: Error creating output file for downloading results. Placeholders: {} 1: file name, 2: strerror() message.
      stdlog << formatText(gettext("Unable to create file {}: {}!"),
                           fileName, strerror(errno)) << "\n";
      LOG_END
      return false;
   }
   bool success = false;
   ssize_t received = messageReader->receiveMessage(controlSocket, resultsMsg, sizeof(messageBuffer));
   while( (received == MRRM_PARTIAL_READ) || (received >= (ssize_t)sizeof(NetPerfMeterResults)) ) {
      if(received > 0) {
         const size_t bytes = be16toh(resultsMsg->Header.Length);
         if(resultsMsg->Header.Type != NETPERFMETER_RESULTS) {
            LOG_ERROR
            // TRANSLATORS: Error message when an unexpected message type is received. Placeholders: {:02x} 1: hex message type, {} 2: socket descriptor.
            stdlog << formatText(gettext("Received unexpected message type ${:02x} on socket {}!"),
                                 static_cast<unsigned int>(resultsMsg->Header.Type), controlSocket) << "\n";
            LOG_END
            fclose(fh);
            return false;
         }
         if(bytes != (size_t)received) {
            LOG_ERROR
            // TRANSLATORS: Error message when NETPERFMETER_RESULTS message header length does not match bytes received.
            stdlog << gettext("Received malformed NETPERFMETER_RESULTS message!") << "\n";
            LOG_END
            fclose(fh);
            return false;
         }

         if(bytes > sizeof(NetPerfMeterResults)) {
            if(fwrite((char*)&resultsMsg->Data, bytes - sizeof(NetPerfMeterResults), 1, fh) != 1) {
               LOG_ERROR
               // TRANSLATORS: Error writing downloaded measurement results to file. Placeholders: {} 1: file name, 2: strerror() message.
               stdlog << formatText(gettext("Unable to write results to file {}: {}!"),
                                    fileName, strerror(errno)) << "\n";
               LOG_END
               fclose(fh);
               return false;
            }
         }
         else {
            LOG_WARNING
            // TRANSLATORS: Warning message when empty results are received for a file. Placeholders: {} 1: file name, 2: socket descriptor.
            stdlog << formatText(gettext("No results received for file {} from socket {}!"),
                                 fileName, controlSocket) << "\n";
            LOG_END
         }

         if(resultsMsg->Header.Flags & NPMRF_EOF) {
            LOG_TRACE
            stdlog << "<done>\n";
            LOG_END
            success = true;
            break;
         }
      }
      received = messageReader->receiveMessage(controlSocket, resultsMsg, sizeof(messageBuffer));
   }
   if(!success) {
      LOG_ERROR
      // TRANSLATORS: Error message when downloading results from a socket fails. Placeholders: {} 1: socket descriptor, 2: strerror() message.
      stdlog << formatText(gettext("Unable to download results from socket {} (errno={})!"),
                           controlSocket, strerror(errno)) << "\n";
      LOG_END
   }
   fclose(fh);

   // ====== Transform format if older instance returned BZip2 =============
   if(success) {
      const FileFormat actualFormat = detectFormatFromContent(fileName);
      const FileFormat targetFormat = detectFormat(fileName);

      if((actualFormat == FileFormat::BZip2) && (targetFormat != FileFormat::BZip2)) {
         try {
            transformFile(fileName, actualFormat, fileName, targetFormat);
         }
         catch(const std::exception& ex) {
            LOG_ERROR
            stdlog << formatText("ERROR: Failed to transform downloaded BZip2 file <{}> to target format: {}!\n",
                                 fileName, ex.what());
            LOG_END
            return false;
         }
      }
   }

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
         formatText("-{:08x}-{:04x}", flow->getFlowID(), flow->getStreamID()));

   LOG_INFO
   // TRANSLATORS: Log message when beginning download of flow results. Placeholders: {} 1: file path, 2: socket descriptor.
   stdlog << formatText(gettext("Downloading results [{}] from socket {}"),
                        outputName, controlSocket) << "\n";
   LOG_END
   bool success = awaitNetPerfMeterAcknowledge(messageReader, controlSocket,
                                               flow->getMeasurementID(),
                                               flow->getFlowID(),
                                               flow->getStreamID());
   if(success) {
      LOG_TRACE
      stdlog << "<ack>\n";
      LOG_END
      success = downloadOutputFile(messageReader, controlSocket, outputName.c_str());
   }

   LOG_TRACE
   stdlog << ((success == true) ? "okay": "FAILED") << "\n";
   LOG_END
   return success;
}


// ###### Tell remote node to add new flow ##################################
bool performNetPerfMeterAddFlow(MessageReader*         messageReader,
                                int                    controlSocket,
                                const Flow*            flow,
                                const OutputFileFormat vectorFileFormat)
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

   addFlowMsg->Header.Length = htobe16(addFlowMsgSize);
   addFlowMsg->MeasurementID = htobe64(flow->getMeasurementID());
   addFlowMsg->FlowID        = htobe32(flow->getFlowID());
   addFlowMsg->StreamID      = htobe16(flow->getStreamID());
   addFlowMsg->Protocol      = htole16(flow->getTrafficSpec().Protocol);
   for(size_t i = 0;i < NETPERFMETER_RNG_INPUT_PARAMETERS;i++) {
      addFlowMsg->FrameRate[i] = doubleToNetwork(flow->getTrafficSpec().InboundFrameRate[i]);
      addFlowMsg->FrameSize[i] = doubleToNetwork(flow->getTrafficSpec().InboundFrameSize[i]);
   }
   addFlowMsg->FrameRateRng  = flow->getTrafficSpec().InboundFrameRateRng;
   addFlowMsg->FrameSizeRng  = flow->getTrafficSpec().InboundFrameSizeRng;
   addFlowMsg->RcvBufferSize = htobe32(flow->getTrafficSpec().RcvBufferSize);
   addFlowMsg->SndBufferSize = htobe32(flow->getTrafficSpec().SndBufferSize);
   addFlowMsg->MaxMsgSize    = htobe16(flow->getTrafficSpec().MaxMsgSize);
   addFlowMsg->OrderedMode   = htobe32((uint32_t)((long long)rint(flow->getTrafficSpec().OrderedMode * (double)0xffffffff)));
   addFlowMsg->ReliableMode  = htobe32((uint32_t)((long long)rint(flow->getTrafficSpec().ReliableMode * (double)0xffffffff)));
   addFlowMsg->RetransmissionTrials =
      htobe32((uint32_t)flow->getTrafficSpec().RetransmissionTrials |
            (uint32_t)(flow->getTrafficSpec().RetransmissionTrialsInMS ? NPMAF_RTX_TRIALS_IN_MILLISECONDS : 0));
   addFlowMsg->CCID          = flow->getTrafficSpec().CCID;
   addFlowMsg->CMT           = flow->getTrafficSpec().CMT;

   addFlowMsg->OnOffEvents   = htobe16(flow->getTrafficSpec().OnOffEvents.size());
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

   // Obsolete fields (used for Linux MPTCP development version):
   memset((char*)&addFlowMsg->PathMgr,   0, sizeof(addFlowMsg->PathMgr));
   memset((char*)&addFlowMsg->Scheduler, 0, sizeof(addFlowMsg->Scheduler));
   addFlowMsg->NDiffPorts = htobe16(0);

   LOG_TRACE
   stdlog << formatText("<R1 sd={}>\n", controlSocket);
   LOG_END
   if(ext_send(controlSocket, addFlowMsg, addFlowMsgSize, 0) <= 0) {
      LOG_ERROR
      // TRANSLATORS: Error message when transmitting a message on a socket fails. Placeholders: {} 1: socket descriptor, 2: strerror() message.
      stdlog << formatText(gettext("Sending message failed on socket {}: {}!"),
                           controlSocket, strerror(errno)) << "\n";
      LOG_END
      return false;
   }

   // ====== Wait for NETPERFMETER_ACKNOWLEDGE ==============================
   LOG_TRACE
   stdlog << formatText("<R2 sd={}>\n", controlSocket);
   LOG_END
   if(awaitNetPerfMeterAcknowledge(messageReader, controlSocket,
                                   flow->getMeasurementID(),
                                   flow->getFlowID(), flow->getStreamID()) == false) {
      LOG_ERROR
      // TRANSLATORS: Error message when receiving a message on a socket fails. Placeholders: {} 1: socket descriptor, 2: strerror() message.
      stdlog << formatText(gettext("Receiving message failed on socket {}: {}!"),
                           controlSocket, strerror(errno)) << "\n";
      LOG_END
      return false;
   }

   // ======  Let passive side identify the new flow ========================
   return performNetPerfMeterIdentifyFlow(messageReader, controlSocket, flow,
                                          vectorFileFormat);
}


// ###### Let remote identify a new flow ####################################
bool performNetPerfMeterIdentifyFlow(MessageReader*         messageReader,
                                     int                    controlSocket,
                                     const Flow*            flow,
                                     const OutputFileFormat vectorFileFormat)
{
   // ====== Sent NETPERFMETER_IDENTIFY_FLOW to remote node =================
   unsigned int maxTrials;
   int          timeout;
   if( (flow->getTrafficSpec().Protocol != IPPROTO_SCTP) &&
       (flow->getTrafficSpec().Protocol != IPPROTO_TCP)
#if defined(HAVE_MPTCP)
       && (flow->getTrafficSpec().Protocol != IPPROTO_MPTCP)
#endif
     ) {
      maxTrials = UNRELIABLE_IDENTIFY_MAX_TRIALS;
      timeout   = UNRELIABLE_IDENTIFY_TIMEOUT;
   }
   else {
      // SCTP and TCP are reliable transport protocols
      // => no retransmissions necessary!
      maxTrials = RELIABLE_IDENTIFY_MAX_TRIALS;
      timeout   = RELIABLE_IDENTIFY_TIMEOUT;
   }
   for(unsigned int trial = 1; trial <= maxTrials; trial++) {
      NetPerfMeterIdentifyMessage identifyMsg;
      identifyMsg.Header.Type   = NETPERFMETER_IDENTIFY_FLOW;
      identifyMsg.Header.Flags  = 0x00;
      identifyMsg.Header.Length = htobe16(sizeof(identifyMsg));
      identifyMsg.MagicNumber   = htobe64(NETPERFMETER_IDENTIFY_FLOW_MAGIC_NUMBER);
      identifyMsg.MeasurementID = htobe64(flow->getMeasurementID());
      identifyMsg.FlowID        = htobe32(flow->getFlowID());
      identifyMsg.StreamID      = htobe16(flow->getStreamID());

      // ====== Backwards compatibility flags for older passive instances ===
      if(vectorFileFormat == OFF_None) {
         identifyMsg.Header.Flags |= NPMIF_NO_VECTORS;
      }
      else if((vectorFileFormat == OFF_BZip2) || (vectorFileFormat != OFF_Plain)) {
         // Older passive peers only support BZip2 compression:
         identifyMsg.Header.Flags |= NPMIF_COMPRESS_VECTORS;
      }

      LOG_TRACE
      stdlog << formatText("<R3 sd={} trial={}/{}>\n", controlSocket, trial, maxTrials);
      LOG_END
#if defined(HAVE_SCTP)
      if(flow->getTrafficSpec().Protocol == IPPROTO_SCTP) {
         sctp_sndrcvinfo sinfo;
         memset(&sinfo, 0, sizeof(sinfo));
         sinfo.sinfo_stream = flow->getStreamID();
         sinfo.sinfo_ppid   = htobe32(PPID_NETPERFMETER_CONTROL);
         if(sctp_send(flow->getSocketDescriptor(), &identifyMsg, sizeof(identifyMsg), &sinfo, 0) <= 0) {
            return false;
         }
      }
#endif
#if defined(HAVE_QUIC)
      else if(flow->getTrafficSpec().Protocol == IPPROTO_QUIC) {
         const int64_t sid    = ((int64_t)flow->getStreamID() << 2) | QUIC_STREAM_TYPE_UNI_MASK;
         const uint32_t flags = MSG_QUIC_STREAM_NEW;
         if(quic_sendmsg(flow->getSocketDescriptor(), &identifyMsg, sizeof(identifyMsg), sid, flags) <= 0) {
            return false;
         }
      }
#endif
      else {
         if(ext_send(flow->getSocketDescriptor(), &identifyMsg, sizeof(identifyMsg), 0) <= 0) {
            return false;
         }
      }
      LOG_TRACE
      stdlog << formatText("<R4 sd={} trial={}/{}>\n", controlSocket, trial, maxTrials);
      LOG_END
      if(awaitNetPerfMeterAcknowledge(messageReader, controlSocket,
                                      flow->getMeasurementID(),
                                      flow->getFlowID(), flow->getStreamID(),
                                      timeout) == true) {
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
         // TRANSLATORS: Error message when config file cannot be created. Placeholders: {} 1: config file name, 2: strerror() message.
         stdlog << formatText(gettext("Unable to create config file {}: {}!"),
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
         fprintf(configFile, "FLOW%u_VECTOR_ACTIVE_NODE=\"%s\"\n",           flow->getFlowID(),
                              Flow::getNodeOutputName(vectorNamePattern, "active",
                                 formatText("-{:08x}-{:04x}", flow->getFlowID(), flow->getStreamID())).c_str());
         fprintf(configFile, "FLOW%u_VECTOR_PASSIVE_NODE=\"%s\"\n\n",        flow->getFlowID(),
                              Flow::getNodeOutputName(vectorNamePattern, "passive",
                                 formatText("-{:08x}-{:04x}", flow->getFlowID(), flow->getStreamID())).c_str());
      }

      FlowManager::getFlowManager()->unlock();

      fclose(configFile);
   }

   // ====== Start flows ====================================================
   const bool success = FlowManager::getFlowManager()->beginMeasurement(
                           controlSocket, measurementID, getMicroTime(),
                           vectorNamePattern, vectorFileFormat,
                           scalarNamePattern, scalarFileFormat);
   if(success) {
      // ====== Tell passive node to start measurement ======================
      NetPerfMeterStartMessage startMsg;
      startMsg.Header.Type   = NETPERFMETER_START;
      startMsg.Header.Length = htobe16(sizeof(startMsg));
      startMsg.Header.Flags  = 0x00;
      startMsg.VectorFormat  = static_cast<uint8_t>(vectorFileFormat);
      startMsg.ScalarFormat  = static_cast<uint8_t>(scalarFileFormat);
      startMsg.Padding       = 0x0000;
      startMsg.MeasurementID = htobe64(measurementID);

      // Backwards compatibility flags for older passive instances:
      if(scalarFileFormat == OFF_None) {
         startMsg.Header.Flags |= NPMSF_NO_SCALARS;
      }
      else if(scalarFileFormat != OFF_Plain) {
         startMsg.Header.Flags |= NPMSF_COMPRESS_SCALARS;
      }
      if(vectorFileFormat == OFF_None) {
         startMsg.Header.Flags |= NPMSF_NO_VECTORS;
      }
      else if(vectorFileFormat != OFF_Plain) {
         startMsg.Header.Flags |= NPMSF_COMPRESS_VECTORS;
      }

      LOG_INFO
      // TRANSLATORS: Status message when starting a measurement. Placeholders: {:x} 1: measurement ID (hex), {} 2: socket descriptor.
      stdlog << formatText(gettext("Starting measurement ${:x} on socket {} ..."),
                           measurementID, controlSocket) << "\n";
      LOG_END
      if(ext_send(controlSocket, &startMsg, sizeof(startMsg), 0) < 0) {
         return false;
      }
      LOG_TRACE
      stdlog << formatText("<wait sd={}>\n", controlSocket);
      LOG_END
      if(awaitNetPerfMeterAcknowledge(messageReader, controlSocket,
                                      measurementID, 0, 0) == false) {
         return false;
      }
      LOG_TRACE
      stdlog << formatText("<okay sd={}>\n", controlSocket);
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
   try {
      flow->getVectorFile().finish(true);
   }
   catch(const OutputFileException& ex) {
      LOG_ERROR
      stdlog << formatText(gettext("ERROR: Failed to finish vector file <{}>: {}!"),
                           flow->getVectorFile().getName(), ex.getReason()) << "\n";
      LOG_END
   }
   catch(...) {
   }

   NetPerfMeterRemoveFlowMessage removeFlowMsg;
   removeFlowMsg.Header.Type   = NETPERFMETER_REMOVE_FLOW;
   removeFlowMsg.Header.Flags  = 0x00;
   removeFlowMsg.Header.Length = htobe16(sizeof(removeFlowMsg));
   removeFlowMsg.MeasurementID = htobe64(flow->getMeasurementID());
   removeFlowMsg.FlowID        = htobe32(flow->getFlowID());
   removeFlowMsg.StreamID      = htobe16(flow->getStreamID());

   LOG_TRACE
   stdlog << formatText("<flow {}>\n", flow->getFlowID());
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
   std::vector<Flow*>::iterator iterator = FlowManager::getFlowManager()->getFlowSet().begin();
   while(iterator != FlowManager::getFlowManager()->getFlowSet().end()) {
      Flow* flow = *iterator;
      flow->deactivate(true);
      iterator++;
   }
   Measurement* measurement = FlowManager::getFlowManager()->findMeasurement(controlSocket,
                                                                             measurementID);
   assure(measurement != nullptr);
   measurement->writeScalarStatistics(getMicroTime());
   FlowManager::getFlowManager()->unlock();

   // ====== Tell passive node to stop measurement ==========================
   NetPerfMeterStopMessage stopMsg;
   stopMsg.Header.Type   = NETPERFMETER_STOP;
   stopMsg.Header.Flags  = 0x00;
   stopMsg.Header.Length = htobe16(sizeof(stopMsg));
   stopMsg.Padding       = 0x00000000;
   stopMsg.MeasurementID = htobe64(measurementID);

   LOG_INFO
   // TRANSLATORS: Status message when stopping a measurement. Placeholder: {} is socket descriptor.
   stdlog << formatText(gettext("Stopping measurement on socket {} ..."),
                        controlSocket) << "\n";
   LOG_END
   if(ext_send(controlSocket, &stopMsg, sizeof(stopMsg), 0) < 0) {
      return false;
   }
   LOG_TRACE
   stdlog << formatText("<wait sd={}>\n", controlSocket);
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
      // TRANSLATORS: Status message when downloading vector results. Placeholders: {} 1: file path, 2: socket descriptor.
      stdlog << formatText(gettext("Downloading vector results [{}] from socket {}"),
                           vectorName, controlSocket) << "\n";
      LOG_END
      if(downloadOutputFile(messageReader, controlSocket, vectorName.c_str()) == false) {
         delete measurement;
         return false;
      }
      LOG_TRACE
      stdlog << formatText("<okay sd={}>\n", controlSocket);
      LOG_END
   }

   // ====== Download passive node's scalar file ============================
   if(measurement->getScalarNamePattern() != "") {
      const std::string scalarName = Flow::getNodeOutputName(
         measurement->getScalarNamePattern(), "passive");
      LOG_INFO
      // TRANSLATORS: Status message when downloading scalar results. Placeholders: {} 1: file path, 2: socket descriptor.
      stdlog << formatText(gettext("Downloading scalar results [{}] from socket {}"),
                           scalarName, controlSocket) << "\n";
      LOG_END
      if(downloadOutputFile(messageReader, controlSocket, scalarName.c_str()) == false) {
         delete measurement;
         return false;
      }
      LOG_TRACE
      stdlog << formatText("<okay sd={}>\n", controlSocket);
      LOG_END
   }

   // ====== Download flow results and remove the flows =====================
   FlowManager::getFlowManager()->lock();

   for(std::vector<Flow*>::iterator iterator = FlowManager::getFlowManager()->getFlowSet().begin();
      iterator != FlowManager::getFlowManager()->getFlowSet().end();
      iterator++) {
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
      }
   }

   iterator = FlowManager::getFlowManager()->getFlowSet().begin();
   while(iterator != FlowManager::getFlowManager()->getFlowSet().end()) {
      Flow* flow = *iterator;
      if(flow->getMeasurementID() == measurementID) {
         flow->deactivate(false);
         delete flow;
         iterator = FlowManager::getFlowManager()->getFlowSet().begin();
         continue;
      }
      iterator++;
   }

   FlowManager::getFlowManager()->finishMeasurement(controlSocket, measurementID);

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
      stdlog << "<timeout>\n";
      LOG_END
      return false;
   }
   if(!(pfd.revents & (POLLIN|POLLERR))) {
      LOG_TRACE
      stdlog << "<no answer>\n";
      LOG_END
      return false;
   }

   // ====== Read NETPERFMETER_ACKNOWLEDGE message ==========================
   NetPerfMeterAcknowledgeMessage ackMsg;
   ssize_t                        received;
   do {
      received = messageReader->receiveMessage(controlSocket, &ackMsg, sizeof(ackMsg));
   } while(received == MRRM_PARTIAL_READ);
   if(received < (ssize_t)sizeof(ackMsg)) {
      return false;
   }
   if(ackMsg.Header.Type != NETPERFMETER_ACKNOWLEDGE) {
      LOG_WARNING
      // TRANSLATORS: Warning message when unexpected message type is received instead of ACK. Placeholders: {:02x} 1: hex message type, {} 2: socket descriptor.
      stdlog << formatText(gettext("Received message type ${:02x} instead of NETPERFMETER_ACKNOWLEDGE on socket {}!"),
                           static_cast<unsigned int>(ackMsg.Header.Type), controlSocket) << "\n";
      LOG_END
      return false;
   }

   // ====== Check whether NETPERFMETER_ACKNOWLEDGE is okay =================
   if( (be64toh(ackMsg.MeasurementID) != measurementID) ||
       (be32toh(ackMsg.FlowID) != flowID) ||
       (be16toh(ackMsg.StreamID) != streamID) ) {
      LOG_WARNING
      // TRANSLATORS: Warning message when ACK parameters do not match the expected flow/stream. Placeholder: {} is socket descriptor.
      stdlog << formatText(gettext("Received NETPERFMETER_ACKNOWLEDGE for wrong measurement/flow/stream on socket {}!"),
                           controlSocket) << "\n";
      LOG_END
      return false;
   }

   const uint32_t status = be32toh(ackMsg.Status);
   LOG_TRACE
   stdlog << formatText("<status={} sd={}>\n", status, controlSocket);
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
   LOG_DEBUG
   // TRANSLATORS: Log message when uploading measurement results. Placeholder: {} is socket descriptor.
   stdlog << formatText(gettext("Uploading results on socket {} ..."), controlSocket) << "\n";
   LOG_END

   bool success = true;
   do {
      // ====== Read chunk from file ========================================
      const size_t bytes = fread(&resultsMsg->Data, 1,
                                 NETPERFMETER_RESULTS_MAX_DATA_LENGTH,
                                 outputFile.getFile());
      resultsMsg->Header.Flags  = feof(outputFile.getFile()) ? NPMRF_EOF : 0x00;
      resultsMsg->Header.Length = htobe16(sizeof(NetPerfMeterResults) + bytes);
      if(ferror(outputFile.getFile())) {
         LOG_ERROR
         // TRANSLATORS: Error message when reading measurement output file fails. Placeholders: {} 1: file path, 2: strerror() message.
         stdlog << formatText(gettext("Unable to read results from {}: {}!"),
                              outputFile.getName(), strerror(errno)) << "\n";
         LOG_END
         success = false;
         break;
      }

      // ====== Transmit chunk ==============================================
      if(ext_send(controlSocket, resultsMsg, sizeof(NetPerfMeterResults) + bytes, 0) < 0) {
         LOG_ERROR
         // TRANSLATORS: Error message when uploading measurement results fails. Placeholders: {} 1: socket descriptor, 2: strerror() message.
         stdlog << formatText(gettext("Unable to upload results on socket {}: {}!"),
                              controlSocket, strerror(errno)) << "\n";
         LOG_END
         success = false;
         break;
      }
   } while(!(resultsMsg->Header.Flags & NPMRF_EOF));

   // ====== Check results ==================================================
   if(!success) {
      LOG_WARNING
      // TRANSLATORS: Warning message when uploading results fails. Placeholders: {} 1: socket descriptor, 2: strerror() message.
      stdlog << formatText(gettext("Unable to upload results on socket {} (errno={})!"),
                           controlSocket, strerror(errno)) << "\n";
      LOG_END
      ext_shutdown(controlSocket, SHUT_RDWR);
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
   bool success = false;
   try {
      flow->getVectorFile().finish(false);
      success = true;
   }
   catch(const OutputFileException& ex) {
      LOG_ERROR
      stdlog << formatText(gettext("ERROR: Failed to finish vector file <{}>: {}!"),
                           flow->getVectorFile().getName(), ex.getReason()) << "\n";
      LOG_END
      success = false;
   }
   catch(...) {
      success = false;
   }

   if(success) {
      success = sendNetPerfMeterAcknowledge(
                   controlSocket,
                   flow->getMeasurementID(), flow->getFlowID(), flow->getStreamID(),
                   NETPERFMETER_STATUS_OKAY);
      if(success) {
         if(flow->getVectorFile().exists()) {
            success = uploadOutputFile(controlSocket, flow->getVectorFile());
         }
      }
   }
   else {
      sendNetPerfMeterAcknowledge(
         controlSocket,
         flow->getMeasurementID(), flow->getFlowID(), flow->getStreamID(),
         NETPERFMETER_STATUS_ERROR);
   }

   try {
      flow->getVectorFile().finish(true);
   }
   catch(...) {
   }
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
      // TRANSLATORS: Warning message when incoming ADD_FLOW control message is too short. Placeholder: {} is socket descriptor.
      stdlog << formatText(gettext("Received malformed NETPERFMETER_ADD_FLOW control message on socket {}!"),
                           controlSocket) << "\n";
      LOG_END
      ext_shutdown(controlSocket, SHUT_RDWR);
      return false;
   }
   const uint64_t measurementID   = be64toh(addFlowMsg->MeasurementID);
   const uint32_t flowID          = be32toh(addFlowMsg->FlowID);
   const uint16_t streamID        = be16toh(addFlowMsg->StreamID);
   const size_t   startStopEvents = be16toh(addFlowMsg->OnOffEvents);
   if(received < sizeof(NetPerfMeterAddFlowMessage) + (startStopEvents * sizeof(NetPerfMeterOnOffEvent))) {
      LOG_WARNING
      // TRANSLATORS: Warning message when ADD_FLOW control message has fewer on/off entries than specified. Placeholder: {} is socket descriptor.
      stdlog << formatText(gettext("Too few start/stop entries in NETPERFMETER_ADD_FLOW control message on socket {}!"),
                           controlSocket) << "\n";
      LOG_END
      ext_shutdown(controlSocket, SHUT_RDWR);
      return false;
   }
   char description[sizeof(addFlowMsg->Description) + 1];
   memcpy((char*)&description, (const char*)&addFlowMsg->Description, sizeof(addFlowMsg->Description));
   description[sizeof(addFlowMsg->Description)] = 0x00;
   if(FlowManager::getFlowManager()->findFlow(measurementID, flowID, streamID)) {
      LOG_WARNING
      // TRANSLATORS: Warning message when ADD_FLOW tries to add a flow that already exists. Placeholder: {} is socket descriptor.
      stdlog << formatText(gettext("NETPERFMETER_ADD_FLOW tried to add already-existing flow on socket {}!"),
                           controlSocket) << "\n";
      LOG_END
      return(sendNetPerfMeterAcknowledge(controlSocket,
                                         measurementID, flowID, streamID,
                                         NETPERFMETER_STATUS_ERROR));
   }
   else {
      // ====== Create new flow =============================================
      FlowTrafficSpec trafficSpec;
      trafficSpec.Protocol    = le16toh(addFlowMsg->Protocol);
      trafficSpec.Description = std::string(description);
      for(size_t i = 0;i < NETPERFMETER_RNG_INPUT_PARAMETERS;i++) {
         trafficSpec.OutboundFrameRate[i] = networkToDouble(addFlowMsg->FrameRate[i]);
         trafficSpec.OutboundFrameSize[i] = networkToDouble(addFlowMsg->FrameSize[i]);
      }
      trafficSpec.ErrorOnAbort             = false;
      trafficSpec.OutboundFrameRateRng     = addFlowMsg->FrameRateRng;
      trafficSpec.OutboundFrameSizeRng     = addFlowMsg->FrameSizeRng;
      trafficSpec.MaxMsgSize               = be16toh(addFlowMsg->MaxMsgSize);
      trafficSpec.RcvBufferSize            = be32toh(addFlowMsg->RcvBufferSize);
      trafficSpec.SndBufferSize            = be32toh(addFlowMsg->SndBufferSize);
      trafficSpec.OrderedMode              = be32toh(addFlowMsg->OrderedMode)  / (double)0xffffffff;
      trafficSpec.ReliableMode             = be32toh(addFlowMsg->ReliableMode) / (double)0xffffffff;
      trafficSpec.CMT                      = addFlowMsg->CMT;
      trafficSpec.CCID                     = addFlowMsg->CCID;
      trafficSpec.NoDelay                  = (addFlowMsg->Header.Flags & NPMAFF_NODELAY);
      trafficSpec.Debug                    = (addFlowMsg->Header.Flags & NPMAFF_DEBUG);
      trafficSpec.RepeatOnOff              = (addFlowMsg->Header.Flags & NPMAFF_REPEATONOFF);
      trafficSpec.RetransmissionTrials     = be32toh(addFlowMsg->RetransmissionTrials) & ~NPMAF_RTX_TRIALS_IN_MILLISECONDS;
      trafficSpec.RetransmissionTrialsInMS = (be32toh(addFlowMsg->RetransmissionTrials) & NPMAF_RTX_TRIALS_IN_MILLISECONDS);
      if( (trafficSpec.RetransmissionTrialsInMS) && (trafficSpec.RetransmissionTrials == NPMAF_RTX_DEFAULT) ) {
         trafficSpec.RetransmissionTrials = ~0U;
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

      Flow* flow = new Flow(be64toh(addFlowMsg->MeasurementID), be32toh(addFlowMsg->FlowID),
                            be16toh(addFlowMsg->StreamID), trafficSpec,
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
      // TRANSLATORS: Warning message when incoming REMOVE_FLOW control message is too short. Placeholder: {} is socket descriptor.
      stdlog << formatText(gettext("Received malformed NETPERFMETER_REMOVE_FLOW control message on socket {}!"),
                           controlSocket) << "\n";
      LOG_END
      ext_shutdown(controlSocket, SHUT_RDWR);
      return false;
   }
   const uint64_t measurementID = be64toh(removeFlowMsg->MeasurementID);
   const uint32_t flowID        = be32toh(removeFlowMsg->FlowID);
   const uint16_t streamID      = be16toh(removeFlowMsg->StreamID);
   Flow* flow = FlowManager::getFlowManager()->findFlow(measurementID, flowID, streamID);
   if(flow == nullptr) {
      LOG_WARNING
      // TRANSLATORS: Warning message when REMOVE_FLOW references a non-existent flow. Placeholder: {} is socket descriptor.
      stdlog << formatText(gettext("NETPERFMETER_REMOVE_FLOW tried to remove not-existing flow on socket {}!"),
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
      try {
         flow->getVectorFile().finish(false);
      }
      catch(const OutputFileException& ex) {
         LOG_ERROR
         stdlog << formatText(gettext("ERROR: Failed to finish vector file <{}>: {}!"),
                              flow->getVectorFile().getName(), ex.getReason()) << "\n";
         LOG_END
      }
      catch(...) {
      }
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
      // TRANSLATORS: Warning message when incoming START control message is too short. Placeholder: {} is socket descriptor.
      stdlog << formatText(gettext("Received malformed NETPERFMETER_START control message on socket {}!"),
                           controlSocket) << "\n";
      LOG_END
      ext_shutdown(controlSocket, SHUT_RDWR);
      return false;
   }
   const uint64_t measurementID = be64toh(startMsg->MeasurementID);
   LOG_INFO
   // TRANSLATORS: Status message when remote peer requests measurement start. Placeholders: {:x} 1: measurement ID (hex), {} 2: socket descriptor.
   stdlog << formatText(gettext("Starting measurement ${:x} on socket {} ..."),
                        measurementID, controlSocket) << "\n";
   LOG_END

   // ====== Determine scalar format ======
   OutputFileFormat scalarFileFormat = static_cast<OutputFileFormat>(startMsg->ScalarFormat);
   if(scalarFileFormat == OFF_None) {
      if(startMsg->Header.Flags & NPMSF_NO_SCALARS) {
         scalarFileFormat = OFF_None;
      }
      else if(startMsg->Header.Flags & NPMSF_COMPRESS_SCALARS) {
         scalarFileFormat = OFF_BZip2;
      }
      else {
         scalarFileFormat = OFF_Plain;
      }
   }

   // ====== Determine vector format ======
   OutputFileFormat vectorFileFormat = static_cast<OutputFileFormat>(startMsg->VectorFormat);
   if(vectorFileFormat == OFF_None) {
      if(startMsg->Header.Flags & NPMSF_NO_VECTORS) {
         vectorFileFormat = OFF_None;
      }
      else if(startMsg->Header.Flags & NPMSF_COMPRESS_VECTORS) {
         vectorFileFormat = OFF_BZip2;
      }
      else {
         vectorFileFormat = OFF_Plain;
      }
   }

   const uint64_t now = getMicroTime();
   bool success = FlowManager::getFlowManager()->beginMeasurement(
      controlSocket, measurementID, now,
      nullptr, vectorFileFormat,
      nullptr, scalarFileFormat);

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
      // TRANSLATORS: Warning message when incoming STOP control message is too short. Placeholder: {} is socket descriptor.
      stdlog << formatText(gettext("Received malformed NETPERFMETER_STOP control message on socket {}!"),
                           controlSocket) << "\n";
      LOG_END
      ext_shutdown(controlSocket, SHUT_RDWR);
      return false;
   }
   const uint64_t measurementID = be64toh(stopMsg->MeasurementID);
   LOG_INFO
   // TRANSLATORS: Status message when remote peer requests measurement stop. Placeholders: {:x} 1: measurement ID (hex), {} 2: socket descriptor.
   stdlog << formatText(gettext("Stopping measurement ${:x} on socket {} ..."),
                        measurementID, controlSocket) << "\n";
   LOG_END

   // ====== Stop flows =====================================================
   FlowManager::getFlowManager()->lock();

   bool         success     = false;
   Measurement* measurement =
      FlowManager::getFlowManager()->findMeasurement(controlSocket, measurementID);
   if(measurement) {
      measurement->lock();
      measurement->writeScalarStatistics(getMicroTime());
      try {
         measurement->finish(false);
         success = true;
      }
      catch(const OutputFileException& ex) {
         LOG_ERROR
         stdlog << formatText(gettext("ERROR: Failed to finish measurement output files: {}!"),
                              ex.getReason()) << "\n";
         LOG_END
         success = false;
      }
      catch(...) {
         success = false;
      }
      measurement->unlock();
   }

   // ====== Remove pointer to Measurement from its flows ===================
   for(std::vector<Flow*>::iterator iterator = FlowManager::getFlowManager()->getFlowSet().begin();
      iterator != FlowManager::getFlowManager()->getFlowSet().end();
      iterator++) {
      Flow* flow = *iterator;
      if(flow->getMeasurement() == measurement) {
         flow->setMeasurement(nullptr);
         flow->deactivate(true);
      }
   }
   FlowManager::getFlowManager()->finishMeasurement(controlSocket, measurementID);
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

   // ====== Read message (or fragment) =====================================
   const ssize_t received =
      messageReader->receiveMessage(controlSocket, &inputBuffer, sizeof(inputBuffer),
                                    &from.sa, &fromlen, nullptr, &flags);
   if(received == MRRM_PARTIAL_READ) {
      return true;   // Partial read -> wait for next fragment.
   }
   else if(received < (ssize_t)sizeof(NetPerfMeterHeader)) {
      if(received < 0) {
         LOG_WARNING
         // TRANSLATORS: Warning message when control connection socket encounters read error. Placeholder: {} is socket descriptor.
         stdlog << formatText(gettext("Control connection is broken on socket {}!"),
                              controlSocket) << "\n";
         LOG_END
         ext_shutdown(controlSocket, SHUT_RDWR);
      }
      return false;
   }

   // ====== Received a SCTP notification ===================================
#if defined(HAVE_SCTP)
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
#endif
      const NetPerfMeterHeader* header = (const NetPerfMeterHeader*)&inputBuffer;
      if(be16toh(header->Length) != received) {
         LOG_WARNING
         // TRANSLATORS: Warning message when control message length does not match payload length received. Placeholders: {} 1: socket descriptor, 2: expected length, 3: received length.
         stdlog << formatText(gettext("Received malformed control message on socket {}: expected length {}, but received length {}!"),
                              controlSocket, static_cast<unsigned int>(be16toh(header->Length)), static_cast<unsigned int>(received)) << "\n";
         LOG_END
         ext_shutdown(controlSocket, SHUT_RDWR);
         return false;
      }

      switch(header->Type) {
         case NETPERFMETER_ADD_FLOW:
            return handleNetPerfMeterAddFlow(
                      messageReader, controlSocket,
                      (const NetPerfMeterAddFlowMessage*)&inputBuffer, (size_t)received);
         case NETPERFMETER_REMOVE_FLOW:
            return handleNetPerfMeterRemoveFlow(
                      messageReader, controlSocket,
                      (const NetPerfMeterRemoveFlowMessage*)&inputBuffer, (size_t)received);
         case NETPERFMETER_START:
            return handleNetPerfMeterStart(
                      messageReader, controlSocket,
                      (const NetPerfMeterStartMessage*)&inputBuffer, (size_t)received);
         case NETPERFMETER_STOP:
            return handleNetPerfMeterStop(
                      messageReader, controlSocket,
                      (const NetPerfMeterStopMessage*)&inputBuffer, (size_t)received);
         default:
            LOG_WARNING
            // TRANSLATORS: Warning message when unrecognized control message type is received. Placeholders: {:02x} 1: hex message type, {} 2: socket descriptor.
            stdlog << formatText(gettext("Received invalid control message of type ${:02x} on socket {}!"),
                                 static_cast<unsigned int>(header->Type), controlSocket) << "\n";
            LOG_END
            ext_shutdown(controlSocket, SHUT_RDWR);
            return false;
      }
#if defined(HAVE_SCTP)
   }
#endif
   return true;
}


// ###### Handle NETPERFMETER_IDENTIFY_FLOW message #########################
bool handleNetPerfMeterIdentify(const NetPerfMeterIdentifyMessage* identifyMsg,
                                const int                          sd,
                                const sockaddr_union*              from)
{
   bool  isAlreadyInitialised;
   Flow* flow = FlowManager::getFlowManager()->identifySocket(
                   be64toh(identifyMsg->MeasurementID),
                   be32toh(identifyMsg->FlowID), be16toh(identifyMsg->StreamID),
                   sd, from, isAlreadyInitialised);
   if(flow != nullptr) {
      if(!isAlreadyInitialised) {
         flow->lock();
         const int  controlSocketDescriptor = flow->getControlSocketDescriptor();
         const bool socketConfigured        = flow->configureSocket(sd);
         flow->unlock();
         sendNetPerfMeterAcknowledge(controlSocketDescriptor,
                                     be64toh(identifyMsg->MeasurementID),
                                     be32toh(identifyMsg->FlowID),
                                     be16toh(identifyMsg->StreamID),
                                     (socketConfigured == true) ? NETPERFMETER_STATUS_OKAY :
                                                                  NETPERFMETER_STATUS_ERROR);
      }
      return true;
   }
   else {
      // No known flow -> no association to send response to!
      LOG_WARNING
      // TRANSLATORS: Warning message when IDENTIFY_FLOW message references an unknown flow. Placeholder: {} is socket descriptor.
      stdlog << formatText(gettext("NETPERFMETER_IDENTIFY_FLOW message for unknown flow on socket {}!"),
                           sd) << "\n";
      LOG_END
      return false;
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
   ackMsg.Header.Length = htobe16(sizeof(ackMsg));
   ackMsg.MeasurementID = htobe64(measurementID);
   ackMsg.FlowID        = htobe32(flowID);
   ackMsg.StreamID      = htobe16(streamID);
   ackMsg.Padding       = 0x0000;
   ackMsg.Status        = htobe32(status);

   return ext_send(controlSocket, &ackMsg, sizeof(ackMsg), 0) > 0;
}
