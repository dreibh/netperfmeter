/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2009-2012 by Thomas Dreibholz
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

#ifndef FLOWBANDWIDTHSTATS_H
#define FLOWBANDWIDTHSTATS_H

#include <math.h>
#include <string>
#include <iostream>


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

   inline static unsigned long long calculateRate(const unsigned long long value,
                                                  const double             duration) {
      if(duration < 0.000001) {
         return(0);
      }
      return((unsigned long long)rint(value / duration));
   }

   // ====== Public Data ====================================================
   public:
   unsigned long long TransmittedBytes;
   unsigned long long TransmittedPackets;
   unsigned long long TransmittedFrames;

   unsigned long long ReceivedBytes;
   unsigned long long ReceivedPackets;
   unsigned long long ReceivedFrames;

   unsigned long long LostBytes;
   unsigned long long LostPackets;
   unsigned long long LostFrames;
};


FlowBandwidthStats operator+(const FlowBandwidthStats& s1, const FlowBandwidthStats& s2);
FlowBandwidthStats operator-(const FlowBandwidthStats& s1, const FlowBandwidthStats& s2);

#endif
