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

#ifndef FLOWTRAFFICSPEC_H
#define FLOWTRAFFICSPEC_H

#include "netperfmeterpackets.h"
#include "tools.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>


struct OnOffEvent
{
   uint8_t RandNumGen                                          { 0 };
   bool    RelativeTime                                        { false };
   double  ValueArray[NETPERFMETER_RNG_INPUT_PARAMETERS] { 0.0 };
};

class FlowTrafficSpec
{
   // ====== Methods ========================================================
   public:
   FlowTrafficSpec() = default;
   ~FlowTrafficSpec() = default;

   void print(std::ostream& os) const;
   void reset();

   [[nodiscard]] inline constexpr bool outgoingFlowIsSaturated() const noexcept {
      return ( (OutboundFrameRateRng == RANDOM_CONSTANT) &&
               (OutboundFrameRate[0] <= 0.000000001)     &&
               (OutboundFrameSize[0] > 0.000000001) );
   }
   [[nodiscard]] inline constexpr bool outgoingFlowIsNotSaturated() const noexcept {
      return ( (OutboundFrameRate[0] > 0.000000001) &&
               (OutboundFrameSize[0] > 0.000000001) );
   }

   // ====== Public Data ====================================================
   public:
   static void showEntry(std::ostream& os, const double* valueArray, const uint8_t rng);

   std::string             Description;
   int                     Protocol                 { 0 };

   double                  OrderedMode              { 1.0 };
   double                  ReliableMode             { 1.0 };
   uint32_t                RetransmissionTrials     { static_cast<uint32_t>(-1) };
   bool                    RetransmissionTrialsInMS { true };

   uint16_t                MaxMsgSize               { 16000 };

   // 209712 is the upper limit for NetBSD (with default kernel settings):
   uint32_t                RcvBufferSize            { 209712 };
   uint32_t                SndBufferSize            { 209712 };

   uint64_t                DefragmentTimeout        { 5000000ULL };

   double                  OutboundFrameRate[NETPERFMETER_RNG_INPUT_PARAMETERS] { 0.0 };
   double                  OutboundFrameSize[NETPERFMETER_RNG_INPUT_PARAMETERS] { 0.0 };
   double                  InboundFrameRate[NETPERFMETER_RNG_INPUT_PARAMETERS]  { 0.0 };
   double                  InboundFrameSize[NETPERFMETER_RNG_INPUT_PARAMETERS]  { 0.0 };
   uint8_t                 OutboundFrameRateRng     { RANDOM_CONSTANT };
   uint8_t                 OutboundFrameSizeRng     { RANDOM_CONSTANT };
   uint8_t                 InboundFrameRateRng      { RANDOM_CONSTANT };
   uint8_t                 InboundFrameSizeRng      { RANDOM_CONSTANT };

   uint8_t                 CMT                      { 0x00 };
   uint8_t                 CCID                     { 0x00 };
   std::string             CongestionControl        { "default" };

   bool                    Debug                    { false };
   bool                    NoDelay                  { false };
   bool                    ErrorOnAbort             { true };
   bool                    RepeatOnOff              { false };
   bool                    BindV6Only               { false };

   std::vector<OnOffEvent> OnOffEvents;
};

#endif // FLOWTRAFFICSPEC_H
