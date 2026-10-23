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

   uint64_t getMeasurementID() const {
      return MeasurementID;
   }
   const std::string& getVectorNamePattern() const {
      return VectorNamePattern;
   }
   const OutputFile& getVectorFile() const {
      return VectorFile;
   }
   const std::string& getScalarNamePattern() const {
      return ScalarNamePattern;
   }
   const OutputFile& getScalarFile() const {
      return ScalarFile;
   }
   inline uint64_t getFirstStatisticsEvent() const {
      return FirstStatisticsEvent;
   }

   bool initialize(const uint64_t         now,
                   const int              controlSocketDescriptor,
                   const uint64_t         measurementID,
                   const char*            vectorNamePattern,
                   const OutputFileFormat vectorFileFormat,
                   const char*            scalarNamePattern,
                   const OutputFileFormat scalarFileFormat);
   bool finish(const bool closeFiles);

   void writeScalarStatistics(const uint64_t now);
   void writeVectorStatistics(const uint64_t     now,
                              FlowBandwidthStats& globalStats,
                              FlowBandwidthStats& relGlobalStats);


   // ====== Private Data ===================================================
   private:
   int         ControlSocketDescriptor;
   uint64_t    MeasurementID;
   uint64_t    StatisticsInterval;
   uint64_t    FirstStatisticsEvent;
   uint64_t    LastStatisticsEvent;
   uint64_t    NextStatisticsEvent;

   std::string VectorNamePattern;
   std::string ScalarNamePattern;
   OutputFile  VectorFile;
   OutputFile  ScalarFile;

   uint64_t    LastTransmission;
   uint64_t    FirstTransmission;
   uint64_t    LastReception;
   uint64_t    FirstReception;
};

#endif // MEASUREMENT_H
