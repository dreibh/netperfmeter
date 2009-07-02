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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact: dreibh@iem.uni-due.de
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <math.h>
#include <assert.h>
#include <bzlib.h>
#include <netinet/in.h>

#include <iostream>
#include <vector>
#include <set>

#include "tools.h"
#include "flowspec.h"
#include "netperfmeterpackets.h"
#include "control.h"
#include "transfer.h"
#include "statisticswriter.h"

#include "t3.cc"


#ifndef HAVE_DCCP
#warning DCCP is not supported by the API of this system!
#endif


using namespace std;


// ????????????
// vector<FlowSpec*> gFlowSet;
set<int>          gConnectedSocketsSet;
const char*       gActiveNodeName  = "Client";
const char*       gPassiveNodeName = "Server";
int               gControlSocket   = -1;
int               gTCPSocket       = -1;
int               gUDPSocket       = -1;
int               gSCTPSocket      = -1;
int               gDCCPSocket      = -1;
size_t            gMaxMsgSize      = 16000;
double            gRuntime         = -1.0;
bool              gStopTimeReached = false;
MessageReader     gMessageReader;



// ###### Handle global command-line parameter ##############################
bool handleGlobalParameter(const char* parameter)
{
   if(strncmp(parameter, "-maxmsgsize=", 12) == 0) {
      gMaxMsgSize = atol((const char*)&parameter[12]);
      if(gMaxMsgSize > 65536) {
         gMaxMsgSize = 65536;
      }
      else if(gMaxMsgSize < 128) {
         gMaxMsgSize = 128;
      }
      return(true);
   }
   else if(strncmp(parameter, "-runtime=", 9) == 0) {
      gRuntime = atof((const char*)&parameter[9]);
      return(true);
   }
   else if(strncmp(parameter, "-activenodename=", 16) == 0) {
      gActiveNodeName = (const char*)&parameter[16];
   }
   else if(strncmp(parameter, "-passivenodename=", 17) == 0) {
      gPassiveNodeName = (const char*)&parameter[17];
   }
   return(false);
}


// ###### Print global parameter settings ###################################
void printGlobalParameters()
{
   std::cout << "Global Parameters:" << std::endl
             << "   - Runtime           = ";
   if(gRuntime >= 0.0) {
      std::cout << gRuntime << "s" << std::endl;
   }
   else {
      std::cout << "until manual stop" << std::endl;
   }
   std::cout << "   - Active Node Name  = " << gActiveNodeName  << std::endl
             << "   - Passive Node Name = " << gPassiveNodeName << std::endl
             << "   - Max Message Size  = " << gMaxMsgSize      << std::endl
             << std::endl;
}


// ###### Read random number parameter ######################################
static const char* parseNextEntry(const char* parameters,
                                  double*     value,
                                  uint8_t*    rng)
{
   int n = 0;
   if(sscanf(parameters, "exp%lf%n", value, &n) == 1) {
      *rng = RANDOM_EXPONENTIAL;
   }
   else if(sscanf(parameters, "const%lf%n", value, &n) == 1) {
      *rng = RANDOM_CONSTANT;
   }
   else {
      cerr << "ERROR: Invalid parameters " << parameters << endl;
      exit(1);
   }
   if(parameters[n] == 0x00) {
      return(NULL);
   }
   return((const char*)&parameters[n + 1]);
}


// ###### Read flow option ##################################################
static const char* parseTrafficSpecOption(const char*  parameters,
                                          TrafficSpec& trafficSpec)
{
   char   description[256];
   int    n     = 0;
   double value = 0.0;

   if(sscanf(parameters, "unordered=%lf%n", &value, &n) == 1) {
      if((value < 0.0) || (value > 1.0)) {
         cerr << "ERROR: Bad probability for \"unordered\" option in " << parameters << "!" << endl;
         exit(1);
      }
      trafficSpec.OrderedMode = value;
   }
   else if(sscanf(parameters, "unreliable=%lf%n", &value, &n) == 1) {
      if((value < 0.0) || (value > 1.0)) {
         cerr << "ERROR: Bad probability for \"unreliable\" option in " << parameters << "!" << endl;
         exit(1);
      }
      trafficSpec.ReliableMode = value;
   }
   else if(sscanf(parameters, "description=%255[^:]s%n", (char*)&description, &n) == 1) {
      trafficSpec.Description = std::string(description);
      n = 12 + strlen(description);
   }
   else if(strncmp(parameters,"onoff=", 6) == 0) {
      unsigned int lastEvent = 0;
      size_t       m         = 5;
      while( (parameters[m] != 0x00) && (parameters[m] != ':') ) {
         m++;
         double       value;
         unsigned int onoff;
         if(sscanf((const char*)&parameters[m], "+%lf%n", &value, &n) == 1) {
            // Relative time
            onoff = (unsigned int)rint(1000.0 * value);
            onoff += lastEvent;
         }
         else if(sscanf((const char*)&parameters[m], "%lf%n", &value, &n) == 1) {
            // Absolute time
            onoff = (unsigned int)rint(1000.0 * value);
         }
         else {
            cerr << "ERROR: Invalid on/off list at " << (const char*)&parameters[m] << "!" << std::endl;
            exit(1);
         }
         trafficSpec.OnOffEvents.insert(onoff);
         lastEvent = onoff;
         m += n;
      }
      n = m;
   }
   else {
      cerr << "ERROR: Invalid options " << parameters << "!" << endl;
      exit(1);
   }
   if(parameters[n] == 0x00) {
      return(NULL);
   }
   return((const char*)&parameters[n + 1]);
}


// ###### Create FlowSpec for new flow ######################################
static Flow* createFlow(Flow*           previousFlow,
                        const char*     parameters,
                        const uint64_t  measurementID,
                        const uint8_t   protocol,
                        const sockaddr* remoteAddress)
{
   // ====== Get flow ID and stream ID ======================================
   static uint32_t flowID    = 0; // will be increased with each successfull call
   uint16_t         streamID = (previousFlow != NULL) ? previousFlow->getStreamID() + 1 : 0;

   // ====== Get TrafficSpec ================================================
   TrafficSpec trafficSpec;
   trafficSpec.Description = format("Flow %u", flowID);
   trafficSpec.Protocol    = protocol;
   parameters = parseNextEntry(parameters, &trafficSpec.OutboundFrameRate, &trafficSpec.OutboundFrameRateRng);
   if(parameters) {
      parameters = parseNextEntry(parameters, &trafficSpec.OutboundFrameSize, &trafficSpec.OutboundFrameSizeRng);
      if(parameters) {
         parameters = parseNextEntry(parameters, &trafficSpec.InboundFrameRate, &trafficSpec.InboundFrameRateRng);
         if(parameters) {
            parameters = parseNextEntry(parameters, &trafficSpec.InboundFrameSize, &trafficSpec.InboundFrameSizeRng);
            if(parameters) {
               while( (parameters = parseTrafficSpecOption(parameters, trafficSpec)) ) {
               }
            }
         }
      }
   }

   // ====== Create new flow ================================================
   Flow* flow = new Flow(measurementID, flowID, streamID, trafficSpec);
   assert(flow != NULL);

   // ====== Set up socket ==================================================
   int  socketDescriptor;
   bool originalSocketDescriptor;

   if(previousFlow) {
      originalSocketDescriptor = false;
      socketDescriptor         = previousFlow->getSocketDescriptor();
      cout << "      - Connection okay; sd=" << socketDescriptor << endl;
   }
   else {
      originalSocketDescriptor = true;
      socketDescriptor          = -1;
      switch(protocol) {
         case IPPROTO_SCTP:
            socketDescriptor = ext_socket(remoteAddress->sa_family, SOCK_STREAM, IPPROTO_SCTP);
           break;
         case IPPROTO_TCP:
            socketDescriptor = ext_socket(remoteAddress->sa_family, SOCK_STREAM, IPPROTO_TCP);
           break;
         case IPPROTO_UDP:
            socketDescriptor = ext_socket(remoteAddress->sa_family, SOCK_DGRAM, IPPROTO_UDP);
           break;
#ifdef HAVE_DCCP
         case IPPROTO_DCCP:
            socketDescriptor = ext_socket(remoteAddress->sa_family, SOCK_DCCP, IPPROTO_DCCP);
           break;
#endif
      }
      flow->print(cout);
      if(socketDescriptor < 0) {
         cerr << "ERROR: Unable to create " << getProtocolName(protocol)
              << " socket - " << strerror(errno) << "!" << endl;
         exit(1);
      }

      // ====== Establish connection ========================================
      cout << "      - Connecting " << getProtocolName(protocol) << " socket to ";
      printAddress(cout, remoteAddress, true);
      cout << " ... ";
      cout.flush();

      if(protocol == IPPROTO_SCTP) {
         sctp_event_subscribe events;
         memset((char*)&events, 0 ,sizeof(events));
         events.sctp_data_io_event = 1;
         if(ext_setsockopt(socketDescriptor, IPPROTO_SCTP, SCTP_EVENTS,
                           &events, sizeof(events)) < 0) {
            cerr << "ERROR: Failed to configure events on SCTP socket - "
                 << strerror(errno) << "!" << endl;
            exit(1);
         }
      }
      if(ext_connect(socketDescriptor, remoteAddress, getSocklen(remoteAddress)) < 0) {
         cerr << "ERROR: Unable to connect " << getProtocolName(protocol)
              << " socket - " << strerror(errno) << "!" << endl;
         exit(1);
      }
      cout << "okay; sd=" << socketDescriptor << endl;

      // ====== Update flow with socket descriptor ==========================
      flow->setSocketDescriptor(socketDescriptor, originalSocketDescriptor);
      //gMessageReader.registerSocket(flow->Protocol, socketDescriptor);   /// ?????????????!!!
   }

   flowID++;
   return(flow);
}



// ###### Main loop #########################################################
bool mainLoop(const bool               isActiveMode,
              const unsigned long long stopAt,
              const uint64_t           measurementID)
{
/*
   int                    tcpConnectionIDs[gConnectedSocketsSet.size()];
   pollfd                 fds[5 + gFlowSet.size() + gConnectedSocketsSet.size()];
   int                    n                     = 0;
   int                    controlID             = -1;
   int                    tcpID                 = -1;
   int                    udpID                 = -1;
   int                    sctpID                = -1;
   int                    dccpID                = -1;
   unsigned long long     nextStatusChangeEvent = (1ULL << 63);
   unsigned long long     nextTransmissionEvent = (1ULL << 63);
   unsigned long long     now                   = getMicroTime();
   StatisticsWriter*      statisticsWriter      = StatisticsWriter::getStatisticsWriter();
   std::map<int, pollfd*> pollFDIndex;
   static size_t          flowRoundRobin        = 0;


   // ====== Get parameters for poll() ======================================
   for(set<int>::iterator iterator = gConnectedSocketsSet.begin();iterator != gConnectedSocketsSet.end();iterator++) {
      fds[n].fd      = *iterator;
      fds[n].events  = POLLIN;
      fds[n].revents = 0;
      n++;
   }
   if(gControlSocket >= 0) {
      fds[n].fd      = gControlSocket;
      fds[n].events  = POLLIN;
      fds[n].revents = 0;
      controlID      = n;
      n++;
   }
   if(gTCPSocket >= 0) {
      fds[n].fd      = gTCPSocket;
      fds[n].events  = POLLIN;
      fds[n].revents = 0;
      tcpID          = n;
      n++;
   }
   if(gUDPSocket >= 0) {
      fds[n].fd      = gUDPSocket;
      fds[n].events  = POLLIN;
      fds[n].revents = 0;
      udpID          = n;
      n++;
   }
   if(gSCTPSocket >= 0) {
      fds[n].fd      = gSCTPSocket;
      fds[n].events  = POLLIN;
      fds[n].revents = 0;
      sctpID         = n;
      n++;
   }
   if(gDCCPSocket >= 0) {
      fds[n].fd      = gDCCPSocket;
      fds[n].events  = POLLIN;
      fds[n].revents = 0;
      dccpID         = n;
      n++;
   }
   for(vector<FlowSpec*>::iterator iterator = gFlowSet.begin();iterator != gFlowSet.end();iterator++) {
      FlowSpec* flowSpec = *iterator;
      if(flowSpec->SocketDescriptor >= 0) {
         // ====== Find or create PollFD entry ==============================
         pollfd*                          pfd; 
         std::map<int, pollfd*>::iterator found =
            pollFDIndex.find(flowSpec->SocketDescriptor);
         if(found != pollFDIndex.end()) {
            pfd = found->second;
         }
         else {
            assert(n < sizeof(fds) / sizeof(pollfd));
            pfd = &fds[n];
            n++;
            pfd->fd      = flowSpec->SocketDescriptor;
            pfd->events  = POLLIN;   // Always set POLLIN
            pfd->revents = 0;
            pollFDIndex.insert(std::pair<int, pollfd*>(flowSpec->SocketDescriptor, pfd));
         }

         // ====== Saturated sender: set POLLOUT ============================
         if( (flowSpec->Status == FlowSpec::On) &&
             (flowSpec->OutboundFrameSize > 0.0) &&
             (flowSpec->OutboundFrameRate <= 0.0000001) ) {
            pfd->events |= POLLOUT;
         }

         // ====== Schedule next status change event ========================
         flowSpec->scheduleNextStatusChangeEvent(now);
         if(flowSpec->NextStatusChangeEvent < nextStatusChangeEvent) {
            nextStatusChangeEvent = flowSpec->NextStatusChangeEvent;
         }

         // ====== Schedule next transmission event =========================
         if( (flowSpec->Status == FlowSpec::On) &&
             (flowSpec->OutboundFrameSize > 0.0) && 
             (flowSpec->OutboundFrameRate > 0.0000001) ) {
            flowSpec->scheduleNextTransmissionEvent();
            if(flowSpec->NextTransmissionEvent < nextTransmissionEvent) {
               nextTransmissionEvent = flowSpec->NextTransmissionEvent;
            }
         }
      }
   }


   // ====== Use poll() to wait for events ==================================
   const long long nextEvent = (long long)std::min(std::min(nextStatusChangeEvent, nextTransmissionEvent),
                                                   std::min(stopAt, statisticsWriter->getNextEvent()));
   const int timeout         = (int)(std::max(0LL, nextEvent - (long long)now) / 1000LL);

   // printf("to=%d   txNext=%lld\n", timeout, nextEvent - (long long)now);
   const int result = ext_poll((pollfd*)&fds, n, timeout);
// ???    printf("result=%d\n");
   now = getMicroTime();   // Get current time.

   // ====== Handle socket events ===========================================
   if(result >= 0) {

      // ====== Incoming control message ====================================
      if( (controlID >= 0) && (fds[controlID].revents & POLLIN) ) {
         const bool controlOkay = handleControlMessage(&gMessageReader, gFlowSet, gControlSocket);
         if((!controlOkay) && (isActiveMode)) {
            return(false);
         }
      }

      // ====== Incoming data message =======================================
      if( (sctpID >= 0) && (fds[sctpID].revents & POLLIN) ) {
         handleDataMessage(isActiveMode, &gMessageReader, statisticsWriter, gFlowSet,
                           now, gSCTPSocket, IPPROTO_SCTP, gControlSocket);
      }
      if( (udpID >= 0) && (fds[udpID].revents & POLLIN) ) {
         handleDataMessage(isActiveMode, &gMessageReader, statisticsWriter, gFlowSet,
                           now, gUDPSocket, IPPROTO_UDP, gControlSocket);
      }
      bool gConnectedSocketsSetUpdated = false;
      if( (tcpID >= 0) && (fds[tcpID].revents & POLLIN) ) {
         const int newSD = ext_accept(gTCPSocket, NULL, 0);
         if(newSD >= 0) {
            gMessageReader.registerSocket(IPPROTO_TCP, newSD);
            gConnectedSocketsSet.insert(newSD);
            gConnectedSocketsSetUpdated = true;
         }
      }
#ifdef HAVE_DCCP
      if( (dccpID >= 0) && (fds[dccpID].revents & POLLIN) ) {
         const int newSD = ext_accept(gDCCPSocket, NULL, 0);
         if(newSD >= 0) {
            gMessageReader.registerSocket(IPPROTO_DCCP, newSD);
            gConnectedSocketsSet.insert(newSD);
            gConnectedSocketsSetUpdated = true;
         }
      }
#endif


      // ====== Incoming data on connected sockets ==========================
      if(!gConnectedSocketsSetUpdated) {
         for(int i = 0;i < gConnectedSocketsSet.size();i++) {
            if(fds[i].revents & POLLIN) {
               const ssize_t result =
                  handleDataMessage(isActiveMode, &gMessageReader, statisticsWriter, gFlowSet,
                                    now, fds[i].fd, IPPROTO_TCP, gControlSocket);
               if( (result < 0) && (result != MRRM_PARTIAL_READ) ) {
                  gMessageReader.deregisterSocket(fds[i].fd);
                  gConnectedSocketsSet.erase(fds[i].fd);
                  ext_close(fds[i].fd);
               }
            }
         }
      }


      // ====== Outgoing flow events ========================================
      const size_t flows = gFlowSet.size();
      for(size_t i = 0;i < flows;i++) {
         FlowSpec* flowSpec = gFlowSet[(i + flowRoundRobin) % flows];

         if(flowSpec->SocketDescriptor >= 0) {
            // ====== Find PollFD entry =====================================
            std::map<int, pollfd*>::iterator found =
               pollFDIndex.find(flowSpec->SocketDescriptor);
            if(found == pollFDIndex.end()) {
               continue;
            }
            const pollfd* pfd = found->second;

            // ====== Incoming data =========================================
            if(pfd->revents & POLLIN) {
               handleDataMessage(isActiveMode, &gMessageReader, statisticsWriter, gFlowSet,
                                 now, pfd->fd, flowSpec->Protocol, gControlSocket);
            }

            // ====== Status change =========================================
            if(flowSpec->NextStatusChangeEvent <= now) {
               flowSpec->statusChangeEvent(now);
            }

            // ====== Send outgoing data ====================================
            if(flowSpec->Status == FlowSpec::On) {
               // ====== Outgoing data (saturated sender) ===================
               if( (flowSpec->OutboundFrameSize > 0.0) &&
                   (flowSpec->OutboundFrameRate <= 0.0000001) ) {
                  if(pfd->revents & POLLOUT) {
// ??? printf("   TRY: %d   %04x\n",(i + flowRoundRobin) % flows,   pfd->revents);
                     transmitFrame(statisticsWriter, flowSpec, now, gMaxMsgSize);
                  }
               }

               // ====== Outgoing data (non-saturated sender) ===============
               if( (flowSpec->OutboundFrameSize >= 1.0) &&
                   (flowSpec->OutboundFrameRate > 0.0000001) ) {
                  const unsigned long long lastEvent = flowSpec->LastTransmission;
                  if(flowSpec->NextTransmissionEvent <= now) {
                     do {
                        transmitFrame(statisticsWriter, flowSpec, now, gMaxMsgSize);
                        if(now - lastEvent > 1000000) {
                           // Time gap of more than 1s -> do not try to correct
                           break;
                        }
                        flowSpec->scheduleNextTransmissionEvent();
                     } while(flowSpec->NextTransmissionEvent <= now);
                  }
               }
            }
         }
      }


      // ====== Stop-time reached ===========================================
      if(now >= stopAt) {
         gStopTimeReached = true;
      }
   }

   // ====== Handle statistics timer ========================================
   if(statisticsWriter->getNextEvent() <= now) {
      statisticsWriter->writeAllVectorStatistics(now, gFlowSet, measurementID);
   }

   // ====== Ensure round-robin handling of streams over the same assoc =====
   flowRoundRobin++;
   if(flowRoundRobin >= gFlowSet.size()) {
      flowRoundRobin = 0;
   }
*/
   return(true);
}



// ###### Passive Mode ######################################################
void passiveMode(int argc, char** argv, const uint16_t localPort)
{
/*
   // ====== Initialize control socket ======================================
   gControlSocket = createAndBindSocket(SOCK_SEQPACKET, IPPROTO_SCTP, localPort + 1, true);   // Leave it blocking!
   if(gControlSocket < 0) {
      cerr << "ERROR: Failed to create and bind SCTP socket for control port - "
           << strerror(errno) << "!" << endl;
      exit(1);
   }
   sctp_event_subscribe events;
   memset((char*)&events, 0 ,sizeof(events));
   events.sctp_data_io_event     = 1;
   events.sctp_association_event = 1;
   if(ext_setsockopt(gControlSocket, IPPROTO_SCTP, SCTP_EVENTS, &events, sizeof(events)) < 0) {
      cerr << "ERROR: Failed to configure events on control socket - "
           << strerror(errno) << "!" << endl;
      exit(1);
   }
   gMessageReader.registerSocket(IPPROTO_SCTP, gControlSocket);

   // ====== Initialize data socket for each protocol =======================
   gTCPSocket = createAndBindSocket(SOCK_STREAM, IPPROTO_TCP, localPort);
   if(gTCPSocket < 0) {
      cerr << "ERROR: Failed to create and bind TCP socket - "
           << strerror(errno) << "!" << endl;
      exit(1);
   }
   gUDPSocket = createAndBindSocket(SOCK_DGRAM, IPPROTO_UDP, localPort);
   if(gUDPSocket < 0) {
      cerr << "ERROR: Failed to create and bind UDP socket - "
           << strerror(errno) << "!" << endl;
      exit(1);
   }
   gMessageReader.registerSocket(IPPROTO_UDP, gUDPSocket);
#ifdef HAVE_DCCP
   gDCCPSocket = createAndBindSocket(SOCK_DCCP, IPPROTO_DCCP, localPort);
   if(gDCCPSocket < 0) {
      cerr << "NOTE: Your kernel does not provide DCCP support." << endl;
   }
#endif
   gSCTPSocket = createAndBindSocket(SOCK_SEQPACKET, IPPROTO_SCTP, localPort);
   if(gSCTPSocket < 0) {
      cerr << "ERROR: Failed to create and bind SCTP socket - "
           << strerror(errno) << "!" << endl;
      exit(1);
   }
   memset((char*)&events, 0 ,sizeof(events));
   events.sctp_data_io_event = 1;
   if(ext_setsockopt(gSCTPSocket, IPPROTO_SCTP, SCTP_EVENTS, &events, sizeof(events)) < 0) {
      cerr << "ERROR: Failed to configure events on SCTP socket - "
           << strerror(errno) << "!" << endl;
      exit(1);
   }
   gMessageReader.registerSocket(IPPROTO_SCTP, gSCTPSocket);


   // ====== Set options ====================================================
   for(int i = 2;i < argc;i++) {
      if(!handleGlobalParameter(argv[i])) {
         std::cerr << "Invalid argument: " << argv[i] << "!" << std::endl;
         exit(1);
      }
   }
   printGlobalParameters();

   // ====== Print status ===================================================
   cout << "Passive Mode: Accepting TCP/UDP/SCTP" << ((gDCCPSocket > 0) ? "/DCCP" : "")
        << " connections on port " << localPort << endl << endl;


   // ====== Main loop ======================================================
   signal(SIGPIPE, SIG_IGN);
   installBreakDetector();
   const unsigned long long stopAt = (gRuntime > 0) ?
      (getMicroTime() + (unsigned long long)rint(gRuntime * 1000000.0)) : ~0ULL;
   while( (!breakDetected()) && (!gStopTimeReached) ) {
      mainLoop(false, stopAt, 0);
   }


   // ====== Print status ===================================================
   FlowSpec::printFlows(cout, gFlowSet, true);


   // ====== Clean up =======================================================
   gMessageReader.deregisterSocket(gControlSocket);
   ext_close(gControlSocket);
   ext_close(gTCPSocket);
   gMessageReader.deregisterSocket(gUDPSocket);
   ext_close(gUDPSocket);
   gMessageReader.deregisterSocket(gSCTPSocket);
   ext_close(gSCTPSocket);
   if(gDCCPSocket >= 0) {
      ext_close(gDCCPSocket);
   }
  */
  puts("STOP!!!");
  exit(1);
}



// ###### Active Mode #######################################################
void activeMode(int argc, char** argv)
{
   // ====== Initialize remote and control addresses ========================
   sockaddr_union remoteAddress;
   if(string2address(argv[1], &remoteAddress) == false) {
      cerr << "ERROR: Invalid remote address " << argv[1] << "!" << endl;
      exit(1);
   }
   if(getPort(&remoteAddress.sa) == 0) {
      setPort(&remoteAddress.sa, 9000);
   }
   sockaddr_union controlAddress = remoteAddress;
   setPort(&controlAddress.sa, getPort(&remoteAddress.sa) + 1);


   // ====== Initialize IDs and print status ================================
   uint32_t flows         = 0;
   uint64_t measurementID = getMicroTime();
   
   StatisticsWriter* statisticsWriter = StatisticsWriter::getStatisticsWriter();
   statisticsWriter->setMeasurementID(measurementID);

   char mIDString[32];
   snprintf((char*)&mIDString, sizeof(mIDString), "%lx", measurementID);
   cout << "Active Mode:" << endl
        << "   - Measurement ID  = " << mIDString << endl
        << "   - Remote Address  = "; printAddress(cout, &remoteAddress.sa, true); 
   cout << endl
        << "   - Control Address = "; printAddress(cout, &controlAddress.sa, true);
   cout << " - connecting ... ";
   cout.flush();


   // ====== Initialize control socket ======================================
   gControlSocket = ext_socket(controlAddress.sa.sa_family, SOCK_STREAM, IPPROTO_SCTP);
   if(gControlSocket < 0) {
      cerr << "ERROR: Failed to create SCTP socket for control port - "
           << strerror(errno) << "!" << endl;
      exit(1);
   }
   if(ext_connect(gControlSocket, &controlAddress.sa, getSocklen(&controlAddress.sa)) < 0) {
      cerr << "ERROR: Unable to establish control association - "
           << strerror(errno) << "!" << endl;
      exit(1);
   }
   cout << "okay; sd=" << gControlSocket << endl << endl;
   gMessageReader.registerSocket(IPPROTO_SCTP, gControlSocket);


   // ====== Get vector, scalar and config names ============================
   const char* configName = "output.config";
   for(int i = 2;i < argc;i++) {
      if(strncmp(argv[i], "-vector=", 8) == 0) {
         statisticsWriter->setVectorName((const char*)&argv[i][8]);
      }
      else if(strncmp(argv[i], "-scalar=", 8) == 0) {
         statisticsWriter->setScalarName((const char*)&argv[i][8]);
      }
      else if(strncmp(argv[i], "-config=", 8) == 0) {
         configName = (const char*)&argv[i][8];
      }
   }
   FILE* configFile = fopen(configName, "w");
   if(configFile == NULL) {
      cerr << "ERROR: Unable to create config file " << configName << "!" << endl;
      exit(1);
   }


   // ====== Initialize flows ===============================================
   uint8_t protocol = 0;
   Flow*   lastFlow = NULL;
   for(int i = 2;i < argc;i++) {
      if(argv[i][0] == '-') {
         lastFlow = NULL;
         if(handleGlobalParameter(argv[i])) {
         }
         else if(strcmp(argv[i], "-tcp") == 0) {
            protocol = IPPROTO_TCP;
         }
         else if(strcmp(argv[i], "-udp") == 0) {
            protocol = IPPROTO_UDP;
         }
         else if(strcmp(argv[i], "-sctp") == 0) {
            protocol = IPPROTO_SCTP;
         }
         else if(strcmp(argv[i], "-dccp") == 0) {
#ifdef HAVE_DCCP
            protocol = IPPROTO_DCCP;
#else
            cerr << "ERROR: DCCP support is not compiled in!" << endl;
            exit(1);
#endif
         }
         else if(strncmp(argv[i], "-vector=", 8) == 0) {
            // Already processed above!
         }
         else if(strncmp(argv[i], "-scalar=", 8) == 0) {
            // Already processed above!
         }
         else if(strncmp(argv[i], "-config=", 8) == 0) {
            // Already processed above!
         }
      }
      else {
         if(protocol == 0) {
            cerr << "ERROR: Protocol specification needed before flow specification "
                 << argv[i] << "!" << endl;
            exit(1);
         }

         lastFlow = createFlow(lastFlow, argv[i], measurementID,
                               protocol, &remoteAddress.sa);
// // ??????????         const bool success = lastFlow->initializeVectorFile();
// //          if(!success) {
// //             return;
// //          }

         cout << "      - Registering flow at remote node ... ";
         cout.flush();
/*         if(!addFlowToRemoteNode(gControlSocket, lastFlow)) {
            cerr << "ERROR: Failed to add flow to remote node!" << endl;
            exit(1);
         }*/
         cout << "okay" << endl;

         if(protocol != IPPROTO_SCTP) {
            lastFlow = NULL;
            protocol = 0;
         }
      }
   }
   cout << endl;
   
   
   printGlobalParameters();


   // ====== Start measurement ==============================================
   const unsigned long long now = getMicroTime();
   if(!startMeasurement(gControlSocket, measurementID)) {
      std::cerr << "ERROR: Failed to start measurement!" << std::endl;
      exit(1);
   }
//  ??????
//    for(vector<FlowSpec*>::iterator iterator = gFlowSet.begin();iterator != gFlowSet.end(); iterator++) {
//       FlowSpec* flowSpec = *iterator;
//       flowSpec->BaseTime = now;
//       flowSpec->Status   = (flowSpec->OnOffEvents.size() > 0) ? FlowSpec::Off : FlowSpec::On;
//    }


   // ====== Main loop ======================================================
   const unsigned long long stopAt  = (gRuntime > 0) ?
      (getMicroTime() + (unsigned long long)rint(gRuntime * 1000000.0)) : ~0ULL;
   bool                     aborted = false;
   installBreakDetector();
   while( (!breakDetected()) && (!gStopTimeReached) ) {
      if(!mainLoop(true, stopAt, measurementID)) {
         cout << endl << "*** Aborted ***" << endl;
         aborted = true;
         break;
      }
   }


   // ====== Stop measurement ===============================================
   if(!aborted) {
      // Write scalar statistics first!
// ??????????      statisticsWriter->writeAllScalarStatistics(getMicroTime(), gFlowSet, measurementID);
   }
   if(!stopMeasurement(gControlSocket, measurementID)) {
      std::cerr << "ERROR: Failed to stop measurement!" << std::endl;
      exit(1);
   }

   if(!aborted) {
      FlowManager::getFlowManager()->print(cout, true);
   }

/*
   // ====== Write config file ==============================================
   fprintf(configFile, "NUM_FLOWS=%u\n", flows);
   fprintf(configFile, "NAME_ACTIVE_NODE=\"%s\"\n", gActiveNodeName);
   fprintf(configFile, "NAME_PASSIVE_NODE=\"%s\"\n", gPassiveNodeName);
   fprintf(configFile, "SCALAR_ACTIVE_NODE=\"%s\"\n", statisticsWriter->ScalarName.c_str());
   fprintf(configFile, "SCALAR_PASSIVE_NODE=\"%s-passive%s\"\n",
         statisticsWriter->ScalarPrefix.c_str(), statisticsWriter->ScalarSuffix.c_str());
   fprintf(configFile, "VECTOR_ACTIVE_NODE=\"%s\"\n", statisticsWriter->VectorName.c_str());
   fprintf(configFile, "VECTOR_PASSIVE_NODE=\"%s-passive%s\"\n\n",
           statisticsWriter->VectorPrefix.c_str(), statisticsWriter->VectorSuffix.c_str());

   for(vector<FlowSpec*>::iterator iterator = gFlowSet.begin();iterator != gFlowSet.end();iterator++) {
      const FlowSpec* flowSpec = *iterator;
      char extension[32];
      snprintf((char*)&extension, sizeof(extension), "-%08x-%04x", flowSpec->FlowID, flowSpec->StreamID);
      fprintf(configFile, "FLOW%u_DESCRIPTION=\"%s\"\n",           flowSpec->FlowID, flowSpec->Description.c_str());
      fprintf(configFile, "FLOW%u_PROTOCOL=\"%s\"\n",              flowSpec->FlowID, getProtocolName(flowSpec->Protocol));
      fprintf(configFile, "FLOW%u_OUTBOUND_FRAME_RATE=%f\n",       flowSpec->FlowID, flowSpec->OutboundFrameRate);
      fprintf(configFile, "FLOW%u_OUTBOUND_FRAME_RATE_RNG=%u\n",   flowSpec->FlowID, flowSpec->OutboundFrameRateRng);
      fprintf(configFile, "FLOW%u_OUTBOUND_FRAME_SIZE=%f\n",       flowSpec->FlowID, flowSpec->OutboundFrameSize);
      fprintf(configFile, "FLOW%u_OUTBOUND_FRAME_SIZE_RNG=%u\n",   flowSpec->FlowID, flowSpec->OutboundFrameSizeRng);
      fprintf(configFile, "FLOW%u_INBOUND_FRAME_RATE=%f\n",        flowSpec->FlowID, flowSpec->InboundFrameRate);
      fprintf(configFile, "FLOW%u_INBOUND_FRAME_RATE_RNG=%u\n",    flowSpec->FlowID, flowSpec->InboundFrameRateRng);
      fprintf(configFile, "FLOW%u_INBOUND_FRAME_SIZE=%f\n",        flowSpec->FlowID, flowSpec->InboundFrameSize);
      fprintf(configFile, "FLOW%u_INBOUND_FRAME_SIZE_RNG=%u\n",    flowSpec->FlowID, flowSpec->InboundFrameSizeRng);
      fprintf(configFile, "FLOW%u_RELIABLE=%f\n",                  flowSpec->FlowID, flowSpec->ReliableMode);
      fprintf(configFile, "FLOW%u_ORDERED=%f\n",                   flowSpec->FlowID, flowSpec->OrderedMode);
      fprintf(configFile, "FLOW%u_VECTOR_ACTIVE_NODE=\"%s\"\n\n",  flowSpec->FlowID, flowSpec->VectorName.c_str());
      fprintf(configFile, "FLOW%u_VECTOR_PASSIVE_NODE=\"%s\"\n\n", flowSpec->FlowID,
                           StatisticsWriter::getPassivNodeFilename(
                              statisticsWriter->VectorPrefix, statisticsWriter->VectorSuffix, extension).c_str());
   }         
   fclose(configFile);
      */

   // ====== Clean up =======================================================
   /*
   cout << "Shutdown:" << endl;
   vector<FlowSpec*>::iterator iterator = gFlowSet.begin();
   while(iterator != gFlowSet.end()) {
      FlowSpec* flowSpec = *iterator;
      cout << "   o Flow ID #" << flowSpec->FlowID << ": ";
      cout.flush();
      if(flowSpec->OriginalSocketDescriptor) {
         gMessageReader.deregisterSocket(flowSpec->SocketDescriptor);
      }
      removeFlowFromRemoteNode(gControlSocket, flowSpec);
      delete flowSpec;
      gFlowSet.erase(iterator);
      iterator = gFlowSet.begin();
   }
   cout << endl;
   gMessageReader.deregisterSocket(gControlSocket);
   ext_close(gControlSocket);
   */
}



// ###### Main program ######################################################
int main(int argc, char** argv)
{
   if(argc < 2) {
      cerr << "Usage: " << argv[0]
           << " [Port|Remote Endpoint] {-tcp|-udp|-sctp|-dccp} {flow spec} ..."
           << endl;
      exit(1);
   }

   cout << "Network Performance Meter - Version 1.0" << endl
        << "---------------------------------------" << endl << endl;

   const uint16_t localPort = atol(argv[1]);
   if( (localPort >= 1024) && (localPort < 65535) ) {
      passiveMode(argc, argv, localPort);
   }
   else {
      activeMode(argc, argv);
   }
   cout << endl;

   return 0;
}
