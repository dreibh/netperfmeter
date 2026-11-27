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
#include "quic.h"
#include "transfer.h"
#include "tools.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <netinet/tcp.h>
#if defined(__linux__)
#include <linux/in6.h>
#endif
#include <signal.h>
#include <sstream>
#include <thread>


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
   HasFlowLabelLease        = false;
   memset(&FlowLabelLeaseAddr, 0, sizeof(FlowLabelLeaseAddr));
   FlowLabelLeaseValue      = 0;

   InputStatus              = WaitingForStartup;
   OutputStatus             = WaitingForStartup;
   TimeBase                 = getMonotonicMicroTime().value;

   TrafficSpec              = trafficSpec;

   MyMeasurement            = nullptr;
   FirstTransmission        = 0;
   LastTransmission         = 0;
   FirstReception           = 0;
   LastReception            = 0;
   resetStatistics();
   LastOutboundSeqNumber    = static_cast<uint64_t>(-1);
   LastOutboundFrameID      = static_cast<uint32_t>(-1);
   NextTransmissionEvent    = static_cast<uint64_t>(-1);
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
   if(HasFlowLabelLease && (SocketDescriptor >= 0)) {
      releaseFlowLabel(SocketDescriptor, FlowLabelLeaseAddr, FlowLabelLeaseValue);
      HasFlowLabelLease = false;
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
   // TRANSLATORS: Text direction: LTR (Left-to-Right) or RTL (Right-to-Left)
   const bool rtl = (std::string_view(gettext("LTR")) == "RTL");
   PropertyFormatter formatter("   ", { "", "- ", "+ ", "* " }, "=", rtl);

   lock();
   const bool isMultiStream =
      (TrafficSpec.Protocol == IPPROTO_SCTP)
#if defined(HAVE_QUIC)
      || (TrafficSpec.Protocol == IPPROTO_QUIC)
#endif
      ;
   if(!isMultiStream) {
      // TRANSLATORS: Flow summary header. Placeholders: {} 1: protocol name, 2: flow ID, 3: flow description, {:x} 4: measurement ID (hex).
      formatter.addLine(0, formatText(gettext("+ {} Flow (Flow #{} \"{}\" of Measurement ${:x}):"),
                                      getProtocolName(TrafficSpec.Protocol),
                                      FlowID, TrafficSpec.Description,
                                      MeasurementID));
   }
   else {
      // TRANSLATORS: Multi-stream flow header (SCTP association or QUIC connection). Placeholders: {} 1: protocol name, {:x} 2: measurement ID (hex).
      formatter.addLine(0, formatText(gettext("+ {} Flow (of Measurement ${:x}):"),
                                      getProtocolName(TrafficSpec.Protocol),
                                      MeasurementID));
      // TRANSLATORS: Stream description inside a multi-stream flow. Placeholders: {} 1: stream ID, 2: flow ID, 3: flow description, {:x} 4: measurement ID (hex).
      formatter.addLine(1, formatText(gettext("Stream #{} (Flow ID #{} \"{}\" of Measurement ${:x}):"),
                                      StreamID,
                                      FlowID, TrafficSpec.Description,
                                      MeasurementID));
   }

   const size_t baseLevel = isMultiStream ? 2 : 1;
   TrafficSpec.print(formatter, baseLevel);

   if(printStatistics) {
      CurrentBandwidthStats.print(formatter,
                                  std::chrono::duration<double>(std::chrono::microseconds(LastTransmission - FirstTransmission)).count(),
                                  std::chrono::duration<double>(std::chrono::microseconds(LastReception - FirstReception)).count(),
                                  baseLevel);
   }
   unlock();

   os << formatter;
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
void Flow::updateTransmissionStatistics(const MonotonicMicroTime nowMonotonic,
                                        const uint64_t           addedFrames,
                                        const uint64_t           addedPackets,
                                        const uint64_t           addedBytes)
{
   lock();

   // ====== Update statistics ==============================================
   LastTransmission = nowMonotonic.value;
   if(FirstTransmission == 0) {
      FirstTransmission = nowMonotonic.value;
   }
   CurrentBandwidthStats.TransmittedFrames  += addedFrames;
   CurrentBandwidthStats.TransmittedPackets += addedPackets;
   CurrentBandwidthStats.TransmittedBytes   += addedBytes;

   unlock();
}


// ###### Update reception statistics #######################################
void Flow::updateReceptionStatistics(const MonotonicMicroTime nowMonotonic,
                                     const SystemMicroTime    nowSystem,
                                     const uint64_t           addedFrames,
                                     const uint64_t           addedBytes,
                                     const uint64_t           lostFrames,
                                     const uint64_t           lostPackets,
                                     const uint64_t           lostBytes,
                                     const uint64_t           seqNumber,
                                     const double             delay,
                                     const double             delayDiff,
                                     const double             jitter)
{
   lock();

   // ====== Update statistics ==============================================
   LastReception = nowMonotonic.value;
   if(FirstReception == 0) {
      FirstReception = nowMonotonic.value;
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
         "%06" PRIu64 "\t%" PRIu64 "\t%1.6f\t%" PRIu64 "\t"
         "%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t"
         "%" PRIu64 "\t%u\t%" PRIu64 "\t"
         "%1.3f\t%1.3f\t%1.3f\n",
         static_cast<uint64_t>(VectorFile.nextLine()),
         nowSystem.value,
         std::chrono::duration<double>(std::chrono::microseconds(nowMonotonic.value - MyMeasurement->getFirstStatisticsEvent())).count(),
         seqNumber,
         CurrentBandwidthStats.ReceivedBytes,
         CurrentBandwidthStats.ReceivedPackets,
         CurrentBandwidthStats.ReceivedFrames,
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

      // Check whether another flow is still sharing this socket descriptor:
      bool socketStillShared = false;
      switch(TrafficSpec.Protocol) {
         case IPPROTO_TCP:
#if defined(HAVE_MPTCP)
         case IPPROTO_MPTCP:
#endif
         case IPPROTO_UDP:
#if defined(HAVE_DCCP)
         case IPPROTO_DCCP:
#endif
            // Single connection only -> nothing to do!
          break;
         default:
            FlowManager* flowManager = FlowManager::getFlowManager();
            flowManager->lock();
            for(const auto* otherFlow : flowManager->getFlowSet()) {
               if((otherFlow != this) && (otherFlow->getSocketDescriptor() == SocketDescriptor)) {
                  socketStillShared = true;
                  break;
               }
            }
            flowManager->unlock();
          break;
      }

      if((SocketDescriptor >= 0) && (!socketStillShared)) {
         if(TrafficSpec.Protocol == IPPROTO_UDP) {
            // NOTE: There is only one UDP socket. We cannot close it here!
         }
         else {
            const bool isTCPOrMPTCP =
                          (TrafficSpec.Protocol == IPPROTO_TCP)
#if defined(HAVE_MPTCP)
                          || (TrafficSpec.Protocol == IPPROTO_MPTCP)
#endif
                          ;
            ext_shutdown(SocketDescriptor, isTCPOrMPTCP ? SHUT_WR : SHUT_RDWR);
         }
      }

      stop();
      if(!asyncStop) {
         waitForFinish();
         if(!socketStillShared) {
            FlowManager::getFlowManager()->getMessageReader()->deregisterSocket(SocketDescriptor);
         }
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
         NextTransmissionEvent = 0;
         nextTransmissionEvent = 0;
      }
      // ====== Non-saturated sender ========================================
      else if((TrafficSpec.OutboundFrameSize[0] > 0.0) && (TrafficSpec.OutboundFrameRate[0] > 0.0000001)) {
         const double nextFrameRate = getRandomValue(static_cast<const double*>(TrafficSpec.OutboundFrameRate),
                                                     TrafficSpec.OutboundFrameRateRng);
         NextTransmissionEvent += static_cast<uint64_t>(rint(1000000.0 / nextFrameRate));
         nextTransmissionEvent = NextTransmissionEvent;
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
         if(TrafficSpec.outgoingFlowIsSaturated()) {
            NextTransmissionEvent = 0;
         }
         else if(TrafficSpec.outgoingFlowIsNotSaturated()) {
            NextTransmissionEvent = now;
         }
         else {
            NextTransmissionEvent = static_cast<uint64_t>(-1);
         }
      }
      else if(OutputStatus == On) {
         OutputStatus          = Off;
         NextTransmissionEvent = static_cast<uint64_t>(-1);
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

   const uint64_t startTime = getMonotonicMicroTime().value;
   scheduleNextStatusChangeEvent(startTime);
   lock();
   if(OutputStatus == Flow::On) {
      if(TrafficSpec.outgoingFlowIsSaturated()) {
         NextTransmissionEvent = 0;
      }
      else if(TrafficSpec.outgoingFlowIsNotSaturated()) {
         NextTransmissionEvent = startTime;
      }
      else {
         NextTransmissionEvent = static_cast<uint64_t>(-1);
      }
   }
   unlock();

   bool result = true;
   do {
      // ====== Schedule next event =========================================
      MonotonicMicroTime now = getMonotonicMicroTime();
      lock();
      uint64_t nextTransmission = NextTransmissionEvent;
      uint64_t nextEvent        = std::min(NextStatusChangeEvent, nextTransmission);
      unlock();

      // ====== Wait until there is something to do =========================
      if(nextEvent > now.value) {
         const auto waitTime = std::min(std::chrono::microseconds(nextEvent - now.value),
                                        std::chrono::microseconds(1000000));
         std::this_thread::sleep_for(waitTime);
         now = getMonotonicMicroTime();
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
            if(nextTransmission <= now.value) {
               do {
                  result = transmitFrame(this, now);
                  if(now.value - nextTransmission > 1000000ULL) {
                     // Time gap of more than 1s -> do not try to correct.
                     // But the next transmission needs to be scheduled!
                     lock();
                     NextTransmissionEvent = now.value;
                     unlock();
                     scheduleNextTransmissionEvent();
                     break;
                  }
               } while((result == true) &&
                       (!isStopping()) &&
                       ((nextTransmission = scheduleNextTransmissionEvent()) <= std::min(now.value, NextStatusChangeEvent)));

               if(TrafficSpec.Protocol == IPPROTO_UDP) {
                  // Keep sending, even if there is a temporary failure.
                  result = true;
               }
            }
         }
      }

      // ====== Handle status changes =======================================
      if(NextStatusChangeEvent <= now.value) {
         handleStatusChangeEvent(now.value);
      }
   } while((result == true) && (!isStopping()));
}


// ###### Set IP_TOS / IPV6_TCLASS on socket ################################
bool setTrafficClass(const int sd, const uint8_t trafficClass, const bool isMPTCP)
{
   (void)isMPTCP;
   sockaddr_union localAddr;
   socklen_t      localAddrLen = sizeof(localAddr);
   memset(&localAddr, 0, sizeof(localAddr));
   if(ext_getsockname(sd, &localAddr.sa, &localAddrLen) < 0) {
      LOG_ERROR
      // TRANSLATORS: Error message when getsockname() fails while setting Traffic Class. Placeholders: {} 1: socket fd, 2: strerror() message.
      stdlog << formatText(gettext("Unable to determine address family of socket {}: {}!"),
                           sd, strerror(errno)) << "\n";
      LOG_END
      return false;
   }

   const int  tc           = static_cast<int>(trafficClass & 0xfcU);
   const bool isIPv4Mapped = (localAddr.sa.sa_family == AF_INET6) &&
                             IN6_IS_ADDR_V4MAPPED(&localAddr.in6.sin6_addr);
   if((localAddr.sa.sa_family == AF_INET) || isIPv4Mapped) {
#if defined(IP_TOS)
      if(ext_setsockopt(sd, IPPROTO_IP, IP_TOS, &tc, sizeof(tc)) < 0) {
         if(isIPv4Mapped && ((errno == ENOPROTOOPT) || (errno == EINVAL) || (errno == EAFNOSUPPORT))) {
#if defined(IPV6_TCLASS)
            if(ext_setsockopt(sd, IPPROTO_IPV6, IPV6_TCLASS, &tc, sizeof(tc)) < 0) {
               LOG_WARNING
               // TRANSLATORS: Warning when setting Traffic Class on an IPv4-mapped IPv6 socket fails. Placeholders: {:02x} 1: hex TC value, {} 2: socket fd, 3: strerror() message.
               stdlog << formatText(gettext("Unable to configure Traffic Class 0x{:02x} (IPV6_TCLASS option) on socket {}: {}!"),
                                    static_cast<unsigned int>(tc), sd, strerror(errno)) << "\n";
               LOG_END
            }
#endif
            return true;
         }
         if(isMPTCP && ((errno == ENOENT) || (errno == ENOPROTOOPT) || (errno == EOPNOTSUPP))) {
            LOG_WARNING
            // TRANSLATORS: Warning when setsockopt(IP_TOS) is unsupported on MPTCP socket. Placeholders: {:02x} 1: hex TC value, {} 2: socket fd, 3: strerror() message.
            stdlog << formatText(gettext("Unable to configure Traffic Class 0x{:02x} (IP_TOS option) on MPTCP socket {}: {}!"),
                                 static_cast<unsigned int>(tc), sd, strerror(errno)) << "\n";
            LOG_END
            return true;
         }
         LOG_ERROR
         // TRANSLATORS: Error message when setsockopt(IP_TOS) fails. Placeholders: {:02x} 1: hex TC value, {} 2: socket fd, 3: strerror() message.
         stdlog << formatText(gettext("Unable to configure Traffic Class 0x{:02x} (IP_TOS option) on socket {}: {}!"),
                              static_cast<unsigned int>(tc), sd, strerror(errno)) << "\n";
         LOG_END
         return false;
      }
#else
      LOG_WARNING
      // TRANSLATORS: Warning when IP_TOS socket option is not available on the OS.
      stdlog << gettext("IP_TOS socket option is not supported on this platform!") << "\n";
      LOG_END
#endif
   }
   else if(localAddr.sa.sa_family == AF_INET6) {
#if defined(IPV6_TCLASS)
      if(ext_setsockopt(sd, IPPROTO_IPV6, IPV6_TCLASS, &tc, sizeof(tc)) < 0) {
         if(isMPTCP && ((errno == ENOENT) || (errno == ENOPROTOOPT) || (errno == EOPNOTSUPP))) {
            LOG_WARNING
            // TRANSLATORS: Warning when setsockopt(IPV6_TCLASS) is unsupported on MPTCP socket. Placeholders: {:02x} 1: hex TC value, {} 2: socket fd, 3: strerror() message.
            stdlog << formatText(gettext("Unable to configure Traffic Class 0x{:02x} (IPV6_TCLASS option) on MPTCP socket {}: {}!"),
                                 static_cast<unsigned int>(tc), sd, strerror(errno)) << "\n";
            LOG_END
         }
         else {
            LOG_ERROR
            // TRANSLATORS: Error message when setsockopt(IPV6_TCLASS) fails. Placeholders: {:02x} 1: hex TC value, {} 2: socket fd, 3: strerror() message.
            stdlog << formatText(gettext("Unable to configure Traffic Class 0x{:02x} (IPV6_TCLASS option) on socket {}: {}!"),
                                 static_cast<unsigned int>(tc), sd, strerror(errno)) << "\n";
            LOG_END
            return false;
         }
      }
#else
      LOG_WARNING
      // TRANSLATORS: Warning when IPV6_TCLASS socket option is not available on the OS.
      stdlog << gettext("IPV6_TCLASS socket option is not supported on this platform!") << "\n";
      LOG_END
#endif
      // On dual-stack AF_INET6 sockets (IPV6_V6ONLY == 0), also attempt IP_TOS
      // for IPv4-mapped peers (Linux supports it; BSDs return ENOPROTOOPT/EINVAL).
#if defined(IP_TOS) && defined(IPV6_V6ONLY)
      int       v6only    = 1;
      socklen_t v6onlyLen = sizeof(v6only);
      if((ext_getsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, &v6onlyLen) == 0) && (v6only == 0)) {
         if(ext_setsockopt(sd, IPPROTO_IP, IP_TOS, &tc, sizeof(tc)) < 0) {
            if((errno != ENOPROTOOPT) && (errno != EINVAL) && (errno != EAFNOSUPPORT) && (errno != EOPNOTSUPP)) {
               LOG_WARNING
               // TRANSLATORS: Warning when setting IP_TOS on a dual-stack IPv6 socket fails. Placeholders: {:02x} 1: hex TC value, {} 2: socket fd, 3: strerror() message.
               stdlog << formatText(gettext("Unable to configure IPv4 Traffic Class 0x{:02x} (IP_TOS option) on dual-stack socket {}: {}!"),
                                    static_cast<unsigned int>(tc), sd, strerror(errno)) << "\n";
               LOG_END
            }
         }
      }
#endif
   }
   return true;
}


// ###### Configure IPv6 Flow Label on socket ###############################
bool setFlowLabel(const int               sd,
                  const struct sockaddr*  peerAddress,
                  const FlowLabelSetting& flowLabel,
                  const bool              isAcceptedConnection,
                  bool&                   leaseAcquired,
                  struct in6_addr&        leaseAddr,
                  uint32_t&               leaseValue)
{
   leaseAcquired = false;
   if(!flowLabel.isSet()) {
      return true;
   }

   sockaddr_union localAddr;
   socklen_t      localAddrLen = sizeof(localAddr);
   memset(&localAddr, 0, sizeof(localAddr));
   if((ext_getsockname(sd, &localAddr.sa, &localAddrLen) < 0) ||
      (localAddr.sa.sa_family != AF_INET6)) {
      LOG_WARNING
      // TRANSLATORS: Warning when IPv6 Flow Label is requested on a non-IPv6 socket. Placeholder: {} is socket fd.
      stdlog << formatText(gettext("IPv6 Flow Label ignored on non-IPv6 socket {}!"), sd) << "\n";
      LOG_END
      return true;
   }
   if((peerAddress != nullptr) &&
      ((peerAddress->sa_family != AF_INET6) ||
       IN6_IS_ADDR_V4MAPPED(&reinterpret_cast<const struct sockaddr_in6*>(peerAddress)->sin6_addr))) {
      LOG_WARNING
      // TRANSLATORS: Warning when IPv6 Flow Label is requested for an IPv4 or IPv4-mapped peer. Placeholder: {} is socket fd.
      stdlog << formatText(gettext("IPv6 Flow Label ignored for IPv4 peer on socket {}!"), sd) << "\n";
      LOG_END
      return true;
   }

#if defined(__linux__) && defined(IPV6_FLOWLABEL_MGR) && defined(IPV6_FLOWINFO_SEND)
   if(flowLabel.Mode == FlowLabelMode::Auto) {
#if defined(IPV6_AUTOFLOWLABEL)
      const int enable = 1;
      if(ext_setsockopt(sd, IPPROTO_IPV6, IPV6_AUTOFLOWLABEL, &enable, sizeof(enable)) < 0) {
         LOG_WARNING
         // TRANSLATORS: Warning when enabling IPV6_AUTOFLOWLABEL fails. Placeholders: {} 1: socket fd, 2: strerror() message.
         stdlog << formatText(gettext("Unable to enable IPV6_AUTOFLOWLABEL on socket {}: {}!"),
                              sd, strerror(errno)) << "\n";
         LOG_END
      }
#else
      LOG_WARNING
      // TRANSLATORS: Warning when IPV6_AUTOFLOWLABEL is unsupported on the OS.
      stdlog << gettext("IPV6_AUTOFLOWLABEL is not supported on this platform!") << "\n";
      LOG_END
#endif
      return true;
   }

   if(flowLabel.Mode == FlowLabelMode::Reflect) {
#if defined(IPV6_FL_F_REFLECT)
      if(peerAddress != nullptr) {
         struct in6_flowlabel_req freq;
         memset(&freq, 0, sizeof(freq));
         freq.flr_dst    = reinterpret_cast<const struct sockaddr_in6*>(peerAddress)->sin6_addr;
         freq.flr_action = IPV6_FL_A_GET;
         freq.flr_flags  = IPV6_FL_F_CREATE | IPV6_FL_F_REFLECT;
         freq.flr_share  = IPV6_FL_S_ANY;
         if(ext_setsockopt(sd, IPPROTO_IPV6, IPV6_FLOWLABEL_MGR, &freq, sizeof(freq)) < 0) {
            LOG_WARNING
            // TRANSLATORS: Warning when enabling IPv6 Flow Label reflection fails. Placeholders: {} 1: socket fd, 2: strerror() message.
            stdlog << formatText(gettext("Unable to enable IPv6 Flow Label reflection on socket {}: {}!"),
                                 sd, strerror(errno)) << "\n";
            LOG_END
         }
         else {
            const int enable = 1;
            ext_setsockopt(sd, IPPROTO_IPV6, IPV6_FLOWINFO_SEND, &enable, sizeof(enable));
         }
      }
#else
      LOG_WARNING
      // TRANSLATORS: Warning when IPv6 Flow Label reflection is unsupported on the OS.
      stdlog << gettext("IPv6 Flow Label reflection is not supported on this platform!") << "\n";
      LOG_END
#endif
      return true;
   }

   // Explicit 20-bit Flow Label:
   if(flowLabel.Label == 0) {
#if defined(IPV6_AUTOFLOWLABEL)
      const int disable = 0;
      if(ext_setsockopt(sd, IPPROTO_IPV6, IPV6_AUTOFLOWLABEL, &disable, sizeof(disable)) < 0) {
         LOG_WARNING
         // TRANSLATORS: Warning when disabling IPV6_AUTOFLOWLABEL fails. Placeholders: {} 1: socket fd, 2: strerror() message.
         stdlog << formatText(gettext("Unable to disable IPV6_AUTOFLOWLABEL on socket {}: {}!"),
                              sd, strerror(errno)) << "\n";
         LOG_END
      }
#endif
      return true;
   }

   if(isAcceptedConnection) {
      // On Linux, post-accept() connection-oriented sockets cannot set a custom
      // sin6_flowinfo (passive UDP uses sendmsg() with isAcceptedConnection = false).
      LOG_WARNING
      // TRANSLATORS: Warning when a custom IPv6 Flow Label cannot be applied to an accepted connection socket. Placeholders: {:05x} 1: hex label, {} 2: socket fd.
      stdlog << formatText(gettext("Custom IPv6 Flow Label 0x{:05x} cannot be applied to accepted connection socket {} on this OS!"),
                           flowLabel.Label & 0x000fffffU, sd) << "\n";
      LOG_END
      return true;
   }

   if(peerAddress == nullptr) {
      return true;
   }

   const int enableSend = 1;
   if(ext_setsockopt(sd, IPPROTO_IPV6, IPV6_FLOWINFO_SEND, &enableSend, sizeof(enableSend)) < 0) {
      LOG_WARNING
      // TRANSLATORS: Warning when enabling IPV6_FLOWINFO_SEND fails. Placeholders: {} 1: socket fd, 2: strerror() message.
      stdlog << formatText(gettext("Unable to enable IPV6_FLOWINFO_SEND on socket {}: {}!"),
                           sd, strerror(errno)) << "\n";
      LOG_END
      return true;
   }

   struct in6_flowlabel_req freq;
   memset(&freq, 0, sizeof(freq));
   freq.flr_dst    = reinterpret_cast<const struct sockaddr_in6*>(peerAddress)->sin6_addr;
   freq.flr_label  = htobe32(flowLabel.Label & 0x000fffffU);
   freq.flr_action = IPV6_FL_A_GET;
   freq.flr_flags  = IPV6_FL_F_CREATE;
   freq.flr_share  = IPV6_FL_S_ANY;
   if(ext_setsockopt(sd, IPPROTO_IPV6, IPV6_FLOWLABEL_MGR, &freq, sizeof(freq)) < 0) {
      LOG_WARNING
      // TRANSLATORS: Warning when acquiring an IPv6 Flow Label via IPV6_FLOWLABEL_MGR fails. Placeholders: {:05x} 1: hex label, {} 2: socket fd, 3: strerror() message.
      stdlog << formatText(gettext("Unable to acquire IPv6 Flow Label 0x{:05x} (IPV6_FLOWLABEL_MGR) on socket {}: {}!"),
                           flowLabel.Label & 0x000fffffU, sd, strerror(errno)) << "\n";
      LOG_END
      return true;
   }
   leaseAcquired = true;
   leaseAddr     = freq.flr_dst;
   leaseValue    = flowLabel.Label & 0x000fffffU;
#else
   (void)isAcceptedConnection;
   (void)leaseAddr;
   (void)leaseValue;
   LOG_WARNING
   // TRANSLATORS: Warning when IPv6 Flow Label configuration is unsupported on the OS. Placeholder: {} is socket fd.
   stdlog << formatText(gettext("IPv6 Flow Label configuration is not supported on this platform (socket {})!"), sd) << "\n";
   LOG_END
#endif
   return true;
}


// ###### Release Linux IPv6 Flow Label lease ###############################
void releaseFlowLabel(const int sd, const struct in6_addr& leaseAddr, const uint32_t leaseValue)
{
#if defined(__linux__) && defined(IPV6_FLOWLABEL_MGR)
   struct in6_flowlabel_req freq;
   memset(&freq, 0, sizeof(freq));
   freq.flr_dst    = leaseAddr;
   freq.flr_label  = htobe32(leaseValue & 0x000fffffU);
   freq.flr_action = IPV6_FL_A_PUT;
   ext_setsockopt(sd, IPPROTO_IPV6, IPV6_FLOWLABEL_MGR, &freq, sizeof(freq));
#else
   (void)sd;
   (void)leaseAddr;
   (void)leaseValue;
#endif
}


// ###### Configure TCP / MPTCP keep-alive options ##########################
bool configureTCPKeepAlive(const int                      sd,
                           const bool                     isMPTCP,
                           const std::optional<uint32_t>& hbInterval,
                           const std::optional<uint32_t>& hbPathMaxRtx)
{
   (void)isMPTCP;
   if(!hbInterval.has_value() && !hbPathMaxRtx.has_value()) {
      return true;
   }

   if(hbInterval.has_value() && (*hbInterval == 0)) {
      const int keepAliveOption = 0;
      if(ext_setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE,
                        reinterpret_cast<const char*>(&keepAliveOption), sizeof(keepAliveOption)) < 0) {
         LOG_ERROR
         // TRANSLATORS: Error disabling SO_KEEPALIVE on TCP socket. Placeholders: {} 1: socket fd, 2: strerror() message.
         stdlog << formatText(gettext("Unable to disable SO_KEEPALIVE on TCP socket {}: {}!"),
                              sd, strerror(errno)) << "\n";
         LOG_END
         return false;
      }
      return true;
   }

   const int keepAliveOption = 1;
   if(ext_setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE,
                     reinterpret_cast<const char*>(&keepAliveOption), sizeof(keepAliveOption)) < 0) {
      LOG_ERROR
      // TRANSLATORS: Error enabling SO_KEEPALIVE on TCP socket. Placeholders: {} 1: socket fd, 2: strerror() message.
      stdlog << formatText(gettext("Unable to set SO_KEEPALIVE on TCP socket {}: {}!"),
                           sd, strerror(errno)) << "\n";
      LOG_END
      return false;
   }

   if(hbInterval.has_value()) {
      if((*hbInterval < 1000) || ((*hbInterval % 1000) != 0)) {
         LOG_WARNING
         // TRANSLATORS: Warning when TCP keep-alive interval in milliseconds is rounded to seconds. Placeholders: {} 1: milliseconds, 2: socket fd.
         stdlog << formatText(gettext("TCP keep-alive uses whole seconds; rounding {} ms on socket {}!"),
                              *hbInterval, sd) << "\n";
         LOG_END
      }
      const int intervalSec = std::max(1, static_cast<int>(*hbInterval / 1000));
#if defined(TCP_KEEPIDLE) && !defined(__GNU__)
      if(ext_setsockopt(sd, IPPROTO_TCP, TCP_KEEPIDLE,
                        reinterpret_cast<const char*>(&intervalSec), sizeof(intervalSec)) < 0) {
         if((!isMPTCP) || ((errno != ENOENT) && (errno != ENOPROTOOPT) && (errno != EOPNOTSUPP))) {
            LOG_ERROR
            // TRANSLATORS: Error setting TCP keepalive idle time. Placeholders: {} 1: socket fd, 2: strerror() message.
            stdlog << formatText(gettext("Unable to set TCP_KEEPIDLE on TCP socket {}: {}!"),
                                 sd, strerror(errno)) << "\n";
            LOG_END
            return false;
         }
         LOG_WARNING
         // TRANSLATORS: Warning setting TCP keepalive idle time on MPTCP socket. Placeholders: {} 1: socket fd, 2: strerror() message.
         stdlog << formatText(gettext("Unable to set TCP_KEEPIDLE on MPTCP socket {}: {}!"),
                              sd, strerror(errno)) << "\n";
         LOG_END
      }
#else
#warning No TCP_KEEPIDLE option available!
#endif
#if defined(TCP_KEEPINTVL) && !defined(__GNU__)
      if(ext_setsockopt(sd, IPPROTO_TCP, TCP_KEEPINTVL,
                        reinterpret_cast<const char*>(&intervalSec), sizeof(intervalSec)) < 0) {
         if((!isMPTCP) || ((errno != ENOENT) && (errno != ENOPROTOOPT) && (errno != EOPNOTSUPP))) {
            LOG_ERROR
            // TRANSLATORS: Error setting TCP keepalive probe interval. Placeholders: {} 1: socket fd, 2: strerror() message.
            stdlog << formatText(gettext("Unable to set TCP_KEEPINTVL on TCP socket {}: {}!"),
                                 sd, strerror(errno)) << "\n";
            LOG_END
            return false;
         }
         LOG_WARNING
         // TRANSLATORS: Warning setting TCP keepalive probe interval on MPTCP socket. Placeholders: {} 1: socket fd, 2: strerror() message.
         stdlog << formatText(gettext("Unable to set TCP_KEEPINTVL on MPTCP socket {}: {}!"),
                              sd, strerror(errno)) << "\n";
         LOG_END
      }
#else
#warning No TCP_KEEPINTVL option available!
#endif
   }

   if(hbPathMaxRtx.has_value()) {
#if defined(TCP_KEEPCNT) && !defined(__GNU__)
      const int keepCountOption = static_cast<int>(*hbPathMaxRtx);
      if(ext_setsockopt(sd, IPPROTO_TCP, TCP_KEEPCNT,
                        reinterpret_cast<const char*>(&keepCountOption), sizeof(keepCountOption)) < 0) {
         if((!isMPTCP) || ((errno != ENOENT) && (errno != ENOPROTOOPT) && (errno != EOPNOTSUPP))) {
            LOG_ERROR
            // TRANSLATORS: Error setting TCP keepalive probe count. Placeholders: {} 1: socket fd, 2: strerror() message.
            stdlog << formatText(gettext("Unable to set TCP_KEEPCNT on TCP socket {}: {}!"),
                                 sd, strerror(errno)) << "\n";
            LOG_END
            return false;
         }
         LOG_WARNING
         // TRANSLATORS: Warning setting TCP keepalive probe count on MPTCP socket. Placeholders: {} 1: socket fd, 2: strerror() message.
         stdlog << formatText(gettext("Unable to set TCP_KEEPCNT on MPTCP socket {}: {}!"),
                              sd, strerror(errno)) << "\n";
         LOG_END
      }
#else
#warning No TCP_KEEPCNT option available!
#endif
   }
   return true;
}


#if defined(HAVE_SCTP)
// ###### Configure SCTP heartbeat and retransmission parameters ############
bool configureSCTPHeartbeats(const int                      sd,
                             const sctp_assoc_t             assocID,
                             const sockaddr_union*          peerAddress,
                             const std::optional<uint32_t>& hbInterval,
                             const std::optional<uint32_t>& hbPathMaxRtx,
                             const std::optional<uint32_t>& hbAssocMaxRtx)
{
   if(hbInterval.has_value() || hbPathMaxRtx.has_value()) {
      struct sctp_paddrparams params;
      memset(&params, 0, sizeof(params));
      params.spp_assoc_id = assocID;
      if((peerAddress != nullptr) &&
         ((peerAddress->sa.sa_family == AF_INET) || (peerAddress->sa.sa_family == AF_INET6))) {
         if(assocID == SCTP_FUTURE_ASSOC) {
            params.spp_address.ss_family = peerAddress->sa.sa_family;
         }
         else {
            memcpy(&params.spp_address, peerAddress, getSocklen(&peerAddress->sa));
         }
      }
      else {
         params.spp_address.ss_family = checkIPv6() ? AF_INET6 : AF_INET;
      }

      if(hbInterval.has_value()) {
         if(*hbInterval == 0) {
#if defined(SPP_HB_DISABLE)
            params.spp_flags |= SPP_HB_DISABLE;
#endif
         }
         else {
#if defined(SPP_HB_ENABLE)
            params.spp_flags |= SPP_HB_ENABLE;
#endif
            params.spp_hbinterval = *hbInterval;
         }
      }
      if(hbPathMaxRtx.has_value()) {
         params.spp_pathmaxrxt = static_cast<uint16_t>(std::min<uint32_t>(*hbPathMaxRtx, 65535U));
      }

      if(ext_setsockopt(sd, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &params, sizeof(params)) < 0) {
         bool retryOk = false;
         if(assocID != SCTP_FUTURE_ASSOC) {
            const sa_family_t fam = params.spp_address.ss_family;
            memset(&params.spp_address, 0, sizeof(params.spp_address));
            params.spp_address.ss_family = fam;
            params.spp_assoc_id          = 0;
            retryOk = (ext_setsockopt(sd, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &params, sizeof(params)) == 0);
         }
         if(!retryOk) {
            LOG_WARNING
            // TRANSLATORS: Warning configuring SCTP heartbeats via SCTP_PEER_ADDR_PARAMS. Placeholders: {} 1: socket fd, 2: strerror() message.
            stdlog << formatText(gettext("Unable to configure heartbeats (SCTP_PEER_ADDR_PARAMS option) on SCTP socket {}: {}!"),
                                 sd, strerror(errno)) << "\n";
            LOG_END
         }
      }
   }

   if(hbAssocMaxRtx.has_value()) {
      sctp_assocparams assocparams;
      memset(&assocparams, 0, sizeof(assocparams));
      assocparams.sasoc_assoc_id   = assocID;
      assocparams.sasoc_asocmaxrxt = static_cast<uint16_t>(std::min<uint32_t>(*hbAssocMaxRtx, 65535U));
      if(ext_setsockopt(sd, IPPROTO_SCTP, SCTP_ASSOCINFO, &assocparams, sizeof(assocparams)) < 0) {
         bool retryOk = false;
         if(assocID != SCTP_FUTURE_ASSOC) {
            assocparams.sasoc_assoc_id = 0;
            retryOk = (ext_setsockopt(sd, IPPROTO_SCTP, SCTP_ASSOCINFO, &assocparams, sizeof(assocparams)) == 0);
         }
         if(!retryOk) {
            LOG_WARNING
            // TRANSLATORS: Warning configuring SCTP association retransmission max. Placeholders: {} 1: socket fd, 2: strerror() message.
            stdlog << formatText(gettext("Unable to configure retransmissions (SCTP_ASSOCINFO option) on SCTP socket {}: {}!"),
                                 sd, strerror(errno)) << "\n";
            LOG_END
         }
      }
   }
   return true;
}
#endif


// ###### Configure socket parameters #######################################
bool Flow::configureSocket(const int             socketDescriptor,
                           const sockaddr_union* remoteAddress)
{
   if(!setBufferSizes(socketDescriptor,
                      static_cast<int>(TrafficSpec.SndBufferSize),
                      static_cast<int>(TrafficSpec.RcvBufferSize))) {
      return false;
   }

   const bool isPassiveUDP = (TrafficSpec.Protocol == IPPROTO_UDP) && isAcceptedIncomingFlow();
   const bool isMPTCPSocket =
#if defined(HAVE_MPTCP)
      (TrafficSpec.Protocol == IPPROTO_MPTCP);
#else
      false;
#endif
   if(TrafficSpec.TrafficClass.has_value() && (!isPassiveUDP)) {
      if(!setTrafficClass(socketDescriptor, *TrafficSpec.TrafficClass, isMPTCPSocket)) {
         return false;
      }
   }
   if(TrafficSpec.FlowLabel.isSet()) {
      const sockaddr_union* peerAddr = (remoteAddress != nullptr) ? remoteAddress :
                                       (RemoteAddressIsValid ? &RemoteAddress : nullptr);
      setFlowLabel(socketDescriptor,
                   (peerAddr != nullptr) ? &peerAddr->sa : nullptr,
                   TrafficSpec.FlowLabel,
                   isAcceptedIncomingFlow() && (!isPassiveUDP),
                   HasFlowLabelLease, FlowLabelLeaseAddr, FlowLabelLeaseValue);
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

      const bool isMPTCP =
#if defined(HAVE_MPTCP)
         (TrafficSpec.Protocol == IPPROTO_MPTCP);
#else
         false;
#endif
      if(!configureTCPKeepAlive(socketDescriptor, isMPTCP,
                                TrafficSpec.HBInterval, TrafficSpec.HBPathMaxRtx)) {
         return false;
      }
      if(TrafficSpec.HBAssocMaxRtx.has_value()) {
         LOG_WARNING
         // TRANSLATORS: Warning when hbassocmaxrtx is set on TCP/MPTCP where only path keep-alive count applies. Placeholder: {} is socket fd.
         stdlog << formatText(gettext("hbassocmaxrtx is not applicable to TCP/MPTCP socket {}!"),
                              socketDescriptor) << "\n";
         LOG_END
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
         // TRANSLATORS: Error message when CMT is requested on SCTP but unsupported by the system. Placeholder: {} is socket fd.
         stdlog << formatText(gettext("CMT usage on SCTP socket {} configured, but not supported by this system!"),
                              socketDescriptor) << "\n";
         LOG_END
         return false;
      }
#endif
      if(TrafficSpec.HBInterval.has_value() || TrafficSpec.HBPathMaxRtx.has_value() || TrafficSpec.HBAssocMaxRtx.has_value()) {
         const sockaddr_union* peerAddr = (remoteAddress != nullptr) ? remoteAddress :
                                          (RemoteAddressIsValid ? &RemoteAddress : nullptr);
         configureSCTPHeartbeats(socketDescriptor,
                                 isAcceptedIncomingFlow() ? SCTP_CURRENT_ASSOC : SCTP_FUTURE_ASSOC,
                                 peerAddr,
                                 TrafficSpec.HBInterval, TrafficSpec.HBPathMaxRtx, TrafficSpec.HBAssocMaxRtx);
      }
   }
#endif
#if defined(HAVE_DCCP)
   else if(TrafficSpec.Protocol == IPPROTO_DCCP) {
      const uint8_t value = TrafficSpec.CCID;
      if(value != 0) {
         if(ext_setsockopt(socketDescriptor, SOL_DCCP, DCCP_SOCKOPT_CCID, &value, sizeof(value)) < 0) {
            LOG_WARNING
            // TRANSLATORS: Warning message when setsockopt(DCCP_SOCKOPT_CCID) fails. Placeholders: {} 1: CCID number, 2: socket fd, 3: strerror() message.
            stdlog << formatText(gettext("Unable to configure CCID {} (DCCP_SOCKOPT_CCID option) on DCCP socket {}: {}!"),
                                 static_cast<unsigned int>(value), socketDescriptor, strerror(errno)) << "\n";
            LOG_END
         }
      }
      const uint32_t service[1] = { htobe32(SC_NETPERFMETER_DATA) };
      if(ext_setsockopt(socketDescriptor, SOL_DCCP, DCCP_SOCKOPT_SERVICE, &service, sizeof(service)) < 0) {
         LOG_ERROR
         // TRANSLATORS: Error message when setsockopt(DCCP_SOCKOPT_SERVICE) fails. Placeholders: {} 1: service code number, 2: socket fd, 3: strerror() message.
         stdlog << formatText(gettext("Unable to configure DCCP service code {} (DCCP_SOCKOPT_SERVICE option) on DCCP socket {}: {}!"),
                              static_cast<unsigned int>(be32toh(service[0])), socketDescriptor, strerror(errno)) << "\n";
         LOG_END
         return false;
      }
      const int txQlen = 128;
      if(ext_setsockopt(socketDescriptor, SOL_DCCP, DCCP_SOCKOPT_QPOLICY_TXQLEN, &txQlen, sizeof(txQlen)) < 0) {
         LOG_WARNING
         // TRANSLATORS: Warning message when setsockopt(DCCP_SOCKOPT_QPOLICY_TXQLEN) fails. Placeholders: {} 1: queue length in packets, 2: socket fd, 3: strerror() message.
         stdlog << formatText(gettext("Unable to configure DCCP TX queue length {} (DCCP_SOCKOPT_QPOLICY_TXQLEN option) on DCCP socket {}: {}!"),
                              txQlen, socketDescriptor, strerror(errno)) << "\n";
         LOG_END
      }
   }
#endif
#if defined(HAVE_QUIC)
   else if(TrafficSpec.Protocol == IPPROTO_QUIC) {
      quic_config quicConfig;
      socklen_t   quicConfigLength = sizeof(quicConfig);
      if(ext_getsockopt(socketDescriptor, SOL_QUIC, QUIC_SOCKOPT_CONFIG, &quicConfig, &quicConfigLength) < 0) {
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
      if(ext_setsockopt(socketDescriptor, SOL_QUIC, QUIC_SOCKOPT_CONFIG, &quicConfig, quicConfigLength) < 0) {
         LOG_WARNING
         // TRANSLATORS: Warning message when setsockopt(QUIC_SOCKOPT_CONFIG) fails. Placeholders: {} 1: socket fd, 2: strerror() message.
         stdlog << formatText(gettext("Unable to set configuration (QUIC_SOCKOPT_CONFIG option) on QUIC socket {}: {}!"),
                              socketDescriptor, strerror(errno)) << "\n";
         LOG_END
      }
      if(TrafficSpec.HBInterval.has_value() || TrafficSpec.HBAssocMaxRtx.has_value()) {
         const bool disableIdle = TrafficSpec.HBInterval.has_value() && (*TrafficSpec.HBInterval == 0);
         const uint64_t idleTimeoutMs = disableIdle ? 0ULL :
            (static_cast<uint64_t>(TrafficSpec.HBInterval.value_or(15000U)) *
             static_cast<uint64_t>(TrafficSpec.HBAssocMaxRtx.value_or(6U)));
         if(!configureQuicIdleTimeout(socketDescriptor, idleTimeoutMs, disableIdle)) {
            LOG_WARNING
            // TRANSLATORS: Warning when configuring QUIC idle timeout fails. Placeholders: {} 1: socket fd, 2: strerror() message.
            stdlog << formatText(gettext("Unable to configure QUIC idle timeout on QUIC socket {}: {}!"),
                                 socketDescriptor, strerror(errno)) << "\n";
            LOG_END
         }
      }
   }
#endif

   if(((TrafficSpec.Protocol == IPPROTO_UDP)
#if defined(HAVE_DCCP)
       || (TrafficSpec.Protocol == IPPROTO_DCCP)
#endif
      ) && (TrafficSpec.HBInterval.has_value() || TrafficSpec.HBPathMaxRtx.has_value() || TrafficSpec.HBAssocMaxRtx.has_value())) {
      LOG_WARNING
      // TRANSLATORS: Warning when heartbeat/keep-alive options are requested on a protocol that does not support them. Placeholders: {} 1: protocol name, 2: socket fd.
      stdlog << formatText(gettext("Heartbeat/keep-alive options are not supported for {} socket {}!"),
                           getProtocolName(TrafficSpec.Protocol), socketDescriptor) << "\n";
      LOG_END
   }

   return true;
}
