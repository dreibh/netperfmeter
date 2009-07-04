#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <map>

#include "netperfmeterpackets.h"


/*
char str[128];
if(name == NULL) {
   static int yyy=1000;
   name = (char*)&str;
   sprintf((char*)&str, "FILE-%03d", yyy++);
   puts("HACK!!!???");
   Name = std::string(name);
}
*/


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


class Defragmenter
{
   public:
   Defragmenter();
   ~Defragmenter();
   
   void print(std::ostream& os);

   void addFragment(const unsigned long long       now,
                    const NetPerfMeterDataMessage* dataMsg);
   void purge(const unsigned long long now,
              const unsigned long long defragmentTimeout,
              unsigned long long&      lostBytes,
              unsigned long long&      lostPackets,
              unsigned long long&      lostFrames);
   
   private:
   bool checkFrame(Frame* frame);
   
   std::map<uint32_t, Frame*> FrameSet;
};


Defragmenter::Defragmenter()
{
}


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
            << ", Length=" << fragment->Length << std::endl;
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
      if(fragment->Flags & DHF_FRAME_BEGIN) {
         if(beginFound) {
            std::cout << "Malformed frame " << frame->FrameID
                      << ": multiple DHF_FRAME_BEGIN!" << std::endl;
            return(false);
         }
         beginFound = false;
      }
      if(fragment->Flags & DHF_FRAME_END) {
         if(endFound) {
            std::cout << "Malformed frame " << frame->FrameID
                      << ": multiple DHF_FRAME_END!" << std::endl;
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
                         unsigned long long&      lostBytes,
                         unsigned long long&      lostPackets,
                         unsigned long long&      lostFrames)
{
}



using namespace std;

int main(int argc, char** argv)
{
}
