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

#if defined(HAVE_QUIC)

#include "loglevel.h"

#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <type_traits>


namespace {

// Maps active QUIC control socket descriptor -> needs MSG_QUIC_STREAM_NEW on next send
std::mutex          gQuicControlMutex;
std::map<int, bool> gQuicControlSockets;

struct CredentialsDeleter
{
   void operator()(gnutls_certificate_credentials_t cred) const noexcept
   {
      if(cred != nullptr) {
         gnutls_certificate_free_credentials(cred);
      }
   }
};

struct SessionDeleter
{
   void operator()(gnutls_session_t session) const noexcept
   {
      if(session != nullptr) {
         gnutls_deinit(session);
      }
   }
};

using CredentialsPtr = std::unique_ptr<std::remove_pointer_t<gnutls_certificate_credentials_t>, CredentialsDeleter>;
using SessionPtr     = std::unique_ptr<std::remove_pointer_t<gnutls_session_t>, SessionDeleter>;


void logQuicConnectionClose(const int sd, const bool toStdErr)
{
   quic_connection_close closeInfo;
   socklen_t             closeInfoLength = sizeof(closeInfo);
   std::memset(&closeInfo, 0, sizeof(closeInfo));
   if(ext_getsockopt(sd, SOL_QUIC, QUIC_SOCKOPT_CONNECTION_CLOSE,
                     &closeInfo, &closeInfoLength) == 0) {
      closeInfo.phrase[sizeof(closeInfo.phrase) - 1] = 0x00;
      if(toStdErr) {
         // TRANSLATORS: Error details when a QUIC connection is closed during handshake. Placeholders: {} 1: error code, 2: frame type, 3: reason phrase.
         std::cerr << formatText(gettext("ERROR: QUIC connection closed: errcode={}, frame={}, phrase=\"{}\""),
                                 closeInfo.errcode,
                                 static_cast<unsigned int>(closeInfo.frame),
                                 reinterpret_cast<const char*>(closeInfo.phrase)) << "\n";
      }
      else {
         // TRANSLATORS: Error details when a QUIC connection is closed during handshake. Placeholders: {} 1: error code, 2: frame type, 3: reason phrase.
         stdlog << formatText(gettext("QUIC connection closed: errcode={}, frame={}, phrase=\"{}\""),
                              closeInfo.errcode,
                              static_cast<unsigned int>(closeInfo.frame),
                              reinterpret_cast<const char*>(closeInfo.phrase)) << "\n";
      }
   }
}

}   // namespace


// ###### QUIC server handshake #############################################
int quicServerHandshake(const int sd, const QuicTlsConfig& config)
{
   if((config.certFile == nullptr) || (config.keyFile == nullptr)) {
      LOG_ERROR
      // TRANSLATORS: Error message when server TLS certificate or key file is missing for QUIC.
      stdlog << gettext("Server certificate and private key are required for QUIC!") << "\n";
      LOG_END
      return -1;
   }

   gnutls_certificate_credentials_t rawCred = nullptr;
   int error = gnutls_certificate_allocate_credentials(&rawCred);
   if(error < 0) {
      LOG_ERROR
      // TRANSLATORS: Error message when TLS setup fails. {} is the GnuTLS error string.
      stdlog << formatText(gettext("TLS setup failed: {}"), gnutls_strerror(error)) << "\n";
      LOG_END
      return error;
   }
   CredentialsPtr credentials(rawCred);

   if(config.caFile != nullptr) {
      const int loadedCAs =
         gnutls_certificate_set_x509_trust_file(credentials.get(),
                                                config.caFile, GNUTLS_X509_FMT_PEM);
      if(loadedCAs <= 0) {
         LOG_ERROR
         // TRANSLATORS: Error message when TLS CA certificate fails to load. {} is the file path.
         stdlog << formatText(gettext("Loading CA certificate from {} failed"), config.caFile) << "\n";
         LOG_END
         return -1;
      }
   }

   error = gnutls_certificate_set_x509_key_file(credentials.get(),
                                                config.certFile, config.keyFile,
                                                GNUTLS_X509_FMT_PEM);
   if(error < 0) {
      LOG_ERROR
      // TRANSLATORS: Error message when TLS server certificate/key fails to load. Placeholders: {} 1: cert path, 2: key path, 3: GnuTLS error string.
      stdlog << formatText(gettext("Loading server certificate from {} and key from {} failed: {}"),
                           config.certFile, config.keyFile, gnutls_strerror(error)) << "\n";
      LOG_END
      return error;
   }

   gnutls_session_t rawSession = nullptr;
   error = gnutls_init(&rawSession,
                       GNUTLS_SERVER |
                       GNUTLS_NO_AUTO_SEND_TICKET |
                       GNUTLS_ENABLE_EARLY_DATA |
                       GNUTLS_NO_END_OF_EARLY_DATA);
   if(error < 0) {
      LOG_ERROR
      // TRANSLATORS: Error message when TLS setup fails. {} is the GnuTLS error string.
      stdlog << formatText(gettext("TLS setup failed: {}"), gnutls_strerror(error)) << "\n";
      LOG_END
      return error;
   }
   SessionPtr session(rawSession);

   error = gnutls_credentials_set(session.get(), GNUTLS_CRD_CERTIFICATE, credentials.get());
   if((error == 0) && (config.caFile != nullptr)) {
      gnutls_certificate_server_set_request(session.get(), GNUTLS_CERT_REQUEST);
      gnutls_session_set_verify_cert(session.get(), nullptr, 0);
   }
   if((error == 0) && (config.sessionKey != nullptr) && (config.sessionKeyLength > 0)) {
      gnutls_datum_t skey = {
         const_cast<unsigned char*>(config.sessionKey),
         config.sessionKeyLength
      };
      error = gnutls_session_ticket_enable_server(session.get(), &skey);
   }
   if(error == 0) {
      error = gnutls_record_set_max_early_data_size(session.get(), 0xffffffffu);
   }
   if(error == 0) {
      error = gnutls_priority_set_direct(session.get(), QUIC_PRIORITY, nullptr);
   }
   if((error == 0) && (config.alpn != nullptr)) {
      error = quic_session_set_alpn(session.get(), config.alpn, std::strlen(config.alpn));
   }
   if(error == 0) {
      gnutls_transport_set_int(session.get(), sd);
      error = quic_handshake(session.get());
   }
   if((error == 0) && (config.alpn != nullptr)) {
      char   negotiatedAlpn[64];
      size_t negotiatedAlpnLength = sizeof(negotiatedAlpn);
      error = quic_session_get_alpn(session.get(), negotiatedAlpn, &negotiatedAlpnLength);
   }

   if(error != 0) {
      LOG_ERROR
      // TRANSLATORS: Error message when TLS setup fails. {} is the GnuTLS error string.
      stdlog << formatText(gettext("TLS setup failed: {}"), gnutls_strerror(error)) << "\n";
      if(error < 0) {
         logQuicConnectionClose(sd, false);
      }
      LOG_END
   }
   return error;
}


// ###### QUIC client handshake #############################################
int quicClientHandshake(const int sd, const QuicTlsConfig& config)
{
   gnutls_certificate_credentials_t rawCred = nullptr;
   int error = gnutls_certificate_allocate_credentials(&rawCred);
   if(error < 0) {
      // TRANSLATORS: Error message when TLS setup fails. {} is the GnuTLS error string.
      std::cerr << formatText(gettext("ERROR: TLS setup failed: {}"), gnutls_strerror(error)) << "\n";
      return error;
   }
   CredentialsPtr credentials(rawCred);

   if(config.caFile != nullptr) {
      const int loadedCAs =
         gnutls_certificate_set_x509_trust_file(credentials.get(),
                                                config.caFile, GNUTLS_X509_FMT_PEM);
      if(loadedCAs <= 0) {
         // TRANSLATORS: Error message when TLS CA certificate fails to load. {} is the file path.
         std::cerr << formatText(gettext("ERROR: Loading CA certificate from {} failed"), config.caFile) << "\n";
         return -1;
      }
   }
   else {
      const int loadedCAs = gnutls_certificate_set_x509_system_trust(credentials.get());
      if(loadedCAs < 0) {
         // TRANSLATORS: Error message when system CA certificates fail to load. {} is the GnuTLS error string.
         std::cerr << formatText(gettext("ERROR: Loading system CA certificates failed: {}"),
                                 gnutls_strerror(loadedCAs)) << "\n";
         return loadedCAs;
      }
   }

   if((config.certFile != nullptr) && (config.keyFile != nullptr)) {
      error = gnutls_certificate_set_x509_key_file(credentials.get(),
                                                   config.certFile, config.keyFile,
                                                   GNUTLS_X509_FMT_PEM);
      if(error < 0) {
         // TRANSLATORS: Error message when TLS client certificate/key fails to load. Placeholders: {} 1: cert path, 2: key path, 3: GnuTLS error string.
         std::cerr << formatText(gettext("ERROR: Loading client certificate from {} and key from {} failed: {}"),
                                 config.certFile, config.keyFile, gnutls_strerror(error)) << "\n";
         return error;
      }
   }

   gnutls_session_t rawSession = nullptr;
   error = gnutls_init(&rawSession,
                       GNUTLS_CLIENT |
                       GNUTLS_ENABLE_EARLY_DATA |
                       GNUTLS_NO_END_OF_EARLY_DATA);
   if(error < 0) {
      // TRANSLATORS: Error message when TLS setup fails. {} is the GnuTLS error string.
      std::cerr << formatText(gettext("ERROR: TLS setup failed: {}"), gnutls_strerror(error)) << "\n";
      return error;
   }
   SessionPtr session(rawSession);

   error = gnutls_credentials_set(session.get(), GNUTLS_CRD_CERTIFICATE, credentials.get());
   if(error == 0) {
      error = gnutls_priority_set_direct(session.get(), QUIC_PRIORITY, nullptr);
   }
   if((error == 0) && (config.alpn != nullptr)) {
      error = quic_session_set_alpn(session.get(), config.alpn, std::strlen(config.alpn));
   }
   if((error == 0) && (config.hostname != nullptr) && (config.hostname[0] != '\0')) {
      error = gnutls_server_name_set(session.get(), GNUTLS_NAME_DNS,
                                     config.hostname, std::strlen(config.hostname));
   }
   if(error == 0) {
      const char* verifyHost = ((config.hostname != nullptr) && (config.hostname[0] != '\0')) ?
                                  config.hostname : nullptr;
      gnutls_session_set_verify_cert(session.get(), verifyHost, 0);
      gnutls_transport_set_int(session.get(), sd);
      if(config.ticketIn != nullptr) {
         error = quic_session_set_data(session.get(), config.ticketIn, config.ticketInLength);
      }
      if(error == 0) {
         error = quic_handshake(session.get());
      }
      if((error == 0) && (config.ticketOut != nullptr) && (config.ticketOutLength != nullptr)) {
         error = quic_session_get_data(session.get(), config.ticketOut, config.ticketOutLength);
      }
   }

   if(error != 0) {
      // TRANSLATORS: Error message when TLS setup fails. {} is the GnuTLS error string.
      std::cerr << formatText(gettext("ERROR: TLS setup failed: {}"), gnutls_strerror(error)) << "\n";
      if(error < 0) {
         logQuicConnectionClose(sd, true);
      }
   }
   return error;
}


// ###### Configure QUIC control socket parameters ##########################
bool configureQuicControlSocket(const int sd, const unsigned int idleTimeoutMs)
{
   if(idleTimeoutMs > 0) {
      quic_transport_param param;
      socklen_t            paramLength = sizeof(param);
      std::memset(&param, 0, sizeof(param));
      param.remote = 0;
      if(ext_getsockopt(sd, SOL_QUIC, QUIC_SOCKOPT_TRANSPORT_PARAM, &param, &paramLength) < 0) {
         return false;
      }
      param.max_idle_timeout = static_cast<uint64_t>(idleTimeoutMs) * 1000ULL;
      if(ext_setsockopt(sd, SOL_QUIC, QUIC_SOCKOPT_TRANSPORT_PARAM, &param, paramLength) < 0) {
         return false;
      }
   }
   return true;
}


// ###### Register QUIC control socket ######################################
void quicRegisterControlSocket(const int sd, const bool isClient)
{
   std::lock_guard<std::mutex> lock(gQuicControlMutex);
   gQuicControlSockets[sd] = isClient;
}


// ###### Deregister QUIC control socket ####################################
void quicDeregisterControlSocket(const int sd)
{
   std::lock_guard<std::mutex> lock(gQuicControlMutex);
   gQuicControlSockets.erase(sd);
}


// ###### Check whether socket descriptor is a QUIC control socket ##########
bool quicIsControlSocket(const int sd)
{
   std::lock_guard<std::mutex> lock(gQuicControlMutex);
   return gQuicControlSockets.find(sd) != gQuicControlSockets.end();
}


// ###### Send control message over QUIC bidirectional stream 0 #############
ssize_t quicSendControlMessage(const int sd, const void* buffer, const size_t length)
{
   uint32_t flags = 0;
   {
      std::lock_guard<std::mutex> lock(gQuicControlMutex);
      const auto found = gQuicControlSockets.find(sd);
      if((found != gQuicControlSockets.end()) && found->second) {
         flags         = MSG_QUIC_STREAM_NEW;
         found->second = false;
      }
   }
   // Stream ID 0 = client-initiated bidirectional stream ((0 << 2) | 0)
   return quic_sendmsg(sd, buffer, length, 0, flags);
}

#endif
