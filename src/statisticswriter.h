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
      VectorName = name;
      dissectName(VectorName, 
                  (char*)&VectorPrefix, sizeof(VectorPrefix),
                  (char*)&VectorSuffix, sizeof(VectorSuffix));
   }
   bool openOutputFiles();
   void closeOutputFiles();
   
   static StatisticsWriter* addMeasurement(const uint64_t measurementID, const bool compressed);
   static void removeMeasurement(const uint64_t measurementID);

   static bool openOutputFile(const char* name,
                              FILE**     fh,
                              BZFILE**   bz);
   static bool closeOutputFile(const char* name,
                               FILE**     fh,
                               BZFILE**   bz);
   static bool writeString(const char* name,
                           FILE*       fh,
                           BZFILE*     bz,
                           const char* str);

   bool writeAllVectorStatistics(const unsigned long long now,
                                 std::vector<FlowSpec*>&  flowSet,
                                 const uint64_t           measurementID);
   bool writeScalarStatistics(const unsigned long long now,
                              std::vector<FlowSpec*>&  flowSet);

   public:
   uint64_t           MeasurementID;
   unsigned long long StatisticsInterval;
   unsigned long long FirstStatisticsEvent;
   unsigned long long LastStatisticsEvent;
   unsigned long long NextStatisticsEvent;

   unsigned long long VectorLine;
   const char*        VectorName;
   char               VectorPrefix[256];
   char               VectorSuffix[32];
   FILE*              VectorFile;
   BZFILE*            VectorBZFile;

   const char*        ScalarName;
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

   static StatisticsWriter                      GlobalStatisticsWriter;
   static std::map<uint64_t, StatisticsWriter*> StatisticsSet;
};

#endif
