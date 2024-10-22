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
#include <signal.h>
#include <netinet/tcp.h>

#include <cstring>
#include <sstream>


// ###### Constructor #######################################################
Flow::Flow(const uint64_t         measurementID,
           const uint32_t         flowID,
           const uint16_t         streamID,
           const FlowTrafficSpec& trafficSpec,
           const int              controlSocketDescriptor)
{
   MeasurementID                 = measurementID;
   FlowID                        = flowID;
   StreamID                      = streamID;

   SocketDescriptor              = -1;
   OriginalSocketDescriptor      = false;
   RemoteControlSocketDescriptor = controlSocketDescriptor;
   RemoteAddressIsValid          = false;

   InputStatus                   = WaitingForStartup;
   OutputStatus                  = WaitingForStartup;
   TimeBase                      = getMicroTime();

   TrafficSpec                   = trafficSpec;

   MyMeasurement                 = nullptr;
   FirstTransmission             = 0;
   LastTransmission              = 0;
   FirstReception                = 0;
   LastReception                 = 0;
   resetStatistics();
   LastOutboundSeqNumber         = ~0;
   LastOutboundFrameID           = ~0;
   NextStatusChangeEvent         = ~0ULL;
   OnOffEventPointer             = 0;

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


// ###### Set reference to measurement ######################################
bool Flow::setMeasurement(Measurement* measurement)
{
   bool success;
   lock();
   if( (MyMeasurement == nullptr) || (measurement == nullptr) ) {
      MyMeasurement = measurement;
      success = true;
   }
   else {
      success = false;
   }
   unlock();
   return success;
}


// ###### Print Flow ########################################################
void Flow::print(std::ostream& os, const bool printStatistics)
{
   std::stringstream ss;

   lock();
   if(TrafficSpec.Protocol != IPPROTO_SCTP) {
      ss << format("+ %s Flow (Flow #%u \"%s\" of Measurement $%llx):",
                   getProtocolName(TrafficSpec.Protocol),
                   FlowID, TrafficSpec.Description.c_str(),
                   MeasurementID) << "\n";
   }
   else {
      ss << format("+ %s Flow (of Measurement $%llx):",
                   getProtocolName(TrafficSpec.Protocol),
                   MeasurementID) << "\n";
   }
   if(TrafficSpec.Protocol == IPPROTO_SCTP) {
      ss << format("   o Stream #%u (Flow ID #%u \"%s\" of Measurement $%llx):",
                   StreamID,
                   FlowID, TrafficSpec.Description.c_str(),
                   MeasurementID) << "\n";
   }
   TrafficSpec.print(ss);


   if(printStatistics) {
      CurrentBandwidthStats.print(ss,
                                  (LastTransmission - FirstTransmission) / 1000000.0,
                                  (LastReception    - FirstReception)    / 1000000.0);
   }
   unlock();

   os << ss.str();
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
      success = VectorFile.printf(
                   "AbsTime RelTime SeqNumber\t"
                   "AbsBytes AbsPackets AbsFrames\t"
                   "RelBytes RelPackets RelFrames\t"
                   "Delay PrevPacketDelayDiff Jitter\n");
      VectorFile.nextLine();
   }
   unlock();

   return success;
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
         "%06llu %llu %1.6f %llu\t"
         "%llu %llu %llu\t"
         "%u %u %u\t"
         "%1.3f %1.3f %1.3f\n",
         VectorFile.nextLine(), now,
         (double)(now - MyMeasurement->getFirstStatisticsEvent()) / 1000000.0,
         seqNumber,
         CurrentBandwidthStats.ReceivedBytes, CurrentBandwidthStats.ReceivedPackets, CurrentBandwidthStats.ReceivedFrames,
         addedBytes, 1, addedFrames,
         delay, delayDiff, jitter);
   }

   unlock();
}


// ###### Start flow's transmission thread ##################################
bool Flow::activate()
{
   deactivate();
   assert(SocketDescriptor >= 0);
   return start();
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
            ext_shutdown(SocketDescriptor, SHUT_RDWR);
         }
      }
      if(!asyncStop) {
         waitForFinish();
         FlowManager::getFlowManager()->getMessageReader()->deregisterSocket(SocketDescriptor);
         PollFDEntry = nullptr;   // Poll FD entry is now invalid!
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

   return nextTransmissionEvent;
}


// ###### Schedule next status change event #################################
unsigned long long Flow::scheduleNextStatusChangeEvent(const unsigned long long now)
{
   lock();

   if(OnOffEventPointer >= TrafficSpec.OnOffEvents.size()) {
      NextStatusChangeEvent = ~0ULL;
      if(TrafficSpec.RepeatOnOff == true) {
         OnOffEventPointer = 0;
      }
   }

   if(OnOffEventPointer < TrafficSpec.OnOffEvents.size()) {
      const OnOffEvent&        event        = TrafficSpec.OnOffEvents[OnOffEventPointer];
      const unsigned long long relNextEvent = (const unsigned long long)rint(1000000.0 * getRandomValue((const double*)&event.ValueArray, event.RandNumGen));
      const unsigned long long absNextEvent = TimeBase + TimeOffset + relNextEvent;

      TimeOffset            = TimeOffset + relNextEvent;
      NextStatusChangeEvent = absNextEvent;

      if((long long)absNextEvent - (long long)now < -10000000) {
         LOG_WARNING
         stdlog << "Schedule is more than 10s behind clock time! Check on/off parameters!" << "\n";
         LOG_END
      }
   }

   unlock();
   return NextStatusChangeEvent;
}


// ###### Status change event ###############################################
void Flow::handleStatusChangeEvent(const unsigned long long now)
{
   lock();
   if(NextStatusChangeEvent <= now) {
      assert(OnOffEventPointer < TrafficSpec.OnOffEvents.size());

      if(OutputStatus == Off) {
         OutputStatus = On;
      }
      else if(OutputStatus == On) {
         OutputStatus = Off;
      }
      else {
         assert(false);
      }

      OnOffEventPointer++;
      scheduleNextStatusChangeEvent(now);
   }
   unlock();
}


// ###### Flow's transmission thread function ###############################
void Flow::run()
{
   signal(SIGPIPE, SIG_IGN);

   scheduleNextStatusChangeEvent(getMicroTime());

   bool result = true;
   do {
      // ====== Schedule next status change event ===========================
      unsigned long long       now              = getMicroTime();
      const unsigned long long nextTransmission = scheduleNextTransmissionEvent();
      unsigned long long       nextEvent        = std::min(NextStatusChangeEvent, nextTransmission);

      // ====== Wait until there is something to do =========================
      if(nextEvent > now) {
         int timeout = pollTimeout(now, 2,
                                   now + 1000000,
                                   nextEvent);
         ext_poll_wrapper(nullptr, 0, timeout);
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
      if(NextStatusChangeEvent <= now) {
         handleStatusChangeEvent(now);
      }
   } while( (result == true) && (!isStopping()) );
}


// ###### Configure socket parameters #######################################
bool Flow::configureSocket(const int socketDescriptor)
{
   if(setBufferSizes(socketDescriptor,
                     (int)TrafficSpec.SndBufferSize,
                     (int)TrafficSpec.RcvBufferSize) == false) {
      return false;
   }
   if( (TrafficSpec.Protocol == IPPROTO_TCP) || (TrafficSpec.Protocol == IPPROTO_MPTCP) ) {
      const int noDelayOption = (TrafficSpec.NoDelay == true) ? 1 : 0;
      if (ext_setsockopt(socketDescriptor, IPPROTO_TCP, TCP_NODELAY, (const char*)&noDelayOption, sizeof(noDelayOption)) < 0) {
         LOG_ERROR
         stdlog << format("Failed to configure Nagle algorithm (TCP_NODELAY option) on TCP socket %d: %s!",
                           socketDescriptor, strerror(errno)) << "\n";
         LOG_END
         return false;
      }

      if(TrafficSpec.Protocol == IPPROTO_MPTCP) {
         // FIXME! Add proper, platform-independent code here!
#ifndef __linux__
#warning MPTCP is currently only available on Linux!
#else

         const int debugOption = (TrafficSpec.Debug == true) ? 1 : 0;
         if (ext_setsockopt(socketDescriptor, IPPROTO_TCP, MPTCP_DEBUG_LEGACY, (const char*)&debugOption, sizeof(debugOption)) < 0) {
            if (ext_setsockopt(socketDescriptor, IPPROTO_TCP, MPTCP_DEBUG, (const char*)&debugOption, sizeof(debugOption)) < 0) {
               LOG_WARNING
               stdlog << format("Failed to configure debugging level %d (MPTCP_DEBUG option) on MPTCP socket %d: %s!",
                                debugOption, socketDescriptor, strerror(errno)) << "\n";
               LOG_END
            }
         }

         const int nDiffPorts = (int)TrafficSpec.NDiffPorts;
         if (ext_setsockopt(socketDescriptor, IPPROTO_TCP, MPTCP_NDIFFPORTS_LEGACY, (const char*)&nDiffPorts, sizeof(nDiffPorts)) < 0) {
            if (ext_setsockopt(socketDescriptor, IPPROTO_TCP, MPTCP_NDIFFPORTS, (const char*)&nDiffPorts, sizeof(nDiffPorts)) < 0) {
               LOG_WARNING
               stdlog << format("Failed to configure number of different ports %d (MPTCP_NDIFFPORTS option) on MPTCP socket %d: %s!",
                                nDiffPorts, socketDescriptor, strerror(errno)) << "\n";
               LOG_END
            }
         }

         const char* pathMgr = TrafficSpec.PathMgr.c_str();
         if (ext_setsockopt(socketDescriptor, IPPROTO_TCP, MPTCP_PATH_MANAGER_LEGACY, pathMgr, strlen(pathMgr)) < 0) {
            if (ext_setsockopt(socketDescriptor, IPPROTO_TCP, MPTCP_PATH_MANAGER, pathMgr, strlen(pathMgr)) < 0) {
               LOG_WARNING
               stdlog << format("Failed to configure path manager %s (MPTCP_PATH_MANAGER option) on MPTCP socket %d: %s!",
                                pathMgr, socketDescriptor, strerror(errno)) << "\n";
               LOG_END
            }
         }

         const char* scheduler = TrafficSpec.Scheduler.c_str();
         if (ext_setsockopt(socketDescriptor, IPPROTO_TCP, MPTCP_SCHEDULER_LEGACY, scheduler, strlen(scheduler)) < 0) {
            if (ext_setsockopt(socketDescriptor, IPPROTO_TCP, MPTCP_SCHEDULER, scheduler, strlen(scheduler)) < 0) {
               LOG_WARNING
               stdlog << format("Failed to configure scheduler %s (MPTCP_SCHEDULER option) on MPTCP socket %d: %s!",
                                scheduler, socketDescriptor, strerror(errno)) << "\n";
               LOG_END
            }
         }
#endif
      }
#ifndef __linux__
#warning Congestion Control selection is currently only available on Linux!
#else
      const char* congestionControl = TrafficSpec.CongestionControl.c_str();
      if(strcmp(congestionControl, "default") != 0) {
         if (ext_setsockopt(socketDescriptor, IPPROTO_TCP, TCP_CONGESTION, congestionControl, strlen(congestionControl)) < 0) {
            LOG_ERROR
            stdlog << format("Failed to configure congestion control %s (TCP_CONGESTION option) on SCTP socket %d: %s!",
                             congestionControl, socketDescriptor, strerror(errno)) << "\n";
            LOG_END
            return false;
         }
      }
#endif
   }
   else if(TrafficSpec.Protocol == IPPROTO_SCTP) {
      if (TrafficSpec.NoDelay) {
         const int noDelayOption = 1;
         if (ext_setsockopt(socketDescriptor, IPPROTO_SCTP, SCTP_NODELAY, (const char*)&noDelayOption, sizeof(noDelayOption)) < 0) {
            LOG_ERROR
            stdlog << format("Failed to configure Nagle algorithm (SCTP_NODELAY option) on SCTP socket %d: %s!",
                             socketDescriptor, strerror(errno)) << "\n";
            LOG_END
            return false;
         }
      }
#ifdef SCTP_CMT_ON_OFF
      struct sctp_assoc_value cmtOnOff;
      cmtOnOff.assoc_id    = 0;
      cmtOnOff.assoc_value = TrafficSpec.CMT;
      if(ext_setsockopt(socketDescriptor, IPPROTO_SCTP, SCTP_CMT_ON_OFF, &cmtOnOff, sizeof(cmtOnOff)) < 0) {
         if(TrafficSpec.CMT != NPAF_PRIMARY_PATH) {
            LOG_ERROR
            stdlog << format("Failed to configure CMT usage (SCTP_CMT_ON_OFF option) on SCTP socket %d: %s!",
                             socketDescriptor, strerror(errno)) << "\n";
            LOG_END
            return false;
         }
      }
#else
      if(TrafficSpec.CMT != NPAF_PRIMARY_PATH) {
         LOG_ERROR
         stdlog << "CMT usage on SCTP socket configured, but not supported by this system!" << "\n";
         LOG_END
         return false;
      }
#endif
   }
#ifdef HAVE_DCCP
   else if(TrafficSpec.Protocol == IPPROTO_DCCP) {
      const uint8_t value = TrafficSpec.CCID;
      if(value != 0) {
         if(ext_setsockopt(socketDescriptor, SOL_DCCP, DCCP_SOCKOPT_CCID, &value, sizeof(value)) < 0) {
            LOG_WARNING
            stdlog << format("Failed to configure CCID %u (DCCP_SOCKOPT_CCID option) on DCCP socket: %s!",
                             (unsigned int)value, socketDescriptor, strerror(errno)) << "\n";
            LOG_END
         }
      }
      const uint32_t service[1] = { htonl(SC_NETPERFMETER_DATA) };
      if(ext_setsockopt(socketDescriptor, SOL_DCCP, DCCP_SOCKOPT_SERVICE, &service, sizeof(service)) < 0) {
         LOG_ERROR
         stdlog << format("Failed to configure DCCP service code %u (DCCP_SOCKOPT_SERVICE option) on DCCP socket: %s!",
                           (unsigned int)ntohl(service[0]), socketDescriptor, strerror(errno)) << "\n";
         LOG_END
         return false;
      }
   }
#endif

   return true;
}
