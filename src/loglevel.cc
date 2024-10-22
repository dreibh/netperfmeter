/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2024 by Thomas Dreibholz
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
unsigned int  gLogLevel      = LOGLEVEL_TRACE;
bool          gColorMode     = true;
bool          gCloseStdLog   = false;
char          gHostName[256] = { 0x00 };
Mutex         gLogMutex;


// ###### Initialise log file ###############################################
static bool initLogFile(const unsigned int logLevel,
                        const char*        fileName,
                        const bool         appendMode)
{
   finishLogging();
   if(fileName != nullptr) {
      std::ofstream* newLogFile =
         (appendMode == true) ? new std::ofstream(fileName, std::ios_base::app) :
                                new std::ofstream(fileName);
      if(newLogFile != nullptr) {
         if(newLogFile->good()) {
            gStdLog = newLogFile;
            gCloseStdLog = true;
            gLogLevel    = std::max(logLevel, MIN_LOGLEVEL);
            return true;
         }
         delete newLogFile;
      }
   }
   return false;
}


// ###### Initialise logging ################################################
bool initLogging(const char* parameter)
{
   if(!(strncmp(parameter, "-logfile=", 9))) {
      return(initLogFile(gLogLevel, (char*)&parameter[9], false));
   }
   else if(!(strncmp(parameter, "-logappend=", 11))) {
      return(initLogFile(gLogLevel, (char*)&parameter[11], true));
   }
   else if(!(strcmp(parameter, "-logquiet"))) {
      initLogFile(0, nullptr, false);
      gLogLevel = 0;
   }
   else if(!(strncmp(parameter, "-loglevel=", 10))) {
      gLogLevel = std::max((unsigned int)atoi((char*)&parameter[10]), MIN_LOGLEVEL);
   }
   else if(!(strncmp(parameter, "-logcolor=", 10))) {
      if(!(strcmp((char*)&parameter[10], "off"))) {
         gColorMode = false;
      }
      else {
         gColorMode = true;
      }
   }
   else {
      std::cerr << format("ERROR: Invalid logging parameter %s", parameter) << "\n";
      return false;
   }
   return true;
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

   LOG_INFO
   *gStdLog << format("Logging started, log level is %d.", gLogLevel)
            << "\n";
   LOG_END

   gLogMutex.unlock();
}


// ###### Finish logging ####################################################
void finishLogging()
{
   if((gStdLog) && (gCloseStdLog)) {
      LOG_INFO
      *gStdLog << "Logging finished.\n";
      LOG_END
      delete gStdLog;
      gStdLog      = &std::cerr;
      gCloseStdLog = false;
   }
}
