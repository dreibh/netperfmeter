#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <iostream>
#include <map>

#include "netperfmeterpackets.h"


unsigned int         gQuietMode       = 0;



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
              size_t&                  lostBytes,
              size_t&                  lostPackets,
              size_t&                  lostFrames);


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

//    bool checkFrame(Frame* frame);
//    void deleteObsoleteFragments(const unsigned long long now,
//                                 const unsigned long long defragmentTimeout);

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


// ###### Constructor #################b######################################
Defragmenter::Defragmenter()
{
   NextFrameID = 0;
}


// ###### Destructor ########################################################
Defragmenter::~Defragmenter()
{
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


// // ###### Check whether a frame is complete #################################
// bool Defragmenter::checkFrame(Frame* frame)
// {
//    bool beginFound = false;
//    bool endFound   = false;
//    for(std::map<uint64_t, Fragment*>::iterator fragmentIterator = frame->FragmentSet.begin();
//        fragmentIterator != frame->FragmentSet.end(); fragmentIterator++) {
//       const Fragment* fragment = fragmentIterator->second;
//       if(fragment->Flags & NPMDF_FRAME_BEGIN) {
//          if(beginFound) {
//             std::cout << "Malformed frame " << frame->FrameID
//                       << ": multiple NPMDF_FRAME_BEGIN!" << std::endl;
//             return(false);
//          }
//          beginFound = false;
//       }
//       if(fragment->Flags & NPMDF_FRAME_END) {
//          if(endFound) {
//             std::cout << "Malformed frame " << frame->FrameID
//                       << ": multiple NPMDF_FRAME_END!" << std::endl;
//             return(false);
//          }
//          endFound = false;
//       }
//    }
//    return(beginFound && endFound);
// }


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
         fragment->Length          = ntohs(dataMsg->Header.Length) - sizeof(NetPerfMeterDataMessage);
         fragment->Flags           = dataMsg->Header.Flags;
         frame->FragmentSet.insert(std::pair<uint64_t, Fragment*>(fragment->PacketSeqNumber, fragment));
      }      
// ?????      checkFrame(frame);
   }
   else {
      puts("Duplicate???");
   }
}


// // ====== Delete completed or out of date frames ============================
// void Defragmenter::deleteObsoleteFragments(const unsigned long long now,
//                                            const unsigned long long defragmentTimeout)
// {
//    std::map<uint32_t, Frame*>::iterator frameIterator = FrameSet.begin();
//    while(frameIterator != FrameSet.end()) {
//       Frame* frame = frameIterator->second;
//       if((frame->Completed == false) &&
//          (frame->LastUpdate <= now + defragmentTimeout)) {
//          break;
//       }
//       FrameSet.erase(frameIterator);
//       delete frame;
//       frameIterator = FrameSet.begin();
//    }
// }


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
         return(false);
      }
      frame            = FrameIterator->second;
      FragmentIterator = frame->FragmentSet.begin();
      assert(FragmentIterator != frame->FragmentSet.end());
   }
   fragment = FragmentIterator->second;   

   if(eraseCurrentFrame) {
      lastFrame->FragmentSet.erase(lastFragmentIterator);
      delete lastFragment;
      if(lastFrame->FragmentSet.size() == 0) {
         FrameSet.erase(lastFrameIterator);
         delete lastFrame;
      }
   }
   
   return(true);
}


// ###### Purge incomplete frames from Defragmenter #########################
void Defragmenter::purge(const unsigned long long now,
                         const unsigned long long defragmentTimeout,
                         size_t&                  lostBytes,
                         size_t&                  lostPackets,
                         size_t&                  lostFrames)
{
   lostBytes   = 0;
   lostPackets = 0;
   lostFrames  = 0;
   
   Frame*    frame;
   Fragment* fragment;
   if(getFirstFragment(frame, fragment)) {
      bool eraseCurrentFragment;
      do {
         eraseCurrentFragment = false;  
         if(frame->LastUpdate + defragmentTimeout <= now) {
            eraseCurrentFragment = true;
            puts("+++");
         }
         else {
          printf("ST: id=%u %llu %llu %llu\n",frame->FrameID,frame->LastUpdate,defragmentTimeout,now);
            break;
         }
      } while(getNextFragment(frame, fragment, eraseCurrentFragment));
   }

#if 0
   Frame*                                   frame;
   for(std::map<uint32_t, Frame*>::iterator frameIterator = FrameSet.begin();
       frameIterator != FrameSet.end(); frameIterator++) {
      Frame* frame = frameIterator->second;
      if(frame->FrameID != NextFrameID) {
         puts("O-O-SYNC");
         goto finished;
      }

      size_t   n                   = 0;
      bool     hasBegin            = false;
      bool     hasEnd              = false;
      uint64_t nextByteSeqNumber   = 0;
      uint64_t nextPacketSeqNumber = 0;
      for(std::map<uint64_t, Fragment*>::iterator fragmentIterator = frame->FragmentSet.begin();
          fragmentIterator != frame->FragmentSet.end(); fragmentIterator++, n++) {
         Fragment* fragment = fragmentIterator->second;
      
         if( (n == 0) && (fragment->Flags & NPMDF_FRAME_BEGIN) ) {
            hasBegin            = true;
            nextByteSeqNumber   = fragment->PacketSeqNumber + fragment->Length + 1;
            nextPacketSeqNumber = fragment->PacketSeqNumber + 1;
         }
         else {
            bool isInSequence = false;
            if(fragment->PacketSeqNumber == nextPacketSeqNumber) {
               nextPacketSeqNumber++;
               isInSequence = true;
            }
            if(fragment->ByteSeqNumber == nextByteSeqNumber) {
               nextByteSeqNumber +=fragment->Length + 1;
               // PacketSeqNumber *must* be okay here!
            }
            if(!isInSequence) {
               puts("OUT-OF-SEQ");
               goto finished;
            }
         }
         if(fragment->Flags & NPMDF_FRAME_END) {
            hasEnd = true;
         }
      }
      
      NextPacketSeqNumber = nextPacketSeqNumber;
      NextByteSeqNumber   = nextByteSeqNumber;
      NextFrameID         = frame->FrameID + 1;
      frame->Completed    = true;
      printf("FULL FRAME %d\n", frame->FrameID);
   }
   
finished:
   puts("Ende!");
   deleteObsoleteFragments(now, defragmentTimeout);
#endif
}



using namespace std;

Defragmenter gDefrag;


void add(unsigned long long now,
         uint32_t frameID, uint64_t seqNum, uint64_t byteSeq, uint8_t flags = 0)
{
   NetPerfMeterDataMessage d;
   d.Header.Flags = flags;
   d.Header.Length = htons(1000 + sizeof(d));
   d.FrameID = htonl(frameID);
   d.SeqNumber = hton64(seqNum);
   d.ByteSeqNumber = hton64(byteSeq);
   gDefrag.addFragment(now, &d);
}



int main(int argc, char** argv)
{
   size_t b,p,f;

   add(1000000,   0,    0,   0, NPMDF_FRAME_BEGIN);
   add(1000001,   0,    1,   1000);
   add(1000002,   0,    2,   2000, NPMDF_FRAME_END);

   add(1000002,   1,    3,   4000, NPMDF_FRAME_BEGIN);

   add(1000003,   2,    8,   8000, NPMDF_FRAME_END);
   
   gDefrag.print(cout);
   gDefrag.purge(5000000, 1000000, b, p, f);
   printf("=> b=%d, p=%d, f=%d\n", (int)b, (int)p, (int)f);
}
