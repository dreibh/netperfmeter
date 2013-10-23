/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2013 by Thomas Dreibholz
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
 * You should have relReceived a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact: dreibh@iem.uni-due.de
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <iostream>

#include "defragmenter.h"


using namespace std;

Defragmenter gDefrag;


void add(unsigned long long now,
         uint32_t frameID,
         uint64_t seqNum,
         uint64_t byteSeq,
         uint8_t flags = 0)
{
   NetPerfMeterDataMessage d;
   d.Header.Flags = flags;
   d.Header.Length = htons(1000);
   d.FrameID = htonl(frameID);
   d.SeqNumber = hton64(seqNum);
   d.ByteSeqNumber = hton64(byteSeq);
   gDefrag.addFragment(now, &d);
}



int main(int argc, char** argv)
{
   size_t b,p,f,rf;

   add(1000000,   0,    0,   0, NPMDF_FRAME_BEGIN);
   add(1000001,   0,    1,   1000);
   add(1000002,   0,    2,   2000, NPMDF_FRAME_END);

   add(1000002,   1,    3,   4000, NPMDF_FRAME_BEGIN);
   add(1000002,   0,    5,   4000, NPMDF_FRAME_BEGIN);

   add(1000003,   2,    8,   8000, NPMDF_FRAME_END);

   add(4000003,   6,   16,   12000, NPMDF_FRAME_BEGIN);

   gDefrag.print(cout);
   gDefrag.purge(5000000, 1000000, rf, f, p, b);
   printf("=> rf=%d   f=%d, p=%d, b=%d\n", (int)rf, (int)b, (int)p, (int)f);
   gDefrag.print(cout);
}
