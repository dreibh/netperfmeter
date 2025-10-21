#include "tools.h"

#include <string.h>


int main(int argc, char** argv)
{
   if(argc < 2) {
      puts("Usage: tc remote_endpoint");
      exit(1);
   }
   sockaddr_union a;
   if(!string2address(argv[1], &a, true)) {
      puts("Bad remote_endpoint!");
      exit(1);
   }

   int sd = socket(a.sa.sa_family, SOCK_DGRAM, IPPROTO_QUIC);
   if(sd < 0) {
      perror("socket()");
      exit(1);
   }

   const char* alpn = "sample";
   // if(setsockopt(sd, SOL_QUIC, QUIC_SOCKOPT_ALPN, alpn, strlen(alpn)) != 0) {
   //    perror("socket(QUIC_SOCKOPT_ALPN)");
   //    exit(1);
   // }

   char address[128];
   address2string(&a.sa, address, sizeof(address), true);
   printf("Connecting to %s ...\n", address);
   if(connect(sd, &a.sa, sizeof(a)) != 0) {
      perror("connect()");
      exit(1);
   }
   printf("Connected %d\n", sd);

   printf("Handshake on %d ...\n", sd);
   if(quic_client_handshake(sd, NULL, "pc2.northbound.hencsat", alpn) != 0) {
      perror("quic_client_handshake()");
      exit(1);
   }


   const unsigned int N = 64;
   /*
   for(unsigned int i = 0; i < N; i++) {
      quic_stream_info si;
      si.stream_id    = ((i + 1) << 4);
      si.stream_flags = 0;
      socklen_t silen = sizeof(si);
      if(getsockopt(sd, SOL_QUIC, QUIC_SOCKOPT_STREAM_OPEN, &si, &silen) != 0) {
         perror("socket(QUIC_SOCKOPT_STREAM_OPEN)");
         exit(1);
      }
      puts("ok");
   }
   */

   puts("Connected!");
   char buffer[65536];
   for(unsigned int i = 0; i < N; i++) {
      printf("Iteration %d:\n", i + 1);
      snprintf(buffer, sizeof(buffer), "This is test #%u!\n", i);
      int64_t  sid   = ((i + 1) << 4);
      // QUIC_STREAM_TYPE_UNI_MASK;
      uint32_t flags = MSG_STREAM_NEW;
      printf("sending: %d (sid=%llu)\n", (int)strlen(buffer), (unsigned long long)sid);
      ssize_t s = quic_sendmsg(sd, &buffer, strlen(buffer), sid, flags);
      if(s < 0) {
         perror("send()");
         break;
      }
      printf("sent: %d (sid=%llu)\n", (int)s, (unsigned long long)sid);

      sid   = 0;
      flags = 0;
      ssize_t r = quic_recvmsg(sd, &buffer, sizeof(buffer), &sid, &flags);
      if(r < 0) {
         perror("recv()");
         break;
      }
      printf("received: %d (sid=%llu)\n", (int)r, (unsigned long long)sid);
   }

   close(sd);
}
