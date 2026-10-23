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

#ifndef FLOWBANDWIDTHSTATS_H
#define FLOWBANDWIDTHSTATS_H

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>


class FlowBandwidthStats
{
   // ====== Methods ========================================================
   public:
   FlowBandwidthStats();
   ~FlowBandwidthStats();

   void print(std::ostream& os,
              const double  transmissionDuration,
              const double  receptionDuration) const;
   void reset();

   inline static uint64_t calculateRate(const uint64_t value,
                                        const double   duration) {
      if(duration < 0.000001) {
         return 0;
      }
      return static_cast<uint64_t>(rint(static_cast<double>(value) / duration));
   }

   // ====== Public Data ====================================================
   public:
   uint64_t TransmittedBytes;
   uint64_t TransmittedPackets;
   uint64_t TransmittedFrames;

   uint64_t ReceivedBytes;
   uint64_t ReceivedPackets;
   uint64_t ReceivedFrames;

   uint64_t LostBytes;
   uint64_t LostPackets;
   uint64_t LostFrames;
};


FlowBandwidthStats operator+(const FlowBandwidthStats& s1, const FlowBandwidthStats& s2);
FlowBandwidthStats operator-(const FlowBandwidthStats& s1, const FlowBandwidthStats& s2);

#endif // FLOWBANDWIDTHSTATS_H
