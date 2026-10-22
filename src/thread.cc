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

#include "thread.h"
#include "loglevel.h"
#include "tools.h"

#include <system_error>


// ###### Constructor #######################################################
Thread::Thread()
   : Stopping(false)
{
}


// ###### Destructor ########################################################
Thread::~Thread()
{
   if(MyThread.joinable()) {
      waitForFinish();
   }
}


// ###### Start thread ######################################################
bool Thread::start()
{
   if(!MyThread.joinable()) {
      lock();
      Stopping.store(false);
      unlock();
      try {
         MyThread = std::thread(&Thread::run, this);
         return true;
      }
      catch(const std::system_error& e) {
         LOG_ERROR
         // TRANSLATORS: Error message when starting a new thread fails. {} is the system error description.
         stdlog << formatText(gettext("Unable to start new thread: {}!"), e.what()) << "\n";
         LOG_END
      }
   }
   else {
      LOG_ERROR
      // TRANSLATORS: Error message when trying to start an already running thread.
      stdlog << gettext("Thread is already running!") << "\n";
      LOG_END
   }
   return false;
}


// ###### Stop thread #######################################################
void Thread::stop()
{
   lock();
   Stopping.store(true);
   unlock();
}


// ###### Wait until thread has been finished ###############################
void Thread::waitForFinish()
{
   if(MyThread.joinable()) {
      MyThread.join();
   }
}


// ###### Wait a given amount of microseconds ###############################
void Thread::delay(const uint32_t us)
{
   std::this_thread::sleep_for(std::chrono::microseconds(us));
}


// ###### Wait a given chrono duration ######################################
void Thread::delay(const std::chrono::microseconds duration)
{
   std::this_thread::sleep_for(duration);
}
