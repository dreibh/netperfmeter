/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2009 by Thomas Dreibholz
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
                                const double  value,
                                const uint8_t rng)
{
   char str[256];

   const char* rngName = "unknown?!";
   switch(rng) {
      case RANDOM_CONSTANT:
         rngName = "constant";
       break;
      case RANDOM_EXPONENTIAL:
         rngName = "negative exponential";
       break;
   }

   snprintf((char*)&str, sizeof(str), "%1.6lf (%s)", value, rngName);
   os << str;
}


// ###### Print FlowTrafficSpec #############################################
void FlowTrafficSpec::print(std::ostream& os) const
{
   os << "      - Max. Message Size:   "
      << MaxMsgSize << std::endl;
   os << "      - Outbound Frame Rate: ";
   showEntry(os, OutboundFrameRate, OutboundFrameRateRng);
   os << std::endl;
   os << "      - Outbound Frame Size: ";
   showEntry(os, OutboundFrameSize, OutboundFrameSizeRng);
   os << std::endl;
   os << "      - Inbound Frame Rate:  ";
   showEntry(os, InboundFrameRate, InboundFrameRateRng);
   os << std::endl;
   os << "      - Inbound Frame Size:  ";
   showEntry(os, InboundFrameSize, InboundFrameSizeRng);
   os << std::endl;
   if(Protocol == IPPROTO_SCTP) {
      char ordered[32];
      char reliable[32];
      snprintf((char*)&ordered, sizeof(ordered), "%1.2f%%", 100.0 * OrderedMode);
      snprintf((char*)&reliable, sizeof(reliable), "%1.2f%%", 100.0 * ReliableMode);
      os << "      - Ordered Mode:        " << ordered  << std::endl;
      os << "      - Reliable Mode:       " << reliable << std::endl;
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
}


// ###### Reset FlowTrafficSpec #############################################
void FlowTrafficSpec::reset()
{
   MaxMsgSize               = 16000;
   OrderedMode              = 1.0;
   ReliableMode             = 1.0;
   OutboundFrameRate        = 0.0;
   OutboundFrameSize        = 0.0;
   OutboundFrameRateRng     = RANDOM_CONSTANT;
   OutboundFrameSizeRng     = RANDOM_CONSTANT;
   InboundFrameRate         = 0.0;
   InboundFrameSize         = 0.0;
   InboundFrameRateRng      = RANDOM_CONSTANT;
   InboundFrameSizeRng      = RANDOM_CONSTANT;
}
