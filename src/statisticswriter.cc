/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2009 by Thomas Dreibholz
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

#include "statisticswriter.h"

#include <string.h>
#include <math.h>
#include <iostream>


// ###### Constructor #######################################################
StatisticsWriter::StatisticsWriter()
{
   StatisticsInterval          = 1000000;
   NextStatisticsEvent         = 0;
   FirstStatisticsEvent        = 0;
   LastStatisticsEvent         = 0;

   TotalTransmittedBytes       = 0;
   TotalTransmittedPackets     = 0;
   TotalTransmittedFrames      = 0;
   TotalReceivedBytes          = 0;
   TotalReceivedPackets        = 0;
   TotalReceivedFrames         = 0;
   TotalLostBytes              = 0;
   TotalLostPackets            = 0;
   TotalLostFrames             = 0;
   LastTotalTransmittedBytes   = 0;
   LastTotalTransmittedPackets = 0;
   LastTotalTransmittedFrames  = 0;
   LastTotalReceivedBytes      = 0;
   LastTotalReceivedPackets    = 0;
   LastTotalReceivedFrames     = 0;
   LastTotalLostBytes          = 0;
   LastTotalLostPackets        = 0;
   LastTotalLostFrames         = 0;

   VectorLine                  = 0;
   VectorName                  = NULL;
   VectorFile                  = NULL;
   VectorBZFile                = NULL;

   ScalarName                  = NULL;
   ScalarFile                  = NULL;
   ScalarBZFile                = NULL;
}


// ###### Destructor ########################################################
StatisticsWriter::~StatisticsWriter()
{
}


// ###### Create output file ################################################
bool StatisticsWriter::openOutputFile(const char* name,
                                      FILE**     fh,
                                      BZFILE**   bz)
{
   *bz = NULL;
   *fh = fopen(name, "w");
   if(*fh == NULL) {
      std::cerr << "ERROR: Unable to create output file " << name << "!" << std::endl;
      return(false);
   }
   if(strstr(name, ".bz2")) {
      int bzerror;
      *bz = BZ2_bzWriteOpen(&bzerror, *fh, 9, 0, 30);
      if(bzerror != BZ_OK) {
         std::cerr << "ERROR: Unable to initialize BZip2 compression on file " << name << "!" << std::endl
              << "Reason: " << BZ2_bzerror(*bz, &bzerror) << std::endl;
         BZ2_bzWriteClose(&bzerror, *bz, 0, NULL, NULL);
         fclose(*fh);
         *fh = NULL;
         unlink(name);
         return(false);
      }
   }
   return(true);
}


// ###### Close output file #################################################
bool StatisticsWriter::closeOutputFile(const char* name,
                                       FILE**     fh,
                                       BZFILE**   bz)
{
   bool result = true;
   if(*bz) {
      int bzerror;
      BZ2_bzWriteClose(&bzerror, *bz, 0, NULL, NULL);
      if(bzerror != BZ_OK) {
         std::cerr << "ERROR: Unable to finish BZip2 compression on file " << name << "!" << std::endl
              << "Reason: " << BZ2_bzerror(*bz, &bzerror) << std::endl;
         result = false;
      }
      *bz = NULL;
   }
   if(*fh) {
      fclose(*fh);
      *fh = NULL;
   }
   return(result);
}


// ###### Write string into output file #####################################
bool StatisticsWriter::writeString(const char* name,
                                   FILE*       fh,
                                   BZFILE*     bz,
                                   const char* str)
{
   if(bz) {
      int bzerror;
      BZ2_bzWrite(&bzerror, bz, (void*)str, strlen(str));
      if(bzerror != BZ_OK) {
         std::cerr << std::endl
               << "ERROR: libbz2 failed to write into file <" << name << ">!" << std::endl
               << "Reason: " << BZ2_bzerror(bz, &bzerror) << std::endl;
         return(false);
      }
   }
   else if(fh) {
      if(fputs(str, fh) <= 0) {
         std::cerr << "ERROR: Failed to write into file <" << name << ">!" << std::endl;
         return(false);
      }
   }
   return(true);
}


// ###### Write vector statistics ###########################################
bool StatisticsWriter::writeVectorStatistics(const unsigned long long now,
                                             std::vector<FlowSpec*>& flowSet)
{
   // ====== Timer management ===============================================
   NextStatisticsEvent += StatisticsInterval;
   if(NextStatisticsEvent <= now) {   // Initialization!
      NextStatisticsEvent = now + StatisticsInterval;
   }
   if(FirstStatisticsEvent == 0) {
      FirstStatisticsEvent = now;
      LastStatisticsEvent  = now;
   }


   // ====== Write vector statistics header =================================
   if(VectorLine == 0) {
      if(writeString(VectorName, VectorFile, VectorBZFile,
                     "AbsTime RelTime Interval\t"
                     "FlowID Description\t"
                     "AbsTransmittedBytes AbsTransmittedPackets AbsTransmittedFrames\t"
                     "RelTransmittedBytes RelTransmittedPackets RelTransmittedFrames\t"
                     "TransmittedByteRate TransmittedPacketRate TransmittedFrameRate\t"
                     "AbsReceivedBytes AbsReceivedPackets AbsReceivedFrames\t"
                     "RelReceivedBytes RelReceivedPackets ToralRelReceivedFrames\t"
                     "ReceivedByteRate ReceivedPacketRate ReceivedFrameRate\t"
                     "Jitter\t"
                     "AbsLostBytes AbsLostPackets AbsLostFrames\t"
                     "RelLostBytes RelLostPackets ToralRelLostFrames\t"
                     "LostByteRate LostPacketRate LostFrameRate\n") == false) {
         return(false);
      }
      VectorLine = 1;
   }

   // ====== Write flow statistics ==========================================
   char str[1024];
   const double duration = (now - LastStatisticsEvent) / 1000000.0;
   for(std::vector<FlowSpec*>::iterator iterator = flowSet.begin();iterator != flowSet.end();iterator++) {
      FlowSpec* flowSpec = (FlowSpec*)*iterator;

      const unsigned long long relTransmittedBytes   = flowSpec->TransmittedBytes   - flowSpec->LastTransmittedBytes;
      const unsigned long long relTransmittedPackets = flowSpec->TransmittedPackets - flowSpec->LastTransmittedPackets;
      const unsigned long long relTransmittedFrames  = flowSpec->TransmittedFrames  - flowSpec->LastTransmittedFrames;
      const double relTransmittedByteRate            = (duration > 0.0) ? relTransmittedBytes   / duration : 0.0;
      const double relTransmittedPacketRate          = (duration > 0.0) ? relTransmittedPackets / duration : 0.0;
      const double relTransmittedFrameRate           = (duration > 0.0) ? relTransmittedFrames  / duration : 0.0;
      const unsigned long long relReceivedBytes      = flowSpec->ReceivedBytes   - flowSpec->LastReceivedBytes;
      const unsigned long long relReceivedPackets    = flowSpec->ReceivedPackets - flowSpec->LastReceivedPackets;
      const unsigned long long relReceivedFrames     = flowSpec->ReceivedFrames  - flowSpec->LastReceivedFrames;
      const double relReceivedByteRate               = (duration > 0.0) ? relReceivedBytes   / duration : 0.0;
      const double relReceivedPacketRate             = (duration > 0.0) ? relReceivedPackets / duration : 0.0;
      const double relReceivedFrameRate              = (duration > 0.0) ? relReceivedFrames  / duration : 0.0;
      const unsigned long long relLostBytes          = flowSpec->LostBytes   - flowSpec->LastLostBytes;
      const unsigned long long relLostPackets        = flowSpec->LostPackets - flowSpec->LastLostPackets;
      const unsigned long long relLostFrames         = flowSpec->LostFrames  - flowSpec->LastLostFrames;
      const double relLostByteRate                   = (duration > 0.0) ? relLostBytes   / duration : 0.0;
      const double relLostPacketRate                 = (duration > 0.0) ? relLostPackets / duration : 0.0;
      const double relLostFrameRate                  = (duration > 0.0) ? relLostFrames  / duration : 0.0;

      snprintf((char*)&str, sizeof(str),
               "%06llu %llu %1.6f %1.6f\t"
               "%u \"%s\"\t"
                  "%llu %llu %llu\t"
                    "%llu %llu %llu\t"
                    "%1.6f %1.6f %1.6f\t"
                  "%llu %llu %llu\t"
                    "%llu %llu %llu\t"
                    "%1.6f %1.6f %1.6f\t"
                  "%1.3f\t"
                  "%llu %llu %llu\t"
                    "%llu %llu %llu\t"
                    "%1.6f %1.6f %1.6f\n",

               VectorLine++, now, (double)(now - FirstStatisticsEvent) / 1000000.0, duration,
               flowSpec->FlowID, flowSpec->Description.c_str(),
               
               flowSpec->TransmittedBytes, flowSpec->TransmittedPackets, flowSpec->TransmittedFrames,
               relTransmittedBytes, relTransmittedPackets, relTransmittedFrames,
               relTransmittedByteRate, relTransmittedPacketRate, relTransmittedFrameRate,

               flowSpec->ReceivedBytes, flowSpec->ReceivedPackets, flowSpec->ReceivedFrames,
               relReceivedBytes, relReceivedPackets, relReceivedFrames,
               relReceivedByteRate, relReceivedPacketRate, relReceivedFrameRate,

               flowSpec->Jitter,
                
               flowSpec->LostBytes, flowSpec->LostPackets, flowSpec->LostFrames,
               relLostBytes, relLostPackets, relLostFrames,
               relLostByteRate, relLostPacketRate, relLostFrameRate);
               
      if(writeString(VectorName, VectorFile, VectorBZFile, str) == false) {
         return(false);
      }

      flowSpec->LastTransmittedBytes   = flowSpec->TransmittedBytes;
      flowSpec->LastTransmittedPackets = flowSpec->TransmittedPackets;
      flowSpec->LastTransmittedFrames  = flowSpec->TransmittedFrames;
      flowSpec->LastReceivedBytes      = flowSpec->ReceivedBytes;
      flowSpec->LastReceivedPackets    = flowSpec->ReceivedPackets;
      flowSpec->LastReceivedFrames     = flowSpec->ReceivedFrames;
   }


   // ====== Get total flow statistics  =====================================
   const unsigned long long relTotalTransmittedBytes   = TotalTransmittedBytes   - LastTotalTransmittedBytes;
   const unsigned long long relTotalTransmittedPackets = TotalTransmittedPackets - LastTotalTransmittedPackets;
   const unsigned long long relTotalTransmittedFrames  = TotalTransmittedFrames  - LastTotalTransmittedFrames;
   const double relTotalTransmittedByteRate            = (duration > 0.0) ? relTotalTransmittedBytes   / duration : 0.0;
   const double relTotalTransmittedPacketRate          = (duration > 0.0) ? relTotalTransmittedPackets / duration : 0.0;
   const double relTotalTransmittedFrameRate           = (duration > 0.0) ? relTotalTransmittedFrames  / duration : 0.0;
   const unsigned long long relTotalReceivedBytes      = TotalReceivedBytes   - LastTotalReceivedBytes;
   const unsigned long long relTotalReceivedPackets    = TotalReceivedPackets - LastTotalReceivedPackets;
   const unsigned long long relTotalReceivedFrames     = TotalReceivedFrames  - LastTotalReceivedFrames;
   const double relTotalReceivedByteRate               = (duration > 0.0) ? relTotalReceivedBytes   / duration : 0.0;
   const double relTotalReceivedPacketRate             = (duration > 0.0) ? relTotalReceivedPackets / duration : 0.0;
   const double relTotalReceivedFrameRate              = (duration > 0.0) ? relTotalReceivedFrames  / duration : 0.0;
   const unsigned long long relTotalLostBytes          = TotalLostBytes   - LastTotalLostBytes;
   const unsigned long long relTotalLostPackets        = TotalLostPackets - LastTotalLostPackets;
   const unsigned long long relTotalLostFrames         = TotalLostFrames  - LastTotalLostFrames;
   const double relTotalLostByteRate                   = (duration > 0.0) ? relTotalLostBytes   / duration : 0.0;
   const double relTotalLostPacketRate                 = (duration > 0.0) ? relTotalLostPackets / duration : 0.0;
   const double relTotalLostFrameRate                  = (duration > 0.0) ? relTotalLostFrames  / duration : 0.0;

   snprintf((char*)&str, sizeof(str),
            "%06llu %llu %1.6f %1.6f\t"
            "-1 \"Total\" \t"
               "%llu %llu %llu\t"
                  "%llu %llu %llu\t"
                  "%1.6f %1.6f %1.6f\t"
               "%llu %llu %llu\t"
                  "%llu %llu %llu\t"
                  "%1.6f %1.6f %1.6f\t"
               "-1.0\t"
               "%llu %llu %llu\t"
                  "%llu %llu %llu\t"
                  "%1.6f %1.6f %1.6f\n",

            VectorLine++, now, (double)(now - FirstStatisticsEvent) / 1000000.0, duration,

            TotalTransmittedBytes, TotalTransmittedPackets, TotalTransmittedFrames,
            relTotalTransmittedBytes, relTotalTransmittedPackets, relTotalTransmittedFrames,
            relTotalTransmittedByteRate, relTotalTransmittedPacketRate, relTotalTransmittedFrameRate,

            TotalReceivedBytes, TotalReceivedPackets, TotalReceivedFrames,
            relTotalReceivedBytes, relTotalReceivedPackets, relTotalReceivedFrames,
            relTotalReceivedByteRate, relTotalReceivedPacketRate, relTotalReceivedFrameRate,

            TotalLostBytes, TotalLostPackets, TotalLostFrames,
            relTotalLostBytes, relTotalLostPackets, relTotalLostFrames,
            relTotalLostByteRate, relTotalLostPacketRate, relTotalLostFrameRate);
   if(writeString(VectorName, VectorFile, VectorBZFile, str) == false) {
      return(false);
   }

   LastStatisticsEvent         = now;
   LastTotalTransmittedBytes   = TotalTransmittedBytes;
   LastTotalTransmittedPackets = TotalTransmittedPackets;
   LastTotalTransmittedFrames  = TotalTransmittedFrames;
   LastTotalReceivedBytes      = TotalReceivedBytes;
   LastTotalReceivedPackets    = TotalReceivedPackets;
   LastTotalReceivedFrames     = TotalReceivedFrames;
   LastTotalLostBytes          = TotalLostBytes;
   LastTotalLostPackets        = TotalLostPackets;
   LastTotalLostFrames         = TotalLostFrames;


   // ====== Print bandwidth information line ===============================
   const unsigned int totalDuration = (unsigned int)((now - FirstStatisticsEvent) / 1000000ULL);
   printf("\r<-- Duration: %2u:%02u:%02u   Flows: %u   Transmitted: %1.2f MiB at %1.1f Kbit/s   Received: %1.2f MiB at %1.1f Kbit/s -->\x1b[0K",
           totalDuration / 3600,
           (totalDuration / 60) % 60,
           totalDuration % 60,
           (unsigned int)flowSet.size(),
           TotalTransmittedBytes / (1024.0 * 1024.0),
           (duration > 0.0) ? (8 * relTotalTransmittedBytes / (1000.0 * duration)) : 0.0,
           TotalReceivedBytes / (1024.0 * 1024.0),
           (duration > 0.0) ? (8 * relTotalReceivedBytes / (1000.0 * duration)) : 0.0);
   fflush(stdout);

   return(true);
}


// ###### Write scalar statistics ###########################################
bool StatisticsWriter::writeScalarStatistics(const unsigned long long now,
                                             std::vector<FlowSpec*>&  flowSet)
{
   const char* objectName = "netMeter";
   char        str[4096];

   // ====== Write flow statistics ==========================================
   for(std::vector<FlowSpec*>::iterator iterator = flowSet.begin();iterator != flowSet.end();iterator++) {
      const FlowSpec* flowSpec = (const FlowSpec*)*iterator;

      const double transmissionDuration = (flowSpec->LastTransmission - flowSpec->FirstTransmission) / 1000000.0;
      const double receptionDuration    = (flowSpec->LastReception - flowSpec->FirstReception) / 1000000.0;

      snprintf((char*)&str, sizeof(str),
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
               objectName, flowSpec->FlowID, flowSpec->TransmittedBytes,
               objectName, flowSpec->FlowID, flowSpec->TransmittedPackets,
               objectName, flowSpec->FlowID, flowSpec->TransmittedFrames,
               objectName, flowSpec->FlowID, (transmissionDuration > 0.0) ? flowSpec->TransmittedBytes   / transmissionDuration : 0.0,
               objectName, flowSpec->FlowID, (transmissionDuration > 0.0) ? flowSpec->TransmittedPackets / transmissionDuration : 0.0,
               objectName, flowSpec->FlowID, (transmissionDuration > 0.0) ? flowSpec->TransmittedFrames  / transmissionDuration : 0.0,
               objectName, flowSpec->FlowID, flowSpec->ReceivedBytes,
               objectName, flowSpec->FlowID, flowSpec->ReceivedPackets,
               objectName, flowSpec->FlowID, flowSpec->ReceivedFrames,
               objectName, flowSpec->FlowID, (receptionDuration > 0.0) ? flowSpec->ReceivedBytes   / receptionDuration : 0.0,
               objectName, flowSpec->FlowID, (receptionDuration > 0.0) ? flowSpec->ReceivedPackets / receptionDuration : 0.0,
               objectName, flowSpec->FlowID, (receptionDuration > 0.0) ? flowSpec->ReceivedFrames  / receptionDuration : 0.0
               );
      if(writeString(ScalarName, ScalarFile, ScalarBZFile, str) == false) {
         return(false);
      }
   }

   // ====== Write total statistics =========================================
   const double totalDuration = (now - FirstStatisticsEvent) / 1000000.0;
   snprintf((char*)&str, sizeof(str),
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
            objectName, TotalTransmittedBytes,
            objectName, TotalTransmittedPackets,
            objectName, TotalTransmittedFrames,
            objectName, (totalDuration > 0.0) ? TotalTransmittedBytes   / totalDuration : 0.0,
            objectName, (totalDuration > 0.0) ? TotalTransmittedPackets / totalDuration : 0.0,
            objectName, (totalDuration > 0.0) ? TotalTransmittedFrames  / totalDuration : 0.0,
             
            objectName, TotalReceivedBytes,
            objectName, TotalReceivedPackets,
            objectName, TotalReceivedFrames,
            objectName, (totalDuration > 0.0) ? TotalReceivedBytes   / totalDuration : 0.0,
            objectName, (totalDuration > 0.0) ? TotalReceivedPackets / totalDuration : 0.0,
            objectName, (totalDuration > 0.0) ? TotalReceivedFrames  / totalDuration : 0.0,

            objectName, TotalLostBytes,
            objectName, TotalLostPackets,
            objectName, TotalLostFrames,
            objectName, (totalDuration > 0.0) ? TotalLostBytes   / totalDuration : 0.0,
            objectName, (totalDuration > 0.0) ? TotalLostPackets / totalDuration : 0.0,
            objectName, (totalDuration > 0.0) ? TotalLostFrames  / totalDuration : 0.0
            );
   if(writeString(ScalarName, ScalarFile, ScalarBZFile, str) == false) {
      return(false);
   }

   return(true);
}
