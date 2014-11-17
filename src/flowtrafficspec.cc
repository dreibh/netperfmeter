/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2013 by Sebastian Wallat (TCP No delay)
 * Copyright (C) 2009-2015 by Thomas Dreibholz
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
 * Contact: sebastian.wallat@uni-due.de
 *          dreibh@iem.uni-due.de
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
         snprintf((char*)&str, sizeof(str), "%1.6lf (neg. exp.)",
                  valueArray[0]);
       break;
      case RANDOM_UNIFORM:
         snprintf((char*)&str, sizeof(str), "%1.6lf +/- %1.3lf%% (uniform)",
                  valueArray[0], 100.0 * valueArray[1]);
       break;
      case RANDOM_PARETO:
         snprintf((char*)&str, sizeof(str), "m=%1.6lf, k=%1.6lf%% (pareto)",
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
   os << "      - Receive Buffer Size: "
      << RcvBufferSize << std::endl;
   os << "      - Send Buffer Size:    "
      << SndBufferSize << std::endl;
   os << "      - Max. Message Size:   "
      << MaxMsgSize << std::endl;
   os << "      - Defragment Timeout:  "
      << DefragmentTimeout / 1000 << "ms" << std::endl;
   os << "      - Outbound Frame Rate: ";
   showEntry(os, (const double*)&OutboundFrameRate, OutboundFrameRateRng);
   os << std::endl;
   os << "      - Outbound Frame Size: ";
   showEntry(os, (const double*)&OutboundFrameSize, OutboundFrameSizeRng);
   os << std::endl;
   os << "      - Inbound Frame Rate:  ";
   showEntry(os, (const double*)&InboundFrameRate, InboundFrameRateRng);
   os << std::endl;
   os << "      - Inbound Frame Size:  ";
   showEntry(os, (const double*)&InboundFrameSize, InboundFrameSizeRng);
   os << std::endl;
   if(Protocol == IPPROTO_SCTP) {
      char ordered[32];
      char reliable[32];
      snprintf((char*)&ordered, sizeof(ordered), "%1.2f%%", 100.0 * OrderedMode);
      snprintf((char*)&reliable, sizeof(reliable), "%1.2f%%", 100.0 * ReliableMode);
      os << "      - Ordered Mode:        " << ordered  << std::endl;
      os << "      - Reliable Mode:       " << reliable << std::endl;
      os << "      - Retransmissions:     ";
      if( (RetransmissionTrialsInMS) && (RetransmissionTrials == ~((uint32_t)0)) ) {
         os << "unlimited";
      }
      else {
         os << RetransmissionTrials
            << (RetransmissionTrialsInMS ? "ms" : " trials");
      }
      os << std::endl;
   }

   os << "      - On/Off:              {";
   if(OnOffEvents.size() > 0) {
      bool start = true;
      for(std::vector<OnOffEvent>::const_iterator iterator = OnOffEvents.begin();
          iterator != OnOffEvents.end();iterator++) {
         os << std::endl << "         " << ((start == true) ? "* " : "~ ");
         showEntry(os, (const double*)&(*iterator).ValueArray, (*iterator).RandNumGen);
         start = !start;
      }
   }
   else {
      os << "*0 ";
   }
   os << "}" << std::endl
      << "      - Repeat On/Off:       " << ((RepeatOnOff == true) ? "yes" : "no") << std::endl;

   os << "      - Error on Abort:      "
      << ((ErrorOnAbort == true) ? "yes" : "no") << std::endl
      << "      - Debug:               "
      << ((Debug == true) ? "yes" : "no") << std::endl
      << "      - No Delay:            "
      << ((NoDelay == true) ? "yes" : "no") << std::endl
      << "      Congestion Control:    " << CongestionControl << std::endl
      << "      Number of Diff. Ports: " << NDiffPorts        << std::endl
      << "      Path Manager:          " << PathMgr           << std::endl;

   os << "      - CMT:                 #" << (unsigned int)CMT << " ";
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
   os << std::endl;
   os << "      - CCID:                #" << (unsigned int)CCID << std::endl;
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
   RepeatOnOff              = false;
   NDiffPorts               = 4;
   PathMgr                  = "default";
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
