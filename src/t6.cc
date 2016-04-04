/* $Id$
 *
 * Simple Connectivity Test
 * Copyright (C) 2016 by Thomas Dreibholz
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
 * Contact: dreibh@simula.no
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include "boost/dynamic_bitset.hpp"

#include "tools.h"
#include "outputfile.h"


static OutputFile gOutputFile;


struct TestPacket {
   uint8_t  Type;
   uint8_t  Flags;
   uint16_t Padding;
   uint32_t Length;
   uint64_t CurrentSeqNumber;
   uint64_t TotalInSequence;
   uint64_t TestStartTime;
   uint64_t PacketSendTime;
   char     Data[];
} __attribute__((packed));

#define TPT_ECHO_REQUEST  1
#define TPT_ECHO_RESPONSE 2
#define TPT_DISCARD       3


// ====== Initialize test packet ============================================
inline unsigned int makePacket(char*              buffer,
                               const size_t       bufferSize,
                               const uint8_t      type,
                               const uint64_t     testStartTime,
                               const uint64_t     currentSeqNumber,
                               const uint64_t     totalInSequence,
                               const unsigned int totalPacketSize)
{
   const unsigned int payloadLength =
      (unsigned int)std::max((int)totalPacketSize - (int)sizeof(TestPacket),
                             (int)sizeof(TestPacket));
   const unsigned int totalLength = payloadLength + sizeof(TestPacket);

   TestPacket* packet = (TestPacket*)buffer;
   assert(bufferSize >= totalLength);

   packet->Type             = type;
   packet->Flags            = 0;
   packet->Padding          = 0;
   packet->Length           = htonl(totalLength);
   packet->CurrentSeqNumber = hton64(currentSeqNumber);
   packet->TotalInSequence  = hton64(totalInSequence);
   for(unsigned int i = 0; i < payloadLength; i++) {
      packet->Data[i] = (i & 0xff);
   }
   packet->TestStartTime  = hton64(testStartTime);
   packet->PacketSendTime = hton64(getMicroTime());

   return(totalLength);
}


// ====== Record event ======================================================
static void recordEvent(const uint64_t           now,
                        const uint64_t           testStartTime,
                        const sockaddr_union*    local,
                        const sockaddr_union*    remote,
                        const char*              purpose,
                        const unsigned long long parameter1,
                        const unsigned long long parameter2,
                        const uint64_t           currentSeqNumber,
                        const uint64_t           totalSeqNumber,
                        const char*              valueUnit,
                        const double             value,
                        const char*              str1 = "",
                        const char*              str2 = "")

{
   char localAddress[64];
   if(!address2string(&local->sa, (char*)&localAddress, sizeof(localAddress), true)) {
      localAddress[0] = 0x00;
   }
   char remoteAddress[64];
   if(!address2string(&remote->sa, (char*)&remoteAddress, sizeof(remoteAddress), true)) {
      remoteAddress[0] = 0x00;
   }
   if(gOutputFile.getLine() == 0) {
      gOutputFile.printf("CurrentTime TestStartTime Purpose Remote Local P1 P2 CurrentSeqNumber TotalSeqNumber Unit Value S1 S2\n");
      gOutputFile.nextLine();
   }
   gOutputFile.printf("%06llu %llu %llu %s \"%s\" \"%s\" %llu %llu\t%llu %llu\t%s %1.6f\t\"%s\" \"%s\"\n",
                      gOutputFile.getLine(),
                      (unsigned long long)now, (unsigned long long)testStartTime,
                      purpose,
                      remoteAddress, localAddress,
                      parameter1, parameter2,
                      (unsigned long long)currentSeqNumber, (unsigned long long)totalSeqNumber,
                      valueUnit, value, str1, str2);
   gOutputFile.nextLine();
}


// ====== Record packet =====================================================
static void recordPacket(const uint64_t           now,
                         const sockaddr_union*    local,
                         const sockaddr_union*    remote,
                         const TestPacket*        packet,
                         const char*              purpose,
                         const unsigned long long interPacketTime = 0,
                         const bool               outOfSequence   = false,
                         const bool               duplicate       = false)
{
   recordEvent(now, (unsigned long long)ntoh64(packet->TestStartTime),
               local, remote, purpose,
               ntohl(packet->Length), interPacketTime,
               (unsigned long long)ntoh64(packet->CurrentSeqNumber),
               (unsigned long long)ntoh64(packet->TotalInSequence),
               "Delay",
               ((long long)now - (long long)ntoh64(packet->PacketSendTime)) / 1000.0,
               (outOfSequence == true) ? "OOS" : "",
               (duplicate == true)     ? "DUP" : "");
}


// ====== Passive mode ======================================================
static void passiveMode(int sd, const sockaddr_union* local)
{
  while(true) {
      pollfd pfd;
      pfd.fd      = sd;
      pfd.events  = POLLIN;
      pfd.revents = 0;

      int ready = poll(&pfd, 1, 3600000);
      if(ready > 0) {
         if(pfd.revents & POLLIN) {
            char           buffer[65536];
            sockaddr_union remote;
            socklen_t      remoteLength = sizeof(remote);

            const ssize_t bytes = recvfrom(sd, (char*)&buffer, sizeof(buffer), 0,
                                           &remote.sa, &remoteLength);
            if(bytes >= (ssize_t)sizeof(TestPacket)) {
               TestPacket* packet = (TestPacket*)&buffer;
               if(ntohl(packet->Length) == bytes) {
                  const uint64_t now = getMicroTime();
                  if(packet->Type == TPT_ECHO_REQUEST) {
                     recordPacket(now, local, &remote, packet, "RecvEchoReq");
                     packet->Type = TPT_ECHO_RESPONSE;

                     // ====== !!! TEST !!! =================================
                     // packet->CurrentSeqNumber = hton64(1);
                     // =====================================================

                     if(sendto(sd, &buffer, bytes, MSG_DONTWAIT,
                               &remote.sa, remoteLength) < 0) {
                        perror("send() failed");
                     }
                  }
                  else if(packet->Type == TPT_DISCARD) {
                     recordPacket(now, local, &remote, packet, "RecvDiscard");
                  }
               }
               else {
                  fprintf(stderr, "Length error: %u/%u\n", ntohl(packet->Length), (unsigned int)bytes);
               }
            }
         }
      }
      else if(ready == 0) {
         // puts("Timeout");
      }
      else {
         perror("poll()");
         return;
      }
   }
}


// ====== One-way test ======================================================
static void discard(int                   sd,
                    const sockaddr_union* local,
                    const sockaddr_union* remote,
                    const unsigned int    packetCount     = 3,
                    const unsigned int    packetSize      = 1000,
                    const unsigned int    interPacketTime = 1000000)
{
   char           buffer[sizeof(TestPacket) + packetSize];
   const uint64_t testStartTime = getMicroTime();

   for(unsigned int i = 0; i < packetCount; i++) {
      const unsigned int bytes = makePacket((char*)&buffer, sizeof(buffer),
                                            TPT_ECHO_REQUEST, testStartTime,
                                            1 + i, packetCount,
                                            packetSize);
      if(sendto(sd, &buffer, bytes, MSG_DONTWAIT, &remote->sa, sizeof(sockaddr_union)) < 0) {
         perror("send() failed");
      }
      else {
         const TestPacket* packet = (const TestPacket*)&buffer;
         recordPacket(ntoh64(packet->PacketSendTime), local, remote, packet, "SentDiscard", interPacketTime);
      }
      usleep(interPacketTime);
   }
}


// ====== Ping test =========================================================
static void ping(int                   sd,
                 const sockaddr_union* local,
                 const sockaddr_union* remote,
                 const unsigned int    packetCount     = 3,
                 const unsigned int    packetSize      = 1000,
                 const unsigned int    interPacketTime = 1000000)
{
   char                    buffer[sizeof(TestPacket) + packetSize];
   const uint64_t          testStartTime     = getMicroTime();
   uint64_t                nextPacketTime    = 0;
   uint64_t                seqNumber         = 1;
   uint64_t                ackNumber         = 0;
   uint64_t                requestsSent      = 0;
   uint64_t                responsesReceived = 0;
   uint64_t                dupsReceived      = 0;
   uint64_t                oosReceived       = 0;
   boost::dynamic_bitset<> packetMap(packetCount + 1);

   while(true) {
      uint64_t now = getMicroTime();
      if(now >= nextPacketTime) {
         if(seqNumber > packetCount) {
            /*
            printf("%llu/%llu -> %1.3f%% loss\n", (unsigned long long)responsesReceived,
                                                  (unsigned long long)requestsSent,
                                                  100.0 * (requestsSent - responsesReceived) / requestsSent);
            */
            recordEvent(now, testStartTime, local, remote, "LossSummary",
                        packetSize, interPacketTime, 0, 0,
                        "Loss",
                        100.0 * (requestsSent - responsesReceived) / requestsSent);
            recordEvent(now, testStartTime, local, remote, "DupsSummary",
                        packetSize, interPacketTime, 0, 0,
                        "Dups", dupsReceived);
            recordEvent(now, testStartTime, local, remote, "OoSeqSummary",
                        packetSize, interPacketTime, 0, 0,
                        "OoSeq", oosReceived);
            return;
         }
         nextPacketTime = now + interPacketTime;
         const unsigned int bytes = makePacket((char*)&buffer, sizeof(buffer),
                                               TPT_ECHO_REQUEST, testStartTime,
                                               seqNumber, packetCount,
                                               packetSize);
         seqNumber++;
         if(sendto(sd, &buffer, bytes, MSG_DONTWAIT, &remote->sa, sizeof(sockaddr_union)) < 0) {
            perror("send() failed");
         }
         else {
            const TestPacket* packet = (const TestPacket*)&buffer;
            recordPacket(ntoh64(packet->PacketSendTime), local, remote, packet, "SentEchoReq", interPacketTime);
            requestsSent++;
         }
      }

      const int timeout = (int)std::max(((int64_t)nextPacketTime - (int64_t)now) / 1000,
                                        (int64_t)0);
      pollfd pfd;
      pfd.fd      = sd;
      pfd.events  = POLLIN;
      pfd.revents = 0;
      const int ready = poll(&pfd, 1, timeout);
      if( (ready > 0) && (pfd.revents & POLLIN) ) {
         sockaddr_union remote;
         socklen_t      remoteLength = sizeof(remote);
         const ssize_t  bytes = recvfrom(sd, (char*)&buffer, sizeof(buffer), 0,
                                         &remote.sa, &remoteLength);
         now = getMicroTime();
         if(bytes >= (ssize_t)sizeof(TestPacket)) {
            TestPacket* packet = (TestPacket*)&buffer;
            if(ntohl(packet->Length) == bytes) {
               if( (packet->Type == TPT_ECHO_RESPONSE) &&
                   (ntoh64(packet->TestStartTime) == testStartTime) &&
                   (ntoh64(packet->CurrentSeqNumber) <= seqNumber) ) {
                  bool oos = true;
                  if(ntoh64(packet->CurrentSeqNumber) > ackNumber) {
                     oos       = false;
                     ackNumber = ntoh64(packet->CurrentSeqNumber);
                  }
                  else {
                     oosReceived++;
                  }
                  const bool dup = packetMap.test(ntoh64(packet->CurrentSeqNumber));
                  if(!dup) {
                     responsesReceived++;
                  }
                  else {
                     dupsReceived++;
                  }
                  packetMap.set(ntoh64(packet->CurrentSeqNumber), 1);
                  recordPacket(now, local, &remote, packet, "RecvEchoRes", oos, dup);

               }
            }
            else {
               fputs("ERROR: ping protocol violation?!\n", stderr);
            }
         }
      }
   }
}


int main(int argc, char** argv)
{
   if(argc < 5) {
      fprintf(stderr, "Usage: %s output_file udp|sctp local_address:local_port tos [passive|...] {...}\n", argv[0]);
      exit(1);
   }

   // ====== Get local address ==============================================
   sockaddr_union local;
   if(!string2address(argv[3], &local)) {
      fprintf(stderr, "ERROR: Bad local address %s!\n", argv[3]);
      exit(1);
   }

   // ====== Create socket ==================================================
   int sd;
   int protocol;
   if(!(strcmp(argv[2], "udp"))) {
      sd = socket(local.sa.sa_family, SOCK_DGRAM, IPPROTO_UDP);
      protocol = IPPROTO_UDP;
   }
   else if(!(strcmp(argv[2], "sctp"))) {
      sd = socket(local.sa.sa_family, SOCK_SEQPACKET, IPPROTO_SCTP);
      protocol = IPPROTO_SCTP;
   }
   else {
      fprintf(stderr, "ERROR: Bad protocol %s!\n", argv[2]);
      exit(1);
   }

   // ====== Bind to local address ==========================================
   if(bind(sd, (struct sockaddr*)&local.sa, sizeof(local)) < 0) {
      perror("bind() of local address failed");
      exit(1);
   }

   // ====== Set TOS ========================================================
   int opt;
   if( (sscanf(argv[4], "0x%x", &opt) < 1) &&
       (sscanf(argv[4], "%u", &opt) < 1) ) {
      fprintf(stderr, "ERROR: Bad TOS setting %s!\n", argv[4]);
      exit(1);
   }
   if( (opt & 0xfc) != opt ) {
      fprintf(stderr, "ERROR: Bad TOS setting 0x%x: should in [0x00,0xff], with ECN bits not set!\n", opt);
      exit(1);
   }
   if(local.sa.sa_family == AF_INET) {
//      if(setsockopt(sd, SOL_IP, IP_TOS, &opt, sizeof(opt)) < 0) {
	if(setsockopt(sd, IPPROTO_IP, IP_TOS, &opt, sizeof(opt)) < 0) {
         perror("setsockopt() of TOS failed");
         exit(1);
      }
   }
   else {
      if(setsockopt(sd, IPPROTO_IPV6, IPV6_TCLASS, &opt, sizeof(opt)) < 0) {
         perror("setsockopt() of Traffic Class failed");
         exit(1);
      }
   }

   // ====== Open output file ===============================================
   const std::string gOutputFileName   = argv[1];
   OutputFileFormat  gOutputFileFormat = OFF_Plain;
   if( (gOutputFileName.rfind(".bz2") == gOutputFileName.size() - 4) ||
       (gOutputFileName.rfind(".BZ2") == gOutputFileName.size() - 4) ) {
      gOutputFileFormat = OFF_BZip2;
   }
   if(gOutputFile.initialize(gOutputFileName.c_str(), gOutputFileFormat) == false) {
      exit(1);
   }

   if(!(strcmp(argv[5], "passive"))) {
      if(protocol != IPPROTO_UDP) {
         if(listen(sd, 10) < 0) {
            perror("listen() failed");
            exit(1);
         }
      }

      puts("Working in passive mode ...");
      passiveMode(sd, &local);
      exit(1);
   }
   else if(!(strcmp(argv[5], "active"))) {
      if(argc < 6) {
         fputs("ERROR: No remote address specified in active mode!\n", stderr);
         exit(1);
      }
      sockaddr_union remote;
      if(!string2address(argv[6], &remote)) {
         fprintf(stderr, "ERROR: Bad remote address %s!\n", argv[6]);
         exit(1);
      }
      if(connect(sd, (struct sockaddr*)&remote.sa, sizeof(remote)) < 0) {
         perror("connect() to remote address failed");
         exit(1);
      }

      for(int i = 7;i < argc;i++) {
         double a, b, c;
         if(sscanf(argv[i], "ping:%lf:%lf:%lf", &a, &b, &c) == 3) {
            ping(sd, &local, &remote,
                 (unsigned int)a, (unsigned int)b,
                 (unsigned int)(1000000.0 * c));
         }
         else if(sscanf(argv[i], "discard:%lf:%lf:%lf", &a, &b, &c) == 3) {
            discard(sd, &local, &remote,
                    (unsigned int)a, (unsigned int)b,
                    (unsigned int)(1000000.0 * c));
         }
         else {
            fprintf(stderr, "ERROR: Invalid action %s!\n", argv[i]);
            exit(1);
         }
      }
   }

   close(sd);
   if(!gOutputFile.finish()) {
      exit(1);
   }
   return 0;
}
