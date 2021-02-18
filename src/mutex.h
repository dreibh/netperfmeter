/*
 * ==========================================================================
 *                  NetPerfMeter -- Network Performance Meter                 
 *                 Copyright (C) 2009-2021 by Thomas Dreibholz
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
 * Contact:  dreibh@iem.uni-due.de
 * Homepage: https://www.uni-due.de/~be0001/netperfmeter/
 */

#ifndef MUTEX_H
#define MUTEX_H

#include <assert.h>
#include <pthread.h>


class Mutex
{
   public:
   Mutex();
   virtual ~Mutex();

   inline void lock() {
#ifndef __APPLE__
      pthread_mutex_lock(&MyMutex);
#else
      if(!pthread_equal(MutexOwner, pthread_self())) {
         pthread_mutex_lock(&MyMutex);
         MutexOwner = pthread_self();
      }
      MutexRecursionLevel++;
#endif
   }

   inline void unlock() {
#ifndef __APPLE__
      pthread_mutex_unlock(&MyMutex);
#else
      assert(MutexRecursionLevel != 0);
      if(pthread_equal(MutexOwner, pthread_self())) {
         MutexRecursionLevel--;
         if(MutexRecursionLevel == 0) {
            MutexOwner = 0;
            pthread_mutex_unlock(&MyMutex);
         }
      }
#endif
   }

   private:
   pthread_mutex_t MyMutex;
#ifdef __APPLE__
   pthread_t       MutexOwner;
   unsigned int    MutexRecursionLevel;
#endif
};

#endif
