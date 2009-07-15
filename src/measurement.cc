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
 * You should have relReceived a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact: dreibh@iem.uni-due.de
 */

#include "measurement.h"
#include "flow.h"


// ###### Destructor ########################################################
Measurement::Measurement()
{
   MeasurementID        = 0;
   FirstTransmission    = 0;
   LastTransmission     = 0;
   FirstReception       = 0;
   LastReception        = 0;

   StatisticsInterval   = 1000000;
   FirstStatisticsEvent = 0;
   LastStatisticsEvent  = 0;
   NextStatisticsEvent  = 0;
}


// ###### Destructor ########################################################
Measurement::~Measurement()
{
   finish(true);
}


// ###### Initialize measurement ############################################
bool Measurement::initialize(const unsigned long long now,
                             const uint64_t           measurementID,
                             const char*              vectorNamePattern,
                             const bool               compressVectorFile,
                             const char*              scalarNamePattern,
                             const bool               compressScalarFile)
{
   MeasurementID        = measurementID;
   FirstStatisticsEvent = 0;
   LastStatisticsEvent  = 0;
   NextStatisticsEvent  = 0;

   if(FlowManager::getFlowManager()->addMeasurement(this)) {
      VectorNamePattern = (vectorNamePattern != NULL) ?
                             std::string(vectorNamePattern) : std::string(); 
      const bool s1 = VectorFile.initialize(
                         (vectorNamePattern != NULL) ?
                            Flow::getNodeOutputName(vectorNamePattern, "active").c_str() : NULL,
                         compressVectorFile);
      ScalarNamePattern = (scalarNamePattern != NULL) ?
                             std::string(scalarNamePattern) : std::string(); 
      const bool s2 = ScalarFile.initialize(
                         (scalarNamePattern != NULL) ?
                            Flow::getNodeOutputName(scalarNamePattern, "active").c_str() : NULL,
                         compressScalarFile);
      return(s1 && s2);
   }
   return(false);
}


// ###### Finish measurement ################################################
bool Measurement::finish(const bool closeFiles)
{
   FlowManager::getFlowManager()->removeMeasurement(this);
   const bool s1 = VectorFile.finish(closeFiles);
   const bool s2 = ScalarFile.finish(closeFiles);
   return(s1 && s2);
}


// ###### Write scalars #####################################################
void Measurement::writeScalarStatistics(const unsigned long long now)
{
   lock();
   FlowManager::getFlowManager()->writeScalarStatistics(
      MeasurementID, now, ScalarFile,
      FirstStatisticsEvent);
   unlock();
}


// ###### Write vectors #####################################################
void Measurement::writeVectorStatistics(const unsigned long long now,
                                        FlowBandwidthStats&      globalStats,
                                        FlowBandwidthStats&      relGlobalStats)
{
   lock();

   // ====== Timer management ===============================================
   NextStatisticsEvent += StatisticsInterval;
   if(NextStatisticsEvent <= now) {   // Initialization!
      NextStatisticsEvent = now + StatisticsInterval;
   }
   if(FirstStatisticsEvent == 0) {
      FirstStatisticsEvent = now;
      LastStatisticsEvent  = now;
   }

   // ====== Write statistics ===============================================
   FlowManager::getFlowManager()->writeVectorStatistics(
      MeasurementID, now, VectorFile,
      globalStats, relGlobalStats,
      FirstStatisticsEvent, LastStatisticsEvent);
   
   // ====== Update timing ==================================================   
   LastStatisticsEvent = now;

   unlock();
}