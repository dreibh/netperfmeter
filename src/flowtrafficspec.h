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

#ifndef FLOWTRAFFICSPEC_H
#define FLOWTRAFFICSPEC_H

#include "netperfmeterpackets.h"
#include "tools.h"

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

   inline bool outgoingFlowIsSaturated() const {
      return ( (OutboundFrameRateRng == RANDOM_CONSTANT) &&
               (OutboundFrameRate[0] <= 0.000000001)     &&
               (OutboundFrameSize[0] > 0.000000001) );
   }
   inline bool outgoingFlowIsNotSaturated() const {
      return ( (OutboundFrameRate[0] > 0.000000001) &&
               (OutboundFrameSize[0] > 0.000000001) );
   }

   // ====== Public Data ====================================================
   public:
   static void showEntry(std::ostream& os, const double* valueArray, const uint8_t rng);

   std::string             Description;
   int                     Protocol;

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
   std::string             CongestionControl;

   bool                    Debug;
   bool                    NoDelay;
   bool                    ErrorOnAbort;
   bool                    RepeatOnOff;
   bool                    BindV6Only;

   std::vector<OnOffEvent> OnOffEvents;
};

#endif
