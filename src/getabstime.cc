/*
 * ==========================================================================
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2018 by Thomas Dreibholz
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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>


// ###### Get current time stamp ############################################
unsigned long long getMicroTime()
{
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return(((unsigned long long)tv.tv_sec * (unsigned long long)1000000) +
         (unsigned long long)tv.tv_usec);
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   printf("%llu\n", getMicroTime());
   return 0;
}
