/* $Id: netperfmeter.cc 720 2010-08-26 07:29:11Z dreibh $
 *
 * Network Performance Meter
 * Copyright (C) 2009-2010 by Thomas Dreibholz
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <math.h>
#include <assert.h>

#include <iostream>


typedef unsigned long long tick_t;


FILE* ProcStatFD;
unsigned int CPUs;
unsigned int CpuStates;

tick_t CpuTimes[1000];


//    TIC_t u, n, s, i, w, x, y, z; // as represented in /proc/stat
const char* CpuStateNames[] = {
   "User",
   "Nice",
   "System",
   "Idle",
   "IOWait",
   "Hardware Interrupts",
   "Software Interrupts",
   "Hypervisor"
};

void update()
{
#if defined __FreeBSD__
#error xy
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
      printf("i=%d:\t",result);
      for(int j = 0;j < 8;j++) {
         printf("\t%s=%8Ld", CpuStateNames[j], CpuTimes[(i * CpuStates) + j]);
      }
      puts("");
      if( ((i == 0) && (result < 8)) || ((i > 0) && (result < 9)) ) {
         std::cerr << "ERROR: Bad input fromat in /proc/stat!" << std::endl;
         exit(1);
      }
   }
#endif
}

// ###### Main program ######################################################
int main(int argc, char** argv)
{
#if defined __FreeBSD__
#error xy
#elif defined __linux__
   CpuStates = 8;
   CPUs = sysconf(_SC_NPROCESSORS_CONF);
   if(CPUs < 1) {
      CPUs = 1;
   }
   printf("CPUs=%d\n", CPUs);

   ProcStatFD = fopen("/proc/stat", "r");
   if(ProcStatFD == NULL) {
      std::cerr << "ERROR: Unable to open /proc/stat!" << std::endl;
      exit(1);
   }
#endif

   update();

   return 0;
}
