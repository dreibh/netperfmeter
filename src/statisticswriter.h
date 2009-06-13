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

#include "flowspec.h"


class StatisticsWriter
{
   public:
   StatisticsWriter();
   ~StatisticsWriter();

   inline unsigned long long getNextEvent() const {
      return(NextStatisticsEvent);
   }

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

   bool writeVectorStatistics(const unsigned long long now,
                              std::vector<FlowSpec*>& flowSet);
   bool writeScalarStatistics(const unsigned long long now,
                              std::vector<FlowSpec*>&  flowSet);

   public:
   unsigned long long StatisticsInterval;



   unsigned long long FirstStatisticsEvent;
   unsigned long long LastStatisticsEvent;
   unsigned long long NextStatisticsEvent;

   unsigned long long VectorLine;
   const char*        VectorName;
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
   unsigned long long LastTotalTransmittedBytes;
   unsigned long long LastTotalTransmittedPackets;
   unsigned long long LastTotalTransmittedFrames;
   unsigned long long LastTotalReceivedBytes;
   unsigned long long LastTotalReceivedPackets;
   unsigned long long LastTotalReceivedFrames;
};

#endif
