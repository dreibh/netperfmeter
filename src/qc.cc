#include "tools.h"

#include <gnutls/gnutls.h>
#include <string.h>


static int client_handshake(int            sd,
                            const char*    alpns,
                            const char*    host,
                            const char*    tlsCAFile,
                            const uint8_t* ticketIn,
                            size_t         ticketInLength,
                            uint8_t*       ticketOut,
                            size_t*        ticketOutLength)
{
   gnutls_certificate_credentials_t credentials;
   gnutls_session_t                 session;
   size_t                           alpnLength;
   char                             alpn[64];
   int                              error;

puts("C-1");
   error = gnutls_certificate_allocate_credentials(&credentials);
   if(!error) {
printf("T=%s\n", tlsCAFile);
      int loadedCAs = gnutls_certificate_set_x509_trust_file(credentials, tlsCAFile, GNUTLS_X509_FMT_PEM);
      printf("loaded=%d\n", loadedCAs);
      if(loadedCAs <= 0) {
         std::cerr << "Loading CA certificate from " << tlsCAFile << " failed";
         gnutls_certificate_free_credentials(credentials);
         return -1;
      }
puts("C-2");
      error = gnutls_init(&session, GNUTLS_CLIENT|GNUTLS_ENABLE_EARLY_DATA|GNUTLS_NO_END_OF_EARLY_DATA);
      if(!error) {
         error = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, credentials);
puts("C-3");
         if(!error) {
            error = gnutls_priority_set_direct(session, QUIC_PRIORITY, NULL);
         }
puts("C-4");
         if( (!error) && (alpns != NULL) ) {
            error = quic_session_set_alpn(session, alpns, strlen(alpns));
         }
puts("C-5");
         if(!error) {
            printf("H=%s\n", host);
            error = gnutls_server_name_set(session, GNUTLS_NAME_DNS, host, strlen(host));
         }
puts("C-6");
         if(!error) {
puts("C-7");
            gnutls_session_set_verify_cert(session, host, 0);
            gnutls_transport_set_int(session, sd);
            if(ticketIn != NULL) {
               error = quic_session_set_data(session, ticketIn, ticketInLength);
            }
            if(!error) {
puts("C-8");
               error = quic_handshake(session);
               printf("e=%d\n",error);
               if( (!error) && (alpns != NULL) ) {
puts("C-9");
                  alpnLength = sizeof(alpn);
                  error = quic_session_get_alpn(session, alpn, &alpnLength);
               }
            }
            if( (!error) && (ticketOut != NULL) ) {
puts("C-10");
               error =  quic_session_get_data(session, ticketOut, ticketOutLength);
            }
         }
         gnutls_deinit(session);
      }
      gnutls_certificate_free_credentials(credentials);
   }

puts("C-11");
   if(error) {
      std::cerr << "TLS setup failed: " << gnutls_strerror(error) << "\n";
   }
   return error;
}


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

   char address[128];
   address2string(&a.sa, address, sizeof(address), true);
   printf("Connecting to %s ...\n", address);
   if(connect(sd, &a.sa, sizeof(a)) != 0) {
      perror("connect()");
      exit(1);
   }
   printf("Connected %d\n", sd);

   printf("Handshake on %d ...\n", sd);
   const char* alpn = "sample";

   uint8_t ticket[4096];
   size_t  ticketLength = sizeof(ticket);
   if(client_handshake(sd, alpn, "localhost",
                       // "quic-setup/TestCA/TestLevel1/certs/TestLevel1.crt",
                       "/home/dreibh/src/quic/tests/server.pem",
                       NULL, 0, ticket, &ticketLength) != 0) {
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
      int64_t  sid   = 0 | QUIC_STREAM_TYPE_UNI_MASK;   //  ((i + 1) << 4);
      // QUIC_STREAM_TYPE_UNI_MASK;
      uint32_t flags = (i == 0) ? MSG_QUIC_STREAM_NEW : 0;
      printf("sending: %d (sid=%llu flags=%x)\n", (int)strlen(buffer), (unsigned long long)sid, (int)flags);
      ssize_t s = quic_sendmsg(sd, &buffer, strlen(buffer), sid, flags);
      if(s < 0) {
         perror("quic_sendmsg()");
         break;
      }
      printf("sent: %d (sid=%llu)\n", (int)s, (unsigned long long)sid);

      sid   = 0;
      flags = 0;
      ssize_t r = quic_recvmsg(sd, &buffer, sizeof(buffer), &sid, &flags);
      if(r < 0) {
         perror("quic_recvmsg()");
         break;
      }
      printf("received: %d (sid=%llu)\n", (int)r, (unsigned long long)sid);
   }

   close(sd);
}
