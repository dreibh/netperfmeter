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

#include <sstream>
#include <string_view>


// ###### Format configuration entry as string ##############################
static std::string formatEntry(const double* valueArray, const uint8_t rng)
{
   std::ostringstream oss;
   FlowTrafficSpec::showEntry(oss, valueArray, rng);
   std::string result = oss.str();
   while(!result.empty() && result.back() == ' ') {
      result.pop_back();
   }
   return result;
}


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


// ###### Print FlowTrafficSpec into PropertyFormatter ######################
void FlowTrafficSpec::print(PropertyFormatter& formatter, const size_t level) const
{
   // TRANSLATORS: Socket receive buffer size label.
   formatter.add(level, gettext("Receive Buffer Size"), std::to_string(RcvBufferSize));
   // TRANSLATORS: Socket send buffer size label.
   formatter.add(level, gettext("Send Buffer Size"), std::to_string(SndBufferSize));
   // TRANSLATORS: Maximum message size in bytes label.
   formatter.add(level, gettext("Max. Message Size"), std::to_string(MaxMsgSize));
   // TRANSLATORS: Defragmentation timeout label.
   formatter.add(level, gettext("Defragment Timeout"),
                 // TRANSLATORS: Defragmentation timeout value in milliseconds. Placeholder: {} is duration in ms.
                 formatText(gettext("{} ms"), DefragmentTimeout / 1000ULL));
   // TRANSLATORS: Outbound frame transmission rate label.
   formatter.add(level, gettext("Outbound Frame Rate"),
                 formatEntry(static_cast<const double*>(OutboundFrameRate), OutboundFrameRateRng));
   // TRANSLATORS: Outbound frame size label.
   formatter.add(level, gettext("Outbound Frame Size"),
                 formatEntry(static_cast<const double*>(OutboundFrameSize), OutboundFrameSizeRng));
   // TRANSLATORS: Inbound frame rate label.
   formatter.add(level, gettext("Inbound Frame Rate"),
                 formatEntry(static_cast<const double*>(InboundFrameRate), InboundFrameRateRng));
   // TRANSLATORS: Inbound frame size label.
   formatter.add(level, gettext("Inbound Frame Size"),
                 formatEntry(static_cast<const double*>(InboundFrameSize), InboundFrameSizeRng));

   if(Protocol == IPPROTO_SCTP) {
      // TRANSLATORS: SCTP ordered delivery mode percentage label.
      formatter.add(level, gettext("Ordered Mode"), formatText("{:1.3f}%", 100.0 * OrderedMode));
      // TRANSLATORS: SCTP reliable delivery mode percentage label.
      formatter.add(level, gettext("Reliable Mode"), formatText("{:1.3f}%", 100.0 * ReliableMode));

      std::string rtxValue;
      if((RetransmissionTrialsInMS) && (RetransmissionTrials == static_cast<uint32_t>(-1))) {
         // TRANSLATORS: Indicator for unlimited retransmissions.
         rtxValue = gettext("unlimited");
      }
      else if(RetransmissionTrialsInMS) {
         // TRANSLATORS: SCTP retransmission timeout in milliseconds. Placeholder: {} is duration in ms.
         rtxValue = formatText(gettext("{} ms"), RetransmissionTrials);
      }
      else {
         // TRANSLATORS: SCTP retransmission attempt count. Placeholder: {} is trial count.
         rtxValue = formatText(gettext("{} trials"), RetransmissionTrials);
      }
      // TRANSLATORS: Parameter label for SCTP retransmissions.
      formatter.add(level, gettext("Retransmissions"), std::move(rtxValue));
   }

   if(!OnOffEvents.empty()) {
      // TRANSLATORS: Label for on/off traffic patterns.
      formatter.add(level, gettext("On/Off"), "");
      bool start = true;
      for(const auto& event : OnOffEvents) {
         formatter.add(level + 1,
                       start ?
                          // TRANSLATORS: On-period label for on/off traffic pattern.
                          gettext("🟢on") :
                          // TRANSLATORS: Off-period label for on/off traffic pattern.
                          gettext("🔴off"),
                       formatEntry(static_cast<const double*>(event.ValueArray), event.RandNumGen));
         start = !start;
      }
   }
   else {
      // TRANSLATORS: Label for on/off traffic patterns.
      formatter.add(level, gettext("On/Off"), "{ *0 }");
   }
   // TRANSLATORS: Parameter label for repeating on/off traffic pattern.
   formatter.add(level, gettext("Repeat On/Off"), RepeatOnOff ? gettext("yes") : gettext("no"));
   // TRANSLATORS: Error on abort configuration parameter label.
   formatter.add(level, gettext("Error on Abort"), ErrorOnAbort ? gettext("yes") : gettext("no"));

   if((Protocol == IPPROTO_SCTP) || (Protocol == IPPROTO_TCP)
#if defined(HAVE_MPTCP)
      || (Protocol == IPPROTO_MPTCP)
#endif
     ) {
      // TRANSLATORS: TCP/SCTP Nagle algorithm status (No Delay) label.
      formatter.add(level, gettext("No Delay"), NoDelay ? gettext("yes") : gettext("no"));
   }
   if((Protocol == IPPROTO_TCP)
#if defined(HAVE_MPTCP)
      || (Protocol == IPPROTO_MPTCP)
#endif
#if defined(HAVE_QUIC)
      || (Protocol == IPPROTO_QUIC)
#endif
     ) {
      // TRANSLATORS: Congestion control algorithm label.
      formatter.add(level, gettext("Congestion Control"), CongestionControl);
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
      // TRANSLATORS: Concurrent Multipath Transfer setting label.
      formatter.add(level, gettext("CMT"),
                    cmtMode.empty()
                       ? formatText("#{}", static_cast<unsigned int>(CMT))
                       : formatText("#{} {}", static_cast<unsigned int>(CMT), cmtMode));
   }
#if defined(HAVE_DCCP)
   if(Protocol == IPPROTO_DCCP) {
      // TRANSLATORS: DCCP Congestion Control ID setting label.
      formatter.add(level, gettext("CCID"), formatText("#{}", static_cast<unsigned int>(CCID)));
   }
#endif
}


// ###### Print FlowTrafficSpec #############################################
void FlowTrafficSpec::print(std::ostream& os) const
{
   // TRANSLATORS: Text direction: LTR (Left-to-Right) or RTL (Right-to-Left)
   const bool rtl = (std::string_view(gettext("LTR")) == "RTL");
   PropertyFormatter formatter("   ", { "", "- ", "+ ", "* " }, "=", rtl);
   print(formatter, 1);
   os << formatter;
}


// ###### Reset FlowTrafficSpec #############################################
void FlowTrafficSpec::reset()
{
   *this = FlowTrafficSpec{};
}
