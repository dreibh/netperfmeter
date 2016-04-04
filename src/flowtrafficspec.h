/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2013 by Sebastian Wallat (TCP No delay)
 * Copyright (C) 2009-2016 by Thomas Dreibholz
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

#ifndef FLOWTRAFFICSPEC_H
#define FLOWTRAFFICSPEC_H

#include "netperfmeterpackets.h"

#include <sys/types.h>

#include <vector>
#include <string>
#include <iostream>


struct OnOffEvent
{
   uint8_t  RandNumGen;
   bool     RelativeTime;
   double   ValueArray[NETPERFMETER_RNG_INPUT_PARAMETERS];
};

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

   std::string             Description;
   uint8_t                 Protocol;

   double                  OrderedMode;
   double                  ReliableMode;
   uint32_t                RetransmissionTrials;
   bool                    RetransmissionTrialsInMS;

   uint16_t                MaxMsgSize;

   unsigned int            RcvBufferSize;
   unsigned int            SndBufferSize;

   unsigned long long      DefragmentTimeout;

   double                  OutboundFrameRate[NETPERFMETER_RNG_INPUT_PARAMETERS];
   double                  OutboundFrameSize[NETPERFMETER_RNG_INPUT_PARAMETERS];
   double                  InboundFrameRate[NETPERFMETER_RNG_INPUT_PARAMETERS];
   double                  InboundFrameSize[NETPERFMETER_RNG_INPUT_PARAMETERS];
   uint8_t                 OutboundFrameRateRng;
   uint8_t                 OutboundFrameSizeRng;
   uint8_t                 InboundFrameRateRng;
   uint8_t                 InboundFrameSizeRng;

   uint8_t                 CMT;
   uint8_t                 CCID;

   uint16_t                NDiffPorts;
   std::string             CongestionControl;
   std::string             PathMgr;
   std::string             Scheduler;

   bool                    Debug;
   bool                    NoDelay;
   bool                    ErrorOnAbort;
   bool                    RepeatOnOff;

   std::vector<OnOffEvent> OnOffEvents;
};

#endif
