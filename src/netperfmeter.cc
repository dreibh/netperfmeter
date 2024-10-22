/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2024 by Thomas Dreibholz
 * ==========================================================================
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
 * Contact:  dreibh@simula.no
 * Homepage: https://www.nntb.no/~dreibh/netperfmeter/
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
#include "loglevel.h"
#include "transfer.h"


#define MAX_LOCAL_ADDRESSES 16
static unsigned int   gLocalDataAddresses = 0;
static sockaddr_union gLocalDataAddressArray[MAX_LOCAL_ADDRESSES];
static unsigned int   gLocalControlAddresses = 0;
static sockaddr_union gLocalControlAddressArray[MAX_LOCAL_ADDRESSES];

static const char*    gActiveNodeName   = "Client";
static const char*    gPassiveNodeName  = "Server";
static const char*    gPathMgr          = "fullmesh";
static const char*    gScheduler        = "default";
static bool           gBindV6Only       = false;
static int            gSndBufSize       = -1;
static int            gRcvBufSize       = -1;
static bool           gControlOverTCP   = false;
static int            gControlSocket    = -1;
static int            gControlSocketTCP = -1;
static int            gTCPSocket        = -1;
static int            gMPTCPSocket      = -1;
static int            gUDPSocket        = -1;
static int            gSCTPSocket       = -1;
static int            gDCCPSocket       = -1;
static double         gRuntime          = -1.0;
static bool           gDisplayEnabled   = true;
static bool           gStopTimeReached  = false;
static MessageReader  gMessageReader;



// ###### Handle global command-line parameter ##############################
bool handleGlobalParameter(char* parameter)
{
   if(strncmp(parameter, "-log", 4) == 0) {
      // Already handled!
   }
   else if(strncmp(parameter, "-runtime=", 9) == 0) {
      gRuntime = atof((const char*)&parameter[9]);
   }
   else if(strcmp(parameter, "-control-over-tcp") == 0) {
      gControlOverTCP = true;
   }
   else if(strncmp(parameter, "-activenodename=", 16) == 0) {
      gActiveNodeName = (const char*)&parameter[16];
   }
   else if(strncmp(parameter, "-passivenodename=", 17) == 0) {
      gPassiveNodeName = (const char*)&parameter[17];
   }
   else if(strncmp(parameter, "-pathmgr=", 9) == 0) {
      gPathMgr = (const char*)&parameter[9];
   }
   else if(strncmp(parameter, "-scheduler=", 11) == 0) {
      gScheduler = (const char*)&parameter[11];
   }
   else if(strncmp(parameter, "-sndbuf=", 8) == 0) {
      gSndBufSize = atol((const char*)&parameter[8]);
   }
   else if(strncmp(parameter, "-rcvbuf=", 8) == 0) {
      gRcvBufSize = atol((const char*)&parameter[8]);
   }
   else if(strcmp(parameter, "-v6only") == 0) {
      gBindV6Only = true;
   }
   else if(strcmp(parameter, "-quiet") == 0) {
      // Already handled before!
   }
   else if(strcmp(parameter, "-display") == 0) {
      gDisplayEnabled = true;
   }
   else if(strcmp(parameter, "-nodisplay") == 0) {
      gDisplayEnabled = false;
   }
   else if(strncmp(parameter, "-local=", 7) == 0) {
      gLocalDataAddresses = 0;
      char* address = (char*)&parameter[7];
      while(gLocalDataAddresses < MAX_LOCAL_ADDRESSES) {
         char* idx = index(address, ',');
         if(idx) {
            *idx = 0x00;
         }
         if(!string2address(address, &gLocalDataAddressArray[gLocalDataAddresses])) {
            fprintf(stderr, "ERROR: Bad address %s for local data endpoint! Use format <address:port>.\n",address);
            exit(1);
         }
         gLocalDataAddresses++;
         if(!idx) {
            break;
         }
         address = idx;
         address++;
      }
   }
   else if(strncmp(parameter, "-controllocal=", 14) == 0) {
      gLocalControlAddresses = 0;
      char* address = (char*)&parameter[14];
      while(gLocalControlAddresses < MAX_LOCAL_ADDRESSES) {
         char* idx = index(address, ',');
         if(idx) {
            *idx = 0x00;
         }
         if(!string2address(address, &gLocalControlAddressArray[gLocalControlAddresses])) {
            fprintf(stderr, "ERROR: Bad address %s for local control endpoint! Use format <address:port>.\n",address);
            exit(1);
         }
         gLocalControlAddresses++;
         if(!idx) {
            break;
         }
         address = idx;
         address++;
      }
   }
   else {
      return false;
   }
   return true;
}


// ###### Print global parameter settings ###################################
void printGlobalParameters()
{
   LOG_INFO
   stdlog << "Global Parameters:\n"
          << "   - Runtime                   = ";
   if(gRuntime >= 0.0) {
      stdlog << gRuntime << " s\n";
   }
   else {
      stdlog << "until manual stop\n";
   }
   stdlog << "   - Active Node Name          = " << gActiveNodeName  << "\n"
          << "   - Passive Node Name         = " << gPassiveNodeName << "\n"
          << "   - Local Data Address(es)    = ";
   if(gLocalDataAddresses > 0) {
      for(unsigned int i = 0;i < gLocalDataAddresses;i++) {
         if(i > 0) {
            stdlog << ", ";
         }
         printAddress(stdlog, &gLocalDataAddressArray[i].sa, false);
      }
   }
   else {
      stdlog << "(any)";
   }
   stdlog << "\n   - Local Control Address(es) = ";
   if(gLocalControlAddresses > 0) {
      for(unsigned int i = 0;i < gLocalControlAddresses;i++) {
         if(i > 0) {
            stdlog << ", ";
         }
         printAddress(stdlog, &gLocalControlAddressArray[i].sa, false);
      }
   }
   else {
      stdlog << "(any)";
   }
   stdlog << "\n   - Logging Verbosity         = " << gLogLevel << "\n\n";
   LOG_END
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
   }
   else if(sscanf(parameters, "pareto%lf,%lf%n", &valueArray[0], &valueArray[1], &n) == 2) {
      *rng = RANDOM_PARETO;
   }
   else if(sscanf(parameters, "%lf%n", &valueArray[0], &n) == 1) {
      // shortcut: number interpreted as constant
      *rng = RANDOM_CONSTANT;
   }
   else {
      std::cerr << "ERROR: Invalid parameters " << parameters << "!\n";
      exit(1);
   }
   if(parameters[n] == 0x00) {
      return nullptr;
   }
   return (const char*)&parameters[n + 1];
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
         std::cerr << "ERROR: Bad probability for \"ordered\" option in " << parameters << "!\n";
         exit(1);
      }
      trafficSpec.OrderedMode = dblValue;
   }
   else if(sscanf(parameters, "unordered=%lf%n", &dblValue, &n) == 1) {
      if((dblValue < 0.0) || (dblValue > 1.0)) {
         std::cerr << "ERROR: Bad probability for \"unordered\" option in " << parameters << "!\n";
         exit(1);
      }
      trafficSpec.OrderedMode = 1.0 - dblValue;
   }
   else if(sscanf(parameters, "reliable=%lf%n", &dblValue, &n) == 1) {
      if((dblValue < 0.0) || (dblValue > 1.0)) {
         std::cerr << "ERROR: Bad probability for \"reliable\" option in " << parameters << "!\n";
         exit(1);
      }
      trafficSpec.ReliableMode = dblValue;
   }
   else if(sscanf(parameters, "unreliable=%lf%n", &dblValue, &n) == 1) {
      if((dblValue < 0.0) || (dblValue > 1.0)) {
         std::cerr << "ERROR: Bad probability for \"unreliable\" option in " << parameters << "!\n";
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
   }
   else if(sscanf(parameters, "sndbuf=%u%n", &intValue, &n) == 1) {
      trafficSpec.SndBufferSize = (uint32_t)intValue;
   }
   else if(strncmp(parameters, "cmt=", 4) == 0) {
      unsigned int cmt;
      int          pos;
      if(strncmp((const char*)&parameters[4], "off", 3) == 0) {
         trafficSpec.CMT = NPAF_PRIMARY_PATH;
         n = 4 + 3;
      }
      else if(strncmp((const char*)&parameters[4], "cmtrpv1", 7) == 0) {
         trafficSpec.CMT = NPAF_CMTRPv1;
         n = 4 + 7;
      }
      else if(strncmp((const char*)&parameters[4], "cmtrpv2", 7) == 0) {
         trafficSpec.CMT = NPAF_CMTRPv2;
         n = 4 + 7;
      }
      else if(strncmp((const char*)&parameters[4], "cmt", 3) == 0) {
         trafficSpec.CMT = NPAF_CMT;
         n = 4 + 3;
      }
      else if( (strncmp((const char*)&parameters[4], "like-mptcp", 10) == 0) ||
               (strncmp((const char*)&parameters[4], "mptcp-like", 10) == 0) ) {
         trafficSpec.CMT = NPAF_LikeMPTCP;
         n = 4 + 10;
      }
      else if(strncmp((const char*)&parameters[4], "mptcp", 5) == 0) {
         trafficSpec.CMT = NPAF_LikeMPTCP;
         n = 4 + 5;
      }
      else if(strncmp((const char*)&parameters[4], "normal", 6) == 0) {   // Legacy: use "cmt"!
         trafficSpec.CMT = NPAF_CMT;
         n = 4 + 6;
      }
      else if(strncmp((const char*)&parameters[4], "rp", 2) == 0) {   // Legacy: use "cmtrpv1"!
         trafficSpec.CMT = NPAF_CMTRPv1;
         n = 4 + 2;
      }
      else if(sscanf((const char*)&parameters[4], "%u%n", &cmt, &pos) == 1) {
         trafficSpec.CMT = cmt;
         n = 4 + pos;
      }
      else {
         std::cerr << "ERROR: Invalid \"cmt\" setting: " << (const char*)&parameters[4] << "!\n";
         exit(1);
      }

      // ------ Set correct "pseudo-protocol" for MPTCP ---------------------
      if( (trafficSpec.Protocol == IPPROTO_TCP) ||
          (trafficSpec.Protocol == IPPROTO_MPTCP) ) {
         if(trafficSpec.CMT == NPAF_PRIMARY_PATH) {
            trafficSpec.Protocol = IPPROTO_TCP;
         }
         else {
            trafficSpec.Protocol = IPPROTO_MPTCP;
            if(trafficSpec.CMT != NPAF_LikeMPTCP) {
               std::cerr << "WARNING: Invalid \"cmt\" setting: " << (const char*)&parameters[4]
                         << " for MPTCP! Using default instead!\n";
            }
         }
      }
      // --------------------------------------------------------------------
   }
   else if(strncmp(parameters, "ccid=", 5) == 0) {
      unsigned int ccid;
      int          pos;
      if(sscanf((const char*)&parameters[5], "%u%n", &ccid, &pos) == 1) {
         trafficSpec.CCID = ccid;
         n = 5 + pos;
      }
      else {
         std::cerr << "ERROR: Invalid \"ccid\" setting: " << (const char*)&parameters[5] << "!\n";
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
         std::cerr << "ERROR: Invalid \"error_on_abort\" setting: " << (const char*)&parameters[4] << "!\n";
         exit(1);
      }
   }
   else if(strncmp(parameters, "v6only", 6) == 0) {
      trafficSpec.BindV6Only = true;
      n = 6;
   }
   else if(sscanf(parameters, "description=%255[^:]s%n", (char*)&description, &n) == 1) {
      trafficSpec.Description = std::string(description);
      n = 12 + strlen(description);
   }
   else if(strncmp(parameters,"onoff=", 6) == 0) {
      size_t m = 5;
      trafficSpec.RepeatOnOff = false;
      while( (parameters[m] != 0x00) && (parameters[m] != ':') ) {
         m++;

         OnOffEvent event;
         event.RandNumGen    = 0x00;
         event.RelativeTime  = false;
         for(size_t i = 0; i < sizeof(event.ValueArray) / sizeof(event.ValueArray[0]); i++) {
            event.ValueArray[i] = 0.0;
         }
         if(parameters[m] == '+') {
            event.RelativeTime = true;
            m++;
         }
         else if(strncmp((const char*)&parameters[m], "repeat", 6) == 0) {
            m += 6;
            trafficSpec.RepeatOnOff = true;
            break;
         }
         const char* p = parseNextEntry((const char*)&parameters[m], (double*)&event.ValueArray, (uint8_t*)&event.RandNumGen);
         if(p != nullptr) {
            m += (long)p - (long)&parameters[m + 1];
         }
         else {
            m += strlen((const char*)&parameters[m]);
         }

         trafficSpec.OnOffEvents.push_back(event);
      }
      if(trafficSpec.RepeatOnOff == true) {
         if((trafficSpec.OnOffEvents.size() % 2) != 0) {
            std::cerr << "ERROR: Repeated on/off traffic needs even number of events!\n";
            exit(1);
         }
         for(std::vector<OnOffEvent>::const_iterator iterator = trafficSpec.OnOffEvents.begin();
            iterator != trafficSpec.OnOffEvents.end();iterator++) {
            if((*iterator).RelativeTime == false) {
               std::cerr << "ERROR: Repeated on/off traffic needs relative times!\n";
               exit(1);
            }
         }
      }
      n = m;
   }
   else if(strncmp(parameters, "nodelay=", 8) == 0) {
      if(strncmp((const char*)&parameters[8], "on", 2) == 0) {
         trafficSpec.NoDelay = true;
         n = 8 + 2;
      }
      else if(strncmp((const char*)&parameters[8], "off", 3) == 0) {
         trafficSpec.NoDelay = false;
         n = 8 + 3;
      }
      else {
         std::cerr << "ERROR: Invalid \"nodelay\" setting: " << (const char*)&parameters[8] << "!\n";
         exit(1);
      }
   }
   else if(strncmp(parameters, "debug=", 6) == 0) {
      if(strncmp((const char*)&parameters[6], "on", 2) == 0) {
         trafficSpec.Debug = true;
         n = 6 + 2;
      }
      else if(strncmp((const char*)&parameters[6], "off", 3) == 0) {
         trafficSpec.Debug = false;
         n = 6 + 3;
      }
      else {
         std::cerr << "ERROR: Invalid \"debug\" setting: " << (const char*)&parameters[6] << "!\n";
         exit(1);
      }
   }
   else if(strncmp(parameters, "ndiffports=", 11) == 0) {
      unsigned int nDiffPorts;
      int          pos;
      if(sscanf((const char*)&parameters[11], "%u%n", &nDiffPorts, &pos) == 1) {
         trafficSpec.NDiffPorts = (uint16_t)nDiffPorts;
         n = 11 + pos;
      }
   }
   else if(strncmp(parameters, "pathmgr=", 8) == 0) {
      char   pathMgr[NETPERFMETER_PATHMGR_LENGTH + 1];
      size_t i = 0;
      while(i < NETPERFMETER_PATHMGR_LENGTH) {
         if( (parameters[8 + i] == ':') ||
             (parameters[8 + i] == 0x00) ) {
            break;
         }
         pathMgr[i] = parameters[8 + i];
         i++;
      }
      pathMgr[i] = 0x00;
      if( (parameters[8 + i] != ':') && (parameters[8 + i] != 0x00) ) {
         std::cerr << "ERROR: Invalid \"pathmgr\" setting: " << (const char*)&parameters[8]
                   << " - name too long!\n";
          exit(1);
      }
      trafficSpec.PathMgr = std::string((const char*)&pathMgr);
      n = 8 + strlen((const char*)&pathMgr);
   }
   else if(strncmp(parameters, "scheduler=", 10) == 0) {
      char   scheduler[NETPERFMETER_SCHEDULER_LENGTH + 1];
      size_t i = 0;
      while(i < NETPERFMETER_SCHEDULER_LENGTH) {
         if( (parameters[10 + i] == ':') ||
             (parameters[10 + i] == 0x00) ) {
            break;
         }
         scheduler[i] = parameters[10 + i];
         i++;
      }
      scheduler[i] = 0x00;
      if( (parameters[10 + i] != ':') && (parameters[10 + i] != 0x00) ) {
         std::cerr << "ERROR: Invalid \"scheduler\" setting: " << (const char*)&parameters[10]
                   << " - name too long!\n";
          exit(1);
      }
      trafficSpec.Scheduler = std::string((const char*)&scheduler);
      n = 10 + strlen((const char*)&scheduler);
   }
   else if(strncmp(parameters, "cc=", 3) == 0) {
      char   congestionControl[NETPERFMETER_CC_LENGTH + 1];
      size_t i = 0;
      while(i < NETPERFMETER_CC_LENGTH) {
         if( (parameters[3 + i] == ':') ||
             (parameters[3 + i] == 0x00) ) {
            break;
         }
         congestionControl[i] = parameters[3 + i];
         i++;
      }
      congestionControl[i] = 0x00;
      if( (parameters[3 + i] != ':') && (parameters[3 + i] != 0x00) ) {
         std::cerr << "ERROR: Invalid \"pathmgr\" setting: " << (const char*)&parameters[8]
                   << " - name too long!\n";
          exit(1);
      }
      trafficSpec.CongestionControl = std::string((const char*)&congestionControl);
      n = 3 + strlen((const char*)&congestionControl);
   }
   else {
      std::cerr << "ERROR: Invalid option \"" << parameters << "\"!\n";
      exit(1);
   }
   if(parameters[n] == 0x00) {
      return nullptr;
   }
   return (const char*)&parameters[n + 1];
}


// ###### Create Flow for new flow ##########################################
static Flow* createFlow(Flow*                  previousFlow,
                        const char*            parameters,
                        const uint64_t         measurementID,
                        const char*            vectorNamePattern,
                        const OutputFileFormat vectorFileFormat,
                        const uint8_t          initialProtocol,
                        const sockaddr_union&  remoteAddress)
{
   // ====== Get flow ID and stream ID ======================================
   static uint32_t flowID   = 0; // will be increased with each successfull call
   uint16_t        streamID = (previousFlow != nullptr) ? previousFlow->getStreamID() + 1 : 0;

   // ====== Get FlowTrafficSpec ============================================
   FlowTrafficSpec trafficSpec;
   trafficSpec.Protocol = initialProtocol;
   if(strncmp(parameters, "default", 7) == 0) {
      trafficSpec.OutboundFrameRateRng = RANDOM_CONSTANT;
      trafficSpec.OutboundFrameRate[0] = 0.0;
      switch(trafficSpec.Protocol) {
         case IPPROTO_TCP:
         case IPPROTO_MPTCP:
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
                  while( (parameters = parseTrafficSpecOption(parameters, trafficSpec, flowID)) ) { }
               }
            }
         }
      }
   }
   if(trafficSpec.Description == "") {
      trafficSpec.Description = format("Flow %u", flowID);
   }

   // ====== Create new flow ================================================
   if(FlowManager::getFlowManager()->findFlow(measurementID, flowID, streamID) != nullptr) {
      std::cerr << "ERROR: Flow ID " << flowID << " is used twice. Ensure correct id=<ID> parameters!\n";
      exit(1);
   }
   Flow* flow = new Flow(measurementID, flowID, streamID, trafficSpec);
   assert(flow != nullptr);

   // ====== Initialize vector file =========================================
   const std::string vectorName = flow->getNodeOutputName(vectorNamePattern,
                                                          "active",
                                                          format("-%08x-%04x", flowID, streamID));
   if(!flow->initializeVectorFile(vectorName.c_str(), vectorFileFormat)) {
      std::cerr << "ERROR: Unable to create vector file <" << vectorName << ">!\n";
      exit(1);
   }

   // ====== MPTCP: choose MPTCP socket instead of TCP socket ============
   sockaddr_union destinationAddress = remoteAddress;
   if(trafficSpec.Protocol == IPPROTO_MPTCP) {
      setPort(&destinationAddress.sa, getPort(&destinationAddress.sa) - 1);
   }

   // ====== Print information ==============================================
   LOG_INFO
   stdlog << format("Flow #%u: connecting %s socket to",
                     flow->getFlowID(), getProtocolName(trafficSpec.Protocol)) << " ";
   printAddress(stdlog, &destinationAddress.sa, true);
   stdlog << " from ";
   if(gLocalDataAddresses > 0) {
      for(unsigned int i = 0;i < gLocalDataAddresses;i++) {
         if(i > 0) {
            stdlog << ", ";
         }
         printAddress(stdlog, &gLocalDataAddressArray[i].sa, false);
      }
   }
   else {
      stdlog << "(any)";
   }
   stdlog << " ...\n";
   LOG_END

   // ====== Set up socket ==================================================
   int  socketDescriptor;
   bool originalSocketDescriptor;

   if(previousFlow) {
      originalSocketDescriptor = false;
      socketDescriptor         = previousFlow->getSocketDescriptor();
      LOG_INFO
      stdlog << format("Flow #%u: connected socket %d",
                       flow->getFlowID(), socketDescriptor) << "\n";
      LOG_END
   }
   else {
      originalSocketDescriptor = true;
      switch(trafficSpec.Protocol) {
         case IPPROTO_SCTP:
            socketDescriptor = createAndBindSocket(remoteAddress.sa.sa_family, SOCK_STREAM, IPPROTO_SCTP, 0,
                                                   gLocalDataAddresses, (const sockaddr_union*)&gLocalDataAddressArray, false,
                                                   trafficSpec.BindV6Only);
           break;
         case IPPROTO_TCP:
            socketDescriptor = createAndBindSocket(remoteAddress.sa.sa_family, SOCK_STREAM, IPPROTO_TCP, 0,
                                                   gLocalDataAddresses, (const sockaddr_union*)&gLocalDataAddressArray, false,
                                                   trafficSpec.BindV6Only);
           break;
#ifdef HAVE_MPTCP
         case IPPROTO_MPTCP:
            socketDescriptor = createAndBindSocket(remoteAddress.sa.sa_family, SOCK_STREAM, IPPROTO_MPTCP, 0,
                                                   gLocalDataAddresses, (const sockaddr_union*)&gLocalDataAddressArray, false,
                                                   trafficSpec.BindV6Only);
           break;
#endif
         case IPPROTO_UDP:
            socketDescriptor = createAndBindSocket(remoteAddress.sa.sa_family, SOCK_DGRAM, IPPROTO_UDP, 0,
                                                   gLocalDataAddresses, (const sockaddr_union*)&gLocalDataAddressArray, false,
                                                   trafficSpec.BindV6Only);
           break;
#ifdef HAVE_DCCP
         case IPPROTO_DCCP:
            socketDescriptor = createAndBindSocket(remoteAddress.sa.sa_family, SOCK_DCCP, IPPROTO_DCCP, 0,
                                                   gLocalDataAddresses, (const sockaddr_union*)&gLocalDataAddressArray, false,
                                                   trafficSpec.BindV6Only);
           break;
#endif
         default:
            abort();

      }
      if(socketDescriptor < 0) {
         std::cerr << "ERROR: Unable to create " << getProtocolName(trafficSpec.Protocol)
                   << " socket - " << strerror(errno) << "!\n";
         exit(1);
      }

      // ====== Establish connection ========================================
      if(trafficSpec.Protocol == IPPROTO_SCTP) {
         // ------ Set SCTP stream parameters--------------------------------
         sctp_initmsg initmsg;
         memset((char*)&initmsg, 0 ,sizeof(initmsg));
         initmsg.sinit_num_ostreams  = 65535;
         initmsg.sinit_max_instreams = 65535;
         if(ext_setsockopt(socketDescriptor, IPPROTO_SCTP, SCTP_INITMSG,
                           &initmsg, sizeof(initmsg)) < 0) {
            std::cerr << "ERROR: Failed to configure INIT parameters on SCTP socket (SCTP_INITMSG option) - "
                      << strerror(errno) << "!\n";
            exit(1);
         }

         // ------ Enable SCTP events ---------------------------------------
         sctp_event_subscribe events;
         memset((char*)&events, 0 ,sizeof(events));
         events.sctp_data_io_event = 1;
         if(ext_setsockopt(socketDescriptor, IPPROTO_SCTP, SCTP_EVENTS,
                           &events, sizeof(events)) < 0) {
            std::cerr << "ERROR: Failed to configure events on SCTP socket (SCTP_EVENTS option) - "
                      << strerror(errno) << "!\n";
            exit(1);
         }
      }

      if(flow->configureSocket(socketDescriptor) == false) {
         exit(1);
      }
      if(ext_connect(socketDescriptor, &destinationAddress.sa, getSocklen(&destinationAddress.sa)) < 0) {
         std::cerr << "ERROR: Unable to connect " << getProtocolName(trafficSpec.Protocol)
                   << " socket - " << strerror(errno) << "!\n";
         exit(1);
      }

      LOG_TRACE
      stdlog << "okay <sd=" << socketDescriptor << ">\n";
      LOG_END
   }

   // ====== Update flow with socket descriptor =============================
   flow->setSocketDescriptor(socketDescriptor, originalSocketDescriptor);

   flowID++;
   return flow;
}


// ###### Initialize pollFD entry in main loop ##############################
static void addToPollFDs(pollfd* pollFDs, const int fd, int& n, int* index = nullptr)
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
   pollfd                 fds[5 + gMessageReader.size()];
   int                    n       = 0;
   int                    tcpID   = -1;
   int                    mptcpID = -1;
   int                    udpID   = -1;
   int                    sctpID  = -1;
   int                    dccpID  = -1;
   unsigned long long     now     = getMicroTime();
   std::map<int, pollfd*> pollFDIndex;


   // ====== Get parameters for poll() ======================================
   addToPollFDs((pollfd*)&fds, gTCPSocket,     n, &tcpID);
   addToPollFDs((pollfd*)&fds, gMPTCPSocket,   n, &mptcpID);
   addToPollFDs((pollfd*)&fds, gUDPSocket,     n, &udpID);
   addToPollFDs((pollfd*)&fds, gSCTPSocket,    n, &sctpID);
   addToPollFDs((pollfd*)&fds, gDCCPSocket,    n, &dccpID);
   int    controlFDSet[gMessageReader.size()];
   size_t controlFDs = gMessageReader.getAllSDs((int*)&controlFDSet, sizeof(controlFDSet) / sizeof(int));
   const int controlIDMin = n;
   for(size_t i = 0; i < controlFDs; i++) {
      addToPollFDs((pollfd*)&fds, controlFDSet[i], n, nullptr);
   }
   const int controlIDMax = n - 1;


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
      int controlID;
      for(controlID = controlIDMin; controlID <= controlIDMax; controlID++) {
         if(fds[controlID].revents & POLLIN) {
            if( (isActiveMode == false) &&
                (fds[controlID].fd == gControlSocket) ) {
               const int newSD = ext_accept(gControlSocket, nullptr, 0);
               if(newSD >= 0) {
                  gMessageReader.registerSocket(IPPROTO_SCTP, newSD);
               }
               else {
                  LOG_ERROR
                  stdlog << format("Accept on SCTP control socket %d failed: %s!",
                                   gControlSocket, strerror(errno)) << "\n";
                  LOG_END
               }
            }
            else if( (isActiveMode == false) &&
                (fds[controlID].fd == gControlSocketTCP) ) {
               const int newSD = ext_accept(gControlSocketTCP, nullptr, 0);
               if(newSD >= 0) {
                  gMessageReader.registerSocket(IPPROTO_TCP, newSD);
               }
               else {
                  LOG_ERROR
                  stdlog << format("Accept on TCP control socket %d failed: %s!",
                                   gControlSocketTCP, strerror(errno)) << "\n";
                  LOG_END
               }
            }
            else {
               const bool controlOkay = handleNetPerfMeterControlMessage(
                                           &gMessageReader, fds[controlID].fd);
               if(!controlOkay) {
                  if(isActiveMode) {
                     return false;
                  }
                  // Make sure to deregister all flows belonging to this
                  // control connection!
                  handleControlAssocShutdown(fds[controlID].fd);
                  gMessageReader.deregisterSocket(fds[controlID].fd);
                  ext_close(fds[controlID].fd);
               }
            }
         }
      }

      // ====== Incoming data message =======================================
      if( (tcpID >= 0) && (fds[tcpID].revents & POLLIN) ) {
         const int newSD = ext_accept(gTCPSocket, nullptr, 0);
         if(newSD >= 0) {
            FlowManager::getFlowManager()->addSocket(IPPROTO_TCP, newSD);
         }
      }
      if( (mptcpID >= 0) && (fds[mptcpID].revents & POLLIN) ) {
         const int newSD = ext_accept(gMPTCPSocket, nullptr, 0);
         if(newSD >= 0) {
            FlowManager::getFlowManager()->addSocket(IPPROTO_MPTCP, newSD);
         }
      }
      if( (udpID >= 0) && (fds[udpID].revents & POLLIN) ) {
         FlowManager::getFlowManager()->lock();
         handleNetPerfMeterData(isActiveMode, now, IPPROTO_UDP, gUDPSocket);
         FlowManager::getFlowManager()->unlock();
      }
      if( (sctpID >= 0) && (fds[sctpID].revents & POLLIN) ) {
         const int newSD = ext_accept(gSCTPSocket, nullptr, 0);
         if(newSD >= 0) {
            FlowManager::getFlowManager()->addSocket(IPPROTO_SCTP, newSD);
         }
      }
#ifdef HAVE_DCCP
      if( (dccpID >= 0) && (fds[dccpID].revents & POLLIN) ) {
         const int newSD = ext_accept(gDCCPSocket, nullptr, 0);
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
   return true;
}



// ###### Passive Mode ######################################################
void passiveMode(int argc, char** argv, const uint16_t localPort)
{
   // ====== Set options ====================================================
   for(int i = 2;i < argc;i++) {
      if(!handleGlobalParameter(argv[i])) {
         LOG_FATAL
         stdlog << format("Invalid argument %s!", argv[i]) << "\n";
         LOG_END_FATAL
      }
   }
   printGlobalParameters();


   // ====== Test for problems ==============================================
   sockaddr_union testAddress;
   assert(string2address("172.17.0.1:0", &testAddress));
   int testSD = createAndBindSocket(AF_INET, SOCK_STREAM, IPPROTO_SCTP, 0, 1, &testAddress, true, false);
   if(testSD >= 0) {
      LOG_WARNING
      stdlog << "NOTE: This machine seems to have an interface with address 172.17.0.1! This is typically used by Docker setups. If you connect from another machine having the same configuration, in an environment with only private addresses SCTP may try to use this address -> OOTB ABORT." << "\n";
      LOG_END
      ext_close(testSD);
   }

   // ====== Initialize control socket ======================================
   gControlSocket = createAndBindSocket(AF_UNSPEC, SOCK_STREAM, IPPROTO_SCTP,
                                        localPort + 1,
                                        gLocalControlAddresses, (const sockaddr_union*)&gLocalControlAddressArray,
                                        true, gBindV6Only);
   if(gControlSocket < 0) {
      LOG_FATAL
      stdlog << format("Failed to create and bind SCTP socket on port %d: %s!",
                       localPort + 1, strerror(errno)) << "\n";
      LOG_END_FATAL
   }
   sctp_event_subscribe events;
   if(!gControlOverTCP) {
      // ------ Set default send parameters ---------------------------------
      sctp_sndrcvinfo sinfo;
      memset(&sinfo, 0, sizeof(sinfo));
      sinfo.sinfo_ppid = htonl(PPID_NETPERFMETER_CONTROL);
      if(ext_setsockopt(gControlSocket, IPPROTO_SCTP, SCTP_DEFAULT_SEND_PARAM, &sinfo, sizeof(sinfo)) < 0) {
         LOG_FATAL
         stdlog << format("Failed to configure default send parameters (SCTP_DEFAULT_SEND_PARAM option) on SCTP control socket %d: %s!",
                          gControlSocket, strerror(errno)) << "\n";
         LOG_END_FATAL
      }

      // ------ Enable SCTP events ------------------------------------------
      memset((char*)&events, 0 ,sizeof(events));
      events.sctp_data_io_event     = 1;
      events.sctp_association_event = 1;
      if(ext_setsockopt(gControlSocket, IPPROTO_SCTP, SCTP_EVENTS, &events, sizeof(events)) < 0) {
         LOG_FATAL
         stdlog << format("Failed to configure events (SCTP_EVENTS option) on SCTP socket %d: %s!",
                           gControlSocket, strerror(errno)) << "\n";
         LOG_END_FATAL
      }
   }
   gMessageReader.registerSocket(IPPROTO_SCTP, gControlSocket);

   gControlSocketTCP = createAndBindSocket(AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP,
                                           localPort + 1, 0, nullptr, true, gBindV6Only);
   if(gControlSocketTCP < 0) {
      LOG_FATAL
      stdlog << format("Failed to create and bind TCP socket on port %d: %s!",
                       localPort + 1, strerror(errno)) << "\n";
      LOG_END_FATAL
   }
   gMessageReader.registerSocket(IPPROTO_TCP, gControlSocketTCP);

   // ====== Initialize data socket for each protocol =======================
   gTCPSocket = createAndBindSocket(AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, localPort,
                                    gLocalDataAddresses, (const sockaddr_union*)&gLocalDataAddressArray, true, gBindV6Only);
   if(gTCPSocket < 0) {
      LOG_FATAL
      stdlog << format("Failed to create and bind TCP socket on port %d: %s!",
                       localPort, strerror(errno)) << "\n";
      LOG_END_FATAL
   }
   if(setBufferSizes(gTCPSocket, gSndBufSize, gRcvBufSize) == false) {
      LOG_FATAL
      stdlog << format("Failed to configure buffer sizes on TCP socket %d!",
                        gSCTPSocket) << "\n";
      LOG_END_FATAL
   }

#ifdef HAVE_MPTCP
   gMPTCPSocket = createAndBindSocket(AF_UNSPEC, SOCK_STREAM, IPPROTO_MPTCP, localPort - 1,
                                      gLocalDataAddresses, (const sockaddr_union*)&gLocalDataAddressArray, false, gBindV6Only);
   if(gMPTCPSocket < 0) {
      LOG_DEBUG
      stdlog << format("NOTE: Failed to create and bind MPTCP socket on port %d: %s!",
                        localPort - 1, strerror(errno)) << "\n";
      LOG_END
   }
   else {
      if (ext_setsockopt(gMPTCPSocket, IPPROTO_TCP, MPTCP_PATH_MANAGER_LEGACY, gPathMgr, strlen(gPathMgr)) < 0) {
         if (ext_setsockopt(gMPTCPSocket, IPPROTO_TCP, MPTCP_PATH_MANAGER, gPathMgr, strlen(gPathMgr)) < 0) {
            if(strcmp(gPathMgr, "default") != 0) {
               LOG_WARNING
               stdlog << format("Failed to configure path manager %s (MPTCP_PATH_MANAGER option) on MPTCP socket %d: %s!",
                                gPathMgr, gMPTCPSocket, strerror(errno)) << "\n";
               LOG_END
            }
         }
      }
      if (ext_setsockopt(gMPTCPSocket, IPPROTO_TCP, MPTCP_SCHEDULER_LEGACY, gScheduler, strlen(gScheduler)) < 0) {
         if (ext_setsockopt(gMPTCPSocket, IPPROTO_TCP, MPTCP_SCHEDULER, gScheduler, strlen(gScheduler)) < 0) {
            if(strcmp(gScheduler, "default") != 0) {
               LOG_WARNING
               stdlog << format("Failed to configure scheduler %s (MPTCP_SCHEDULER option) on MPTCP socket %d: %s!",
                                gScheduler, gMPTCPSocket, strerror(errno)) << "\n";
               LOG_END
            }
         }
      }
      if(setBufferSizes(gMPTCPSocket, gSndBufSize, gRcvBufSize) == false) {
         LOG_FATAL
         stdlog << format("Failed to configure buffer sizes on MPTCP socket %d!",
                          gMPTCPSocket) << "\n";
         LOG_END_FATAL
      }
      ext_listen(gMPTCPSocket, 100);
   }
#endif

   gUDPSocket = createAndBindSocket(AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP, localPort,
                                    gLocalDataAddresses, (const sockaddr_union*)&gLocalDataAddressArray, true, gBindV6Only);
   if(gUDPSocket < 0) {
      std::cerr << "ERROR: Failed to create and bind UDP socket on port " << localPort << " - "
                << strerror(errno) << "!\n";
      exit(1);
   }
   // NOTE: For connection-less UDP, the FlowManager takes care of the socket!
   FlowManager::getFlowManager()->addSocket(IPPROTO_UDP, gUDPSocket);

#ifdef HAVE_DCCP
   gDCCPSocket = createAndBindSocket(AF_UNSPEC, SOCK_DCCP, IPPROTO_DCCP, localPort,
                                     gLocalDataAddresses, (const sockaddr_union*)&gLocalDataAddressArray, true, gBindV6Only);
   if(gDCCPSocket < 0) {
      LOG_INFO
      stdlog << format("NOTE: Failed to create and bind DCCP socket on port %d: %s!",
                        localPort, strerror(errno)) << "\n";
      LOG_END
   }
   else {
      const uint32_t service[1] = { htonl(SC_NETPERFMETER_DATA) };
      if(ext_setsockopt(gDCCPSocket, SOL_DCCP, DCCP_SOCKOPT_SERVICE, &service, sizeof(service)) < 0) {
         LOG_FATAL
         stdlog << format("Failed to configure DCCP service code %u (DCCP_SOCKOPT_SERVICE option) on DCCP socket: %s!",
                           (unsigned int)ntohl(service[0]), gDCCPSocket, strerror(errno)) << "\n";
         LOG_END_FATAL
      }
      if(setBufferSizes(gDCCPSocket, gSndBufSize, gRcvBufSize) == false) {
         LOG_FATAL
         stdlog << format("Failed to configure buffer sizes on DCCP socket %d!",
                          gDCCPSocket) << "\n";
         LOG_END_FATAL
      }
   }
#endif

   gSCTPSocket = createAndBindSocket(AF_UNSPEC, SOCK_STREAM, IPPROTO_SCTP, localPort,
                                     gLocalDataAddresses, (const sockaddr_union*)&gLocalDataAddressArray, true, gBindV6Only);
   if(gSCTPSocket < 0) {
      std::cerr << "ERROR: Failed to create and bind SCTP socket on port " << localPort << " - "
                << strerror(errno) << "!\n";
      exit(1);
   }

   // ------ Set SCTP stream parameters--------------------------------------
   sctp_initmsg initmsg;
   memset((char*)&initmsg, 0 ,sizeof(initmsg));
   initmsg.sinit_num_ostreams  = 65535;
   initmsg.sinit_max_instreams = 65535;
   if(ext_setsockopt(gSCTPSocket, IPPROTO_SCTP, SCTP_INITMSG,
                     &initmsg, sizeof(initmsg)) < 0) {
      LOG_FATAL
      stdlog << format("Failed to configure INIT parameters (SCTP_INITMSG option) on SCTP socket %d: %s!",
                        gSCTPSocket, strerror(errno)) << "\n";
      LOG_END_FATAL
   }

   // ------ Enable SCTP events ---------------------------------------------
   memset((char*)&events, 0 ,sizeof(events));
   events.sctp_data_io_event = 1;
   if(ext_setsockopt(gSCTPSocket, IPPROTO_SCTP, SCTP_EVENTS, &events, sizeof(events)) < 0) {
      LOG_FATAL
      stdlog << format("Failed to configure events (SCTP_EVENTS option) on SCTP socket %d: %s!",
                        gSCTPSocket, strerror(errno)) << "\n";
      LOG_END_FATAL
   }

   // ------ Set SCTP buffer sizes ------------------------------------------
   if(setBufferSizes(gSCTPSocket, gSndBufSize, gRcvBufSize) == false) {
      LOG_FATAL
      stdlog << format("Failed to configure buffer sizes on SCTP socket %d!",
                        gSCTPSocket) << "\n";
      LOG_END_FATAL
   }


   // ====== Print status ===================================================
   LOG_INFO
   stdlog << format("Passive Mode: Accepting TCP%s/UDP/SCTP%s connections on port %u",
                    ((gMPTCPSocket > 0) ? "+MPTCP" : ""),
                    ((gDCCPSocket > 0)  ? "/DCCP"  : ""),
                    localPort) << "\n";
   LOG_END


   // ====== Main loop ======================================================
   signal(SIGPIPE, SIG_IGN);
   installBreakDetector();
   if(gDisplayEnabled) {
      FlowManager::getFlowManager()->enableDisplay();
   }

   const unsigned long long stopAt = (gRuntime > 0) ?
      (getMicroTime() + (unsigned long long)rint(gRuntime * 1000000.0)) : ~0ULL;
   while( (!breakDetected()) && (!gStopTimeReached) ) {
      mainLoop(false, stopAt, 0);
   }

   FlowManager::getFlowManager()->disableDisplay();


   // ====== Clean up =======================================================
   gMessageReader.deregisterSocket(gControlSocketTCP);
   ext_close(gControlSocketTCP);
   gMessageReader.deregisterSocket(gControlSocket);
   ext_close(gControlSocket);
   ext_close(gTCPSocket);
   if(gMPTCPSocket >= 0) {
      ext_close(gMPTCPSocket);
   }
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
      LOG_FATAL
      std::cerr << format("ERROR: Invalid remote address %s", argv[1]) << "!\n";
      LOG_END_FATAL
   }
   if(getPort(&remoteAddress.sa) < 2) {
      setPort(&remoteAddress.sa, 9000);
   }
   else if(getPort(&remoteAddress.sa) > 65534) {
      setPort(&remoteAddress.sa, 65534);
   }
   sockaddr_union controlAddress = remoteAddress;
   setPort(&controlAddress.sa, getPort(&remoteAddress.sa) + 1);


   // ====== Initialize IDs and print status ================================
   uint64_t measurementID = random64();

   LOG_INFO
   stdlog << "Active Mode:\n"
          << "   - Measurement ID  = " << format("%lx", measurementID) << "\n"
          << "   - Remote Address  = ";
   printAddress(stdlog, &remoteAddress.sa, true);
   stdlog << "\n"
          << "   - Control Address = ";
   printAddress(stdlog, &controlAddress.sa, true);
   stdlog << " - connecting ..." << "\n";
   LOG_END

   // ====== Initialize control socket ======================================
   for(int i = 2;i < argc;i++) {
      if(strcmp(argv[i], "-control-over-tcp") == 0) {
         gControlOverTCP = true;
      }
   }
   const int controlSocketProtocol = (gControlOverTCP == false) ? IPPROTO_SCTP : IPPROTO_TCP;
   gControlSocket = createAndBindSocket(controlAddress.sa.sa_family, SOCK_STREAM, controlSocketProtocol,
                                        0, gLocalControlAddresses, (const sockaddr_union*)&gLocalControlAddressArray,
                                        false, gBindV6Only);
   if(gControlSocket < 0) {
      LOG_FATAL
      stdlog << format("Failed to create and bind control socket: %s!",
                       strerror(errno)) << "\n";
      LOG_END_FATAL
   }
   if(gControlOverTCP == false) {
      sctp_sndrcvinfo sinfo;
      memset(&sinfo, 0, sizeof(sinfo));
      sinfo.sinfo_ppid = htonl(PPID_NETPERFMETER_CONTROL);
      if(ext_setsockopt(gControlSocket, IPPROTO_SCTP, SCTP_DEFAULT_SEND_PARAM, &sinfo, sizeof(sinfo)) < 0) {
         LOG_FATAL
         stdlog << format("Failed to configure default send parameters (SCTP_DEFAULT_SEND_PARAM) on SCTP control socket: %s!",
                          strerror(errno)) << "\n";
         LOG_END_FATAL
      }
   }
   if(ext_connect(gControlSocket, &controlAddress.sa, getSocklen(&controlAddress.sa)) < 0) {
      LOG_FATAL
      stdlog << format("Unable to establish control association: %s!",
                       strerror(errno)) << "\n";
      LOG_END_FATAL
   }
   if(gControlOverTCP == false) {
      sctp_paddrparams paddr;
      memset(&paddr, 0, sizeof(paddr));
      memcpy(&paddr.spp_address, &controlAddress.sa, getSocklen(&controlAddress.sa));
      paddr.spp_flags      = SPP_HB_ENABLE;
      paddr.spp_hbinterval = 30000;
      if(setsockopt(gControlSocket, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &paddr, sizeof(paddr)) < 0) {
         LOG_WARNING
         stdlog << format("Unable to enable heartbeats on control association: %s!",
                          strerror(errno)) << "\n";
         LOG_END
      }
   }
   LOG_TRACE
   stdlog << "<okay; sd=" << gControlSocket << ">\n";
   LOG_END
   gMessageReader.registerSocket(controlSocketProtocol, gControlSocket);


   // ====== Handle command-line parameters =================================
   bool             hasFlow           = false;
   const char*      vectorNamePattern = "";
   OutputFileFormat vectorFileFormat  = OFF_None;
   const char*      scalarNamePattern = "";
   OutputFileFormat scalarFileFormat  = OFF_None;
   const char*      configName        = "";
   uint8_t          protocol          = 0;
   Flow*            lastFlow          = nullptr;

   // ------ Handle other parameters ----------------------------------------
   for(int i = 2;i < argc;i++) {
      if(handleGlobalParameter(argv[i])) {
         // Parameter has been handled in handleGlobalParameter()!
      }
      else if(argv[i][0] == '-') {
         lastFlow = nullptr;
         if(strcmp(argv[i], "-tcp") == 0) {
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
            LOG_FATAL
            stdlog << "ERROR: DCCP support is not compiled in!" << "\n";
            LOG_END_FATAL
#endif
         }
         else if(strncmp(argv[i], "-vector=", 8) == 0) {
            if(hasFlow) {
               LOG_FATAL
               stdlog << "ERROR: Vector file must be set *before* first flow!" << "\n";
               LOG_END_FATAL
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
               LOG_FATAL
               stdlog << "ERROR: Scalar file must be set *before* first flow!" << "\n";
               LOG_END_FATAL
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
            LOG_FATAL
            stdlog << format("Invalid argument %s!", argv[i]) << "\n";
            LOG_END_FATAL
         }
      }
      else {
         if(protocol == 0) {
            LOG_FATAL
            stdlog << format("Protocol specification needed before flow specification at argument %s!",
                             argv[i]) << "\n";
            LOG_END_FATAL
         }

         lastFlow = createFlow(lastFlow, argv[i], measurementID,
                               vectorNamePattern, vectorFileFormat,
                               protocol, remoteAddress);
         hasFlow = true;

         if(!performNetPerfMeterAddFlow(&gMessageReader, gControlSocket, lastFlow)) {
            LOG_FATAL
            stdlog << "ERROR: Failed to add flow to remote node!\n";
            LOG_END_FATAL
         }
         LOG_TRACE
         stdlog << "<okay; sd=" << gControlSocket << ">\n";
         LOG_END

         if(protocol != IPPROTO_SCTP) {
            lastFlow = nullptr;
            protocol = 0;
         }
      }
   }
   printGlobalParameters();


   // ====== Start measurement ==============================================
   if(!performNetPerfMeterStart(&gMessageReader, gControlSocket, measurementID,
                                gActiveNodeName, gPassiveNodeName,
                                configName,
                                vectorNamePattern, vectorFileFormat,
                                scalarNamePattern, scalarFileFormat)) {
      LOG_FATAL
      stdlog << "ERROR: Failed to start measurement!\n";
      LOG_END_FATAL
   }


   // ====== Main loop ======================================================
   const unsigned long long stopAt  = (gRuntime > 0) ?
      (getMicroTime() + (unsigned long long)rint(gRuntime * 1000000.0)) : ~0ULL;
   signal(SIGPIPE, SIG_IGN);
   installBreakDetector();
   if(gDisplayEnabled) {
      FlowManager::getFlowManager()->enableDisplay();
   }

   while( (!breakDetected()) && (!gStopTimeReached) ) {
      if(!mainLoop(true, stopAt, measurementID)) {
         std::cout << "\n" << "*** Aborted ***\n";
         break;
      }
   }

   FlowManager::getFlowManager()->disableDisplay();
   if(gStopTimeReached) {
      std::cout << "\n";
   }


   // ====== Stop measurement ===============================================
   LOG_INFO
   stdlog << "Shutdown" << "\n";
   LOG_END
   if(!performNetPerfMeterStop(&gMessageReader, gControlSocket, measurementID)) {
      LOG_FATAL
      stdlog << "ERROR: Failed to stop measurement and download the results!\n";
      LOG_END_FATAL
   }
}



// ###### Main program ######################################################
int main(int argc, char** argv)
{
   if(argc < 2) {
      std::cerr << "Usage: " << argv[0]
                << " [Local Port|Remote Endpoint] {-control-over-tcp} {-tcp|-udp|-sctp|-dccp} {flow spec} ..."
                << "\n";
      return 0;
   }

   for(int i = 2; i < argc; i++) {
      if(!(strncmp(argv[i], "-log" , 4))) {
         if(!initLogging(argv[i])) {
            return 1;
         }
      }
   }
   LOG_INFO
   stdlog << "Network Performance Meter\n"
          << "-------------------------\n\n";
   LOG_END

   const uint16_t localPort = atol(argv[1]);
   if( (localPort >= 1024) && (localPort < 65535) ) {
      passiveMode(argc, argv, localPort);
   }
   else {
      activeMode(argc, argv);
   }

   return 0;
}
