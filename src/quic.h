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

#ifndef QUIC_H
#define QUIC_H

#include "ext_socket.h"

#include <cstddef>
#include <cstdint>


#if defined(HAVE_QUIC)

#if !defined(ALPN_NETPERFMETER_CONTROL)
#define ALPN_NETPERFMETER_CONTROL "netperfmeter/control"
#endif

bool configureQuicControlSocket(const int sd, const unsigned int idleTimeoutMs);

int quicServerHandshake(int          sd,
                        const char*  alpns,
                        const char*  tlsCAFile,
                        const char*  tlsCertFile,
                        const char*  tlsKeyFile,
                        uint8_t*     sessionKey,
                        unsigned int sessionKeyLength);

int quicClientHandshake(int            sd,
                        const char*    alpns,
                        const char*    host,
                        const char*    tlsCAFile,
                        const char*    tlsCertFile,
                        const char*    tlsKeyFile,
                        const uint8_t* ticketIn,
                        size_t         ticketInLength,
                        uint8_t*       ticketOut,
                        size_t*        ticketOutLength);

void quicRegisterControlSocket(const int sd, const bool isClient);
void quicDeregisterControlSocket(const int sd);
bool quicIsControlSocket(const int sd);
ssize_t quicSendControlMessage(const int sd, const void* buffer, const size_t length);

#endif

#endif
