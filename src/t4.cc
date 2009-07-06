#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "tools.h"
#include "ext_socket.h"


int main(int argc, char** argv)
{
   if(argc <= 1) {
	int sock = createAndBindSocket(SOCK_SEQPACKET, IPPROTO_SCTP, 9001);
	if(sock) {
		char buffer[60000];
		for(;;) {
			ssize_t r = ext_recv(sock, (char*)&buffer, sizeof(buffer), 0);
			printf("r=%d\n", r);
		}
	}
   }
   else {
      int sd = ext_socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
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
