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

#include "cpustatus.h"
#include "loglevel.h"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#endif


#if defined(__linux__)
#define IDLE_INDEX 3
const char* CPUStatus::CpuStateNames[] = {
   "User", "Nice", "System", "Idle", "IOWait",
   "Hardware Interrupts", "Software Interrupts", "Hypervisor"
};

#elif defined(__FreeBSD__) || defined(__NetBSD__)
#define IDLE_INDEX 4
const char* CPUStatus::CpuStateNames[] = {
   "User", "Nice", "System", "Interrupt", "Idle"
};

#elif defined(__OpenBSD__)
#define IDLE_INDEX 5
const char* CPUStatus::CpuStateNames[] = {
   "User", "Nice", "System", "Spinning", "Interrupt", "Idle"
};

#elif defined(__APPLE__)
#define IDLE_INDEX 2
const char* CPUStatus::CpuStateNames[] = {
   "User", "System", "Idle", "Nice"
};

#else
#error Missing case!
#endif


// ###### Constructor #######################################################
CPUStatus::CPUStatus()
{
   // ====== Initialize =====================================================
#if defined(__linux__)
   CpuStates = 8;
   CPUs = sysconf(_SC_NPROCESSORS_CONF);
   if(CPUs < 1) {
      CPUs = 1;
   }

   ProcStatFD = fopen("/proc/stat", "r");
   if(ProcStatFD == nullptr) {
      LOG_FATAL
      stdlog << "Unable to open /proc/stat!" << "\n";
      LOG_END_FATAL
   }

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
   CpuStates = CPUSTATES;
   const int  mibHwNCpu[2] = { CTL_HW, HW_NCPU };
   size_t     cpusLength = sizeof(CPUs);
   if(sysctl(mibHwNCpu, 2, &CPUs, &cpusLength, nullptr, 0) < 0) {
      CPUs = 1;
   }

#elif defined(__APPLE__)
   kern_return_t          kr;
   mach_msg_type_number_t count;
   host_basic_info_data_t hinfo;

   CpuStates = CPU_STATE_MAX;
   if((host = mach_host_self()) == MACH_PORT_NULL) {
      LOG_FATAL
      stdlog << "Couldn't receive send rights!" << "\n";
      LOG_END_FATAL
   }
   count = HOST_BASIC_INFO_COUNT;
   if((kr = host_info(host, HOST_BASIC_INFO, (host_info_t)&hinfo, &count)) != KERN_SUCCESS) {
      LOG_FATAL
      mach_error("host_info():", kr);
      LOG_END_FATAL
   };
   CPUs = (unsigned int)hinfo.max_cpus;

#else
#error Missing case!
#endif

   // ====== Allocate current times array ===================================
   const size_t cpuTimeElements = (CPUs + 1) * CpuStates;
   CpuTimes = new tick_t[cpuTimeElements]();   // () initialises the elements!
   assert(CpuTimes != nullptr);

   // ====== Allocate old times array =======================================
   OldCpuTimes = new tick_t[cpuTimeElements];
   assert(OldCpuTimes != nullptr);
   const size_t cpuTimesSize = sizeof(tick_t) * cpuTimeElements;
   memcpy(OldCpuTimes, CpuTimes, cpuTimesSize);

   // ====== Allocate percentages array =====================================
   Percentages = new float[cpuTimeElements];
   assert(Percentages != nullptr);
   for(unsigned int i = 0; i < cpuTimeElements; i++) {
      Percentages[i] = 100.0 / CpuStates;
   }

   // ====== Initialise =====================================================
   update();
}


// ###### Destructor ########################################################
CPUStatus::~CPUStatus()
{
   delete[] CpuTimes;
   CpuTimes = nullptr;
   delete[] OldCpuTimes;
   OldCpuTimes = nullptr;
   delete[] Percentages;
   Percentages = nullptr;
#if defined(__linux__)
   fclose(ProcStatFD);
   ProcStatFD = nullptr;
#elif defined(__APPLE__)
   mach_port_deallocate(mach_task_self(), host);
#endif
}


// ###### Update status #####################################################
void CPUStatus::update()
{
   // ====== Save old values ================================================
   size_t cpuTimesSize = sizeof(tick_t) * (CPUs + 1) * CpuStates;
   memcpy(OldCpuTimes, CpuTimes, cpuTimesSize);


   // ====== Get counters ===================================================
#if defined(__linux__)
   fseek(ProcStatFD, 0, SEEK_SET);
   for(unsigned int i = 0; i <= CPUs; i++) {
      char buffer[1024];
      if(fgets(buffer, sizeof(buffer), ProcStatFD) == 0) {
         LOG_FATAL
         stdlog << "Unable to read from /proc/stat!" << "\n";
         LOG_END_FATAL
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
         LOG_FATAL
         stdlog << "Bad input format in /proc/stat!" << "\n";
         LOG_END_FATAL
      }
   }

#else
   // ------ Get the per-core values ----------------------------------------
#if defined(__FreeBSD__) || defined(__NetBSD__)
   cpuTimesSize = sizeof(tick_t) * CPUs * CpuStates;   /* Total is calculated later! */
   size_t length = cpuTimesSize;
   if( (sysctlbyname(
#if defined(__FreeBSD__)
           "kern.cp_times",
#else
           "kern.cp_time",
#endif
           &CpuTimes[CpuStates], &length, nullptr, 0) < 0) ||
       (length != cpuTimesSize) ) {
      LOG_FATAL
      stdlog << "Failed to obtain kern.cp_times!" << "\n";
      LOG_END_FATAL
   }
#elif defined(__NetBSD__)
   int mibKernCpTime[3] = { CTL_KERN, KERN_CPTIME2, 0 };
   for(unsigned int i = 0; i < CPUs; i++) {
      mibKernCpTime[2] = (int)i;
      size_t length = sizeof(tick_t) * CpuStates;
      if(sysctl(mibKernCpTime, sizeof(mibKernCpTime) / sizeof(mibKernCpTime[0]),
                &CpuTimes[(i + 1) * CpuStates], &length, nullptr, 0) < 0) {
         LOG_FATAL
         stdlog << "Failed to obtain per-core KERN_CPTIME2 for core " << i << "!\n";
         LOG_END_FATAL
      }
   }
#elif defined(__APPLE__)
   kern_return_t          kr;
   processor_info_array_t processor_info_array;
   natural_t              processor_count;
   mach_msg_type_number_t info_count;

   if((kr = host_processor_info(host, PROCESSOR_CPU_LOAD_INFO, &processor_count,
                                &processor_info_array, &info_count)) != KERN_SUCCESS) {
      mach_error("host_processor_info():", kr);
      exit(1);
   }
   const processor_cpu_load_info_t cpuLoadInfo =
      (processor_cpu_load_info_t)processor_info_array;

   for(unsigned int i = 0; i < processor_count; i++) {
      for(unsigned int j = 0; j < CpuStates; j++) {
         CpuTimes[((i + 1) * CpuStates) + j] = cpuLoadInfo[i].cpu_ticks[j];
      }
   }
   vm_deallocate(mach_task_self(), (vm_address_t)processor_info_array,
                 info_count * sizeof(integer_t));
#endif

   // ------ Compute total values -------------------------------------------
   for(unsigned int j = 0; j < CpuStates; j++) {
      CpuTimes[j] = 0;
      for(unsigned int i = 0; i < CPUs; i++) {
         CpuTimes[j] += CpuTimes[((i + 1) * CpuStates) + j];
      }
   }
#endif

   // ====== Calculate percentages ==========================================
   for(unsigned int i = 0; i < CPUs + 1; i++) {
      tick_t diffTotal = 0;
      tick_t diff[CpuStates];
      for(unsigned int j = 0; j < CpuStates; j++) {
         const unsigned int index = (i * CpuStates) + j;
         diff[j] = CpuTimes[index] - OldCpuTimes[index];
         diffTotal += diff[j];
      }
      for(unsigned int j = 0; j < CpuStates; j++) {
         const unsigned int index = (i * CpuStates) + j;
         if(diffTotal != 0) {   // Avoid division by zero!
            Percentages[index] = 100.0 * (float)diff[j] / (float)diffTotal;
         } else {
            // No ticks passed => 100% idle
            Percentages[index] = (j == IDLE_INDEX) ? 100.0 : 0.0;
         }
         assert( (Percentages[index] >= 0.0) && (Percentages[index] <= 100.0) );
      }
   }
}


// ###### Get CPU utilization ###############################################
float CPUStatus::getCpuUtilization(const unsigned int cpuIndex) const
{
   return 100.0 - getCpuStatePercentage(cpuIndex, IDLE_INDEX);
}
