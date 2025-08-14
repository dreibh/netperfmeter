/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2025 by Thomas Dreibholz
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

#include "loglevel.h"
#include "tools.h"

#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>

#include <fstream>
#include <iostream>


std::ostream* gStdLog        = &std::cerr;
unsigned int  gLogLevel      = LOGLEVEL_INFO;
bool          gColorMode     = true;
bool          gCloseStdLog   = false;
char          gHostName[256] = { 0x00 };
Mutex         gLogMutex;


// ###### Initialise log file ###############################################
bool initLogFile(const unsigned int logLevel,
                 const char*        fileName,
                 const bool         appendMode)
{
   finishLogging();
   if(fileName != nullptr) {
      std::ofstream* newLogFile =
         new std::ofstream(fileName, (appendMode == true) ?
                                        std::ios_base::app : std::ios_base::trunc);
      if(newLogFile != nullptr) {
         if(newLogFile->good()) {
            gStdLog = newLogFile;
            gCloseStdLog = true;
            gLogLevel    = std::min(std::max(logLevel, MIN_LOGLEVEL), MAX_LOGLEVEL);
            return true;
         }
         delete newLogFile;
      }
   }
   return false;
}


// ###### Begin logging #####################################################
void beginLogging()
{
   gLogMutex.lock();

   if(gStdLog == nullptr) {
      gStdLog      = &std::cerr;
      gCloseStdLog = false;
   }
   if((gCloseStdLog) && (gStdLog->tellp() > 0)) {
      *gStdLog << "\n"
               << "#############################################################################"
               << "\n\n";
   }

   utsname hostInfo;
   if(uname(&hostInfo) != 0) {
      strcpy((char*)&gHostName, "?");
   }
   else {
      snprintf((char*)&gHostName, sizeof(gHostName), "%s",
               hostInfo.nodename);
   }

   LOG_DEBUG
   *gStdLog << format("Logging started, log level is %d.", gLogLevel)
            << "\n";
   LOG_END

   gLogMutex.unlock();
}


// ###### Finish logging ####################################################
void finishLogging()
{
   if((gStdLog) && (gCloseStdLog)) {
      LOG_DEBUG
      *gStdLog << "Logging finished.\n";
      LOG_END
      delete gStdLog;
      gStdLog      = &std::cerr;
      gCloseStdLog = false;
   }
}
