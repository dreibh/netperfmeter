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

#include "flow.h"
#include "control.h"
#include "loglevel.h"
#include "transfer.h"

#include <assert.h>
#include <poll.h>
#include <signal.h>
#include <netinet/tcp.h>

#include <cstring>
#include <set>
#include <sstream>


// #define DEBUG_POLL


// Flow Manager Singleton object
FlowManager FlowManager::FlowManagerSingleton;


// ###### Constructor #######################################################
FlowManager::FlowManager()
{
   DisplayOn         = false;
   DisplayInterval   = 1000000;
   FirstDisplayEvent = 0;
   LastDisplayEvent  = 0;
   NextDisplayEvent  = 0;
   start();
}


// ###### Destructor ########################################################
FlowManager::~FlowManager()
{
   stop();
   waitForFinish();
}


// ###### Add flow ##########################################################
void FlowManager::addFlow(Flow* flow)
{
   lock();
   flow->PollFDEntry = nullptr;
   FlowSet.push_back(flow);
   unlock();
}


// ###### Remove flow #######################################################
void FlowManager::removeFlow(Flow* flow)
{
   lock();
   flow->deactivate();

   // ====== Remove flow from flow set ======================================
   flow->PollFDEntry = nullptr;
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
       if(*iterator == flow) {
          FlowSet.erase(iterator);
         break;
      }
   }

   // ====== Check whether the socket descriptor is still referenced ========
   flow->OriginalSocketDescriptor = true;
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
       if((*iterator)->SocketDescriptor == flow->SocketDescriptor) {
          flow->OriginalSocketDescriptor = false;
          break;
       }
   }

   unlock();
}


// ###### Print all flows ###################################################
void FlowManager::printFlows(std::ostream& os,
                             const bool    printStatistics)
{
   lock();
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
      Flow* flow= *iterator;
      flow->print(os, printStatistics);
   }
   unlock();
}


// ###### Find Flow by Measurement ID/Flow ID/Stream ID #####################
Flow* FlowManager::findFlow(const uint64_t measurementID,
                            const uint32_t flowID,
                            const uint16_t streamID)
{
   Flow* found = nullptr;

   lock();
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
      Flow* flow = *iterator;
      if( (flow->MeasurementID == measurementID) &&
          (flow->FlowID == flowID) &&
          (flow->StreamID == streamID) ) {
         found = flow;
         break;
      }
   }
   unlock();

   return found;
}


// ###### Find Flow by socket descriptor and Stream ID ######################
Flow* FlowManager::findFlow(const int      socketDescriptor,
                            const uint16_t streamID)
{
   Flow* found = nullptr;

   lock();
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
      Flow* flow = *iterator;
      if(flow->SocketDescriptor == socketDescriptor) {
         if(flow->StreamID == streamID) {
            found = flow;
            break;
         }
      }
   }
   unlock();

   return found;
}


// ###### Find Flow by source address #######################################
Flow* FlowManager::findFlow(const struct sockaddr* from)
{
   Flow* found = nullptr;

   lock();
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
      Flow* flow = *iterator;
      if( (flow->RemoteAddressIsValid) &&
          (addresscmp(from, &flow->RemoteAddress.sa, true) == 0) ) {
         found = flow;
         break;
      }
   }
   unlock();

   return found;
}


// ###### Start measurement #################################################
bool FlowManager::startMeasurement(const int                controlSocket,
                                   const uint64_t           measurementID,
                                   const unsigned long long now,
                                   const char*              vectorNamePattern,
                                   const OutputFileFormat   vectorFileFormat,
                                   const char*              scalarNamePattern,
                                   const OutputFileFormat   scalarFileFormat)
{
   std::stringstream ss;
   bool              success = false;

   lock();
   Measurement* measurement = new Measurement;
   if(measurement != nullptr) {
      if(measurement->initialize(now, controlSocket, measurementID,
                                 vectorNamePattern, vectorFileFormat,
                                 scalarNamePattern, scalarFileFormat)) {
         success = true;
         for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
            iterator != FlowSet.end();iterator++) {
            Flow* flow = *iterator;
            if(flow->MeasurementID == measurementID) {
               if(flow->setMeasurement(measurement)) {
                  if(flow->SocketDescriptor >= 0) {
                     flow->TimeBase     = now;
                     flow->TimeOffset   = 0;
                     flow->InputStatus  = Flow::On;
                     flow->OutputStatus = (flow->TrafficSpec.OnOffEvents.size() > 0) ?
                                             Flow::Off : Flow::On;
                     flow->print(ss);
                     flow->activate();
                  }
               }
               else {
                  LOG_WARNING
                  stdlog << format("Flow associated with Measurement $%llx on socket %d is already owned by another measurement!",
                                   measurementID, controlSocket) << "\n";
                  LOG_END
               }
            }
         }
      }
      else {
         delete measurement;
      }
   }
   unlock();
   CPULoadStats.update();

   LOG_INFO
   stdlog << ss.str();
   LOG_END
   return success;
}


// ###### Stop measurement ##################################################
void FlowManager::stopMeasurement(const int                controlSocket,
                                  const uint64_t           measurementID,
                                  const unsigned long long now)
{
   std::stringstream ss;

   CPULoadStats.update();
   lock();

   // We make a two-staged stopping process here:
   // In stage 0, the flows' sender threads are told to stop.
   //   => all threads can perform shutdown simultaneously
   //   => much faster if there are many flows
   // In stage 1, we wait until the threads have stopped.
   for(unsigned int stage = 0;stage < 2;stage++) {
      for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
         iterator != FlowSet.end();iterator++) {
         Flow* flow = *iterator;
         if(flow->MeasurementID == measurementID) {
            // ====== Stop flow ================================================
            if(stage == 0) {
               flow->deactivate(true);
            }
            else {
               flow->deactivate(false);
               flow->print(ss);
            }
         }
      }
   }
   unlock();

   LOG_INFO
   stdlog << ss.str();
   LOG_END
}


// ###### Add measurement ###################################################
bool FlowManager::addMeasurement(const int    controlSocket,
                                 Measurement* measurement)
{
   bool success = false;

   lock();
   if(!findMeasurement(controlSocket, measurement->MeasurementID)) {
      MeasurementSet.insert(std::pair<std::pair<int, uint64_t>, Measurement*>(
         std::pair<int, uint64_t>(controlSocket, measurement->MeasurementID),
         measurement));
      success = true;
   }
   unlock();

   return success;
}


// ###### Find measurement ##################################################
Measurement* FlowManager::findMeasurement(const int      controlSocket,
                                          const uint64_t measurementID)
{
   std::map<std::pair<int, uint64_t>, Measurement*>::iterator found = MeasurementSet.find(
      std::pair<int, uint64_t>(controlSocket, measurementID));
   if(found != MeasurementSet.end()) {
      return found->second;
   }
   return nullptr;
}


// ###### Remove measurement ################################################
void FlowManager::removeMeasurement(const int    controlSocket,
                                    Measurement* measurement)
{
   lock();
   MeasurementSet.erase(std::pair<int, uint64_t>(controlSocket, measurement->MeasurementID));
   unlock();
}


// ###### Remove all measurements belonging to control socket ###############
void FlowManager::removeAllMeasurements(const int controlSocket)
{
   lock();

   std::vector<Flow*>::iterator flowIterator = FlowSet.begin();
   while(flowIterator != FlowSet.end()) {
      Flow* flow = *flowIterator;
      if(flow->getRemoteControlSocketDescriptor() == controlSocket) {
         flowIterator = FlowSet.erase(flowIterator);
         delete flow;
      }
      else {
         flowIterator++;
      }
   }

   std::map<std::pair<int, uint64_t>, Measurement*>::iterator measurementIterator = MeasurementSet.begin();
   while(measurementIterator != MeasurementSet.end()) {
      Measurement* measurement = measurementIterator->second;
      if(measurement->ControlSocketDescriptor == controlSocket) {
         measurementIterator = MeasurementSet.erase(measurementIterator);
         delete measurement;
      }
      else {
         measurementIterator++;
      }
   }

   unlock();
}


// ###### Print measurements ################################################
void FlowManager::printMeasurements(std::ostream& os)
{
   os << "Measurements:\n";
   for(std::map<std::pair<int, uint64_t>, Measurement*>::iterator iterator = MeasurementSet.begin();
       iterator != MeasurementSet.end(); iterator++) {
      char str[64];
      snprintf((char*)&str, sizeof(str), "%llx -> ptr=%p",
               (unsigned long long)iterator->first.second, iterator->second);
      os << "   - " << str << "\n";
   }
}


// ###### Add socket to flow manager ########################################
void FlowManager::addSocket(const int protocol, const int socketDescriptor)
{
   UnidentifiedSocket* us = new UnidentifiedSocket;
   us->SocketDescriptor = socketDescriptor;
   us->Protocol         = protocol;
   us->PollFDEntry      = nullptr;

   lock();
   Reader.registerSocket(protocol, socketDescriptor);
   UnidentifiedSockets.insert(std::pair<int, UnidentifiedSocket*>(socketDescriptor, us));
   UpdatedUnidentifiedSockets = true;
   unlock();
}


// ###### Remove socket #####################################################
void FlowManager::removeSocket(const int  socketDescriptor,
                               const bool closeSocket)
{
   lock();
   std::map<int, UnidentifiedSocket*>::iterator found =
      UnidentifiedSockets.find(socketDescriptor);
   if(found != UnidentifiedSockets.end()) {
      UnidentifiedSocket* us = found->second;
      UnidentifiedSockets.erase(found);
      delete us;
      UpdatedUnidentifiedSockets = true;
   }
   if(closeSocket) {
      Reader.deregisterSocket(socketDescriptor);
      ext_close(socketDescriptor);
   }
   unlock();
}


// ###### Map socket to flow ################################################
Flow* FlowManager::identifySocket(const uint64_t         measurementID,
                                  const uint32_t         flowID,
                                  const uint16_t         streamID,
                                  const int              socketDescriptor,
                                  const sockaddr_union*  from,
                                  const OutputFileFormat vectorFileFormat,
                                  int&                   controlSocketDescriptor)
{
   bool success            = false;
   controlSocketDescriptor = -1;

   lock();
   Flow* flow = findFlow(measurementID, flowID, streamID);
   if( (flow != nullptr) && (flow->RemoteAddressIsValid == false) ) {
      flow->lock();
      flow->setSocketDescriptor(socketDescriptor, false,
                                (flow->getTrafficSpec().Protocol != IPPROTO_UDP));
      flow->RemoteAddress        = *from;
      flow->RemoteAddressIsValid = (from->sa.sa_family != AF_UNSPEC);
      controlSocketDescriptor    = flow->RemoteControlSocketDescriptor;
      success = flow->initializeVectorFile(nullptr, vectorFileFormat);
      flow->unlock();
      if(flow->getTrafficSpec().Protocol != IPPROTO_UDP) {
         removeSocket(socketDescriptor, false);   // Socket is now managed as flow!
      }
   }
   unlock();

   if(!success) {
      flow = nullptr;
   }

   return flow;
}


// ###### Get next statistics or display events #############################
unsigned long long FlowManager::getNextEvent()
{
   unsigned long long nextEvent = NextDisplayEvent;

   lock();
   for(std::map<std::pair<int, uint64_t>, Measurement*>::iterator iterator = MeasurementSet.begin();
       iterator != MeasurementSet.end(); iterator++) {
       const Measurement* measurement = iterator->second;
       nextEvent = std::min(nextEvent, measurement->NextStatisticsEvent);
   }
   unlock();

   return nextEvent;
}


// ###### Handle statistics and display events ##############################
void FlowManager::handleEvents(const unsigned long long now)
{
   lock();

   // ====== Handle statistics events =======================================
   for(std::map<std::pair<int, uint64_t>, Measurement*>::iterator iterator = MeasurementSet.begin();
       iterator != MeasurementSet.end(); iterator++) {
       Measurement* measurement = iterator->second;
       if(measurement->NextStatisticsEvent <= now) {
          measurement->writeVectorStatistics(now, GlobalStats, RelGlobalStats);
       }
   }

   // ====== Display status =================================================
   if(NextDisplayEvent <= now) {
      // ====== Timer management ============================================
      NextDisplayEvent += DisplayInterval;
      if(NextDisplayEvent <= now) {   // Initialization!
         NextDisplayEvent = now + DisplayInterval;
      }
      if(FirstDisplayEvent == 0) {
         FirstDisplayEvent = now;
         LastDisplayEvent  = now;
      }

      // ====== Print bandwidth/CPU information line ========================
      if(DisplayOn) {
         static const unsigned int cpuDisplayLimit = 4;
         static const char* colorOff         = "\x1b[0m";
         static const char* colorDuration    = "\x1b[34m";
         static const char* colorFlows       = "\x1b[32m";
         static const char* colorTransmitted = "\x1b[33m";
         static const char* colorReceived    = "\x1b[36m";
         static const char* colorCPU         = "\x1b[0m\x1b[31m";
         static const char* colorCPUHighLoad = "\x1b[31;47;21m";
         const double       duration         = (now - LastDisplayEvent) / 1000000.0;
         const unsigned int totalDuration    = (unsigned int)((now - FirstDisplayEvent) / 1000000ULL);

         char cpuUtilization[160];
         cpuUtilization[0] = 0x00;
         CPUDisplayStats.update();
         if(CPUDisplayStats.getNumberOfCPUs() <= cpuDisplayLimit) {
            for(unsigned int i = 1; i <= CPUDisplayStats.getNumberOfCPUs(); i++) {
               const unsigned int load = (unsigned int)rint(10 * CPUDisplayStats.getCpuUtilization(i));
               char str[48];
               snprintf((char*)&str, sizeof(str), "%s%s%3u.%u%%%s",
                        (i > 1) ? "/" : "",
                        (load < 900) ? colorCPU : colorCPUHighLoad,
                        load / 10, load % 10,
                        colorCPU);
               safestrcat((char*)&cpuUtilization, str, sizeof(cpuUtilization));
            }
         }
         const unsigned int load = (unsigned int)rint(10 * CPUDisplayStats.getCpuUtilization(0));
         char str[48];
         snprintf((char*)&str, sizeof(str), "%s%s%3u.%u%%%s%s",
                  (CPUDisplayStats.getNumberOfCPUs() > cpuDisplayLimit) ? "" : " [",
                  (load < 900) ? colorCPU : colorCPUHighLoad,
                  load / 10, load % 10,
                  colorCPU,
                  (CPUDisplayStats.getNumberOfCPUs() > cpuDisplayLimit) ? " total" : "]");
         safestrcat((char*)&cpuUtilization, str, sizeof(cpuUtilization));

         fprintf(stdout,
                 "\r<-- %sDuration: %2u:%02u:%02u   "
                 "%sFlows: %u   "
                 "%sTransmitted: %1.2f MiB at %1.1f Kbit/s   "
                 "%sReceived: %1.2f MiB at %1.1f Kbit/s   "
                 "%sCPU: %s "
                 "%s-->\x1b[0K",
                 colorDuration,
                 totalDuration / 3600,
                 (totalDuration / 60) % 60,
                 totalDuration % 60,
                 colorFlows,
                 (unsigned int)FlowSet.size(),
                 colorTransmitted,
                 GlobalStats.TransmittedBytes / (1024.0 * 1024.0),
                 (duration > 0.0) ? (8 * RelGlobalStats.TransmittedBytes / (1000.0 * duration)) : 0.0,
                 colorReceived,
                 GlobalStats.ReceivedBytes / (1024.0 * 1024.0),
                 (duration > 0.0) ? (8 * RelGlobalStats.ReceivedBytes / (1000.0 * duration)) : 0.0,
                 colorCPU,
                 cpuUtilization,
                 colorOff);
         fflush(stdout);
      }

      LastDisplayEvent = now;
      RelGlobalStats.reset();
   }

   unlock();
}


// ###### Write scalars #####################################################
void FlowManager::writeScalarStatistics(const uint64_t           measurementID,
                                        const unsigned long long now,
                                        OutputFile&              scalarFile,
                                        const unsigned long long firstStatisticsEvent)
{
   std::string objectName =
      std::string("netPerfMeter.") +
         ((scalarFile.getName() == std::string()) ? "passive" : "active");

   lock();
   FlowBandwidthStats totalBandwidthStats;
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
      iterator != FlowSet.end();iterator++) {
      Flow* flow = *iterator;
      flow->lock();
      if(flow->MeasurementID == measurementID) {
         const double transmissionDuration = (flow->LastTransmission - flow->FirstTransmission) / 1000000.0;
         const double receptionDuration    = (flow->LastReception - flow->FirstReception) / 1000000.0;
         scalarFile.printf(
            "scalar \"%s.flow[%u]\" \"First Transmission\"      %llu\n"
            "scalar \"%s.flow[%u]\" \"Last Transmission\"       %llu\n"
            "scalar \"%s.flow[%u]\" \"First Reception\"         %llu\n"
            "scalar \"%s.flow[%u]\" \"Last Reception\"          %llu\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Bytes\"       %llu\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Packets\"     %llu\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Frames\"      %llu\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Bit Rate\"    %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Byte Rate\"   %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Packet Rate\" %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Frame Rate\"  %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Received Bytes\"          %llu\n"
            "scalar \"%s.flow[%u]\" \"Received Packets\"        %llu\n"
            "scalar \"%s.flow[%u]\" \"Received Frames\"         %llu\n"
            "scalar \"%s.flow[%u]\" \"Received Bit Rate\"       %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Received Byte Rate\"      %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Received Packet Rate\"    %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Received Frame Rate\"     %1.6f\n"
            ,
            objectName.c_str(), flow->FlowID, flow->FirstTransmission,
            objectName.c_str(), flow->FlowID, flow->LastTransmission,
            objectName.c_str(), flow->FlowID, flow->FirstReception,
            objectName.c_str(), flow->FlowID, flow->LastReception,

            objectName.c_str(), flow->FlowID, flow->CurrentBandwidthStats.TransmittedBytes,
            objectName.c_str(), flow->FlowID, flow->CurrentBandwidthStats.TransmittedPackets,
            objectName.c_str(), flow->FlowID, flow->CurrentBandwidthStats.TransmittedFrames,
            objectName.c_str(), flow->FlowID, (transmissionDuration > 0.0) ? 8ULL * flow->CurrentBandwidthStats.TransmittedBytes / transmissionDuration : 0.0,
            objectName.c_str(), flow->FlowID, (transmissionDuration > 0.0) ? flow->CurrentBandwidthStats.TransmittedBytes   / transmissionDuration : 0.0,
            objectName.c_str(), flow->FlowID, (transmissionDuration > 0.0) ? flow->CurrentBandwidthStats.TransmittedPackets / transmissionDuration : 0.0,
            objectName.c_str(), flow->FlowID, (transmissionDuration > 0.0) ? flow->CurrentBandwidthStats.TransmittedFrames  / transmissionDuration : 0.0,

            objectName.c_str(), flow->FlowID, flow->CurrentBandwidthStats.ReceivedBytes,
            objectName.c_str(), flow->FlowID, flow->CurrentBandwidthStats.ReceivedPackets,
            objectName.c_str(), flow->FlowID, flow->CurrentBandwidthStats.ReceivedFrames,
            objectName.c_str(), flow->FlowID, (receptionDuration > 0.0) ? 8ULL * flow->CurrentBandwidthStats.ReceivedBytes / receptionDuration : 0.0,
            objectName.c_str(), flow->FlowID, (receptionDuration > 0.0) ? flow->CurrentBandwidthStats.ReceivedBytes   / receptionDuration : 0.0,
            objectName.c_str(), flow->FlowID, (receptionDuration > 0.0) ? flow->CurrentBandwidthStats.ReceivedPackets / receptionDuration : 0.0,
            objectName.c_str(), flow->FlowID, (receptionDuration > 0.0) ? flow->CurrentBandwidthStats.ReceivedFrames  / receptionDuration : 0.0
            );
         totalBandwidthStats = totalBandwidthStats + flow->CurrentBandwidthStats;
      }
      flow->unlock();
   }

   // ====== Write total statistics =========================================
   const double totalDuration = (now - firstStatisticsEvent) / 1000000.0;
   scalarFile.printf(
      "scalar \"%s.total\" \"First Statistics Event\"  %llu\n"
      "scalar \"%s.total\" \"Last Statistics Event\"   %llu\n"
      "scalar \"%s.total\" \"Transmitted Bytes\"       %llu\n"
      "scalar \"%s.total\" \"Transmitted Packets\"     %llu\n"
      "scalar \"%s.total\" \"Transmitted Frames\"      %llu\n"
      "scalar \"%s.total\" \"Transmitted Byte Rate\"   %1.6f\n"
      "scalar \"%s.total\" \"Transmitted Packet Rate\" %1.6f\n"
      "scalar \"%s.total\" \"Transmitted Frame Rate\"  %1.6f\n"
      "scalar \"%s.total\" \"Received Bytes\"          %llu\n"
      "scalar \"%s.total\" \"Received Packets\"        %llu\n"
      "scalar \"%s.total\" \"Received Frames\"         %llu\n"
      "scalar \"%s.total\" \"Received Byte Rate\"      %1.6f\n"
      "scalar \"%s.total\" \"Received Packet Rate\"    %1.6f\n"
      "scalar \"%s.total\" \"Received Frame Rate\"     %1.6f\n"
      "scalar \"%s.total\" \"Lost Bytes\"              %llu\n"
      "scalar \"%s.total\" \"Lost Packets\"            %llu\n"
      "scalar \"%s.total\" \"Lost Frames\"             %llu\n"
      "scalar \"%s.total\" \"Lost Byte Rate\"          %1.6f\n"
      "scalar \"%s.total\" \"Lost Packet Rate\"        %1.6f\n"
      "scalar \"%s.total\" \"Lost Frame Rate\"         %1.6f\n"
      ,
      objectName.c_str(), firstStatisticsEvent,
      objectName.c_str(), now,

      objectName.c_str(), totalBandwidthStats.TransmittedBytes,
      objectName.c_str(), totalBandwidthStats.TransmittedPackets,
      objectName.c_str(), totalBandwidthStats.TransmittedFrames,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.TransmittedBytes   / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.TransmittedPackets / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.TransmittedFrames  / totalDuration : 0.0,

      objectName.c_str(), totalBandwidthStats.ReceivedBytes,
      objectName.c_str(), totalBandwidthStats.ReceivedPackets,
      objectName.c_str(), totalBandwidthStats.ReceivedFrames,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.ReceivedBytes   / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.ReceivedPackets / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.ReceivedFrames  / totalDuration : 0.0,

      objectName.c_str(), totalBandwidthStats.LostBytes,
      objectName.c_str(), totalBandwidthStats.LostPackets,
      objectName.c_str(), totalBandwidthStats.LostFrames,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.LostBytes   / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.LostPackets / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.LostFrames  / totalDuration : 0.0
      );
   unlock();

   // ====== Write CPU statistics ===========================================
   for(unsigned int i = 1; i <= CPULoadStats.getNumberOfCPUs(); i++) {
      for(unsigned int j = 0; j < CPULoadStats.getCpuStates(); j++) {
         scalarFile.printf("scalar \"%s.CPU[%u]\" \"%s\" %1.3f\n",
                           objectName.c_str(), i,
                           CPULoadStats.getCpuStateName(j),
                           CPULoadStats.getCpuStatePercentage(i, j));
      }
   }
   for(unsigned int j = 0; j < CPULoadStats.getCpuStates(); j++) {
      scalarFile.printf("scalar \"%s.totalCPU\" \"%s\" %1.3f\n",
                        objectName.c_str(),
                        CPULoadStats.getCpuStateName(j),
                        CPULoadStats.getCpuStatePercentage(0, j));
   }
}


// ###### Write vectors #####################################################
void FlowManager::writeVectorStatistics(const uint64_t           measurementID,
                                        const unsigned long long now,
                                        OutputFile&              vectorFile,
                                        FlowBandwidthStats&      globalStats,
                                        FlowBandwidthStats&      relGlobalStats,
                                        const unsigned long long firstStatisticsEvent,
                                        const unsigned long long lastStatisticsEvent)
{
   // ====== Write vector statistics header =================================
   if(vectorFile.getLine() == 0) {
      vectorFile.printf("AbsTime RelTime Interval\t"
                        "FlowID Description Jitter\t"
                        "Action\t"
                           "AbsBytes AbsPackets AbsFrames\t"
                           "RelBytes RelPackets RelFrames\n");
   }

   lock();

   // ====== Write flow statistics ==========================================
   FlowBandwidthStats lastTotalStats;
   FlowBandwidthStats currentTotalStats;
   const double duration = (now - lastStatisticsEvent) / 1000000.0;
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
       Flow* flow = *iterator;
       if(flow->MeasurementID == measurementID) {
          flow->lock();

          const FlowBandwidthStats relStats = flow->CurrentBandwidthStats - flow->LastBandwidthStats;
          lastTotalStats     = lastTotalStats + flow->LastBandwidthStats;
          currentTotalStats  = currentTotalStats + flow->CurrentBandwidthStats;
          CurrentGlobalStats = CurrentGlobalStats + relStats;

          const unsigned long long line = vectorFile.getLine();
          vectorFile.printf(
            "%06llu %llu %1.6f %1.6f\t%u \"%s\" %1.3f\t"
               "\"Sent\"     %llu %llu %llu\t%llu %llu %llu\n"
            "%06llu %llu %1.6f %1.6f\t%u \"%s\" %1.3f\t"
               "\"Received\" %llu %llu %llu\t%llu %llu %llu\n"
            "%06llu %llu %1.6f %1.6f\t%u \"%s\" %1.3f\t"
               "\"Lost\"     %llu %llu %llu\t%llu %llu %llu\n",

            line + 1, now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
               flow->FlowID, flow->TrafficSpec.Description.c_str(), flow->Jitter,
               flow->CurrentBandwidthStats.TransmittedBytes, flow->CurrentBandwidthStats.TransmittedPackets, flow->CurrentBandwidthStats.TransmittedFrames,
               relStats.TransmittedBytes, relStats.TransmittedPackets, relStats.TransmittedFrames,

            line + 2, now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
               flow->FlowID, flow->TrafficSpec.Description.c_str(), flow->Jitter,
               flow->CurrentBandwidthStats.ReceivedBytes, flow->CurrentBandwidthStats.ReceivedPackets, flow->CurrentBandwidthStats.ReceivedFrames,
               relStats.ReceivedBytes, relStats.ReceivedPackets, relStats.ReceivedFrames,

            line + 3, now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
               flow->FlowID, flow->TrafficSpec.Description.c_str(), flow->Jitter,
               flow->CurrentBandwidthStats.LostBytes, flow->CurrentBandwidthStats.LostPackets, flow->CurrentBandwidthStats.LostFrames,
               relStats.LostBytes, relStats.LostPackets, relStats.LostFrames);

          vectorFile.nextLine(); vectorFile.nextLine(); vectorFile.nextLine();

          flow->LastBandwidthStats = flow->CurrentBandwidthStats;
          flow->unlock();
       }
   }

   // ====== Write total statistics =========================================
   const unsigned long long line = vectorFile.getLine();
   const FlowBandwidthStats relTotalStats = currentTotalStats - lastTotalStats;
      vectorFile.printf(
      "%06llu %llu %1.6f %1.6f\t-1 \"Total\" 0\t"
         "\"Sent\"     %llu %llu %llu\t%llu %llu %llu\n"
      "%06llu %llu %1.6f %1.6f\t-1 \"Total\" 0\t"
         "\"Received\" %llu %llu %llu\t%llu %llu %llu\n"
      "%06llu %llu %1.6f %1.6f\t-1 \"Total\" 0\t"
         "\"Lost\"     %llu %llu %llu\t%llu %llu %llu\n",

      line + 1, now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
         currentTotalStats.TransmittedBytes, currentTotalStats.TransmittedPackets, currentTotalStats.TransmittedFrames,
         relTotalStats.TransmittedBytes, relTotalStats.TransmittedPackets, relTotalStats.TransmittedFrames,

      line + 2, now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
         currentTotalStats.ReceivedBytes, currentTotalStats.ReceivedPackets, currentTotalStats.ReceivedFrames,
         relTotalStats.ReceivedBytes, relTotalStats.ReceivedPackets, relTotalStats.ReceivedFrames,

      line + 3, now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
         currentTotalStats.LostBytes, currentTotalStats.LostPackets, currentTotalStats.LostFrames,
         relTotalStats.LostBytes, relTotalStats.LostPackets, relTotalStats.LostFrames);

    vectorFile.nextLine(); vectorFile.nextLine(); vectorFile.nextLine();

   // ====== Return global values (all measurements) for displaying =========
   globalStats     = CurrentGlobalStats;
   relGlobalStats  = CurrentGlobalStats - LastGlobalStats;

   LastGlobalStats = CurrentGlobalStats;

   unlock();
}


// ###### Reception thread function #########################################
void FlowManager::run()
{
   signal(SIGPIPE, SIG_IGN);

   do {
      // ====== Collect PollFDs for poll() ==================================
      lock();

      // NOTE:
      // The UDP socket handles flows and incoming new connects.
      // It is handled by mainLoop() of netperfmeter.cc, i.e. it must to be
      // ignored here!

      // ------ Get socket descriptors for flows ----------------------------
      std::set<int> socketSet;
      pollfd        pollFDs[FlowSet.size() + UnidentifiedSockets.size()];
      unsigned int  n = 0;
#ifdef DEBUG_POLL
      std::cerr << "poll-init:\n";
#endif
      for(unsigned int i = 0;i  < FlowSet.size(); i++) {
         FlowSet[i]->lock();
         FlowSet[i]->PollFDEntry = nullptr;
         if( (FlowSet[i]->InputStatus != Flow::Off) &&
             (FlowSet[i]->SocketDescriptor >= 0) ) {
            if(!((FlowSet[i]->getTrafficSpec().Protocol == IPPROTO_UDP) &&   // Global UDP socket is handled by main loop!
                 (FlowSet[i]->RemoteControlSocketDescriptor >= 0)) ) {       // Incoming UDP association has RemoteControlSocketDescriptor >= 0.
               if(socketSet.find(FlowSet[i]->SocketDescriptor) == socketSet.end()) {
                  pollFDs[n].fd      = FlowSet[i]->SocketDescriptor;
                  pollFDs[n].events  = POLLIN;
                  pollFDs[n].revents = 0;
                  FlowSet[i]->PollFDEntry = &pollFDs[n];
                  n++;
                  socketSet.insert(FlowSet[i]->SocketDescriptor);
#ifdef DEBUG_POLL
                  std::cerr << "\t" << n << ": flow socket " << FlowSet[i]->SocketDescriptor
                            << " protocol " << FlowSet[i]->getTrafficSpec().Protocol << "\n";
#endif
               }
            }
         }
         else {
            FlowSet[i]->PollFDEntry = nullptr;
         }
         FlowSet[i]->unlock();
      }
      assert(n <= FlowSet.size());

      // ------ Get socket descriptors for yet unidentified associations ----
      UpdatedUnidentifiedSockets = false;
      for(std::map<int, UnidentifiedSocket*>::iterator iterator = UnidentifiedSockets.begin();
          iterator != UnidentifiedSockets.end(); iterator++) {
         UnidentifiedSocket* ud = iterator->second;
         ud->PollFDEntry = nullptr;
         // See note above about UDP: UDP is handled by mainLoop()!
         if(ud->Protocol != IPPROTO_UDP) {
            pollFDs[n].fd      = ud->SocketDescriptor;
            pollFDs[n].events  = POLLIN;
            pollFDs[n].revents = 0;
            ud->PollFDEntry = &pollFDs[n];
#ifdef DEBUG_POLL
            std::cerr << "\t" << n << ": unidentified socket "
                      << ud->SocketDescriptor
                      << " protocol " << ud->Protocol << "\n";
#endif
            n++;
         }
      }
      assert(n <= FlowSet.size() + UnidentifiedSockets.size());
      const unsigned long long nextEvent = getNextEvent();
      unlock();


      // ====== Wait for events =============================================
      unsigned long long now = getMicroTime();
      const int timeout = pollTimeout(getMicroTime(), 2,
                                      now + 2500000,
                                      nextEvent);
#ifdef DEBUG_POLL
      std::cerr << "poll with timeout=" << timeout << " ms\n";
#endif
      const int result = ext_poll_wrapper((pollfd*)&pollFDs, n, timeout);
#ifdef DEBUG_POLL
      std::cerr << "poll result=" << result << "\n";
#endif


      // ====== Handle events ===============================================
      lock();

      now = getMicroTime();
      if(result > 0) {
         // ====== Handle read events of flows ==============================
         for(unsigned int i = 0; i < FlowSet.size(); i++) {
            FlowSet[i]->lock();
            const pollfd* entry    = FlowSet[i]->PollFDEntry;
            const int     protocol = FlowSet[i]->getTrafficSpec().Protocol;
            if(entry) {
               if(entry->revents & POLLIN) {
#ifdef DEBUG_POLL
                  std::cerr << "\tPOLLIN: flow " << FlowSet[i]->SocketDescriptor
                            << " protocol " << FlowSet[i]->getTrafficSpec().Protocol << "\n";
#endif
                  // NOTE: FlowSet[i] may not be the actual Flow!
                  //       It may be another stream of the same SCTP assoc!
                  const bool dataOkay = handleNetPerfMeterData(true, now,
                                                               protocol, entry->fd);
                  if(!dataOkay) {
                     assert(protocol != IPPROTO_UDP);
                     if(protocol != IPPROTO_UDP) {
                        // Close the broken connection!
                        LOG_WARNING
                        stdlog << format("Closing disconnected socket %d!",
                                         FlowSet[i]->SocketDescriptor) << "\n";
                        LOG_END
                        ext_close(FlowSet[i]->SocketDescriptor);
                        FlowSet[i]->SocketDescriptor = -1;
                     }
                  }
               }
            }
            FlowSet[i]->unlock();
         }

         // ====== Handle read events of yet unidentified sockets ===========
         if(!UpdatedUnidentifiedSockets) {
            std::map<int, UnidentifiedSocket*>::iterator iterator = UnidentifiedSockets.begin();
            while(iterator != UnidentifiedSockets.end()) {
               const UnidentifiedSocket* ud = iterator->second;
               // See note above about UDP: UDP is handled by mainLoop()!
               if(ud->Protocol != IPPROTO_UDP) {
                  const pollfd* entry = ud->PollFDEntry;
                  if(entry->revents & POLLIN) {
#ifdef DEBUG_POLL
                     std::cerr << "\tPOLLIN: unidentified " << ud->SocketDescriptor
                               << " protocol " << ud->Protocol << "\n";
#endif
                     const bool dataOkay = handleNetPerfMeterData(true, now,
                                                                  ud->Protocol,
                                                                  ud->SocketDescriptor);
                     if(!dataOkay) {
                        // Incoming connection has already been closed -> remove it!
                        LOG_WARNING
                        stdlog << format("Shutdown of still unidentified incoming connection on socket %d!",
                                         ud->SocketDescriptor) << "\n";
                        LOG_END
                        iterator = UnidentifiedSockets.erase(iterator);
                        Reader.deregisterSocket(ud->SocketDescriptor);
                        ext_close(ud->SocketDescriptor);
                        assert(UnidentifiedSockets.find(ud->SocketDescriptor) == UnidentifiedSockets.end());
                        delete ud;
                        continue;
                     }

//                      if(UpdatedUnidentifiedSockets) {
//                         // NOTE: The UnidentifiedSockets set may have changed in
//                         // handleDataMessage -> ... -> FlowManager::identifySocket
//                         // => stop processing if this is the case
//                         break;
//                      }
                  }
               }
               iterator++;
            }
         }
      }

      // ====== Handle statistics timer =====================================
      if(getNextEvent() <= now) {
         handleEvents(now);
      }

      unlock();
   } while(!isStopping());
}


// ###### Configure send and receive buffer sizes ###########################
bool setBufferSizes(int sd, const int sndBufSize, const int rcvBufSize)
{
   if(sndBufSize > 0) {
      if(ext_setsockopt(sd, SOL_SOCKET, SO_SNDBUF, &sndBufSize, sizeof(sndBufSize)) < 0) {
         LOG_ERROR
         stdlog << format("Failed to configure send buffer size %d (SO_SNDBUF option) on socket %d: %s!",
                          sndBufSize, sd, strerror(errno)) << "\n";
         LOG_END
         return false;
      }
      int       newBufferSize       = 0;
      socklen_t newBufferSizeLength = sizeof(newBufferSize);
      if(ext_getsockopt(sd, SOL_SOCKET, SO_SNDBUF, &newBufferSize, &newBufferSizeLength) < 0) {
         LOG_ERROR
         stdlog << format("Failed to obtain send buffer size (SO_SNDBUF option) on socket %d: %s!",
                          sd, strerror(errno)) << "\n";
         LOG_END
         return false;
      }
      if(newBufferSize < sndBufSize) {
         LOG_ERROR
         stdlog << format("Actual send buffer size %d < configured send buffer size %d on socket %d!",
                          newBufferSize, sndBufSize, sd) << "\n";
         LOG_END
         return false;
      }
   }

   if(rcvBufSize > 0) {
      if(ext_setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &rcvBufSize, sizeof(rcvBufSize)) < 0) {
         LOG_ERROR
         stdlog << format("Failed to configure receive buffer size %d (SO_RCVBUF option) on socket %d: %s!",
                          rcvBufSize, sd, strerror(errno)) << "\n";
         LOG_END
         return false;
      }
      int       newBufferSize       = 0;
      socklen_t newBufferSizeLength = sizeof(newBufferSize);
      if(ext_getsockopt(sd, SOL_SOCKET, SO_RCVBUF, &newBufferSize, &newBufferSizeLength) < 0) {
         LOG_ERROR
         stdlog << format("Failed to obtain receive buffer size (SO_RCVBUF option) on socket %d: %s!",
                          sd, strerror(errno)) << "\n";
         LOG_END
         return false;
      }
      if(newBufferSize < rcvBufSize) {
         LOG_ERROR
         stdlog << format("Actual receive buffer size %d < configured receive buffer size %d on socket %d!",
                          newBufferSize, rcvBufSize, sd) << "\n";
         LOG_END
         return false;
      }
   }

   return true;
}


// ###### Create server socket of appropriate family and bind it ############
int createAndBindSocket(const int             family,
                        const int             type,
                        const int             protocol,
                        const uint16_t        localPort,
                        const unsigned int    localAddresses,
                        const sockaddr_union* localAddressArray,
                        const bool            listenMode,
                        const bool            bindV6Only)
{
   int sd = createSocket(family, type, protocol,
                         localAddresses, localAddressArray);
   if(sd >= 0) {
      int reuse = 1;
      if(ext_setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
         LOG_WARNING
         stdlog << format("Failed to configure socket reuse (SO_REUSEADDR option) on socket %d: %s!",
                          sd, strerror(errno)) << "\n";
         LOG_END
      }

      const int success = bindSocket(sd, family, type, protocol,
                                     localPort, localAddresses, localAddressArray,
                                     listenMode, bindV6Only);
      if(success < 0) {
         ext_close(sd);
         return success;
      }
   }
   return sd;
}
