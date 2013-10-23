/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2009-2013 by Thomas Dreibholz
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

#ifndef DEFRAGMENTER_H
#define DEFRAGMENTER_H

#include <map>

#include "netperfmeterpackets.h"


class Defragmenter
{
   // ====== Public Methods =================================================
   public:
   Defragmenter();
   ~Defragmenter();

   void print(std::ostream& os);

   void addFragment(const unsigned long long       now,
                    const NetPerfMeterDataMessage* dataMsg);
   void purge(const unsigned long long now,
              const unsigned long long defragmentTimeout,
              size_t&                  receivedFrames,
              size_t&                  lostFrames,
              size_t&                  lostPackets,
              size_t&                  lostBytes);


   // ====== Private Data ===================================================
   private:
   struct Fragment
   {
      uint64_t PacketSeqNumber;
      uint64_t ByteSeqNumber;
      uint16_t Length;
      uint8_t  Flags;
   };
   struct Frame
   {
      uint32_t                      FrameID;
      unsigned long long            LastUpdate;
      std::map<uint64_t, Fragment*> FragmentSet;
      bool                          Completed;
   };

   bool getFirstFragment(Frame*&    frame,
                         Fragment*& fragment);
   bool getNextFragment(Frame*&    frame,
                        Fragment*& fragment,
                        const bool eraseCurrentFrame);

   std::map<uint32_t, Frame*>              FrameSet;
   std::map<uint32_t, Frame*>::iterator    FrameIterator;
   std::map<uint64_t, Fragment*>::iterator FragmentIterator;
   uint64_t                                NextPacketSeqNumber;
   uint64_t                                NextByteSeqNumber;
   uint32_t                                NextFrameID;
};

#endif
