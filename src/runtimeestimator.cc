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

#include <byteswap.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <endian.h>
#include <stdint.h>
#include <sys/time.h>


// ###### Get current time stamp ############################################
static unsigned long long getMicroTime()
{
  timespec ts;
  if(__builtin_expect( (clock_gettime(CLOCK_REALTIME, &ts) != 0) , 0)) {
     perror("clock_gettime():");
     abort();
  }
  return ((unsigned long long)ts.tv_sec * 1000000ULL) +
            ((unsigned long long)ts.tv_nsec / 1000ULL);
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Handle arguments ===============================================
   if(argc < 4) {
      fprintf(stderr, "Usage: %s start_file total_runs current_run\n",
              argv[0]);
      exit(1);
   }

   char* endptr;
   long  value;

   value = strtol(argv[2], &endptr, 10);
   if( (endptr == argv[2]) || (*endptr != 0x00) || (value < 1) ) {
      fprintf(stderr, "ERROR: Invalid total runs parameter (must be >= 1)!\n");
      exit(1);
   }
   const unsigned int totalRuns = (unsigned int)value;

   value = strtol(argv[3], &endptr, 10);
   if( (endptr == argv[3]) || (*endptr != 0x00) || (value < 0) ) {
      fprintf(stderr, "ERROR: Invalid current run parameter (must be >= 0)!\n");
      exit(1);
   }
   const unsigned int currentRun = (unsigned int)value;

   if(currentRun > totalRuns) {
      fprintf(stderr, "ERROR: Current run must be less or equal to total runs!\n");
      exit(1);
   }

   // ====== Initialise (Run 0) =============================================
   if(currentRun == 0) {
      FILE* fh = fopen(argv[1], "w");
      if(fh == nullptr) {
         fprintf(stderr, "ERROR: Unable to create file <%s>!\n", argv[1]);
         exit(1);
      }
      unsigned long long now = getMicroTime();
#if __BYTE_ORDER == __BIG_ENDIAN
      const uint64_t nowToWrite = now;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
      const uint64_t nowToWrite = bswap_64(now);
#else
#error Byte order undefined!
#endif
      if(fwrite((void*)&nowToWrite, sizeof(nowToWrite), 1, fh) != 1) {
         fprintf(stderr, "ERROR: Unable to write timestamp to file <%s>!\n", argv[1]);
         fclose(fh);
         exit(1);
      }
      fclose(fh);
      printf("\x1b[34m##### ESTIMATION: - [0 of %u - 0.00%%]\x1b[0m\n", totalRuns);
   }

   // ====== Estimate runtime (Run > 0) =====================================
   else {
      FILE* fh = fopen(argv[1], "r");
      if(fh == nullptr) {
         fprintf(stderr, "ERROR: Unable to open file <%s>!\n", argv[1]);
         exit(1);
      }
      uint64_t timeStampRead;
      if(fread((void*)&timeStampRead, sizeof(timeStampRead), 1, fh) != 1) {
         fprintf(stderr, "ERROR: Unable to read timestamp from file <%s>!\n", argv[1]);
         fclose(fh);
         exit(1);
      }
#if __BYTE_ORDER == __BIG_ENDIAN
      const unsigned long long startTimeStamp = timeStampRead;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
      const unsigned long long startTimeStamp = bswap_64(timeStampRead);
#else
#error Byte order undefined!
#endif
      fclose(fh);

      const unsigned long long now  = getMicroTime();
      const double tstart           = (double)startTimeStamp;
      const double tend             = (double)now;
      const double usecPerRun       = (tend - tstart) / currentRun;
      const double estimatedRuntime = (totalRuns - currentRun) * usecPerRun;
      const unsigned long long estEndTimeStamp =
         now + (unsigned long long)((estimatedRuntime > 0) ? estimatedRuntime : 0.0);

      char             str[64];
      const time_t     timeStamp = (time_t)(estEndTimeStamp / 1000000);
      const struct tm* timeptr   = localtime(&timeStamp);

      strftime(str, sizeof(str), "ESTIMATION: %d-%b-%Y %H:%M:%S", timeptr);
      printf("\x1b[34m##### %s.%04d (in %1.2f minutes)  [%u of %u - %1.2f%%]\x1b[0m\n",
             str, (unsigned int)(estEndTimeStamp % 1000000) / 100,
             estimatedRuntime / 60000000.0,
             currentRun, totalRuns, (100.0 * currentRun) / (double)totalRuns);
   }

   return 0;
}
