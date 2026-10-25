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

#ifndef TOOLS_H
#define TOOLS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <string_view>
#include <unistd.h>

#include <ext_socket.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#else
#define bindtextdomain(domain, dirname) { }
#define textdomain(domain)              { }
#define gettext(string)                 (string)
#define ngettext(singular, plural, n)   (((n) == 1) ? (singular) : (plural))
#endif

// Endianness conversions: htobe*(), be*toh():
#if defined(__linux__) || defined(__OpenBSD__) || defined(__sun) || defined(__sun__) || defined(__GNU__)
#include <endian.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/endian.h>
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define htobe16(x) OSSwapHostToBigInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define htobe32(x) OSSwapHostToBigInt32(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define htole16(x) OSSwapHostToLittleInt16(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)
#else
#error Unsupported platform; add missing case here!
#endif

#include "mutex.h"

// MPTCP definitions
#if defined(HAVE_MPTCP)
#include <linux/mptcp.h>
#endif

// QUIC definitions
#if defined(HAVE_QUIC)
extern "C" {
#include <netinet/quic.h>
}
#endif

// DCCP definitions
#if defined(HAVE_DCCP)
#include <linux/dccp.h>
#endif


// ###### Modern C++20 string formatting ###################################
template<typename... Args>
inline std::string formatText(const std::string_view fmt, const Args&... args)
{
   try {
      return std::vformat(fmt, std::make_format_args(args...));
   }
   catch(const std::format_error&) {
      return std::string(fmt);
   }
}


// ###### Chrono-based time helpers #########################################
struct MonotonicMicroTime
{
   uint64_t value{0};

   constexpr MonotonicMicroTime() noexcept = default;
   constexpr explicit MonotonicMicroTime(uint64_t v) noexcept : value(v) {}

   constexpr auto operator<=>(const MonotonicMicroTime&) const noexcept = default;

   // Duration between two monotonic timepoints is uint64_t microseconds:
   constexpr uint64_t operator-(const MonotonicMicroTime& other) const noexcept {
      return value - other.value;
   }
   constexpr MonotonicMicroTime operator+(uint64_t us) const noexcept {
      return MonotonicMicroTime(value + us);
   }
   constexpr MonotonicMicroTime operator-(uint64_t us) const noexcept {
      return MonotonicMicroTime(value - us);
   }
};

struct SystemMicroTime
{
   uint64_t value{0};

   constexpr SystemMicroTime() noexcept = default;
   constexpr explicit SystemMicroTime(uint64_t v) noexcept : value(v) {}

   constexpr auto operator<=>(const SystemMicroTime&) const noexcept = default;
};

[[nodiscard]] inline MonotonicMicroTime getMonotonicMicroTime() noexcept
{
   return MonotonicMicroTime(static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
         std::chrono::steady_clock::now().time_since_epoch()
      ).count()
   ));
}

[[nodiscard]] inline SystemMicroTime getSystemMicroTime() noexcept
{
   return SystemMicroTime(static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
         std::chrono::system_clock::now().time_since_epoch()
      ).count()
   ));
}


void printTimeStamp(std::ostream& os);
int pollTimeout(const unsigned long long now, const size_t n, ...);


// ###### String copy/cat (strlcpy/strlcat) #################################
#if !defined(HAVE_STRLCPY)
size_t strlcpy(char* dest, const char* src, size_t size);
#endif
#if !defined(HAVE_STRLCAT)
size_t strlcat(char* dest, const char* src, size_t size);
#endif

// Backward compatibility wrappers
inline int safestrcpy(char* dest, const char* src, const size_t size)
{
   return strlcpy(dest, src, size) < size;
}

inline int safestrcat(char* dest, const char* src, const size_t size)
{
   return strlcat(dest, src, size) < size;
}

[[nodiscard]] bool hasSuffix(const std::string& name, const std::string& suffix);
void dissectName(const std::string& name,
                 std::string&       prefix,
                 std::string&       suffix);


union sockaddr_union {
   struct sockaddr     sa;
   struct sockaddr_in  in;
   struct sockaddr_in6 in6;
};

[[nodiscard]] bool checkIPv6();
[[nodiscard]] size_t getSocklen(const struct sockaddr* address);
[[nodiscard]] int addresscmp(const struct sockaddr* a1,
                             const struct sockaddr* a2,
                             const bool             port);
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

[[nodiscard]] const char* getProtocolName(const int protocol);
[[nodiscard]] uint16_t getPort(const struct sockaddr* address);
bool setPort(struct sockaddr* address, uint16_t port);
#if defined(HAVE_SCTP)
bool sendAbort(int sd, sctp_assoc_t assocID = 0);
#endif
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


typedef unsigned long long network_double_t;

[[nodiscard]] network_double_t doubleToNetwork(double value);
[[nodiscard]] double networkToDouble(const network_double_t value);


void installBreakDetector();
void uninstallBreakDetector();
[[nodiscard]] bool breakDetected();
void sendBreak(const bool quiet);


#define RANDOM_CONSTANT         0
#define RANDOM_UNIFORM          1
#define RANDOM_EXPONENTIAL      2
#define RANDOM_PARETO           3
#define RANDOM_NORMAL           4
#define RANDOM_TRUNCATED_NORMAL 5

[[nodiscard]] const char* getRandomGeneratorName(const uint8_t rng);
[[nodiscard]] double getRandomValue(const double* valueArray, const uint8_t rng);
[[nodiscard]] uint8_t random8();
[[nodiscard]] uint16_t random16();
[[nodiscard]] uint64_t random64();
[[nodiscard]] uint32_t random32();
[[nodiscard]] double randomDouble();
[[nodiscard]] double randomExpDouble(const double mean);
[[nodiscard]] double randomNormal(const double mean, const double stddev);
[[nodiscard]] double randomTruncNormal(const double mean, const double stddev);
[[nodiscard]] double randomParetoDouble(const double location, const double shape);

#endif // TOOLS_H
