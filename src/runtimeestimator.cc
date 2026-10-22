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

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact:  dreibh@simula.no
 * Homepage: https://www.nntb.no/~dreibh/netperfmeter/
 */

#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <unistd.h>

#if defined(HAVE_LIBIBERTY)
#include <libiberty.h>
extern "C" {
int getopt_long_only(int argc, char* const* argv, const char* optstring,
                     const struct option* longopts, int* longindex);
}
#endif

// Endianness conversions: htobe64(), be64toh():
#if defined(__linux__) || defined(__GNU__)
#include <endian.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/endian.h>
#elif defined(__OpenBSD__) || defined(__sun)
#include <endian.h>
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#elif defined(_WIN32)
#if defined(_MSC_VER)
#define htobe64(x) _byteswap_uint64(x)
#define be64toh(x) _byteswap_uint64(x)
#else
#define htobe64(x) __builtin_bswap64(x)
#define be64toh(x) __builtin_bswap64(x)
#endif
#else
#error Unsupported system: add endianness conversion function definitions!
#endif

#include "package-version.h"


// ###### Version ###########################################################
[[ noreturn ]] static void version()
{
   std::cerr << "RuntimeEstimator" << " " << RUNTIMEESTIMATOR_VERSION << "\n";
   exit(0);
}


// ------ BEGIN-OF-USAGE-BLOCK ------
// ###### Usage #############################################################
#define HEADER(s)    "\x1b[4m" << (s) << "\x1b[0m"
#define CMD(s)       "\x1b[1m" << (s) << "\x1b[0m"
#define OPT(s)       "\x1b[1m" s "\x1b[0m"
#define ARG(s)       "\x1b[4m" s "\x1b[0m"
#define SYN(s)       "\x1b[1m" s "\x1b[0m"
#define PIPE         SYN(" | ")
#define ENTRY(s)     "\t" SYN("[") s SYN("]\n")

[[ noreturn ]]
static void usage(const char* program, const int exitCode)
{
   std::cerr << HEADER("Usage:") << "\n"
             << CMD(program) << " "
                ARG("start_file") " " ARG("total_runs") " " ARG("current_run") "\n"
             << CMD(program) << "\n"
                   ENTRY(OPT("-h") PIPE OPT("--help"))
             << CMD(program) << "\n"
                   ENTRY(OPT("-v") PIPE OPT("--version"));
   exit(exitCode);
}
// ------ END-OF-USAGE-BLOCK ------


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Handle command-line arguments ==================================
   const static struct option long_options[] = {
      { "help",    no_argument, 0, 'h' },
      { "version", no_argument, 0, 'v' },
      {  nullptr,  0,           0, 0   }
   };

   int option;
   int longIndex;
   while( (option = getopt_long_only(argc, argv, "hv", long_options, &longIndex)) != -1 ) {
      switch(option) {
         case 'v':
            version();
          break;
         case 'h':
         case '?':
            // Exit with 0 on h/help, exit with 1 on '?' (unknown option):
            usage(argv[0], (option == 'h') ? 0 : 1);
          break;
         case '-':
          break;
         default:
            // This should not happen: wrong getopt parameters, or missing case?
            fprintf(stderr, "INTERNAL ERROR: Unhandled option c=%c code=%x!\n",
                    (isprint(option) ? (char)option : ' '), option);
            return 1;
          break;
      }
   }

   if(optind + 3 != argc) {
      usage(argv[0], 1);
   }

   const char* startFileName = argv[optind++];
   const char* totalRunsStr  = argv[optind++];
   const char* currentRunStr = argv[optind++];

   char* endptr;
   long  value;

   value = strtol(totalRunsStr, &endptr, 10);
   if( (endptr == totalRunsStr) || (*endptr != 0x00) || (value < 1) ) {
      std::cerr << "ERROR: Invalid total runs parameter (must be >= 1)!\n";
      exit(1);
   }
   const unsigned int totalRuns = (unsigned int)value;

   value = strtol(currentRunStr, &endptr, 10);
   if( (endptr == currentRunStr) || (*endptr != 0x00) || (value < 0) ) {
      std::cerr << "ERROR: Invalid current run parameter (must be >= 0)!\n";
      exit(1);
   }
   const unsigned int currentRun = (unsigned int)value;

   if(currentRun > totalRuns) {
      std::cerr << "ERROR: Current run must be less or equal to total runs!\n";
      exit(1);
   }

   // ====== Initialise (Run 0) =============================================
   if(currentRun == 0) {
      std::ofstream stream(startFileName, std::ios::binary | std::ios::trunc);
      if(!stream.is_open()) {
         std::cerr << "ERROR: Unable to create file <" << startFileName << ">!\n";
         exit(1);
      }
      const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
      const uint64_t nowMicroseconds = static_cast<uint64_t>(
         std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count()
      );
      const uint64_t nowToWrite = htobe64(nowMicroseconds);
      if(!stream.write(reinterpret_cast<const char*>(&nowToWrite), sizeof(nowToWrite))) {
         std::cerr << "ERROR: Unable to write timestamp to file <" << startFileName << ">!\n";
         exit(1);
      }
      printf("\x1b[34m##### ESTIMATION: - [0 of %u - 0.00%%]\x1b[0m\n", totalRuns);
   }

   // ====== Estimate runtime (Run > 0) =====================================
   else {
      std::ifstream stream(startFileName, std::ios::binary);
      if(!stream.is_open()) {
         std::cerr << "ERROR: Unable to open file <" << startFileName << ">!\n";
         exit(1);
      }
      uint64_t timeStampRead;
      if(!stream.read(reinterpret_cast<char*>(&timeStampRead), sizeof(timeStampRead))) {
         std::cerr << "ERROR: Unable to read timestamp from file <" << startFileName << ">!\n";
         exit(1);
      }
      const uint64_t startTimeStamp = be64toh(timeStampRead);

      const std::chrono::system_clock::time_point now       = std::chrono::system_clock::now();
      const std::chrono::system_clock::time_point startTime = std::chrono::system_clock::time_point(
         std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::microseconds(startTimeStamp)
         )
      );

      const std::chrono::duration<double, std::micro> elapsed =
         now - startTime;
      const std::chrono::duration<double, std::micro> usecPerRun =
         elapsed / currentRun;
      const std::chrono::duration<double, std::micro> estimatedRuntime =
         usecPerRun * (totalRuns - currentRun);
      const std::chrono::system_clock::time_point estimatedEndTime = now +
         std::chrono::duration_cast<std::chrono::system_clock::duration>(
         (estimatedRuntime.count() > 0.0) ?
            estimatedRuntime :
            std::chrono::duration<double, std::micro>::zero()
      );

      const std::time_t timeStamp = std::chrono::system_clock::to_time_t(estimatedEndTime);
      const struct tm*  timeptr   = std::localtime(&timeStamp);

      char str[64];
      std::strftime(str, sizeof(str), "ESTIMATION: %d-%b-%Y %H:%M:%S", timeptr);

      const uint64_t estimatedEndTimeUs = static_cast<uint64_t>(
         std::chrono::duration_cast<std::chrono::microseconds>(
            estimatedEndTime.time_since_epoch()).count()
      );
      const double remainingMinutes =
         std::chrono::duration_cast<std::chrono::duration<double, std::ratio<60>>>(estimatedRuntime).count();

      printf("\x1b[34m##### %s.%04u (in %1.2f minutes)  [%u of %u - %1.2f%%]\x1b[0m\n",
             str, static_cast<unsigned int>((estimatedEndTimeUs % 1000000ULL) / 100ULL),
             remainingMinutes,
             currentRun, totalRuns, (100.0 * currentRun) / (double)totalRuns);
   }

   return 0;
}
