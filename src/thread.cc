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

#include "thread.h"
#include "loglevel.h"

#include <pthread.h>


// ###### Constructor #######################################################
Thread::Thread()
{
   MyThread = 0;
   lock();
   Stopping = false;
   unlock();
}


// ###### Destructor ########################################################
Thread::~Thread()
{
   if(MyThread != 0) {
      waitForFinish();
   }
}


// ###### Start routine to lauch thread's run() function ####################
void* Thread::startRoutine(void* object)
{
   Thread* thread = (Thread*)object;
   thread->run();
   return nullptr;
}


// ###### Start thread ######################################################
bool Thread::start()
{
   if(MyThread == 0) {
      lock();
      Stopping = false;
      unlock();
      if(pthread_create(&MyThread, nullptr, startRoutine, (void*)this) == 0) {
         return true;
      }
      MyThread = 0;
      LOG_ERROR
      stdlog << "Unable to start new thread!" << "\n";
      LOG_END
   }
   else {
      LOG_ERROR
      stdlog << "Thread is already running!" << "\n";
      LOG_END
   }
   return false;
}


// ###### Stop thread #######################################################
void Thread::stop()
{
   lock();
   Stopping = true;
   unlock();
}


// ###### Wait until thread has been finished ###############################
void Thread::waitForFinish()
{
   if(MyThread != 0) {
      pthread_join(MyThread, nullptr);
      MyThread = 0;
   }
}


// ###### Wait a given amount of microseconds ###############################
void Thread::delay(const unsigned int us)
{
   usleep(us);
}
