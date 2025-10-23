#include "tools.h"

#include <string.h>
#include <poll.h>


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
         "quic-setup/TestCA/pc2.northbound.hencsat/pc2.northbound.hencsat.key",
         "quic-setup/TestCA/pc2.northbound.hencsat/pc2.northbound.hencsat.crt",
         "sample") != 0) {
      perror("quic_server_handshake()");
      exit(1);
   }

   puts("Waiting for incoming data ...");
   bool firstMsg = true;
   while(1) {
      pollfd pfd[1];
      pfd[0].fd      = accepted;
      pfd[0].events  = POLLIN;
      pfd[0].revents = 0;

      puts("poll ...");
      if(poll(pfd, 1, -1) > 0) {
         if(pfd[0].revents & POLLIN) {
            puts("receiving ...");

            char buffer[65536];
            int64_t  sid   = 0;
            uint32_t flags = 0;
            ssize_t r = quic_recvmsg(accepted, &buffer, sizeof(buffer), &sid, &flags);
            if(r < 0) {
               perror("quic_recvmsg()");
               break;
            }
            printf("received: %d (sid=%llu)\n", (int)r, (unsigned long long)sid);

            sid   = 0 | QUIC_STREAM_TYPE_SERVER_MASK | QUIC_STREAM_TYPE_UNI_MASK;
            flags = (firstMsg == true) ? MSG_QUIC_STREAM_NEW : 0;

            printf("sending: %d (sid=%llu flags=%x)\n", (int)r, (unsigned long long)sid, (int)flags);
            ssize_t s = quic_sendmsg(accepted, &buffer, r, sid, flags);
            if(s < 0) {
               perror("quic_sendmsg()");
               break;
            }
            printf("sent: %d (sid=%llu)\n", (int)s, (unsigned long long)sid);

            firstMsg = false;
         }
      }
   }

   close(accepted);
   close(sd);
}
