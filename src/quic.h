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

#include "netperfmeterpackets.h"
#include "tools.h"

#if defined(HAVE_QUIC)

#include <cstddef>
#include <cstdint>

struct QuicTlsConfig
{
   const char*    alpn             = ALPN_NETPERFMETER_DATA;
   const char*    hostname         = nullptr;
   const char*    caFile           = nullptr;
   const char*    certFile         = nullptr;
   const char*    keyFile          = nullptr;
   const uint8_t* sessionKey       = nullptr;
   unsigned int   sessionKeyLength = 0;
   const uint8_t* ticketIn         = nullptr;
   size_t         ticketInLength   = 0;
   uint8_t*       ticketOut        = nullptr;
   size_t*        ticketOutLength  = nullptr;
};

int quicServerHandshake(int sd, const QuicTlsConfig& config);
int quicClientHandshake(int sd, const QuicTlsConfig& config);

#endif

#endif // QUIC_H
