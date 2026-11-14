/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2026 by Thomas Dreibholz
 * ==========================================================================
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact:  dreibh@simula.no
 * Homepage: https://www.nntb.no/~dreibh/netperfmeter/
 */

#include "quic.h"
#include "loglevel.h"
#include "mutex.h"
#include "tools.h"

#include <cstring>
#include <iostream>
#include <map>


#if defined(HAVE_QUIC)

static Mutex               gQuicControlMutex;
static std::map<int, bool> gQuicControlSockets;


// ###### Configure QUIC control socket parameters ##########################
bool configureQuicControlSocket(const int sd, const unsigned int idleTimeoutMs)
{
   if(idleTimeoutMs > 0) {
      quic_transport_param param;
      socklen_t            paramLength = sizeof(param);
      memset(&param, 0, sizeof(param));
      param.remote = 0;
      if(ext_getsockopt(sd, SOL_QUIC, QUIC_SOCKOPT_TRANSPORT_PARAM, &param, &paramLength) < 0) {
         return false;
      }
      param.max_idle_timeout = (uint64_t)idleTimeoutMs * 1000ULL;
      if(ext_setsockopt(sd, SOL_QUIC, QUIC_SOCKOPT_TRANSPORT_PARAM, &param, paramLength) < 0) {
         return false;
      }
   }
   return true;
}


// ###### QUIC server handshake #############################################
int quicServerHandshake(int          sd,
                        const char*  alpns,
                        const char*  tlsCAFile,
                        const char*  tlsCertFile,
                        const char*  tlsKeyFile,
                        uint8_t*     sessionKey,
                        unsigned int sessionKeyLength)
{
   gnutls_certificate_credentials_t credentials;
   gnutls_session_t                 session;
   gnutls_datum_t                   skey = { sessionKey, sessionKeyLength };
   int                              error;

   error = gnutls_certificate_allocate_credentials(&credentials);
   if(!error) {
      if(tlsCAFile != nullptr) {
         const int loadedCAs =
            gnutls_certificate_set_x509_trust_file(credentials,
                                                   tlsCAFile, GNUTLS_X509_FMT_PEM);
         if(loadedCAs <= 0) {
            LOG_ERROR
            stdlog << "Loading CA certificate from " << tlsCAFile << " failed\n";
            LOG_END
            gnutls_certificate_free_credentials(credentials);
            return -1;
         }
      }

      error = gnutls_certificate_set_x509_key_file(credentials,
                                                   tlsCertFile, tlsKeyFile,
                                                   GNUTLS_X509_FMT_PEM);
      if(!error) {
         error = gnutls_init(&session,
                             GNUTLS_SERVER|
                             GNUTLS_NO_AUTO_SEND_TICKET|
                             GNUTLS_ENABLE_EARLY_DATA|GNUTLS_NO_END_OF_EARLY_DATA);
         if(!error) {
            error = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, credentials);
            if(!error) {
               if(tlsCAFile != nullptr) {
                  gnutls_certificate_server_set_request(session, GNUTLS_CERT_REQUEST);
                  gnutls_session_set_verify_cert(session, nullptr, 0);
               }
               error = gnutls_session_ticket_enable_server(session, &skey);
               if(!error) {
                  error = gnutls_record_set_max_early_data_size(session, 0xffffffffu);
                  if(!error) {
                     error = gnutls_priority_set_direct(session, QUIC_PRIORITY, NULL);
                     if( (!error) && (alpns != NULL) ) {
                        error = quic_session_set_alpn(session, alpns, strlen(alpns));
                     }
                     if(!error) {
                        gnutls_transport_set_int(session, sd);
                        error = quic_handshake(session);
                        if( (!error) && (alpns != NULL) ) {
                           size_t alpnLength;
                           char   alpn[64];
                           alpnLength = sizeof(alpn);
                           error = quic_session_get_alpn(session, alpn, &alpnLength);
                        }
                     }
                  }
               }
            }
         }
         gnutls_deinit(session);
      }
      gnutls_certificate_free_credentials(credentials);
   }

   if(error) {
      LOG_ERROR
      stdlog << "TLS setup failed: " << gnutls_strerror(error) << "\n";
      if(error < 0) {
         quic_connection_close closeInfo;
         socklen_t             closeInfoLength = sizeof(closeInfo);
         memset(&closeInfo, 0, sizeof(closeInfo));
         if(ext_getsockopt(sd, SOL_QUIC, QUIC_SOCKOPT_CONNECTION_CLOSE,
                           &closeInfo, &closeInfoLength) == 0) {
            closeInfo.phrase[sizeof(closeInfo.phrase) - 1] = 0x00;
            stdlog << format("QUIC connection closed: errcode=%u, frame=%u, phrase=\"%s\"",
                             closeInfo.errcode, (unsigned int)closeInfo.frame,
                             (const char*)closeInfo.phrase) << "\n";
         }
      }
      LOG_END
   }
   return error;
}


// ###### QUIC client handshake #############################################
int quicClientHandshake(int            sd,
                        const char*    alpns,
                        const char*    host,
                        const char*    tlsCAFile,
                        const char*    tlsCertFile,
                        const char*    tlsKeyFile,
                        const uint8_t* ticketIn,
                        size_t         ticketInLength,
                        uint8_t*       ticketOut,
                        size_t*        ticketOutLength)
{
   gnutls_certificate_credentials_t credentials;
   gnutls_session_t                 session;
   int                              error;

   error = gnutls_certificate_allocate_credentials(&credentials);
   if(!error) {
      int loadedCAs;
      if(tlsCAFile != nullptr) {
         loadedCAs = gnutls_certificate_set_x509_trust_file(credentials, tlsCAFile, GNUTLS_X509_FMT_PEM);
      }
      else {
         loadedCAs = gnutls_certificate_set_x509_system_trust(credentials);
      }
      if(loadedCAs <= 0) {
         std::cerr << "ERROR: Loading CA certificate from "
                   << ((tlsCAFile != nullptr) ? tlsCAFile : "system truststore") << " failed\n";
         gnutls_certificate_free_credentials(credentials);
         return -1;
      }

      if((tlsCertFile != nullptr) && (tlsKeyFile != nullptr)) {
         error = gnutls_certificate_set_x509_key_file(credentials,
                                                      tlsCertFile, tlsKeyFile,
                                                      GNUTLS_X509_FMT_PEM);
         if(error < 0) {
            std::cerr << "ERROR: Loading client certificate from " << tlsCertFile
                      << " and key from " << tlsKeyFile
                      << " failed: " << gnutls_strerror(error) << "\n";
            gnutls_certificate_free_credentials(credentials);
            return -1;
         }
      }

      error = gnutls_init(&session, GNUTLS_CLIENT|GNUTLS_ENABLE_EARLY_DATA|GNUTLS_NO_END_OF_EARLY_DATA);
      if(!error) {
         error = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, credentials);
         if(!error) {
            error = gnutls_priority_set_direct(session, QUIC_PRIORITY, NULL);
         }
         if( (!error) && (alpns != NULL) ) {
            error = quic_session_set_alpn(session, alpns, strlen(alpns));
         }
         if(!error) {
            error = gnutls_server_name_set(session, GNUTLS_NAME_DNS, host, (host != NULL) ? strlen(host) : 0);
         }
         if(!error) {
            gnutls_session_set_verify_cert(session, host, 0);
            gnutls_transport_set_int(session, sd);
            if(ticketIn != NULL) {
               error = quic_session_set_data(session, ticketIn, ticketInLength);
            }
            if(!error) {
               error = quic_handshake(session);
            }
            if( (!error) && (ticketOut != NULL) ) {
               error = quic_session_get_data(session, ticketOut, ticketOutLength);
            }
         }
         gnutls_deinit(session);
      }
      gnutls_certificate_free_credentials(credentials);
   }

   if(error) {
      std::cerr << "ERROR: TLS setup failed: " << gnutls_strerror(error) << "\n";
      if(error < 0) {
         quic_connection_close closeInfo;
         socklen_t             closeInfoLength = sizeof(closeInfo);
         memset(&closeInfo, 0, sizeof(closeInfo));
         if(ext_getsockopt(sd, SOL_QUIC, QUIC_SOCKOPT_CONNECTION_CLOSE,
                           &closeInfo, &closeInfoLength) == 0) {
            closeInfo.phrase[sizeof(closeInfo.phrase) - 1] = 0x00;
            std::cerr << format("ERROR: QUIC connection closed: errcode=%u, frame=%u, phrase=\"%s\"",
                                closeInfo.errcode, (unsigned int)closeInfo.frame,
                                (const char*)closeInfo.phrase) << "\n";
         }
      }
   }
   return error;
}


// ###### Register QUIC control socket ######################################
void quicRegisterControlSocket(const int sd, const bool isClient)
{
   gQuicControlMutex.lock();
   gQuicControlSockets[sd] = isClient;
   gQuicControlMutex.unlock();
}


// ###### Deregister QUIC control socket ####################################
void quicDeregisterControlSocket(const int sd)
{
   gQuicControlMutex.lock();
   gQuicControlSockets.erase(sd);
   gQuicControlMutex.unlock();
}


// ###### Check whether socket descriptor is a QUIC control socket ##########
bool quicIsControlSocket(const int sd)
{
   gQuicControlMutex.lock();
   const bool found = (gQuicControlSockets.find(sd) != gQuicControlSockets.end());
   gQuicControlMutex.unlock();
   return found;
}


// ###### Send message over QUIC control socket (stream 0) ##################
ssize_t quicSendControlMessage(const int sd, const void* buffer, const size_t length)
{
   uint32_t flags = 0;
   gQuicControlMutex.lock();
   std::map<int, bool>::iterator found = gQuicControlSockets.find(sd);
   if(found != gQuicControlSockets.end()) {
      if(found->second) {
         flags         = MSG_QUIC_STREAM_NEW;
         found->second = false;
      }
   }
   gQuicControlMutex.unlock();

   const char* data      = (const char*)buffer;
   size_t      totalSent = 0;
   while(totalSent < length) {
      const ssize_t sent = quic_sendmsg(sd, data + totalSent, length - totalSent, 0, flags);
      if(sent <= 0) {
         return (totalSent > 0) ? (ssize_t)totalSent : sent;
      }
      totalSent += (size_t)sent;
      flags = 0;
   }
   return (ssize_t)totalSent;
}

#endif
