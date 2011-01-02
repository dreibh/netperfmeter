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
      default:
         snprintf((char*)&str, sizeof(str), "unknown?!");
       break;
   }

   os << str;
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
   os << "      - Start/Stop:          { ";
   if(OnOffEvents.size() > 0) {
      bool start = true;
      for(std::set<unsigned int>::iterator iterator = OnOffEvents.begin();
          iterator != OnOffEvents.end();iterator++) {
         if(start) {
            os << "*" << (*iterator / 1000.0) << " ";
         }
         else {
            os << "~" << (*iterator / 1000.0) << " ";
         }
         start = !start;
      }
   }
   else {
      os << "*0 ";
   }
   os << "}" << std::endl;
   os << "      - Error on Abort:      "
      << ((ErrorOnAbort == true) ? "yes" : "no") << std::endl;
   os << "      - Use CMT:             "
      << ((UseCMT == true) ? "yes" : "no") << std::endl;
   os << "      - Use RP:              "
      << ((UseRP == true) ? "yes" : "no") << std::endl;
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
   UseCMT                   = false;
   UseRP                    = false;
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
