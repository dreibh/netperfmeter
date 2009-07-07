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

#include <iostream>

#include "flow.h"
#include "control.h"
#include "transfer.h"


#ifndef HAVE_DCCP
#warning DCCP is not supported by the API of this system!
#endif


using namespace std;


static const char*   gActiveNodeName  = "Client";
static const char*   gPassiveNodeName = "Server";
static int           gControlSocket   = -1;
static int           gTCPSocket       = -1;
static int           gUDPSocket       = -1;
static int           gSCTPSocket      = -1;
static int           gDCCPSocket      = -1;
static double        gRuntime         = -1.0;
static bool          gStopTimeReached = false;
static MessageReader gMessageReader;



// ###### Handle global command-line parameter ##############################
bool handleGlobalParameter(const char* parameter)
{
   if(strncmp(parameter, "-runtime=", 9) == 0) {
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
static const char* parseTrafficSpecOption(const char*      parameters,
                                          FlowTrafficSpec& trafficSpec)
{
   char   description[256];
   int    n        = 0;
   double dblValue = 0.0;
   int    intValue = 0;

   if(sscanf(parameters, "maxmsgsize=%u%n", &intValue, &n) == 1) {
      if(intValue > 65535) {
         intValue = 65535;
      }
      else if(intValue < 128) {
         intValue = 128;
      }
      trafficSpec.MaxMsgSize = intValue;
   }
   else if(sscanf(parameters, "unordered=%lf%n", &dblValue, &n) == 1) {
      if((dblValue < 0.0) || (dblValue > 1.0)) {
         cerr << "ERROR: Bad probability for \"unordered\" option in " << parameters << "!" << endl;
         exit(1);
      }
      trafficSpec.OrderedMode = dblValue;
   }
   else if(sscanf(parameters, "unreliable=%lf%n", &dblValue, &n) == 1) {
      if((dblValue < 0.0) || (dblValue > 1.0)) {
         cerr << "ERROR: Bad probability for \"unreliable\" option in " << parameters << "!" << endl;
         exit(1);
      }
      trafficSpec.ReliableMode = dblValue;
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
         double       dblValue;
         unsigned int onoff;
         if(sscanf((const char*)&parameters[m], "+%lf%n", &dblValue, &n) == 1) {
            // Relative time
            onoff = (unsigned int)rint(1000.0 * dblValue);
            onoff += lastEvent;
         }
         else if(sscanf((const char*)&parameters[m], "%lf%n", &dblValue, &n) == 1) {
            // Absolute time
            onoff = (unsigned int)rint(1000.0 * dblValue);
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


// ###### Create Flow for new flow ##########################################
static Flow* createFlow(Flow*              previousFlow,
                        const char*        parameters,
                        const uint64_t     measurementID,
                        const char*        vectorNamePattern,
                        const uint8_t      protocol,
                        const sockaddr*    remoteAddress)
{
   // ====== Get flow ID and stream ID ======================================
   static uint32_t flowID   = 0; // will be increased with each successfull call
   uint16_t        streamID = (previousFlow != NULL) ? previousFlow->getStreamID() + 1 : 0;

   // ====== Get FlowTrafficSpec ============================================
   FlowTrafficSpec trafficSpec;
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
   Flow* flow = new Flow(measurementID,
                         flowID, streamID, trafficSpec);
   assert(flow != NULL);

   // ====== Initialize vector file =========================================
   const std::string vectorName = flow->getNodeOutputName(vectorNamePattern,
                                                          "active",
                                                          format("-%08x-%04x", flowID, streamID));
   if(!flow->initializeVectorFile(vectorName.c_str(), hasSuffix(vectorNamePattern, ".bz2"))) {
      std::cerr << "ERROR: Unable to create vector file <" << vectorName << ">!" << std::endl;
      exit(1);
   }
   
   // ====== Set up socket ==================================================
   int  socketDescriptor;
   bool originalSocketDescriptor;

   if(previousFlow) {
      originalSocketDescriptor = false;
      socketDescriptor         = previousFlow->getSocketDescriptor();
      cout << "      - Connection okay (stream " << streamID << " on existing assoc); sd=" << socketDescriptor << endl;
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
   }
   
   // ====== Update flow with socket descriptor =============================
   flow->setSocketDescriptor(socketDescriptor, originalSocketDescriptor);
   
   flowID++;
   return(flow);
}


// ###### Initialize pollFD entry in main loop ##############################
static void addToPollFDs(pollfd* pollFDs, const int fd, int& n, int* index = NULL)
{
   if(fd >= 0) {
      pollFDs[n].fd      = fd;
      pollFDs[n].events  = POLLIN;
      pollFDs[n].revents = 0;
      if(index) {
         *index = n;
      }
      n++;
   }
}


// ###### Main loop #########################################################
bool mainLoop(const bool               isActiveMode,
              const unsigned long long stopAt,
              const uint64_t           measurementID)
{
   pollfd                 fds[5];
   int                    n         = 0;
   int                    controlID = -1;
   int                    tcpID     = -1;
   int                    udpID     = -1;
   int                    sctpID    = -1;
   int                    dccpID    = -1;
   unsigned long long     now       = getMicroTime();
   std::map<int, pollfd*> pollFDIndex;


   // ====== Get parameters for poll() ======================================
   addToPollFDs((pollfd*)&fds, gControlSocket, n, &controlID);
   addToPollFDs((pollfd*)&fds, gTCPSocket,  n, &tcpID);
   addToPollFDs((pollfd*)&fds, gUDPSocket,  n, &udpID);
   addToPollFDs((pollfd*)&fds, gSCTPSocket, n, &sctpID);
   addToPollFDs((pollfd*)&fds, gDCCPSocket, n, &dccpID);

   
   // ====== Use poll() to wait for events ==================================
   const int timeout = pollTimeout(now, 2,
                                   stopAt,
                                   now + 1000000);

   // printf("timeout=%d\n",timeout);
   const int result = ext_poll_wrapper((pollfd*)&fds, n, timeout);
   // printf("result=%d\n",result);
   

   // ====== Handle socket events ===========================================
   now = getMicroTime();   // Get current time.
   if(result > 0) {

      // ====== Incoming control message ====================================
      if( (controlID >= 0) && (fds[controlID].revents & POLLIN) ) {
         const bool controlOkay = handleNetPerfMeterControlMessage(
                                     &gMessageReader, gControlSocket);
         if((!controlOkay) && (isActiveMode)) {
            return(false);
         }
      }

      // ====== Incoming data message =======================================
      if( (tcpID >= 0) && (fds[tcpID].revents & POLLIN) ) {
         const int newSD = ext_accept(gTCPSocket, NULL, 0);
         if(newSD >= 0) {
            FlowManager::getFlowManager()->addSocket(IPPROTO_TCP, newSD);
         }
      }
      if( (udpID >= 0) && (fds[udpID].revents & POLLIN) ) {
         FlowManager::getFlowManager()->lock();
         handleNetPerfMeterData(isActiveMode, now, IPPROTO_UDP, gUDPSocket);
         FlowManager::getFlowManager()->unlock();
      }
      if( (sctpID >= 0) && (fds[sctpID].revents & POLLIN) ) {
         const int newSD = ext_accept(gSCTPSocket, NULL, 0);
         if(newSD >= 0) {
            FlowManager::getFlowManager()->addSocket(IPPROTO_SCTP, newSD);
         }
      }
#ifdef HAVE_DCCP
      if( (dccpID >= 0) && (fds[dccpID].revents & POLLIN) ) {
         const int newSD = ext_accept(gDCCPSocket, NULL, 0);
         if(newSD >= 0) {
            FlowManager::getFlowManager()->addSocket(IPPROTO_DCCP, newSD);
         }
      }
#endif

   }

   // ====== Stop-time reached ==============================================
   if(now >= stopAt) {
      gStopTimeReached = true;
   }
   return(true);
}



// ###### Passive Mode ######################################################
void passiveMode(int argc, char** argv, const uint16_t localPort)
{
   // ====== Initialize control socket ======================================
   gControlSocket = createAndBindSocket(SOCK_SEQPACKET, IPPROTO_SCTP, localPort + 1);
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
   // NOTE: For connection-less UDP, the FlowManager takes care of the socket!
   FlowManager::getFlowManager()->addSocket(IPPROTO_UDP, gUDPSocket);
   
#ifdef HAVE_DCCP
   gDCCPSocket = createAndBindSocket(SOCK_DCCP, IPPROTO_DCCP, localPort);
   if(gDCCPSocket < 0) {
      cerr << "NOTE: Your kernel does not provide DCCP support." << endl;
   }
#endif

   gSCTPSocket = createAndBindSocket(SOCK_STREAM, IPPROTO_SCTP, localPort);
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
   FlowManager::getFlowManager()->enableDisplay();
   
   const unsigned long long stopAt = (gRuntime > 0) ?
      (getMicroTime() + (unsigned long long)rint(gRuntime * 1000000.0)) : ~0ULL;
   while( (!breakDetected()) && (!gStopTimeReached) ) {
      mainLoop(false, stopAt, 0);
   }

   FlowManager::getFlowManager()->disableDisplay();


   // ====== Clean up =======================================================
   gMessageReader.deregisterSocket(gControlSocket);
   ext_close(gControlSocket);
   ext_close(gTCPSocket);
   FlowManager::getFlowManager()->removeSocket(gUDPSocket);
   ext_close(gUDPSocket);
   ext_close(gSCTPSocket);
   if(gDCCPSocket >= 0) {
      ext_close(gDCCPSocket);
   }
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
   uint64_t measurementID = random64();
   
   cout << "Active Mode:" << endl
        << "   - Measurement ID  = " << format("%lx", measurementID) << endl
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


   // ====== Handle command-line parameters =================================
   const char* vectorNamePattern = "vector.vec.bz2";
   const char* scalarNamePattern = "scalar.sca.bz2";
   const char* configName        = "output.config";
   uint8_t     protocol          = 0;
   Flow*       lastFlow          = NULL;
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
            vectorNamePattern = (const char*)&argv[i][8];
         }
         else if(strncmp(argv[i], "-scalar=", 8) == 0) {
            scalarNamePattern = (const char*)&argv[i][8];
         }
         else if(strncmp(argv[i], "-config=", 8) == 0) {
            configName = (const char*)&argv[i][8];
         }
      }
      else {
         if(protocol == 0) {
            cerr << "ERROR: Protocol specification needed before flow specification "
                 << argv[i] << "!" << endl;
            exit(1);
         }

         lastFlow = createFlow(lastFlow, argv[i], measurementID, vectorNamePattern,
                               protocol, &remoteAddress.sa);

         cout << "      - Registering flow at remote node ... ";
         cout.flush();
         if(!performNetPerfMeterAddFlow(gControlSocket, lastFlow)) {
            cerr << endl << "ERROR: Failed to add flow to remote node!" << endl;
            exit(1);
         }
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
   if(!performNetPerfMeterStart(gControlSocket, measurementID,
                                gActiveNodeName, gPassiveNodeName,
                                configName, vectorNamePattern, scalarNamePattern)) {
      std::cerr << "ERROR: Failed to start measurement!" << std::endl;
      exit(1);
   }


   // ====== Main loop ======================================================
   const unsigned long long stopAt  = (gRuntime > 0) ?
      (getMicroTime() + (unsigned long long)rint(gRuntime * 1000000.0)) : ~0ULL;
   bool                     aborted = false;
   signal(SIGPIPE, SIG_IGN);
   installBreakDetector();
   FlowManager::getFlowManager()->enableDisplay();

   while( (!breakDetected()) && (!gStopTimeReached) ) {
      if(!mainLoop(true, stopAt, measurementID)) {
         cout << endl << "*** Aborted ***" << endl;
         aborted = true;
         break;
      }
   }

   FlowManager::getFlowManager()->disableDisplay();


   // ====== Stop measurement ===============================================
   cout << "Shutdown:" << endl;
   if(!performNetPerfMeterStop(gControlSocket, measurementID, true)) {
      std::cerr << "ERROR: Failed to stop measurement!" << std::endl;
      exit(1);
   }
}



// ###### Main program ######################################################
int main(int argc, char** argv)
{
   if(argc < 2) {
      cerr << "Usage: " << argv[0]
           << " [Local Port|Remote Endpoint] {-tcp|-udp|-sctp|-dccp} {flow spec} ..."
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
