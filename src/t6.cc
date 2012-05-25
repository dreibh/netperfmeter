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

#include "tools.h"


struct TestPacket {
   uint8_t  Type;
   uint8_t  Flags;
   uint16_t Padding;
   uint32_t Length;
   uint32_t CurrentSeqNumber;
   uint32_t TotalInSequence;
   uint64_t TestStartTime;
   uint64_t PacketSendTime;
   char     Data[];
} __attribute__((packed));

#define TPT_ECHO_REQUEST  1
#define TPT_ECHO_RESPONSE 2


inline unsigned int makePacket(char*              buffer,
                               const size_t       bufferSize,
                               const uint8_t      type,
                               const uint64_t     testStartTime,
                               const uint32_t     currentSeqNumber,
                               const uint32_t     totalInSequence,
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
   packet->CurrentSeqNumber = htonl(currentSeqNumber);
   packet->TotalInSequence  = htonl(totalInSequence);
   for(unsigned int i = 0; i < payloadLength; i++) {
      packet->Data[i] = (i & 0xff);
   }
   packet->TestStartTime  = hton64(testStartTime);
   packet->PacketSendTime = hton64(getMicroTime());

   return(totalLength);
}



void passiveMode(int sd)
{
 puts("pasv");
   while(true) {
      pollfd pfd;
      pfd.fd      = sd;
      pfd.events  = POLLIN|POLLOUT;
      pfd.revents = 0;

      int ready = poll(&pfd, 1, 1000);
      if(ready > 0) {
         if(pfd.revents & POLLIN) {
            char buffer[65536];
            const ssize_t bytes = recv(sd, (char*)&buffer, sizeof(buffer), 0);
            if(bytes >= (ssize_t)sizeof(TestPacket)) {
               const uint64_t    now    = getMicroTime();
               const TestPacket* packet = (const TestPacket*)&buffer;
               if(ntohl(packet->Length) == bytes) {
//                   printf("%llu
//
//                          ",
//                          now,
//

                  printf("R=%d\n", (int)bytes);
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


void ping(int                sd,
          const unsigned int packetCount     = 1,
          const unsigned int packetSize      = 1000,
          const unsigned int interPacketTime = 1000000)
{
   char buffer[sizeof(TestPacket) + packetSize];

   uint64_t testStartTime = getMicroTime();
   for(unsigned int i = 0; i < packetCount; i++) {
      const unsigned int bytes = makePacket((char*)&buffer, sizeof(buffer),
                                            TPT_ECHO_REQUEST, testStartTime,
                                            1 + i, packetCount,
                                            packetSize);
      if(send(sd, &buffer, bytes, MSG_DONTWAIT) < 0) {
         perror("send() failed");
      }
      else puts("ok!");
      usleep(interPacketTime);
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
      passiveMode(sd);
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
            ping(sd);
         }
         else {
            fprintf(stderr, "ERROR: Invalid action %s!\n", argv[i]);
            exit(1);
         }
      }
   }


   sockaddr_union remote;

   if( (argc > 2) &&
       (string2address(argv[1], &local)) &&
       (string2address(argv[2], &remote)) ) {

      puts("CLIENT MODE");

      int sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      if(sd < 0) {
         perror("socket()");
         exit(1);
      }

      if(bind(sd, (struct sockaddr*)&local.sa, sizeof(local)) < 0) {
         perror("bind()");
         exit(1);
      }

      if(connect(sd, (struct sockaddr*)&remote.sa, sizeof(remote)) < 0) {
         perror("connect()");
         exit(1);
      }

      char buffer[1000];
      for(unsigned int i = 0; i < sizeof(buffer); i++) {
         buffer[i] = (i & 0xff);
      }

      for(int n = 0; n < 1; n++) {
         if(send(sd, buffer, sizeof(buffer), 0) < 0) {
            perror("send()");
            exit(1);
         }
      }

      close(sd);
   }
   return 0;
}
