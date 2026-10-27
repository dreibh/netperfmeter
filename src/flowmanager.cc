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
 *
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

#include "assure.h"
#include "flow.h"
#include "control.h"
#include "loglevel.h"
#include "transfer.h"
#include "tools.h"

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <set>
#include <signal.h>
#include <sstream>


// #define DEBUG_POLL


// Flow Manager Singleton object
FlowManager FlowManager::FlowManagerSingleton;


// ###### Constructor #######################################################
FlowManager::FlowManager()
{
   DisplayEnabled    = false;
   DisplayInterval   = 1000000ULL;
   FirstDisplayEvent = 0;
   LastDisplayEvent  = 0;
   NextDisplayEvent  = 0;

   // ====== Initialize Wakeup Pipe =========================================
   WakeupPipe[0] = -1;
   WakeupPipe[1] = -1;
   if(pipe(WakeupPipe) == 0) {
      for(int i = 0; i < 2; i++) {
         const int flags = fcntl(WakeupPipe[i], F_GETFL, 0);
         if(flags >= 0) {
            fcntl(WakeupPipe[i], F_SETFL, flags | O_NONBLOCK);
         }
         fcntl(WakeupPipe[i], F_SETFD, FD_CLOEXEC);
      }
   }
}


// ###### Destructor ########################################################
FlowManager::~FlowManager()
{
   if(isRunning()) {
      stop();
      waitForFinish();
   }

   // ====== Close Wakeup Pipe ==============================================
   if(WakeupPipe[0] >= 0) {
      ext_close(WakeupPipe[0]);
      WakeupPipe[0] = -1;
   }
   if(WakeupPipe[1] >= 0) {
      ext_close(WakeupPipe[1]);
      WakeupPipe[1] = -1;
   }

   // ====== Clean up measurements ==========================================
   std::map<std::pair<int, uint64_t>, Measurement*>::iterator measurementIterator = MeasurementSet.begin();
   while(measurementIterator != MeasurementSet.end()) {
      Measurement* measurement = measurementIterator->second;
      measurementIterator = MeasurementSet.erase(measurementIterator);
      delete measurement;
   }

   // ====== Clean up flows =================================================
   std::vector<Flow*>::iterator flowIterator = FlowSet.begin();
   while(flowIterator != FlowSet.end()) {
      Flow* flow = *flowIterator;
      if(flow->SocketDescriptor >= 0) {
         Reader.deregisterSocket(flow->SocketDescriptor);
         ext_close(flow->SocketDescriptor);
      }
      flowIterator = FlowSet.erase(flowIterator);
      delete flow;
   }

   // ====== Clean up unidentified sockets ==================================
   std::map<int, UnidentifiedSocket*>::iterator udSocketsIterator = UnidentifiedSockets.begin();
   while(udSocketsIterator != UnidentifiedSockets.end()) {
      UnidentifiedSocket* ud = udSocketsIterator->second;
      udSocketsIterator = UnidentifiedSockets.erase(udSocketsIterator);
      assure(Reader.deregisterSocket(ud->SocketDescriptor) == true);
      ext_close(ud->SocketDescriptor);
      delete ud;
   }
}


// ###### Wake up polling thread ############################################
void FlowManager::wakeup()
{
   if(WakeupPipe[1] >= 0) {
      const char token = 'W';
      const ssize_t written = write(WakeupPipe[1], &token, 1);
      (void)written;
   }
}


// ###### Stop polling thread ###############################################
void FlowManager::stop()
{
   Thread::stop();
   wakeup();
}


// ###### Configure status display ##########################################
void FlowManager::configureDisplay(const bool on)
{
   lock();
   DisplayEnabled = on;
   wakeup();
   unlock();
}


// ###### Add flow ##########################################################
void FlowManager::addFlow(Flow* flow)
{
   if(!isRunning()) {
      start();
   }
   lock();
   flow->PollFDEntry = nullptr;
   FlowSet.push_back(flow);
   wakeup();
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
       iterator != FlowSet.end(); iterator++) {
       if(*iterator == flow) {
          FlowSet.erase(iterator);
          break;
      }
   }

   // ====== Check whether the socket descriptor is still referenced ========
   flow->OriginalSocketDescriptor = true;
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end(); iterator++) {
       if((*iterator)->SocketDescriptor == flow->SocketDescriptor) {
          flow->OriginalSocketDescriptor = false;
          break;
       }
   }

   wakeup();
   unlock();
}


// ###### Print all flows ###################################################
void FlowManager::printFlows(std::ostream& os,
                             const bool    printStatistics)
{
   lock();
   // TRANSLATORS: Header for printing active flows in the FlowManager.
   os << gettext("FlowManager:") << "\n";
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end(); iterator++) {
      Flow* flow = *iterator;
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
       iterator != FlowSet.end(); iterator++) {
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
       iterator != FlowSet.end(); iterator++) {
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
       iterator != FlowSet.end(); iterator++) {
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


// ###### Begin measurement #################################################
bool FlowManager::beginMeasurement(const int        controlSocket,
                                   const uint64_t   measurementID,
                                   const uint64_t   nowMonotonic,
                                   const char*      vectorNamePattern,
                                   const FileFormat vectorFileFormat,
                                   const char*      scalarNamePattern,
                                   const FileFormat scalarFileFormat)
{
   std::stringstream ss;
   bool              success = false;

   lock();
   Measurement* measurement = new Measurement;
   if(measurement != nullptr) {
      try {
         // 1. Initialize measurement-wide scalar and vector files
         measurement->initialize(nowMonotonic, controlSocket, measurementID,
                                 vectorNamePattern, vectorFileFormat,
                                 scalarNamePattern, scalarFileFormat);

         // 2. Initialize per-flow vector files and activate flows
         for(auto* flow : FlowSet) {
            if(flow->MeasurementID == measurementID) {
               if(flow->setMeasurement(measurement)) {
                  if(vectorFileFormat != FileFormat::None) {
                     std::string flowVectorName;
                     if(vectorNamePattern != nullptr && vectorNamePattern[0] != '\0') {
                        // Active node: write directly to target file path
                        flowVectorName = flow->getNodeOutputName(
                           vectorNamePattern, "active",
                           formatText("-{:08x}-{:04x}", flow->FlowID, flow->StreamID));
                     }
                     // Passive node: flowVectorName is empty -> buffers in std::tmpfile()
                     flow->initializeVectorFile(
                        !flowVectorName.empty() ? flowVectorName.c_str() : nullptr,
                        vectorFileFormat);
                  }

                  if(flow->SocketDescriptor >= 0) {
                     flow->TimeBase     = nowMonotonic;
                     flow->TimeOffset   = 0;
                     flow->InputStatus  = Flow::On;
                     flow->OutputStatus = (flow->TrafficSpec.OnOffEvents.size() > 0) ?
                                             Flow::Off : Flow::On;
                     flow->print(ss);
                     flow->activate();
                  }
               }
            }
         }
         success = true;
      }
      catch(const OutputFileException& ex) {
         LOG_ERROR
         stdlog << formatText("ERROR: Failed to prepare output files for measurement ${:x}: {}!\n",
                              measurementID, ex.what());
         LOG_END
         delete measurement;
         measurement = nullptr;
         success = false;
      }
      catch(const std::exception& ex) {
         LOG_ERROR
         stdlog << formatText("ERROR: Failed to prepare measurement ${:x}: {}!\n",
                              measurementID, ex.what());
         LOG_END
         delete measurement;
         measurement = nullptr;
         success = false;
      }
   }
   wakeup();
   unlock();
   CPULoadStats.update();

   if(success) {
      LOG_INFO
      // TRANSLATORS: Status message when preparing measurement. Placeholders: {:x} 1: measurement ID (hex), {} 2: socket descriptor.
      stdlog << formatText(gettext("Prepared measurement ${:x} on socket {}:"),
                           measurementID, controlSocket)
             << "\n" << ss.str();
      LOG_END
   }
   return success;
}


// ###### Finish measurement ################################################
void FlowManager::finishMeasurement(const int      controlSocket,
                                    const uint64_t measurementID)
{
   CPULoadStats.update();

   lock();
   Measurement* measurement = findMeasurement(controlSocket, measurementID);
   if(measurement != nullptr) {
      if(measurement->NextStatisticsEvent != ~0ULL) {
         const MonotonicMicroTime nowMonotonic = getMonotonicMicroTime();
         // 1. Write the final vector statistics while flows are still in FlowSet:
         measurement->writeVectorStatistics(nowMonotonic.value, GlobalStats, RelGlobalStats);
         // 2. Stop the timer, so no subsequent events are generated:
         measurement->NextStatisticsEvent = ~0ULL;
      }
   }
   unlock();

   LOG_INFO
   // TRANSLATORS: Status message when finishing measurement. Placeholders: {:x} 1: measurement ID (hex), {} 2: socket descriptor.
   stdlog << formatText(gettext("Stopped measurement ${:x} on socket {}"),
                        measurementID, controlSocket) << "\n";
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

   const MonotonicMicroTime nowMonotonic = getMonotonicMicroTime();

   // 1. Flush final vector statistics and stop timers before removing flows:
   for(std::map<std::pair<int, uint64_t>, Measurement*>::iterator measurementIterator = MeasurementSet.begin();
       measurementIterator != MeasurementSet.end(); measurementIterator++) {
      Measurement* measurement = measurementIterator->second;
      if(measurement->ControlSocketDescriptor == controlSocket) {
         if(measurement->NextStatisticsEvent != ~0ULL) {
            measurement->writeVectorStatistics(nowMonotonic.value, GlobalStats, RelGlobalStats);
            measurement->NextStatisticsEvent = ~0ULL;
         }
      }
   }

   // 2. Remove and delete the flows:
   std::vector<Flow*>::iterator flowIterator = FlowSet.begin();
   while(flowIterator != FlowSet.end()) {
      Flow* flow = *flowIterator;
      if(flow->getControlSocketDescriptor() == controlSocket) {
         flowIterator = FlowSet.erase(flowIterator);
         delete flow;
      }
      else {
         flowIterator++;
      }
   }

   // Remove and delete the measurements:
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

   wakeup();
   unlock();
}


// ###### Print measurements ################################################
void FlowManager::printMeasurements(std::ostream& os)
{
   // TRANSLATORS: Header for printing active measurements in the FlowManager.
   os << gettext("Measurements:") << "\n";
   for(std::map<std::pair<int, uint64_t>, Measurement*>::iterator iterator = MeasurementSet.begin();
       iterator != MeasurementSet.end(); iterator++) {
      os << "   - " << formatText("{:x} -> ptr={}", iterator->first.second, static_cast<const void*>(iterator->second)) << "\n";
   }
}


// ###### Add socket to flow manager ########################################
void FlowManager::addUnidentifiedSocket(const int protocol,
                                        const int socketDescriptor)
{
   if(!isRunning()) {
      start();
   }

   UnidentifiedSocket* us = new UnidentifiedSocket;
   us->SocketDescriptor = socketDescriptor;
   us->Protocol         = protocol;
   us->PollFDEntry      = nullptr;
   us->ToBeRemoved      = false;
   us->ToBeClosed       = false;

   lock();
   Reader.registerSocket(protocol, socketDescriptor);
   UnidentifiedSockets.insert(std::pair<int, UnidentifiedSocket*>(socketDescriptor, us));
   wakeup();
   unlock();
}


// ###### Remove socket #####################################################
void FlowManager::removeUnidentifiedSocket(const int  socketDescriptor,
                                           const bool closeSocket)
{
   lock();
   std::map<int, UnidentifiedSocket*>::iterator found =
      UnidentifiedSockets.find(socketDescriptor);
   if(found != UnidentifiedSockets.end()) {
      UnidentifiedSocket* us = found->second;
      assure(us->ToBeRemoved == false);
      // Mark socket for removal. The actual remove cannot be done here,
      // since this function may be called in the loop iterating over the
      // UnidentifiedSockets. Removing would invalidate the iterator!
      us->ToBeRemoved = true;
      us->ToBeClosed  = closeSocket;
   }
   wakeup();
   unlock();
}


// ###### Map socket to flow ################################################
Flow* FlowManager::identifySocket(const uint64_t         measurementID,
                                  const uint32_t         flowID,
                                  const uint16_t         streamID,
                                  const int              socketDescriptor,
                                  const sockaddr_union*  from,
                                  bool&                  isAlreadyInitialised)
{
   lock();
   Flow* flow = findFlow(measurementID, flowID, streamID);
   if(flow != nullptr) {
      isAlreadyInitialised = (flow->getSocketDescriptor() >= 0);
      if(!isAlreadyInitialised) {
         flow->lock();
         flow->setSocketDescriptor(socketDescriptor, false,
                                   (flow->getTrafficSpec().Protocol != IPPROTO_UDP));
         flow->RemoteAddress        = *from;
         flow->RemoteAddressIsValid = (from->sa.sa_family != AF_UNSPEC);
         const int protocol         = flow->getTrafficSpec().Protocol;
         flow->unlock();
         if(protocol != IPPROTO_UDP) {
            // Socket is now managed in a Flow!
            removeUnidentifiedSocket(socketDescriptor, false);
         }
      } else {
         // Already initialised -> nothing to do here!
      }
   }
   unlock();

   return flow;
}


// ###### Get next statistics or display events #############################
uint64_t FlowManager::getNextEvent()
{
   lock();
   uint64_t nextEvent = DisplayEnabled ? NextDisplayEvent : ~0ULL;

   for(std::map<std::pair<int, uint64_t>, Measurement*>::iterator iterator = MeasurementSet.begin();
       iterator != MeasurementSet.end(); iterator++) {
       const Measurement* measurement = iterator->second;
       nextEvent = std::min(nextEvent, measurement->NextStatisticsEvent);
   }
   unlock();

   return nextEvent;
}


// ###### Handle statistics and display events ##############################
void FlowManager::handleEvents(const uint64_t nowMonotonic)
{
   lock();

   // ====== Handle statistics events =======================================
   for(std::map<std::pair<int, uint64_t>, Measurement*>::iterator iterator = MeasurementSet.begin();
       iterator != MeasurementSet.end(); iterator++) {
       Measurement* measurement = iterator->second;
       if(measurement->NextStatisticsEvent <= nowMonotonic) {
          measurement->writeVectorStatistics(nowMonotonic, GlobalStats, RelGlobalStats);
       }
   }

   // ====== Display status =================================================
   if(NextDisplayEvent <= nowMonotonic) {
      // ====== Timer management ============================================
      NextDisplayEvent += DisplayInterval;
      if(NextDisplayEvent <= nowMonotonic) {   // Initialization!
         NextDisplayEvent = nowMonotonic + DisplayInterval;
      }
      if(FirstDisplayEvent == 0) {
         FirstDisplayEvent = nowMonotonic;
         LastDisplayEvent  = nowMonotonic;
      }

      // ====== Print bandwidth/CPU information line ========================
      if(DisplayEnabled) {
         static const unsigned int cpuDisplayLimit = 4;
         static const char* colorOff         = "\x1b[0m";
         static const char* colorDuration    = "\x1b[34m";
         static const char* colorFlows       = "\x1b[32m";
         static const char* colorTransmitted = "\x1b[33m";
         static const char* colorReceived    = "\x1b[36m";
         static const char* colorCPU         = "\x1b[0m\x1b[31m";
         static const char* colorCPUHighLoad = "\x1b[31;47;21m";
         const double       duration         = (nowMonotonic - LastDisplayEvent) / 1000000.0;
         const unsigned int totalDuration    = static_cast<unsigned int>((nowMonotonic - FirstDisplayEvent) / 1000000ULL);

         char cpuUtilization[160];
         cpuUtilization[0] = '\0';
         CPUDisplayStats.update();
         if(CPUDisplayStats.getNumberOfCPUs() <= cpuDisplayLimit) {
            for(unsigned int i = 1; i <= CPUDisplayStats.getNumberOfCPUs(); i++) {
               const unsigned int load = static_cast<unsigned int>(rint(10 * CPUDisplayStats.getCpuUtilization(i)));
               char str[48];
               snprintf(str, sizeof(str), "%s%s%3u.%u%%%s",
                        (i > 1) ? "/" : "",
                        (load < 900) ? colorCPU : colorCPUHighLoad,
                        load / 10, load % 10,
                        colorCPU);
               strlcat(cpuUtilization, str, sizeof(cpuUtilization));
            }
         }
         const unsigned int load = static_cast<unsigned int>(rint(10 * CPUDisplayStats.getCpuUtilization(0)));
         char str[48];
         snprintf(str, sizeof(str), "%s%s%3u.%u%%%s%s",
                  (CPUDisplayStats.getNumberOfCPUs() > cpuDisplayLimit) ? "" : " [",
                  (load < 900) ? colorCPU : colorCPUHighLoad,
                  load / 10, load % 10,
                  colorCPU,
                  (CPUDisplayStats.getNumberOfCPUs() > cpuDisplayLimit) ? " total" : "]");
         strlcat(cpuUtilization, str, sizeof(cpuUtilization));

         fprintf(stdout,
                 "\r<-- %s%u:%02u:%02u   "
                 "%sFlows: %u Measurements: %u   "
                 "%sTransmitted: %1.3f MiB at %6.1f Kbit/s   "
                 "%sReceived: %1.3f MiB at %6.1f Kbit/s   "
                 "%sCPU: %s "
                 "%s-->\x1b[0K",
                 colorDuration,
                 totalDuration / 3600,
                 (totalDuration / 60) % 60,
                 totalDuration % 60,
                 colorFlows,
                 static_cast<unsigned int>(FlowSet.size()),
                 static_cast<unsigned int>(MeasurementSet.size()),
                 colorTransmitted,
                 static_cast<double>(GlobalStats.TransmittedBytes) / (1024.0 * 1024.0),
                 (duration > 0.0) ? (8.0 * static_cast<double>(RelGlobalStats.TransmittedBytes) / (1000.0 * duration)) : 0.0,
                 colorReceived,
                 static_cast<double>(GlobalStats.ReceivedBytes) / (1024.0 * 1024.0),
                 (duration > 0.0) ? (8.0 * static_cast<double>(RelGlobalStats.ReceivedBytes) / (1000.0 * duration)) : 0.0,
                 colorCPU,
                 cpuUtilization,
                 colorOff);
         fflush(stdout);
      }

      LastDisplayEvent = nowMonotonic;
      RelGlobalStats.reset();
   }

   unlock();
}


// ###### Write scalars #####################################################
void FlowManager::writeScalarStatistics(const uint64_t measurementID,
                                        const uint64_t nowMonotonic,
                                        OutputFile&    scalarFile,
                                        const uint64_t firstStatisticsEvent)
{
   std::string objectName =
      std::string("netPerfMeter.") +
         ((scalarFile.getName() == std::string()) ? "passive" : "active");

   lock();
   FlowBandwidthStats totalBandwidthStats;
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
      iterator != FlowSet.end(); iterator++) {
      Flow* flow = *iterator;
      flow->lock();
      if(flow->MeasurementID == measurementID) {
         const double transmissionDuration = (flow->LastTransmission - flow->FirstTransmission) / 1000000.0;
         const double receptionDuration    = (flow->LastReception - flow->FirstReception) / 1000000.0;
         scalarFile.printf(
            "scalar \"%s.flow[%u]\" \"First Transmission\"      %" PRIu64 "\n"
            "scalar \"%s.flow[%u]\" \"Last Transmission\"       %" PRIu64 "\n"
            "scalar \"%s.flow[%u]\" \"First Reception\"         %" PRIu64 "\n"
            "scalar \"%s.flow[%u]\" \"Last Reception\"          %" PRIu64 "\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Bytes\"       %" PRIu64 "\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Packets\"     %" PRIu64 "\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Frames\"      %" PRIu64 "\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Bit Rate\"    %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Byte Rate\"   %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Packet Rate\" %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Frame Rate\"  %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Received Bytes\"          %" PRIu64 "\n"
            "scalar \"%s.flow[%u]\" \"Received Packets\"        %" PRIu64 "\n"
            "scalar \"%s.flow[%u]\" \"Received Frames\"         %" PRIu64 "\n"
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
            objectName.c_str(), flow->FlowID, (transmissionDuration > 0.0) ? static_cast<double>(flow->CurrentBandwidthStats.TransmittedBytes)   / transmissionDuration : 0.0,
            objectName.c_str(), flow->FlowID, (transmissionDuration > 0.0) ? static_cast<double>(flow->CurrentBandwidthStats.TransmittedPackets) / transmissionDuration : 0.0,
            objectName.c_str(), flow->FlowID, (transmissionDuration > 0.0) ? static_cast<double>(flow->CurrentBandwidthStats.TransmittedFrames)  / transmissionDuration : 0.0,

            objectName.c_str(), flow->FlowID, flow->CurrentBandwidthStats.ReceivedBytes,
            objectName.c_str(), flow->FlowID, flow->CurrentBandwidthStats.ReceivedPackets,
            objectName.c_str(), flow->FlowID, flow->CurrentBandwidthStats.ReceivedFrames,
            objectName.c_str(), flow->FlowID, (receptionDuration > 0.0) ? 8ULL * flow->CurrentBandwidthStats.ReceivedBytes / receptionDuration : 0.0,
            objectName.c_str(), flow->FlowID, (receptionDuration > 0.0) ? static_cast<double>(flow->CurrentBandwidthStats.ReceivedBytes)   / receptionDuration : 0.0,
            objectName.c_str(), flow->FlowID, (receptionDuration > 0.0) ? static_cast<double>(flow->CurrentBandwidthStats.ReceivedPackets) / receptionDuration : 0.0,
            objectName.c_str(), flow->FlowID, (receptionDuration > 0.0) ? static_cast<double>(flow->CurrentBandwidthStats.ReceivedFrames)  / receptionDuration : 0.0
            );
         totalBandwidthStats = totalBandwidthStats + flow->CurrentBandwidthStats;
      }
      flow->unlock();
   }

   // ====== Write total statistics =========================================
   const double totalDuration = (nowMonotonic - firstStatisticsEvent) / 1000000.0;
   scalarFile.printf(
      "scalar \"%s.total\" \"First Statistics Event\"  %" PRIu64 "\n"
      "scalar \"%s.total\" \"Last Statistics Event\"   %" PRIu64 "\n"
      "scalar \"%s.total\" \"Transmitted Bytes\"       %" PRIu64 "\n"
      "scalar \"%s.total\" \"Transmitted Packets\"     %" PRIu64 "\n"
      "scalar \"%s.total\" \"Transmitted Frames\"      %" PRIu64 "\n"
      "scalar \"%s.total\" \"Transmitted Byte Rate\"   %1.6f\n"
      "scalar \"%s.total\" \"Transmitted Packet Rate\" %1.6f\n"
      "scalar \"%s.total\" \"Transmitted Frame Rate\"  %1.6f\n"
      "scalar \"%s.total\" \"Received Bytes\"          %" PRIu64 "\n"
      "scalar \"%s.total\" \"Received Packets\"        %" PRIu64 "\n"
      "scalar \"%s.total\" \"Received Frames\"         %" PRIu64 "\n"
      "scalar \"%s.total\" \"Received Byte Rate\"      %1.6f\n"
      "scalar \"%s.total\" \"Received Packet Rate\"    %1.6f\n"
      "scalar \"%s.total\" \"Received Frame Rate\"     %1.6f\n"
      "scalar \"%s.total\" \"Lost Bytes\"              %" PRIu64 "\n"
      "scalar \"%s.total\" \"Lost Packets\"            %" PRIu64 "\n"
      "scalar \"%s.total\" \"Lost Frames\"             %" PRIu64 "\n"
      "scalar \"%s.total\" \"Lost Byte Rate\"          %1.6f\n"
      "scalar \"%s.total\" \"Lost Packet Rate\"        %1.6f\n"
      "scalar \"%s.total\" \"Lost Frame Rate\"         %1.6f\n"
      ,
      objectName.c_str(), firstStatisticsEvent,
      objectName.c_str(), nowMonotonic,

      objectName.c_str(), totalBandwidthStats.TransmittedBytes,
      objectName.c_str(), totalBandwidthStats.TransmittedPackets,
      objectName.c_str(), totalBandwidthStats.TransmittedFrames,
      objectName.c_str(), (totalDuration > 0.0) ? static_cast<double>(totalBandwidthStats.TransmittedBytes)   / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? static_cast<double>(totalBandwidthStats.TransmittedPackets) / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? static_cast<double>(totalBandwidthStats.TransmittedFrames)  / totalDuration : 0.0,

      objectName.c_str(), totalBandwidthStats.ReceivedBytes,
      objectName.c_str(), totalBandwidthStats.ReceivedPackets,
      objectName.c_str(), totalBandwidthStats.ReceivedFrames,
      objectName.c_str(), (totalDuration > 0.0) ? static_cast<double>(totalBandwidthStats.ReceivedBytes)   / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? static_cast<double>(totalBandwidthStats.ReceivedPackets) / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? static_cast<double>(totalBandwidthStats.ReceivedFrames)  / totalDuration : 0.0,

      objectName.c_str(), totalBandwidthStats.LostBytes,
      objectName.c_str(), totalBandwidthStats.LostPackets,
      objectName.c_str(), totalBandwidthStats.LostFrames,
      objectName.c_str(), (totalDuration > 0.0) ? static_cast<double>(totalBandwidthStats.LostBytes)   / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? static_cast<double>(totalBandwidthStats.LostPackets) / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? static_cast<double>(totalBandwidthStats.LostFrames)  / totalDuration : 0.0
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
void FlowManager::writeVectorStatistics(const uint64_t      measurementID,
                                        const uint64_t      nowMonotonic,
                                        OutputFile&         vectorFile,
                                        FlowBandwidthStats& globalStats,
                                        FlowBandwidthStats& relGlobalStats,
                                        const uint64_t      firstStatisticsEvent,
                                        const uint64_t      lastStatisticsEvent)
{
   // ====== Write vector statistics header =================================
   if(vectorFile.getLine() == 0) {
      vectorFile.printf("AbsTime\tRelTime\tInterval\t"
                        "FlowID\tDescription\tJitter\t"
                        "Action\t"
                           "AbsBytes\tAbsPackets\tAbsFrames\t"
                           "RelBytes\tRelPackets\tRelFrames\n");
   }

   lock();

   // ====== Write flow statistics ==========================================
   FlowBandwidthStats lastTotalStats;
   FlowBandwidthStats currentTotalStats;
   unsigned int       matchingFlows = 0;
   const double       duration      =
      (nowMonotonic - lastStatisticsEvent) / 1000000.0;

   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end(); iterator++) {
       Flow* flow = *iterator;
       if(flow->MeasurementID == measurementID) {
          matchingFlows++;
          flow->lock();

          const FlowBandwidthStats relStats = flow->CurrentBandwidthStats - flow->LastBandwidthStats;
          lastTotalStats     = lastTotalStats + flow->LastBandwidthStats;
          currentTotalStats  = currentTotalStats + flow->CurrentBandwidthStats;
          CurrentGlobalStats = CurrentGlobalStats + relStats;

          const uint64_t line = static_cast<uint64_t>(vectorFile.getLine());
          vectorFile.printf(
            "%06" PRIu64 "\t%" PRIu64 "\t%1.6f\t%1.6f\t%u\t\"%s\"\t%1.3f\t"
               "\"Sent\"\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\n"
            "%06" PRIu64 "\t%" PRIu64 "\t%1.6f\t%1.6f\t%u\t\"%s\"\t%1.3f\t"
               "\"Received\"\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\n"
            "%06" PRIu64 "\t%" PRIu64 "\t%1.6f\t%1.6f\t%u\t\"%s\"\t%1.3f\t"
               "\"Lost\"\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\n",

            line + 1, nowMonotonic, (double)(nowMonotonic - firstStatisticsEvent) / 1000000.0, duration,
               flow->FlowID, flow->TrafficSpec.Description.c_str(), flow->Jitter,
               flow->CurrentBandwidthStats.TransmittedBytes,
               flow->CurrentBandwidthStats.TransmittedPackets,
               flow->CurrentBandwidthStats.TransmittedFrames,
               relStats.TransmittedBytes,
               relStats.TransmittedPackets,
               relStats.TransmittedFrames,

            line + 2, nowMonotonic, (double)(nowMonotonic - firstStatisticsEvent) / 1000000.0, duration,
               flow->FlowID, flow->TrafficSpec.Description.c_str(), flow->Jitter,
               flow->CurrentBandwidthStats.ReceivedBytes,
               flow->CurrentBandwidthStats.ReceivedPackets,
               flow->CurrentBandwidthStats.ReceivedFrames,
               relStats.ReceivedBytes,
               relStats.ReceivedPackets,
               relStats.ReceivedFrames,

            line + 3, nowMonotonic, (double)(nowMonotonic - firstStatisticsEvent) / 1000000.0, duration,
               flow->FlowID, flow->TrafficSpec.Description.c_str(), flow->Jitter,
               flow->CurrentBandwidthStats.LostBytes,
               flow->CurrentBandwidthStats.LostPackets,
               flow->CurrentBandwidthStats.LostFrames,
               relStats.LostBytes,
               relStats.LostPackets,
               relStats.LostFrames);

          vectorFile.nextLine(); vectorFile.nextLine(); vectorFile.nextLine();

          flow->LastBandwidthStats = flow->CurrentBandwidthStats;
          flow->unlock();
       }
   }

   // ====== Write total statistics =========================================
   if(matchingFlows > 0) {
      const uint64_t line = static_cast<uint64_t>(vectorFile.getLine());
      const FlowBandwidthStats relTotalStats = currentTotalStats - lastTotalStats;
      vectorFile.printf(
         "%06" PRIu64 "\t%" PRIu64 "\t%1.6f\t%1.6f\t-1\t\"Total\"\t0\t"
            "\"Sent\"\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\n"
         "%06" PRIu64 "\t%" PRIu64 "\t%1.6f\t%1.6f\t-1\t\"Total\"\t0\t"
            "\"Received\"\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\n"
         "%06" PRIu64 "\t%" PRIu64 "\t%1.6f\t%1.6f\t-1\t\"Total\"\t0\t"
            "\"Lost\"\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\n",

         line + 1, nowMonotonic, (double)(nowMonotonic - firstStatisticsEvent) / 1000000.0, duration,
            currentTotalStats.TransmittedBytes,
            currentTotalStats.TransmittedPackets,
            currentTotalStats.TransmittedFrames,
            relTotalStats.TransmittedBytes,
            relTotalStats.TransmittedPackets,
            relTotalStats.TransmittedFrames,

         line + 2, nowMonotonic, (double)(nowMonotonic - firstStatisticsEvent) / 1000000.0, duration,
            currentTotalStats.ReceivedBytes,
            currentTotalStats.ReceivedPackets,
            currentTotalStats.ReceivedFrames,
            relTotalStats.ReceivedBytes,
            relTotalStats.ReceivedPackets,
            relTotalStats.ReceivedFrames,

         line + 3, nowMonotonic, (double)(nowMonotonic - firstStatisticsEvent) / 1000000.0, duration,
            currentTotalStats.LostBytes,
            currentTotalStats.LostPackets,
            currentTotalStats.LostFrames,
            relTotalStats.LostBytes,
            relTotalStats.LostPackets,
            relTotalStats.LostFrames);

      vectorFile.nextLine(); vectorFile.nextLine(); vectorFile.nextLine();

      // ====== Return global values (all measurements) for displaying ======
      globalStats     = CurrentGlobalStats;
      relGlobalStats  = CurrentGlobalStats - LastGlobalStats;

      LastGlobalStats = CurrentGlobalStats;
   }

   unlock();
}


// ###### Reception thread function #########################################
void FlowManager::run()
{
   signal(SIGPIPE, SIG_IGN);

   do {
      // ====== Collect PollFDs for poll() ==================================
      lock();

      // Allocate space for flows + unidentified sockets + 1 wakeup pipe
      pollfd       pollFDs[FlowSet.size() + UnidentifiedSockets.size() + 1];
      unsigned int n = 0;
#if defined(DEBUG_POLL)
      std::cerr << "poll-init:\n";
#endif

      // ------ Register Wakeup Pipe ----------------------------------------
      pollfd* wakeupPollFD = nullptr;
      if(WakeupPipe[0] >= 0) {
         pollFDs[n].fd      = WakeupPipe[0];
         pollFDs[n].events  = POLLIN;
         pollFDs[n].revents = 0;
         wakeupPollFD       = &pollFDs[n];
         n++;
      }

      // ------ Get socket descriptors for flows ----------------------------
      for(unsigned int i = 0; i < FlowSet.size(); i++) {
         FlowSet[i]->lock();
         FlowSet[i]->PollFDEntry = nullptr;
         if( (FlowSet[i]->InputStatus != Flow::Off) &&
             (FlowSet[i]->SocketDescriptor >= 0) ) {
            if(!((FlowSet[i]->getTrafficSpec().Protocol == IPPROTO_UDP) &&
                 (FlowSet[i]->isAcceptedIncomingFlow())) ) {

               // Check if the descriptor was already added to pollFDs:
               bool alreadyAdded = false;
               for(unsigned int j = 0; j < n; j++) {
                  if(pollFDs[j].fd == FlowSet[i]->SocketDescriptor) {
                     alreadyAdded = true;
                     break;
                  }
               }

               if(!alreadyAdded) {
                  pollFDs[n].fd      = FlowSet[i]->SocketDescriptor;
                  pollFDs[n].events  = POLLIN;
                  pollFDs[n].revents = 0;
                  FlowSet[i]->PollFDEntry = &pollFDs[n];
                  n++;
               }
            }
         }
         FlowSet[i]->unlock();
      }

      // ------ Get socket descriptors for yet unidentified associations ----
      for(std::map<int, UnidentifiedSocket*>::iterator iterator = UnidentifiedSockets.begin();
          iterator != UnidentifiedSockets.end(); iterator++) {
         UnidentifiedSocket* ud = iterator->second;
         ud->PollFDEntry = nullptr;
         if(ud->Protocol != IPPROTO_UDP) {
            pollFDs[n].fd      = ud->SocketDescriptor;
            pollFDs[n].events  = POLLIN;
            pollFDs[n].revents = 0;
            ud->PollFDEntry    = &pollFDs[n];
#if defined(DEBUG_POLL)
            std::cerr << "\t" << n << ": unidentified socket "
                      << ud->SocketDescriptor
                      << " protocol " << ud->Protocol << "\n";
#endif
            n++;
         }
      }

      const uint64_t nextEvent = getNextEvent();
      unlock();

      // ====== Wait for events =============================================
      // No more maxIdleTimeout! Sleep indefinitely (-1) if no timers are scheduled,
      // or sleep until the exact nextEvent timestamp.
      MonotonicMicroTime nowMonotonic = getMonotonicMicroTime();
      int timeout = -1;
      if(nextEvent != ~0ULL) {
         timeout = pollTimeout(nowMonotonic.value, 2, nextEvent, nextEvent);
      }

#if defined(DEBUG_POLL)
      std::cerr << "poll with timeout=" << timeout << " ms, n=" << n << "\n";
#endif
      const int result = ext_poll((pollfd*)&pollFDs, n, timeout);
#if defined(DEBUG_POLL)
      std::cerr << "poll result=" << result << "\n";
#endif

      // ====== Handle events ===============================================
      lock();

      nowMonotonic                    = getMonotonicMicroTime();
      const SystemMicroTime nowSystem = getSystemMicroTime();
      if(result > 0) {
         // ------ Drain wakeup pipe ----------------------------------------
         if(wakeupPollFD && (wakeupPollFD->revents & (POLLIN | POLLERR))) {
            char buffer[64];
            while(read(WakeupPipe[0], buffer, sizeof(buffer)) > 0) {
               // Drained
            }
         }

         // ====== Handle read events of flows ==============================
         for(unsigned int i = 0; i < FlowSet.size(); i++) {
            FlowSet[i]->lock();
            const pollfd* entry = FlowSet[i]->PollFDEntry;
            if(entry) {

               // ====== Handle data message ================================
               if(entry->revents & (POLLIN|POLLERR)) {
                  const int protocol = FlowSet[i]->getTrafficSpec().Protocol;
#if defined(DEBUG_POLL)
                  std::cerr << "\tPOLLIN: flow " << FlowSet[i]->SocketDescriptor
                            << " protocol " << FlowSet[i]->getTrafficSpec().Protocol << "\n";
#endif
                  // NOTE: FlowSet[i] may not be the actual Flow!
                  //       It may be another stream of the same SCTP assoc!
                  const bool dataOkay = handleNetPerfMeterData(true, nowMonotonic, nowSystem,
                                                               protocol, entry->fd);
                  if(!dataOkay) {
                     // Close the broken connection!
                     LOG_WARNING
                     // TRANSLATORS: Warning message when closing a broken socket. Placeholder: {} is socket descriptor.
                     stdlog << formatText(gettext("Closing disconnected socket {}!"),
                                          FlowSet[i]->SocketDescriptor) << "\n";
                     LOG_END
                     const bool deregisteredFromReader =
                        Reader.deregisterSocket(FlowSet[i]->SocketDescriptor);
                     if( (protocol != IPPROTO_SCTP)
#if defined(HAVE_QUIC)
                         && (protocol != IPPROTO_QUIC)
#endif
                       ) {
                        // Only SCTP and QUIC have streams. For other protocols
                        // there is just one flow using this socket. Then, it
                        // cannot be referenced any more.
                        assure(deregisteredFromReader);
                     }
                     if(deregisteredFromReader) {
                        ext_close(FlowSet[i]->SocketDescriptor);
                        FlowSet[i]->SocketDescriptor = -1;
                     }
                  }
               }
            }
            FlowSet[i]->unlock();
         }

         // ====== Handle read events of yet unidentified sockets ===========
         // Iterate in while loop, for removals and necessary iterator updates!
         std::map<int, UnidentifiedSocket*>::iterator iterator = UnidentifiedSockets.begin();
         while(iterator != UnidentifiedSockets.end()) {
            UnidentifiedSocket* ud = iterator->second;
            // See note above about UDP: UDP is handled by mainLoop()!
            if(ud->Protocol != IPPROTO_UDP) {
               const pollfd* entry = ud->PollFDEntry;
               if(entry) {

                  // ====== Handle data message =============================
                  if( (ud->ToBeRemoved == false) &&
                      (entry->revents & (POLLIN|POLLERR)) ) {
#if defined(DEBUG_POLL)
                     std::cerr << "\tPOLLIN: unidentified " << ud->SocketDescriptor
                                 << " protocol " << ud->Protocol << "\n";
#endif
                     const bool dataOkay = handleNetPerfMeterData(true, nowMonotonic, nowSystem,
                                                                  ud->Protocol,
                                                                  ud->SocketDescriptor);
                     if(!dataOkay) {
                        // Incoming connection has already been closed -> remove it!
                        LOG_WARNING
                        // TRANSLATORS: Warning message when closing an unidentified incoming socket that failed handshake. Placeholder: {} is socket descriptor.
                        stdlog << formatText(gettext("Shutdown of still unidentified incoming connection on socket {}!"),
                                             ud->SocketDescriptor) << "\n";
                        LOG_END
                        ud->ToBeRemoved = true;   // Mark for removal
                        ud->ToBeClosed  = true;
                     }
                  }

                  // ====== Check removal from UnidentifiedSockets ==========
                  if(ud->ToBeRemoved == true) {
                     // Remove the socket marked by removeUnidentifiedSocket(),
                     // or by ud->ToBeRemoved = true above:
                     // However, since we are in an interation loop, we
                     // must ensure that the iterator remains valid!
                     iterator = UnidentifiedSockets.erase(iterator);
                     const bool deregisteredFromReader =
                        Reader.deregisterSocket(ud->SocketDescriptor);
                     if(ud->ToBeClosed) {
                        assure(deregisteredFromReader);
                        ext_close(ud->SocketDescriptor);
                     }
                     delete ud;
                     continue;   // Next iteration, the iterator is already correct!
                  }
               }
            }
            iterator++;
         }
      }

      // ====== Handle statistics timer =====================================
      if(getNextEvent() <= nowMonotonic.value) {
         handleEvents(nowMonotonic.value);
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
         // TRANSLATORS: Error message when setsockopt(SO_SNDBUF) fails. Placeholders: {} 1: requested buffer size, 2: socket descriptor, 3: strerror() message.
         stdlog << formatText(gettext("Unable to configure send buffer size {} (SO_SNDBUF option) on socket {}: {}!"),
                              sndBufSize, sd, strerror(errno)) << "\n";
         LOG_END
         return false;
      }
      int       newBufferSize       = 0;
      socklen_t newBufferSizeLength = sizeof(newBufferSize);
      if(ext_getsockopt(sd, SOL_SOCKET, SO_SNDBUF, &newBufferSize, &newBufferSizeLength) < 0) {
         LOG_ERROR
         // TRANSLATORS: Error message when getsockopt(SO_SNDBUF) fails. Placeholders: {} 1: socket descriptor, 2: strerror() message.
         stdlog << formatText(gettext("Unable to obtain send buffer size (SO_SNDBUF option) on socket {}: {}!"),
                              sd, strerror(errno)) << "\n";
         LOG_END
         return false;
      }
      if(newBufferSize < sndBufSize) {
         LOG_ERROR
         // TRANSLATORS: Error message when actual buffer size is less than configured. Placeholders: {} 1: actual size, 2: requested size, 3: socket descriptor.
         stdlog << formatText(gettext("Actual send buffer size {} < configured send buffer size {} on socket {}!"),
                              newBufferSize, sndBufSize, sd) << "\n";
         LOG_END
         return false;
      }
   }

   if(rcvBufSize > 0) {
      if(ext_setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &rcvBufSize, sizeof(rcvBufSize)) < 0) {
         LOG_ERROR
         // TRANSLATORS: Error message when setsockopt(SO_RCVBUF) fails. Placeholders: {} 1: requested buffer size, 2: socket descriptor, 3: strerror() message.
         stdlog << formatText(gettext("Unable to configure receive buffer size {} (SO_RCVBUF option) on socket {}: {}!"),
                              rcvBufSize, sd, strerror(errno)) << "\n";
         LOG_END
         return false;
      }
      int       newBufferSize       = 0;
      socklen_t newBufferSizeLength = sizeof(newBufferSize);
      if(ext_getsockopt(sd, SOL_SOCKET, SO_RCVBUF, &newBufferSize, &newBufferSizeLength) < 0) {
         LOG_ERROR
         // TRANSLATORS: Error message when getsockopt(SO_RCVBUF) fails. Placeholders: {} 1: socket descriptor, 2: strerror() message.
         stdlog << formatText(gettext("Unable to obtain receive buffer size (SO_RCVBUF option) on socket {}: {}!"),
                              sd, strerror(errno)) << "\n";
         LOG_END
         return false;
      }
      if(newBufferSize < rcvBufSize) {
         LOG_ERROR
         // TRANSLATORS: Error message when actual buffer size is less than configured. Placeholders: {} 1: actual size, 2: requested size, 3: socket descriptor.
         stdlog << formatText(gettext("Actual receive buffer size {} < configured receive buffer size {} on socket {}!"),
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
         // TRANSLATORS: Warning message when setsockopt(SO_REUSEADDR) fails. Placeholders: {} 1: socket descriptor, 2: strerror() message.
         stdlog << formatText(gettext("Unable to configure socket reuse (SO_REUSEADDR option) on socket {}: {}!"),
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
