/*
 * ==========================================================================
 *                  NetPerfMeter -- Network Performance Meter                 
 *                 Copyright (C) 2009-2018 by Thomas Dreibholz
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
 * Contact:  dreibh@iem.uni-due.de
 * Homepage: https://www.uni-due.de/~be0001/netperfmeter/
 */

#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include "mutex.h"
#include "outputfile.h"
#include "flowbandwidthstats.h"
#include "tools.h"


class Measurement : public Mutex
{
   friend class FlowManager;

   // ====== Public Methods =================================================
   public:
   Measurement();
   ~Measurement();

   const uint64_t getMeasurementID() const {
      return(MeasurementID);
   }
   const std::string& getVectorNamePattern() const {
      return(VectorNamePattern);
   }
   const OutputFile& getVectorFile() const {
      return(VectorFile);
   }
   const std::string& getScalarNamePattern() const {
      return(ScalarNamePattern);
   }
   const OutputFile& getScalarFile() const {
      return(ScalarFile);
   }
   inline unsigned long long getFirstStatisticsEvent() const {
      return(FirstStatisticsEvent);
   }

   bool initialize(const unsigned long long now,
                   const uint64_t           measurementID,
                   const char*              vectorNamePattern,
                   const OutputFileFormat   vectorFileFormat,
                   const char*              scalarNamePattern,
                   const OutputFileFormat   scalarFileFormat);
   bool finish(const bool closeFiles);

   void writeScalarStatistics(const unsigned long long now);
   void writeVectorStatistics(const unsigned long long now,
                              FlowBandwidthStats&      globalStats,
                              FlowBandwidthStats&      relGlobalStats);


   // ====== Private Data ===================================================
   private:
   uint64_t           MeasurementID;
   unsigned long long StatisticsInterval;
   unsigned long long FirstStatisticsEvent;
   unsigned long long LastStatisticsEvent;
   unsigned long long NextStatisticsEvent;

   std::string        VectorNamePattern;
   std::string        ScalarNamePattern;
   OutputFile         VectorFile;
   OutputFile         ScalarFile;

   unsigned long long LastTransmission;
   unsigned long long FirstTransmission;
   unsigned long long LastReception;
   unsigned long long FirstReception;
};

#endif
