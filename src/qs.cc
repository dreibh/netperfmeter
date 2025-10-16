#include "tools.h"

#include <string.h>


int main(int argc, char** argv)
{
   int sd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_QUIC);
   if(sd < 0) {
      perror("socket()");
      exit(1);
   }

   sockaddr_union a;
   memset((void*)&a, 0, sizeof(a));
   a.in6.sin6_family = AF_INET6;
   a.in6.sin6_port = htons(1234);
   if(bind(sd, &a.sa, sizeof(a)) != 0) {
      perror("bind()");
      exit(1);
   }

   const char* alpn = "sample";
   if(setsockopt(sd, SOL_QUIC, QUIC_SOCKOPT_ALPN, alpn, strlen(alpn)) != 0) {
      perror("socket(QUIC_SOCKOPT_ALPN)");
      exit(1);
   }

   if(listen(sd, 1) != 0) {
      perror("listen()");
      exit(1);
   }

   char address[128];
   address2string(&a.sa, address, sizeof(address), true);
   printf("Listening on %s ...\n", address);
   int accepted = accept(sd, 0, 0);
   if(accepted < 0) {
      perror("accept()");
      exit(1);
   }
   printf("Accepted %d\n", sd);

   printf("Handshake on %d ...\n", accepted);
   if(quic_server_handshake(accepted,
         "/home/nornetpp/src/quic/tests/keys/server-key.pem",
         "/home/nornetpp/src/quic/tests/keys/server-cert.pem",
         "sample") != 0) {
      perror("quic_server_handshake()");
      exit(1);
   }

   puts("Waiting for incoming data ...");
   while(1) {
      char buffer[65536];
      int64_t  sid   = 0;
      uint32_t flags = 0;
      ssize_t r = quic_recvmsg(accepted, &buffer, sizeof(buffer), &sid, &flags);
      if(r < 0) {
         perror("recv()");
         break;
      }
      printf("received: %d (sid=%llu)\n", (int)r, (unsigned long long)sid);

      flags = 0;
      ssize_t s = quic_sendmsg(accepted, &buffer, r, sid, flags);
      if(s < 0) {
         perror("send()");
         break;
      }
      printf("sent: %d (sid=%llu)\n", (int)s, (unsigned long long)sid);
   }

   close(accepted);
   close(sd);
}

/*
if(connect(sd, (const sockaddr*)&a, sizeof(a)) != 0) {
      perror("connect();
   }*/

