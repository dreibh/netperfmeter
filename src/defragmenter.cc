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
 *
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

#include "assure.h"
#include "defragmenter.h"
#include "tools.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>


// ###### Constructor #######################################################
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
   os << "FragmentSet:" << "\n";
   for(std::map<uint32_t, Frame*>::iterator frameIterator = FrameSet.begin();
       frameIterator != FrameSet.end(); frameIterator++) {
      Frame* frame = frameIterator->second;
      os << formatText("   - Frame {}, LastUpdate={}:",
                       frame->FrameID, frame->LastUpdate) << "\n";

      for(std::map<uint64_t, Fragment*>::iterator fragmentIterator = frame->FragmentSet.begin();
          fragmentIterator != frame->FragmentSet.end(); fragmentIterator++) {
         Fragment* fragment = fragmentIterator->second;
         os << formatText("      + Fragment {}:\tByteSeq={}, Length={}",
                          fragment->PacketSeqNumber, fragment->ByteSeqNumber, fragment->Length) << "   ";
         if(fragment->Flags & NPMDF_FRAME_BEGIN) {
            os << "<Begin> ";
         }
         if(fragment->Flags & NPMDF_FRAME_END) {
            os << "<End> ";
         }
         os << "\n";
      }
   }
}


// ###### Add fragment to Defragmenter ######################################
void Defragmenter::addFragment(const uint64_t                 now,
                               const NetPerfMeterDataMessage* dataMsg)
{
   const uint32_t frameID = be32toh(dataMsg->FrameID);

   // ====== Find frame =====================================================
   Frame*                               frame;
   std::map<uint32_t, Frame*>::iterator foundFrame = FrameSet.find(frameID);
   if(foundFrame != FrameSet.end()) {
      frame = foundFrame->second;
      frame->LastUpdate = now;
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
   const uint64_t packetSeqNumber = be64toh(dataMsg->SeqNumber);
   std::map<uint64_t, Fragment*>::iterator foundFragment =
      frame->FragmentSet.find(packetSeqNumber);
   if(foundFragment == frame->FragmentSet.end()) {
      Fragment* fragment = new Fragment;
      if(fragment) {
         fragment->PacketSeqNumber = packetSeqNumber;
         fragment->ByteSeqNumber   = be64toh(dataMsg->ByteSeqNumber);
         fragment->Length          = be16toh(dataMsg->Header.Length);
         fragment->Flags           = dataMsg->Header.Flags;
         frame->FragmentSet.insert(std::pair<uint64_t, Fragment*>(fragment->PacketSeqNumber, fragment));
      }
   }

   // ====== Check whether frame is now complete ============================
   if(!frame->FragmentSet.empty()) {
      const Fragment* firstFrag = frame->FragmentSet.begin()->second;
      const Fragment* lastFrag  = frame->FragmentSet.rbegin()->second;

      if((firstFrag->Flags & NPMDF_FRAME_BEGIN) && (lastFrag->Flags & NPMDF_FRAME_END)) {
         bool contiguous = true;
         auto it = frame->FragmentSet.begin();
         auto prev = it->second;
         it++;
         while(it != frame->FragmentSet.end()) {
            auto curr = it->second;
            if((prev->PacketSeqNumber + 1 != curr->PacketSeqNumber) ||
               (prev->ByteSeqNumber + prev->Length != curr->ByteSeqNumber)) {
               contiguous = false;
               break;
            }
            prev = curr;
            it++;
         }
         frame->Completed = contiguous;
      }
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
      assure(FragmentIterator != frame->FragmentSet.end());
      fragment = FragmentIterator->second;
      return true;
   }
   frame    = nullptr;
   fragment = nullptr;
   return false;
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
         frame    = nullptr;
         fragment = nullptr;
      }
      else {
         frame            = FrameIterator->second;
         FragmentIterator = frame->FragmentSet.begin();
         assure(FragmentIterator != frame->FragmentSet.end());
         fragment = FragmentIterator->second;
      }
   }
   else {
      fragment = FragmentIterator->second;
   }

   if(eraseCurrentFrame) {
      lastFrame->FragmentSet.erase(lastFragmentIterator);
      delete lastFragment;
      if(lastFrame->FragmentSet.empty()) {
         FrameSet.erase(lastFrameIterator);
         delete lastFrame;
         if(lastFrame == frame) {   // Ensure reference gets invalidated!
            frame = nullptr;
         }
      }
      if(lastFragment == fragment) {   // Ensure reference gets invalidated!
         fragment = nullptr;
      }
   }

   return ((frame != nullptr) && (fragment != nullptr));
}


// ###### Purge incomplete frames from Defragmenter #########################
void Defragmenter::purge(const uint64_t now,
                         const uint64_t defragmentTimeout,
                         size_t&        receivedFrames,
                         size_t&        lostFrames,
                         size_t&        lostPackets,
                         size_t&        lostBytes)
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
         const bool canReleaseComplete = (frame->Completed && (frame->FrameID <= NextFrameID));
         const bool isTimedOut         = (frame->LastUpdate + defragmentTimeout <= now);

         if(canReleaseComplete || isTimedOut) {
            eraseCurrentFragment = true;

            if(frame->FrameID >= NextFrameID) {
               if(frame->Completed) {
                  receivedFrames++;
               }
               else {
                  lostFrames++; // Incomplete frame timed out
               }
               lostFrames += (static_cast<uint64_t>(frame->FrameID) - static_cast<uint64_t>(NextFrameID));
               NextFrameID = frame->FrameID + 1;
            }
            if(fragment->ByteSeqNumber >= NextByteSeqNumber) {
               lostBytes += (fragment->ByteSeqNumber - NextByteSeqNumber);
               NextByteSeqNumber = fragment->ByteSeqNumber + fragment->Length;
            }
            if(fragment->PacketSeqNumber >= NextPacketSeqNumber) {
               lostPackets += (fragment->PacketSeqNumber - NextPacketSeqNumber);
               NextPacketSeqNumber = fragment->PacketSeqNumber + 1;
            }
         }
         else {
            break;
         }
      } while(getNextFragment(frame, fragment, eraseCurrentFragment));
   }
}
