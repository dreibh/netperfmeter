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

#include "tools.h"

#include <arpa/inet.h>
#include <atomic>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <ext_socket.h>
#include <format>
#include <mutex>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>


// ###### Fallback implementation of strlcpy() ##############################
#if !defined(HAVE_STRLCPY)
size_t strlcpy(char* dest, const char* src, size_t size) noexcept
{
   if(src == nullptr) {
      if((dest != nullptr) && (size > 0)) {
         dest[0] = 0x00;
      }
      return 0;
   }
   const size_t src_len = strlen(src);
   if((dest != nullptr) && (size > 0)) {
      const size_t copy_len = (src_len >= size) ? size - 1 : src_len;
      memcpy(dest, src, copy_len);
      dest[copy_len] = 0x00;
   }
   return src_len;
}
#endif


// ###### Fallback implementation of strlcat() ##############################
#if !defined(HAVE_STRLCAT)
size_t strlcat(char* dest, const char* src, size_t size) noexcept
{
   size_t dest_len = 0;
   if(dest != nullptr) {
      while((dest_len < size) && (dest[dest_len] != 0x00)) {
         dest_len++;
      }
   }
   if(src == nullptr) {
      return dest_len;
   }
   const size_t src_len = strlen(src);
   if((dest == nullptr) || (dest_len >= size)) {
      return size + src_len;
   }
   const size_t copy_len = (src_len >= size - dest_len) ? size - dest_len - 1 : src_len;
   memcpy(dest + dest_len, src, copy_len);
   dest[dest_len + copy_len] = 0x00;
   return dest_len + src_len;
}
#endif


// ###### Print time stamp ##################################################
void printTimeStamp(std::ostream& os)
{
   const std::chrono::system_clock::time_point now     = std::chrono::system_clock::now();
   const auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
   const time_t sec   = std::chrono::system_clock::to_time_t(now);

   struct tm timeinfo;
   localtime_r(&sec, &timeinfo);

   char str[64];
   strftime(str, sizeof(str), "%d-%b-%Y %H:%M:%S", &timeinfo);
   os << str << std::format(".{:04d}: ", static_cast<unsigned int>((totalUs % 1000000) / 100));
}


// ###### Get timeout value for poll() from microtime values ################
int pollTimeout(const unsigned long long now, const size_t n, ...)
{
   va_list va;
   va_start(va, n);
   unsigned long long timeout = ~0ULL;
   for(size_t i = 0; i < n; i++) {
      const unsigned long long t = va_arg(va, unsigned long long);
      timeout = std::min(timeout, t);
   }
   va_end(va);

   if(timeout == ~0ULL) {
      return -1;   // Infinite wait time
   }
   const double delta = static_cast<double>(timeout) - static_cast<double>(now);
   if(delta <= 0.0) {
      return 0;    // Do not wait
   }
   // Return wait time in milliseconds.
   // NOTE: Return ceiling of the value, since 999ULL/1000ULL == 0!
   return static_cast<int>(ceil(delta / 1000.0));
}


// ###### Find first occurrence of character in string ######################
static char* strindex(char* string, const char character)
{
   if(string != nullptr) {
      while(*string != character) {
         if(*string == 0x00) {
            return nullptr;
         }
         string++;
      }
      return string;
   }
   return nullptr;
}


// ###### Find last occurrence of character in string #######################
static char* strrindex(char* string, const char character)
{
   const char* original = string;
   if(original != nullptr) {
      string = &string[strlen(string)];
      while(*string != character) {
         if(string == original) {
            return nullptr;
         }
         string--;
      }
      return string;
   }
   return nullptr;
}


// ###### Check filename for given suffix ###################################
bool hasSuffix(const std::string& name, const std::string& suffix)
{
   return (name.length() >= suffix.length()) &&
          (name.compare(name.length() - suffix.length(), suffix.length(), suffix) == 0);
}


// ###### Dissect file name into prefix and suffix ##########################
void dissectName(const std::string& name,
                 std::string&       prefix,
                 std::string&       suffix)
{
   const size_t slash = name.find_last_of('/');
   size_t dot         = name.find_last_of('.');
   if((dot != std::string::npos) &&
      ((slash == std::string::npos) || (slash < dot))) {
      std::string ext = std::string(name, dot);
      for(char& c : ext) {
         c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      }
      if((ext == ".bz2")  ||
         (ext == ".gz")   ||
         (ext == ".xz")   || (ext == ".lzma") ||
         (ext == ".zst")  || (ext == ".zstd")) {
         const size_t dot2 = name.find_last_of('.', dot - 1);
         if((dot2 != std::string::npos) &&
            ((slash == std::string::npos) || (slash < dot2))) {
            dot = dot2;
         }
      }
      suffix = std::string(name, dot);
      prefix = std::string(name, 0, dot);
   }
   else {
      prefix = name;
      suffix = "";
   }
}


// ###### Check for support of IPv6 #########################################
bool checkIPv6()
{
   int sd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
   if(sd >= 0) {
      close(sd);
      return true;
   }
   return false;
}


// ###### Get socklen for given address #####################################
size_t getSocklen(const struct sockaddr* address)
{
   switch(address->sa_family) {
      case AF_INET:
         return sizeof(struct sockaddr_in);
      case AF_INET6:
         return sizeof(struct sockaddr_in6);
      default:
         return sizeof(struct sockaddr);
   }
}


// ###### Compare addresses #################################################
int addresscmp(const struct sockaddr* a1, const struct sockaddr* a2, const bool port)
{
   uint16_t p1, p2;
   uint32_t x1[4];
   uint32_t x2[4];

   if(((a1->sa_family == AF_INET) || (a1->sa_family == AF_INET6)) &&
      ((a2->sa_family == AF_INET) || (a2->sa_family == AF_INET6))) {
      if(a1->sa_family == AF_INET6) {
         memcpy(x1, &((const struct sockaddr_in6*)a1)->sin6_addr, 16);
      }
      else {
         x1[0] = 0;
         x1[1] = 0;
         x1[2] = htobe32(0xffff);
         x1[3] = *reinterpret_cast<const uint32_t*>(&((const struct sockaddr_in*)a1)->sin_addr);
      }

      if(a2->sa_family == AF_INET6) {
         memcpy(x2, &((const struct sockaddr_in6*)a2)->sin6_addr, 16);
      }
      else {
         x2[0] = 0;
         x2[1] = 0;
         x2[2] = htobe32(0xffff);
         x2[3] = *reinterpret_cast<const uint32_t*>(&((const struct sockaddr_in*)a2)->sin_addr);
      }

      const int result = memcmp(x1, x2, 16);
      if(result != 0) {
         return result;
      }

      if(port) {
         p1 = getPort(a1);
         p2 = getPort(a2);
         if(p1 < p2) {
            return -1;
         }
         else if(p1 > p2) {
            return 1;
         }
      }
      return 0;
   }
   return 0;
}


// ###### Convert address to string #########################################
bool address2string(const struct sockaddr* address,
                    char*                  buffer,
                    const size_t           length,
                    const bool             port,
                    const bool             hideScope)
{
   char str[128];
   char scope[IFNAMSIZ + 16];

   switch(address->sa_family) {
      case AF_INET: {
         const auto* ipv4 = reinterpret_cast<const struct sockaddr_in*>(address);
         const std::string formatted = port ?
            std::format("{}:{}", inet_ntoa(ipv4->sin_addr), be16toh(ipv4->sin_port)) :
            std::string(inet_ntoa(ipv4->sin_addr));
         strlcpy(buffer, formatted.c_str(), length);
         return true;
      }

      case AF_INET6: {
         const auto* ipv6 = reinterpret_cast<const struct sockaddr_in6*>(address);
         if(!hideScope && (IN6_IS_ADDR_LINKLOCAL(&ipv6->sin6_addr) || IN6_IS_ADDR_MC_LINKLOCAL(&ipv6->sin6_addr))) {
            char ifnamebuffer[IFNAMSIZ];
            const char* ifname = if_indextoname(ipv6->sin6_scope_id, ifnamebuffer);
            if(ifname == nullptr) {
               return false;
            }
            snprintf(scope, sizeof(scope), "%%%s", ifname);
         }
         else {
            scope[0] = 0x00;
         }
         if(inet_ntop(AF_INET6, &ipv6->sin6_addr, str, sizeof(str)) != nullptr) {
            const std::string formatted = port ?
               std::format("[{}{}]:{}", str, scope, be16toh(ipv6->sin6_port)) :
               std::format("{}{}", str, scope);
            strlcpy(buffer, formatted.c_str(), length);
            return true;
         }
         break;
      }

      case AF_UNSPEC:
         // TRANSLATORS: Representation for an uninitialized or unspecified network address.
         strlcpy(buffer, gettext("(unspecified)"), length);
         return true;
   }
   return false;
}


// ###### Convert string to address #########################################
bool string2address(const char*           string,
                    union sockaddr_union* address,
                    const bool            readPort)
{
   char  host[128];
   char  port[128];
   int   portNumber = 0;
   char* p1;

   if((string == nullptr) || (strlen(string) >= sizeof(host))) {
      return false;
   }
   strlcpy(host, string, sizeof(host));
   strlcpy(port, "0", sizeof(port));

   if(string[0] == '[') {
      p1 = strindex(host, ']');
      if(p1 == nullptr) {
         return false;
      }
      if(p1 <= host + 1) {
         return false;
      }
      if((p1[1] == ':') || (p1[1] == '!')) {
         if(p1[2] == 0x00) {
            return false;
         }
         strlcpy(port, &p1[2], sizeof(port));
      }
      else if(p1[1] != 0x00) {
         return false;
      }
      const size_t hostLen = static_cast<size_t>(p1 - host - 1);
      memmove(host, &host[1], hostLen);
      host[hostLen] = 0x00;
   }
   else if(readPort) {
      unsigned int colons = 0;
      for(size_t i = 0; i < strlen(host); i++) {
         if(host[i] == ':') {
            colons++;
         }
      }
      if(colons == 1) {
         p1 = strrindex(host, ':');
         if(p1 == nullptr) {
            p1 = strrindex(host, '!');
         }
         if(p1 != nullptr) {
            p1[0] = 0x00;
            if(p1[1] == 0x00) {
               return false;
            }
            strlcpy(port, &p1[1], sizeof(port));
         }
      }
   }

   if((sscanf(port, "%d", &portNumber) != 1) || (portNumber < 0) || (portNumber > 65535)) {
      return false;
   }

   struct addrinfo hints;
   struct addrinfo* res = nullptr;
   memset(&hints, 0, sizeof(hints));
   hints.ai_socktype = SOCK_DGRAM;
   hints.ai_family   = AF_UNSPEC;

   bool isNumeric = true;
   bool isIPv6    = false;
   const size_t hostLength = strlen(host);

   for(size_t i = 0; i < hostLength; i++) {
      if(host[i] == ':') {
         isIPv6 = true;
         break;
      }
   }
   if(!isIPv6) {
      for(size_t i = 0; i < hostLength; i++) {
         if(!isdigit(static_cast<unsigned char>(host[i])) && (host[i] != '.')) {
            isNumeric = false;
            break;
         }
      }
   }
   if(isNumeric) {
      hints.ai_flags = AI_NUMERICHOST;
   }

   if(getaddrinfo(host, nullptr, &hints, &res) != 0) {
      return false;
   }

   memset(address, 0, sizeof(union sockaddr_union));
   memcpy(address, res->ai_addr, res->ai_addrlen);

   auto* ipv4 = reinterpret_cast<struct sockaddr_in*>(address);
   auto* ipv6 = reinterpret_cast<struct sockaddr_in6*>(address);

   switch(ipv4->sin_family) {
      case AF_INET:
         ipv4->sin_port = htobe16(static_cast<uint16_t>(portNumber));
#if defined(HAVE_SIN_LEN)
         ipv4->sin_len  = sizeof(struct sockaddr_in);
#endif
         break;
      case AF_INET6:
         ipv6->sin6_port = htobe16(static_cast<uint16_t>(portNumber));
#if defined(HAVE_SIN6_LEN)
         ipv6->sin6_len  = sizeof(struct sockaddr_in6);
#endif
         break;
      default:
         freeaddrinfo(res);
         return false;
   }

   freeaddrinfo(res);
   return true;
}


// ###### Print address #####################################################
void printAddress(std::ostream&          os,
                  const struct sockaddr* address,
                  const bool             port,
                  const bool             hideScope)
{
   char str[160];
   if((address != nullptr) && address2string(address, str, sizeof(str), port, hideScope)) {
      os << str;
   }
   else {
      // TRANSLATORS: Fallback text when formatting a network address fails.
      os << gettext("(invalid!)");
   }
}


// ###### Format socket address as std::string ##############################
std::string formatAddress(const struct sockaddr* address,
                          const bool             port,
                          const bool             hideScope)
{
   if(address == nullptr) {
      // TRANSLATORS: Representation for an uninitialized or unspecified network address.
      return gettext("(unspecified)");
   }
   char str[160];
   if(address2string(address, str, sizeof(str), port, hideScope)) {
      return std::string(str);
   }
   // TRANSLATORS: Fallback text when formatting a network address fails.
   return gettext("(invalid!)");
}


// ###### Resolve peer endpoint string (primary -> fallback -> getpeername) #
std::string resolvePeerAddress(const int              sd,
                               const struct sockaddr* primaryAddr,
                               const struct sockaddr* fallbackAddr)
{
   char str[160];
   if((primaryAddr != nullptr) &&
      ((primaryAddr->sa_family == AF_INET) || (primaryAddr->sa_family == AF_INET6)) &&
      address2string(primaryAddr, str, sizeof(str), true, false)) {
      return std::string(str);
   }
   if((fallbackAddr != nullptr) &&
      ((fallbackAddr->sa_family == AF_INET) || (fallbackAddr->sa_family == AF_INET6)) &&
      address2string(fallbackAddr, str, sizeof(str), true, false)) {
      return std::string(str);
   }
   if(sd >= 0) {
      sockaddr_union peer;
      socklen_t      peerLen = sizeof(peer);
      memset(&peer, 0, sizeof(peer));
      if((ext_getpeername(sd, &peer.sa, &peerLen) == 0) &&
         (peerLen > 0) &&
         ((peer.sa.sa_family == AF_INET) || (peer.sa.sa_family == AF_INET6)) &&
         address2string(&peer.sa, str, sizeof(str), true, false)) {
         return std::string(str);
      }
   }
   // TRANSLATORS: Fallback text when a remote peer socket address cannot be determined.
   return gettext("(unknown)");
}


// ###### Get protocol name #################################################
const char* getProtocolName(const int protocol)
{
   switch(protocol) {
      case IPPROTO_SCTP:  return "SCTP";
      case IPPROTO_TCP:   return "TCP";
      case IPPROTO_UDP:   return "UDP";
#if defined(HAVE_MPTCP)
      case IPPROTO_MPTCP: return "MPTCP";
#endif
#if defined(HAVE_DCCP)
      case IPPROTO_DCCP:  return "DCCP";
#endif
#if defined(HAVE_QUIC)
      case IPPROTO_QUIC:  return "QUIC";
#endif
      default:
         // TRANSLATORS: Fallback text when an unknown transport protocol number is passed.
         return gettext("unknown?!");
   }
}


// ###### Parse Traffic Class / DSCP specification ##########################
bool parseTrafficClass(const char* str,
                       uint8_t&    trafficClass,
                       bool&       ecnBitsCleared)
{
   ecnBitsCleared = false;
   if((str == nullptr) || (str[0] == '\0')) {
      return false;
   }

   struct DscpEntry
   {
      const char* Name;
      uint8_t     Value;
   };
   static constexpr DscpEntry dscpTable[] = {
      { "BE",   0x00 }, { "DF",   0x00 }, { "CS0",  0x00 },
      { "LE",   0x04 },
      { "CS1",  0x20 }, { "AF11", 0x28 }, { "AF12", 0x30 }, { "AF13", 0x38 },
      { "CS2",  0x40 }, { "AF21", 0x48 }, { "AF22", 0x50 }, { "AF23", 0x58 },
      { "CS3",  0x60 }, { "AF31", 0x68 }, { "AF32", 0x70 }, { "AF33", 0x78 },
      { "CS4",  0x80 }, { "AF41", 0x88 }, { "AF42", 0x90 }, { "AF43", 0x98 },
      { "CS5",  0xa0 }, { "VA",   0xb0 }, { "EF",   0xb8 },
      { "CS6",  0xc0 },
      { "CS7",  0xe0 }
   };

   std::string upper(str);
   for(char& c : upper) {
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
   }
   for(const auto& entry : dscpTable) {
      if(upper == entry.Name) {
         trafficClass = entry.Value;
         return true;
      }
   }

   // Numeric value: 0x00..0xff (hex) or 0..255 (decimal)
   if((str[0] == '-') || (str[0] == '+') || std::isspace(static_cast<unsigned char>(str[0]))) {
      return false;
   }
   const int base = ((str[0] == '0') && ((str[1] == 'x') || (str[1] == 'X'))) ? 16 : 10;
   if((base == 16) && (str[2] == '\0')) {
      return false;
   }
   char* endPtr = nullptr;
   errno = 0;
   const unsigned long value = std::strtoul(str, &endPtr, base);
   if((errno != 0) || (endPtr == str) || (*endPtr != '\0') || (value > 0xffUL)) {
      return false;
   }
   if((value & 0x03UL) != 0) {
      ecnBitsCleared = true;
   }
   trafficClass = static_cast<uint8_t>(value & 0xfcUL);
   return true;
}


// ###### Get standard DSCP name for Traffic Class byte #####################
const char* getTrafficClassName(const uint8_t trafficClass)
{
   switch(trafficClass & 0xfcU) {
      case 0x00: return "BE";
      case 0x04: return "LE";
      case 0x20: return "CS1";
      case 0x28: return "AF11";
      case 0x30: return "AF12";
      case 0x38: return "AF13";
      case 0x40: return "CS2";
      case 0x48: return "AF21";
      case 0x50: return "AF22";
      case 0x58: return "AF23";
      case 0x60: return "CS3";
      case 0x68: return "AF31";
      case 0x70: return "AF32";
      case 0x78: return "AF33";
      case 0x80: return "CS4";
      case 0x88: return "AF41";
      case 0x90: return "AF42";
      case 0x98: return "AF43";
      case 0xa0: return "CS5";
      case 0xb0: return "VA";
      case 0xb8: return "EF";
      case 0xc0: return "CS6";
      case 0xe0: return "CS7";
      default:   return nullptr;
   }
}


// ###### Format Traffic Class for display ##################################
std::string formatTrafficClass(const std::optional<uint8_t>& trafficClass)
{
   if(!trafficClass.has_value()) {
      // TRANSLATORS: Value label indicating OS/protocol default setting.
      return gettext("default");
   }
   const uint8_t tc   = static_cast<uint8_t>(*trafficClass & 0xfcU);
   const char*   name = getTrafficClassName(tc);
   if(name != nullptr) {
      return std::format("0x{:02x} ({})", static_cast<unsigned int>(tc), name);
   }
   return std::format("0x{:02x}", static_cast<unsigned int>(tc));
}


// ###### Format Traffic Class value for config file ########################
std::string formatTrafficClassConfig(const std::optional<uint8_t>& trafficClass)
{
   if(!trafficClass.has_value()) {
      return "default";
   }
   return std::format("0x{:02x}", static_cast<unsigned int>(*trafficClass & 0xfcU));
}


// ###### Format Traffic Class DSCP name for config file ####################
std::string formatTrafficClassNameConfig(const std::optional<uint8_t>& trafficClass)
{
   if(!trafficClass.has_value()) {
      return "default";
   }
   const uint8_t tc   = static_cast<uint8_t>(*trafficClass & 0xfcU);
   const char*   name = getTrafficClassName(tc);
   if(name != nullptr) {
      return name;
   }
   return std::format("0x{:02x}", static_cast<unsigned int>(tc));
}


// ###### Parse IPv6 Flow Label specification ###############################
bool parseFlowLabel(const char*       str,
                    FlowLabelSetting& flowLabel)
{
   if((str == nullptr) || (str[0] == '\0')) {
      return false;
   }
   std::string lower(str);
   for(char& c : lower) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
   }
   if(lower == "default") {
      flowLabel.Mode  = FlowLabelMode::Default;
      flowLabel.Label = 0;
      return true;
   }
   if(lower == "auto") {
      flowLabel.Mode  = FlowLabelMode::Auto;
      flowLabel.Label = 0;
      return true;
   }
   if(lower == "reflect") {
      flowLabel.Mode  = FlowLabelMode::Reflect;
      flowLabel.Label = 0;
      return true;
   }

   if((str[0] == '-') || (str[0] == '+') || std::isspace(static_cast<unsigned char>(str[0]))) {
      return false;
   }
   const int base = ((str[0] == '0') && ((str[1] == 'x') || (str[1] == 'X'))) ? 16 : 10;
   if((base == 16) && (str[2] == '\0')) {
      return false;
   }
   char* endPtr = nullptr;
   errno = 0;
   const unsigned long value = std::strtoul(str, &endPtr, base);
   if((errno != 0) || (endPtr == str) || (*endPtr != '\0') || (value > 0x000fffffUL)) {
      return false;
   }
   flowLabel.Mode  = FlowLabelMode::Explicit;
   flowLabel.Label = static_cast<uint32_t>(value);
   return true;
}


// ###### Format IPv6 Flow Label for display ################################
std::string formatFlowLabel(const FlowLabelSetting& flowLabel)
{
   switch(flowLabel.Mode) {
      case FlowLabelMode::Default:
         // TRANSLATORS: Value label indicating OS/protocol default setting.
         return gettext("default");
      case FlowLabelMode::Auto:
         return "auto";
      case FlowLabelMode::Reflect:
         return "reflect";
      case FlowLabelMode::Explicit:
         return std::format("0x{:05x}", flowLabel.Label & 0x000fffffU);
      default:
         // TRANSLATORS: Fallback text when an unknown value is encountered.
         return gettext("unknown?!");
   }
}


// ###### Format IPv6 Flow Label for config file ############################
std::string formatFlowLabelConfig(const FlowLabelSetting& flowLabel)
{
   switch(flowLabel.Mode) {
      case FlowLabelMode::Default:  return "default";
      case FlowLabelMode::Auto:     return "auto";
      case FlowLabelMode::Reflect:  return "reflect";
      case FlowLabelMode::Explicit: return std::format("0x{:05x}", flowLabel.Label & 0x000fffffU);
      default:                      return "default";
   }
}


// ###### Get port ##########################################################
uint16_t getPort(const struct sockaddr* address)
{
   if(address != nullptr) {
      switch(address->sa_family) {
         case AF_INET:
            return be16toh(reinterpret_cast<const struct sockaddr_in*>(address)->sin_port);
         case AF_INET6:
            return be16toh(reinterpret_cast<const struct sockaddr_in6*>(address)->sin6_port);
         default:
            return 0;
      }
   }
   return 0;
}


// ###### Set port ##########################################################
bool setPort(struct sockaddr* address, uint16_t port)
{
   if(address != nullptr) {
      switch(address->sa_family) {
         case AF_INET:
            reinterpret_cast<struct sockaddr_in*>(address)->sin_port = htobe16(port);
            return true;
         case AF_INET6:
            reinterpret_cast<struct sockaddr_in6*>(address)->sin6_port = htobe16(port);
            return true;
      }
   }
   return false;
}


// ###### Resolve socket address family #####################################
static int resolveSocketFamily(const int             family,
                               const int             protocol,
                               const unsigned int    localAddresses,
                               const sockaddr_union* localAddressArray)
{
   int socketFamily = family;
   if(socketFamily == AF_UNSPEC) {
      socketFamily = checkIPv6() ? AF_INET6 : AF_INET;
      if((localAddresses >= 1) && (localAddressArray != nullptr)) {
         if(protocol != IPPROTO_SCTP) {
            if(localAddressArray[0].sa.sa_family == AF_INET) {
               socketFamily = AF_INET;
            }
         }
         else {
            bool hasIPv6 = false;
            for(unsigned int i = 0; i < localAddresses; i++) {
               if(localAddressArray[i].sa.sa_family == AF_INET6) {
                  hasIPv6 = true;
                  break;
               }
            }
            if(!hasIPv6) {
               socketFamily = AF_INET;
            }
         }
      }
   }
   return socketFamily;
}


// ###### Create socket #####################################################
int createSocket(const int             family,
                 const int             type,
                 const int             protocol,
                 const unsigned int    localAddresses,
                 const sockaddr_union* localAddressArray)
{
   const int socketFamily = resolveSocketFamily(family, protocol,
                                                localAddresses, localAddressArray);
   return ext_socket(socketFamily, type, protocol);
}


// ###### Bind socket #######################################################
int bindSocket(const int             sd,
               const int             family,
               const int             type,
               const int             protocol,
               const uint16_t        localPort,
               const unsigned int    localAddresses,
               const sockaddr_union* localAddressArray,
               const bool            listenMode,
               const bool            bindV6Only)
{
   (void)type;

   unsigned int localAddressCount = localAddresses;
   const int    socketFamily      = resolveSocketFamily(family, protocol,
                                                        localAddresses, localAddressArray);

   sockaddr_union anyAddress;
   memset(&anyAddress, 0, sizeof(anyAddress));
   if(socketFamily == AF_INET6) {
      anyAddress.in6.sin6_family = AF_INET6;
      anyAddress.in6.sin6_port   = htobe16(localPort);
#if defined(HAVE_SIN6_LEN)
      anyAddress.in6.sin6_len    = sizeof(struct sockaddr_in6);
#endif
   }
   else {
      anyAddress.in.sin_family = AF_INET;
      anyAddress.in.sin_port   = htobe16(localPort);
#if defined(HAVE_SIN_LEN)
      anyAddress.in.sin_len    = sizeof(struct sockaddr_in);
#endif
   }

#if defined(HAVE_MPTCP)
   if((protocol == IPPROTO_MPTCP) && (localAddresses > 1)) {
      // TRANSLATORS: Warning about multipath TCP limitations when multiple local addresses are provided.
      std::cerr << gettext("WARNING: Currently, MPTCP does not support TCP_MULTIPATH_ADD. Binding to ANY address instead ...") << "\n";
      localAddressCount = 0;
   }
#endif

#if !defined(__OpenBSD__) && defined(IPV6_V6ONLY)
   if(socketFamily == AF_INET6) {
      const int on = bindV6Only ? 1 : 0;
      if(ext_setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0) {
         return -2;
      }
   }
#endif

   if(localAddressCount == 0) {
      if(ext_bind(sd, &anyAddress.sa, getSocklen(&anyAddress.sa)) != 0) {
         return -3;
      }
   }
   else {
#if defined(HAVE_SCTP)
      if(protocol == IPPROTO_SCTP) {
         // ====== SCTP bind: bind to specified addresses ===================
         char         buffer[localAddressCount * sizeof(sockaddr_union)];
         char*        ptr          = &buffer[0];
         size_t       firstAddrLen = 0;
         unsigned int added        = 0;

         // Place addresses matching socketFamily first so ext_bind() on an
         // AF_INET6 socket always receives an AF_INET6 primary address:
         for(int pass = 0; pass < 2; pass++) {
            for(unsigned int i = 0; i < localAddressCount; i++) {
               const bool matchesFamily = (localAddressArray[i].sa.sa_family == socketFamily);
               if((pass == 0) != matchesFamily) {
                  continue;
               }
               size_t addrLen = 0;
               if(localAddressArray[i].sa.sa_family == AF_INET) {
#if defined(__sun) || defined(__sun__)
                  if(socketFamily == AF_INET6) {
                     sockaddr_in6* mapped = (sockaddr_in6*)ptr;
                     memset(mapped, 0, sizeof(sockaddr_in6));
                     mapped->sin6_family = AF_INET6;
                     mapped->sin6_port   = htobe16(localPort);
#if defined(HAVE_SIN6_LEN)
                     mapped->sin6_len    = sizeof(struct sockaddr_in6);
#endif
                     mapped->sin6_addr.s6_addr[10] = 0xff;
                     mapped->sin6_addr.s6_addr[11] = 0xff;
                     memcpy(&mapped->sin6_addr.s6_addr[12], &localAddressArray[i].in.sin_addr, sizeof(struct in_addr));
                     addrLen = sizeof(sockaddr_in6);
                  }
                  else {
#endif
                     memcpy(ptr, (void*)&localAddressArray[i].in, sizeof(sockaddr_in));
                     ((sockaddr_in*)ptr)->sin_port = htobe16(localPort);
                     addrLen = sizeof(sockaddr_in);
#if defined(__sun) || defined(__sun__)
                  }
#endif
               }
               else if(localAddressArray[i].sa.sa_family == AF_INET6) {
                  memcpy(ptr, (void*)&localAddressArray[i].in6, sizeof(sockaddr_in6));
                  ((sockaddr_in6*)ptr)->sin6_port = htobe16(localPort);
                  addrLen = sizeof(sockaddr_in6);
               }
               else {
                  assert(false);
               }
               if(added == 0) {
                  firstAddrLen = addrLen;
               }
               ptr += addrLen;
               added++;
            }
         }
         assert(added == localAddressCount);

         if(ext_bind(sd, (const sockaddr*)&buffer[0], firstAddrLen) != 0) {
            return -3;
         }
         if(localAddressCount > 1) {
            if(localPort == 0) {
               sockaddr_union boundAddr;
               socklen_t      boundAddrLen = sizeof(boundAddr);
               if(ext_getsockname(sd, &boundAddr.sa, &boundAddrLen) == 0) {
                  const uint16_t assignedPort = getPort(&boundAddr.sa);
                  char* addPtr = &buffer[0] + firstAddrLen;
                  for(unsigned int i = 1; i < localAddressCount; i++) {
                     setPort((sockaddr*)addPtr, assignedPort);
                     addPtr += (((const sockaddr*)addPtr)->sa_family == AF_INET6) ?
                                  sizeof(sockaddr_in6) : sizeof(sockaddr_in);
                  }
               }
            }
            if(sctp_bindx(sd, (sockaddr*)(&buffer[0] + firstAddrLen),
                          (int)(localAddressCount - 1),
                          SCTP_BINDX_ADD_ADDR) != 0) {
               return -3;
            }
         }
      }
      else {
#endif
         sockaddr_union localAddress;
         memset(&localAddress, 0, sizeof(localAddress));
         if(localAddressArray[0].sa.sa_family == AF_INET) {
            memcpy(&localAddress, &localAddressArray[0].in, sizeof(sockaddr_in));
            localAddress.in.sin_port = htobe16(localPort);
         }
         else if(localAddressArray[0].sa.sa_family == AF_INET6) {
            memcpy(&localAddress, &localAddressArray[0].in6, sizeof(sockaddr_in6));
            localAddress.in6.sin6_port = htobe16(localPort);
         }
         else {
            assert(false);
         }

         if(ext_bind(sd, &localAddress.sa, getSocklen(&localAddress.sa)) != 0) {
            return -3;
         }
#if defined(HAVE_SCTP)
      }
#endif
   }

   if(listenMode) {
      ext_listen(sd, 10);
   }
   return sd;
}


#if defined(HAVE_SCTP)
// ###### Send SCTP ABORT ###################################################
bool sendAbort(int sd, sctp_assoc_t assocID)
{
   sctp_sndrcvinfo sinfo;
   memset(&sinfo, 0, sizeof(sinfo));
   sinfo.sinfo_assoc_id = assocID;
   sinfo.sinfo_flags    = SCTP_ABORT;

   return sctp_send(sd, nullptr, 0, &sinfo, 0) >= 0;
}
#endif


// ###### IEEE Floating Point Conversion ####################################
union DoubleIntUnion
{
   double             Double;
   unsigned long long Integer;
};


// ###### Convert double to network format (64-bit integer) #################
network_double_t doubleToNetwork(const double d)
{
   union DoubleIntUnion valueUnion;
   valueUnion.Double = d;
   return htobe64(valueUnion.Integer);
}


// ###### Convert network format (64-bit integer) to double #################
double networkToDouble(network_double_t value)
{
   union DoubleIntUnion valueUnion;
   valueUnion.Integer = be64toh(value);
   return valueUnion.Double;
}


// ###### Break and Termination Handling ####################################
static std::atomic<bool>     DetectedBreak{false};
static bool                  PrintedBreak{false};
static std::atomic<bool>     Quiet{false};
static std::atomic<pid_t>    MainThreadPID{0};
static std::atomic<bool>     PrintedKill{false};
static std::atomic<uint64_t> LastDetectionUs{static_cast<uint64_t>(-1)};


// ###### Break detector signal handler #####################################
void breakDetector(int)
{
   DetectedBreak.store(true, std::memory_order_release);
   if(!PrintedKill.load(std::memory_order_acquire)) {
      struct timespec ts;
      if(clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
         const uint64_t nowUs = (static_cast<uint64_t>(ts.tv_sec) * 1000000ULL) +
                                (static_cast<uint64_t>(ts.tv_nsec) / 1000ULL);
         uint64_t prevUs = LastDetectionUs.load(std::memory_order_acquire);
         if(prevUs == static_cast<uint64_t>(-1)) {
            LastDetectionUs.store(nowUs, std::memory_order_release);
         }
         else if((nowUs - prevUs) >= 2000000ULL) {
            if(!PrintedKill.exchange(true, std::memory_order_acq_rel)) {
               static constexpr char killMsg[] = "\x1b[0m\n*** Kill ***\n\n";
               const ssize_t written = write(STDERR_FILENO, killMsg, sizeof(killMsg) - 1);
               (void)written;
               kill(MainThreadPID.load(std::memory_order_acquire), SIGKILL);
            }
         }
      }
   }
}


// ###### Install break detector ############################################
void installBreakDetector()
{
   DetectedBreak.store(false, std::memory_order_release);
   PrintedBreak = false;
   Quiet.store(false, std::memory_order_release);
   MainThreadPID.store(getpid(), std::memory_order_release);
   PrintedKill.store(false, std::memory_order_release);
   LastDetectionUs.store(static_cast<uint64_t>(-1), std::memory_order_release);
   signal(SIGINT, &breakDetector);
}


// ###### Uninstall break detector ##########################################
void uninstallBreakDetector()
{
   signal(SIGINT, SIG_DFL);
   PrintedKill.store(false, std::memory_order_release);
   LastDetectionUs.store(static_cast<uint64_t>(-1), std::memory_order_release);
   Quiet.store(false, std::memory_order_release);
}


// ###### Check whether break has been detected #############################
bool breakDetected()
{
   const bool broken = DetectedBreak.load(std::memory_order_acquire);
   if(broken && !PrintedBreak) {
      if(!Quiet.load(std::memory_order_acquire)) {
         std::cerr << "\x1b[0m\n"
                   // TRANSLATORS: Signal notification printed on the first SIGINT. {} is the signal number (SIGINT=2).
                   << formatText(gettext("*** Break ***    Signal #{}"), SIGINT)
                   << "\n\n";
      }
      PrintedBreak = true;
   }
   return broken;
}


// ###### Send break signal to main thread ##################################
void sendBreak(const bool quiet)
{
   Quiet.store(quiet, std::memory_order_release);
   kill(MainThreadPID.load(std::memory_order_acquire), SIGINT);
}


// ###### Get random value for given distribution ###########################
double getRandomValue(const double* valueArray, const uint8_t rng)
{
   switch(rng) {
      case RANDOM_CONSTANT:         return valueArray[0];
      case RANDOM_UNIFORM:          return valueArray[0] + (randomDouble() * (valueArray[1] - valueArray[0]));
      case RANDOM_EXPONENTIAL:      return randomExpDouble(valueArray[0]);
      case RANDOM_PARETO:           return randomParetoDouble(valueArray[0], valueArray[1]);
      case RANDOM_NORMAL:           return randomNormal(valueArray[0], valueArray[1]);
      case RANDOM_TRUNCATED_NORMAL: return randomTruncNormal(valueArray[0], valueArray[1]);
      default:                      abort();
   }
}


// ###### Get name of random number generator ###############################
const char* getRandomGeneratorName(const uint8_t rng)
{
   switch(rng) {
      case RANDOM_CONSTANT:         return "constant";
      case RANDOM_UNIFORM:          return "uniform";
      case RANDOM_EXPONENTIAL:      return "exponential";
      case RANDOM_PARETO:           return "pareto";
      case RANDOM_NORMAL:           return "normal";
      case RANDOM_TRUNCATED_NORMAL: return "truncated normal";
      default:                      return "(invalid!)";
   }
}

static std::mutex RandomDeviceMutex;
static FILE*      RandomDevice      = nullptr;
static bool       RandomTriedDevice = false;
static bool       RandomClibSeeded  = false;


// ###### Generate 8-bit random integer #####################################
uint8_t random8()
{
   return static_cast<uint8_t>(random32());
}


// ###### Generate 16-bit random integer ####################################
uint16_t random16()
{
   return static_cast<uint16_t>(random32());
}


// ###### Generate 64-bit random integer ####################################
uint64_t random64()
{
   return (static_cast<uint64_t>(random32()) << 32) | random32();
}


// ###### Generate 32-bit random integer ####################################
uint32_t random32()
{
   uint32_t number;
   std::lock_guard<std::mutex> guard(RandomDeviceMutex);
   if(!RandomTriedDevice) {
      RandomTriedDevice = true;
      RandomDevice      = fopen("/dev/urandom", "r");
      if(RandomDevice != nullptr) {
         if(fread(&number, sizeof(number), 1, RandomDevice) == 1) {
            srandom(number);
            RandomClibSeeded = true;
            return number;
         }
         fclose(RandomDevice);
         RandomDevice = nullptr;
      }
   }
   else if(RandomDevice != nullptr) {
      if(fread(&number, sizeof(number), 1, RandomDevice) == 1) {
         return number;
      }
      fclose(RandomDevice);
      RandomDevice = nullptr;
   }

   if(!RandomClibSeeded) {
      srandom(static_cast<unsigned int>(getSystemMicroTime().value & 0xffffffff));
      RandomClibSeeded = true;
   }
   const uint16_t a = random() & 0xffff;
   const uint16_t b = random() & 0xffff;
   return (static_cast<uint32_t>(a) << 16) | b;
}


// ###### Generate random double in [0.0, 1.0) ##############################
double randomDouble()
{
   return static_cast<double>(random32()) / static_cast<double>(0x100000000ULL);
}


// ###### Generate negative exponential random double #######################
double randomExpDouble(const double mean)
{
   // randomDouble() returns value in [0.0, 1.0), so 1.0 - randomDouble() avoids log(0.0):
   return -mean * log(1.0 - randomDouble());
}


// ###### Generate normal distributed random double #########################
double randomNormal(const double mean, const double stddev)
{
   const double u = 1.0 - randomDouble();
   const double v = randomDouble();
   return mean + stddev * sqrt(-2.0 * log(u)) * cos(M_PI * 2.0 * v);
}


// ###### Generate truncated normal distributed random double ###############
double randomTruncNormal(const double mean, const double stddev)
{
   double result;
   do {
      result = randomNormal(mean, stddev);
   } while(result < 0.0);
   return result;
}


// ###### Generate Pareto distributed random double #########################
double randomParetoDouble(const double location, const double shape)
{
   assert(shape > 0.0);
   double r = randomDouble();
   while((r <= 0.0) || (r >= 1.0)) {
      r = randomDouble();
   }
   return location / pow(r, 1.0 / shape);
}
