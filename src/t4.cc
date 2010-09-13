#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/sctp.h>


int main(int argc, char** argv)
{
   if(argc <= 1) {
      puts("SERVER MODE");

      sockaddr_in local;
      memset(&local, 0, sizeof(local));
      local.sin_family = AF_INET;
      local.sin_port   = htons(8000);

      int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
      if(sd < 0) {
         perror("socket()");
         exit(1);
      }

      if(bind(sd, (struct sockaddr*)&local, sizeof(local)) < 0) {
         perror("bind()");
         exit(1);
      }

      if(listen(sd, 5) < 0) {
         perror("bind()");
         exit(1);
      }

      for(;;) {
         int sd2 = accept(sd, NULL, NULL);
         if(sd2 >= 0) {
            char data[1404];
            memset(&data, 'A', sizeof(data));
            for(;;) {
               if(send(sd2, &data, sizeof(data), MSG_NOSIGNAL) < 0) {
                  perror("send()");
                  break;
               }
//                printf("."); fflush(stdout);
//                usleep(100000);
            }
            close(sd2);
         }
      }
   }

   else {
      puts("CLIENT MODE");

      sockaddr_in remote;
      memset(&remote, 0, sizeof(remote));
      remote.sin_family = AF_INET;
      remote.sin_port   = htons(8000);
      if(inet_pton(AF_INET, argv[1], &remote.sin_addr) != 1) {
         perror("inet_pton()");
         exit(1);
      }

      int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
      if(sd < 0) {
         perror("socket()");
         exit(1);
      }

      if(connect(sd, (struct sockaddr*)&remote, sizeof(remote)) < 0) {
         perror("connect()");
         exit(1);
      }

         struct sctp_paddrparams paddrparams;
         memset(&paddrparams, 0, sizeof(paddrparams));
         paddrparams.spp_hbinterval = 250;
         paddrparams.spp_flags      = SPP_HB_ENABLE|SPP_HB_DEMAND;
         memcpy(&paddrparams.spp_address, &remote, sizeof(remote));
         if(setsockopt(sd, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &paddrparams, sizeof(paddrparams)) < 0) {
            perror("setsockopt()");
            exit(1);
         }

      int n = 0;
      while(n < 1000000) {

         fd_set rset;
         FD_ZERO(&rset);
         FD_SET(sd, &rset);
         int result = select(sd + 1, &rset, NULL, NULL, NULL);

         if(result > 0) {
            char buffer[65536];
            ssize_t r = recv(sd, (char*)&buffer, sizeof(buffer), MSG_NOSIGNAL);
            if(r < 0) {
               perror("recv()");
               exit(1);
            }
            n++;
            if((n % 1000) == 0) {
               printf("%d: received %d bytes\n", n, (int)r);
            }
         }
      }
      close(sd);
   }
   return 0;
}
