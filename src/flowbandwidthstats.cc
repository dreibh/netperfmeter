/* $Id$
 *
 * Network Performance Meter
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
 * Contact: dreibh@iem.uni-due.de
 */

#include "flowbandwidthstats.h"


// ###### Constructor #######################################################
FlowBandwidthStats::FlowBandwidthStats()
{
   reset();
}


// ###### Destructor ########################################################
FlowBandwidthStats::~FlowBandwidthStats()
{
}


// ###### "+" operator ######################################################
FlowBandwidthStats operator+(const FlowBandwidthStats& s1,
                             const FlowBandwidthStats& s2)
{
   FlowBandwidthStats result;
   result.TransmittedBytes   = s1.TransmittedBytes + s2.TransmittedBytes;
   result.TransmittedPackets = s1.TransmittedPackets + s2.TransmittedPackets;
   result.TransmittedFrames  = s1.TransmittedFrames + s2.TransmittedFrames;

   result.ReceivedBytes      = s1.ReceivedBytes + s2.ReceivedBytes;
   result.ReceivedPackets    = s1.ReceivedPackets + s2.ReceivedPackets;
   result.ReceivedFrames     = s1.ReceivedFrames + s2.ReceivedFrames;

   result.LostBytes          = s1.LostBytes + s2.LostBytes;
   result.LostPackets        = s1.LostPackets + s2.LostPackets;
   result.LostFrames         = s1.LostFrames + s2.LostFrames;
   return(result);
}


// ###### "-" operator ######################################################
FlowBandwidthStats operator-(const FlowBandwidthStats& s1,
                             const FlowBandwidthStats& s2)
{
   FlowBandwidthStats result;
   result.TransmittedBytes   = s1.TransmittedBytes - s2.TransmittedBytes;
   result.TransmittedPackets = s1.TransmittedPackets - s2.TransmittedPackets;
   result.TransmittedFrames  = s1.TransmittedFrames - s2.TransmittedFrames;

   result.ReceivedBytes      = s1.ReceivedBytes - s2.ReceivedBytes;
   result.ReceivedPackets    = s1.ReceivedPackets - s2.ReceivedPackets;
   result.ReceivedFrames     = s1.ReceivedFrames - s2.ReceivedFrames;

   result.LostBytes          = s1.LostBytes - s2.LostBytes;
   result.LostPackets        = s1.LostPackets - s2.LostPackets;
   result.LostFrames         = s1.LostFrames - s2.LostFrames;
   return(result);
}


// ###### Print FlowBandwidthStats ##########################################
void FlowBandwidthStats::print(std::ostream& os,
                               const double  transmissionDuration,
                               const double  receptionDuration) const
{
   const unsigned long long transmittedBits       = 8 * TransmittedBytes;
   const unsigned long long transmittedBitRate    = calculateRate(transmittedBits, transmissionDuration);
   const unsigned long long transmittedBytes      = TransmittedBytes;
   const unsigned long long transmittedByteRate   = calculateRate(transmittedBytes, transmissionDuration);
   const unsigned long long transmittedPackets    = TransmittedPackets;
   const unsigned long long transmittedPacketRate = calculateRate(transmittedPackets, transmissionDuration);
   const unsigned long long transmittedFrames     = TransmittedFrames;
   const unsigned long long transmittedFrameRate  = calculateRate(transmittedFrames, transmissionDuration);

   const unsigned long long receivedBits          = 8 * ReceivedBytes;
   const unsigned long long receivedBitRate       = calculateRate(receivedBits, receptionDuration);
   const unsigned long long receivedBytes         = ReceivedBytes;
   const unsigned long long receivedByteRate      = calculateRate(receivedBytes, receptionDuration);
   const unsigned long long receivedPackets       = ReceivedPackets;
   const unsigned long long receivedPacketRate    = calculateRate(receivedPackets, receptionDuration);
   const unsigned long long receivedFrames        = ReceivedFrames;
   const unsigned long long receivedFrameRate     = calculateRate(receivedFrames, receptionDuration);

   const unsigned long long lostBits              = 8 * LostBytes;
   const unsigned long long lostBitRate           = calculateRate(lostBits, receptionDuration);
   const unsigned long long lostBytes             = LostBytes;
   const unsigned long long lostByteRate          = calculateRate(lostBytes, receptionDuration);
   const unsigned long long lostPackets           = LostPackets;
   const unsigned long long lostPacketRate        = calculateRate(lostPackets, receptionDuration);
   const unsigned long long lostFrames            = LostFrames;
   const unsigned long long lostFrameRate         = calculateRate(lostFrames, receptionDuration);

   os << "      - Transmission:"         << std::endl
      << "         * Duration:         " << transmissionDuration << " s" << std::endl
      << "         * Bytes:            " << transmittedBytes << " B\t-> "
                                         << transmittedByteRate << " B/s" << std::endl
      << "         * Bits:             " << transmittedBits << " bit\t-> "
                                         << transmittedBitRate << " bit/s" << std::endl
      << "         * Packets:          " << transmittedPackets << " packets\t-> "
                                         << transmittedPacketRate << " packets/s" << std::endl
      << "         * Frames:           " << transmittedFrames << " frames\t-> "
                                         << transmittedFrameRate << " frames/s" << std::endl;

   os << "      - Reception:"            << std::endl
      << "         * Duration:         " << receptionDuration << "s" << std::endl
      << "         * Bytes:            " << receivedBytes << " B\t-> "
                                         << receivedByteRate << " B/s" << std::endl
      << "         * Bits:             " << receivedBits << " bit\t-> "
                                         << receivedBitRate << " bit/s" << std::endl
      << "         * Packets:          " << receivedPackets << " packets\t-> "
                                         << receivedPacketRate << " packets/s" << std::endl
      << "         * Frames:           " << receivedFrames << " frames\t-> "
                                         << receivedFrameRate << " frames/s" << std::endl;

   os << "      - Loss:"                 << std::endl
      << "         * Duration:         " << receptionDuration << "s" << std::endl
      << "         * Bytes:            " << lostBytes << " B\t-> "
                                         << lostByteRate << " B/s" << std::endl
      << "         * Bits:             " << lostBits << " bit\t-> "
                                         << lostBitRate << " bit/s" << std::endl
      << "         * Packets:          " << lostPackets << " packets\t-> "
                                         << lostPacketRate << " packets/s" << std::endl
      << "         * Frames:           " << lostFrames << " frames\t-> "
                                         << lostFrameRate << " frames/s" << std::endl;
}


// ###### Reset FlowBandwidthStats ##########################################
void FlowBandwidthStats::reset()
{
   TransmittedBytes   = 0;
   TransmittedPackets = 0;
   TransmittedFrames  = 0;

   ReceivedBytes      = 0;
   ReceivedPackets    = 0;
   ReceivedFrames     = 0;

   LostBytes          = 0;
   LostPackets        = 0;
   LostFrames         = 0;
}
