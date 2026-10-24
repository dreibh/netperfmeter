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
#include <cmath>
#include <cstdint>
#include <cstring>
#include <netinet/tcp.h>
#include <signal.h>
#include <sstream>


// ###### Constructor #######################################################
Flow::Flow(const uint64_t         measurementID,
           const uint32_t         flowID,
           const uint16_t         streamID,
           const FlowTrafficSpec& trafficSpec,
           const int              controlSocketDescriptor)
{
   MeasurementID            = measurementID;
   FlowID                   = flowID;
   StreamID                 = streamID;

   SocketDescriptor         = -1;
   OriginalSocketDescriptor = false;
   ControlSocketDescriptor  = controlSocketDescriptor;
   RemoteAddressIsValid     = false;

   InputStatus              = WaitingForStartup;
   OutputStatus             = WaitingForStartup;
   TimeBase                 = getMicroTime();

   TrafficSpec              = trafficSpec;

   MyMeasurement            = nullptr;
   FirstTransmission        = 0;
   LastTransmission         = 0;
   FirstReception           = 0;
   LastReception            = 0;
   resetStatistics();
   LastOutboundSeqNumber    = static_cast<uint64_t>(-1);
   LastOutboundFrameID      = static_cast<uint32_t>(-1);
   NextStatusChangeEvent    = static_cast<uint64_t>(-1);
   OnOffEventPointer        = 0;

   FlowManager::getFlowManager()->addFlow(this);
}


// ###### Destructor ########################################################
Flow::~Flow()
{
   FlowManager::getFlowManager()->removeFlow(this);
   deactivate();
   try {
      VectorFile.finish(true);
   }
   catch(...) {
   }
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
   Jitter = 0.0;
   Delay  = 0.0;
   unlock();
}


// ###### Set reference to measurement ######################################
bool Flow::setMeasurement(Measurement* measurement)
{
   bool success;
   lock();
   if((MyMeasurement == nullptr) || (measurement == nullptr)) {
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
      // TRANSLATORS: Flow summary header. Placeholders: {} 1: protocol name, 2: flow ID, 3: flow description, {:x} 4: measurement ID (hex).
      ss << formatText(gettext("+ {} Flow (Flow #{} \"{}\" of Measurement ${:x}):"),
                       getProtocolName(TrafficSpec.Protocol),
                       FlowID, TrafficSpec.Description,
                       MeasurementID) << "\n";
   }
   else {
      // TRANSLATORS: SCTP association flow header. Placeholders: {} 1: protocol name, {:x} 2: measurement ID (hex).
      ss << formatText(gettext("+ {} Flow (of Measurement ${:x}):"),
                       getProtocolName(TrafficSpec.Protocol),
                       MeasurementID) << "\n";
   }
   if(TrafficSpec.Protocol == IPPROTO_SCTP) {
      // TRANSLATORS: SCTP stream description inside an association. Placeholders: {} 1: stream ID, 2: flow ID, 3: flow description, {:x} 4: measurement ID (hex).
      ss << formatText(gettext("   o Stream #{} (Flow ID #{} \"{}\" of Measurement ${:x}):"),
                       StreamID,
                       FlowID, TrafficSpec.Description,
                       MeasurementID) << "\n";
   }
   TrafficSpec.print(ss);

   if(printStatistics) {
      CurrentBandwidthStats.print(ss,
                                  std::chrono::duration<double>(std::chrono::microseconds(LastTransmission - FirstTransmission)).count(),
                                  std::chrono::duration<double>(std::chrono::microseconds(LastReception - FirstReception)).count());
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
   assure(SocketDescriptor == -1);
   SocketDescriptor         = socketDescriptor;
   OriginalSocketDescriptor = originalSocketDescriptor;
   DeleteWhenFinished       = deleteWhenFinished;

   if(SocketDescriptor >= 0) {
      FlowManager::getFlowManager()->getMessageReader()->registerSocket(
         TrafficSpec.Protocol, SocketDescriptor, false);
   }
   unlock();
}


// ###### Initialize flow's vector file #####################################
void Flow::initializeVectorFile(const char* name, const FileFormat format)
{
   lock();
   try {
      VectorFile.initialize(name, format);
      VectorFile.printf(
         "AbsTime\tRelTime\tSeqNumber\t"
         "AbsBytes\tAbsPackets\tAbsFrames\t"
         "RelBytes\tRelPackets\tRelFrames\t"
         "Delay\tPrevPacketDelayDiff\tJitter\n");
      VectorFile.nextLine();
   }
   catch(...) {
      unlock();
      throw;
   }
   unlock();
}


// ###### Update transmission statistics ####################################
void Flow::updateTransmissionStatistics(const uint64_t          now,
                                        const unsigned long long addedFrames,
                                        const unsigned long long addedPackets,
                                        const unsigned long long addedBytes)
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
void Flow::updateReceptionStatistics(const uint64_t          now,
                                     const unsigned long long addedFrames,
                                     const unsigned long long addedBytes,
                                     const unsigned long long lostFrames,
                                     const unsigned long long lostPackets,
                                     const unsigned long long lostBytes,
                                     const uint64_t          seqNumber,
                                     const double            delay,
                                     const double            delayDiff,
                                     const double            jitter)
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
   if((MyMeasurement) && (MyMeasurement->getFirstStatisticsEvent() > 0)) {
      VectorFile.printf(
         "%06llu\t%llu\t%1.6f\t%llu\t"
         "%llu\t%llu\t%llu\t"
         "%llu\t%u\t%llu\t"
         "%1.3f\t%1.3f\t%1.3f\n",
         VectorFile.nextLine(),
         static_cast<unsigned long long>(now),
         std::chrono::duration<double>(std::chrono::microseconds(now - MyMeasurement->getFirstStatisticsEvent())).count(),
         static_cast<unsigned long long>(seqNumber),
         static_cast<unsigned long long>(CurrentBandwidthStats.ReceivedBytes),
         static_cast<unsigned long long>(CurrentBandwidthStats.ReceivedPackets),
         static_cast<unsigned long long>(CurrentBandwidthStats.ReceivedFrames),
         addedBytes, 1u, addedFrames,
         delay, delayDiff, jitter);
   }

   unlock();
}


// ###### Start flow's transmission thread ##################################
bool Flow::activate()
{
   deactivate();
   assure(SocketDescriptor >= 0);
   return start();
}


// ###### Stop flow's transmission thread ###################################
void Flow::deactivate(const bool asyncStop)
{
   if(isRunning()) {
      lock();
      InputStatus  = Off;
      OutputStatus = Off;
      PollFDEntry  = nullptr;   // Poll FD entry is now invalid!
      unlock();
      if(SocketDescriptor >= 0) {
         if(TrafficSpec.Protocol == IPPROTO_UDP) {
            // NOTE: There is only one UDP socket. We cannot close it here!
            // The thread will notice the need to finish after the poll()
            // timeout.
         }
         else {
            ext_shutdown(SocketDescriptor, (TrafficSpec.Protocol == IPPROTO_TCP) ? SHUT_WR : SHUT_RDWR);
         }
      }
      stop();
      if(!asyncStop) {
         waitForFinish();
         FlowManager::getFlowManager()->getMessageReader()->deregisterSocket(SocketDescriptor);
      }
   }
}


// ###### Schedule next transmission event (non-saturated sender) ###########
uint64_t Flow::scheduleNextTransmissionEvent()
{
   uint64_t nextTransmissionEvent = static_cast<uint64_t>(-1);   // No transmission

   lock();
   if(OutputStatus == On) {
      // ====== Saturated sender ============================================
      if((TrafficSpec.OutboundFrameSize[0] > 0.0) && (TrafficSpec.OutboundFrameRate[0] <= 0.0000001)) {
         nextTransmissionEvent = 0;
      }
      // ====== Non-saturated sender ========================================
      else if((TrafficSpec.OutboundFrameSize[0] > 0.0) && (TrafficSpec.OutboundFrameRate[0] > 0.0000001)) {
         const double nextFrameRate = getRandomValue(static_cast<const double*>(TrafficSpec.OutboundFrameRate),
                                                     TrafficSpec.OutboundFrameRateRng);
         nextTransmissionEvent = LastTransmission + static_cast<uint64_t>(rint(1000000.0 / nextFrameRate));
      }
   }
   unlock();

   return nextTransmissionEvent;
}


// ###### Schedule next status change event #################################
uint64_t Flow::scheduleNextStatusChangeEvent(const uint64_t now)
{
   lock();

   if(OnOffEventPointer >= TrafficSpec.OnOffEvents.size()) {
      NextStatusChangeEvent = static_cast<uint64_t>(-1);
      if(TrafficSpec.RepeatOnOff == true) {
         OnOffEventPointer = 0;
      }
   }

   if(OnOffEventPointer < TrafficSpec.OnOffEvents.size()) {
      const OnOffEvent& event        = TrafficSpec.OnOffEvents[OnOffEventPointer];
      const uint64_t    relNextEvent = static_cast<uint64_t>(rint(1000000.0 * getRandomValue(static_cast<const double*>(event.ValueArray), event.RandNumGen)));
      const uint64_t    absNextEvent = TimeBase + TimeOffset + relNextEvent;

      TimeOffset            = TimeOffset + relNextEvent;
      NextStatusChangeEvent = absNextEvent;

      if(static_cast<int64_t>(absNextEvent) - static_cast<int64_t>(now) < -10000000) {
         LOG_WARNING
         // TRANSLATORS: Warning when event scheduling falls significantly behind real time.
         stdlog << gettext("Schedule is more than 10s behind clock time! Check on/off parameters!") << "\n";
         LOG_END
      }
   }

   unlock();
   return NextStatusChangeEvent;
}


// ###### Status change event ###############################################
void Flow::handleStatusChangeEvent(const uint64_t now)
{
   lock();
   if(NextStatusChangeEvent <= now) {
      assure(OnOffEventPointer < TrafficSpec.OnOffEvents.size());

      if(OutputStatus == Off) {
         OutputStatus = On;
      }
      else if(OutputStatus == On) {
         OutputStatus = Off;
      }
      else {
         abort();
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
      uint64_t       now              = getMicroTime();
      const uint64_t nextTransmission = scheduleNextTransmissionEvent();
      uint64_t       nextEvent        = std::min(NextStatusChangeEvent, nextTransmission);

      // ====== Wait until there is something to do =========================
      if(nextEvent > now) {
         int timeout = pollTimeout(now, 2,
                                   now + 1000000ULL,
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
         if(TrafficSpec.outgoingFlowIsSaturated()) {
            result = transmitFrame(this, now);
         }

         // ====== Outgoing data (non-saturated sender) =====================
         else if(TrafficSpec.outgoingFlowIsNotSaturated()) {
            const uint64_t lastEvent = LastTransmission;
            if(nextTransmission <= now) {
               do {
                  result = transmitFrame(this, now);
                  if(now - lastEvent > 1000000ULL) {
                     // Time gap of more than 1s -> do not try to correct.
                     // But the next transmission needs to be scheduled!
                     scheduleNextTransmissionEvent();
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
   } while((result == true) && (!isStopping()));
}


// ###### Configure socket parameters #######################################
bool Flow::configureSocket(const int socketDescriptor)
{
   if(!setBufferSizes(socketDescriptor,
                      static_cast<int>(TrafficSpec.SndBufferSize),
                      static_cast<int>(TrafficSpec.RcvBufferSize))) {
      return false;
   }
#if defined(HAVE_MPTCP)
   if((TrafficSpec.Protocol == IPPROTO_TCP) || (TrafficSpec.Protocol == IPPROTO_MPTCP)) {
#else
   if(TrafficSpec.Protocol == IPPROTO_TCP) {
#endif
      const int noDelayOption = (TrafficSpec.NoDelay == true) ? 1 : 0;
      if(ext_setsockopt(socketDescriptor, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelayOption), sizeof(noDelayOption)) < 0) {
         LOG_ERROR
         // TRANSLATORS: Error message when setsockopt(TCP_NODELAY) fails. Placeholders: {} 1: socket fd, 2: strerror() message.
         stdlog << formatText(gettext("Unable to configure Nagle algorithm (TCP_NODELAY option) on TCP socket {}: {}!"),
                              socketDescriptor, strerror(errno)) << "\n";
         LOG_END
         return false;
      }

      const char* congestionControl = TrafficSpec.CongestionControl.c_str();
      if(strcmp(congestionControl, "default") != 0) {
#if defined(TCP_CONGESTION)
         if(ext_setsockopt(socketDescriptor, IPPROTO_TCP, TCP_CONGESTION, congestionControl, strlen(congestionControl)) < 0) {
            LOG_ERROR
            // TRANSLATORS: Error message when setsockopt(TCP_CONGESTION) fails. Placeholders: {} 1: congestion control algorithm name, 2: socket fd, 3: strerror() message.
            stdlog << formatText(gettext("Unable to configure congestion control {} (TCP_CONGESTION option) on TCP socket {}: {}!"),
                                 congestionControl, socketDescriptor, strerror(errno)) << "\n";
            LOG_END
            return false;
         }
#else
#warning TCP option TCP_CONGESTION is not supported by the API of this system!
         LOG_ERROR
         // TRANSLATORS: Error message when congestion control configuration is unsupported on TCP socket. Placeholders: {} 1: congestion control algorithm name, 2: socket fd.
         stdlog << formatText(gettext("Congestion control configuration to {} is not supported by TCP socket {}!"),
                              congestionControl, socketDescriptor) << "\n";
         LOG_END
         return false;
#endif
      }
   }
#if defined(HAVE_SCTP)
   else if(TrafficSpec.Protocol == IPPROTO_SCTP) {
      if(TrafficSpec.NoDelay) {
         const int noDelayOption = 1;
         if(ext_setsockopt(socketDescriptor, IPPROTO_SCTP, SCTP_NODELAY, reinterpret_cast<const char*>(&noDelayOption), sizeof(noDelayOption)) < 0) {
            LOG_ERROR
            // TRANSLATORS: Error message when setsockopt(SCTP_NODELAY) fails. Placeholders: {} 1: socket fd, 2: strerror() message.
            stdlog << formatText(gettext("Unable to configure Nagle algorithm (SCTP_NODELAY option) on SCTP socket {}: {}!"),
                                 socketDescriptor, strerror(errno)) << "\n";
            LOG_END
            return false;
         }
      }
#if defined(SCTP_CMT_ON_OFF)
      struct sctp_assoc_value cmtOnOff;
      cmtOnOff.assoc_id    = 0;
      cmtOnOff.assoc_value = TrafficSpec.CMT;
      if(ext_setsockopt(socketDescriptor, IPPROTO_SCTP, SCTP_CMT_ON_OFF, &cmtOnOff, sizeof(cmtOnOff)) < 0) {
         if(TrafficSpec.CMT != NPAF_PRIMARY_PATH) {
            LOG_ERROR
            // TRANSLATORS: Error message when setsockopt(SCTP_CMT_ON_OFF) fails. Placeholders: {} 1: socket fd, 2: strerror() message.
            stdlog << formatText(gettext("Unable to configure CMT usage (SCTP_CMT_ON_OFF option) on SCTP socket {}: {}!"),
                                 socketDescriptor, strerror(errno)) << "\n";
            LOG_END
            return false;
         }
      }
#else
      if(TrafficSpec.CMT != NPAF_PRIMARY_PATH) {
         LOG_ERROR
         // TRANSLATORS: Error message when CMT is requested on SCTP but unsupported by the system.
         stdlog << gettext("CMT usage on SCTP socket configured, but not supported by this system!") << "\n";
         LOG_END
         return false;
      }
#endif
   }
#endif
#if defined(HAVE_DCCP)
   else if(TrafficSpec.Protocol == IPPROTO_DCCP) {
      const uint8_t value = TrafficSpec.CCID;
      if(value != 0) {
         if(ext_setsockopt(socketDescriptor, IPPROTO_DCCP, DCCP_SOCKOPT_CCID, &value, sizeof(value)) < 0) {
            LOG_WARNING
            // TRANSLATORS: Warning message when setsockopt(DCCP_SOCKOPT_CCID) fails. Placeholders: {} 1: CCID number, 2: socket fd, 3: strerror() message.
            stdlog << formatText(gettext("Unable to configure CCID {} (DCCP_SOCKOPT_CCID option) on DCCP socket {}: {}!"),
                                 static_cast<unsigned int>(value), socketDescriptor, strerror(errno)) << "\n";
            LOG_END
         }
      }
      const uint32_t service[1] = { htobe32(SC_NETPERFMETER_DATA) };
      if(ext_setsockopt(socketDescriptor, IPPROTO_DCCP, DCCP_SOCKOPT_SERVICE, &service, sizeof(service)) < 0) {
         LOG_ERROR
         // TRANSLATORS: Error message when setsockopt(DCCP_SOCKOPT_SERVICE) fails. Placeholders: {} 1: service code number, 2: socket fd, 3: strerror() message.
         stdlog << formatText(gettext("Unable to configure DCCP service code {} (DCCP_SOCKOPT_SERVICE option) on DCCP socket: {}!"),
                              static_cast<unsigned int>(be32toh(service[0])), socketDescriptor, strerror(errno)) << "\n";
         LOG_END
         return false;
      }
   }
#if defined(HAVE_QUIC)
   else if(TrafficSpec.Protocol == IPPROTO_QUIC) {
      quic_config quicConfig;
      socklen_t   quicConfigLength = sizeof(quicConfig);
      if(ext_getsockopt(socketDescriptor, IPPROTO_QUIC, QUIC_SOCKOPT_CONFIG, &quicConfig, &quicConfigLength) < 0) {
         LOG_ERROR
         // TRANSLATORS: Error message when getsockopt(QUIC_SOCKOPT_CONFIG) fails. Placeholders: {} 1: socket fd, 2: strerror() message.
         stdlog << formatText(gettext("Unable to obtain configuration (QUIC_SOCKOPT_CONFIG option) on QUIC socket {}: {}!"),
                              socketDescriptor, strerror(errno)) << "\n";
         LOG_END
         return false;
      }
      const char* congestionControl = TrafficSpec.CongestionControl.c_str();
      if((strcmp(congestionControl, "reno") == 0) || (strcmp(congestionControl, "default") == 0)) {
         quicConfig.congestion_control_algo = QUIC_CONG_ALG_RENO;
      }
      else if(strcmp(congestionControl, "cubic") == 0) {
         quicConfig.congestion_control_algo = QUIC_CONG_ALG_CUBIC;
      }
      else {
         LOG_ERROR
         // TRANSLATORS: Error message when invalid congestion control algorithm is requested for QUIC. Placeholders: {} 1: congestion control name, 2: socket fd.
         stdlog << formatText(gettext("Invalid congestion control {} for QUIC requested on socket {}!"),
                              congestionControl, socketDescriptor) << "\n";
         LOG_END
         return false;
      }
      if(ext_setsockopt(socketDescriptor, IPPROTO_QUIC, QUIC_SOCKOPT_CONFIG, &quicConfig, quicConfigLength) < 0) {
         LOG_WARNING
         // TRANSLATORS: Warning message when setsockopt(QUIC_SOCKOPT_CONFIG) fails. Placeholders: {} 1: socket fd, 2: strerror() message.
         stdlog << formatText(gettext("Unable to set configuration (QUIC_SOCKOPT_CONFIG option) on QUIC socket {}: {}!"),
                              socketDescriptor, strerror(errno)) << "\n";
         LOG_END
      }
   }
#endif
#endif

   return true;
}
