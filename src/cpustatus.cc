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
 *
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
#include "tools.h"

#include <algorithm>
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
#elif defined(__sun)
#include <kstat.h>
#include <sys/sysinfo.h>
#endif


#if defined(__linux__) || defined(__GNU__)
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

#elif defined(__sun)
#define IDLE_INDEX 3
const char* CPUStatus::CpuStateNames[] = {
   "User", "System", "Wait", "Idle"
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
#if defined(__linux__) || defined(__GNU__)
   CpuStates = 8;
   const long configuredCpus = sysconf(_SC_NPROCESSORS_ONLN);
   CPUs = (configuredCpus > 0) ? static_cast<unsigned int>(configuredCpus) : 1u;

   FILE* procStatFD = fopen("/proc/stat", "r");
   if(procStatFD == nullptr) {
      LOG_FATAL
      // TRANSLATORS: Error message when /proc/stat cannot be opened.
      stdlog << gettext("Unable to open /proc/stat!") << "\n";
      LOG_END_FATAL
   }
   fclose(procStatFD);

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
   if((MachHost = mach_host_self()) == MACH_PORT_NULL) {
      LOG_FATAL
      // TRANSLATORS: Fatal error when macOS mach_host_self() fails.
      stdlog << gettext("mach_host_self() failed!") << "\n";
      LOG_END_FATAL
   }
   count = HOST_BASIC_INFO_COUNT;
   if((kr = host_info(MachHost, HOST_BASIC_INFO, (host_info_t)&hinfo, &count)) != KERN_SUCCESS) {
      LOG_FATAL
      // TRANSLATORS: Fatal error when macOS host_info() fails. Placeholder: {} is error code.
      stdlog << formatText(gettext("host_info() failed with error code {}!"), kr) << "\n";
      LOG_END_FATAL
   }
   CPUs = (hinfo.max_cpus > 0) ? static_cast<unsigned int>(hinfo.max_cpus) : 1u;

#elif defined(__sun)
   CpuStates = 4;
   const long configuredCpus = sysconf(_SC_NPROCESSORS_CONF);
   CPUs = (configuredCpus > 0) ? static_cast<unsigned int>(configuredCpus) : 1u;

#else
#error Missing case!
#endif

   // ====== Allocate buffers ===============================================
   const size_t cpuTimeElements = (CPUs + 1) * CpuStates;
   CpuTimes.assign(cpuTimeElements, 0);
   OldCpuTimes.assign(cpuTimeElements, 0);
   Percentages.assign(cpuTimeElements, 100.0f / static_cast<float>(CpuStates));

   // ====== Initialise =====================================================
   update();
}


// ###### Destructor ########################################################
CPUStatus::~CPUStatus()
{
#if defined(__APPLE__)
   if(MachHost != MACH_PORT_NULL) {
      mach_port_deallocate(mach_task_self(), MachHost);
      MachHost = MACH_PORT_NULL;
   }
#endif
}


// ###### Move Constructor ##################################################
CPUStatus::CPUStatus(CPUStatus&& other) noexcept
   :
#if defined(__APPLE__)
     MachHost(other.MachHost),
#endif
     CPUs(other.CPUs),
     CpuStates(other.CpuStates),
     CpuTimes(std::move(other.CpuTimes)),
     OldCpuTimes(std::move(other.OldCpuTimes)),
     Percentages(std::move(other.Percentages))
{
#if defined(__APPLE__)
   other.MachHost = MACH_PORT_NULL;
#endif
   other.CPUs      = 0;
   other.CpuStates = 0;
}


// ###### Move Assignment Operator ##########################################
CPUStatus& CPUStatus::operator=(CPUStatus&& other) noexcept
{
   if(this != &other) {
#if defined(__APPLE__)
      if(MachHost != MACH_PORT_NULL) {
         mach_port_deallocate(mach_task_self(), MachHost);
      }
      MachHost       = other.MachHost;
      other.MachHost = MACH_PORT_NULL;
#endif
      CPUs           = other.CPUs;
      CpuStates      = other.CpuStates;
      CpuTimes       = std::move(other.CpuTimes);
      OldCpuTimes    = std::move(other.OldCpuTimes);
      Percentages    = std::move(other.Percentages);

      other.CPUs      = 0;
      other.CpuStates = 0;
   }
   return *this;
}


// ###### Update status #####################################################
void CPUStatus::update()
{
   // ====== Save old values ================================================
   OldCpuTimes = CpuTimes;
   std::ranges::fill(CpuTimes, 0);

   // ====== Get counters ===================================================
#if defined(__linux__) || defined(__GNU__)
   FILE* procStatFD = fopen("/proc/stat", "r");
   if(procStatFD != nullptr) {
      for(unsigned int i = 0; i <= CPUs; i++) {
         char buffer[1024];
         if(fgets(buffer, sizeof(buffer), procStatFD) == nullptr) {
            LOG_FATAL
            // TRANSLATORS: Error message when reading from /proc/stat fails.
            stdlog << gettext("Unable to read from /proc/stat!") << "\n";
            LOG_END_FATAL
         }
         int result = 0;
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
            unsigned int id = 0;
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
            if(result < 1) {
               break;
            }
         }
         if( ((i == 0) && (result < 8)) || ((i > 0) && (result < 9)) ) {
            LOG_FATAL
            // TRANSLATORS: Error message when /proc/stat contains malformed input.
            stdlog << gettext("Bad input format in /proc/stat!") << "\n";
            LOG_END_FATAL
         }
      }
      fclose(procStatFD);
   }
#else

   // ------ Get the per-core values ----------------------------------------
#if defined(__FreeBSD__) || defined(__NetBSD__)
   const size_t cpuTimesSize = sizeof(tick_t) * CPUs * CpuStates;   /* Total is calculated later! */
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
      // TRANSLATORS: Error message when sysctl for kern.cp_times fails.
      stdlog << gettext("Unable to obtain kern.cp_times!") << "\n";
      LOG_END_FATAL
   }

#elif defined(__OpenBSD__)
   int mibKernCpTime[3] = { CTL_KERN, KERN_CPTIME2, 0 };
   for(unsigned int i = 0; i < CPUs; i++) {
      mibKernCpTime[2] = static_cast<int>(i);
      size_t length = sizeof(tick_t) * CpuStates;
      if(sysctl(mibKernCpTime, sizeof(mibKernCpTime) / sizeof(mibKernCpTime[0]),
                &CpuTimes[(i + 1) * CpuStates], &length, nullptr, 0) < 0) {
         LOG_FATAL
         // TRANSLATORS: Error message when OpenBSD sysctl for per-core KERN_CPTIME2 fails. Placeholder: {} is CPU core index.
         stdlog << formatText(gettext("Unable to obtain per-core KERN_CPTIME2 for core {}!"), i) << "\n";
         LOG_END_FATAL
      }
   }

#elif defined(__sun)
   kstat_ctl_t* kc = kstat_open();
   if(kc == nullptr) {
      LOG_FATAL
      // TRANSLATORS: Error message when Solaris kstat_open() fails.
      stdlog << gettext("Unable to open kstat!") << "\n";
      LOG_END_FATAL
   }

   unsigned int cpuIndex = 0;
   for(kstat_t* ksp = kc->kc_chain; (ksp != nullptr) && (cpuIndex < CPUs); ksp = ksp->ks_next) {
      if(strcmp(ksp->ks_module, "cpu_stat") == 0) {
         if(kstat_read(kc, ksp, nullptr) != -1) {
            const cpu_stat_t* cs = static_cast<cpu_stat_t*>(ksp->ks_data);
            const unsigned int base = (cpuIndex + 1) * CpuStates;
            CpuTimes[base + 0] = cs->cpu_sysinfo.cpu[CPU_USER];
            CpuTimes[base + 1] = cs->cpu_sysinfo.cpu[CPU_KERNEL];
            CpuTimes[base + 2] = cs->cpu_sysinfo.cpu[CPU_WAIT];
            CpuTimes[base + 3] = cs->cpu_sysinfo.cpu[CPU_IDLE];
            cpuIndex++;
         }
      }
   }
   kstat_close(kc);

#elif defined(__APPLE__)
   kern_return_t          kr;
   processor_info_array_t processorInfoArray;
   natural_t              processorCount;
   mach_msg_type_number_t infoCount;

   if((kr = host_processor_info(MachHost, PROCESSOR_CPU_LOAD_INFO, &processorCount,
                                &processorInfoArray, &infoCount)) != KERN_SUCCESS) {
      LOG_FATAL
      // TRANSLATORS: Fatal error when macOS host_processor_info() fails. Placeholder: {} is error code.
      stdlog << formatText(gettext("host_processor_info() failed with error code {}"), kr) << "\n";
      LOG_END_FATAL
   }
   const processor_cpu_load_info_t cpuLoadInfo =
      reinterpret_cast<processor_cpu_load_info_t>(processorInfoArray);
   const natural_t limit = std::min(processorCount, static_cast<natural_t>(CPUs));
   for(natural_t i = 0; i < limit; i++) {
      for(unsigned int j = 0; j < CpuStates; j++) {
         CpuTimes[((i + 1) * CpuStates) + j] = cpuLoadInfo[i].cpu_ticks[j];
      }
   }
   vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(processorInfoArray),
                 infoCount * sizeof(integer_t));
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
   std::vector<tick_t> diff(CpuStates);
   for(unsigned int i = 0; i < CPUs + 1; i++) {
      tick_t diffTotal = 0;
      for(unsigned int j = 0; j < CpuStates; j++) {
         const unsigned int index = (i * CpuStates) + j;
         diff[j] = CpuTimes[index] - OldCpuTimes[index];
         diffTotal += diff[j];
      }
      for(unsigned int j = 0; j < CpuStates; j++) {
         const unsigned int index = (i * CpuStates) + j;
         if(diffTotal != 0) {   // Avoid division by zero!
            Percentages[index] = static_cast<float>(100.0 * static_cast<double>(diff[j]) / static_cast<double>(diffTotal));
         } else {
            // No ticks passed => 100% idle
            Percentages[index] = (j == IDLE_INDEX) ? 100.0f : 0.0f;
         }
         assert((Percentages[index] >= 0.0f) && (Percentages[index] <= 100.0f));
      }
   }
}


// ###### Get CPU utilization ###############################################
float CPUStatus::getCpuUtilization(const unsigned int cpuIndex) const noexcept
{
   return 100.0f - getCpuStatePercentage(cpuIndex, IDLE_INDEX);
}
