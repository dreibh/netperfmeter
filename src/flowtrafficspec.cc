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

#include "flowtrafficspec.h"
#include "tools.h"


// ###### Show configuration entry (value + random number generator) ########
void FlowTrafficSpec::showEntry(std::ostream& os,
                                const double* valueArray,
                                const uint8_t rng)
{
   switch(rng) {
      case RANDOM_CONSTANT:
         // TRANSLATORS: Random distribution parameter: constant value. Placeholder: {:1.6f} is constant value.
         os << formatText(gettext("{:1.6f} (constant)"), valueArray[0]) << " ";
         break;
      case RANDOM_EXPONENTIAL:
         // TRANSLATORS: Random distribution parameter: negative exponential distribution. Placeholder: {:1.6f} is mean value.
         os << formatText(gettext("mean={:1.6f} (neg. exp.)"), valueArray[0]) << " ";
         break;
      case RANDOM_UNIFORM:
         // TRANSLATORS: Random distribution parameter: uniform distribution interval. Placeholders: {:1.6f} 1: lower bound, {:1.3f} 2: upper bound.
         os << formatText(gettext("interval=[{:1.6f}, {:1.3f}) (uniform)"), valueArray[0], valueArray[1]) << " ";
         break;
      case RANDOM_PARETO:
         // TRANSLATORS: Random distribution parameter: Pareto distribution. Placeholders: {:1.6f} 1: location parameter, {:1.6f} 2: shape parameter.
         os << formatText(gettext("location={:1.6f}, shape={:1.6f} (pareto)"), valueArray[0], valueArray[1]) << " ";
         break;
      case RANDOM_NORMAL:
         // TRANSLATORS: Random distribution parameter: normal distribution. Placeholders: {:1.6f} 1: mean value, {:1.6f} 2: standard deviation.
         os << formatText(gettext("mean={:1.6f}, stddev={:1.6f} (normal)"), valueArray[0], valueArray[1]) << " ";
         break;
      case RANDOM_TRUNCATED_NORMAL:
         // TRANSLATORS: Random distribution parameter: truncated normal distribution. Placeholders: {:1.6f} 1: mean value, {:1.6f} 2: standard deviation.
         os << formatText(gettext("mean={:1.6f}, stddev={:1.6f} (truncated normal)"), valueArray[0], valueArray[1]) << " ";
         break;
      default:
         // TRANSLATORS: Fallback text when an unknown random distribution is specified.
         os << gettext("unknown?!") << " ";
         break;
   }
}


// ###### Print FlowTrafficSpec #############################################
void FlowTrafficSpec::print(std::ostream& os) const
{
   // TRANSLATORS: Socket receive buffer size. Placeholder: {} is buffer size in bytes.
   os << formatText(gettext(" - Receive Buffer Size: {}"), RcvBufferSize) << "\n";
   // TRANSLATORS: Socket send buffer size. Placeholder: {} is buffer size in bytes.
   os << formatText(gettext(" - Send Buffer Size:    {}"), SndBufferSize) << "\n";
   // TRANSLATORS: Maximum message size in bytes. Placeholder: {} is size in bytes.
   os << formatText(gettext(" - Max. Message Size:   {}"), MaxMsgSize) << "\n";
   // TRANSLATORS: Defragmentation timeout in milliseconds. Placeholder: {} is duration in ms.
   os << formatText(gettext(" - Defragment Timeout:  {} ms"), DefragmentTimeout / 1000ULL) << "\n";
   // TRANSLATORS: Outbound frame transmission rate label.
   os << gettext(" - Outbound Frame Rate: ");
   showEntry(os, static_cast<const double*>(OutboundFrameRate), OutboundFrameRateRng);
   // TRANSLATORS: Outbound frame size label.
   os << "\n" << gettext(" - Outbound Frame Size: ");
   showEntry(os, static_cast<const double*>(OutboundFrameSize), OutboundFrameSizeRng);
   // TRANSLATORS: Inbound frame rate label.
   os << "\n" << gettext(" - Inbound Frame Rate:  ");
   showEntry(os, static_cast<const double*>(InboundFrameRate), InboundFrameRateRng);
   // TRANSLATORS: Inbound frame size label.
   os << "\n" << gettext(" - Inbound Frame Size:  ");
   showEntry(os, static_cast<const double*>(InboundFrameSize), InboundFrameSizeRng);
   os << "\n";
   if(Protocol == IPPROTO_SCTP) {
      // TRANSLATORS: SCTP ordered delivery mode percentage. Placeholder: {} is percentage string.
      os << formatText(gettext(" - Ordered Mode:        {}"), formatText("{:1.3f}%", 100.0 * OrderedMode)) << "\n";
      // TRANSLATORS: SCTP reliable delivery mode percentage. Placeholder: {} is percentage string.
      os << formatText(gettext(" - Reliable Mode:       {}"), formatText("{:1.3f}%", 100.0 * ReliableMode)) << "\n";
      // TRANSLATORS: Parameter label for SCTP retransmissions.
      os << gettext(" - Retransmissions:     ");
      if((RetransmissionTrialsInMS) && (RetransmissionTrials == static_cast<uint32_t>(-1))) {
         // TRANSLATORS: Indicator for unlimited retransmissions.
         os << gettext("unlimited");
      }
      else if(RetransmissionTrialsInMS) {
         // TRANSLATORS: SCTP retransmission timeout in milliseconds. Placeholder: {} is duration in ms.
         os << formatText(gettext("{} ms"), RetransmissionTrials);
      }
      else {
         // TRANSLATORS: SCTP retransmission attempt count. Placeholder: {} is trial count.
         os << formatText(gettext("{} trials"), RetransmissionTrials);
      }
      os << "\n";
   }

   // TRANSLATORS: Label for on/off traffic patterns.
   os << gettext(" - On/Off:              { ");
   if(!OnOffEvents.empty()) {
      bool start = true;
      for(std::vector<OnOffEvent>::const_iterator iterator = OnOffEvents.begin();
          iterator != OnOffEvents.end(); iterator++) {
         os << "\n" << "         ";
         if(start) {
            // TRANSLATORS: On-period prefix indicator for on/off traffic pattern.
            os << gettext("🟢on:  ");
         }
         else {
            // TRANSLATORS: Off-period prefix indicator for on/off traffic pattern.
            os << gettext("🔴off: ");
         }
         showEntry(os, static_cast<const double*>((*iterator).ValueArray), (*iterator).RandNumGen);
         start = !start;
      }
   }
   else {
      os << "*0 ";
   }
   os << "}\n"
      // TRANSLATORS: Parameter label for repeating on/off traffic pattern. Placeholder: {} is yes/no.
      << formatText(gettext(" - Repeat On/Off:       {}"), RepeatOnOff ? gettext("yes") : gettext("no")) << "\n";

   // TRANSLATORS: Error on abort configuration parameter label. Placeholder: {} is yes/no.
   os << formatText(gettext(" - Error on Abort:      {}"), ErrorOnAbort ? gettext("yes") : gettext("no")) << "\n";
   if((Protocol == IPPROTO_SCTP) || (Protocol == IPPROTO_TCP)
#if defined(HAVE_MPTCP)
      || (Protocol == IPPROTO_MPTCP)
#endif
     ) {
      // TRANSLATORS: TCP/SCTP Nagle algorithm status (No Delay). Placeholder: {} is yes/no.
      os << formatText(gettext(" - No Delay:            {}"), NoDelay ? gettext("yes") : gettext("no")) << "\n";
   }
   if((Protocol == IPPROTO_TCP)
#if defined(HAVE_MPTCP)
      || (Protocol == IPPROTO_MPTCP)
#endif
     ) {
      // TRANSLATORS: Congestion control algorithm label. Placeholder: {} is the algorithm name.
      os << formatText(gettext(" - Congestion Control:  {}"), CongestionControl) << "\n";
   }
   if(Protocol == IPPROTO_SCTP) {
      std::string cmtMode;
      switch(CMT) {
         case NPAF_PRIMARY_PATH:
            // TRANSLATORS: CMT disabled mode description.
            cmtMode = gettext("(off)");
            break;
         case NPAF_CMT:
            cmtMode = "(CMT)";
            break;
         case NPAF_CMTRPv1:
            cmtMode = "(CMT/RPv1)";
            break;
         case NPAF_CMTRPv2:
            cmtMode = "(CMT/RPv2)";
            break;
         case NPAF_LikeMPTCP:
            cmtMode = "(MPTCP)";
            break;
         default:
            cmtMode = "";
            break;
      }
      // TRANSLATORS: Concurrent Multipath Transfer setting. Placeholders: {} 1: CMT number, 2: CMT mode name.
      os << formatText(gettext(" - CMT:                 #{} {}"), static_cast<unsigned int>(CMT), cmtMode) << "\n";
   }
#if defined(HAVE_DCCP)
   if(Protocol == IPPROTO_DCCP) {
      // TRANSLATORS: DCCP Congestion Control ID setting. Placeholder: {} is CCID number.
      os << formatText(gettext(" - CCID:                #{}"), static_cast<unsigned int>(CCID)) << "\n";
   }
#endif
}


// ###### Reset FlowTrafficSpec #############################################
void FlowTrafficSpec::reset()
{
   *this = FlowTrafficSpec{};
}
