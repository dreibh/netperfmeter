/* $Id$
 *
 * Simple Transfer Test
 * Copyright (C) 2012 by Thomas Dreibholz
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


static void recordPacket(const uint64_t        now,
                         const sockaddr_union* local,
                         const sockaddr_union* remote,
                         const TestPacket*     packet,
                         const char*           purpose,
                         const bool            outOfSequence = false,
                         const bool            duplicate     = false)
{
   char localAddress[64];
   if(!address2string(&local->sa, (char*)&localAddress, sizeof(localAddress), true)) {
      localAddress[0] = 0x00;
   }
   char remoteAddress[64];
   if(!address2string(&remote->sa, (char*)&remoteAddress, sizeof(remoteAddress), true)) {
      remoteAddress[0] = 0x00;
   }
   printf("%llu %llu %s \"%s\" \"%s\" %u\t%llu\t%llu\t%1.6f\t\"%s\" \"%s\"\n",
          (unsigned long long)now,
          (unsigned long long)ntoh64(packet->TestStartTime),
          purpose,
          remoteAddress, localAddress,
          (unsigned int)ntohl(packet->Length),
          (unsigned long long)ntoh64(packet->CurrentSeqNumber),
          (unsigned long long)ntoh64(packet->TotalInSequence),
          ((long long)now - (long long)ntoh64(packet->PacketSendTime)) / 1000.0,
          (outOfSequence == true) ? "OOS" : "",
          (duplicate == true)     ? "DUP" : ""
         );
}


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
         puts("TO");
      }
      else {
         perror("poll()");
         return;
      }
   }
}


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
         recordPacket(ntoh64(packet->PacketSendTime), local, remote, packet, "SentDiscard");
      }
      usleep(interPacketTime);
   }
}


#define MAX_PACKET_COUNT
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
   boost::dynamic_bitset<> packetMap(packetCount + 1);

   while(true) {
      uint64_t now = getMicroTime();
      if(now >= nextPacketTime) {
         if(seqNumber > packetCount) {
            printf("%llu/%llu -> %1.3f%% loss\n", (unsigned long long)responsesReceived,
                                                  (unsigned long long)requestsSent,
                                                  100.0 * (requestsSent - responsesReceived) / requestsSent);
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
            recordPacket(ntoh64(packet->PacketSendTime), local, remote, packet, "SentEchoReq");
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
                  const bool dup = packetMap.test(ntoh64(packet->CurrentSeqNumber));
                  if(!dup) {
                     responsesReceived++;
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
   if(argc < 4) {
      fprintf(stderr, "Usage: %s [udp|tcp|sctp|dccp] [local address] [passive|...] {...}\n", argv[0]);
      exit(1);
   }

   int sd;
   if(!(strcmp(argv[1], "udp"))) {
      sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   }
   else if(!(strcmp(argv[1], "tcp"))) {
      sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   }
   else if(!(strcmp(argv[1], "sctp"))) {
      sd = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
   }
   else if(!(strcmp(argv[1], "dccp"))) {
      sd = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_DCCP);
   }
   else {
      fprintf(stderr, "ERROR: Bad protocol %s!\n", argv[1]);
      exit(1);
   }

   sockaddr_union local;
   if(!string2address(argv[2], &local)) {
      fprintf(stderr, "ERROR: Bad local address %s!\n", argv[2]);
      exit(1);
   }
   if(bind(sd, (struct sockaddr*)&local.sa, sizeof(local)) < 0) {
      perror("bind() of local address failed");
      exit(1);
   }

   if(!(strcmp(argv[3], "passive"))) {
      puts("Working in passive mode ...");
      passiveMode(sd, &local);
      exit(1);
   }
   else if(!(strcmp(argv[3], "active"))) {
      if(argc < 4) {
         fputs("ERROR: No remote address specified in active mode!\n", stderr);
         exit(1);
      }
      sockaddr_union remote;
      if(!string2address(argv[4], &remote)) {
         fprintf(stderr, "ERROR: Bad remote address %s!\n", argv[4]);
         exit(1);
      }
      if(connect(sd, (struct sockaddr*)&remote.sa, sizeof(remote)) < 0) {
         perror("connect() to remote address failed");
         exit(1);
      }

      for(int i = 5;i < argc;i++) {
         if(!(strcmp(argv[i], "ping"))) {
            ping(sd, &local, &remote);
         }
         else {
            fprintf(stderr, "ERROR: Invalid action %s!\n", argv[i]);
            exit(1);
         }
      }
   }

   close(sd);
   return 0;
}
