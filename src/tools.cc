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
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <ext_socket.h>
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
size_t strlcpy(char* dest, const char* src, size_t size)
{
   const size_t src_len = strlen(src);
   if(size > 0) {
      const size_t copy_len = (src_len >= size) ? size - 1 : src_len;
      memcpy(dest, src, copy_len);
      dest[copy_len] = 0x00;
   }
   return src_len;
}
#endif


// ###### Fallback implementation of strlcat() ##############################
#if !defined(HAVE_STRLCAT)
size_t strlcat(char* dest, const char* src, size_t size)
{
   const size_t dest_len = strlen(dest);
   const size_t src_len  = strlen(src);
   if(dest_len >= size) {
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
   os << str << formatText(".{:04d}: ", static_cast<unsigned int>((totalUs % 1000000) / 100));
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
   const double delta = (double)timeout - (double)now;
   if(delta <= 0.0) {
      return 0;    // Do not wait
   }
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
      if(std::string(name, dot) == ".bz2") {
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
   char ifnamebuffer[IFNAMSIZ];

   switch(address->sa_family) {
      case AF_INET: {
         const auto* ipv4 = reinterpret_cast<const struct sockaddr_in*>(address);
         const std::string formatted = port ?
            formatText("{}:{}", inet_ntoa(ipv4->sin_addr), be16toh(ipv4->sin_port)) :
            std::string(inet_ntoa(ipv4->sin_addr));
         strlcpy(buffer, formatted.c_str(), length);
         return true;
      }

      case AF_INET6: {
         const auto* ipv6 = reinterpret_cast<const struct sockaddr_in6*>(address);
         if(!hideScope && (IN6_IS_ADDR_LINKLOCAL(&ipv6->sin6_addr) || IN6_IS_ADDR_MC_LINKLOCAL(&ipv6->sin6_addr))) {
            const char* ifname = if_indextoname(ipv6->sin6_scope_id, ifnamebuffer);
            if(ifname == nullptr) {
               strlcpy(ifnamebuffer, "(BAD!)", sizeof(ifnamebuffer));
               return false;
            }
            snprintf(scope, sizeof(scope), "%%%s", ifname);
         }
         else {
            scope[0] = 0x00;
         }
         if(inet_ntop(AF_INET6, &ipv6->sin6_addr, str, sizeof(str)) != nullptr) {
            const std::string formatted = port ?
               formatText("[{}{}]:{}", str, scope, be16toh(ipv6->sin6_port)) :
               formatText("{}{}", str, scope);
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
   char host[128];
   char port[128];
   int  portNumber = 0;
   char* p1;

   if(strlen(string) >= sizeof(host)) {
      return false;
   }
   strlcpy(host, string, sizeof(host));
   strlcpy(port, "0", sizeof(port));

   if(string[0] == '[') {
      p1 = strindex(host, ']');
      if(p1 != nullptr) {
         if((p1[1] == ':') || (p1[1] == '!')) {
            strlcpy(port, &p1[2], sizeof(port));
         }
         memmove(host, &host[1], static_cast<size_t>(p1 - host - 1));
         host[p1 - host - 1] = 0x00;
      }
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
   if(address2string(address, str, sizeof(str), port, hideScope)) {
      os << str;
   }
   else {
      // TRANSLATORS: Fallback text when formatting a network address fails.
      os << gettext("(invalid!)");
   }
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


// ###### Create socket #####################################################
int createSocket(const int             family,
                 const int             type,
                 const int             protocol,
                 const unsigned int    localAddresses,
                 const sockaddr_union* localAddressArray)
{
   int socketFamily = family;
   if(socketFamily == AF_UNSPEC) {
      socketFamily = checkIPv6() ? AF_INET6 : AF_INET;
      if((localAddresses == 1) && (protocol != IPPROTO_SCTP) &&
         (localAddressArray[0].sa.sa_family == AF_INET)) {
         socketFamily = AF_INET;
      }
   }

#if defined(HAVE_MPTCP)
   if((protocol == IPPROTO_MPTCP) && (localAddresses > 1)) {
      // TRANSLATORS: Warning about multipath TCP limitations when multiple local addresses are provided.
      std::cerr << gettext("WARNING: Currently, MPTCP does not support TCP_MULTIPATH_ADD. Binding to ANY address instead ...\n");
   }
#endif

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
   unsigned int localAddressCount = localAddresses;

   int socketFamily = family;
   if(socketFamily == AF_UNSPEC) {
      socketFamily = checkIPv6() ? AF_INET6 : AF_INET;
      if((localAddresses == 1) && (protocol != IPPROTO_SCTP) &&
         (localAddressArray[0].sa.sa_family == AF_INET)) {
         socketFamily = AF_INET;
      }
   }

   sockaddr_union anyAddress;
   memset(&anyAddress, 0, sizeof(anyAddress));
   if(socketFamily == AF_INET6) {
      anyAddress.in6.sin6_family = AF_INET6;
      anyAddress.in6.sin6_port   = htobe16(localPort);
   }
   else {
      anyAddress.in.sin_family = AF_INET;
      anyAddress.in.sin_port   = htobe16(localPort);
   }

#if defined(HAVE_MPTCP)
   if((protocol == IPPROTO_MPTCP) && (localAddresses > 1)) {
      // TRANSLATORS: Warning about multipath TCP limitations when multiple local addresses are provided.
      std::cerr << gettext("WARNING: Currently, MPTCP does not support TCP_MULTIPATH_ADD. Binding to ANY address instead ...\n");
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
         char buffer[localAddressCount * sizeof(sockaddr_union)];
         char* ptr = buffer;
         for(unsigned int i = 0; i < localAddressCount; i++) {
            if(localAddressArray[i].sa.sa_family == AF_INET) {
               memcpy(ptr, &localAddressArray[i].in, sizeof(sockaddr_in));
               reinterpret_cast<sockaddr_in*>(ptr)->sin_port = htobe16(localPort);
               ptr += sizeof(sockaddr_in);
            }
            else if(localAddressArray[i].sa.sa_family == AF_INET6) {
               memcpy(ptr, &localAddressArray[i].in6, sizeof(sockaddr_in6));
               reinterpret_cast<sockaddr_in6*>(ptr)->sin6_port = htobe16(localPort);
               ptr += sizeof(sockaddr_in6);
            }
         }
         if(sctp_bindx(sd, reinterpret_cast<sockaddr*>(buffer), static_cast<int>(localAddressCount),
                       SCTP_BINDX_ADD_ADDR) != 0) {
            return -3;
         }
      }
      else {
#endif
         sockaddr_union localAddress;
         if(localAddressArray[0].sa.sa_family == AF_INET) {
            memcpy(&localAddress, &localAddressArray[0].in, sizeof(sockaddr_in));
            localAddress.in.sin_port = htobe16(localPort);
         }
         else if(localAddressArray[0].sa.sa_family == AF_INET6) {
            memcpy(&localAddress, &localAddressArray[0].in6, sizeof(sockaddr_in6));
            localAddress.in6.sin6_port = htobe16(localPort);
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

network_double_t doubleToNetwork(const double d)
{
   union DoubleIntUnion valueUnion;
   valueUnion.Double = d;
   return htobe64(valueUnion.Integer);
}

double networkToDouble(network_double_t value)
{
   union DoubleIntUnion valueUnion;
   valueUnion.Integer = be64toh(value);
   return valueUnion.Double;
}


// ###### Break and Termination Handling ####################################
static bool   DetectedBreak = false;
static bool   PrintedBreak  = false;
static bool   Quiet         = false;
static pid_t  MainThreadPID = 0;
static bool   PrintedKill   = false;
static uint64_t LastDetection = static_cast<uint64_t>(-1);

void breakDetector(int)
{
   DetectedBreak = true;
   if(!PrintedKill) {
      const uint64_t now = getMicroTime();
      if(LastDetection == static_cast<uint64_t>(-1)) {
         LastDetection = now;
      }
      else if(now - LastDetection >= 2000000ULL) {
         PrintedKill = true;
         // TRANSLATORS: Signal handler alert printed when SIGINT is caught twice within 2 seconds.
         std::cerr << "\x1b[0m\n" << gettext("*** Kill ***") << "\n\n";
         kill(MainThreadPID, SIGKILL);
      }
   }
}

void installBreakDetector()
{
   DetectedBreak = false;
   PrintedBreak  = false;
   Quiet         = false;
   MainThreadPID = getpid();
   PrintedKill   = false;
   LastDetection = static_cast<uint64_t>(-1);
   signal(SIGINT, &breakDetector);
}

void uninstallBreakDetector()
{
   signal(SIGINT, SIG_DFL);
   PrintedKill   = false;
   LastDetection = static_cast<uint64_t>(-1);
   Quiet         = false;
}

bool breakDetected()
{
   if(DetectedBreak && !PrintedBreak) {
      if(!Quiet) {
         // TRANSLATORS: Signal notification printed on the first SIGINT. {} is the signal number (SIGINT=2).
         std::cerr << formatText(gettext("\x1b[0m\n*** Break ***    Signal #{}\n\n"), SIGINT);
      }
      PrintedBreak = true;
   }
   return DetectedBreak;
}

void sendBreak(const bool quiet)
{
   Quiet = quiet;
   kill(MainThreadPID, SIGINT);
}


// ###### Random Number Generation ##########################################
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

const char* getRandomGeneratorName(const uint8_t rng)
{
   switch(rng) {
      case RANDOM_CONSTANT:    return "constant";
      case RANDOM_UNIFORM:     return "uniform";
      case RANDOM_EXPONENTIAL: return "exponential";
      case RANDOM_PARETO:      return "pareto";
      default:                 return "(invalid!)";
   }
}

static FILE* RandomDevice = nullptr;

uint8_t random8()   { return static_cast<uint8_t>(random32()); }
uint16_t random16() { return static_cast<uint16_t>(random32()); }
uint64_t random64() { return (static_cast<uint64_t>(random32()) << 32) | random32(); }

uint32_t random32()
{
   uint32_t number;
   if(RandomDevice == nullptr) {
      RandomDevice = fopen("/dev/urandom", "r");
   }
   if((RandomDevice != nullptr) && (fread(&number, sizeof(number), 1, RandomDevice) == 1)) {
      return number;
   }
   srandom(static_cast<unsigned int>(getMicroTime() & 0xffffffff));
   const uint16_t a = random() & 0xffff;
   const uint16_t b = random() & 0xffff;
   return (static_cast<uint32_t>(a) << 16) | b;
}

double randomDouble()
{
   return static_cast<double>(random32()) / static_cast<double>(0x100000000ULL);
}

double randomExpDouble(const double mean)
{
   return -mean * log(1.0 - randomDouble());
}

double randomNormal(const double mean, const double stddev)
{
   const double u = randomDouble();
   const double v = randomDouble();
   return mean + stddev * sqrt(-2.0 * log(u)) * cos(M_PI * 2.0 * v);
}

double randomTruncNormal(const double mean, const double stddev)
{
   double result;
   do {
      result = randomNormal(mean, stddev);
   } while(result < 0.0);
   return result;
}

double randomParetoDouble(const double location, const double shape)
{
   assert(shape > 0.0);
   double r = randomDouble();
   while((r <= 0.0) || (r >= 1.0)) {
      r = randomDouble();
   }
   return location / pow(r, 1.0 / shape);
}
