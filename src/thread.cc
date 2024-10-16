/*
 * ==========================================================================
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
 * Contact:  thomas.dreibholz@gmail.com
 * Homepage: https://www.nntb.no/~dreibh/netperfmeter/
 */

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>

#include "thread.h"


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
   return(NULL);
}


// ###### Start thread ######################################################
bool Thread::start()
{
   if(MyThread == 0) {
      lock();
      Stopping = false;
      unlock();
      if(pthread_create(&MyThread, NULL, startRoutine, (void*)this) == 0) {
         return(true);
      }
      MyThread = 0;
      std::cerr << "ERROR: Unable to start new thread!\n";
   }
   else {
      std::cerr << "ERROR: Thread already running!\n";
   }
   return(false);
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
      pthread_join(MyThread, NULL);
      MyThread = 0;
   }
}


// ###### Wait a given amount of microseconds ###############################
void Thread::delay(const unsigned int us)
{
   usleep(us);
}
