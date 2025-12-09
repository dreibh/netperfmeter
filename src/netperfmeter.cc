/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2026 by Thomas Dreibholz
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

#include "assure.h"
#include "flow.h"
#include "control.h"
#include "loglevel.h"
#include "transfer.h"
#include "package-version.h"

#include <cerrno>
#include <getopt.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <math.h>

#include <iostream>


#ifndef HAVE_MPTCP
#warning MPTCP is not supported by the API of this system!
#endif
#ifndef HAVE_DCCP
#warning DCCP is not supported by the API of this system!
#endif
#ifndef HAVE_QUIC
#warning QUIC is not supported by the API of this system!
#endif


#define MAX_LOCAL_ADDRESSES 16
static unsigned int     gLocalDataAddresses = 0;
static sockaddr_union   gLocalDataAddressArray[MAX_LOCAL_ADDRESSES];
static unsigned int     gLocalControlAddresses = 0;
static sockaddr_union   gLocalControlAddressArray[MAX_LOCAL_ADDRESSES];

static const char*      gActiveNodeName        = "Client";
static const char*      gPassiveNodeName       = "Server";
static bool             gBindV6Only            = false;
static int              gSndBufSize            = -1;
static int              gRcvBufSize            = -1;
static int              gActiveControlProtocol = IPPROTO_SCTP;
static int              gPassiveControlSCTP    = true;
static int              gPassiveControlTCP     = true;
#ifdef HAVE_MPTCP
static int              gPassiveControlMPTCP   = false;
#endif
static int              gControlSocket         = -1;
static int              gControlSocketTCP      = -1;
static int              gSCTPSocket            = -1;
static int              gTCPSocket             = -1;
#ifdef HAVE_MPTCP
static int              gMPTCPSocket           = -1;
#endif
static int              gUDPSocket             = -1;
#ifdef HAVE_DCCP
static int              gDCCPSocket            = -1;
#endif
#ifdef HAVE_QUIC
static int              gQUICSocket            = -1;
static const char*      gQUICCertificate       = nullptr;
static const char*      gQUICKey               = nullptr;
static const char*      gQUICHostname          = nullptr;
#endif
static double           gRuntime               = -1.0;
static bool             gDisplayEnabled        = true;
static bool             gStopTimeReached       = false;
static const char*      gVectorNamePattern     = "";
static OutputFileFormat gVectorFileFormat      = OFF_None;
static const char*      gScalarNamePattern     = "";
static OutputFileFormat gScalarFileFormat      = OFF_None;
static const char*      gConfigName            = "";

struct AssocSpec
{
   int                      Protocol;
   std::vector<const char*> Flows;
};
static std::vector<AssocSpec> gAssocSpecs;

// This is the MessageReader for the Control messages only!
// (The Data messages are handled by the Flow Manager)
static MessageReader  gMessageReader;


// ###### Version ###########################################################
[[ noreturn ]] static void version()
{
   std::cerr << "NetPerfMeter" << " " << NETPERFMETER_VERSION << "\n";
   exit(0);
}


// ###### Usage #############################################################
[[ noreturn ]] static void usage(const char* program, const int exitCode)
{
   std::cerr << "Usage:\n"
      << "* Passive (Server) Side:\n  " << program
      << " local_port\n"
         "    [-x|--control-over-sctp|-y|--control-over-tcp|--control-over-mptcp]\n"
         "    [-L address[,address,...]|--local address[,address,...]]\n"
         "    [-l address[,address,...]|--controllocal address[,address,...]]\n"
         "    [-K|--tls-key key_file] [-J|--tls-cert certificate_file]\n"
         "    [-6|--v6only]\n"
         "    [--display|--nodisplay]\n"
         "    [--loglevel level]\n"
         "    [--logcolor on|off]\n"
         "    [--logfile file]\n"
         "    [--logappend file]\n"
         "    [-q|--quiet]\n"
         "    [-!|--verbose]\n"
      << "* Active (Client) Side:\n  " << program << " remote_endpoint:remote_port\n"
         "    [-x|--control-over-sctp|-X|--no-control-over-sctp]\n"
         "    [-y|--control-over-tcp|-Y|--no-control-over-tcp|--control-over-mptcp|--no-control-over-mptcp]\n"
         "    [-L address[,address,...]|--local address[,address,...]]\n"
         "    [-l address[,address,...]|--controllocal address[,address,...]]\n"
         "    [-6|--v6only]\n"
         "    [--display|--nodisplay]\n"
         "    [-o bytes|--sndbuf bytes]\n"
         "    [-i bytes|--rcvbuf bytes]\n"
         "    [-T seconds|--runtime seconds]\n"
         "    [-C configuration_file_pattern|--config configuration_file_pattern]\n"
         "    [-S scalar_file_pattern|--scalar scalar_file_pattern]\n"
         "    [-V vector_file_pattern|--vector vector_file_pattern]\n"
         "    [-A description|--activenodename description]\n"
         "    [-P description|--passivenodename description]\n"
         "    [-H|--tls-hostname hostname]\n"
         "    [-t|--tcp FLOWSPEC]\n"
         "    [-m|--mptcp FLOWSPEC]\n"
         "    [-u|--udp FLOWSPEC]\n"
         "    [-d|--dccp FLOWSPEC]\n"
         "    [-s|--sctp FLOWSPEC [FLOWSPEC ...]]\n"
         "    [-k|--quic FLOWSPEC [FLOWSPEC ...]]\n"
         "    [--loglevel level]\n"
         "    [--logcolor on|off]\n"
         "    [--logfile file]\n"
         "    [--logappend file]\n"
         "    [-q|--quiet]\n"
         "    [-!|--verbose]\n"
         "  with FLOWSPEC := outgoing_frame_rate:outgoing_frame_size:incoming_frame_rate:incoming_frame_size:[options[:...]\n"
      << "* Version:\n  " << program << " [-v|--version]\n"
      << "* Help:\n  "    << program << " [-h|--help]\n";
   exit(exitCode);
}


// ###### Handle global command-line parameter ##############################
bool handleGlobalParameters(int argc, char** argv)
{
   const static struct option long_options[] = {
      { "control-over-sctp",             no_argument,       0, 'x'    },
      { "no-control-over-sctp",          no_argument,       0, 'X'    },
      { "control-over-tcp",              no_argument,       0, 'y'    },
      { "no-control-over-tcp",           no_argument,       0, 'Y'    },
      { "control-over-mptcp",            no_argument,       0, 'w'    },
      { "no-control-over-mptcp",         no_argument,       0, 'W'    },
      { "local",                         required_argument, 0, 'L'    },
      { "controllocal",                  required_argument, 0, 'l'    },
      { "display",                       no_argument,       0, 0x2001 },
      { "nodisplay",                     no_argument,       0, 0x2002 },
      { "v6only",                        no_argument,       0, '6'    },

      { "runtime",                       required_argument, 0, 'T'    },
      { "sndbuf",                        required_argument, 0, 'o'    },
      { "rcvbuf",                        required_argument, 0, 'i'    },
      { "config",                        required_argument, 0, 'C'    },
      { "scalar",                        required_argument, 0, 'S'    },
      { "vector",                        required_argument, 0, 'V'    },
      { "activenodename",                required_argument, 0, 'A'    },
      { "passivenodename",               required_argument, 0, 'P'    },
      { "tls-key",                       required_argument, 0, 'K'    },
      { "tls-cert",                      required_argument, 0, 'J'    },
      { "tls-hostname",                  required_argument, 0, 'H'    },

      { "tcp",                           required_argument, 0, 't'    },
      { "mptcp",                         required_argument, 0, 'm'    },
      { "udp",                           required_argument, 0, 'u'    },
      { "dccp",                          required_argument, 0, 'd'    },
      { "sctp",                          required_argument, 0, 's'    },
      { "quic",                          required_argument, 0, 'k'    },

      { "loglevel",                      required_argument, 0, 0x3000 },
      { "logcolor",                      required_argument, 0, 0x3001 },
      { "logappend",                     required_argument, 0, 0x3010 },
      { "logfile",                       required_argument, 0, 0x3011 },
      { "quiet",                         no_argument,       0, 'q'    },
      { "verbose",                       no_argument,       0, '!'    },

      { "help",                          no_argument,       0, 'h'    },
      { "version",                       no_argument,       0, 'v'    },
      {  nullptr,                        0,                 0, 0      }
   };

   int      option;
   int      longIndex;
   while( (option = getopt_long_only(argc, argv, "xXyYwWL:l:6T:o:i:C:S:V:A:P:K:J:H:t:m:u:d:s:k:q!hv", long_options, &longIndex)) != -1 ) {
      switch(option) {
         case 'x':
            gActiveControlProtocol = IPPROTO_SCTP;
            gPassiveControlSCTP    = true;
          break;
         case 'X':
            gPassiveControlSCTP    = false;
          break;
         case 'y':
            gActiveControlProtocol = IPPROTO_TCP;
            gPassiveControlTCP     = true;
          break;
         case 'Y':
            gPassiveControlTCP     = false;
#ifdef HAVE_MPTCP
            gPassiveControlMPTCP   = false;
#endif
          break;
         case 'w':
#ifdef HAVE_MPTCP
            gActiveControlProtocol = IPPROTO_MPTCP;
            gPassiveControlTCP     = true;
            gPassiveControlMPTCP   = true;
#else
            std::cerr << "ERROR: MPTCP support is not compiled in!" << "\n";
            exit(1);
#endif
          break;
         case 'W':
#ifdef HAVE_MPTCP
            gPassiveControlMPTCP   = false;
#endif
          break;
         case 'L':
            {
               gLocalDataAddresses = 0;
               char* address = optarg;
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
          break;
         case 'l':
            {
               gLocalControlAddresses = 0;
               char* address = optarg;
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
          break;
         case '6':
            gBindV6Only = true;
          break;
         case 0x2001:
            gDisplayEnabled = true;
          break;
         case 0x2002:
            gDisplayEnabled = false;
          break;
         case 'o':
            gSndBufSize = atol(optarg);
          break;
         case 'i':
            gRcvBufSize = atol(optarg);
          break;
         case 'T':
            gRuntime = atof(optarg);
          break;
         case 'C':
            gConfigName = optarg;
          break;
         case 'S':
            gScalarNamePattern = optarg;
            if(gScalarNamePattern[0] == 0x00)
               gScalarFileFormat = OFF_None;
            else if(hasSuffix(gScalarNamePattern, ".bz2")) {
               gScalarFileFormat = OFF_BZip2;
            }
            else {
               gScalarFileFormat = OFF_Plain;
            }
          break;
         case 'V':
            gVectorNamePattern = optarg;
            if(gVectorNamePattern[0] == 0x00)
               gVectorFileFormat = OFF_None;
            else if(hasSuffix(gVectorNamePattern, ".bz2")) {
               gVectorFileFormat = OFF_BZip2;
            }
            else {
               gVectorFileFormat = OFF_Plain;
            }
          break;
         case 'A':
            gActiveNodeName = optarg;
          break;
         case 'P':
            gPassiveNodeName = optarg;
          break;
         case 't':
         case 'm':
         case 'u':
         case 'd':
         case 's':
         case 'k':
            {
               AssocSpec flow;
               flow.Flows.push_back(optarg);
               switch(option) {
                  case 's':
                     flow.Protocol = IPPROTO_SCTP;
                     while( (optind < argc) && (argv[optind][0] != '-')) {
                        flow.Flows.push_back(argv[optind]);
                        optind++;
                     }
                   break;
                  case 't':
                     flow.Protocol = IPPROTO_TCP;
                   break;
                  case 'u':
                     flow.Protocol = IPPROTO_UDP;
                   break;
                  case 'm':
#ifdef HAVE_MPTCP
                     flow.Protocol = IPPROTO_MPTCP;
#else
                     std::cerr << "ERROR: MPTCP support is not compiled in!" << "\n";
                     exit(1);
#endif
                   break;
                  case 'd':
#ifdef HAVE_DCCP
                     flow.Protocol = IPPROTO_DCCP;
#else
                     std::cerr << "ERROR: DCCP support is not compiled in!" << "\n";
                     exit(1);
#endif
                   break;
                  case 'k':
#ifdef HAVE_QUIC
                     flow.Protocol = IPPROTO_QUIC;
                     while( (optind < argc) && (argv[optind][0] != '-')) {
                        flow.Flows.push_back(argv[optind]);
                        optind++;
                     }
#else
                     std::cerr << "ERROR: QUIC support is not compiled in!" << "\n";
                     exit(1);
#endif
                   break;
                  default:
                     abort();
                   // break;
               }
               gAssocSpecs.push_back(flow);
            }
          break;
         case 'K':
#ifdef HAVE_QUIC
            gQUICKey = optarg;
#endif
          break;
         case 'J':
#ifdef HAVE_QUIC
            gQUICCertificate = optarg;
#endif
          break;
         case 'H':
#ifdef HAVE_QUIC
            gQUICHostname = optarg;
#endif
          break;
         case 0x3000:
            gLogLevel = std::min((unsigned int)atoi(optarg),MAX_LOGLEVEL);
          break;
         case 0x3001:
            if(!(strcmp(optarg,"off"))) {
               gColorMode = false;
            }
            else {
               gColorMode = true;
            }
          break;
         case 0x3010:
            if(!initLogFile(gLogLevel,optarg,"a")) {
               std::cerr << format("ERROR: Failed to initialise log file %s!", optarg) << "\n";
               exit(1);
            }
          break;
         case 0x3011:
            if(!initLogFile(gLogLevel,optarg,"w")) {
               std::cerr << format("ERROR: Failed to initialise log file %s!", optarg) << "\n";
               exit(1);
            }
          break;
         case 'q':
            gLogLevel = LOGLEVEL_ERROR;
          break;
         case '!':
            gLogLevel = LOGLEVEL_TRACE;
          break;
         case 'v':
            version();
          break;
         case 'h':
            usage(argv[0], 0);
          break;
         default:
            usage(argv[0], 1);
          // break;
      }
   }

   if( (gPassiveControlSCTP == false) &&
       (gPassiveControlTCP == false) ) {
      std::cerr << "ERROR: At least one control channel protocol must be enabled!" << "\n";
      exit(1);
   }
   return true;
}


// ###### Print global parameter settings ###################################
void printGlobalParameters()
{
   LOG_INFO
   stdlog << "Global Parameters:\n"
          << " - Runtime                   = ";
   if(gRuntime >= 0.0) {
      stdlog << gRuntime << " s\n";
   }
   else {
      stdlog << "until manual stop\n";
   }
   stdlog << " - Minimum Logging Level     = " << gLogLevel << "\n"
          << " - Active Node Name          = " << gActiveNodeName  << "\n"
          << " - Passive Node Name         = " << gPassiveNodeName << "\n"
          << " - Local Data Address(es)    = ";
   if(gLocalDataAddresses > 0) {
      for(unsigned int i = 0; i < gLocalDataAddresses; i++) {
         if(i > 0) {
            stdlog << ", ";
         }
         printAddress(stdlog, &gLocalDataAddressArray[i].sa, false);
      }
   }
   else {
      stdlog << "(any)";
   }
   stdlog << "\n - Local Control Address(es) = ";
   if(gLocalControlAddresses > 0) {
      for(unsigned int i = 0; i < gLocalControlAddresses; i++) {
         if(i > 0) {
            stdlog << ", ";
         }
         printAddress(stdlog, &gLocalControlAddressArray[i].sa, false);
      }
   }
   else {
      stdlog << "(any)";
   }
   stdlog << "\n";
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
   if(sscanf(parameters, "const%lf%n", &valueArray[0], &n) == 1) {
      *rng = RANDOM_CONSTANT;
   }
   else if(sscanf(parameters, "uniform%lf,%lf%n", &valueArray[0], &valueArray[1], &n) == 2) {
      *rng = RANDOM_UNIFORM;
   }
   else if(sscanf(parameters, "exp%lf%n", &valueArray[0], &n) == 1) {
      *rng = RANDOM_EXPONENTIAL;
   }
   else if(sscanf(parameters, "normal%lf,%lf%n", &valueArray[0], &valueArray[1], &n) == 2) {
      *rng = RANDOM_NORMAL;
   }
   else if(sscanf(parameters, "truncnormal%lf,%lf%n", &valueArray[0], &valueArray[1], &n) == 2) {
      *rng = RANDOM_TRUNCATED_NORMAL;
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
         // n = 6 + 2;
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
         std::cerr << "ERROR: Invalid \"cc\" setting: " << (const char*)&parameters[8]
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
                        const char*            gVectorNamePattern,
                        const OutputFileFormat gVectorFileFormat,
                        const int              initialProtocol,
                        const sockaddr_union&  remoteAddress)
{
   // ====== Get flow ID and stream ID ======================================
   static uint32_t flowID   = 0; // will be increased with each successfull call
   uint16_t        streamID = (previousFlow != nullptr) ? previousFlow->getStreamID() + 1 : 0;

   // ====== Get FlowTrafficSpec ============================================
   FlowTrafficSpec trafficSpec;
   trafficSpec.Protocol = initialProtocol;
#ifdef HAVE_MPTCP
   if(trafficSpec.Protocol == IPPROTO_MPTCP) {
      trafficSpec.CMT = NPAF_LikeMPTCP;
   }
#endif

   if(strncmp(parameters, "default", 7) == 0) {
      trafficSpec.OutboundFrameRateRng = RANDOM_CONSTANT;
      trafficSpec.OutboundFrameRate[0] = 0.0;
      switch(trafficSpec.Protocol) {
         case IPPROTO_TCP:
#ifdef HAVE_MPTCP
         case IPPROTO_MPTCP:
#endif
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
#ifdef HAVE_QUIC
         case IPPROTO_QUIC:
            trafficSpec.OutboundFrameSize[0] = 1400;   // Some reasonable default
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

   // ------ Use TCP or MPTCP? ----------------------------------------------
#ifdef HAVE_MPTCP
   if( (trafficSpec.Protocol == IPPROTO_TCP) && (trafficSpec.CMT == NPAF_LikeMPTCP) ) {
      trafficSpec.Protocol = IPPROTO_MPTCP;
   }
   if( (trafficSpec.Protocol == IPPROTO_MPTCP) && (trafficSpec.CMT != NPAF_LikeMPTCP) ) {
      std::cerr << "WARNING: Invalid \"cmt\" setting: " << (const char*)&parameters[4]
                << " for MPTCP! Using default instead!\n";
      exit(1);
   }
#endif
   // -----------------------------------------------------------------------

   // ====== Create new flow ================================================
   if(FlowManager::getFlowManager()->findFlow(measurementID, flowID, streamID) != nullptr) {
      std::cerr << "ERROR: Flow ID " << flowID << " is used twice. Ensure correct id=<ID> parameters!\n";
      exit(1);
   }
   Flow* flow = new Flow(measurementID, flowID, streamID, trafficSpec);
   assure(flow != nullptr);

   // ====== Initialize vector file =========================================
   const std::string vectorName = flow->getNodeOutputName(gVectorNamePattern,
                                                          "active",
                                                          format("-%08x-%04x", flowID, streamID));
   if(!flow->initializeVectorFile(vectorName.c_str(), gVectorFileFormat)) {
      std::cerr << "ERROR: Unable to create vector file <" << vectorName << ">!\n";
      exit(1);
   }

   // ====== Initialise destination address =================================
   sockaddr_union destinationAddress = remoteAddress;
#ifdef HAVE_MPTCP
   if(trafficSpec.Protocol == IPPROTO_MPTCP) {
      // An MPTCP subflow is just a TCP connection on the wire. To allow for
      // MPTCP and TCP simultaneously, MPTCP needs a different port number:
      setPort(&destinationAddress.sa, getPort(&destinationAddress.sa) - 1);
   }
#endif
#ifdef HAVE_QUIC
   if(trafficSpec.Protocol == IPPROTO_QUIC) {
      // A QUIC connection is just a UDP "connection" on the wire. To allow for
      // QUIC and UDP simultaneously, QUIC needs a different port number:
      setPort(&destinationAddress.sa, getPort(&destinationAddress.sa) - 1);
   }
#endif

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
         case IPPROTO_UDP:
            socketDescriptor = createAndBindSocket(remoteAddress.sa.sa_family, SOCK_DGRAM, IPPROTO_UDP, 0,
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
#ifdef HAVE_DCCP
         case IPPROTO_DCCP:
            socketDescriptor = createAndBindSocket(remoteAddress.sa.sa_family, SOCK_DCCP, IPPROTO_DCCP, 0,
                                                   gLocalDataAddresses, (const sockaddr_union*)&gLocalDataAddressArray, false,
                                                   trafficSpec.BindV6Only);
           break;
#endif
#ifdef HAVE_QUIC
         case IPPROTO_QUIC:
            socketDescriptor = createAndBindSocket(remoteAddress.sa.sa_family, SOCK_DGRAM, IPPROTO_QUIC, 0,
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

      // ====== QUIC handshake ==============================================
#ifdef HAVE_QUIC
      if(trafficSpec.Protocol == IPPROTO_QUIC) {
         LOG_TRACE
         stdlog << "client handshake <sd=" << socketDescriptor
                << ", H=" << gQUICHostname << ">\n";
         LOG_END
         if(quic_client_handshake(socketDescriptor, nullptr,
                                  gQUICHostname,
                                  ALPN_NETPERFMETER_DATA) != 0) {
            std::cerr << "ERROR: QUIC handshake failed: " << strerror(errno) << "!\n";
            exit(1);
         }
      }
#endif
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
#ifdef HAVE_MPTCP
   int                    mptcpID = -1;
#endif
   int                    udpID   = -1;
   int                    sctpID  = -1;
#ifdef HAVE_DCCP
   int                    dccpID  = -1;
#endif
#ifdef HAVE_QUIC
   int                    quicID  = -1;
#endif
   unsigned long long     now     = getMicroTime();
   std::map<int, pollfd*> pollFDIndex;


   // ====== Get parameters for poll() ======================================
   addToPollFDs((pollfd*)&fds, gTCPSocket,     n, &tcpID);
#ifdef HAVE_MPTCP
   addToPollFDs((pollfd*)&fds, gMPTCPSocket,   n, &mptcpID);
#endif
   addToPollFDs((pollfd*)&fds, gUDPSocket,     n, &udpID);
   addToPollFDs((pollfd*)&fds, gSCTPSocket,    n, &sctpID);
#ifdef HAVE_DCCP
   addToPollFDs((pollfd*)&fds, gDCCPSocket,    n, &dccpID);
#endif
#ifdef HAVE_QUIC
   addToPollFDs((pollfd*)&fds, gQUICSocket,    n, &quicID);
#endif
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
         if(fds[controlID].revents & (POLLIN|POLLERR)) {
            if( (isActiveMode == false) &&
                (fds[controlID].fd == gControlSocket) ) {
               const int newSD = ext_accept(gControlSocket, nullptr, 0);
               if(newSD >= 0) {
                  LOG_TRACE
                  stdlog << format("Accept on SCTP control socket %d -> new control connection %d",
                                   gControlSocket, newSD) << "\n";
                  LOG_END
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
                  LOG_TRACE
                  stdlog << format("Accept on TCP control socket %d -> new control connection %d",
                                   gControlSocketTCP, newSD) << "\n";
                  LOG_END
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
      if( (tcpID >= 0) && (fds[tcpID].revents & (POLLIN|POLLERR)) ) {
         const int newSD = ext_accept(gTCPSocket, nullptr, 0);
         if(newSD >= 0) {
            LOG_TRACE
            stdlog << format("Accept on TCP data socket %d -> new TCP data connection %d",
                             gTCPSocket, newSD) << "\n";
            LOG_END
            FlowManager::getFlowManager()->addUnidentifiedSocket(IPPROTO_TCP, newSD);
         }
      }
#ifdef HAVE_MPTCP
      if( (mptcpID >= 0) && (fds[mptcpID].revents & (POLLIN|POLLERR)) ) {
         const int newSD = ext_accept(gMPTCPSocket, nullptr, 0);
         if(newSD >= 0) {
            LOG_TRACE
            stdlog << format("Accept on MPTCP data socket %d -> new MPTCP data connection %d",
                             gMPTCPSocket, newSD) << "\n";
            LOG_END
            FlowManager::getFlowManager()->addUnidentifiedSocket(IPPROTO_MPTCP, newSD);
         }
      }
#endif
      if( (udpID >= 0) && (fds[udpID].revents & (POLLIN|POLLERR)) ) {
         FlowManager::getFlowManager()->lock();
         handleNetPerfMeterData(isActiveMode, now, IPPROTO_UDP, gUDPSocket);
         FlowManager::getFlowManager()->unlock();
      }
      if( (sctpID >= 0) && (fds[sctpID].revents & (POLLIN|POLLERR)) ) {
         const int newSD = ext_accept(gSCTPSocket, nullptr, 0);
         if(newSD >= 0) {
            LOG_TRACE
            stdlog << format("Accept on SCTP data socket %d -> new SCTP data connection %d",
                             gSCTPSocket, newSD) << "\n";
            LOG_END
            FlowManager::getFlowManager()->addUnidentifiedSocket(IPPROTO_SCTP, newSD);
         }
      }
#ifdef HAVE_DCCP
      if( (dccpID >= 0) && (fds[dccpID].revents & (POLLIN|POLLERR)) ) {
         const int newSD = ext_accept(gDCCPSocket, nullptr, 0);
         if(newSD >= 0) {
            LOG_TRACE
            stdlog << format("Accept on DCCP data socket %d -> new DCCP data connection %d",
                             gDCCPSocket, newSD) << "\n";
            LOG_END
            FlowManager::getFlowManager()->addUnidentifiedSocket(IPPROTO_DCCP, newSD);
         }
      }
#endif
#ifdef HAVE_QUIC
      if( (quicID >= 0) && (fds[quicID].revents & (POLLIN|POLLERR)) ) {
         const int newSD = ext_accept(gQUICSocket, nullptr, 0);
         if(newSD >= 0) {
            LOG_TRACE
            stdlog << format("Accept on QUIC data socket %d -> new QUIC data connection %d",
                             gQUICSocket, newSD) << "\n";
            LOG_END
            LOG_TRACE
            stdlog << "server handshake <sd=" << gQUICSocket
                   << ", K=" << ((gQUICKey != nullptr) ? gQUICKey : "(none)")
                   << ", C=" << ((gQUICCertificate != nullptr) ? gQUICCertificate : "(none)")
                   << ">\n";
            LOG_END
            if(quic_server_handshake(newSD, gQUICKey, gQUICCertificate, ALPN_NETPERFMETER_DATA) == 0) {
               LOG_TRACE
               stdlog << format("Accept on QUIC data socket %d -> new QUIC data connection %d",
                                gQUICSocket, newSD) << "\n";
               LOG_END
               FlowManager::getFlowManager()->addUnidentifiedSocket(IPPROTO_QUIC, newSD);
            }
            else {
               perror("quic_server_handshake()");
               ext_close(newSD);
            }
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
void passiveMode(const uint16_t localPort)
{
   // ====== Print global parameters ========================================
   printGlobalParameters();

   // ====== Test for problems ==============================================
   sockaddr_union testAddress;
   assure(string2address("172.17.0.1:0", &testAddress));
   int testSD = createAndBindSocket(AF_INET, SOCK_STREAM, IPPROTO_SCTP, 0, 1,
                                    &testAddress, true, false);
   if(testSD >= 0) {
      LOG_WARNING
      stdlog << "WARNING: This machine seems to have an interface with address 172.17.0.1! This is typically used by Docker setups. If you connect from another machine having the same configuration, in an environment with only private addresses SCTP may try to use this address -> OOTB ABORT." << "\n";
      LOG_END
      ext_close(testSD);
   }

   // ====== Initialize SCTP control socket =================================
   if(gPassiveControlSCTP) {
      gControlSocket = createAndBindSocket(AF_UNSPEC, SOCK_STREAM, IPPROTO_SCTP,
                                           localPort + 1,
                                           gLocalControlAddresses,
                                           (const sockaddr_union*)&gLocalControlAddressArray,
                                           true, gBindV6Only);
      if(gControlSocket >= 0) {
         // ------ Set default send parameters ---------------------------------
         sctp_sndrcvinfo sinfo;
         memset(&sinfo, 0, sizeof(sinfo));
         sinfo.sinfo_ppid = htonl(PPID_NETPERFMETER_CONTROL);
         if(ext_setsockopt(gControlSocket, IPPROTO_SCTP, SCTP_DEFAULT_SEND_PARAM,
                           &sinfo, sizeof(sinfo)) < 0) {
            LOG_FATAL
            stdlog << format("Failed to configure default send parameters (SCTP_DEFAULT_SEND_PARAM option) on SCTP control socket %d: %s!",
                             gControlSocket, strerror(errno)) << "\n";
            LOG_END_FATAL
         }

         const int noDelayOption = 1;
         if(ext_setsockopt(gControlSocket, IPPROTO_SCTP, SCTP_NODELAY,
                           (const char*)&noDelayOption, sizeof(noDelayOption)) < 0) {
            LOG_ERROR
            stdlog << format("Failed to set SCTP_NODELAY on SCTP control socket: %s!",
                              strerror(errno)) << "\n";
            LOG_END_FATAL
         }

         // ------ Enable SCTP events ------------------------------------------
         sctp_event_subscribe events;
         memset((char*)&events, 0 ,sizeof(events));
         events.sctp_data_io_event     = 1;
         events.sctp_association_event = 1;
         if(ext_setsockopt(gControlSocket, IPPROTO_SCTP, SCTP_EVENTS, &events, sizeof(events)) < 0) {
            LOG_FATAL
            stdlog << format("Failed to configure events (SCTP_EVENTS option) on SCTP socket %d: %s!",
                             gControlSocket, strerror(errno)) << "\n";
            LOG_END_FATAL
         }

         gMessageReader.registerSocket(IPPROTO_SCTP, gControlSocket);
      }
      else {
         if( (errno != EPROTONOSUPPORT) ||
             (gPassiveControlTCP == false) ) {
            LOG_FATAL
            stdlog << format("Failed to create and bind SCTP control socket on port %d: %s!",
                             localPort + 1, strerror(errno)) << "\n";
            if(errno == EPROTONOSUPPORT) {
               stdlog << "SCTP is not available => Try -y|--control-over-tcp for control over TCP!\n";
            }
            LOG_END_FATAL
         }
         LOG_WARNING
         stdlog << "WARNING: SCTP is not available => no control connections via SCTP possible!\n"
                   "SCTP can be disabled by -X|--no-control-over-sctp.\n";
         LOG_END
      }
   }

   // ====== Initialize TCP/MPTCP control socket ============================
   if(gPassiveControlTCP) {
      gControlSocketTCP = createAndBindSocket(AF_UNSPEC, SOCK_STREAM,
#ifdef HAVE_MPTCP
                                              (gPassiveControlMPTCP) ? IPPROTO_MPTCP : IPPROTO_TCP,
#else
                                              IPPROTO_TCP,
#endif
                                              localPort + 1,
                                              gLocalControlAddresses,
                                              (const sockaddr_union*)&gLocalControlAddressArray,
                                              true, gBindV6Only);
      if(gControlSocketTCP < 0) {
         LOG_FATAL
         stdlog << format("Failed to create and bind TCP control socket on port %d: %s!",
                        localPort + 1, strerror(errno)) << "\n";
         LOG_END_FATAL
      }
      const int noDelayOption = 1;
      if(ext_setsockopt(gControlSocketTCP, IPPROTO_TCP, TCP_NODELAY,
                        (const char*)&noDelayOption, sizeof(noDelayOption)) < 0) {
         LOG_ERROR
         stdlog << format("Failed to set TCP_NODELAY on TCP control socket: %s!",
                           strerror(errno)) << "\n";
         LOG_END_FATAL
      }
      gMessageReader.registerSocket(IPPROTO_TCP, gControlSocketTCP);
   }

   // ====== Initialize data socket for each protocol =======================

   // ------ TCP ------------------------------------------------------------
   gTCPSocket = createAndBindSocket(AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, localPort,
                                    gLocalDataAddresses,
                                    (const sockaddr_union*)&gLocalDataAddressArray,
                                    true, gBindV6Only);
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

   // ------ MPTCP ----------------------------------------------------------
#ifdef HAVE_MPTCP
   gMPTCPSocket = createAndBindSocket(AF_UNSPEC, SOCK_STREAM, IPPROTO_MPTCP, localPort - 1,
                                      gLocalDataAddresses,
                                      (const sockaddr_union*)&gLocalDataAddressArray,
                                      true, gBindV6Only);
   if(gMPTCPSocket < 0) {
      LOG_DEBUG
      stdlog << format("NOTE: Failed to create and bind MPTCP socket on port %d: %s!",
                        localPort - 1, strerror(errno)) << "\n";
      LOG_END
   }
   else {
      if(setBufferSizes(gMPTCPSocket, gSndBufSize, gRcvBufSize) == false) {
         LOG_FATAL
         stdlog << format("Failed to configure buffer sizes on MPTCP socket %d!",
                          gMPTCPSocket) << "\n";
         LOG_END_FATAL
      }
   }
#endif

   // ------ UDP ------------------------------------------------------------
   gUDPSocket = createAndBindSocket(AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP, localPort,
                                    gLocalDataAddresses,
                                    (const sockaddr_union*)&gLocalDataAddressArray,
                                    true, gBindV6Only);
   if(gUDPSocket < 0) {
      LOG_FATAL
      stdlog << "ERROR: Failed to create and bind UDP socket on port " << localPort << " - "
             << strerror(errno) << "!\n";
      LOG_END_FATAL
   }
   // NOTE: For connection-less UDP, the FlowManager takes care of the socket!
   FlowManager::getFlowManager()->addUnidentifiedSocket(IPPROTO_UDP, gUDPSocket);

   // ------ DCCP -----------------------------------------------------------
#ifdef HAVE_DCCP
   gDCCPSocket = createAndBindSocket(AF_UNSPEC, SOCK_DCCP, IPPROTO_DCCP, localPort,
                                     gLocalDataAddresses,
                                     (const sockaddr_union*)&gLocalDataAddressArray,
                                     true, gBindV6Only);
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

   // ------ QUIC -----------------------------------------------------------
#ifdef HAVE_QUIC
   gQUICSocket = createAndBindSocket(AF_UNSPEC, SOCK_DGRAM, IPPROTO_QUIC, localPort - 1,
                                     gLocalDataAddresses, (const sockaddr_union*)&gLocalDataAddressArray, false, gBindV6Only);
   // NOTE: It is necessary to set the ALPN before enabling the listening mode!
   if(gQUICSocket < 0) {
      LOG_INFO
      stdlog << format("NOTE: Failed to create and bind QUIC socket on port %d: %s!",
                        localPort, strerror(errno)) << "\n";
      LOG_END
   }
   else {
      if(ext_setsockopt(gQUICSocket, SOL_QUIC, QUIC_SOCKOPT_ALPN,
                        ALPN_NETPERFMETER_DATA, strlen(ALPN_NETPERFMETER_DATA)) < 0) {
         LOG_FATAL
         stdlog << format("Failed to configure QUIC ALPN (QUIC_SOCKOPT_ALPN option) on QUIC socket: %s!",
                          gQUICSocket, strerror(errno)) << "\n";
         LOG_END_FATAL
      }
      if(setBufferSizes(gQUICSocket, gSndBufSize, gRcvBufSize) == false) {
         LOG_FATAL
         stdlog << format("Failed to configure buffer sizes on QUIC socket %d!",
                          gQUICSocket) << "\n";
         LOG_END_FATAL
      }
      if(ext_listen(gQUICSocket, 100) < 0) {
         LOG_FATAL
         stdlog << format("Failed to enable listening on QUIC socket %d!",
                          gQUICSocket) << "\n";
         LOG_END_FATAL
      }
   }
#endif

   // ------ SCTP -----------------------------------------------------------
   gSCTPSocket = createAndBindSocket(AF_UNSPEC, SOCK_STREAM, IPPROTO_SCTP, localPort,
                                     gLocalDataAddresses,
                                     (const sockaddr_union*)&gLocalDataAddressArray,
                                     true, gBindV6Only);
   if(gSCTPSocket >= 0) {

      // ------ Set SCTP stream parameters-----------------------------------
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

      // ------ Enable SCTP events ------------------------------------------
      sctp_event_subscribe events;
      memset((char*)&events, 0 ,sizeof(events));
      events.sctp_data_io_event = 1;
      if(ext_setsockopt(gSCTPSocket, IPPROTO_SCTP, SCTP_EVENTS, &events, sizeof(events)) < 0) {
         LOG_FATAL
         stdlog << format("Failed to configure events (SCTP_EVENTS option) on SCTP socket %d: %s!",
                          gSCTPSocket, strerror(errno)) << "\n";
         LOG_END_FATAL
      }

      // ------ Set SCTP buffer sizes ---------------------------------------
      if(setBufferSizes(gSCTPSocket, gSndBufSize, gRcvBufSize) == false) {
         LOG_FATAL
         stdlog << format("Failed to configure buffer sizes on SCTP socket %d!",
                          gSCTPSocket) << "\n";
         LOG_END_FATAL
      }

   }
   else {
      if( (errno != EPROTONOSUPPORT) || (gPassiveControlTCP == false) ) {
         LOG_FATAL
         stdlog << format("Failed to create and bind SCTP socket on port %d: %s!",
                          localPort, strerror(errno)) << "\n";
         LOG_END_FATAL
      }
   }

   // ====== Print status ===================================================
   LOG_INFO
   std::string controlProtocols;
   if(gControlSocket >= 0) {
      controlProtocols += "SCTP";
   }
   if(gControlSocketTCP >= 0) {
      if(controlProtocols.size() > 0) {
         controlProtocols += "/";
      }
      controlProtocols += "TCP";
#ifdef HAVE_MPTCP
      if(gPassiveControlMPTCP) {
         controlProtocols += "/MPTCP";
      }
#endif
   }
   std::string dataProtocols1("SCTP/TCP/UDP");
   std::string dataProtocols2;
#ifdef HAVE_DCCP
   if(gDCCPSocket >= 0) {
      dataProtocols1 += "/DCCP";
   }
#endif
#ifdef HAVE_MPTCP
   if(gMPTCPSocket > 0) {
      dataProtocols2 += "MPTCP";
   }
#endif
#ifdef HAVE_QUIC
   if(gQUICSocket > 0) {
      if(dataProtocols2.size() > 0) {
         dataProtocols2 += "/";
      }
      dataProtocols2 += "QUIC";
   }
#endif
   if(dataProtocols2.size() > 0) {
      dataProtocols2 = format(", and %s on port %u",
                              dataProtocols2.c_str(), localPort - 1);
   }
   stdlog << format("Passive Mode: Waiting for incoming connections:\n"
                    " - Control Channel = %s on port %u\n"
                    " - Data Channel    = %s on port %u%s\n",
                    controlProtocols.c_str(), localPort + 1,
                    dataProtocols1.c_str(), localPort,
                    dataProtocols2.c_str());
   LOG_END
   LOG_TRACE
   stdlog << "Sockets:\n"
          << "- SCTP Control = " << gControlSocket    << "\n"
          << "- TCP Control  = " << gControlSocketTCP << "\n"
          << "- TCP Data     = " << gTCPSocket        << "\n"
#ifdef HAVE_MPTCP
          << "- MPTCP Data   = " << gMPTCPSocket      << "\n"
#endif
          << "- UDP Data     = " << gUDPSocket        << "\n"
          << "- SCTP Data    = " << gSCTPSocket       << "\n"
#ifdef HAVE_DCCP
          << "- DCCP Data    = " << gDCCPSocket       << "\n"
#endif
#ifdef HAVE_QUIC
          << "- QUIC Data    = " << gQUICSocket       << "\n"
#endif
          ;
   LOG_END


   // ====== Main loop ======================================================
   signal(SIGPIPE, SIG_IGN);
   installBreakDetector();

   FlowManager::getFlowManager()->configureDisplay(gDisplayEnabled);

   const unsigned long long stopAt = (gRuntime > 0) ?
      (getMicroTime() + (unsigned long long)rint(gRuntime * 1000000.0)) : ~0ULL;
   while( (!breakDetected()) && (!gStopTimeReached) ) {
      mainLoop(false, stopAt, 0);
   }

   FlowManager::getFlowManager()->configureDisplay(false);


   // ====== Clean up =======================================================
   if(gControlSocketTCP >= 0) {
      gMessageReader.deregisterSocket(gControlSocketTCP);
      ext_close(gControlSocketTCP);
   }
   if(gControlSocket>= 0) {
      gMessageReader.deregisterSocket(gControlSocket);
      ext_close(gControlSocket);
   }
   ext_close(gTCPSocket);
#ifdef HAVE_MPTCP
   if(gMPTCPSocket >= 0) {
      ext_close(gMPTCPSocket);
   }
#endif
   FlowManager::getFlowManager()->removeUnidentifiedSocket(gUDPSocket, false);
   ext_close(gUDPSocket);
   if(gSCTPSocket >= 0) {
      ext_close(gSCTPSocket);
   }
#ifdef HAVE_DCCP
   if(gDCCPSocket >= 0) {
      ext_close(gDCCPSocket);
   }
#endif
#ifdef HAVE_QUIC
   if(gQUICSocket >= 0) {
      ext_close(gQUICSocket);
   }
#endif
}



// ###### Active Mode #######################################################
void activeMode(const char* remoteEndpoint)
{
   // ====== Initialize remote and control addresses ========================
   sockaddr_union remoteAddress;
   if(string2address(remoteEndpoint, &remoteAddress) == false) {
      LOG_FATAL
      std::cerr << format("ERROR: Invalid remote address %s", remoteEndpoint) << "!\n";
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

   std::string controlProtocol;
   switch(gActiveControlProtocol) {
      case IPPROTO_SCTP:
         controlProtocol = "SCTP";
       break;
      case IPPROTO_TCP:
         controlProtocol = "TCP";
       break;
#ifdef HAVE_MPTCP
      case IPPROTO_MPTCP:
         controlProtocol = "MPTCP";
       break;
#endif
   }

   LOG_INFO
   stdlog << "Active Mode:\n"
          << "- Measurement ID  = " << format("$%llx", measurementID) << "\n"
          << "- Remote Address  = ";
   printAddress(stdlog, &remoteAddress.sa, true);
   stdlog << "\n"
          << "- Control Address = ";
   printAddress(stdlog, &controlAddress.sa, true);
   stdlog << " - connecting via " << controlProtocol << " ...\n";
   LOG_END

   // ====== Initialize control socket ======================================
   gControlSocket = createAndBindSocket(controlAddress.sa.sa_family, SOCK_STREAM, gActiveControlProtocol,
                                        0, gLocalControlAddresses, (const sockaddr_union*)&gLocalControlAddressArray,
                                        false, gBindV6Only);
   if(gControlSocket < 0) {
      LOG_FATAL
      stdlog << format("Failed to create and bind control socket: %s!",
                       strerror(errno)) << "\n";
      if( (gActiveControlProtocol == IPPROTO_SCTP) && (errno == EPROTONOSUPPORT) ) {
         stdlog << "SCTP is not available => Try -y|--control-over-tcp for control over TCP!\n";
      }
      LOG_END_FATAL
   }
   if(gActiveControlProtocol == IPPROTO_SCTP) {
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
      if(gActiveControlProtocol == IPPROTO_SCTP) {
         stdlog << "Note: Try -y|--control-over-tcp for control over TCP in case of NAT traversal or restrictive firewalls!\n";
      }
      LOG_END_FATAL
   }
   if(gActiveControlProtocol == IPPROTO_SCTP) {
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
   gMessageReader.registerSocket(gActiveControlProtocol, gControlSocket);


   // ====== Handle command-line parameters =================================

   // ------ Handle other parameters ----------------------------------------
   for(std::vector<AssocSpec>::const_iterator assocSpecIterator = gAssocSpecs.begin();
       assocSpecIterator != gAssocSpecs.end(); assocSpecIterator++) {
      const AssocSpec& assocSpec = *assocSpecIterator;
      Flow* lastFlow = nullptr;
      for(std::vector<const char*>::const_iterator flowIterator = assocSpec.Flows.begin();
          flowIterator != assocSpec.Flows.end(); flowIterator++) {
         const char* flowSpec = *flowIterator;

         lastFlow = createFlow(lastFlow, flowSpec, measurementID,
                               gVectorNamePattern, gVectorFileFormat,
                               assocSpec.Protocol, remoteAddress);
         if(!performNetPerfMeterAddFlow(&gMessageReader, gControlSocket, lastFlow)) {
            LOG_FATAL
            stdlog << "ERROR: Failed to add flow to remote node!\n";
            LOG_END_FATAL
         }
         LOG_TRACE
         stdlog << "<okay; sd=" << gControlSocket << ">\n";
         LOG_END
      }
      lastFlow = nullptr;
   }

   // ====== Print global parameters ========================================
   printGlobalParameters();

   // ====== Start measurement ==============================================
   if(!performNetPerfMeterStart(&gMessageReader, gControlSocket, measurementID,
                                gActiveNodeName, gPassiveNodeName,
                                gConfigName,
                                gVectorNamePattern, gVectorFileFormat,
                                gScalarNamePattern, gScalarFileFormat)) {
      LOG_FATAL
      stdlog << "ERROR: Failed to start measurement!\n";
      LOG_END_FATAL
   }


   // ====== Main loop ======================================================
   const unsigned long long stopAt = (gRuntime > 0) ?
      (getMicroTime() + (unsigned long long)rint(gRuntime * 1000000.0)) : ~0ULL;
   signal(SIGPIPE, SIG_IGN);
   installBreakDetector();

   FlowManager::getFlowManager()->configureDisplay(gDisplayEnabled);
   while( (!breakDetected()) && (!gStopTimeReached) ) {
      if(!mainLoop(true, stopAt, measurementID)) {
         std::cout << "\n" << "*** Aborted ***\n";
         break;
      }
   }
   FlowManager::getFlowManager()->configureDisplay(false);

   if(gStopTimeReached) {
      std::cout << "\n";
   }


   // ====== Stop measurement ===============================================
   LOG_INFO
   stdlog << "Shutdown" << "\n";
   LOG_END
   if(!performNetPerfMeterStop(&gMessageReader, gControlSocket, measurementID)) {
      LOG_FATAL
      stdlog << "Failed to stop measurement and download the results!\n";
      LOG_END_FATAL
   }
}



// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Handle parameters ==============================================
   handleGlobalParameters(argc, argv);
   if(argc - optind != 1) {
      usage(argv[0], 1);
   }
   const unsigned int localPort     = (index(argv[optind], ':') == nullptr) ?
                                         atol(argv[optind]) : 0;
   const bool         inPassiveMode = (localPort >= 1) && (localPort < 65535);
   if( (!inPassiveMode) && (gAssocSpecs.size() < 1) ) {
      std::cerr << "ERROR: No flows specified!\n";
      return 1;
   }

   // ====== Start logging ==================================================
   beginLogging();
   LOG_INFO
   stdlog << "Network Performance Meter " << NETPERFMETER_VERSION
          << " in "
          << ((inPassiveMode == true) ? "Passive Mode" : "Active Mode")
          << "\n";
   LOG_END

   // ====== Run active or passive instance =================================
   if(inPassiveMode) {
      passiveMode(localPort);
   }
   else {
      activeMode(argv[optind]);
   }

   // ====== Finish logging =================================================
   finishLogging();

   return 0;
}
