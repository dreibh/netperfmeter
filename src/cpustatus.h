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

#ifndef CPUSTATUS_H
#define CPUSTATUS_H

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
#endif


class CPUStatus
{
   // ====== Public methods =================================================
   public:
   CPUStatus();
   ~CPUStatus();

   CPUStatus(const CPUStatus&) = delete;
   CPUStatus& operator=(const CPUStatus&) = delete;

   CPUStatus(CPUStatus&& other) noexcept;
   CPUStatus& operator=(CPUStatus&& other) noexcept;

   void update();

   [[nodiscard]] inline unsigned int getNumberOfCPUs() const noexcept {
      return CPUs;
   }
   [[nodiscard]] inline unsigned int getCpuStates() const noexcept {
      return CpuStates;
   }
   [[nodiscard]] inline const char* getCpuStateName(const unsigned int index) const noexcept {
      assert(index < CpuStates);
      return CpuStateNames[index];
   }
   [[nodiscard]] inline float getCpuStatePercentage(const unsigned int cpuIndex,
                                                    const unsigned int stateIndex) const noexcept {
      assert(cpuIndex <= CPUs);
      assert(stateIndex < CpuStates);
      const unsigned int index = (cpuIndex * CpuStates) + stateIndex;
      return Percentages[index];
   }
   [[nodiscard]] float getCpuUtilization(const unsigned int cpuIndex) const noexcept;

   // ====== Private data ===================================================
   private:
#if defined(__linux__) || defined(__GNU__)
   using tick_t = unsigned long long;
#elif defined(__NetBSD__)
   using tick_t = uint64_t;
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__sun)
   using tick_t = unsigned long;
#elif defined(__APPLE__)
   using tick_t = unsigned int;
#endif

#if defined(__APPLE__)
   host_name_port_t    MachHost  { MACH_PORT_NULL };
#endif
   unsigned int        CPUs      { 0 };
   unsigned int        CpuStates { 0 };
   std::vector<tick_t> CpuTimes;
   std::vector<tick_t> OldCpuTimes;
   std::vector<float>  Percentages;

   static const char*  CpuStateNames[];
};

#endif // CPUSTATUS_H
