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

#ifndef LOGLEVEL_H
#define LOGLEVEL_H

#include "mutex.h"
#include "tools.h"


#define LOGLEVEL_FATAL   5U
#define LOGLEVEL_ERROR   4U
#define LOGLEVEL_WARNING 3U
#define LOGLEVEL_INFO    2U
#define LOGLEVEL_DEBUG   1U
#define LOGLEVEL_TRACE   0U
#define MAX_LOGLEVEL     LOGLEVEL_FATAL
#define MIN_LOGLEVEL     LOGLEVEL_TRACE


extern std::ostream* gStdLog;
extern unsigned int  gLogLevel;
extern bool          gColorMode;
extern bool          gCloseStdLog;
extern char          gHostName[256];
extern Mutex         gLogMutex;

#define stdlog (*gStdLog)

#define LOG_BEGIN(ansiColorSeq)   \
   {                              \
      loggingMutexLock();         \
      if(gColorMode) {            \
        *gStdLog << ansiColorSeq; \
      }                           \
      printTimeStamp(*gStdLog);

#define LOG_END                   \
      if(gColorMode) {            \
        *gStdLog << "\e[0m";      \
      }                           \
      gStdLog->flush();           \
      loggingMutexUnlock();       \
   }

#define LOG_END_FATAL                          \
      *gStdLog << "FATAL ERROR - ABORTING!\n"; \
      if(gColorMode) {                         \
        *gStdLog << "\e[0m\e[2K";              \
      }                                        \
      gStdLog->flush();                        \
      loggingMutexUnlock();                    \
   }                                           \
   exit(1);


#define LOG_FATAL   if((LOGLEVEL_FATAL   >= MIN_LOGLEVEL) && (gLogLevel <= LOGLEVEL_FATAL))   LOG_BEGIN("\e[37;41;1m")
#define LOG_ERROR   if((LOGLEVEL_ERROR   >= MIN_LOGLEVEL) && (gLogLevel <= LOGLEVEL_ERROR))   LOG_BEGIN("\e[31;1m")
#define LOG_WARNING if((LOGLEVEL_WARNING >= MIN_LOGLEVEL) && (gLogLevel <= LOGLEVEL_WARNING)) LOG_BEGIN("\e[33m")
#define LOG_INFO    if((LOGLEVEL_INFO    >= MIN_LOGLEVEL) && (gLogLevel <= LOGLEVEL_INFO))    LOG_BEGIN("\e[34m")
#define LOG_DEBUG   if((LOGLEVEL_DEBUG   >= MIN_LOGLEVEL) && (gLogLevel <= LOGLEVEL_DEBUG))   LOG_BEGIN("\e[36m")
#define LOG_TRACE   if((LOGLEVEL_TRACE   >= MIN_LOGLEVEL) && (gLogLevel <= LOGLEVEL_TRACE))   LOG_BEGIN("\e[37m")


bool initLogFile(const unsigned int logLevel,
                 const char*        fileName,
                 const bool         appendMode);
void beginLogging();
void finishLogging();

inline void loggingMutexLock() {
   gLogMutex.lock();
}

inline void loggingMutexUnlock() {
   gLogMutex.unlock();
}

inline const char* getHostName()
{
   return (const char*)gHostName;
}

#endif
