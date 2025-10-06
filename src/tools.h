/*
 * ==========================================================================
 *         _   _      _   ____            __ __  __      _
 *        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
 *        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
 *        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
 *        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
 *
 *                  NetPerfMeter -- Network Performance Meter
 *                 Copyright (C) 2009-2025 by Thomas Dreibholz
 * ==========================================================================
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

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

#ifndef TOOLS_H
#define TOOLS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ext_socket.h>

#include <iostream>

#include "mutex.h"


// MPTCP definitions
#ifdef HAVE_MPTCP
#include <linux/mptcp.h>
// #else
// #warning MPTCP is not supported by the API of this system!
// #ifndef IPPROTO_MPTCP
// #define IPPROTO_MPTCP 262
#endif

// DCCP definitions
#ifdef HAVE_DCCP
#include <linux/dccp.h>
// #else
// #warning DCCP is not supported by the API of this system!
#endif


std::string format(const char* fmt, ...);

unsigned long long getMicroTime();
void printTimeStamp(std::ostream& os);
int pollTimeout(const unsigned long long now, const size_t n, ...);


int safestrcpy(char* dest, const char* src, const size_t size);
int safestrcat(char* dest, const char* src, const size_t size);

bool hasSuffix(const std::string& name, const std::string& suffix);
void dissectName(const std::string& name,
                 std::string&       prefix,
                 std::string&       suffix);


union sockaddr_union {
   struct sockaddr     sa;
   struct sockaddr_in  in;
   struct sockaddr_in6 in6;
};

bool checkIPv6();
size_t getSocklen(const struct sockaddr* address);
int addresscmp(const struct sockaddr* a1, const struct sockaddr* a2, const bool port);
bool address2string(const struct sockaddr* address,
                    char*                  buffer,
                    const size_t           length,
                    const bool             port,
                    const bool             hideScope = false);
bool string2address(const char*           string,
                    union sockaddr_union* address,
                    const bool            readPort = true);

void printAddress(std::ostream&          os,
                  const struct sockaddr* address,
                  const bool             port      = true,
                  const bool             hideScope = false);

const char* getProtocolName(const int protocol);
uint16_t getPort(const struct sockaddr* address);
bool setPort(struct sockaddr* address, uint16_t port);
bool sendAbort(int sd, sctp_assoc_t assocID = 0);
int createSocket(const int             family,
                 const int             type,
                 const int             protocol,
                 const unsigned int    localAddresses,
                 const sockaddr_union* localAddressArray);
int bindSocket(const int             sd,
               const int             family,
               const int             type,
               const int             protocol,
               const uint16_t        localPort,
               const unsigned int    localAddresses,
               const sockaddr_union* localAddressArray,
               const bool            listenMode,
               const bool            bindV6Only);
uint64_t hton64(const uint64_t value);
uint64_t ntoh64(const uint64_t value);


typedef unsigned long long network_double_t;

network_double_t doubleToNetwork(double value);
double networkToDouble(const network_double_t value);


void installBreakDetector();
void uninstallBreakDetector();
bool breakDetected();
void sendBreak(const bool quiet);


#define RANDOM_CONSTANT         0
#define RANDOM_UNIFORM          1
#define RANDOM_EXPONENTIAL      2
#define RANDOM_PARETO           3
#define RANDOM_NORMAL           4
#define RANDOM_TRUNCATED_NORMAL 5

const char* getRandomGeneratorName(const uint8_t rng);
double getRandomValue(const double* valueArray, const uint8_t rng);
uint8_t random8();
uint16_t random16();
uint64_t random64();
uint32_t random32();
double randomDouble();
double randomExpDouble(const double mean);
double randomNormal(const double mean, const double stddev);
double randomTruncNormal(const double mean, const double stddev);
double randomParetoDouble(const double location, const double shape);

#ifdef __APPLE__
// Apple's poll() function is broken. We need a wrapper to select() here!
int ext_poll_wrapper(struct pollfd* fdlist, long unsigned int count, int time);
#else
inline int ext_poll_wrapper(struct pollfd* fdlist, long unsigned int count, int time)
{
   return ext_poll(fdlist, count, time);
}
#endif

#endif
