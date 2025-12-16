#include "tools.h"

#include <gnutls/gnutls.h>
#include <string.h>
#include <poll.h>


// ###### QUIC server handshake #############################################
static int server_handshake(int          sd,
                            const char*  alpns,
                            const char*  host,
                            const char*  tlsCAFile,
                            const char*  tlsCertFile,
                            const char*  tlsKeyFile,
                            uint8_t*     sessionKey,
                            unsigned int sessionKeyLength)
{
   gnutls_certificate_credentials_t credentials;
   gnutls_session_t                 session;
   gnutls_datum_t                   skey = { sessionKey, sessionKeyLength };
   size_t                           alpnLength;
   char                             alpn[64];
   int                              error;

puts("S-1");
   error = gnutls_certificate_allocate_credentials(&credentials);
   if(!error) {
puts("S-2");
      printf("T=%s\n", tlsCAFile);
      int loadedCAs = gnutls_certificate_set_x509_trust_file(credentials, tlsCAFile, GNUTLS_X509_FMT_PEM);
      printf("loaded=%d\n", loadedCAs);
      if(loadedCAs <= 0) {
         std::cerr << "Loading CA certificate from " << tlsCAFile << " failed";
         gnutls_certificate_free_credentials(credentials);
         return -1;
      }

puts("S-3");
      error = gnutls_certificate_set_x509_key_file2(credentials, tlsCertFile, tlsKeyFile, GNUTLS_X509_FMT_PEM, NULL, 0);
      if(!error) {
puts("S-4");
         error = gnutls_init(&session,
                             GNUTLS_SERVER|
                             GNUTLS_NO_AUTO_SEND_TICKET|
                             GNUTLS_ENABLE_EARLY_DATA|GNUTLS_NO_END_OF_EARLY_DATA);
         if(!error) {
puts("S-5");
            error = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, credentials);
            if(!error) {
puts("S-6");
               error = gnutls_session_ticket_enable_server(session, &skey);
               if(!error) {
puts("S-7");
                  error = gnutls_record_set_max_early_data_size(session, 0xffffffffu);
                  if(!error) {
puts("S-8");
                     error = gnutls_priority_set_direct(session, QUIC_PRIORITY, NULL);
                     if( (!error) && (alpns != NULL) ) {
puts("S-9");
                        error = quic_session_set_alpn(session, alpns, strlen(alpns));
                     }
                     if(!error) {
puts("S-10");
                        gnutls_transport_set_int(session, sd);
                        error = quic_handshake(session);
                        printf("e=%d\n",error);
                     }
                     if( (!error) && (alpns != NULL) ) {
puts("S-11");
                        // error = quic_session_get_alpn(session, alpn, &alpnLength);
                        // printf("a=<%s>\n", alpn);
                     }
puts("S-12");
                  }
               }
            }
         }
         gnutls_deinit(session);
      }
      gnutls_certificate_free_credentials(credentials);
   }

puts("S-13");
   if(error) {
      std::cerr << "TLS setup failed: " << gnutls_strerror(error) << "\n";
   }
   return error;
}


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
   printf("Accepted %d\n", accepted);

   uint8_t      sessionKey[64];
   unsigned int sessionKeyLength = sizeof(sessionKey);
   if(getsockopt(sd, SOL_QUIC, QUIC_SOCKOPT_SESSION_TICKET,
                 &sessionKey, &sessionKeyLength)) {
      perror("getsockopt(QUIC_SOCKOPT_SESSION_TICKET)");
      exit(1);
   }

   printf("Handshake on %d ...\n", accepted);
   if(server_handshake(accepted, "sample", "localhost",
                       "/home/dreibh/src/netperfmeter/src/quic-setup/TestCA/TestLevel1/certs/TestLevel1.crt",
                       "/home/dreibh/src/netperfmeter/src/quic-setup/TestCA/localhost/localhost.crt",
                       "/home/dreibh/src/netperfmeter/src/quic-setup/TestCA/localhost/localhost.key",
                       // "/home/dreibh/src/quic/tests/server.pem",
                       // "/home/dreibh/src/quic/tests/server.pem",
                       // "/home/dreibh/src/quic/tests/server.key",
                       sessionKey, sessionKeyLength) != 0) {
      puts("server_handshake() failed!");
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
