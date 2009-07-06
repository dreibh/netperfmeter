#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <poll.h>
#include <sys/select.h>

#include "tools.h"
#include "ext_socket.h"


int main(int argc, char** argv)
{
   if(argc <= 1) {
      int sd = createAndBindSocket(SOCK_SEQPACKET, IPPROTO_SCTP, 9001);
      if(sd >= 0) {
         printf("Warte auf Socket %d ...\n", sd);

         sctp_event_subscribe events;
         memset((char*)&events, 0 ,sizeof(events));
         events.sctp_data_io_event     = 1;
         events.sctp_association_event = 1;
         if(ext_setsockopt(sd, IPPROTO_SCTP, SCTP_EVENTS, &events, sizeof(events)) < 0) {
            exit(1);
         }

         char buffer[65536];
         for(;;) {
         /*
            pollfd pfd;
            pfd.fd = sd;
            pfd.events = POLLIN;
            int result = ext_poll((pollfd*)&pfd, 1, 1000);
         */        
            fd_set rset;
            FD_ZERO(&rset);
            FD_SET(sd, &rset);
            int result = ext_select(sd + 1, &rset, NULL, NULL, NULL);
                 
            if(result > 0) {
               ssize_t r = ext_recv(sd, (char*)&buffer, sizeof(buffer), 0);
               printf("r=%d\n", (int)r);
            }
         }
      }
   }
   else {
      int sd = ext_socket(AF_INET6, SOCK_STREAM, IPPROTO_SCTP);
      if(sd >= 0) {
         sockaddr_union in;
         if(string2address(argv[1], &in)) {
            puts("Connect ...");
            if(ext_connect(sd, &in.sa, getSocklen(&in.sa)) == 0) {
               char buffer[65536];
               printf("sent=%d\n", (int)ext_send(sd, (char*)&buffer, 1024,0));
            }
            ext_close(sd);
         }
      }
   }
   return 0;
}
