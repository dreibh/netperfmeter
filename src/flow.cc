/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2009-2011 by Thomas Dreibholz
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
 * You should have relReceived a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact: dreibh@iem.uni-due.de
 */

#include "flow.h"
#include "control.h"
#include "transfer.h"

#include <string.h>
#include <signal.h>
#include <poll.h>
#include <assert.h>
#include <math.h>


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
   flow->PollFDEntry = NULL;
   FlowSet.push_back(flow);
   unlock();
}


// ###### Remove flow #######################################################
void FlowManager::removeFlow(Flow* flow)
{
   lock();
   flow->deactivate();

   // ====== Remove flow from flow set ======================================
   flow->PollFDEntry = NULL;
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
   Flow* found = NULL;

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

   return(found);
}


// ###### Find Flow by socket descriptor and Stream ID ######################
Flow* FlowManager::findFlow(const int      socketDescriptor,
                            const uint16_t streamID)
{
   Flow* found = NULL;

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

   return(found);
}


// ###### Find Flow by source address #######################################
Flow* FlowManager::findFlow(const struct sockaddr* from)
{
   Flow* found = NULL;

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

   return(found);
}


// ###### Start measurement #################################################
bool FlowManager::startMeasurement(const uint64_t           measurementID,
                                   const unsigned long long now,
                                   const char*              vectorNamePattern,
                                   const OutputFileFormat   vectorFileFormat,
                                   const char*              scalarNamePattern,
                                   const OutputFileFormat   scalarFileFormat,
                                   const bool               printFlows)
{
   bool success = false;

   lock();
   Measurement* measurement = new Measurement;
   if(measurement != NULL) {
      if(measurement->initialize(now, measurementID,
                                 vectorNamePattern, vectorFileFormat,
                                 scalarNamePattern, scalarFileFormat)) {
         success = true;
         for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
            iterator != FlowSet.end();iterator++) {
            Flow* flow = *iterator;
            if(flow->MeasurementID == measurementID) {
               flow->setMeasurement(measurement);
               if(flow->SocketDescriptor >= 0) {
                  flow->BaseTime     = now;
                  flow->InputStatus  = Flow::On;
                  flow->OutputStatus = (flow->TrafficSpec.OnOffEvents.size() > 0) ? Flow::Off : Flow::On;
                  if(printFlows) {
                     flow->print(std::cout);
                  }
                  flow->activate();
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

   return(success);
}


// ###### Stop measurement ##################################################
void FlowManager::stopMeasurement(const uint64_t           measurementID,
                                  const bool               printFlows,
                                  const unsigned long long now)
{
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
               if(printFlows) {
                  flow->print(std::cout, true);
               }
            }
         }
      }
   }
   unlock();
}


// ###### Add measurement ###################################################
bool FlowManager::addMeasurement(Measurement* measurement)
{
   bool success = false;

   lock();
   if(!findMeasurement(measurement->MeasurementID)) {
      MeasurementSet.insert(std::pair<uint64_t, Measurement*>(measurement->MeasurementID,
                                                              measurement));
      success = true;
   }
   unlock();

   return(success);
}


// ###### Print measurements ################################################
void FlowManager::printMeasurements(std::ostream& os)
{
   os << "Measurements:" << std::endl;
   for(std::map<uint64_t, Measurement*>::iterator iterator = MeasurementSet.begin();
       iterator != MeasurementSet.end(); iterator++) {
      char str[64];
      snprintf((char*)&str, sizeof(str), "%llx -> ptr=%p",
               (unsigned long long)iterator->first, iterator->second);
      os << "   - " << str << std::endl;
   }
}


// ###### Find measurement ##################################################
Measurement* FlowManager::findMeasurement(const uint64_t measurementID)
{
   std::map<uint64_t, Measurement*>::iterator found = MeasurementSet.find(measurementID);
   if(found != MeasurementSet.end()) {
      return(found->second);
   }
   return(NULL);
}


// ###### Remove measurement ################################################
void FlowManager::removeMeasurement(Measurement* measurement)
{
   lock();
   MeasurementSet.erase(measurement->MeasurementID);
   unlock();
}


// ###### Add socket to flow manager ########################################
void FlowManager::addSocket(const int protocol, const int socketDescriptor)
{
   lock();
   FlowManager::getFlowManager()->getMessageReader()->registerSocket(protocol, socketDescriptor);
   UnidentifiedSockets.insert(std::pair<int, int>(socketDescriptor, protocol));
   UpdatedUnidentifiedSockets = true;
   unlock();
}


// ###### Map socket to flow ################################################
Flow* FlowManager::identifySocket(const uint64_t         measurementID,
                                  const uint32_t         flowID,
                                  const uint16_t         streamID,
                                  const int              socketDescriptor,
                                  const sockaddr_union*  from,
                                  const OutputFileFormat vectorFileFormat,
                                  int&                   controlSocketDescriptor,
                                  sctp_assoc_t&          controlAssocID)
{
   bool success            = false;
   controlSocketDescriptor = -1;
   controlAssocID          = 0;

   lock();
   Flow* flow = findFlow(measurementID, flowID, streamID);
   if( (flow != NULL) && (flow->RemoteAddressIsValid == false) ) {
      flow->lock();
      flow->setSocketDescriptor(socketDescriptor, false,
                                (flow->getTrafficSpec().Protocol != IPPROTO_UDP));
      flow->RemoteAddress        = *from;
      flow->RemoteAddressIsValid = true;
      controlSocketDescriptor    = flow->RemoteControlSocketDescriptor;
      controlAssocID             = flow->RemoteControlAssocID;
      success = flow->initializeVectorFile(NULL, vectorFileFormat);
      flow->unlock();
      removeSocket(socketDescriptor, false);   // Socket is now managed as flow!
   }
   unlock();

   if(!success) {
      flow = NULL;
   }

   return(flow);
}


// ###### Remove socket #####################################################
void FlowManager::removeSocket(const int  socketDescriptor,
                               const bool closeSocket)
{
   lock();
   std::map<int, int>::iterator found = UnidentifiedSockets.find(socketDescriptor);
   if(found != UnidentifiedSockets.end()) {
      UnidentifiedSockets.erase(found);
      UpdatedUnidentifiedSockets = true;
   }
   if(closeSocket) {
      FlowManager::getFlowManager()->getMessageReader()->deregisterSocket(socketDescriptor);
   }
   unlock();
}


// ###### Get next statistics or display events #############################
unsigned long long FlowManager::getNextEvent()
{
   unsigned long long nextEvent = NextDisplayEvent;

   lock();
   for(std::map<uint64_t, Measurement*>::iterator iterator = MeasurementSet.begin();
       iterator != MeasurementSet.end(); iterator++) {
       const Measurement* measurement = iterator->second;
       nextEvent = std::min(nextEvent, measurement->NextStatisticsEvent);
   }
   unlock();

   return(nextEvent);
}


// ###### Handle statistics and display events ##############################
void FlowManager::handleEvents(const unsigned long long now)
{
   lock();

   // ====== Handle statistics events =======================================
   bool hasUpdate = false;
   for(std::map<uint64_t, Measurement*>::iterator iterator = MeasurementSet.begin();
       iterator != MeasurementSet.end(); iterator++) {
       Measurement* measurement = iterator->second;
       if(measurement->NextStatisticsEvent <= now) {
          measurement->writeVectorStatistics(now, GlobalStats, RelGlobalStats);
          hasUpdate = true;
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
      if((DisplayOn) && (gOutputVerbosity >= NPFOV_BANDWIDTH_INFO)) {
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
               char str[32];
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

         // NOTE: ostream/cout has race condition problem according to helgrind.
         //       => simply using stdout instead.
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

          vectorFile.printf(
            "%06llu %llu %1.6f %1.6f\t%u \"%s\" %1.3f\t"
               "\"Sent\"     %llu %llu %llu\t%llu %llu %llu\n"
            "%06llu %llu %1.6f %1.6f\t%u \"%s\" %1.3f\t"
               "\"Received\" %llu %llu %llu\t%llu %llu %llu\n"
            "%06llu %llu %1.6f %1.6f\t%u \"%s\" %1.3f\t"
               "\"Lost\"     %llu %llu %llu\t%llu %llu %llu\n",

            vectorFile.nextLine(), now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
               flow->FlowID, flow->TrafficSpec.Description.c_str(), flow->Jitter,
               flow->CurrentBandwidthStats.TransmittedBytes, flow->CurrentBandwidthStats.TransmittedPackets, flow->CurrentBandwidthStats.TransmittedFrames,
               relStats.TransmittedBytes, relStats.TransmittedPackets, relStats.TransmittedFrames,

            vectorFile.nextLine(), now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
               flow->FlowID, flow->TrafficSpec.Description.c_str(), flow->Jitter,
               flow->CurrentBandwidthStats.ReceivedBytes, flow->CurrentBandwidthStats.ReceivedPackets, flow->CurrentBandwidthStats.ReceivedFrames,
               relStats.ReceivedBytes, relStats.ReceivedPackets, relStats.ReceivedFrames,

            vectorFile.nextLine(), now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
               flow->FlowID, flow->TrafficSpec.Description.c_str(), flow->Jitter,
               flow->CurrentBandwidthStats.LostBytes, flow->CurrentBandwidthStats.LostPackets, flow->CurrentBandwidthStats.LostFrames,
               relStats.LostBytes, relStats.LostPackets, relStats.LostFrames);

          flow->LastBandwidthStats = flow->CurrentBandwidthStats;
          flow->unlock();
       }
   }

   // ====== Write total statistics =========================================
   const FlowBandwidthStats relTotalStats = currentTotalStats - lastTotalStats;
      vectorFile.printf(
      "%06llu %llu %1.6f %1.6f\t-1 \"Total\" 0\t"
         "\"Sent\"     %llu %llu %llu\t%llu %llu %llu\n"
      "%06llu %llu %1.6f %1.6f\t-1 \"Total\" 0\t"
         "\"Received\" %llu %llu %llu\t%llu %llu %llu\n"
      "%06llu %llu %1.6f %1.6f\t-1 \"Total\" 0\t"
         "\"Lost\"     %llu %llu %llu\t%llu %llu %llu\n",

      vectorFile.nextLine(), now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
         currentTotalStats.TransmittedBytes, currentTotalStats.TransmittedPackets, currentTotalStats.TransmittedFrames,
         relTotalStats.TransmittedBytes, relTotalStats.TransmittedPackets, relTotalStats.TransmittedFrames,

      vectorFile.nextLine(), now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
         currentTotalStats.ReceivedBytes, currentTotalStats.ReceivedPackets, currentTotalStats.ReceivedFrames,
         relTotalStats.ReceivedBytes, relTotalStats.ReceivedPackets, relTotalStats.ReceivedFrames,

      vectorFile.nextLine(), now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
         currentTotalStats.LostBytes, currentTotalStats.LostPackets, currentTotalStats.LostFrames,
         relTotalStats.LostBytes, relTotalStats.LostPackets, relTotalStats.LostFrames);


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
      std::set<int> socketSet;
      pollfd        pollFDs[FlowSet.size() + UnidentifiedSockets.size()];
      size_t        n = 0;
      for(size_t i = 0;i  < FlowSet.size();i++) {
         FlowSet[i]->lock();
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
               }
            }
         }
         else {
            FlowSet[i]->PollFDEntry = NULL;
         }
         FlowSet[i]->unlock();
      }
      UpdatedUnidentifiedSockets = false;
      pollfd* unidentifiedSocketsPollFDIndex[UnidentifiedSockets.size()];
      size_t i = 0;
      for(std::map<int, int>::iterator iterator = UnidentifiedSockets.begin();
          iterator != UnidentifiedSockets.end(); iterator++) {
         pollFDs[n].fd      = iterator->first;
         pollFDs[n].events  = POLLIN;
         pollFDs[n].revents = 0;
         unidentifiedSocketsPollFDIndex[i] = &pollFDs[n];
         n++; i++;
      }
      const unsigned long long nextEvent = getNextEvent();
      unlock();


      // ====== Wait for events =============================================
      unsigned long long now = getMicroTime();
      const int timeout = pollTimeout(getMicroTime(), 2,
                                      now + 250000,
                                      nextEvent);
      // printf("timeout=%d\n", timeout);
      const int result = ext_poll_wrapper((pollfd*)&pollFDs, n, timeout);
      // printf("result=%d\n",result);


      // ====== Handle events ===============================================
      lock();

      now = getMicroTime();
      if(result > 0) {
         // ====== Handle read events of flows ==============================
         for(i = 0;i  < FlowSet.size();i++) {
            FlowSet[i]->lock();
            const pollfd* entry    = FlowSet[i]->PollFDEntry;
            const int     protocol = FlowSet[i]->getTrafficSpec().Protocol;
            // FlowSet[i]->unlock(); --- not necessary: FlowManager is locked
            // NOTE: Release the lock here, because the FlowSet enty may belong
            //       to another stream of the socket. handleNetPerfMeterData()
            //       will find and lock the actual FlowSet entry!
            if( (entry) && (entry->revents & POLLIN) ) {
               // NOTE: FlowSet[i] may not be the actual Flow!
               //       It may be another stream of the same SCTP assoc!
               handleNetPerfMeterData(true, now, protocol, entry->fd);
            }
            FlowSet[i]->unlock();
         }


         // ====== Handle read events of yet unidentified sockets ===========
         if(!UpdatedUnidentifiedSockets) {
            i = 0;
            for(std::map<int, int>::iterator iterator = UnidentifiedSockets.begin();
               iterator != UnidentifiedSockets.end(); iterator++) {
               assert(unidentifiedSocketsPollFDIndex[i]->fd == iterator->first);
               if(unidentifiedSocketsPollFDIndex[i]->revents & POLLIN) {
                  if(handleNetPerfMeterData(true, now,
                                            iterator->second,
                                            iterator->first) == 0) {
                     // Incoming connection has already been closed -> remove it!
                     std::cout << "NOTE: Shutdown of still unidentified incoming connection "
                               << iterator->first << "!" << std::endl;
                     removeSocket(iterator->first, true);
                     break;
                  }
                  if(UpdatedUnidentifiedSockets) {
                     // NOTE: The UnidentifiedSockets set may have changed in
                     // handleDataMessage -> ... -> FlowManager::identifySocket
                     // => stop processing if this is the case
                     break;
                  }
               }
               i++;
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








// ###### Constructor #######################################################
Flow::Flow(const uint64_t         measurementID,
           const uint32_t         flowID,
           const uint16_t         streamID,
           const FlowTrafficSpec& trafficSpec,
           const int              controlSocketDescriptor,
           const sctp_assoc_t     controlAssocID)
{
   lock();
   MeasurementID                 = measurementID;
   FlowID                        = flowID;
   StreamID                      = streamID;

   SocketDescriptor              = -1;
   OriginalSocketDescriptor      = false;
   RemoteControlSocketDescriptor = controlSocketDescriptor;
   RemoteControlAssocID          = controlAssocID;
   RemoteAddressIsValid          = false;

   InputStatus                   = WaitingForStartup;
   OutputStatus                  = WaitingForStartup;
   BaseTime                      = getMicroTime();

   TrafficSpec                   = trafficSpec;

   MyMeasurement                 = NULL;
   FirstTransmission             = 0;
   LastTransmission              = 0;
   FirstReception                = 0;
   LastReception                 = 0;
   resetStatistics();
   LastOutboundSeqNumber         = ~0;
   LastOutboundFrameID           = ~0;
   NextStatusChangeEvent         = ~0ULL;
   unlock();

   FlowManager::getFlowManager()->addFlow(this);
}


// ###### Destructor ########################################################
Flow::~Flow()
{
   FlowManager::getFlowManager()->removeFlow(this);
   deactivate();
   VectorFile.finish(true);
   if((SocketDescriptor >= 0) && (OriginalSocketDescriptor)) {
      if(DeleteWhenFinished) {
         FlowManager::getFlowManager()->getMessageReader()->deregisterSocket(SocketDescriptor);
         ext_close(SocketDescriptor);
      }
   }
}


// ###### Reset statistics ##################################################
void Flow::resetStatistics()
{
   lock();
   CurrentBandwidthStats.reset();
   LastBandwidthStats.reset();
   Jitter = 0;
   Delay  = 0;
   unlock();
}


// ###### Print Flow ########################################################
void Flow::print(std::ostream& os, const bool printStatistics)
{
   lock();
   if((OriginalSocketDescriptor) || (RemoteControlAssocID != 0)) {
      if(TrafficSpec.Protocol != IPPROTO_SCTP) {
         os << "+ " << getProtocolName(TrafficSpec.Protocol) << " Flow (Flow ID #"
            << FlowID << " \"" << TrafficSpec.Description << "\"):" << std::endl;
      }
      else {
         os << "+ " << getProtocolName(TrafficSpec.Protocol) << " Flow:" << std::endl;
      }
   }
   if(TrafficSpec.Protocol == IPPROTO_SCTP) {
      os << "   o Stream #" << StreamID << " (Flow ID #"
         << FlowID << " \"" << TrafficSpec.Description << "\"):" << std::endl;
   }
   TrafficSpec.print(os);


   if(printStatistics) {
      CurrentBandwidthStats.print(os,
                                  (LastTransmission - FirstTransmission) / 1000000.0,
                                  (LastReception    - FirstReception)    / 1000000.0);
   }
   unlock();
}


// ###### Update socket descriptor ##########################################
void Flow::setSocketDescriptor(const int  socketDescriptor,
                               const bool originalSocketDescriptor,
                               const bool deleteWhenFinished)
{
   deactivate();
   lock();
   SocketDescriptor         = socketDescriptor;
   OriginalSocketDescriptor = originalSocketDescriptor;
   DeleteWhenFinished       = deleteWhenFinished;

   if(SocketDescriptor >= 0) {
      FlowManager::getFlowManager()->getMessageReader()->registerSocket(
         TrafficSpec.Protocol, SocketDescriptor);
   }
   unlock();
}


// ###### Initialize flow's vector file #####################################
bool Flow::initializeVectorFile(const char* name, const OutputFileFormat format)
{
   bool success = false;

   lock();
   if(VectorFile.initialize(name, format)) {
      VectorFile.nextLine();
      success = VectorFile.printf(
                   "AbsTime RelTime SeqNumber Delay PrevPacketDelayDiff Jitter\n");
   }
   unlock();

   return(success);
}


// ###### Update transmission statistics ####################################
void Flow::updateTransmissionStatistics(const unsigned long long now,
                                        const size_t             addedFrames,
                                        const size_t             addedPackets,
                                        const size_t             addedBytes)
{
   lock();

   // ====== Update statistics ==============================================
   LastTransmission = now;
   if(FirstTransmission == 0) {
      FirstTransmission = now;
   }
   CurrentBandwidthStats.TransmittedFrames  += addedFrames;
   CurrentBandwidthStats.TransmittedPackets += addedPackets;
   CurrentBandwidthStats.TransmittedBytes   += addedBytes;

   unlock();
}


// ###### Update reception statistics #######################################
void Flow::updateReceptionStatistics(const unsigned long long now,
                                     const size_t             addedFrames,
                                     const size_t             addedBytes,
                                     const size_t             lostFrames,
                                     const size_t             lostPackets,
                                     const size_t             lostBytes,
                                     const unsigned long long seqNumber,
                                     const double             delay,
                                     const double             delayDiff,
                                     const double             jitter)
{
   lock();

   // ====== Update statistics ==============================================
   LastReception = now;
   if(FirstReception == 0) {
      FirstReception = now;
   }
   CurrentBandwidthStats.ReceivedFrames  += addedFrames;
   CurrentBandwidthStats.ReceivedPackets++;
   CurrentBandwidthStats.ReceivedBytes   += addedBytes;
   CurrentBandwidthStats.LostFrames      += lostFrames;
   CurrentBandwidthStats.LostPackets     += lostPackets;
   CurrentBandwidthStats.LostBytes       += lostBytes;
   Delay  = delay;
   Jitter = jitter;

   // ====== Write line to flow's vector file ===============================
   if( (MyMeasurement) && (MyMeasurement->getFirstStatisticsEvent() > 0) ) {
      VectorFile.printf(
         "%06llu %llu %1.6f\t"
         "%llu %1.3f %1.3f %1.3f\n",
         VectorFile.nextLine(), now,
         (double)(now - MyMeasurement->getFirstStatisticsEvent()) / 1000000.0,
         seqNumber, delay, delayDiff, jitter);
   }

   unlock();
}


// ###### Start flow's transmission thread ##################################
bool Flow::activate()
{
   deactivate();
   assert(SocketDescriptor >= 0);
   return(start());
}


// ###### Stop flow's transmission thread ###################################
void Flow::deactivate(const bool asyncStop)
{
   if(isRunning()) {
      lock();
      InputStatus  = Off;
      OutputStatus = Off;
      unlock();
      stop();
      if(SocketDescriptor >= 0) {
         if(TrafficSpec.Protocol == IPPROTO_UDP) {
            // NOTE: There is only one UDP socket. We cannot close it here!
            // The thread will notice the need to finish after the poll()
            // timeout.
         }
         else {
            const int shutdownOkay = ext_shutdown(SocketDescriptor, 2);
/*
            const int shutdownOkay = sctp_sendmsg(SocketDescriptor, NULL, 0, NULL, 0, 0, SCTP_EOF, 0, 0, 0);
*/
            if(shutdownOkay < 0) {
               perror("WARNING: Cannut shut association down");
            }
         }
      }
      if(!asyncStop) {
         waitForFinish();
         FlowManager::getFlowManager()->getMessageReader()->deregisterSocket(SocketDescriptor);
         PollFDEntry = NULL;   // Poll FD entry is now invalid!
      }
   }
}


// ###### Schedule next transmission event (non-saturated sender) ###########
unsigned long long Flow::scheduleNextTransmissionEvent()
{
   unsigned long long nextTransmissionEvent = ~0ULL;   // No transmission

   lock();
   if(OutputStatus == On) {
      // ====== Saturated sender ============================================
      if( (TrafficSpec.OutboundFrameSize[0] > 0.0) && (TrafficSpec.OutboundFrameRate[0] <= 0.0000001) ) {
         nextTransmissionEvent = 0;
      }
      // ====== Non-saturated sender ========================================
      else if( (TrafficSpec.OutboundFrameSize[0] > 0.0) && (TrafficSpec.OutboundFrameRate[0] > 0.0000001) ) {
         const double nextFrameRate = getRandomValue((const double*)&TrafficSpec.OutboundFrameRate,
                                                     TrafficSpec.OutboundFrameRateRng);
         nextTransmissionEvent = LastTransmission + (unsigned long long)rint(1000000.0 / nextFrameRate);
      }
   }
   unlock();

   return(nextTransmissionEvent);
}


// ###### Schedule next status change event #################################
unsigned long long Flow::scheduleNextStatusChangeEvent(const unsigned long long now)
{
   lock();

   std::set<unsigned int>::iterator first = TrafficSpec.OnOffEvents.begin();
   if((OutputStatus != WaitingForStartup) && (first != TrafficSpec.OnOffEvents.end())) {
      const unsigned int relNextEvent = *first;
      const unsigned long long absNextEvent = BaseTime + (1000ULL * relNextEvent);
      NextStatusChangeEvent = absNextEvent;
   }
   else {
      NextStatusChangeEvent = ~0ULL;
   }

   unlock();
   return(NextStatusChangeEvent);
}


// ###### Status change event ###############################################
void Flow::handleStatusChangeEvent(const unsigned long long now)
{
   lock();
   if(NextStatusChangeEvent <= now) {
      std::set<unsigned int>::iterator first = TrafficSpec.OnOffEvents.begin();
      assert(first != TrafficSpec.OnOffEvents.end());

      if(OutputStatus == Off) {
         OutputStatus = On;
      }
      else if(OutputStatus == On) {
         OutputStatus = Off;
      }
      else {
         assert(false);
      }

      TrafficSpec.OnOffEvents.erase(first);
   }
   scheduleNextStatusChangeEvent(now);
   unlock();
}


// ###### Flow's transmission thread function ###############################
void Flow::run()
{
   signal(SIGPIPE, SIG_IGN);

   bool result = true;
   do {
      // ====== Schedule next status change event ===========================
      unsigned long long       now              = getMicroTime();
      const unsigned long long nextStatusChange = scheduleNextStatusChangeEvent(now);
      const unsigned long long nextTransmission = scheduleNextTransmissionEvent();
      unsigned long long       nextEvent        = std::min(nextStatusChange, nextTransmission);

      // ====== Wait until there is something to do =========================
      if(nextEvent > now) {
         int timeout = pollTimeout(now, 2,
                                   now + 1000000,
                                   nextEvent);
         ext_poll_wrapper(NULL, 0, timeout);
         now = getMicroTime();
      }

      // ====== Send outgoing data ==========================================
      lock();
      const FlowStatus outputStatus = OutputStatus;
      unlock();
      if(outputStatus == Flow::On) {
         // ====== Outgoing data (saturated sender) =========================
         if( (TrafficSpec.OutboundFrameSize[0] > 0.0) &&
             (TrafficSpec.OutboundFrameRate[0] <= 0.0000001) ) {
            result = (transmitFrame(this, now) > 0);
         }

         // ====== Outgoing data (non-saturated sender) =====================
         else if( (TrafficSpec.OutboundFrameSize[0] >= 1.0) &&
                  (TrafficSpec.OutboundFrameRate[0] > 0.0000001) ) {
            const unsigned long long lastEvent = LastTransmission;
            if(nextTransmission <= now) {
               do {
                  result = (transmitFrame(this, now) > 0);
                  if(now - lastEvent > 1000000) {
                     // Time gap of more than 1s -> do not try to correct
                     break;
                  }
               } while(scheduleNextTransmissionEvent() <= now);

               if(TrafficSpec.Protocol == IPPROTO_UDP) {
                  // Keep sending, even if there is a temporary failure.
                  result = true;
               }
            }
         }
      }

      // ====== Handle status changes =======================================
      if(nextStatusChange <= now) {
         handleStatusChangeEvent(now);
      }
   } while( (result == true) && (!isStopping()) );
}


// ###### Configure socket parameters #######################################
bool Flow::configureSocket(const int socketDescriptor)
{
    int bufferSize = TrafficSpec.RcvBufferSize;
    if(ext_setsockopt(socketDescriptor, SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize)) < 0) {
        std::cerr << "ERROR: Failed to configure receive buffer size on SCTP socket (SO_RCVBUF option) - "
                  << strerror(errno) << "!" << std::endl;
        return(false);
    }
    bufferSize = TrafficSpec.SndBufferSize;
    if(ext_setsockopt(socketDescriptor, SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize)) < 0) {
        std::cerr << "ERROR: Failed to configure send buffer size on SCTP socket (SO_SNDBUF option) - "
                  << strerror(errno) << "!" << std::endl;
        return(false);
    }

    if(TrafficSpec.Protocol == IPPROTO_SCTP) {
#ifdef SCTP_CMT_ON_OFF
        struct sctp_assoc_value cmtOnOff;
        cmtOnOff.assoc_id    = 0;
        cmtOnOff.assoc_value = TrafficSpec.CMT;
        if(ext_setsockopt(socketDescriptor, IPPROTO_SCTP, SCTP_CMT_ON_OFF, &cmtOnOff, sizeof(cmtOnOff)) < 0) {
          if(TrafficSpec.CMT != NPAF_PRIMARY_PATH) {
              std::cerr << "ERROR: Failed to configure CMT usage on SCTP socket (SCTP_CMT_ON_OFF option) - "
                        << strerror(errno) << "!" << std::endl;
             return(false);
          }
        }
#else
        if(TrafficSpec.CMT != NPAF_PRIMARY_PATH) {
          std::cerr << "ERROR: CMT usage configured, but not supported by this system!" << std::endl;
          return(false);
        }
#endif
   }

#ifdef HAVE_DCCP
   if(TrafficSpec.Protocol == IPPROTO_DCCP) {
      const uint8_t value = TrafficSpec.CCID;
      if(value != 0) {
         if(ext_setsockopt(socketDescriptor, SOL_DCCP, DCCP_SOCKOPT_CCID, &value, sizeof(value)) < 0) {
            std::cerr << "WARNING: Failed to configure CCID #" << (unsigned int)value
                      << " on DCCP socket (DCCP_SOCKOPT_CCID option) - "
                      << strerror(errno) << "!" << std::endl;
            // return(false);
         }
      }
      const uint32_t service[1] = { htonl(SC_NETPERFMETER_DATA) };
      if(ext_setsockopt(socketDescriptor, SOL_DCCP, DCCP_SOCKOPT_SERVICE, &service, sizeof(service)) < 0) {
        std::cerr << "ERROR: Failed to configure DCCP service code on DCCP socket (DCCP_SOCKOPT_SERVICE option) - "
                  << strerror(errno) << "!" << std::endl;
        return(false);
      }
   }
#endif
   return(true);
}
