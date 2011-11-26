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

#include "cpustatus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#ifdef __FreeBSD__
#include <sys/proc.h>
#endif
#include <iostream>


#ifdef __FreeBSD__
#define IDLE_INDEX 4
const char* CPUStatus::CpuStateNames[] = {
   "User", "Nice", "System", "Interrupt", "Idle"
};
#elif defined __linux__
#define IDLE_INDEX 3
const char* CPUStatus::CpuStateNames[] = {
   "User", "Nice", "System", "Idle", "IOWait",
   "Hardware Interrupts", "Software Interrupts", "Hypervisor"
};
#endif

#ifdef __FreeBSD__
bool CPUStatus::getSysCtl(const char* name, void* ptr, size_t len)
{
   size_t nlen = len;
   if(sysctlbyname(name, ptr, &nlen, NULL, 0) < 0) {
      std::cerr << "ERROR: sysctlbyname(" << name << ") failed: "
                << strerror(errno) << std::endl;
      exit(1);
   }
   if(nlen != len) {
      return(false);
   }
   return(true);
}
#endif


// ###### Constructor #######################################################
CPUStatus::CPUStatus()
{
   // ====== Initialize =====================================================
#ifdef __FreeBSD__
   int maxCPUs;
   CpuStates = CPUSTATES;
   getSysCtl("kern.smp.maxcpus", &maxCPUs, sizeof(maxCPUs));
#elif defined __linux__
   CpuStates = 8;
   CPUs = sysconf(_SC_NPROCESSORS_CONF);
   if(CPUs < 1) {
      CPUs = 1;
   }
   const int maxCPUs = CPUs;

   ProcStatFD = fopen("/proc/stat", "r");
   if(ProcStatFD == NULL) {
      std::cerr << "ERROR: Unable to open /proc/stat!" << std::endl;
      exit(1);
   }
#endif

   // ====== Allocate current times array ===================================
   size_t cpuTimesSize = sizeof(tick_t) * (maxCPUs + 1) * CpuStates;
   CpuTimes = (tick_t*)calloc(1, cpuTimesSize);
   assert(CpuTimes != NULL);
#ifdef __FreeBSD__
   getSysCtl("kern.cp_times", CpuTimes, cpuTimesSize);
   CPUs = cpuTimesSize / CpuStates / sizeof(tick_t);
#endif

   // ====== Allocate old times array =======================================
   cpuTimesSize = sizeof(tick_t) * (CPUs + 1) * CpuStates;
   OldCpuTimes = (tick_t*)malloc(cpuTimesSize);
   assert(OldCpuTimes != NULL);
   memcpy(OldCpuTimes, CpuTimes, cpuTimesSize);

   // ====== Allocate percentages array =====================================
   const size_t percentagesSize = sizeof(float) * (CPUs + 1) * CpuStates;
   Percentages = (float*)malloc(percentagesSize);
   assert(Percentages != NULL);
   for(unsigned int i = 0; i < (CPUs + 1) * CpuStates; i++) {
      Percentages[i] = 100.0 / CpuStates;
   }
#ifdef __linux__
   update();
#endif
}


// ###### Destructor ########################################################
CPUStatus::~CPUStatus()
{
   free(CpuTimes);
   CpuTimes = NULL;
   free(OldCpuTimes);
   OldCpuTimes = NULL;
   free(Percentages);
   Percentages = NULL;
#ifdef __linux__
   fclose(ProcStatFD);
   ProcStatFD = NULL;
#endif
}


// ###### Update status #####################################################
void CPUStatus::update()
{
   // ====== Save old values ================================================
   size_t cpuTimesSize = sizeof(tick_t) * (CPUs + 1) * CpuStates;
   memcpy(OldCpuTimes, CpuTimes, cpuTimesSize);


   // ====== Get counters ===================================================
#ifdef __FreeBSD__
   cpuTimesSize = sizeof(tick_t) * CPUs * CpuStates;   /* Total is calculated later! */
   getSysCtl("kern.cp_times", &CpuTimes[CpuStates], cpuTimesSize);
   // ------ Compute total values -------------------------
   for(unsigned int j = 0; j < CpuStates; j++) {
      CpuTimes[j] = 0;
   }
   for(unsigned int i = 0; i < CPUs; i++) {
      for(unsigned int j = 0; j < CpuStates; j++) {
         CpuTimes[j] += CpuTimes[((i + 1) * CpuStates) + j];
      }
   }

#elif defined __linux__
   rewind(ProcStatFD);
   fflush(ProcStatFD);

   for(unsigned int i = 0; i <= CPUs; i++) {
      char buffer[1024];
      if(fgets(buffer, sizeof(buffer), ProcStatFD) == 0) {
         std::cerr << "ERROR: Unable to read from /proc/stat!" << std::endl;
         exit(1);
      }
      int result;
      if(i == 0) {   // Get totals
         result = sscanf(buffer, "cpu %llu %llu %llu %llu %llu %llu %llu %llu\n",
                         &CpuTimes[0],
                         &CpuTimes[1],
                         &CpuTimes[2],
                         &CpuTimes[3],
                         &CpuTimes[4],
                         &CpuTimes[5],
                         &CpuTimes[6],
                         &CpuTimes[7]);
      }
      else {
         unsigned int id;
         result = sscanf(buffer, "cpu%u %llu %llu %llu %llu %llu %llu %llu %llu\n",
                         &id,
                         &CpuTimes[(i * CpuStates) + 0],
                         &CpuTimes[(i * CpuStates) + 1],
                         &CpuTimes[(i * CpuStates) + 2],
                         &CpuTimes[(i * CpuStates) + 3],
                         &CpuTimes[(i * CpuStates) + 4],
                         &CpuTimes[(i * CpuStates) + 5],
                         &CpuTimes[(i * CpuStates) + 6],
                         &CpuTimes[(i * CpuStates) + 7]);
      }
      if( ((i == 0) && (result < 8)) || ((i > 0) && (result < 9)) ) {
         std::cerr << "ERROR: Bad input fromat in /proc/stat!" << std::endl;
         exit(1);
      }
   }
#endif


   // ====== Calculate percentages ==========================================
   for(unsigned int i = 0; i < CPUs + 1; i++) {
      tick_t diffTotal = 0;
      tick_t diff[CpuStates];
      for(unsigned int j = 0; j < CpuStates; j++) {
         const unsigned int index = (i * CpuStates) + j;
         if(CpuTimes[index] >= OldCpuTimes[index]) {
            diff[j] = CpuTimes[index] - OldCpuTimes[index];
         }
         else {   // Counter wrap!
            diff[j] = OldCpuTimes[index] - CpuTimes[index];
         }
         diffTotal += diff[j];
      }
      for(unsigned int j = 0; j < CpuStates; j++) {
         const unsigned int index = (i * CpuStates) + j;
         if(diffTotal != 0) {   // Avoid division by zero!
            Percentages[index] = 100.0 * (float)diff[j] / (float)diffTotal;
         }
         assert( (Percentages[index] >= 0.0) && (Percentages[index] <= 100.0) );
      }
   }
}


// ###### Get CPU utilization ###############################################
float CPUStatus::getCpuUtilization(const unsigned int cpuIndex) const
{
   return(100.0 - getCpuStatePercentage(cpuIndex, IDLE_INDEX));
}
