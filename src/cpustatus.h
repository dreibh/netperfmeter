/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2009-2012 by Thomas Dreibholz
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

#ifndef CPUSTATUS_H
#define CPUSTATUS_H

#include <stdio.h>
#include <assert.h>


class CPUStatus
{
   // ====== Public methods =================================================
   public:
   CPUStatus();
   ~CPUStatus();

   void update();

   inline unsigned int getNumberOfCPUs() const {
      return(CPUs);
   }
   inline unsigned int getCpuStates() const {
      return(CpuStates);
   }
   inline const char* getCpuStateName(const unsigned int index) const {
      assert(index < CpuStates);
      return(CpuStateNames[index]);
   }
   inline float getCpuStatePercentage(const unsigned int cpuIndex,
                                      const unsigned int stateIndex) const {
      assert(cpuIndex <= CPUs);
      assert(stateIndex < CpuStates);
      const unsigned int index = (cpuIndex * CpuStates) + stateIndex;
      return(Percentages[index]);
   }
   float getCpuUtilization(const unsigned int cpuIndex) const;

   // ====== Private data ===================================================
   private:
#ifdef __FreeBSD__
   typedef unsigned long long tick_t;
   static bool getSysCtl(const char* name, void* ptr, size_t len);
#elif defined __linux__
   typedef unsigned long long tick_t;
   FILE*               ProcStatFD;
#endif
   unsigned int        CPUs;
   tick_t*             CpuTimes;
   tick_t*             OldCpuTimes;
   float*              Percentages;
   unsigned int        CpuStates;
   static const char*  CpuStateNames[];
};

#endif
