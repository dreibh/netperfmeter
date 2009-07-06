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



// ###### Constructor #######################################################
OutputFile::OutputFile()
{
   File       = NULL;
   BZFile     = NULL;
   WriteError = false;
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
      Name = "(temporary file)";
   }
   Line = 0;

   // ====== Initialize file ================================================
   File = (name != NULL) ? fopen(name, "w+") : tmpfile();
   if(File == NULL) {
      std::cerr << "ERROR: Unable to create output file " << name << "!" << std::endl;
      WriteError = true;
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
         WriteError = true;
         return(false);
      }
   }
   WriteError = false;
   return(true);
}


// ###### Finish output file ################################################
bool OutputFile::finish(const bool closeFile)
{
   // ====== Finish BZip2 compression =======================================
   if(BZFile) {
      int bzerror;
      BZ2_bzWriteClose(&bzerror, BZFile, 0, NULL, NULL);
      if(bzerror != BZ_OK) {
         std::cerr << "ERROR: Unable to finish BZip2 compression on file <"
                   << Name << ">!" << std::endl
                   << "Reason: " << BZ2_bzerror(BZFile, &bzerror) << std::endl;
         WriteError = true;
      }
      BZFile = NULL;
   }
   // ====== Close or rewind file ===========================================
   if(File) {
      if(closeFile) {
         if(fclose(File) != 0) {
            WriteError = true;
         }
         File = NULL;
      }
      else {
         rewind(File);
      }
   }
   return(!WriteError);
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
FlowTrafficSpec::FlowTrafficSpec()
{
   reset();
}


// ###### Destructor ########################################################
FlowTrafficSpec::~FlowTrafficSpec()
{
}


// ###### Show configuration entry (value + random number generator) ########
void FlowTrafficSpec::showEntry(std::ostream& os,
                                const double  value,
                                const uint8_t rng)
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


// ###### Print FlowTrafficSpec #############################################
void FlowTrafficSpec::print(std::ostream& os) const
{
   os << "      - Max. Message Size:   "
      << MaxMsgSize << std::endl;
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


// ###### Reset FlowTrafficSpec #############################################
void FlowTrafficSpec::reset()
{
   MaxMsgSize               = 16000;
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


// ###### "+" operator ######################################################
FlowBandwidthStats operator+(const FlowBandwidthStats& s1,
                             const FlowBandwidthStats& s2)
{
   FlowBandwidthStats result;
   result.TransmittedBytes   = s1.TransmittedBytes + s2.TransmittedBytes;
   result.TransmittedPackets = s1.TransmittedPackets + s2.TransmittedPackets;
   result.TransmittedFrames  = s1.TransmittedFrames + s2.TransmittedFrames;

   result.ReceivedBytes      = s1.ReceivedBytes + s2.ReceivedBytes;
   result.ReceivedPackets    = s1.ReceivedPackets + s2.ReceivedPackets;
   result.ReceivedFrames     = s1.ReceivedFrames + s2.ReceivedFrames;

   result.LostBytes          = s1.LostBytes + s2.LostBytes;
   result.LostPackets        = s1.LostPackets + s2.LostPackets;
   result.LostFrames         = s1.LostFrames + s2.LostFrames;
   return(result);
}


// ###### "-" operator ######################################################
FlowBandwidthStats operator-(const FlowBandwidthStats& s1, 
                             const FlowBandwidthStats& s2)
{
   FlowBandwidthStats result;
   result.TransmittedBytes   = s1.TransmittedBytes - s2.TransmittedBytes;
   result.TransmittedPackets = s1.TransmittedPackets - s2.TransmittedPackets;
   result.TransmittedFrames  = s1.TransmittedFrames - s2.TransmittedFrames;

   result.ReceivedBytes      = s1.ReceivedBytes - s2.ReceivedBytes;
   result.ReceivedPackets    = s1.ReceivedPackets - s2.ReceivedPackets;
   result.ReceivedFrames     = s1.ReceivedFrames - s2.ReceivedFrames;

   result.LostBytes          = s1.LostBytes - s2.LostBytes;
   result.LostPackets        = s1.LostPackets - s2.LostPackets;
   result.LostFrames         = s1.LostFrames - s2.LostFrames;
   return(result);
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
   DisplayOn         = false;
   DisplayInterval   = 1000000;
   FirstDisplayEvent = 0;
   LastDisplayEvent  = 0;
   NextDisplayEvent  = 0;
   start();
}


// ###### Destructor ########################################################
FlowManager::~FlowManager()
{
   stop();
   waitForFinish();
}


// ###### Print all flows ###################################################
void FlowManager::printFlows(std::ostream& os,
                             const bool    printStatistics)
{
   lock();
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
      Flow* flow= *iterator;
      flow->print(os, printStatistics);
   }
   unlock();
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
   
   lock();
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
bool FlowManager::startMeasurement(const uint64_t           measurementID,
                                   const unsigned long long now,
                                   const char*              vectorNamePattern,
                                   const bool               compressVectorFile,
                                   const char*              scalarNamePattern,
                                   const bool               compressScalarFile,
                                   const bool               printFlows)
{
   bool success = false;
   
   lock();
   Measurement* measurement = new Measurement;
   if(measurement != NULL) {
      if(measurement->initialize(now, measurementID, 
                                 vectorNamePattern, compressVectorFile,
                                 scalarNamePattern, compressScalarFile)) {
         success = true;
         for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
            iterator != FlowSet.end();iterator++) {
            Flow* flow = *iterator;
            if(flow->MeasurementID == measurementID) {
               flow->setMeasurement(measurement);
               if(flow->SocketDescriptor >= 0) {
                  flow->BaseTime     = now;
                  flow->InputStatus  = Flow::On;
                  flow->OutputStatus = (flow->TrafficSpec.OnOffEvents.size() > 0) ? Flow::Off : Flow::On;
                  if(printFlows) {
                     flow->print(std::cout);
                  }            
                  flow->activate();
               }
            }
         }
      }
      else {
         delete measurement;
      }
   }
   unlock();
   
   return(success);
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
         // ====== Stop flow ================================================
         flow->deactivate();
         if(printFlows) {
            flow->print(std::cout, true);
         }
      }
   }
   unlock();
}



void FlowManager::writeScalarStatistics(const uint64_t           measurementID,
                                        const unsigned long long now,
                                        OutputFile&              scalarFile,
                                        const unsigned long long firstStatisticsEvent)
{
   std::string objectName =
      std::string("netPerfMeter.") +
         ((scalarFile.getName() == std::string()) ? "passive" : "active");

   lock();
   FlowBandwidthStats totalBandwidthStats;
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
      iterator != FlowSet.end();iterator++) {
      Flow* flow = *iterator;
      flow->lock();
      if(flow->MeasurementID == measurementID) {
         const double transmissionDuration = (flow->LastTransmission - flow->FirstTransmission) / 1000000.0;
         const double receptionDuration    = (flow->LastReception - flow->FirstReception) / 1000000.0;
         scalarFile.printf(
            "scalar \"%s.flow[%u]\" \"Transmitted Bytes\"       %llu\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Packets\"     %llu\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Frames\"      %llu\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Byte Rate\"   %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Packet Rate\" %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Transmitted Frame Rate\"  %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Received Bytes\"          %llu\n"
            "scalar \"%s.flow[%u]\" \"Received Packets\"        %llu\n"
            "scalar \"%s.flow[%u]\" \"Received Frames\"         %llu\n"
            "scalar \"%s.flow[%u]\" \"Received Byte Rate\"      %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Received Packet Rate\"    %1.6f\n"
            "scalar \"%s.flow[%u]\" \"Received Frame Rate\"     %1.6f\n"
            ,
            objectName.c_str(), flow->FlowID, flow->CurrentBandwidthStats.TransmittedBytes,
            objectName.c_str(), flow->FlowID, flow->CurrentBandwidthStats.TransmittedPackets,
            objectName.c_str(), flow->FlowID, flow->CurrentBandwidthStats.TransmittedFrames,
            objectName.c_str(), flow->FlowID, (transmissionDuration > 0.0) ? flow->CurrentBandwidthStats.TransmittedBytes   / transmissionDuration : 0.0,
            objectName.c_str(), flow->FlowID, (transmissionDuration > 0.0) ? flow->CurrentBandwidthStats.TransmittedPackets / transmissionDuration : 0.0,
            objectName.c_str(), flow->FlowID, (transmissionDuration > 0.0) ? flow->CurrentBandwidthStats.TransmittedFrames  / transmissionDuration : 0.0,
            objectName.c_str(), flow->FlowID, flow->CurrentBandwidthStats.ReceivedBytes,
            objectName.c_str(), flow->FlowID, flow->CurrentBandwidthStats.ReceivedPackets,
            objectName.c_str(), flow->FlowID, flow->CurrentBandwidthStats.ReceivedFrames,
            objectName.c_str(), flow->FlowID, (receptionDuration > 0.0) ? flow->CurrentBandwidthStats.ReceivedBytes   / receptionDuration : 0.0,
            objectName.c_str(), flow->FlowID, (receptionDuration > 0.0) ? flow->CurrentBandwidthStats.ReceivedPackets / receptionDuration : 0.0,
            objectName.c_str(), flow->FlowID, (receptionDuration > 0.0) ? flow->CurrentBandwidthStats.ReceivedFrames  / receptionDuration : 0.0
            );
         totalBandwidthStats = totalBandwidthStats + flow->CurrentBandwidthStats;
      }
      flow->unlock();
   }

   // ====== Write total statistics =========================================
   const double totalDuration = (now - firstStatisticsEvent) / 1000000.0;
   scalarFile.printf(
      "scalar \"%s.total\" \"Transmitted Bytes\"       %llu\n"
      "scalar \"%s.total\" \"Transmitted Packets\"     %llu\n"
      "scalar \"%s.total\" \"Transmitted Frames\"      %llu\n"
      "scalar \"%s.total\" \"Transmitted Byte Rate\"   %1.6f\n"
      "scalar \"%s.total\" \"Transmitted Packet Rate\" %1.6f\n"
      "scalar \"%s.total\" \"Transmitted Frame Rate\"  %1.6f\n"
      "scalar \"%s.total\" \"Received Bytes\"          %llu\n"
      "scalar \"%s.total\" \"Received Packets\"        %llu\n"
      "scalar \"%s.total\" \"Received Frames\"         %llu\n"
      "scalar \"%s.total\" \"Received Byte Rate\"      %1.6f\n"
      "scalar \"%s.total\" \"Received Packet Rate\"    %1.6f\n"
      "scalar \"%s.total\" \"Received Frame Rate\"     %1.6f\n"
      "scalar \"%s.total\" \"Lost Bytes\"              %llu\n"
      "scalar \"%s.total\" \"Lost Packets\"            %llu\n"
      "scalar \"%s.total\" \"Lost Frames\"             %llu\n"
      "scalar \"%s.total\" \"Lost Byte Rate\"          %1.6f\n"
      "scalar \"%s.total\" \"Lost Packet Rate\"        %1.6f\n"
      "scalar \"%s.total\" \"Lost Frame Rate\"         %1.6f\n"
      ,
      objectName.c_str(), totalBandwidthStats.TransmittedBytes,
      objectName.c_str(), totalBandwidthStats.TransmittedPackets,
      objectName.c_str(), totalBandwidthStats.TransmittedFrames,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.TransmittedBytes   / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.TransmittedPackets / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.TransmittedFrames  / totalDuration : 0.0,
         
      objectName.c_str(), totalBandwidthStats.ReceivedBytes,
      objectName.c_str(), totalBandwidthStats.ReceivedPackets,
      objectName.c_str(), totalBandwidthStats.ReceivedFrames,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.ReceivedBytes   / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.ReceivedPackets / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.ReceivedFrames  / totalDuration : 0.0,

      objectName.c_str(), totalBandwidthStats.LostBytes,
      objectName.c_str(), totalBandwidthStats.LostPackets,
      objectName.c_str(), totalBandwidthStats.LostFrames,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.LostBytes   / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.LostPackets / totalDuration : 0.0,
      objectName.c_str(), (totalDuration > 0.0) ? totalBandwidthStats.LostFrames  / totalDuration : 0.0
      );
   unlock();
}
                                        

void FlowManager::writeVectorStatistics(const uint64_t           measurementID,
                                        const unsigned long long now,
                                        OutputFile&              vectorFile,
                                        FlowBandwidthStats&      globalStats,
                                        FlowBandwidthStats&      relGlobalStats,
                                        const unsigned long long firstStatisticsEvent,
                                        const unsigned long long lastStatisticsEvent)
{
   // ====== Write vector statistics header =================================
   if(vectorFile.getLine() == 0) {
      vectorFile.printf("AbsTime RelTime Interval\t"
                        "FlowID Description Jitter\t"
                        "Action\t"
                           "AbsBytes AbsPackets AbsFrames\t"
                           "RelBytes RelPackets RelFrames\n");
   }

   lock();

   // ====== Write flow statistics ==========================================
   FlowBandwidthStats lastTotalStats;
   FlowBandwidthStats currentTotalStats;
   const double duration = (now - lastStatisticsEvent) / 1000000.0;
   for(std::vector<Flow*>::iterator iterator = FlowSet.begin();
       iterator != FlowSet.end();iterator++) {
       Flow* flow = *iterator;
       if(flow->MeasurementID == measurementID) {
          flow->lock();

          const FlowBandwidthStats relStats = flow->CurrentBandwidthStats - flow->LastBandwidthStats;
          lastTotalStats     = lastTotalStats + flow->LastBandwidthStats;
          currentTotalStats  = currentTotalStats + flow->CurrentBandwidthStats;
          CurrentGlobalStats = CurrentGlobalStats + relStats;

          vectorFile.printf(
            "%06llu %llu %1.6f %1.6f\t%u \"%s\" %1.3f\t"
               "\"Sent\"     %llu %llu %llu\t%llu %llu %llu\n"
            "%06llu %llu %1.6f %1.6f\t%u \"%s\" %1.3f\t"
               "\"Received\" %llu %llu %llu\t%llu %llu %llu\n"
            "%06llu %llu %1.6f %1.6f\t%u \"%s\" %1.3f\t"
               "\"Lost\"     %llu %llu %llu\t%llu %llu %llu\n",

            vectorFile.nextLine(), now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
               flow->FlowID, flow->TrafficSpec.Description.c_str(), flow->Jitter,
               flow->CurrentBandwidthStats.TransmittedBytes, flow->CurrentBandwidthStats.TransmittedPackets, flow->CurrentBandwidthStats.TransmittedFrames,
               relStats.TransmittedBytes, relStats.TransmittedPackets, relStats.TransmittedFrames,

            vectorFile.nextLine(), now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
               flow->FlowID, flow->TrafficSpec.Description.c_str(), flow->Jitter,
               flow->CurrentBandwidthStats.ReceivedBytes, flow->CurrentBandwidthStats.ReceivedPackets, flow->CurrentBandwidthStats.ReceivedFrames,
               relStats.ReceivedBytes, relStats.ReceivedPackets, relStats.ReceivedFrames,

            vectorFile.nextLine(), now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
               flow->FlowID, flow->TrafficSpec.Description.c_str(), flow->Jitter,
               flow->CurrentBandwidthStats.LostBytes, flow->CurrentBandwidthStats.LostPackets, flow->CurrentBandwidthStats.LostFrames,
               relStats.LostBytes, relStats.LostPackets, relStats.LostFrames);
               
          flow->LastBandwidthStats = flow->CurrentBandwidthStats;
          flow->unlock();
       }
   }

   // ====== Write total statistics =========================================
   const FlowBandwidthStats relTotalStats = currentTotalStats - lastTotalStats;
      vectorFile.printf(
      "%06llu %llu %1.6f %1.6f\t-1 \"Total\" 0\t"
         "\"Sent\"     %llu %llu %llu\t%llu %llu %llu\n"
      "%06llu %llu %1.6f %1.6f\t-1 \"Total\" 0\t"
         "\"Received\" %llu %llu %llu\t%llu %llu %llu\n"
      "%06llu %llu %1.6f %1.6f\t-1 \"Total\" 0\t"
         "\"Lost\"     %llu %llu %llu\t%llu %llu %llu\n",

      vectorFile.nextLine(), now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
         currentTotalStats.TransmittedBytes, currentTotalStats.TransmittedPackets, currentTotalStats.TransmittedFrames,
         relTotalStats.TransmittedBytes, relTotalStats.TransmittedPackets, relTotalStats.TransmittedFrames,

      vectorFile.nextLine(), now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
         currentTotalStats.ReceivedBytes, currentTotalStats.ReceivedPackets, currentTotalStats.ReceivedFrames,
         relTotalStats.ReceivedBytes, relTotalStats.ReceivedPackets, relTotalStats.ReceivedFrames,

      vectorFile.nextLine(), now, (double)(now - firstStatisticsEvent) / 1000000.0, duration,
         currentTotalStats.LostBytes, currentTotalStats.LostPackets, currentTotalStats.LostFrames,
         relTotalStats.LostBytes, relTotalStats.LostPackets, relTotalStats.LostFrames);

   
   // ====== Return global values (all measurements) for displaying =========
   globalStats     = CurrentGlobalStats;
   relGlobalStats  = CurrentGlobalStats - LastGlobalStats;
   
   LastGlobalStats = CurrentGlobalStats;

   unlock();
}








// ###### Constructor #######################################################
Flow::Flow(const uint64_t         measurementID,
           const uint32_t         flowID,
           const uint16_t         streamID,
           const FlowTrafficSpec& trafficSpec,
           const int              controlSocketDescriptor,
           const sctp_assoc_t     controlAssocID)
{
   lock();
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

   TrafficSpec                   = trafficSpec;

   MyMeasurement                 = NULL;
   FirstTransmission             = 0;
   LastTransmission              = 0;
   FirstReception                = 0;
   LastReception                 = 0;
   resetStatistics();
   LastOutboundSeqNumber         = 0;
   LastOutboundFrameID           = 0;
   NextStatusChangeEvent         = ~0ULL;
   unlock();
   
   FlowManager::getFlowManager()->addFlow(this);
}


// ###### Destructor ########################################################
Flow::~Flow()
{
   FlowManager::getFlowManager()->removeFlow(this);
   deactivate();
   VectorFile.finish(true);
}


void Flow::resetStatistics()
{
   lock();
   CurrentBandwidthStats.reset();
   LastBandwidthStats.reset();
   Jitter = 0;
   Delay  = 0;
   unlock();
}


// ###### Print Flow ########################################################
void Flow::print(std::ostream& os, const bool printStatistics)
{
   lock();
   if((OriginalSocketDescriptor) || (RemoteControlAssocID != 0)) {
      if(TrafficSpec.Protocol != IPPROTO_SCTP) {
         os << "+ " << getProtocolName(TrafficSpec.Protocol) << " Flow (Flow ID #"
            << FlowID << " \"" << TrafficSpec.Description << "\"):" << std::endl;
      }
      else {
         os << "+ " << getProtocolName(TrafficSpec.Protocol) << " Flow:" << std::endl;
      }
   }
   if(TrafficSpec.Protocol == IPPROTO_SCTP) {
      os << "   o Stream #" << StreamID << " (Flow ID #"
         << FlowID << " \"" << TrafficSpec.Description << "\"):" << std::endl;
   }
   TrafficSpec.print(os);


   if(printStatistics) {
      CurrentBandwidthStats.print(os,
                                  (LastTransmission - FirstTransmission) / 1000000.0,
                                  (LastReception    - FirstReception)    / 1000000.0);
   }
   unlock();
}


void Flow::setSocketDescriptor(const int  socketDescriptor,
                               const bool originalSocketDescriptor)
{
   deactivate();
   lock();
   SocketDescriptor         = socketDescriptor;
   OriginalSocketDescriptor = originalSocketDescriptor;
   unlock();
}


bool Flow::initializeVectorFile(const char* name, const bool compressed)
{
   bool success = false;
   
   lock();
   if(VectorFile.initialize(name, compressed)) {
      VectorFile.nextLine();
      success = VectorFile.printf(
                   "AbsTime RelTime SeqNumber Delay PrevPacketDelayDiff Jitter\n");
   }
   unlock();
   
   return(success);
}


void Flow::updateTransmissionStatistics(const unsigned long long now,
                                        const size_t             addedFrames,
                                        const size_t             addedPackets,
                                        const size_t             addedBytes)
{
   lock();

   // ====== Update statistics ==============================================
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
                                     const unsigned long long seqNumber,
                                     const double             delay,
                                     const double             delayDiff,
                                     const double             jitter)
{
   lock();
   
   // ====== Update statistics ==============================================
   LastReception = now;
   if(FirstReception == 0) {
      FirstReception = now;
   }
   CurrentBandwidthStats.ReceivedFrames  += addedFrames;
   CurrentBandwidthStats.ReceivedPackets++;
   CurrentBandwidthStats.ReceivedBytes   += addedBytes;
   Delay  = delay;
   Jitter = jitter;
   
   // ====== Write line to flow's vector file ===============================
   if( (MyMeasurement) && (MyMeasurement->getFirstStatisticsEvent() > 0) ) {
      VectorFile.printf(
         "%06llu %llu %1.6f\t"
         "%llu %1.3f %1.3f %1.3f\n",
         VectorFile.nextLine(), now,
         (double)(now - MyMeasurement->getFirstStatisticsEvent()) / 1000000.0,
         seqNumber, delay, delayDiff, jitter);
   }
   
   unlock();
}


bool Flow::activate()
{
   deactivate();
   assert(SocketDescriptor >= 0);
   FlowManager::getFlowManager()->getMessageReader()->registerSocket(
      TrafficSpec.Protocol, SocketDescriptor);
   return(start());
}


void Flow::deactivate()
{
   if(isRunning()) {
      stop();
      if(SocketDescriptor >= 0) {
         if(TrafficSpec.Protocol == IPPROTO_UDP) {
            // NOTE: There is only one UDP socket. We cannot close it here!
            // The thread will notice the need to finish after the poll()
            // timeout.
         }
         else {
            ext_shutdown(SocketDescriptor, 2);
         }
      }
      waitForFinish();
      FlowManager::getFlowManager()->getMessageReader()->deregisterSocket(SocketDescriptor);
   }
}


// ###### Schedule next transmission event (non-saturated sender) ###########
unsigned long long Flow::scheduleNextTransmissionEvent()
{
   unsigned long long nextTransmissionEvent;

   lock();
   if(OutputStatus == On) {
      // ====== Saturated sender ============================================
      if( (TrafficSpec.OutboundFrameSize > 0.0) && (TrafficSpec.OutboundFrameRate <= 0.0000001) ) {
         nextTransmissionEvent = 0;
      }
      // ====== Non-saturated sender ========================================
      else if( (TrafficSpec.OutboundFrameSize > 0.0) && (TrafficSpec.OutboundFrameRate > 0.0000001) ) {
         const double nextFrameRate = getRandomValue(TrafficSpec.OutboundFrameRate, TrafficSpec.OutboundFrameRateRng);
         nextTransmissionEvent = LastTransmission + (unsigned long long)rint(1000000.0 / nextFrameRate);
      }
   }
   else {
      nextTransmissionEvent = ~0;
   }
   unlock();
   
   return(nextTransmissionEvent);
}


// ###### Schedule next status change event #################################
unsigned long long Flow::scheduleNextStatusChangeEvent(const unsigned long long now)
{
   lock();
   
   std::set<unsigned int>::iterator first = TrafficSpec.OnOffEvents.begin();
   if((OutputStatus != WaitingForStartup) && (first != TrafficSpec.OnOffEvents.end())) {
      const unsigned int relNextEvent = *first;
      const unsigned long long absNextEvent = BaseTime + (1000ULL * relNextEvent);
      NextStatusChangeEvent = absNextEvent;
   }
   else {
      NextStatusChangeEvent = ~0ULL;
   }
   
   unlock();
   return(NextStatusChangeEvent);
}


// ###### Status change event ###############################################
void Flow::handleStatusChangeEvent(const unsigned long long now)
{
   lock();
   if(NextStatusChangeEvent <= now) {
      std::set<unsigned int>::iterator first = TrafficSpec.OnOffEvents.begin();
      assert(first != TrafficSpec.OnOffEvents.end());

      if(OutputStatus == Off) {
         OutputStatus = On;
      }
      else if(OutputStatus == On) {
         OutputStatus = Off;
      }
      else {
         assert(false);
      }

      TrafficSpec.OnOffEvents.erase(first);
   }
   scheduleNextStatusChangeEvent(now);
   unlock();
}




void Flow::run()
{
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
         int timeout = pollTimeout(now, 2,
                                   now + 1000000,
                                   nextEvent);
         ext_poll(NULL, 0, timeout);
         now = getMicroTime();
      }

      // ====== Send outgoing data ==========================================
      if(OutputStatus == Flow::On) {
         // ====== Outgoing data (saturated sender) =========================
         if( (TrafficSpec.OutboundFrameSize > 0.0) &&
             (TrafficSpec.OutboundFrameRate <= 0.0000001) ) {
            result = (transmitFrame(this, now) > 0);
         }

         // ====== Outgoing data (non-saturated sender) =====================
         else if( (TrafficSpec.OutboundFrameSize >= 1.0) &&
                  (TrafficSpec.OutboundFrameRate > 0.0000001) ) {
            const unsigned long long lastEvent = LastTransmission;
            if(nextTransmission <= now) {
               do {
                  result = (transmitFrame(this, now) > 0);
                  if(now - lastEvent > 1000000) {
                     // Time gap of more than 1s -> do not try to correct
                     break;
                  }
               } while(scheduleNextTransmissionEvent() <= now);
            }
         }
      }

      // ====== Handle status changes =======================================
      if(nextStatusChange <= now) {
         handleStatusChangeEvent(now);
      }
   } while( (result == true) && (!isStopping()) );
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
      success = flow->initializeVectorFile(NULL, compressVectorFile);
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
   if(found != UnidentifiedSockets.end()) {
      UnidentifiedSockets.erase(found);
      UpdatedUnidentifiedSockets = true;
   }
   if(closeSocket) {
      FlowManager::getFlowManager()->getMessageReader()->deregisterSocket(socketDescriptor);
      ext_close(socketDescriptor);
   }
   unlock();
}




// ###### Reception thread function #########################################
void FlowManager::run()
{
   signal(SIGPIPE, SIG_IGN);

   do {
      // ====== Collect PollFDs for poll() ==================================
      lock();
      pollfd pollFDs[FlowSet.size() + UnidentifiedSockets.size()];
      size_t n = 0;
      for(size_t i = 0;i  < FlowSet.size();i++) {
         FlowSet[i]->lock();
         if( (FlowSet[i]->InputStatus != Flow::Off) &&
             (FlowSet[i]->SocketDescriptor >= 0) ) {
            pollFDs[n].fd      = FlowSet[i]->SocketDescriptor;
            pollFDs[n].events  = POLLIN;
            pollFDs[n].revents = 0;
            FlowSet[i]->PollFDEntry = &pollFDs[n];
            n++;
         }
         else {
            FlowSet[i]->PollFDEntry = NULL;
         }
         FlowSet[i]->unlock();
      }
      UpdatedUnidentifiedSockets = false;
      pollfd* unidentifiedSocketsPollFDIndex[UnidentifiedSockets.size()];
      size_t i = 0;
      for(std::map<int, int>::iterator iterator = UnidentifiedSockets.begin();
          iterator != UnidentifiedSockets.end(); iterator++) {
         pollFDs[n].fd      = iterator->first;
         pollFDs[n].events  = POLLIN;
         pollFDs[n].revents = 0;
         unidentifiedSocketsPollFDIndex[i] = &pollFDs[n];
         n++; i++;
      }
      unlock();

      
      // ====== Wait for events =============================================
      unsigned long long now = getMicroTime();
      const int timeout = pollTimeout(getMicroTime(), 2,
                                      now + 250000,
                                      getNextEvent());
      const int result = ext_poll((pollfd*)&pollFDs, n, timeout);

      
      // ====== Handle events ===============================================
      now = getMicroTime();
      if(result > 0) {
         lock();
         // ====== Handle read events of flows ==============================
         for(i = 0;i  < FlowSet.size();i++) {
            FlowSet[i]->lock();
            const pollfd* entry    = FlowSet[i]->PollFDEntry;
            const int     protocol = FlowSet[i]->getTrafficSpec().Protocol;
            FlowSet[i]->unlock();
            // NOTE: Release the lock here, because the FlowSet enty may belong
            //       to another stream of the socket. handleNetPerfMeterData()
            //       will find and lock the actual FlowSet entry!
            if( (entry) && (entry->revents & POLLIN) ) {
                // NOTE: FlowSet[i] may not be the actual Flow!
                //       It may be another stream of the same SCTP assoc!
                handleNetPerfMeterData(true, now, protocol, entry->fd);
            }
         }

         // ====== Handle read events of yet unidentified sockets ===========
         if(!UpdatedUnidentifiedSockets) {
            i = 0;
            for(std::map<int, int>::iterator iterator = UnidentifiedSockets.begin();
               iterator != UnidentifiedSockets.end(); iterator++) {
               assert(unidentifiedSocketsPollFDIndex[i]->fd == iterator->first);
               if(unidentifiedSocketsPollFDIndex[i]->revents & POLLIN) {
                  handleNetPerfMeterData(true, now,
                                         iterator->second,
                                         iterator->first);
                  if(UpdatedUnidentifiedSockets) {
                     // NOTE: The UnidentifiedSockets set may have changed in
                     // handleDataMessage -> ... -> FlowManager::identifySocket
                     // => stop processing if this is the case
                     break;
                  }
               }
               i++;
            }
         }
         unlock();
      }
      
      // ====== Handle statistics timer =====================================
      if(getNextEvent() <= now) {
         handleEvents(now);
      }
   } while(!isStopping());
}







// ###### Add measurement ###################################################
bool FlowManager::addMeasurement(Measurement* measurement)
{
   bool success = false;

   lock();
   if(!findMeasurement(measurement->MeasurementID)) {
      MeasurementSet.insert(std::pair<uint64_t, Measurement*>(measurement->MeasurementID,
                                                              measurement));
      success = true;
   }
   unlock();

   return(success);
}


// ###### Print measurements ################################################
void FlowManager::printMeasurements(std::ostream& os)
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
Measurement* FlowManager::findMeasurement(const uint64_t measurementID)
{
   std::map<uint64_t, Measurement*>::iterator found = MeasurementSet.find(measurementID);
   if(found != MeasurementSet.end()) {
      return(found->second);
   }
   return(NULL);
}


// ###### Remove measurement ################################################
void FlowManager::removeMeasurement(Measurement* measurement)
{
   lock();
   MeasurementSet.erase(measurement->MeasurementID);
   unlock();
}


unsigned long long FlowManager::getNextEvent()
{
   unsigned long long nextEvent = NextDisplayEvent;

   lock();
   for(std::map<uint64_t, Measurement*>::iterator iterator = MeasurementSet.begin();
       iterator != MeasurementSet.end(); iterator++) {
       const Measurement* measurement = iterator->second;
       nextEvent = std::min(nextEvent, measurement->NextStatisticsEvent);
   }
   unlock();

   return(nextEvent);
}


void FlowManager::handleEvents(const unsigned long long now)
{
   lock();
   
   // ====== Handle statistics events =======================================
   bool hasUpdate = false;
   for(std::map<uint64_t, Measurement*>::iterator iterator = MeasurementSet.begin();
       iterator != MeasurementSet.end(); iterator++) {
       Measurement* measurement = iterator->second;
       if(measurement->NextStatisticsEvent <= now) {
          measurement->writeVectorStatistics(now, GlobalStats, RelGlobalStats);
          hasUpdate = true;
       }
   }
   
   // ====== Display status =================================================
   if(NextDisplayEvent <= now) {
      // ====== Timer management ============================================
      NextDisplayEvent += DisplayInterval;
      if(NextDisplayEvent <= now) {   // Initialization!
         NextDisplayEvent = now + DisplayInterval;
      }
      if(FirstDisplayEvent == 0) {
         FirstDisplayEvent = now;
         LastDisplayEvent  = now;
      }
      
      // ====== Print bandwidth information line ============================
      if(DisplayOn) {
         const double       duration      = (now - LastDisplayEvent) / 1000000.0;
         const unsigned int totalDuration = (unsigned int)((now - FirstDisplayEvent) / 1000000ULL);
         
         // NOTE: ostream/cout has race condition problem according to helgrind.
         //       => simply using stdout instead.
         fprintf(stdout,
               "\r<-- Duration: %2u:%02u:%02u   "
               "Flows: %u   "
               "Transmitted: %1.2f MiB at %1.1f Kbit/s   "
               "Received: %1.2f MiB at %1.1f Kbit/s -->\x1b[0K",
               totalDuration / 3600,
               (totalDuration / 60) % 60,
               totalDuration % 60,
               (unsigned int)FlowSet.size(),
               GlobalStats.TransmittedBytes / (1024.0 * 1024.0),
               (duration > 0.0) ? (8 * RelGlobalStats.TransmittedBytes / (1000.0 * duration)) : 0.0,
               GlobalStats.ReceivedBytes / (1024.0 * 1024.0),
               (duration > 0.0) ? (8 * RelGlobalStats.ReceivedBytes / (1000.0 * duration)) : 0.0);
         fflush(stdout);
      }
      
      LastDisplayEvent = now;
      RelGlobalStats.reset();
   }

   unlock();
}


void Measurement::writeScalarStatistics(const unsigned long long now)
{
   lock();
   FlowManager::getFlowManager()->writeScalarStatistics(
      MeasurementID, now, ScalarFile,
      FirstStatisticsEvent);
   unlock();
}

void Measurement::writeVectorStatistics(const unsigned long long now,
                                        FlowBandwidthStats&      globalStats,
                                        FlowBandwidthStats&      relGlobalStats)
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

   // ====== Write statistics ===============================================
   FlowManager::getFlowManager()->writeVectorStatistics(
      MeasurementID, now, VectorFile,
      globalStats, relGlobalStats,
      FirstStatisticsEvent, LastStatisticsEvent);
   
   // ====== Update timing ==================================================   
   LastStatisticsEvent = now;

   unlock();
}






// ###### Destructor ########################################################
Measurement::Measurement()
{
   MeasurementID     = 0;
   FirstTransmission = 0;
   LastTransmission  = 0;
   FirstReception    = 0;
   LastReception     = 0;

   StatisticsInterval   = 1000000;
   FirstStatisticsEvent = 0;
   LastStatisticsEvent  = 0;
   NextStatisticsEvent  = 0;
}


// ###### Destructor ########################################################
Measurement::~Measurement()
{
   finish(true);
}


bool Measurement::initialize(const unsigned long long now,
                             const uint64_t           measurementID,
                             const char*              vectorNamePattern,
                             const bool               compressVectorFile,
                             const char*              scalarNamePattern,
                             const bool               compressScalarFile)
{
   MeasurementID        = measurementID;
   FirstStatisticsEvent = 0;
   LastStatisticsEvent  = 0;
   NextStatisticsEvent  = 0;

   if(FlowManager::getFlowManager()->addMeasurement(this)) {
      VectorNamePattern = (vectorNamePattern != NULL) ?
                             std::string(vectorNamePattern) : std::string(); 
      const bool s1 = VectorFile.initialize(
                         (vectorNamePattern != NULL) ?
                            Flow::getNodeOutputName(vectorNamePattern, "active").c_str() : NULL,
                         compressVectorFile);
      ScalarNamePattern = (scalarNamePattern != NULL) ?
                             std::string(scalarNamePattern) : std::string(); 
      const bool s2 = ScalarFile.initialize(
                         (scalarNamePattern != NULL) ?
                            Flow::getNodeOutputName(scalarNamePattern, "active").c_str() : NULL,
                         compressScalarFile);
      return(s1 && s2);
   }
   return(false);
}


bool Measurement::finish(const bool closeFiles)
{
   FlowManager::getFlowManager()->removeMeasurement(this);
   const bool s1 = VectorFile.finish(closeFiles);
   const bool s2 = ScalarFile.finish(closeFiles);
   return(s1 && s2);
}
