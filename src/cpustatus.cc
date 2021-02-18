/*
 * ==========================================================================
 *                  NetPerfMeter -- Network Performance Meter                 
 *                 Copyright (C) 2009-2021 by Thomas Dreibholz
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

#include "cpustatus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#ifdef __linux__
#include <linux/sysctl.h>
#endif
#ifdef __FreeBSD__
#include <sys/proc.h>
#include <sys/sysctl.h>
#endif
#ifdef __APPLE__
#include <mach/mach.h>
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
#elif defined __APPLE__
#define IDLE_INDEX 2
const char* CPUStatus::CpuStateNames[] = {
   "User", "System", "Idle", "Nice"
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
   CpuStates = CPUSTATES;
   getSysCtl("hw.ncpu", &CPUs, sizeof(CPUs));
#elif defined __linux__
   CpuStates = 8;
   CPUs = sysconf(_SC_NPROCESSORS_CONF);
   if(CPUs < 1) {
      CPUs = 1;
   }

   ProcStatFD = fopen("/proc/stat", "r");
   if(ProcStatFD == NULL) {
      std::cerr << "ERROR: Unable to open /proc/stat!" << std::endl;
      exit(1);
   }
#elif defined __APPLE__
#ifdef USE_PER_CPU_STATISTICS
   kern_return_t kr;
   mach_msg_type_number_t count;
   host_basic_info_data_t hinfo;
#endif

   CpuStates = CPU_STATE_MAX;
   if ((host = mach_host_self()) == MACH_PORT_NULL) {
      std::cerr << "ERROR: Couldn't receive send rights." << std::endl;
      exit(1);
   }
#ifdef USE_PER_CPU_STATISTICS
   if((kr = host_get_host_priv_port(host, &host_priv)) != KERN_SUCCESS) {
      mach_error("host_get_host_priv_port():", kr);
      exit(1);
   }
   count = HOST_BASIC_INFO_COUNT;
   if((kr = host_info(host, HOST_BASIC_INFO, (host_info_t)&hinfo, &count)) != KERN_SUCCESS) {
      mach_error("host_info():", kr);
      exit(1);
   };
   CPUs = hinfo.max_cpus;
#else
   CPUs = 1;
#endif
#endif

   // ====== Allocate current times array ===================================
   size_t cpuTimesSize = sizeof(tick_t) * (CPUs + 1) * CpuStates;
   CpuTimes = (tick_t*)calloc(1, cpuTimesSize);
   assert(CpuTimes != NULL);

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
#ifdef __APPLE__
#ifdef USE_PER_CPU_STATISTICS
   mach_port_deallocate(mach_task_self(), host_priv);
#endif
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
#ifdef __FreeBSD__
   cpuTimesSize = sizeof(tick_t) * CPUs * CpuStates;   /* Total is calculated later! */
   getSysCtl("kern.cp_times", &CpuTimes[CpuStates], cpuTimesSize);
   // ------ Compute total values -------------------------
   for(unsigned int j = 0; j < CpuStates; j++) {
      CpuTimes[j] = 0;
      for(unsigned int i = 0; i < CPUs; i++) {
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

#elif __APPLE__
   kern_return_t kr;
#ifdef USE_PER_CPU_STATISTICS
   processor_port_array_t processor_list;
   natural_t processor_count, info_count;
   processor_cpu_load_info_data_t cpu_load_info;

   if((kr = host_processors(host_priv, &processor_list, &processor_count)) != KERN_SUCCESS) {
      mach_error("host_processors():", kr);
      exit(1);
   }
   for(unsigned int i = 0; i < processor_count; i++) {
      info_count = PROCESSOR_CPU_LOAD_INFO_COUNT;
      if ((kr = processor_info(processor_list[i], PROCESSOR_CPU_LOAD_INFO, &host, (processor_info_t)&cpu_load_info, &info_count)) != KERN_SUCCESS) {
         mach_error("processor_info():", kr);
         exit(1);
      }
      for(unsigned int j = 0; j < CpuStates; j++) {
         CpuTimes[((i + 1) * CpuStates) + j] = cpu_load_info.cpu_ticks[j];
      }
   }
   vm_deallocate(mach_task_self(), (vm_address_t)processor_list, processor_count * sizeof(processor_t *));
   // ------ Compute total values -------------------------
   for(unsigned int j = 0; j < CpuStates; j++) {
      CpuTimes[j] = 0;
      for(unsigned int i = 0; i < CPUs; i++) {
         CpuTimes[j] += CpuTimes[((i + 1) * CpuStates) + j];
      }
   }
#else
   mach_msg_type_number_t count;
   host_cpu_load_info_data_t cpu_load_info;

   count = HOST_CPU_LOAD_INFO_COUNT;
   if((kr = host_statistics(host, HOST_CPU_LOAD_INFO, (host_info_t)&cpu_load_info, &count)) != KERN_SUCCESS) {
      mach_error("host_statistics():", kr);
      exit(1);
   }
   for(unsigned int j = 0; j < CpuStates; j++) {
      CpuTimes[j] = CpuTimes[CpuStates + j] = cpu_load_info.cpu_ticks[j];
   }
#endif
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
