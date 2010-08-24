/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2009-2010 by Thomas Dreibholz
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

#ifndef FLOWTRAFFICSPEC_H
#define FLOWTRAFFICSPEC_H

#include "netperfmeterpackets.h"

#include <sys/types.h>

#include <set>
#include <string>
#include <iostream>


class FlowTrafficSpec
{
   // ====== Methods ========================================================
   public:
   FlowTrafficSpec();
   ~FlowTrafficSpec();

   void print(std::ostream& os) const;
   void reset();


   // ====== Public Data ====================================================
   public:
   static void showEntry(std::ostream& os, const double* valueArray, const uint8_t rng);

   std::string            Description;
   uint8_t                Protocol;
   std::set<unsigned int> OnOffEvents;

   double                 OrderedMode;
   double                 ReliableMode;
   uint32_t               RetransmissionTrials;
   bool                   RetransmissionTrialsInMS;

   uint16_t               MaxMsgSize;

   unsigned int           RcvBufferSize;
   unsigned int           SndBufferSize;

   unsigned long long     DefragmentTimeout;

   bool                   UseNRSACK;
   bool                   UseCMT;
   bool                   UseRP;
   
   double                 OutboundFrameRate[NETPERFMETER_RNG_INPUT_PARAMETERS];
   double                 OutboundFrameSize[NETPERFMETER_RNG_INPUT_PARAMETERS];
   double                 InboundFrameRate[NETPERFMETER_RNG_INPUT_PARAMETERS];
   double                 InboundFrameSize[NETPERFMETER_RNG_INPUT_PARAMETERS];
   uint8_t                OutboundFrameRateRng;
   uint8_t                OutboundFrameSizeRng;
   uint8_t                InboundFrameRateRng;
   uint8_t                InboundFrameSizeRng;
};

#endif
