/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2025 by Thomas Dreibholz
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

#include "flowtrafficspec.h"


// ###### Constructor #######################################################
FlowTrafficSpec::FlowTrafficSpec()
{
   reset();
}


// ###### Destructor ########################################################
FlowTrafficSpec::~FlowTrafficSpec()
{
}


// ###### Show configuration entry (value + random number generator) ########
void FlowTrafficSpec::showEntry(std::ostream& os,
                                const double* valueArray,
                                const uint8_t rng)
{
   char str[256];

   switch(rng) {
      case RANDOM_CONSTANT:
         snprintf((char*)&str, sizeof(str), "%1.6lf (constant)",
                  valueArray[0]);
       break;
      case RANDOM_EXPONENTIAL:
         snprintf((char*)&str, sizeof(str), "mean=%1.6lf (neg. exp.)",
                  valueArray[0]);
       break;
      case RANDOM_UNIFORM:
         snprintf((char*)&str, sizeof(str), "internal=[%1.6lf, %1.3lf) (uniform)",
                  valueArray[0], valueArray[1]);
       break;
      case RANDOM_PARETO:
         snprintf((char*)&str, sizeof(str), "location=%1.6lf, shape=%1.6lf (pareto)",
                  valueArray[0], valueArray[1]);
       break;
      case RANDOM_NORMAL:
         snprintf((char*)&str, sizeof(str), "mean=%1.6lf, stddev=%1.6lf (normal)",
                  valueArray[0], valueArray[1]);
       break;
      case RANDOM_TRUNCATED_NORMAL:
         snprintf((char*)&str, sizeof(str), "mean=%1.6lf, stddev=%1.6lf (truncated normal)",
                  valueArray[0], valueArray[1]);
       break;
      default:
         snprintf((char*)&str, sizeof(str), "unknown?!");
       break;
   }

   os << str << " ";
}


// ###### Print FlowTrafficSpec #############################################
void FlowTrafficSpec::print(std::ostream& os) const
{
   os << " - Receive Buffer Size: " << RcvBufferSize << "\n";
   os << " - Send Buffer Size:    " << SndBufferSize << "\n";
   os << " - Max. Message Size:   " << MaxMsgSize << "\n";
   os << " - Defragment Timeout:  " << DefragmentTimeout / 1000 << "ms\n";
   os << " - Outbound Frame Rate: ";
   showEntry(os, (const double*)&OutboundFrameRate, OutboundFrameRateRng);
   os << "\n - Outbound Frame Size: ";
   showEntry(os, (const double*)&OutboundFrameSize, OutboundFrameSizeRng);
   os << "\n - Inbound Frame Rate:  ";
   showEntry(os, (const double*)&InboundFrameRate, InboundFrameRateRng);
   os << "\n - Inbound Frame Size:  ";
   showEntry(os, (const double*)&InboundFrameSize, InboundFrameSizeRng);
   os << "\n";
   if(Protocol == IPPROTO_SCTP) {
      char ordered[32];
      char reliable[32];
      snprintf((char*)&ordered, sizeof(ordered), "%1.3f%%", 100.0 * OrderedMode);
      snprintf((char*)&reliable, sizeof(reliable), "%1.3f%%", 100.0 * ReliableMode);
      os << " - Ordered Mode:        " << ordered  << "\n";
      os << " - Reliable Mode:       " << reliable << "\n";
      os << " - Retransmissions:     ";
      if( (RetransmissionTrialsInMS) && (RetransmissionTrials == ~((uint32_t)0)) ) {
         os << "unlimited";
      }
      else {
         os << RetransmissionTrials
            << (RetransmissionTrialsInMS ? "ms" : " trials");
      }
      os << "\n";
   }

   os << " - On/Off:              { ";
   if(OnOffEvents.size() > 0) {
      bool start = true;
      for(std::vector<OnOffEvent>::const_iterator iterator = OnOffEvents.begin();
          iterator != OnOffEvents.end();iterator++) {
         os << "\n" << "         " << ((start == true) ? "🟢on:  " : "🔴off: ");
         showEntry(os, (const double*)&(*iterator).ValueArray, (*iterator).RandNumGen);
         start = !start;
      }
   }
   else {
      os << "*0 ";
   }
   os << "}\n"
      << " - Repeat On/Off:       " << ((RepeatOnOff == true) ? "yes" : "no") << "\n";

   os << " - Error on Abort:      "
      << ((ErrorOnAbort == true) ? "yes" : "no") << "\n";
   if( (Protocol == IPPROTO_SCTP) || (Protocol == IPPROTO_TCP)
#ifdef HAVE_MPTCP
       || (Protocol == IPPROTO_MPTCP)
#endif
     ) {
      os << " - No Delay:            "
         << ((NoDelay == true) ? "yes" : "no") << "\n";
   }
   if( (Protocol == IPPROTO_TCP)
#ifdef HAVE_MPTCP
       || (Protocol == IPPROTO_MPTCP)
#endif
     ) {
      os << " - Congestion Control:  " << CongestionControl << "\n";
   }
   if(Protocol == IPPROTO_SCTP) {
      os << " - CMT:                 #" << (unsigned int)CMT << " ";
      switch(CMT) {
         case NPAF_PRIMARY_PATH:
            os << "(off)";
         break;
         case NPAF_CMT:
            os << "(CMT)";
         break;
         case NPAF_CMTRPv1:
            os << "(CMT/RPv1)";
         break;
         case NPAF_CMTRPv2:
            os << "(CMT/RPv2)";
         break;
         case NPAF_LikeMPTCP:
            os << "(MPTCP)";
         break;
      }
      os << "\n";
   }
   if(Protocol == IPPROTO_DCCP) {
      os << " - CCID:                #" << (unsigned int)CCID << "\n";
   }
}


// ###### Reset FlowTrafficSpec #############################################
void FlowTrafficSpec::reset()
{
   MaxMsgSize               = 16000;
   DefragmentTimeout        = 5000000;
   SndBufferSize            = 233016;   // Upper limit for FreeBSD
   RcvBufferSize            = 233016;   // Upper limit for FreeBSD
   OrderedMode              = 1.0;
   ReliableMode             = 1.0;
   RetransmissionTrials     = ~0;
   RetransmissionTrialsInMS = true;
   ErrorOnAbort             = true;
   Debug                    = false;
   NoDelay                  = false;
   BindV6Only               = false;
   RepeatOnOff              = false;
   CongestionControl        = "default";
   CMT                      = 0x00;
   CCID                     = 0x00;
   for(size_t i = 0;i < NETPERFMETER_RNG_INPUT_PARAMETERS;i++) {
      OutboundFrameRate[i] = 0.0;
      OutboundFrameSize[i] = 0.0;
      InboundFrameRate[i]  = 0.0;
      InboundFrameSize[i]  = 0.0;
   }
   OutboundFrameRateRng = RANDOM_CONSTANT;
   OutboundFrameSizeRng = RANDOM_CONSTANT;
   InboundFrameRateRng  = RANDOM_CONSTANT;
   InboundFrameSizeRng  = RANDOM_CONSTANT;
}
