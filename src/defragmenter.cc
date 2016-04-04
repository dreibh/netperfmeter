/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2009-2016 by Thomas Dreibholz
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

#include "defragmenter.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <map>


// ###### Constructor #################b######################################
Defragmenter::Defragmenter()
{
   NextFrameID         = 0;
   NextPacketSeqNumber = 0;
   NextByteSeqNumber   = 0;
}


// ###### Destructor ########################################################
Defragmenter::~Defragmenter()
{
   std::map<uint32_t, Frame*>::iterator frameIterator = FrameSet.begin();
   while(frameIterator != FrameSet.end()) {
      Frame* frame = frameIterator->second;

      std::map<uint64_t, Fragment*>::iterator fragmentIterator = frame->FragmentSet.begin();
      while(fragmentIterator != frame->FragmentSet.end()) {
         Fragment* fragment = fragmentIterator->second;
         frame->FragmentSet.erase(fragmentIterator);
         delete fragment;
         fragmentIterator = frame->FragmentSet.begin();
      }

      FrameSet.erase(frameIterator);
      delete frame;
      frameIterator = FrameSet.begin();
   }
}


// ###### Print FragmentSet #################################################
void Defragmenter::print(std::ostream& os)
{
   os << "FragmentSet:" << std::endl;
   for(std::map<uint32_t, Frame*>::iterator frameIterator = FrameSet.begin();
         frameIterator != FrameSet.end(); frameIterator++) {
      Frame* frame = frameIterator->second;
      os << "   - Frame " << frame->FrameID
         << ", LastUpdate=" << frame->LastUpdate
         << ":" << std::endl;

      for(std::map<uint64_t, Fragment*>::iterator fragmentIterator = frame->FragmentSet.begin();
          fragmentIterator != frame->FragmentSet.end(); fragmentIterator++) {
         Fragment* fragment = fragmentIterator->second;
         os << "      + Fragment " << fragment->PacketSeqNumber << ":\t"
            << "ByteSeq=" << fragment->ByteSeqNumber
            << ", Length=" << fragment->Length << "   ";
         if(fragment->Flags & NPMDF_FRAME_BEGIN) {
            os << "<Begin> ";
         }
         if(fragment->Flags & NPMDF_FRAME_END) {
            os << "<End> ";
         }
         os << std::endl;
      }
   }
}


// ###### Add fragment to Defragmenter ######################################
void Defragmenter::addFragment(const unsigned long long       now,
                               const NetPerfMeterDataMessage* dataMsg)
{
   const uint32_t frameID = ntohl(dataMsg->FrameID);

   // ====== Find frame =====================================================
   Frame*                               frame;
   std::map<uint32_t, Frame*>::iterator foundFrame = FrameSet.find(frameID);
   if(foundFrame != FrameSet.end()) {
      frame = foundFrame->second;
   }
   else {
      // ====== Frame has to be created =====================================
      frame = new Frame;
      if(frame) {
         frame->LastUpdate = now;
         frame->FrameID    = frameID;
         frame->Completed  = false;
         FrameSet.insert(std::pair<uint32_t, Frame*>(frame->FrameID, frame));
      }
   }

   // ====== Add fragment ===================================================
   const uint64_t packetSeqNumber = ntoh64(dataMsg->SeqNumber);
   std::map<uint64_t, Fragment*>::iterator foundFragment =
      frame->FragmentSet.find(packetSeqNumber);
   if(foundFragment == frame->FragmentSet.end()) {
      Fragment* fragment = new Fragment;
      if(fragment) {
         fragment->PacketSeqNumber = packetSeqNumber;
         fragment->ByteSeqNumber   = ntoh64(dataMsg->ByteSeqNumber);
         fragment->Length          = ntohs(dataMsg->Header.Length);
         fragment->Flags           = dataMsg->Header.Flags;
         frame->FragmentSet.insert(std::pair<uint64_t, Fragment*>(fragment->PacketSeqNumber, fragment));
      }
   }
   else {
      // puts("Duplicate???");
   }
}


// ====== Get first fragment from storage ===================================
bool Defragmenter::getFirstFragment(Frame*&    frame,
                                    Fragment*& fragment)
{
   FrameIterator = FrameSet.begin();
   if(FrameIterator != FrameSet.end()) {
      frame            = FrameIterator->second;
      FragmentIterator = frame->FragmentSet.begin();
      assert(FragmentIterator != frame->FragmentSet.end());
      fragment = FragmentIterator->second;
      return(true);
   }
   frame    = NULL;
   fragment = NULL;
   return(false);
}


// ====== Get next fragment from storage ====================================
bool Defragmenter::getNextFragment(Frame*&    frame,
                                   Fragment*& fragment,
                                   const bool eraseCurrentFrame)
{
   Frame*                                  lastFrame            = frame;
   Fragment*                               lastFragment         = fragment;
   std::map<uint32_t, Frame*>::iterator    lastFrameIterator    = FrameIterator;
   std::map<uint64_t, Fragment*>::iterator lastFragmentIterator = FragmentIterator;

   FragmentIterator++;
   if(FragmentIterator == frame->FragmentSet.end()) {
      FrameIterator++;
      if(FrameIterator == FrameSet.end()) {
         frame    = NULL;
         fragment = NULL;
      }
      else {
         frame            = FrameIterator->second;
         FragmentIterator = frame->FragmentSet.begin();
         assert(FragmentIterator != frame->FragmentSet.end());
         fragment = FragmentIterator->second;
      }
   }
   else {
      fragment = FragmentIterator->second;
   }

   if(eraseCurrentFrame) {
      lastFrame->FragmentSet.erase(lastFragmentIterator);
      delete lastFragment;
      if(lastFrame->FragmentSet.size() == 0) {
         FrameSet.erase(lastFrameIterator);
         delete lastFrame;
      }
   }

   return(fragment != NULL);
}


// ###### Purge incomplete frames from Defragmenter #########################
void Defragmenter::purge(const unsigned long long now,
                         const unsigned long long defragmentTimeout,
                         size_t&                  receivedFrames,
                         size_t&                  lostFrames,
                         size_t&                  lostPackets,
                         size_t&                  lostBytes)
{
   receivedFrames = 0;
   lostBytes      = 0;
   lostPackets    = 0;
   lostFrames     = 0;

   Frame*    frame;
   Fragment* fragment;
   if(getFirstFragment(frame, fragment)) {
      bool eraseCurrentFragment;
      do {
         eraseCurrentFragment = false;
         if(frame->LastUpdate + defragmentTimeout <= now) {
            eraseCurrentFragment = true;

            if(frame->FrameID >= NextFrameID) {
               receivedFrames++;
               lostFrames += ((long long)frame->FrameID - (long long)NextFrameID);
               NextFrameID = frame->FrameID + 1;
            }
            if(fragment->ByteSeqNumber >= NextByteSeqNumber) {
               lostBytes += ((long long)fragment->ByteSeqNumber - (long long)NextByteSeqNumber);
               NextByteSeqNumber = fragment->ByteSeqNumber + fragment->Length;
            }
            if(fragment->PacketSeqNumber >= NextPacketSeqNumber) {
               lostPackets += ((long long)fragment->PacketSeqNumber - (long long)NextPacketSeqNumber);
               NextPacketSeqNumber = fragment->PacketSeqNumber + 1;
            }
         }
         else {
            break;
         }
      } while(getNextFragment(frame, fragment, eraseCurrentFragment));
   }
}
