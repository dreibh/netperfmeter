#include <stdio.h>
#include <stdlib.h>
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
   };

   bool checkFrame(Frame* frame);
   
   std::map<uint32_t, Frame*> FrameSet;
   uint64_t                   NextPacketSeqNumber;
   uint64_t                   NextByteSeqNumber;
   uint32_t                   NextFrameID;
   bool                       Synchronized;
};


// ###### Constructor #######################################################
Defragmenter::Defragmenter()
{
   Synchronized = false;
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
      os << "   - Frame " << frame->FrameID << ":" << std::endl;
      
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


// ###### Check whether a frame is complete #################################
bool Defragmenter::checkFrame(Frame* frame)
{
   bool beginFound = false;
   bool endFound   = false;
   for(std::map<uint64_t, Fragment*>::iterator fragmentIterator = frame->FragmentSet.begin();
       fragmentIterator != frame->FragmentSet.end(); fragmentIterator++) {
      const Fragment* fragment = fragmentIterator->second;
      if(fragment->Flags & NPMDF_FRAME_BEGIN) {
         if(beginFound) {
            std::cout << "Malformed frame " << frame->FrameID
                      << ": multiple NPMDF_FRAME_BEGIN!" << std::endl;
            return(false);
         }
         beginFound = false;
      }
      if(fragment->Flags & NPMDF_FRAME_END) {
         if(endFound) {
            std::cout << "Malformed frame " << frame->FrameID
                      << ": multiple NPMDF_FRAME_END!" << std::endl;
            return(false);
         }
         endFound = false;
      }
   }
   return(beginFound && endFound);
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
      
      checkFrame(frame);
   }
   else {
      puts("Duplicate???");
   }
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
   
   Frame*                                   frame;
   for(std::map<uint32_t, Frame*>::iterator frameIterator = FrameSet.begin();
       frameIterator != FrameSet.end(); frameIterator++) {
      Frame* frame = frameIterator->second;

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
      NextFrameID         = frame->FrameID;
      Synchronized        = true;
      printf("FULL FRAME %d\n", frame->FrameID);
   }
   
finished:
   puts("Ende!");
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

   add(1000000,   1,    0,   0, NPMDF_FRAME_BEGIN);
   add(1000001,   1,    1,   1000);
   add(1000002,   1,    2,   2000, NPMDF_FRAME_END);

   add(1000002,   2,    4,   4000, NPMDF_FRAME_BEGIN);
   
   gDefrag.print(cout);
   gDefrag.purge(5000000, 1000000, b, p, f);
   printf("=> b=%d, p=%d, f=%d\n", (int)b, (int)p, (int)f);
}
