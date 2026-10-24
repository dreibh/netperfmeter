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

#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include "mutex.h"
#include "outputfile.h"
#include "flowbandwidthstats.h"
#include "tools.h"

#include <cstdint>
#include <string>


class Measurement : public Mutex
{
   friend class FlowManager;

   // ====== Public Methods =================================================
   public:
   Measurement();
   ~Measurement();

   [[nodiscard]] inline uint64_t getMeasurementID() const noexcept {
      return MeasurementID;
   }
   [[nodiscard]] inline const std::string& getVectorNamePattern() const noexcept {
      return VectorNamePattern;
   }
   [[nodiscard]] inline const OutputFile& getVectorFile() const noexcept {
      return VectorFile;
   }
   [[nodiscard]] inline OutputFile& getVectorFile() noexcept {
      return VectorFile;
   }
   [[nodiscard]] inline const std::string& getScalarNamePattern() const noexcept {
      return ScalarNamePattern;
   }
   [[nodiscard]] inline const OutputFile& getScalarFile() const noexcept {
      return ScalarFile;
   }
   [[nodiscard]] inline OutputFile& getScalarFile() noexcept {
      return ScalarFile;
   }
   [[nodiscard]] inline uint64_t getFirstStatisticsEvent() const noexcept {
      return FirstStatisticsEvent;
   }

   void initialize(const uint64_t         now,
                   const int              controlSocketDescriptor,
                   const uint64_t         measurementID,
                   const char*            vectorNamePattern,
                   const OutputFileFormat vectorFileFormat,
                   const char*            scalarNamePattern,
                   const OutputFileFormat scalarFileFormat);
   void finish(const bool closeFiles = true);

   void writeScalarStatistics(const uint64_t now);
   void writeVectorStatistics(const uint64_t      now,
                              FlowBandwidthStats& globalStats,
                              FlowBandwidthStats& relGlobalStats);


   // ====== Private Data ===================================================
   private:
   int         ControlSocketDescriptor { -1 };
   uint64_t    MeasurementID           { 0 };
   uint64_t    StatisticsInterval      { 1000000ULL };
   uint64_t    FirstStatisticsEvent    { 0 };
   uint64_t    LastStatisticsEvent     { 0 };
   uint64_t    NextStatisticsEvent     { 0 };

   std::string VectorNamePattern;
   std::string ScalarNamePattern;
   OutputFile  VectorFile;
   OutputFile  ScalarFile;

   uint64_t    LastTransmission        { 0 };
   uint64_t    FirstTransmission       { 0 };
   uint64_t    LastReception           { 0 };
   uint64_t    FirstReception          { 0 };
};

#endif // MEASUREMENT_H
