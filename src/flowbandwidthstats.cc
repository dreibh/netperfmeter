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

#include "flowbandwidthstats.h"
#include "tools.h"


// ###### "+" operator ######################################################
FlowBandwidthStats operator+(const FlowBandwidthStats& s1,
                             const FlowBandwidthStats& s2) noexcept
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
   return result;
}


// ###### "-" operator ######################################################
FlowBandwidthStats operator-(const FlowBandwidthStats& s1,
                             const FlowBandwidthStats& s2) noexcept
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
   return result;
}


// ###### Print FlowBandwidthStats ##########################################
void FlowBandwidthStats::print(std::ostream& os,
                               const double  transmissionDuration,
                               const double  receptionDuration) const
{
   const uint64_t transmittedBits       = 8 * TransmittedBytes;
   const uint64_t transmittedBitRate    = calculateRate(transmittedBits, transmissionDuration);
   const uint64_t transmittedBytes      = TransmittedBytes;
   const uint64_t transmittedByteRate   = calculateRate(transmittedBytes, transmissionDuration);
   const uint64_t transmittedPackets    = TransmittedPackets;
   const uint64_t transmittedPacketRate = calculateRate(transmittedPackets, transmissionDuration);
   const uint64_t transmittedFrames     = TransmittedFrames;
   const uint64_t transmittedFrameRate  = calculateRate(transmittedFrames, transmissionDuration);

   const uint64_t receivedBits          = 8 * ReceivedBytes;
   const uint64_t receivedBitRate       = calculateRate(receivedBits, receptionDuration);
   const uint64_t receivedBytes         = ReceivedBytes;
   const uint64_t receivedByteRate      = calculateRate(receivedBytes, receptionDuration);
   const uint64_t receivedPackets       = ReceivedPackets;
   const uint64_t receivedPacketRate    = calculateRate(receivedPackets, receptionDuration);
   const uint64_t receivedFrames        = ReceivedFrames;
   const uint64_t receivedFrameRate     = calculateRate(receivedFrames, receptionDuration);

   const uint64_t lostBits              = 8 * LostBytes;
   const uint64_t lostBitRate           = calculateRate(lostBits, receptionDuration);
   const uint64_t lostBytes             = LostBytes;
   const uint64_t lostByteRate          = calculateRate(lostBytes, receptionDuration);
   const uint64_t lostPackets           = LostPackets;
   const uint64_t lostPacketRate        = calculateRate(lostPackets, receptionDuration);
   const uint64_t lostFrames            = LostFrames;
   const uint64_t lostFrameRate         = calculateRate(lostFrames, receptionDuration);

   // TRANSLATORS: Bandwidth statistics section header for transmitted traffic.
   os << gettext(" - Transmission:") << "\n"
      // TRANSLATORS: Measurement duration. Placeholder: {} is duration in seconds.
      << formatText(gettext("  * Duration:         {} s"), transmissionDuration) << "\n"
      // TRANSLATORS: Transmitted byte statistics. Placeholders: {} 1: total bytes, 2: byte rate in B/s.
      << formatText(gettext("  * Bytes:            {} B\t-> {} B/s"), transmittedBytes, transmittedByteRate) << "\n"
      // TRANSLATORS: Transmitted bit statistics. Placeholders: {} 1: total bits, 2: bit rate in bit/s.
      << formatText(gettext("  * Bits:             {} bit\t-> {} bit/s"), transmittedBits, transmittedBitRate) << "\n"
      // TRANSLATORS: Transmitted packet statistics. Placeholders: {} 1: total packets, 2: packet rate in packets/s.
      << formatText(gettext("  * Packets:          {} packets\t-> {} packets/s"), transmittedPackets, transmittedPacketRate) << "\n"
      // TRANSLATORS: Transmitted frame statistics. Placeholders: {} 1: total frames, 2: frame rate in frames/s.
      << formatText(gettext("  * Frames:           {} frames\t-> {} frames/s"), transmittedFrames, transmittedFrameRate) << "\n";

   // TRANSLATORS: Bandwidth statistics section header for received traffic.
   os << gettext(" - Reception:") << "\n"
      // TRANSLATORS: Measurement duration. Placeholder: {} is duration in seconds.
      << formatText(gettext("  * Duration:         {} s"), receptionDuration) << "\n"
      // TRANSLATORS: Received byte statistics. Placeholders: {} 1: total bytes, 2: byte rate in B/s.
      << formatText(gettext("  * Bytes:            {} B\t-> {} B/s"), receivedBytes, receivedByteRate) << "\n"
      // TRANSLATORS: Received bit statistics. Placeholders: {} 1: total bits, 2: bit rate in bit/s.
      << formatText(gettext("  * Bits:             {} bit\t-> {} bit/s"), receivedBits, receivedBitRate) << "\n"
      // TRANSLATORS: Received packet statistics. Placeholders: {} 1: total packets, 2: packet rate in packets/s.
      << formatText(gettext("  * Packets:          {} packets\t-> {} packets/s"), receivedPackets, receivedPacketRate) << "\n"
      // TRANSLATORS: Received frame statistics. Placeholders: {} 1: total frames, 2: frame rate in frames/s.
      << formatText(gettext("  * Frames:           {} frames\t-> {} frames/s"), receivedFrames, receivedFrameRate) << "\n";

   // TRANSLATORS: Bandwidth statistics section header for lost traffic.
   os << gettext(" - Loss:") << "\n"
      // TRANSLATORS: Measurement duration. Placeholder: {} is duration in seconds.
      << formatText(gettext("  * Duration:         {} s"), receptionDuration) << "\n"
      // TRANSLATORS: Lost byte statistics. Placeholders: {} 1: total bytes, 2: byte rate in B/s.
      << formatText(gettext("  * Bytes:            {} B\t-> {} B/s"), lostBytes, lostByteRate) << "\n"
      // TRANSLATORS: Lost bit statistics. Placeholders: {} 1: total bits, 2: bit rate in bit/s.
      << formatText(gettext("  * Bits:             {} bit\t-> {} bit/s"), lostBits, lostBitRate) << "\n"
      // TRANSLATORS: Lost packet statistics. Placeholders: {} 1: total packets, 2: packet rate in packets/s.
      << formatText(gettext("  * Packets:          {} packets\t-> {} packets/s"), lostPackets, lostPacketRate) << "\n"
      // TRANSLATORS: Lost frame statistics. Placeholders: {} 1: total frames, 2: frame rate in frames/s.
      << formatText(gettext("  * Frames:           {} frames\t-> {} frames/s"), lostFrames, lostFrameRate) << "\n";
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
