#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <poll.h>
#include <sys/select.h>

#include "tools.h"
#include "ext_socket.h"


int main(int argc, char** argv)
{
   if(argc <= 1) {
	int sock = createAndBindSocket(SOCK_SEQPACKET, IPPROTO_SCTP, 9001);	
	if(sock) {
	printf("Waiting on socket %d\n", sock);
		sctp_event_subscribe events;
		memset((char*)&events, 0 ,sizeof(events));
		events.sctp_data_io_event     = 1;
		events.sctp_association_event = 1;
		if(ext_setsockopt(sock, IPPROTO_SCTP, SCTP_EVENTS, &events, sizeof(events)) < 0) {
		exit(1);
		}
		char buffer[60000];
		for(;;) {
		/*
		        pollfd pfd;
		        pfd.fd = sock;
		        pfd.events = POLLIN;
		        int result = ext_poll((pollfd*)&pfd, 1, 1000);
		*/        
		        fd_set rset;
		        FD_ZERO(&rset);
		        FD_SET(sock, &rset);
		        int result = ext_select(sock + 1, &rset, NULL, NULL, NULL);
		        
		        if(result > 0) {
   			   ssize_t r = ext_recv(sock, (char*)&buffer, sizeof(buffer), 0);
 			   printf("r=%d\n", r);
                        }
		}
	}
   }
   else {
      int sd = ext_socket(AF_INET6, SOCK_STREAM, IPPROTO_SCTP);
printf("sd=%d\n",sd);
      sockaddr_union in;
      if(string2address(argv[1],&in)) {
       puts("Connect...");
       if(ext_connect(sd, &in.sa, getSocklen(&in.sa)) == 0) {
    	 char buffer[60000];
         printf("sent=%d\n", ext_send(sd, (char*)&buffer, 1024,0));
       }
       ext_close(sd);
      }
   }
}
