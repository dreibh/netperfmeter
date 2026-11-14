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

#include <algorithm>
#include <string_view>


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


// ###### Add one traffic direction/loss block to PropertyFormatter #########
static void addTrafficSection(PropertyFormatter& formatter,
                              const size_t       level,
                              const char*        sectionTitle,
                              const double       duration,
                              const uint64_t     bytes,   const uint64_t byteRate,
                              const uint64_t     bits,    const uint64_t bitRate,
                              const uint64_t     packets, const uint64_t packetRate,
                              const uint64_t     frames,  const uint64_t frameRate)
{
   formatter.addLine(level, sectionTitle);
   // TRANSLATORS: Measurement duration label.
   formatter.add(level + 1, gettext("Duration"),
                 // TRANSLATORS: Measurement duration value in seconds. Placeholder: {} is duration in seconds.
                 formatText(gettext("{} s"), duration));

   struct RateRow {
      const char* label;
      std::string total;
      std::string rate;
   };
   const RateRow rows[4] = {
      // TRANSLATORS: Byte statistics label.
      { gettext("Bytes"),
        // TRANSLATORS: Byte count. Placeholder: {} is total bytes.
        formatText(gettext("{} B"), bytes),
        // TRANSLATORS: Byte rate. Placeholder: {} is byte rate in B/s.
        formatText(gettext("{} B/s"), byteRate) },
      // TRANSLATORS: Bit statistics label.
      { gettext("Bits"),
        // TRANSLATORS: Bit count. Placeholder: {} is total bits.
        formatText(gettext("{} bit"), bits),
        // TRANSLATORS: Bit rate. Placeholder: {} is bit rate in bit/s.
        formatText(gettext("{} bit/s"), bitRate) },
      // TRANSLATORS: Packet statistics label.
      { gettext("Packets"),
        // TRANSLATORS: Packet count. Placeholder: {} is total packets.
        formatText(gettext("{} packets"), packets),
        // TRANSLATORS: Packet rate. Placeholder: {} is packet rate in packets/s.
        formatText(gettext("{} packets/s"), packetRate) },
      // TRANSLATORS: Frame statistics label.
      { gettext("Frames"),
        // TRANSLATORS: Frame count. Placeholder: {} is total frames.
        formatText(gettext("{} frames"), frames),
        // TRANSLATORS: Frame rate. Placeholder: {} is frame rate in frames/s.
        formatText(gettext("{} frames/s"), frameRate) }
   };

   size_t maxTotalLen = 0;
   for(const auto& row : rows) {
      maxTotalLen = std::max(maxTotalLen, row.total.size());
   }
   for(const auto& row : rows) {
      std::string paddedTotal = row.total;
      if(paddedTotal.size() < maxTotalLen) {
         paddedTotal.append(maxTotalLen - paddedTotal.size(), ' ');
      }
      formatter.add(level + 1, row.label, formatText("{} -> {}", paddedTotal, row.rate));
   }
}


// ###### Print FlowBandwidthStats into PropertyFormatter ###################
void FlowBandwidthStats::print(PropertyFormatter& formatter,
                               const double       transmissionDuration,
                               const double       receptionDuration,
                               const size_t       level) const
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

   addTrafficSection(formatter, level,
                     // TRANSLATORS: Bandwidth statistics section header for transmitted traffic.
                     gettext("Transmission:"),
                     transmissionDuration,
                     transmittedBytes, transmittedByteRate,
                     transmittedBits, transmittedBitRate,
                     transmittedPackets, transmittedPacketRate,
                     transmittedFrames, transmittedFrameRate);

   addTrafficSection(formatter, level,
                     // TRANSLATORS: Bandwidth statistics section header for received traffic.
                     gettext("Reception:"),
                     receptionDuration,
                     receivedBytes, receivedByteRate,
                     receivedBits, receivedBitRate,
                     receivedPackets, receivedPacketRate,
                     receivedFrames, receivedFrameRate);

   addTrafficSection(formatter, level,
                     // TRANSLATORS: Bandwidth statistics section header for lost traffic.
                     gettext("Loss:"),
                     receptionDuration,
                     lostBytes, lostByteRate,
                     lostBits, lostBitRate,
                     lostPackets, lostPacketRate,
                     lostFrames, lostFrameRate);
}


// ###### Print FlowBandwidthStats ##########################################
void FlowBandwidthStats::print(std::ostream& os,
                               const double  transmissionDuration,
                               const double  receptionDuration) const
{
   // TRANSLATORS: Text direction: LTR (Left-to-Right) or RTL (Right-to-Left)
   const bool rtl = (std::string_view(gettext("LTR")) == "RTL");
   PropertyFormatter formatter("   ", { "", "- ", "+ ", "* " }, "=", rtl);
   print(formatter, transmissionDuration, receptionDuration, 1);
   os << formatter;
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
