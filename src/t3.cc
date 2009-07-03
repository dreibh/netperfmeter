#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
#include "messagereader.h"
#include "tools.h"
#include "t3.h"


extern size_t gMaxMsgSize;



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
   puts("Shut1");
   stop();
   puts("Shut1b");
   waitForFinish();
   puts("Shut2");
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
   flow->PollFDEntry = NULL;
   FlowSet.push_back(flow);
   unlock();
}


// ###### Remove flow #######################################################
void FlowManager::removeFlow(Flow* flow)
{
   flow->deactivate();

   lock();
   flow->PollFDEntry = NULL;
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
       if(*iterator == flow) {
          FlowSet.erase(iterator);
         break;
      }
   }
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


// ###### Start measurement #################################################
void FlowManager::startMeasurement(const uint64_t           measurementID,
                                   const bool               printFlows,
                                   const unsigned long long now)
{
   lock();
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
      Flow* flow = *iterator;
      if(flow->MeasurementID == measurementID) {
         if(flow->SocketDescriptor >= 0) {
            flow->BaseTime     = now;
            flow->InputStatus  = Flow::On;
            flow->OutputStatus = (flow->Traffic.OnOffEvents.size() > 0) ? Flow::Off : Flow::On;
            if(printFlows) {
               flow->print(std::cout);
            }
            flow->activate();
         }
      }
   }
   unlock();
}


// ###### Stop measurement ##################################################
void FlowManager::stopMeasurement(const uint64_t           measurementID,
                                  const bool               printFlows,
                                  const unsigned long long now)
{
   lock();
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
      Flow* flow = *iterator;
      if(flow->MeasurementID == measurementID) {
         flow->deactivate();
         if(printFlows) {
            flow->print(std::cout, true);
         }
      }
   }
   unlock();
}








// ###### Constructor #######################################################
Flow::Flow(const uint64_t     measurementID,
           const uint32_t     flowID,
           const uint16_t     streamID,
           const TrafficSpec& trafficSpec,
           const int          controlSocketDescriptor,
           const sctp_assoc_t controlAssocID)
{
   MeasurementID                 = measurementID;
   FlowID                        = flowID;
   StreamID                      = streamID;

   SocketDescriptor              = -1;   
   OriginalSocketDescriptor      = false;
   RemoteControlSocketDescriptor = controlSocketDescriptor;
   RemoteControlAssocID          = controlAssocID;
   RemoteAddressIsValid          = false;

   InputStatus                   = WaitingForStartup;
   OutputStatus                  = WaitingForStartup;
   BaseTime                      = getMicroTime();

   Traffic                       = trafficSpec;

   resetStatistics();
   LastOutboundSeqNumber         = 0;
   LastOutboundFrameID           = 0;
   
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
   FlowManager::getFlowManager()->lock();
   SocketDescriptor         = socketDescriptor;
   OriginalSocketDescriptor = originalSocketDescriptor;
   FlowManager::getFlowManager()->unlock();
}


void Flow::updateTransmissionStatistics(const unsigned long long now,
                                        const size_t             addedFrames,
                                        const size_t             addedPackets,
                                        const size_t             addedBytes)
{
   lock();
   LastTransmission = now;
   if(FirstTransmission == 0) {
      FirstTransmission = now;
   }
   CurrentBandwidthStats.TransmittedFrames  += addedFrames;
   CurrentBandwidthStats.TransmittedPackets += addedPackets;
   CurrentBandwidthStats.TransmittedBytes   += addedBytes;
   unlock();
}


void Flow::updateReceptionStatistics(const unsigned long long now,
                                     const size_t             addedFrames,
                                     const size_t             addedBytes,
                                     const double             delay,
                                     const double             jitter)
{
   lock();
   LastReception = now;
   if(FirstReception == 0) {
      FirstReception = now;
   }
   CurrentBandwidthStats.ReceivedFrames  += addedFrames;
   CurrentBandwidthStats.ReceivedPackets++;
   CurrentBandwidthStats.ReceivedBytes   += addedBytes;
   Delay  = delay;
   Jitter = jitter;
   unlock();
}


bool Flow::activate()
{
   deactivate();
   assert(SocketDescriptor >= 0);
   FlowManager::getFlowManager()->getMessageReader()->registerSocket(Traffic.Protocol, SocketDescriptor);
   start();
}


bool Flow::deactivate()
{
   if(isRunning()) {
      puts("------------------ STOP --------------------");
      stop();
      if(SocketDescriptor >= 0) {
         if(Traffic.Protocol == IPPROTO_UDP) {
            puts("udp---");
            ext_close(SocketDescriptor);
         }
         else {
         puts("tcp---");
            ext_shutdown(SocketDescriptor, 2);
         }
      }
      puts("wait...");
      waitForFinish();
      puts("------------------ STOP okay!");
   
      FlowManager::getFlowManager()->getMessageReader()->deregisterSocket(SocketDescriptor);
   }
   else
   puts("ALREADY STOPPED!");
}


// ###### Schedule next status change event #################################
unsigned long long Flow::scheduleNextStatusChangeEvent(const unsigned long long now)
{
   unsigned long long               nextTransmissionEvent;
   std::set<unsigned int>::iterator first = Traffic.OnOffEvents.begin();

   if((OutputStatus != WaitingForStartup) && (first != Traffic.OnOffEvents.end())) {
      const unsigned int relNextEvent = *first;
      const unsigned long long absNextEvent = BaseTime + (1000ULL * relNextEvent);
      nextTransmissionEvent = absNextEvent;
   }
   else {
      nextTransmissionEvent = ~0;
   }
   return(nextTransmissionEvent);
}


// ###### Schedule next transmission event (non-saturated sender) ###########
unsigned long long Flow::scheduleNextTransmissionEvent() const
{
   unsigned long long nextTransmissionEvent;

   if(OutputStatus == On) {
      // ====== Saturated sender ============================================
      if( (Traffic.OutboundFrameSize > 0.0) && (Traffic.OutboundFrameRate <= 0.0000001) ) {
         nextTransmissionEvent = 0;
      }
      // ====== Non-saturated sender ========================================
      else if( (Traffic.OutboundFrameSize > 0.0) && (Traffic.OutboundFrameRate > 0.0000001) ) {
         const double nextFrameRate = getRandomValue(Traffic.OutboundFrameRate, Traffic.OutboundFrameRateRng);
         nextTransmissionEvent = LastTransmission + (unsigned long long)rint(1000000.0 / nextFrameRate);
      }
   }
   else {
      nextTransmissionEvent = ~0;
   }
   return(nextTransmissionEvent);
}



void Flow::run()
{
puts("START-OF-THREAD!!!");
   signal(SIGPIPE, SIG_IGN);

   bool result = true;
   do {
      // ====== Schedule next status change event ===========================
      unsigned long long       now              = getMicroTime();
      const unsigned long long nextStatusChange = scheduleNextStatusChangeEvent(now);
      const unsigned long long nextTransmission = scheduleNextTransmissionEvent();
      unsigned long long       nextEvent        = std::min(nextStatusChange, nextTransmission);

      // ====== Wait until there is something to do =========================
      if(nextEvent > now) {
         int timeout = std::min(1000, (int)((nextEvent - now) / 1000));
// ?????         printf("Wait %d ms    ss=%1.0f  tx=%1.0f\n", timeout,  (double)nextStatusChange-(double)now,(double)nextTransmission-(double)now);
         usleep(1000 * (timeout + 1));
         puts("---");
         now = getMicroTime();
      }
      else {
// ?????         printf("NoWait   ss=%1.0f  tx=%1.0f\n", (double)nextStatusChange-(double)now,(double)nextTransmission-(double)now);
      }

      // ====== Send outgoing data ==========================================
      if(OutputStatus == Flow::On) {
         // ====== Outgoing data (saturated sender) =========================
         if( (Traffic.OutboundFrameSize > 0.0) && (Traffic.OutboundFrameRate <= 0.0000001) ) {
            result = (transmitFrame(this, now, gMaxMsgSize) > 0);
         }

         // ====== Outgoing data (non-saturated sender) =====================
         else if( (Traffic.OutboundFrameSize >= 1.0) && (Traffic.OutboundFrameRate > 0.0000001) ) {
            const unsigned long long lastEvent = LastTransmission;
            if(nextTransmission <= now) {
               do {
                  result = (transmitFrame(this, now, gMaxMsgSize) > 0);
                  if(now - lastEvent > 1000000) {
                     // Time gap of more than 1s -> do not try to correct
                     break;
                  }
               } while(scheduleNextTransmissionEvent() <= now);
            }
         }
      }
   } while( (result == true) && (!Stopping) );
   
   puts("END-OF-THREAD!!!");
}




void FlowManager::addSocket(const int protocol, const int socketDescriptor)
{
   lock();
   FlowManager::getFlowManager()->getMessageReader()->registerSocket(protocol, socketDescriptor);
   UnidentifiedSockets.insert(std::pair<int, int>(socketDescriptor, protocol));
   UpdatedUnidentifiedSockets = true;
   unlock();
}


bool FlowManager::identifySocket(const uint64_t        measurementID,
                                 const uint32_t        flowID,
                                 const uint16_t        streamID,
                                 const int             socketDescriptor,
                                 const sockaddr_union* from,
                                 const bool            compressVectorFile,
                                 int&                  controlSocketDescriptor,
                                 sctp_assoc_t&         controlAssocID)
{
   bool success            = false;
   controlSocketDescriptor = -1;
   controlAssocID          = 0;

   lock();
   Flow* flow = findFlow(measurementID, flowID, streamID);
   if( (flow != NULL) && (flow->RemoteAddressIsValid == false) ) {
      flow->lock();
      flow->setSocketDescriptor(socketDescriptor, false);
      flow->RemoteAddress        = *from;
      flow->RemoteAddressIsValid = true;
      controlSocketDescriptor    = flow->RemoteControlSocketDescriptor;
      controlAssocID             = flow->RemoteControlAssocID;
      success = flow->getVectorFile().initialize(NULL, compressVectorFile);
      flow->unlock();
      removeSocket(socketDescriptor, false);   // Socket is now managed as flow!
   }
   unlock();

   return(success);
}


void FlowManager::removeSocket(const int  socketDescriptor,
                               const bool closeSocket)
{
   lock();
   std::map<int, int>::iterator found = UnidentifiedSockets.find(socketDescriptor);
   assert(found != UnidentifiedSockets.end());
   UnidentifiedSockets.erase(found);
   UpdatedUnidentifiedSockets = true;
   if(closeSocket) {
      FlowManager::getFlowManager()->getMessageReader()->deregisterSocket(socketDescriptor);
      ext_close(socketDescriptor);
   }
   unlock();
}




// ###### Reception thread function #########################################
void FlowManager::run()
{
puts("FM-Thread!!!");
   do {
      lock();
      pollfd pollFDs[FlowSet.size() + UnidentifiedSockets.size()];
      size_t n = 0;
      for(size_t i = 0;i  < FlowSet.size();i++) {
         if( (FlowSet[i]->InputStatus != Flow::Off) &&
             (FlowSet[i]->SocketDescriptor >= 0) ) {
            pollFDs[n].fd      = FlowSet[i]->SocketDescriptor;
            pollFDs[n].events  = POLLIN;
            pollFDs[n].revents = 0;
            FlowSet[i]->PollFDEntry = &pollFDs[n];
printf("wait for IN flow=%d\n",FlowSet[i]->FlowID);         
            n++;
         }
         else {
            FlowSet[i]->PollFDEntry = NULL;
         }
      }
      UpdatedUnidentifiedSockets = false;
      pollfd* unidentifiedSocketsPollFDIndex[UnidentifiedSockets.size()];
      size_t i = 0;
      for(std::map<int, int>::iterator iterator = UnidentifiedSockets.begin();
          iterator != UnidentifiedSockets.end(); iterator++) {
         pollFDs[n].fd      = iterator->first;
         printf("wait for pollin on %d\n", iterator->first);
         pollFDs[n].events  = POLLIN;
         pollFDs[n].revents = 0;
         unidentifiedSocketsPollFDIndex[i] = &pollFDs[n];
         n++; i++;
      }
      unlock();

// puts("WAIT FM...");
      const int timeout = 1000;
      const int result = ext_poll((pollfd*)&pollFDs, n, timeout);
//  puts("WAIT FM OKAY");

      if(result > 0) {
         const unsigned long long now = getMicroTime();
         lock();
         for(i = 0;i  < FlowSet.size();i++) {
            if( (FlowSet[i]->PollFDEntry) &&
                (FlowSet[i]->PollFDEntry->revents & POLLIN) ) {
                printf("IN for flow %u   inputSt=%d\n",FlowSet[i]->FlowID,FlowSet[i]->InputStatus);
                // NOTE: FlowSet[i] may not be the actual Flow!
                //       It may be another stream of the same SCTP assoc!
                handleDataMessage(true, now,
                                  FlowSet[i]->getTrafficSpec().Protocol,
                                  FlowSet[i]->PollFDEntry->fd);
            }
         }
         if(!UpdatedUnidentifiedSockets) {
            i = 0;
            for(std::map<int, int>::iterator iterator = UnidentifiedSockets.begin();
               iterator != UnidentifiedSockets.end(); iterator++) {
               assert(unidentifiedSocketsPollFDIndex[i]->fd == iterator->first);
               if(unidentifiedSocketsPollFDIndex[i]->revents & POLLIN) {
         printf("POLLIN on %d\n", iterator->first);
                  handleDataMessage(true, now,
                                    iterator->second,
                                    iterator->first);
               }
               i++;
            }
         }
         unlock();
      }
   } while(!Stopping);
}






MeasurementManager MeasurementManager::MeasurementManagerSingleton;

// ###### Add measurement ###################################################
bool MeasurementManager::addMeasurement(Measurement* measurement)
{
   bool success = false;

   lock();
   if(!findMeasurement(measurement->MeasurementID)) {
      MeasurementSet.insert(std::pair<uint64_t, Measurement*>(measurement->MeasurementID,
                                                              measurement));
      success = true;
puts("------------ INS");
   }
   unlock();

   return(success);
}


// ###### Print measurements ################################################
void MeasurementManager::printMeasurements(std::ostream& os)
{
   os << "Measurements:" << std::endl; 
   for(std::map<uint64_t, Measurement*>::iterator iterator = MeasurementSet.begin();
       iterator != MeasurementSet.end(); iterator++) {
      char str[64];
      snprintf((char*)&str, sizeof(str), "%llx -> ptr=%p",
               (unsigned long long)iterator->first, iterator->second);
      os << "   - " << str << std::endl;
   }
}
   

// ###### Find measurement ##################################################
Measurement* MeasurementManager::findMeasurement(const uint64_t measurementID)
{
   std::map<uint64_t, Measurement*>::iterator found = MeasurementSet.find(measurementID);
   if(found != MeasurementSet.end()) {
      return(found->second);
   }
   return(NULL);
}


// ###### Remove measurement ################################################
void MeasurementManager::removeMeasurement(Measurement* measurement)
{
   lock();
   MeasurementSet.erase(measurement->MeasurementID);
   unlock();
}


unsigned long long MeasurementManager::getNextEvent()
{
   unsigned long long nextStatisticsEvent = (1ULL << 63);

puts("###################################");
   lock();
   for(std::map<uint64_t, Measurement*>::iterator iterator = MeasurementSet.begin();
       iterator != MeasurementSet.end(); iterator++) {
       const Measurement* measurement = iterator->second;
       nextStatisticsEvent = std::min(nextStatisticsEvent,
                                      measurement->NextStatisticsEvent);
printf("NX=%llx   mNX=%llx\n", nextStatisticsEvent, measurement->NextStatisticsEvent);
   }
   unlock();

printf("  => NX=%llx\n", nextStatisticsEvent);
   return(nextStatisticsEvent);
}


void MeasurementManager::handleEvents(const unsigned long long now)
{
   lock();
   for(std::map<uint64_t, Measurement*>::iterator iterator = MeasurementSet.begin();
       iterator != MeasurementSet.end(); iterator++) {
       Measurement* measurement = iterator->second;
       if(measurement->NextStatisticsEvent <= now) {
          measurement->writeVectorStatistics(now);
       }
   }
   unlock();
}


void Measurement::writeVectorStatistics(const unsigned long long now)
{
   lock();
   // ====== Timer management ===============================================
   NextStatisticsEvent += StatisticsInterval;
   if(NextStatisticsEvent <= now) {   // Initialization!
      NextStatisticsEvent = now + StatisticsInterval;
   }
   if(FirstStatisticsEvent == 0) {
      FirstStatisticsEvent = now;
      LastStatisticsEvent  = now;
   }

puts("wrt...");
   
   unlock();
}






// ###### Destructor ########################################################
Measurement::Measurement()
{
   MeasurementID = 0;
}


// ###### Destructor ########################################################
Measurement::~Measurement()
{
   finish();
puts("######### destr MS");
/*
   std::map<uint64_t, Measurement*>::iterator found = StatisticsSet.find(measurementID);
   if(found != StatisticsSet.end()) {
      Measurement* statisticsWriter = found->second;
      StatisticsSet.erase(found);
      
      // ====== Remove temporary files ======================================
      if(statisticsWriter->VectorName.find("/tmp/") == 0) {
         unlink(statisticsWriter->VectorName.c_str());
      }
      if(statisticsWriter->ScalarName.find("/tmp/") == 0) {
         unlink(statisticsWriter->ScalarName.c_str());
      }

      delete statisticsWriter;
   }   */
}


bool Measurement::initialize(const unsigned long long now,
                             const uint64_t           measurementID,
                             const char*              vectorNamePattern,
                             const char*              scalarNamePattern)
{
   MeasurementID        = measurementID;
   StatisticsInterval   = 1000000;
   NextStatisticsEvent  = 0;
   FirstStatisticsEvent = 0;
   LastStatisticsEvent  = 0;

   VectorNamePattern    = std::string(vectorNamePattern);
   ScalarNamePattern    = std::string(scalarNamePattern);

   return(MeasurementManager::getMeasurementManager()->addMeasurement(this));
}


void Measurement::finish()
{
   MeasurementManager::getMeasurementManager()->removeMeasurement(this);
   VectorFile.finish();
   ScalarFile.finish();
}
