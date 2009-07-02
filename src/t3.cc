#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <bzlib.h>
#include <ext_socket.h>

#include <assert.h>
#include <stdarg.h>

#include "thread.h"
#include "tools.h"
#include "t3.h"


// ###### Constructor #######################################################
OutputFile::OutputFile()
{
   File   = NULL;
   BZFile = NULL;
}


// ###### Destructor ########################################################
OutputFile::~OutputFile()
{
   finish();
}


// ###### Initialize output file ############################################
bool OutputFile::initialize(const char* name, const bool compressFile)
{
   // ====== Initialize object ==============================================
   finish();
   if(name != NULL) {
      Name = std::string(name);
   }
   else {
      Name = "(anonymous file)";
   }
   Line = 0;

   // ====== Initialize file ================================================
   File = (name != NULL) ? fopen(name, "w+") : tmpfile();
   if(File == NULL) {
      std::cerr << "ERROR: Unable to create output file " << name << "!" << std::endl;
      return(false);
   }

   // ====== Initialize BZip2 compressor ====================================
   if(compressFile) {
      int bzerror;
      BZFile = BZ2_bzWriteOpen(&bzerror, File, 9, 0, 30);
      if(bzerror != BZ_OK) {
         std::cerr << "ERROR: Unable to initialize BZip2 compression on file <"
                   << name << ">!" << std::endl
                   << "Reason: " << BZ2_bzerror(BZFile, &bzerror) << std::endl;
         BZ2_bzWriteClose(&bzerror, BZFile, 0, NULL, NULL);
         fclose(File);
         File = NULL;
         unlink(name);
         return(false);
      }
   }
   return(true);
}


// ###### Finish output file ################################################
bool OutputFile::finish(const bool closeFile)
{
   // ====== Finish BZip2 compression =======================================
   bool result = true;
   if(BZFile) {
      int bzerror;
      BZ2_bzWriteClose(&bzerror, BZFile, 0, NULL, NULL);
      if(bzerror != BZ_OK) {
         std::cerr << "ERROR: Unable to finish BZip2 compression on file <"
                   << Name << ">!" << std::endl
                   << "Reason: " << BZ2_bzerror(BZFile, &bzerror) << std::endl;
         result = false;
      }
      BZFile = NULL;
   }
   // ====== Close or rewind file ===========================================
   if(File) {
      if(closeFile) {
         fclose(File);
         File = NULL;
      }
      else {
         rewind(File);
      }
   }
   return(result);
}


// ###### Write string into output file #####################################
bool OutputFile::printf(const char* str, ...)
{
   char buffer[16384];
   
   va_list va;
   va_start(va, str);
   int bufferLength = vsnprintf(buffer, sizeof(buffer), str, va);
   buffer[sizeof(buffer) - 1] = 0x00;   // Just to be really sure ...
   va_end(va);
   
   // ====== Compress string and write data =================================
   if(BZFile) {
      int bzerror;
      BZ2_bzWrite(&bzerror, BZFile, (void*)&buffer, bufferLength);
      if(bzerror != BZ_OK) {
         std::cerr << std::endl
                   << "ERROR: libbz2 failed to write into file <"
                   << Name << ">!" << std::endl
                   << "Reason: " << BZ2_bzerror(BZFile, &bzerror) << std::endl;
         return(false);
      }
   }

   // ====== Write string as plain text =====================================
   else if(File) {
      if(fputs(buffer, File) <= 0) {
         std::cerr << "ERROR: Failed to write into file <"
                   << Name << ">!" << std::endl;
         return(false);
      }
   }
   return(true);
}




// ###### Constructor #######################################################
TrafficSpec::TrafficSpec()
{
}


// ###### Destructor ########################################################
TrafficSpec::~TrafficSpec()
{
}


// ###### Show configuration entry (value + random number generator) ########
void TrafficSpec::showEntry(std::ostream& os, const double value, const uint8_t rng)
{
   char str[256];

   const char* rngName = "unknown?!";
   switch(rng) {
      case RANDOM_CONSTANT:
         rngName = "constant";
       break;
      case RANDOM_EXPONENTIAL:
         rngName = "negative exponential";
       break;
   }

   snprintf((char*)&str, sizeof(str), "%1.6lf (%s)", value, rngName);
   os << str;
}


// ###### Print TrafficSpec #################################################
void TrafficSpec::print(std::ostream& os) const
{
   os << "      - Outbound Frame Rate: ";
   showEntry(os, OutboundFrameRate, OutboundFrameRateRng);
   os << std::endl;
   os << "      - Outbound Frame Size: ";
   showEntry(os, OutboundFrameSize, OutboundFrameSizeRng);
   os << std::endl;
   os << "      - Inbound Frame Rate:  ";
   showEntry(os, InboundFrameRate, InboundFrameRateRng);
   os << std::endl;
   os << "      - Inbound Frame Size:  ";
   showEntry(os, InboundFrameSize, InboundFrameSizeRng);
   os << std::endl;
   if(Protocol == IPPROTO_SCTP) {
      char ordered[32];
      char reliable[32];
      snprintf((char*)&ordered, sizeof(ordered), "%1.2f%%", 100.0 * OrderedMode);
      snprintf((char*)&reliable, sizeof(reliable), "%1.2f%%", 100.0 * ReliableMode);
      os << "      - Ordered Mode:        " << ordered  << std::endl;
      os << "      - Reliable Mode:       " << reliable << std::endl;
   }
   os << "      - Start/Stop:          { ";
   if(OnOffEvents.size() > 0) {
      bool start = true;
      for(std::set<unsigned int>::iterator iterator = OnOffEvents.begin();
          iterator != OnOffEvents.end();iterator++) {
         if(start) {
            os << "*" << (*iterator / 1000.0) << " ";
         }
         else {
            os << "~" << (*iterator / 1000.0) << " ";
         }
         start = !start;
      }
   }
   else {
      os << "*0 ";
   }
   os << "}" << std::endl;
}


// ###### Reset TrafficSpec #################################################
void TrafficSpec::reset()
{
   OrderedMode              = 1.0;
   ReliableMode             = 1.0;
   OutboundFrameRate        = 0.0;
   OutboundFrameSize        = 0.0;
   OutboundFrameRateRng     = RANDOM_CONSTANT;
   OutboundFrameSizeRng     = RANDOM_CONSTANT;
   InboundFrameRate         = 0.0;
   InboundFrameSize         = 0.0;
   InboundFrameRateRng      = RANDOM_CONSTANT;
   InboundFrameSizeRng      = RANDOM_CONSTANT;
}





// ###### Constructor #######################################################
FlowBandwidthStats::FlowBandwidthStats()
{
   reset();
}


// ###### Destructor ########################################################
FlowBandwidthStats::~FlowBandwidthStats()
{
}


// ###### Print FlowBandwidthStats ##########################################
void FlowBandwidthStats::print(std::ostream& os,
                               const double  transmissionDuration,
                               const double  receptionDuration) const
{
   const double transmittedBits       = 8 * TransmittedBytes;
   const double transmittedBitRate    = transmittedBits / transmissionDuration;
   const double transmittedBytes      = TransmittedBytes;
   const double transmittedByteRate   = transmittedBytes / transmissionDuration;
   const double transmittedPackets    = TransmittedPackets;
   const double transmittedPacketRate = transmittedPackets / transmissionDuration;
   const double transmittedFrames     = TransmittedFrames;
   const double transmittedFrameRate  = transmittedFrames / transmissionDuration;

   const double receivedBits          = 8 * ReceivedBytes;
   const double receivedBitRate       = receivedBits / receptionDuration;
   const double receivedBytes         = ReceivedBytes;
   const double receivedByteRate      = receivedBytes / receptionDuration;
   const double receivedPackets       = ReceivedPackets;
   const double receivedPacketRate    = receivedPackets / receptionDuration;
   const double receivedFrames        = ReceivedFrames;
   const double receivedFrameRate     = receivedFrames / receptionDuration;

   os << "      - Transmission:        " << std::endl
      << "         * Duration:         " << transmissionDuration << " s" << std::endl
      << "         * Bytes:            " << transmittedBytes << " B\t-> "
                                         << transmittedByteRate << " B/s" << std::endl
      << "         * Bits:             " << transmittedBits << " bit\t-> "
                                         << transmittedBitRate << " bit/s" << std::endl
      << "         * Packets:          " << transmittedPackets << " packets\t-> "
                                         << transmittedPacketRate << " packets/s" << std::endl
      << "         * Frames:           " << transmittedFrames << " frames\t-> "
                                         << transmittedFrameRate << " frames/s" << std::endl;
   os << "      - Reception:           " << std::endl
      << "         * Duration:         " << receptionDuration << "s" << std::endl
      << "         * Bytes:            " << receivedBytes << " B\t-> "
                                         << receivedByteRate << " B/s" << std::endl
      << "         * Bits:             " << receivedBits << " bit\t-> "
                                         << receivedBitRate << " bit/s" << std::endl
      << "         * Packets:          " << receivedPackets << " packets\t-> "
                                         << receivedPacketRate << " packets/s" << std::endl
      << "         * Frames:           " << receivedFrames << " frames\t-> "
                                         << receivedFrameRate << " frames/s" << std::endl;
}


// ###### Reset FlowBandwidthStats ##########################################
void FlowBandwidthStats::reset()
{
   TransmittedBytes   = 0;
   TransmittedPackets = 0;
   TransmittedFrames  = 0;

   ReceivedBytes      = 0;
   ReceivedPackets    = 0;
   ReceivedFrames     = 0;
   
   LostBytes          = 0;
   LostPackets        = 0;
   LostFrames         = 0;
}


FlowManager FlowManager::FlowManagerSingleton;



// ###### Constructor #######################################################
FlowManager::FlowManager()
{
   puts("Start FMgr...");
   start();
}


// ###### Destructor ########################################################
FlowManager::~FlowManager()
{
   
}


// ###### Print all flows ###################################################
void FlowManager::print(std::ostream& os,
                        const bool    printStatistics)
{
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
      const Flow* flow= *iterator;
      flow->print(os, printStatistics);
   }
}


// ###### Add flow ##########################################################
void FlowManager::addFlow(Flow* flow)
{
   lock();
   FlowSet.push_back(flow);
   unlock();
}


// ###### Remove flow #######################################################
void FlowManager::removeFlow(Flow* flow)
{
   lock();
   puts("REMOVE FEHLT!!!");
   exit(1);
//    FlowSet.erase(flow);
   unlock();
}


// ###### Find Flow by Measurement ID/Flow ID/Stream ID #####################
Flow* FlowManager::findFlow(const uint64_t measurementID,
                            const uint32_t flowID,
                            const uint16_t streamID)
{
   Flow* found = NULL;
   
   lock();   // ???????????????????????????
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
      Flow* flow = *iterator;
      if( (flow->MeasurementID == measurementID) &&
          (flow->FlowID == flowID) &&
          (flow->StreamID == streamID) ) {
         found = flow;
         break;
      }
   }
   unlock();
   
   return(found);
}


// ###### Find Flow by socket descriptor and Stream ID ######################
Flow* FlowManager::findFlow(const int      socketDescriptor,
                            const uint16_t streamID)
{
   Flow* found = NULL;

   lock();
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
      Flow* flow = *iterator;
      if(flow->SocketDescriptor == socketDescriptor) {
         if(flow->StreamID == streamID) {
            found = flow;
            break;
         }
      }
   }
   unlock();
   
   return(found);
}


// ###### Find Flow by source address #######################################
Flow* FlowManager::findFlow(const struct sockaddr* from)
{
   Flow* found = NULL;

   lock();
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
      Flow* flow = *iterator;
      if( (flow->RemoteAddressIsValid) &&
          (addresscmp(from, &flow->RemoteAddress.sa, true) == 0) ) {
         found = flow;
         break;
      }
   }
   unlock();
   
   return(found);
}


void FlowManager::startMeasurement(const uint64_t           measurementID,
                                   const bool               printFlows,
                                   const unsigned long long now)
{
   lock();
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
      Flow* flow = *iterator;
      if(flow->MeasurementID == measurementID) {
         flow->BaseTime = now;
         flow->Status   = (flow->Traffic.OnOffEvents.size() > 0) ? Flow::Off : Flow::On;
         if(printFlows) {
            flow->print(std::cout);
         }
      }
   }
   unlock();
}


void FlowManager::stopMeasurement(const uint64_t           measurementID,
                                  const unsigned long long now)
{

}


// ###### Reception thread function #########################################
void FlowManager::run()
{
   
}







// ###### Constructor #######################################################
Flow::Flow(const uint64_t     measurementID,
           const uint32_t     flowID,
           const uint16_t     streamID,
           const TrafficSpec& trafficSpec)
{
   RemoteControlAssocID     = 0;
   RemoteDataAssocID        = 0;
   RemoteAddressIsValid     = false;
   SocketDescriptor         = -1;
   OriginalSocketDescriptor = false;

   Status                   = WaitingForStartup;
   NextStatusChangeEvent    = ~0;
   NextTransmissionEvent    = ~0;
   BaseTime                 = getMicroTime();

   Traffic                  = trafficSpec;

   resetStatistics();
   LastOutboundSeqNumber    = 0;
   LastOutboundFrameID      = 0;
   
   FlowManager::getFlowManager()->addFlow(this);
}


// ###### Destructor ########################################################
Flow::~Flow()
{
   FlowManager::getFlowManager()->removeFlow(this);
   deactivate();
}


void Flow::resetStatistics()
{
   CurrentBandwidthStats.reset();
   LastBandwidthStats.reset();
   Jitter = 0;
   Delay  = 0;
}


// ###### Print Flow ########################################################
void Flow::print(std::ostream& os, const bool printStatistics) const
{
   if((OriginalSocketDescriptor) || (RemoteControlAssocID != 0)) {
      if(Traffic.Protocol != IPPROTO_SCTP) {
         os << "+ " << getProtocolName(Traffic.Protocol) << " Flow (Flow ID #"
            << FlowID << " \"" << Traffic.Description << "\"):" << std::endl;
      }
      else {
         os << "+ " << getProtocolName(Traffic.Protocol) << " Flow:" << std::endl;
      }
   }
   if(Traffic.Protocol == IPPROTO_SCTP) {
      os << "   o Stream #" << StreamID << " (Flow ID #"
         << FlowID << " \"" << Traffic.Description << "\"):" << std::endl;
   }
   Traffic.print(os);


   if(printStatistics) {
      CurrentBandwidthStats.print(os,
                                  (LastTransmission - FirstTransmission) / 1000000.0,
                                  (LastReception    - FirstReception)    / 1000000.0);
   }
}


void Flow::setSocketDescriptor(const int  socketDescriptor,
                               const bool originalSocketDescriptor)
{
   deactivate();
   SocketDescriptor         = socketDescriptor;
   OriginalSocketDescriptor = originalSocketDescriptor;
}


bool Flow::activate()
{
   deactivate();
   assert(SocketDescriptor >= 0);
   start();
}


bool Flow::deactivate()
{
}


void Flow::run()
{
   
}
