/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2009 by Thomas Dreibholz
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
 * Contact: dreibh@iem.uni-due.de
 */

#ifndef STATISTICSWRITER_H
#define STATISTICSWRITER_H

#include <stdio.h>
#include <bzlib.h>
#include <vector>
#include <map>

#include "flowspec.h"


class StatisticsWriter
{
   protected:
   StatisticsWriter(const uint64_t measurementID = 0);
   ~StatisticsWriter();

   public:
   static StatisticsWriter* getStatisticsWriter(const uint64_t measurementID = 0);
   
   inline unsigned long long getNextEvent() const {
      return(NextStatisticsEvent);
   }
   inline void setMeasurementID(const uint64_t measurementID) {
      MeasurementID = measurementID;
   }
   inline void setVectorName(const char* name) {
      VectorName = std::string(name);
      dissectName(VectorName,  VectorPrefix, VectorSuffix);
   }
   inline void setScalarName(const char* name) {
      ScalarName = std::string(name);
      dissectName(ScalarName,  ScalarPrefix, ScalarSuffix);
   }
   bool initializeOutputFiles();
   bool finishOutputFiles(const bool  closeFile = false);
   
   static StatisticsWriter* addMeasurement(const uint64_t measurementID, const bool compressed);
   static void printMeasurements(std::ostream& os);
   static StatisticsWriter* findMeasurement(const uint64_t measurementID);
   static void removeMeasurement(const uint64_t measurementID);

   static bool initializeOutputFile(const char* name,
                                    FILE**     fh,
                                    BZFILE**   bz);
   static bool finishOutputFile(const char* name,
                                FILE**      fh,
                                BZFILE**    bz,
                                const bool  closeFile = false);
   static bool writeString(const char* name,
                           FILE*       fh,
                           BZFILE*     bz,
                           const char* str);

   bool writeAllVectorStatistics(const unsigned long long now,
                                 std::vector<FlowSpec*>&  flowSet,
                                 const uint64_t           measurementID);
   bool writeAllScalarStatistics(const unsigned long long now,
                                 std::vector<FlowSpec*>& flowSet,
                                 const uint64_t          measurementID);

   public:
   uint64_t           MeasurementID;
   unsigned long long StatisticsInterval;
   unsigned long long FirstStatisticsEvent;
   unsigned long long LastStatisticsEvent;
   unsigned long long NextStatisticsEvent;

   unsigned long long VectorLine;
   std::string        VectorName;
   std::string        VectorPrefix;
   std::string        VectorSuffix;
   FILE*              VectorFile;
   BZFILE*            VectorBZFile;

   std::string        ScalarName;
   std::string        ScalarPrefix;
   std::string        ScalarSuffix;
   FILE*              ScalarFile;
   BZFILE*            ScalarBZFile;

   unsigned long long TotalTransmittedBytes;
   unsigned long long TotalTransmittedPackets;
   unsigned long long TotalTransmittedFrames;
   unsigned long long TotalReceivedBytes;
   unsigned long long TotalReceivedPackets;
   unsigned long long TotalReceivedFrames;
   unsigned long long TotalLostBytes;
   unsigned long long TotalLostPackets;
   unsigned long long TotalLostFrames;
   unsigned long long LastTotalTransmittedBytes;
   unsigned long long LastTotalTransmittedPackets;
   unsigned long long LastTotalTransmittedFrames;
   unsigned long long LastTotalReceivedBytes;
   unsigned long long LastTotalReceivedPackets;
   unsigned long long LastTotalReceivedFrames;
   unsigned long long LastTotalLostBytes;
   unsigned long long LastTotalLostPackets;
   unsigned long long LastTotalLostFrames;
   
   private:
   bool writeVectorStatistics(const unsigned long long now,
                              std::vector<FlowSpec*>&  flowSet,
                              const uint64_t           measurementID);
   bool writeScalarStatistics(const unsigned long long now,
                              std::vector<FlowSpec*>& flowSet,
                              const uint64_t          measurementID);

   static StatisticsWriter                      GlobalStatisticsWriter;
   static std::map<uint64_t, StatisticsWriter*> StatisticsSet;
};

#endif
