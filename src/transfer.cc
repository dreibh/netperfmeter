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

#include "transfer.h"
#include "control.h"
#include "loglevel.h"
#include "tools.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <sys/uio.h>


#define MAXIMUM_MESSAGE_SIZE static_cast<size_t>(65536)
#define MAXIMUM_PAYLOAD_SIZE (MAXIMUM_MESSAGE_SIZE - sizeof(NetPerfMeterDataMessage))


static void updateStatistics(Flow*                          flowSpec,
                             const MonotonicMicroTime       nowMonotonic,
                             const SystemMicroTime          nowSystem,
                             const NetPerfMeterDataMessage* dataMsg,
                             const size_t                   received);



// ###### Generate payload pattern ##########################################
static void fillPayload(unsigned char* payload,
                        const size_t   length,
                        const bool     reverse = false)
{
   // ====== Pattern for Active -> Passive transfer =========================
   if(!reverse) {
      unsigned char c = 30;
      for(size_t i = 0; i < length; i++) {
         *payload++ = c++;
         if(c > 127) {
            c = 30;
         }
      }
   }
   // ====== Pattern for Passive -> Active transfer =========================
   else {
      unsigned char c = 127;
      for(size_t i = 0; i < length; i++) {
         *payload++ = c--;
         if(c < 30) {
            c = 127;
         }
      }
   }
}


// ###### Send NETPERFMETER_DATA message ####################################
static ssize_t sendNetPerfMeterData(Flow*                    flow,
                                    const uint32_t           frameID,
                                    const bool               isFrameBegin,
                                    const bool               isFrameEnd,
                                    const MonotonicMicroTime now,
                                    const uint64_t           byteOffset,
                                    size_t                   bytesToSend)
{
   flow->lock();
   const int sd = flow->getSocketDescriptor();
   flow->unlock();
   if(sd < 0) {
      return -1;
   }

   char  outputBuffer[MAXIMUM_MESSAGE_SIZE];
   auto* dataMsg = reinterpret_cast<NetPerfMeterDataMessage*>(&outputBuffer);

   if(bytesToSend < sizeof(NetPerfMeterDataMessage)) {
      bytesToSend = sizeof(NetPerfMeterDataMessage);
   }

   // ====== Prepare NETPERFMETER_DATA message ==============================
   // ------ Create header --------------------------------
   dataMsg->Header.Type   = NETPERFMETER_DATA;
   dataMsg->Header.Flags  = 0x00;
   if(isFrameBegin) {
      dataMsg->Header.Flags |= NPMDF_FRAME_BEGIN;
   }
   if(isFrameEnd) {
      dataMsg->Header.Flags |= NPMDF_FRAME_END;
   }
   dataMsg->Header.Length = htobe16(static_cast<uint16_t>(bytesToSend));
   dataMsg->MeasurementID = htobe64(flow->getMeasurementID());
   dataMsg->FlowID        = htobe32(flow->getFlowID());
   dataMsg->StreamID      = htobe16(flow->getStreamID());
   dataMsg->Padding       = 0x0000;
   dataMsg->FrameID       = htobe32(frameID);
   dataMsg->SeqNumber     = htobe64(flow->nextOutboundSeqNumber());
   dataMsg->ByteSeqNumber = htobe64(flow->getCurrentBandwidthStats().TransmittedBytes + byteOffset);
   dataMsg->TimeStamp     = htobe64(now.value);

   // ------ Create payload data pattern ------------------
   fillPayload(reinterpret_cast<unsigned char*>(&dataMsg->Payload),
               bytesToSend - sizeof(NetPerfMeterDataMessage),
               flow->isAcceptedIncomingFlow());

   // ====== Send NETPERFMETER_DATA message =================================
   ssize_t sent;
   if( (flow->getTrafficSpec().Protocol == IPPROTO_TCP)
#if defined(HAVE_MPTCP)
       || (flow->getTrafficSpec().Protocol == IPPROTO_MPTCP)
#endif
     ) {
      int flags = 0;
#if defined(MSG_MORE)
      if(!isFrameEnd) {
         flags |= MSG_MORE;   // Accumulate chunks belonging to the frame
      }
#endif
      sent = ext_send(sd, outputBuffer, bytesToSend, flags);
   }
   else if(flow->getTrafficSpec().Protocol == IPPROTO_UDP) {
      if(flow->isRemoteAddressValid()) {
         const FlowTrafficSpec& spec = flow->getTrafficSpec();
         const bool useSendMsg =
            spec.TrafficClass.has_value() ||
            (spec.FlowLabel.Mode == FlowLabelMode::Explicit);
         if(useSendMsg) {
            sockaddr_union dst;
            memset(&dst, 0, sizeof(dst));
            const size_t dstLen = getSocklen(flow->getRemoteAddress());
            memcpy(&dst, flow->getRemoteAddress(), dstLen);

            const bool isNativeIPv6 =
               (dst.sa.sa_family == AF_INET6) && (!IN6_IS_ADDR_V4MAPPED(&dst.in6.sin6_addr));
            if(isNativeIPv6 && (spec.FlowLabel.Mode == FlowLabelMode::Explicit)) {
               dst.in6.sin6_flowinfo = htobe32(spec.FlowLabel.Label & 0x000fffffU);
            }

            struct iovec iov;
            iov.iov_base = outputBuffer;
            iov.iov_len  = bytesToSend;

            struct msghdr msg;
            memset(&msg, 0, sizeof(msg));
            msg.msg_name    = &dst;
            msg.msg_namelen = static_cast<socklen_t>(dstLen);
            msg.msg_iov     = &iov;
            msg.msg_iovlen  = 1;

            alignas(struct cmsghdr) char cmsgBuf[CMSG_SPACE(sizeof(int))];
            memset(cmsgBuf, 0, sizeof(cmsgBuf));
            if(spec.TrafficClass.has_value()) {
               const uint8_t tcByte = static_cast<uint8_t>(*spec.TrafficClass & 0xfcU);
               if(isNativeIPv6) {
#if defined(IPV6_TCLASS)
                  msg.msg_control    = cmsgBuf;
                  msg.msg_controllen = CMSG_SPACE(sizeof(int));
                  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
                  cmsg->cmsg_level = IPPROTO_IPV6;
                  cmsg->cmsg_type  = IPV6_TCLASS;
                  cmsg->cmsg_len   = CMSG_LEN(sizeof(int));
                  const int tclass = static_cast<int>(tcByte);
                  memcpy(CMSG_DATA(cmsg), &tclass, sizeof(tclass));
#endif
               }
               else {
#if defined(IP_TOS)
                  msg.msg_control    = cmsgBuf;
                  msg.msg_controllen = CMSG_SPACE(sizeof(uint8_t));
                  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
                  cmsg->cmsg_level = IPPROTO_IP;
                  cmsg->cmsg_type  = IP_TOS;
                  cmsg->cmsg_len   = CMSG_LEN(sizeof(uint8_t));
                  memcpy(CMSG_DATA(cmsg), &tcByte, sizeof(tcByte));
#endif
               }
            }

            sent = ext_sendmsg(sd, &msg, 0);
            if((sent < 0) && ((errno == EINVAL) || (errno == ENOPROTOOPT)) && (msg.msg_controllen > 0)) {
               static bool warnedCmsgUnsupported = false;
               if(!warnedCmsgUnsupported) {
                  warnedCmsgUnsupported = true;
                  LOG_WARNING
                  // TRANSLATORS: Warning when per-packet UDP Traffic Class cmsg is rejected by the OS. Placeholder: {} is strerror() message.
                  stdlog << formatText(gettext("Per-packet UDP Traffic Class via sendmsg() is unsupported on this OS ({}); falling back to plain send!"),
                                       strerror(errno)) << "\n";
                  LOG_END
               }
               msg.msg_control    = nullptr;
               msg.msg_controllen = 0;
               sent = ext_sendmsg(sd, &msg, 0);
            }
         }
         else {
            sent = ext_sendto(sd,
                              outputBuffer, bytesToSend, 0,
                              flow->getRemoteAddress(),
                              getSocklen(flow->getRemoteAddress()));
         }
      }
      else {
         sent = ext_send(sd,
                         outputBuffer, bytesToSend, 0);
      }
   }
#if defined(HAVE_SCTP)
   else if(flow->getTrafficSpec().Protocol == IPPROTO_SCTP) {
      sctp_sndrcvinfo sinfo;
      memset(&sinfo, 0, sizeof(sinfo));
      sinfo.sinfo_stream   = flow->getStreamID();
      sinfo.sinfo_ppid     = htobe32(PPID_NETPERFMETER_DATA);
      if(flow->getTrafficSpec().ReliableMode < 1.0) {
         const bool sendUnreliable = (randomDouble() > flow->getTrafficSpec().ReliableMode);
         if(sendUnreliable) {
            if(flow->getTrafficSpec().RetransmissionTrialsInMS) {
               sinfo.sinfo_timetolive = flow->getTrafficSpec().RetransmissionTrials;
               sinfo.sinfo_flags |= SCTP_PR_SCTP_TTL;
            }
            else {
#if defined(SCTP_PR_SCTP_RTX)
               sinfo.sinfo_timetolive = flow->getTrafficSpec().RetransmissionTrials;
               sinfo.sinfo_flags |= SCTP_PR_SCTP_RTX;
#else
#warning SCTP option SCTP_PR_SCTP_RTX is not supported by the API of this system!
#endif
            }
         }
      }
      if(flow->getTrafficSpec().OrderedMode < 1.0) {
         const bool sendUnordered = (randomDouble() > flow->getTrafficSpec().OrderedMode);
         if(sendUnordered) {
            sinfo.sinfo_flags |= SCTP_UNORDERED;
         }
      }
      sent = sctp_send(sd,
                       outputBuffer, bytesToSend,
                       &sinfo, 0);
   }
#endif
#if defined(HAVE_QUIC)
   else if(flow->getTrafficSpec().Protocol == IPPROTO_QUIC) {
      // Both active and passive sides use the client-initiated bidirectional
      // stream created by NETPERFMETER_IDENTIFY_FLOW ((StreamID << 2) | 0).
      const int64_t sid = static_cast<int64_t>(flow->getStreamID()) << 2;
      sent = quic_sendmsg(sd, outputBuffer, bytesToSend, sid, 0);
   }
#endif
   else {
      sent = ext_send(sd, outputBuffer, bytesToSend, 0);
   }

   // ====== Check, whether flow has been aborted unintentionally ===========
   if( (sent < 0) &&
       (errno != EAGAIN) &&
       (errno != EWOULDBLOCK) &&
       (!flow->isAcceptedIncomingFlow()) &&
       (flow->getTrafficSpec().ErrorOnAbort) &&
       (flow->getOutputStatus() == Flow::On) &&
       (!flow->isStopping()) ) {
      LOG_ERROR
      // TRANSLATORS: Error message when a data flow is unexpectedly terminated. Placeholders: {} 1: flow ID, 2: socket descriptor, 3: strerror() message.
      stdlog << formatText(gettext("Flow #{} on socket {} has been aborted: {}!"),
                           flow->getFlowID(), sd,
                           strerror(errno)) << "\n";
      LOG_END
      flow->stop();
      sendBreak(true);
   }

   return sent;
}


// ###### Transmit data frame ###############################################
bool transmitFrame(Flow*                    flow,
                   const MonotonicMicroTime now)
{
   // ====== Obtain length of data to send ==================================
   ssize_t bytesToSend =
      static_cast<ssize_t>(rint(getRandomValue(static_cast<const double*>(flow->getTrafficSpec().OutboundFrameSize),
                                               flow->getTrafficSpec().OutboundFrameSizeRng)));
   ssize_t  bytesSent   = 0;
   uint64_t packetsSent = 0;
   uint64_t framesSent  = 0;
   if(bytesToSend > 0) {
      const uint32_t frameID = flow->nextOutboundFrameID();
      bool           success = true;
      while(bytesSent < bytesToSend) {
         // ====== Send message =============================================
         ssize_t chunkSize = std::min(bytesToSend - bytesSent,
                                      std::min(static_cast<ssize_t>(flow->getTrafficSpec().MaxMsgSize),
                                               static_cast<ssize_t>(MAXIMUM_MESSAGE_SIZE)));
         const ssize_t sent =
            sendNetPerfMeterData(flow, frameID,
                                 (bytesSent == 0),                       // Is frame begin?
                                 (bytesSent + chunkSize >= bytesToSend), // Is frame end?
                                 now,
                                 static_cast<uint64_t>(bytesSent),
                                 static_cast<size_t>(chunkSize));
         // NOTE: Due to minimum size for a NETPERFMETER_DATA chunk (48 B),
         //       the sent size may be >= chunkSize!

         // ====== Update statistics ========================================
         if(sent > 0) {
            bytesSent += sent;
            packetsSent++;
         }
         else {
            // Transmission error -> stop sending.
            success = false;
            break;
         }
      }
      if(success) {
         framesSent = 1;
      }
   }
   else {
      bytesToSend = 0;   // There is nothing to send
   }

   // ====== Update statistics ==============================================
   flow->updateTransmissionStatistics(now,
                                      framesSent,
                                      packetsSent,
                                      static_cast<size_t>(bytesSent));
   return (bytesSent >= bytesToSend);
}


// ###### Handle data message ###############################################
bool handleNetPerfMeterData(const bool               isActiveMode,
                            const MonotonicMicroTime nowMonotonic,
                            const SystemMicroTime    nowSystem,
                            const int                protocol,
                            const int                sd)
{
   char            inputBuffer[MAXIMUM_MESSAGE_SIZE];
   sockaddr_union  from;
   socklen_t       fromlen  = sizeof(from);
#if defined(MSG_DONTWAIT)
   int             flags    = MSG_DONTWAIT;
#else
   int             flags    = 0;
#endif
   int64_t         streamID = 0;

   // ====== Read message (or fragment) =====================================
   const ssize_t received =
      FlowManager::getFlowManager()->getMessageReader()->receiveMessage(
         sd, &inputBuffer, sizeof(inputBuffer), &from.sa, &fromlen, &streamID, &flags);
   if(received == MRRM_PARTIAL_READ) {
      return true;   // Partial read -> wait for next fragment.
   }

   // ====== Handle data ====================================================
   if(received > 0) {
#if defined(HAVE_SCTP)
      if(!(flags & MSG_NOTIFICATION)) {
#endif
         const auto* dataMsg     = reinterpret_cast<const NetPerfMeterDataMessage*>(&inputBuffer);
         const auto* identifyMsg = reinterpret_cast<const NetPerfMeterIdentifyMessage*>(&inputBuffer);

         // ====== Handle NETPERFMETER_IDENTIFY_FLOW message ================
         if( (received >= static_cast<ssize_t>(sizeof(NetPerfMeterIdentifyMessage))) &&
             (identifyMsg->Header.Type == NETPERFMETER_IDENTIFY_FLOW) &&
             (be64toh(identifyMsg->MagicNumber) == NETPERFMETER_IDENTIFY_FLOW_MAGIC_NUMBER) ) {
            const bool identifyOkay =
               handleNetPerfMeterIdentify(identifyMsg, sd, &from);
            if(!identifyOkay) {
               LOG_WARNING
               // TRANSLATORS: Warning message when handling flow identification fails. Placeholder: {} is socket descriptor.
               stdlog << formatText(gettext("Failed handling NETPERFMETER_IDENTIFY on socket {}!"), sd) << "\n";
               LOG_END
               if(protocol != IPPROTO_UDP) {
                  ext_shutdown(sd, SHUT_RDWR);
               }
               return false;
            }
         }

         // ====== Handle NETPERFMETER_DATA message =========================
         else if( (received >= static_cast<ssize_t>(sizeof(NetPerfMeterDataMessage))) &&
                  (dataMsg->Header.Type == NETPERFMETER_DATA) ) {
            // ====== Identify flow =========================================
            Flow* flow = nullptr;
            if(protocol == IPPROTO_UDP) {
               // UDP is connectionless and shares sockets on the passive node.
               // Identify flow directly via in-band message header:
               flow = FlowManager::getFlowManager()->findFlow(
                         be64toh(dataMsg->MeasurementID),
                         be32toh(dataMsg->FlowID),
                         be16toh(dataMsg->StreamID));
               if(!flow && !isActiveMode) {
                  // Fallback for initial unmapped datagrams
                  flow = FlowManager::getFlowManager()->findFlow(&from.sa);
               }
            }
#if defined(HAVE_SCTP)
            else if(protocol == IPPROTO_SCTP) {
               // Flow ID = SCTP stream ID:
               flow = FlowManager::getFlowManager()->findFlow(sd, static_cast<uint16_t>(streamID));
            }
#endif
#if defined(HAVE_QUIC)
            else if(protocol == IPPROTO_QUIC) {
               // Flow ID = QUIC stream ID (remove 2 last bits):
               const uint16_t flowStreamID = static_cast<uint16_t>(streamID >> 2);
               flow = FlowManager::getFlowManager()->findFlow(sd, flowStreamID);
            }
#endif
            else {
               // Flow ID is always 0 (TCP, MPTCP, DCCP):
               flow = FlowManager::getFlowManager()->findFlow(sd, 0);
            }

            if(flow) {
               // Update flow statistics by received NETPERFMETER_DATA message.
               updateStatistics(flow, nowMonotonic, nowSystem,
                                dataMsg, static_cast<size_t>(received));
            }
            else {
               LOG_WARNING
               // TRANSLATORS: Warning message when data message is received for an unknown flow. Placeholder: {} is socket descriptor.
               stdlog << formatText(gettext("Received NETPERFMETER_DATA for unknown flow on socket {}!"), sd) << "\n";
               LOG_END
               if(protocol != IPPROTO_UDP) {
                  ext_shutdown(sd, SHUT_RDWR);
               }
               return false;
            }
         }
         else {
            LOG_WARNING
            // TRANSLATORS: Warning message when an unrecognized packet is received on a data socket. Placeholder: {} is socket descriptor.
            stdlog << formatText(gettext("Received garbage on socket {}!"), sd) << "\n";
            LOG_END
            if(protocol != IPPROTO_UDP) {
               ext_shutdown(sd, SHUT_RDWR);
            }
            return false;
         }
#if defined(HAVE_SCTP)
      }
#endif
   }

   // ====== Handle error ===================================================
   else {
      Flow* flow = FlowManager::getFlowManager()->findFlow(sd, 0);
      if(flow) {
         flow->lock();
         if(!flow->isAcceptedIncomingFlow()) {
            // The outgoing flow on the active side is closed:
            LOG_WARNING
            // TRANSLATORS: Warning message when end of input is reached for an active flow. Placeholders: {} 1: flow ID, 2: socket descriptor.
            stdlog << formatText(gettext("End of input for flow #{} on socket {}!"),
                                 flow->getFlowID(), sd) << "\n";
            LOG_END
         }
         else {
            // This is probably just the regular connection shutdown:
            LOG_DEBUG
            // TRANSLATORS: Debug message when regular end of input is reached for an accepted incoming flow. Placeholders: {} 1: flow ID, 2: socket descriptor.
            stdlog << formatText(gettext("End of input for flow #{} on socket {}!"),
                                 flow->getFlowID(), sd) << "\n";
            LOG_END
         }
         flow->unlock();
         flow->endOfInput();
         return flow->isAcceptedIncomingFlow();   // No error for incoming flow!
      }
      else {
         LOG_WARNING
         // TRANSLATORS: Warning message when end of input is reached on an unidentified socket. Placeholder: {} is socket descriptor.
         stdlog << formatText(gettext("End of input for unidentified flow on socket {}!"),
                              sd) << "\n";
         LOG_END
      }
      if(protocol != IPPROTO_UDP) {
         ext_shutdown(sd, SHUT_RDWR);
      }
      return false;
   }

   return true;
}


// ###### Update flow statistics with incoming NETPERFMETER_DATA message ####
static void updateStatistics(Flow*                          flow,
                             const MonotonicMicroTime       nowMonotonic,
                             const SystemMicroTime          nowSystem,
                             const NetPerfMeterDataMessage* dataMsg,
                             const size_t                   receivedBytes)
{
   // ====== Update QoS statistics ==========================================
   const uint64_t seqNumber   = be64toh(dataMsg->SeqNumber);
   const uint64_t timeStamp   = be64toh(dataMsg->TimeStamp);
   const double   transitTime = (static_cast<double>(nowMonotonic.value) - static_cast<double>(timeStamp)) / 1000.0;

   // ------ Jitter calculation according to RFC 3550 -----------------------
   /* From RFC 3550:
      int transit = arrival - r->ts;
      int d = transit - s->transit;
      s->transit = transit;
      if (d < 0) d = -d;
      s->jitter += (1./16.) * ((double)d - s->jitter);
   */
   const double diff   = transitTime - flow->getDelay();
   const double jitter = flow->getJitter() + (1.0 / 16.0) * (fabs(diff) - flow->getJitter());

   // ------ Loss calculation -----------------------------------------------
   flow->getDefragmenter()->addFragment(nowMonotonic.value, dataMsg);
   size_t receivedFrames;
   size_t lostFrames;
   size_t lostPackets;
   size_t lostBytes;
   flow->getDefragmenter()->purge(nowMonotonic.value, flow->getTrafficSpec().DefragmentTimeout,
                                  receivedFrames, lostFrames, lostPackets, lostBytes);

   flow->updateReceptionStatistics(
      nowMonotonic, nowSystem,
      receivedFrames, receivedBytes,
      lostFrames, lostPackets, lostBytes,
      seqNumber, transitTime, diff, jitter);
}
