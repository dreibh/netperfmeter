/* $Id$
 *
 * Network Performance Meter
 * Copyright (C) 2009-2011 by Thomas Dreibholz
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
#else
#include <linux/dccp.h>
#endif


using namespace std;


#define MAX_LOCAL_ADDRESSES 16
static unsigned int   gLocalAddresses = 0;
static sockaddr_union gLocalAddressArray[MAX_LOCAL_ADDRESSES];


static const char*   gActiveNodeName  = "Client";
static const char*   gPassiveNodeName = "Server";
static int           gControlSocket   = -1;
static int           gTCPSocket       = -1;
static int           gUDPSocket       = -1;
static int           gSCTPSocket      = -1;
static int           gDCCPSocket      = -1;
static double        gRuntime         = -1.0;
static bool          gStopTimeReached = false;
MessageReader        gMessageReader;



// ###### Handle global command-line parameter ##############################
bool handleGlobalParameter(char* parameter)
{
   if(strncmp(parameter, "-runtime=", 9) == 0) {
      gRuntime = atof((const char*)&parameter[9]);
   }
   else if(strncmp(parameter, "-activenodename=", 16) == 0) {
      gActiveNodeName = (const char*)&parameter[16];
   }
   else if(strncmp(parameter, "-passivenodename=", 17) == 0) {
      gPassiveNodeName = (const char*)&parameter[17];
   }
   else if(strcmp(parameter, "-quiet") == 0) {
      // Already handled before!
   }
   else if(strcmp(parameter, "-verbose") == 0) {
      // Already handled before!
   }
   else if(strncmp(parameter, "-verbosity=", 11) == 0) {
      // Already handled before!
   }
   else if(strncmp(parameter, "-local=", 7) == 0) {
      gLocalAddresses = 0;
      char* address = (char*)&parameter[7];
      while(gLocalAddresses < MAX_LOCAL_ADDRESSES) {
         char* idx = index(address, ',');
         if(idx) {
            *idx = 0x00;
         }
         if(!string2address(address, &gLocalAddressArray[gLocalAddresses])) {
            fprintf(stderr, "ERROR: Bad address %s for local endpoint! Use format <address:port>.\n",address);
            exit(1);
         }
         gLocalAddresses++;
         if(!idx) {
            break;
         }
         address = idx;
         address++;
      }
   }
   else {
      return(false);
   }
   return(true);
}


// ###### Print global parameter settings ###################################
void printGlobalParameters()
{
   if(gOutputVerbosity >= NPFOV_STATUS) {
      std::cout << "Global Parameters:" << std::endl
               << "   - Runtime           = ";
      if(gRuntime >= 0.0) {
         std::cout << gRuntime << "s" << std::endl;
      }
      else {
         std::cout << "until manual stop" << std::endl;
      }
      std::cout << "   - Active Node Name  = " << gActiveNodeName  << std::endl
                << "   - Passive Node Name = " << gPassiveNodeName << std::endl;
      std::cout << "   - Local Address(es) = ";
      if(gLocalAddresses > 0) {
         for(unsigned int i = 0;i < gLocalAddresses;i++) {
            if(i > 0) {
               std::cout << ", ";
            }
            printAddress(std::cout, &gLocalAddressArray[i].sa, false);
         }
      }
      else {
         std::cout << "(any)";
      }
      std::cout << std::endl;
      std::cout << "   - Logging Verbosity = " << gOutputVerbosity << std::endl
                << std::endl;
   }
}


// ###### Read random number parameter ######################################
static const char* parseNextEntry(const char* parameters,
                                  double*     valueArray,
                                  uint8_t*    rng)
{
   int n = 0;
   for(size_t i = 0;i < NETPERFMETER_RNG_INPUT_PARAMETERS;i++) {
      valueArray[i] = 0.0;
   }
   if(sscanf(parameters, "exp%lf%n", &valueArray[0], &n) == 1) {
      *rng = RANDOM_EXPONENTIAL;
   }
   else if(sscanf(parameters, "const%lf%n", &valueArray[0], &n) == 1) {
      *rng = RANDOM_CONSTANT;
   }
   else if(sscanf(parameters, "uniform%lf,%lf%n", &valueArray[0], &valueArray[1], &n) == 2) {
      *rng = RANDOM_UNIFORM;
      // printf("%lf +/- %lf%%\n",valueArray[0],valueArray[1]*100);
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
                                          FlowTrafficSpec& trafficSpec,
                                          uint32_t&        flowID)
{
   char   description[256];
   int    n        = 0;
   double dblValue = 0.0;
   int    intValue = 0;

   if(sscanf(parameters, "id=%u%n", &intValue, &n) == 1) {
      flowID = (uint32_t)intValue;
   }
   else if(sscanf(parameters, "maxmsgsize=%u%n", &intValue, &n) == 1) {
      if(intValue > 65535) {
         intValue = 65535;
      }
      else if(intValue < 128) {
         intValue = 128;
      }
      trafficSpec.MaxMsgSize = intValue;
   }
   else if(sscanf(parameters, "defragtimeout=%u%n", &intValue, &n) == 1) {
      trafficSpec.DefragmentTimeout = 1000ULL * (uint32_t)intValue;
   }
   else if(sscanf(parameters, "ordered=%lf%n", &dblValue, &n) == 1) {
      if((dblValue < 0.0) || (dblValue > 1.0)) {
         cerr << "ERROR: Bad probability for \"ordered\" option in " << parameters << "!" << endl;
         exit(1);
      }
      trafficSpec.OrderedMode = dblValue;
   }
   else if(sscanf(parameters, "unordered=%lf%n", &dblValue, &n) == 1) {
      if((dblValue < 0.0) || (dblValue > 1.0)) {
         cerr << "ERROR: Bad probability for \"unordered\" option in " << parameters << "!" << endl;
         exit(1);
      }
      trafficSpec.OrderedMode = 1.0 - dblValue;
   }
   else if(sscanf(parameters, "reliable=%lf%n", &dblValue, &n) == 1) {
      if((dblValue < 0.0) || (dblValue > 1.0)) {
         cerr << "ERROR: Bad probability for \"reliable\" option in " << parameters << "!" << endl;
         exit(1);
      }
      trafficSpec.ReliableMode = dblValue;
   }
   else if(sscanf(parameters, "unreliable=%lf%n", &dblValue, &n) == 1) {
      if((dblValue < 0.0) || (dblValue > 1.0)) {
         cerr << "ERROR: Bad probability for \"unreliable\" option in " << parameters << "!" << endl;
         exit(1);
      }
      trafficSpec.ReliableMode = 1.0 - dblValue;
   }
   else if(sscanf(parameters, "rtx_timeout=%u%n", &intValue, &n) == 1) {
      trafficSpec.RetransmissionTrials     = (uint32_t)intValue;
      trafficSpec.RetransmissionTrialsInMS = true;
   }
   else if(sscanf(parameters, "rtx_trials=%u%n", &intValue, &n) == 1) {
      trafficSpec.RetransmissionTrials     = (uint32_t)intValue;
      trafficSpec.RetransmissionTrialsInMS = false;
   }
   else if(sscanf(parameters, "rcvbuf=%u%n", &intValue, &n) == 1) {
      trafficSpec.RcvBufferSize = (uint32_t)intValue;
      if(trafficSpec.RcvBufferSize < 4096) {
         trafficSpec.RcvBufferSize = 4096;
      }
   }
   else if(sscanf(parameters, "sndbuf=%u%n", &intValue, &n) == 1) {
      trafficSpec.SndBufferSize = (uint32_t)intValue;
      if(trafficSpec.SndBufferSize < 4096) {
         trafficSpec.SndBufferSize = 4096;
      }
   }
   else if(strncmp(parameters, "cmt=", 4) == 0) {
      unsigned int cmt;
      int          pos;
      if(strncmp((const char*)&parameters[4], "off", 3) == 0) {
         trafficSpec.CMT = NPAF_PRIMARY_PATH;
         n = 4 + 3;
      }
      else if(strncmp((const char*)&parameters[4], "normal", 6) == 0) {
         trafficSpec.CMT = NPAF_CMT;
         n = 4 + 6;
      }
      else if(strncmp((const char*)&parameters[4], "rp", 2) == 0) {
         trafficSpec.CMT = NPAF_CMTRP;
         n = 4 + 2;
      }
      else if(sscanf((const char*)&parameters[4], "%u%n", &cmt, &pos) == 1) {
         trafficSpec.CMT = cmt;
         n = 4 + pos;
      }
      else {
         cerr << "ERROR: Invalid \"cmt\" setting: " << (const char*)&parameters[4] << "!" << std::endl;
         exit(1);
      }
   }
   else if(strncmp(parameters, "ccid=", 5) == 0) {
      unsigned int ccid;
      int          pos;
      if(sscanf((const char*)&parameters[5], "%u%n", &ccid, &pos) == 1) {
         trafficSpec.CCID = ccid;
         n = 5 + pos;
      }
      else {
         cerr << "ERROR: Invalid \"ccid\" setting: " << (const char*)&parameters[5] << "!" << std::endl;
         exit(1);
      }
   }
   else if(strncmp(parameters, "error_on_abort=", 15) == 0) {
      if(strncmp((const char*)&parameters[15], "on", 2) == 0) {
         trafficSpec.ErrorOnAbort = true;
         n = 15 + 2;
      }
      else if(strncmp((const char*)&parameters[15], "off", 3) == 0) {
         trafficSpec.ErrorOnAbort = false;
         n = 15 + 3;
      }
      else {
         cerr << "ERROR: Invalid \"error_on_abort\" setting: " << (const char*)&parameters[4] << "!" << std::endl;
         exit(1);
      }
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
static Flow* createFlow(Flow*                  previousFlow,
                        const char*            parameters,
                        const uint64_t         measurementID,
                        const char*            vectorNamePattern,
                        const OutputFileFormat vectorFileFormat,
                        const uint8_t          protocol,
                        const sockaddr*        remoteAddress)
{
   // ====== Get flow ID and stream ID ======================================
   static uint32_t flowID   = 0; // will be increased with each successfull call
   uint16_t        streamID = (previousFlow != NULL) ? previousFlow->getStreamID() + 1 : 0;

   // ====== Get FlowTrafficSpec ============================================
   FlowTrafficSpec trafficSpec;
   trafficSpec.Protocol = protocol;
   if(strncmp(parameters, "default", 7) == 0) {
      trafficSpec.OutboundFrameRateRng = RANDOM_CONSTANT;
      trafficSpec.OutboundFrameRate[0] = 0.0;
      switch(trafficSpec.Protocol) {
         case IPPROTO_TCP:
            trafficSpec.OutboundFrameSize[0] = 1500 - 40 - 20;
          break;
         case IPPROTO_UDP:
            trafficSpec.OutboundFrameSize[0] = 1500 - 40 - 8;
            trafficSpec.OutboundFrameRate[0] = 25;
          break;
#ifdef HAVE_DCCP
         case IPPROTO_DCCP:
            trafficSpec.OutboundFrameSize[0] = 1500 - 40 - 40;   // 1420B for IPv6 via 1500B MTU!
          break;
#endif
         default:
            trafficSpec.OutboundFrameSize[0] = 1500 - 40 - 12 - 16;
          break;
      }
      parameters = (char*)&parameters[7];
      if(parameters[0] != 0x00) {
         parameters++;
         while( (parameters = parseTrafficSpecOption(parameters, trafficSpec, flowID)) ) { }
      }
   }
   else {
      parameters = parseNextEntry(parameters, (double*)&trafficSpec.OutboundFrameRate, &trafficSpec.OutboundFrameRateRng);
      if(parameters) {
         parameters = parseNextEntry(parameters, (double*)&trafficSpec.OutboundFrameSize, &trafficSpec.OutboundFrameSizeRng);
         if(parameters) {
            parameters = parseNextEntry(parameters, (double*)&trafficSpec.InboundFrameRate, &trafficSpec.InboundFrameRateRng);
            if(parameters) {
               parameters = parseNextEntry(parameters, (double*)&trafficSpec.InboundFrameSize, &trafficSpec.InboundFrameSizeRng);
               if(parameters) {
                  while( (parameters = parseTrafficSpecOption(parameters, trafficSpec, flowID)) ) {
                  }
               }
            }
         }
      }
   }
   if(trafficSpec.Description == "") {
      trafficSpec.Description = format("Flow %u", flowID);
   }

   // ====== Create new flow ================================================
   if(FlowManager::getFlowManager()->findFlow(measurementID, flowID, streamID) != NULL) {
      std::cerr << "ERROR: Flow ID " << flowID << " is used twice. Ensure correct id=<ID> parameters!" << std::endl;
      exit(1);
   }
   Flow* flow = new Flow(measurementID, flowID, streamID, trafficSpec);
   assert(flow != NULL);

   // ====== Initialize vector file =========================================
   const std::string vectorName = flow->getNodeOutputName(vectorNamePattern,
                                                          "active",
                                                          format("-%08x-%04x", flowID, streamID));
   if(!flow->initializeVectorFile(vectorName.c_str(), vectorFileFormat)) {
      std::cerr << "ERROR: Unable to create vector file <" << vectorName << ">!" << std::endl;
      exit(1);
   }

   // ====== Set up socket ==================================================
   int  socketDescriptor;
   bool originalSocketDescriptor;

   if(previousFlow) {
      originalSocketDescriptor = false;
      socketDescriptor         = previousFlow->getSocketDescriptor();
      if(gOutputVerbosity >= NPFOV_STATUS) {
         cout << "Flow #" << flow->getFlowID()
            << ": connection okay <sd=" << socketDescriptor << "> ";
      }
   }
   else {
      originalSocketDescriptor = true;
      socketDescriptor          = -1;
      switch(protocol) {
         case IPPROTO_SCTP:
            socketDescriptor = createAndBindSocket(remoteAddress->sa_family, SOCK_STREAM, IPPROTO_SCTP, 0,
                                                   gLocalAddresses, (const sockaddr_union*)&gLocalAddressArray, false);
           break;
         case IPPROTO_TCP:
            socketDescriptor = createAndBindSocket(remoteAddress->sa_family, SOCK_STREAM, IPPROTO_TCP, 0,
                                                   gLocalAddresses, (const sockaddr_union*)&gLocalAddressArray, false);
           break;
         case IPPROTO_UDP:
            socketDescriptor = createAndBindSocket(remoteAddress->sa_family, SOCK_DGRAM, IPPROTO_UDP, 0,
                                                   gLocalAddresses, (const sockaddr_union*)&gLocalAddressArray, false);
           break;
#ifdef HAVE_DCCP
         case IPPROTO_DCCP:
            socketDescriptor = createAndBindSocket(remoteAddress->sa_family, SOCK_DCCP, IPPROTO_DCCP, 0,
                                                   gLocalAddresses, (const sockaddr_union*)&gLocalAddressArray, false);
           break;
#endif
      }
      if(socketDescriptor < 0) {
         cerr << "ERROR: Unable to create " << getProtocolName(protocol)
              << " socket - " << strerror(errno) << "!" << endl;
         exit(1);
      }

      // ====== Establish connection ========================================
      if(gOutputVerbosity >= NPFOV_STATUS) {
         cout << "Flow #" << flow->getFlowID() << ": connecting "
            << getProtocolName(protocol) << " socket to ";
         printAddress(cout, remoteAddress, true);
         cout << " ... ";
         cout.flush();
      }

      if(protocol == IPPROTO_SCTP) {
         sctp_initmsg initmsg;
         memset((char*)&initmsg, 0 ,sizeof(initmsg));
         initmsg.sinit_num_ostreams  = 65535;
         initmsg.sinit_max_instreams = 65535;
         if(ext_setsockopt(socketDescriptor, IPPROTO_SCTP, SCTP_INITMSG,
                           &initmsg, sizeof(initmsg)) < 0) {
            cerr << "ERROR: Failed to configure INIT parameters on SCTP socket (SCTP_INITMSG option) - "
                 << strerror(errno) << "!" << endl;
            exit(1);
         }

         sctp_event_subscribe events;
         memset((char*)&events, 0 ,sizeof(events));
         events.sctp_data_io_event = 1;
         if(ext_setsockopt(socketDescriptor, IPPROTO_SCTP, SCTP_EVENTS,
                           &events, sizeof(events)) < 0) {
            cerr << "ERROR: Failed to configure events on SCTP socket (SCTP_EVENTS option) - "
                 << strerror(errno) << "!" << endl;
            exit(1);
         }
      }

      int bufferSize = flow->getTrafficSpec().RcvBufferSize;
      if(ext_setsockopt(socketDescriptor, SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize)) < 0) {
         cerr << "ERROR: Failed to configure receive buffer size on SCTP socket (SO_RCVBUF option) - "
              << strerror(errno) << "!" << endl;
         exit(1);
      }
      bufferSize = flow->getTrafficSpec().SndBufferSize;
      if(ext_setsockopt(socketDescriptor, SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize)) < 0) {
         cerr << "ERROR: Failed to configure send buffer size on SCTP socket (SO_SNDBUF option) - "
              << strerror(errno) << "!" << endl;
         exit(1);
      }

      if(ext_connect(socketDescriptor, remoteAddress, getSocklen(remoteAddress)) < 0) {
         cerr << "ERROR: Unable to connect " << getProtocolName(protocol)
              << " socket - " << strerror(errno) << "!" << endl;
         exit(1);
      }

      if(flow->getTrafficSpec().Protocol == IPPROTO_SCTP) {
#ifdef SCTP_CMT_ON_OFF
         struct sctp_assoc_value cmtOnOff;
         cmtOnOff.assoc_id    = 0;
         cmtOnOff.assoc_value = flow->getTrafficSpec().CMT;
         if(ext_setsockopt(socketDescriptor, IPPROTO_SCTP, SCTP_CMT_ON_OFF, &cmtOnOff, sizeof(cmtOnOff)) < 0) {
            if(flow->getTrafficSpec().CMT != NPAF_PRIMARY_PATH) {
               cerr << "ERROR: Failed to configure CMT usage on SCTP socket (SCTP_CMT_ON_OFF option) - "
                  << strerror(errno) << "!" << endl;
               exit(1);
            }
         }
#else
         if(flow->getTrafficSpec().CMT != NPAF_PRIMARY_PATH) {
            cerr << "ERROR: CMT usage configured, but not supported by this system!" << endl;
            exit(1);
         }
#endif
      }

#ifdef DCCP_SOCKOPT_CCID
      if(flow->getTrafficSpec().Protocol == IPPROTO_DCCP) {
         const uint8_t value = flow->getTrafficSpec().CCID;
         if(ext_setsockopt(socketDescriptor, 0, DCCP_SOCKOPT_CCID, &value, sizeof(value)) < 0) {
            cerr << "ERROR: Failed to configure CCID #" << (unsigned int)value
                 << " on DCCP socket (DCCP_SOCKOPT_CCID option) - "
                 << strerror(errno) << "!" << endl;
            exit(1);
         }
      }
#endif

      if(gOutputVerbosity >= NPFOV_STATUS) {
         cout << "okay <sd=" << socketDescriptor << "> ";
      }
   }
   cout.flush();

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
   // ====== Set options ====================================================
   for(int i = 2;i < argc;i++) {
      if(!handleGlobalParameter(argv[i])) {
         std::cerr << "Invalid argument: " << argv[i] << "!" << std::endl;
         exit(1);
      }
   }
   printGlobalParameters();


   // ====== Initialize control socket ======================================
   gControlSocket = createAndBindSocket(AF_UNSPEC, SOCK_SEQPACKET, IPPROTO_SCTP, localPort + 1,
                                        0, NULL, true);
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
   gTCPSocket = createAndBindSocket(AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, localPort,
                                    gLocalAddresses, (const sockaddr_union*)&gLocalAddressArray, true);
   if(gTCPSocket < 0) {
      cerr << "ERROR: Failed to create and bind TCP socket - "
           << strerror(errno) << "!" << endl;
      exit(1);
   }

   gUDPSocket = createAndBindSocket(AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP, localPort,
                                    gLocalAddresses, (const sockaddr_union*)&gLocalAddressArray, true);
   if(gUDPSocket < 0) {
      cerr << "ERROR: Failed to create and bind UDP socket - "
           << strerror(errno) << "!" << endl;
      exit(1);
   }
   // NOTE: For connection-less UDP, the FlowManager takes care of the socket!
   FlowManager::getFlowManager()->addSocket(IPPROTO_UDP, gUDPSocket);

#ifdef HAVE_DCCP
   gDCCPSocket = createAndBindSocket(AF_UNSPEC, SOCK_DCCP, IPPROTO_DCCP, localPort,
                                     gLocalAddresses, (const sockaddr_union*)&gLocalAddressArray, true);
   if(gDCCPSocket < 0) {
      cerr << "NOTE: Your kernel does not provide DCCP support." << endl;
   }
#endif

   gSCTPSocket = createAndBindSocket(AF_UNSPEC, SOCK_STREAM, IPPROTO_SCTP, localPort,
                                     gLocalAddresses, (const sockaddr_union*)&gLocalAddressArray, true);
   if(gSCTPSocket < 0) {
      cerr << "ERROR: Failed to create and bind SCTP socket - "
           << strerror(errno) << "!" << endl;
      exit(1);
   }
   sctp_initmsg initmsg;
   memset((char*)&initmsg, 0 ,sizeof(initmsg));
   initmsg.sinit_num_ostreams  = 65535;
   initmsg.sinit_max_instreams = 65535;
   if(ext_setsockopt(gSCTPSocket, IPPROTO_SCTP, SCTP_INITMSG,
                     &initmsg, sizeof(initmsg)) < 0) {
      cerr << "ERROR: Failed to configure INIT parameters on SCTP socket - "
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
   FlowManager::getFlowManager()->removeSocket(gUDPSocket, false);
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

   if(gOutputVerbosity >= NPFOV_STATUS) {
      cout << "Active Mode:" << endl
         << "   - Measurement ID  = " << format("%lx", measurementID) << endl
         << "   - Remote Address  = "; printAddress(cout, &remoteAddress.sa, true);
      cout << endl
         << "   - Control Address = "; printAddress(cout, &controlAddress.sa, true);
      cout << " - connecting ... ";
      cout.flush();
   }

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
   if(gOutputVerbosity >= NPFOV_STATUS) {
      cout << "okay; sd=" << gControlSocket << endl << endl;
   }
   gMessageReader.registerSocket(IPPROTO_SCTP, gControlSocket);


   // ====== Handle command-line parameters =================================
   bool             hasFlow           = false;
   const char*      vectorNamePattern = "";
   OutputFileFormat vectorFileFormat  = OFF_None;
   const char*      scalarNamePattern = "";
   OutputFileFormat scalarFileFormat  = OFF_None;
   const char*      configName        = "";
   uint8_t          protocol          = 0;
   Flow*            lastFlow          = NULL;
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
            if(hasFlow) {
               cerr << "ERROR: Vector file must be set *before* first flow!" << endl;
               exit(1);
            }
            vectorNamePattern = (const char*)&argv[i][8];
            if(vectorNamePattern[0] == 0x00)
               vectorFileFormat = OFF_None;
            else if(hasSuffix(vectorNamePattern, ".bz2")) {
               vectorFileFormat = OFF_BZip2;
            }
            else {
               vectorFileFormat = OFF_Plain;
            }
         }
         else if(strncmp(argv[i], "-scalar=", 8) == 0) {
            if(hasFlow) {
               cerr << "ERROR: Scalar file must be set *before* first flow!" << endl;
               exit(1);
            }
            scalarNamePattern = (const char*)&argv[i][8];
            if(scalarNamePattern[0] == 0x00)
               scalarFileFormat = OFF_None;
            else if(hasSuffix(scalarNamePattern, ".bz2")) {
               scalarFileFormat = OFF_BZip2;
            }
            else {
               scalarFileFormat = OFF_Plain;
            }
         }
         else if(strncmp(argv[i], "-config=", 8) == 0) {
            configName = (const char*)&argv[i][8];
         }
         else {
            std::cerr << "Invalid argument: " << argv[i] << "!" << std::endl;
            exit(1);
         }
      }
      else {
         if(protocol == 0) {
            cerr << "ERROR: Protocol specification needed before flow specification at argument \""
                 << argv[i] << "\"!" << endl;
            exit(1);
         }

         lastFlow = createFlow(lastFlow, argv[i], measurementID,
                               vectorNamePattern, vectorFileFormat,
                               protocol, &remoteAddress.sa);
         hasFlow = true;

         if(!performNetPerfMeterAddFlow(&gMessageReader, gControlSocket, lastFlow)) {
            cerr << endl << "ERROR: Failed to add flow to remote node!" << endl;
            exit(1);
         }
         if(gOutputVerbosity >= NPFOV_STATUS) {
            cout << "okay" << endl;
         }

         if(protocol != IPPROTO_SCTP) {
            lastFlow = NULL;
            protocol = 0;
         }
      }
   }
   if(gOutputVerbosity >= NPFOV_STATUS) {
      cout << endl;
   }

   printGlobalParameters();


   // ====== Start measurement ==============================================
   if(!performNetPerfMeterStart(&gMessageReader, gControlSocket, measurementID,
                                gActiveNodeName, gPassiveNodeName,
                                configName,
                                vectorNamePattern, vectorFileFormat,
                                scalarNamePattern, scalarFileFormat)) {
      std::cerr << "ERROR: Failed to start measurement!" << std::endl;
      exit(1);
   }


   // ====== Main loop ======================================================
   const unsigned long long stopAt  = (gRuntime > 0) ?
      (getMicroTime() + (unsigned long long)rint(gRuntime * 1000000.0)) : ~0ULL;
   bool                     aborted = false;
   signal(SIGPIPE, SIG_IGN);
   installBreakDetector();
   if(gOutputVerbosity >= NPFOV_BANDWIDTH_INFO) {
      FlowManager::getFlowManager()->enableDisplay();
   }

   while( (!breakDetected()) && (!gStopTimeReached) ) {
      if(!mainLoop(true, stopAt, measurementID)) {
         cout << endl << "*** Aborted ***" << endl;
         aborted = true;
         break;
      }
   }

   if(gOutputVerbosity >= NPFOV_BANDWIDTH_INFO) {
      FlowManager::getFlowManager()->disableDisplay();
      if(gStopTimeReached) {
         cout << endl;
      }
   }


   // ====== Stop measurement ===============================================
   if(gOutputVerbosity >= NPFOV_STATUS) {
      cout << "Shutdown:" << endl;
   }
   if(!performNetPerfMeterStop(&gMessageReader, gControlSocket, measurementID)) {
      std::cerr << "ERROR: Failed to stop measurement and download the results!" << std::endl;
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

   for(int i = 2;i < argc;i++) {
      if(strcmp(argv[i], "-quiet") == 0) {
         if(gOutputVerbosity > 0) {
            gOutputVerbosity--;
         }
      }
      else if(strcmp(argv[i], "-verbose") == 0) {
         gOutputVerbosity++;
      }
      else if(strncmp(argv[i], "-verbosity=", 11) == 0) {
         gOutputVerbosity = atol((const char*)&argv[i][11]);
      }
   }
   if(gOutputVerbosity >= NPFOV_STATUS) {
      cout << "Network Performance Meter - Version 1.0" << endl
           << "---------------------------------------" << endl << endl;
   }

   const uint16_t localPort = atol(argv[1]);
   if( (localPort >= 1024) && (localPort < 65535) ) {
      passiveMode(argc, argv, localPort);
   }
   else {
      activeMode(argc, argv);
   }

   return 0;
}
