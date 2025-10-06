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

#include "tools.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <poll.h>
#include <math.h>

#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ext_socket.h>
#include <stdio.h>
#include <netdb.h>
#include <time.h>

#include <iostream>


// ###### Create formatted string (printf-like) #############################
std::string format(const char* fmt, ...)
{
   char buffer[16384];
   va_list va;
   va_start(va, fmt);
   vsnprintf(buffer, sizeof(buffer), fmt, va);
   va_end(va);
   return std::string(buffer);
}


// ###### Get current time stamp ############################################
unsigned long long getMicroTime()
{
  struct timeval tv;
  gettimeofday(&tv,nullptr);
  return(((unsigned long long)tv.tv_sec * (unsigned long long)1000000) +
         (unsigned long long)tv.tv_usec);
}


// ###### Print time stamp ##################################################
void printTimeStamp(std::ostream& os)
{
   char str[128];
   const unsigned long long microTime = getMicroTime();
   const time_t timeStamp = microTime / 1000000;
   const struct tm *timeptr = localtime(&timeStamp);
   strftime((char*)&str,sizeof(str),"%d-%b-%Y %H:%M:%S",timeptr);
   os << str;
   snprintf((char*)&str,sizeof(str),
            ".%04d: ",(unsigned int)(microTime % 1000000) / 100);
   os << str;
}


// ###### Get timeout value for poll() from microtime values ################
int pollTimeout(const unsigned long long now, const size_t n, ...)
{
   va_list va;
   va_start(va, n);
   unsigned long long timeout = ~0;
   for(size_t i = 0;i < n;i++) {
      const unsigned long long t = va_arg(va, unsigned long long);
      timeout = std::min(timeout, t);
   }
   if(timeout == ~0ULL) {
      return -1;   // Infinite wait time (only care for sockets/files)
   }
   const double delta = (double)timeout - (double)now;
   if(delta <= 0.0) {
      return 0;   // Do not wait, just check sockets/files
   }
   else {
      // Return wait time in milliseconds.
      // NOTE: Return ceiling of the value, since 999ULL/1000ULL == 0!
      return (int)ceil(delta / 1000.0);
   }
}


// ###### Length-checking strcpy() #########################################
int safestrcpy(char* dest, const char* src, const size_t size)
{
   assert(size > 0);
   strncpy(dest, src, size);
   dest[size - 1] = 0x00;
   return strlen(dest) < size;
}


// ###### Length-checking strcat() #########################################
int safestrcat(char* dest, const char* src, const size_t size)
{
   const size_t l1 = strlen(dest);
   const size_t l2 = strlen(src);

   assert(size > 0);
   strncat(dest, src, size - l1 - 1);
   dest[size - 1] = 0x00;
   return l1 + l2 < size;
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
      string = (char*)&string[strlen(string)];
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
   const size_t found = name.rfind(suffix);
   if(found == name.length() - suffix.length()) {
      return true;
   }
   return false;
}


// ###### Dissect file name into prefix and suffix ##########################
void dissectName(const std::string& name,
                 std::string&       prefix,
                 std::string&       suffix)
{
   const size_t slash = name.find_last_of('/');
   size_t dot         = name.find_last_of('.');
   if((dot != std::string::npos) &&
      ((slash == std::string::npos) || (slash < dot)) ) {
      // There is a suffix which is part of the file name itself
      if(std::string(name, dot) == ".bz2") {
         // There is a .bz2 suffix. Look for the actual suffix.
         const size_t dot2 = name.find_last_of('.', dot - 1);
         if( (dot2 != std::string::npos) &&
             ((slash == std::string::npos) || (slash < dot2)) ) {
            // There is another suffix (*.bz2) *and* the second dot
            // is not part of a directory name
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
   uint16_t     p1, p2;
   uint32_t     x1[4];
   uint32_t     x2[4];
   int          result;

   if( ((a1->sa_family == AF_INET) || (a1->sa_family == AF_INET6)) &&
       ((a2->sa_family == AF_INET) || (a2->sa_family == AF_INET6)) ) {
      if(a1->sa_family == AF_INET6) {
         memcpy((void*)&x1, (void*)&((struct sockaddr_in6*)a1)->sin6_addr, 16);
      }
      else {
         x1[0] = 0;
         x1[1] = 0;
         x1[2] = htonl(0xffff);
         x1[3] = *((uint32_t*)&((struct sockaddr_in*)a1)->sin_addr);
      }

      if(a2->sa_family == AF_INET6) {
         memcpy((void*)&x2, (void*)&((struct sockaddr_in6*)a2)->sin6_addr, 16);
      }
      else {
         x2[0] = 0;
         x2[1] = 0;
         x2[2] = htonl(0xffff);
         x2[3] = *((uint32_t*)&((struct sockaddr_in*)a2)->sin_addr);
      }

      result = memcmp((void*)&x1,(void*)&x2,16);
      if(result != 0) {
         return result;
      }

      if(port) {
         p1 = getPort((struct sockaddr*)a1);
         p2 = getPort((struct sockaddr*)a2);
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
   const struct sockaddr_in*  ipv4address;
   const struct sockaddr_in6* ipv6address;
   char                       str[128];
   char                       scope[IFNAMSIZ + 16];
   char                       ifnamebuffer[IFNAMSIZ];
   const char*                ifname;

   switch(address->sa_family) {
      case AF_INET:
         ipv4address = (const struct sockaddr_in*)address;
         if(port) {
            snprintf(buffer, length,
                     "%s:%d", inet_ntoa(ipv4address->sin_addr), ntohs(ipv4address->sin_port));
         }
         else {
            snprintf(buffer, length, "%s", inet_ntoa(ipv4address->sin_addr));
         }
         return true;

      case AF_INET6:
         ipv6address = (const struct sockaddr_in6*)address;
         if( (!hideScope) &&
             (IN6_IS_ADDR_LINKLOCAL(&ipv6address->sin6_addr) ||
              IN6_IS_ADDR_MC_LINKLOCAL(&ipv6address->sin6_addr)) ) {
            ifname = if_indextoname(ipv6address->sin6_scope_id, (char*)&ifnamebuffer);
            if(ifname == nullptr) {
               safestrcpy((char*)&ifnamebuffer, "(BAD!)", sizeof(ifnamebuffer));
               return false;
            }
            snprintf((char*)&scope, sizeof(scope), "%%%s", ifname);
         }
         else {
            scope[0] = 0x00;
         }
         if(inet_ntop(AF_INET6, &ipv6address->sin6_addr, str, sizeof(str)) != nullptr) {
            if(port) {
               snprintf(buffer, length,
                        "[%s%s]:%d", str, scope, ntohs(ipv6address->sin6_port));
            }
            else {
               snprintf(buffer, length, "%s%s", str, scope);
            }
            return true;
         }
       break;

      case AF_UNSPEC:
         safestrcpy(buffer, "(unspecified)", length);
         return true;
   }
   return false;
}


// ###### Convert string to address #########################################
bool string2address(const char*           string,
                    union sockaddr_union* address,
                    const bool            readPort)
{
   char                 host[128];
   char                 port[128];
   struct sockaddr_in*  ipv4address = (struct sockaddr_in*)address;
   struct sockaddr_in6* ipv6address = (struct sockaddr_in6*)address;
   int                  portNumber  = 0;
   char*                p1;

   struct addrinfo  hints;
   struct addrinfo* res;
   bool isNumeric;
   bool isIPv6;
   size_t hostLength;
   size_t i;

   if(strlen(string) > sizeof(host)) {
      return false;
   }
   strcpy((char*)&host,string);
   strcpy((char*)&port, "0");

   /* ====== Handle RFC2732-compliant addresses ========================== */
   if(string[0] == '[') {
      p1 = strindex(host,']');
      if(p1 != nullptr) {
         if((p1[1] == ':') || (p1[1] == '!')) {
            strcpy((char*)&port, &p1[2]);
         }
         memmove((char*)&host, (char*)&host[1], (long)p1 - (long)host - 1);
         host[(long)p1 - (long)host - 1] = 0x00;
      }
   }

   /* ====== Handle standard address:port ================================ */
   else {
      if(readPort) {
         unsigned int colons = 0;
         for(size_t i = 0;i < strlen(host);i++) {
            if(host[i] == ':') {
               colons++;
            }
         }
         if(colons == 1) {
            p1 = strrindex(host,':');
            if(p1 == nullptr) {
               p1 = strrindex(host,'!');
            }
            if(p1 != nullptr) {
               p1[0] = 0x00;
               strcpy((char*)&port, &p1[1]);
            }
         }
      }
   }

   /* ====== Check port number =========================================== */
   portNumber = ~0;
   if((sscanf(port, "%d", &portNumber) != 1) ||
      (portNumber < 0) ||
      (portNumber > 65535)) {
      return false;
   }


   /* ====== Create address structure ==================================== */

   /* ====== Get information for host ==================================== */
   res        = nullptr;
   isNumeric  = true;
   isIPv6     = false;
   hostLength = strlen(host);

   memset((char*)&hints,0,sizeof(hints));
   hints.ai_socktype = SOCK_DGRAM;
   hints.ai_family   = AF_UNSPEC;

   for(i = 0;i < hostLength;i++) {
      if(host[i] == ':') {
         isIPv6 = true;
         break;
      }
   }
   if(!isIPv6) {
      for(i = 0;i < hostLength;i++) {
         if(!(isdigit(host[i]) || (host[i] == '.'))) {
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

   memset((char*)address,0,sizeof(union sockaddr_union));
   memcpy((char*)address,res->ai_addr,res->ai_addrlen);

   switch(ipv4address->sin_family) {
      case AF_INET:
         ipv4address->sin_port = htons(portNumber);
#ifdef HAVE_SIN_LEN
         ipv4address->sin_len  = sizeof(struct sockaddr_in);
#endif
       break;
      case AF_INET6:
         ipv6address->sin6_port = htons(portNumber);
#ifdef HAVE_SIN6_LEN
         ipv6address->sin6_len  = sizeof(struct sockaddr_in6);
#endif
       break;

      default:
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
   static char str[160];

   if(address2string(address, (char*)&str, sizeof(str), port, hideScope)) {
      os << str;
   }
   else {
      os << "(invalid!)";
   }
}


// ###### Get protocol name #################################################
const char* getProtocolName(const int protocol)
{
   const char* protocolName;
   switch(protocol) {
      case IPPROTO_SCTP:
         protocolName = "SCTP";
       break;
      case IPPROTO_TCP:
         protocolName = "TCP";
       break;
      case IPPROTO_UDP:
         protocolName = "UDP";
       break;
#ifdef HAVE_MPTCP
      case IPPROTO_MPTCP:
         protocolName = "MPTCP";
       break;
#endif
#ifdef HAVE_DCCP
      case IPPROTO_DCCP:
         protocolName = "DCCP";
       break;
#endif
      default:
         protocolName = "unknown?!";
       break;
   }
   return protocolName;
}


// ###### Get port ##########################################################
uint16_t getPort(const struct sockaddr* address)
{
   if(address != nullptr) {
      switch(address->sa_family) {
         case AF_INET:
            return ntohs(((struct sockaddr_in*)address)->sin_port);
         case AF_INET6:
            return ntohs(((struct sockaddr_in6*)address)->sin6_port);
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
            ((struct sockaddr_in*)address)->sin_port = htons(port);
            return true;
         case AF_INET6:
            ((struct sockaddr_in6*)address)->sin6_port = htons(port);
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
   // ====== Get family =====================================================
   int socketFamily = family;
   if(socketFamily == AF_UNSPEC) {
      socketFamily = checkIPv6() ? AF_INET6 : AF_INET;
      if( (localAddresses == 1) &&
          (protocol != IPPROTO_SCTP) &&
          (localAddressArray[0].sa.sa_family == AF_INET) ) {
         socketFamily = AF_INET;   // Restrict to IPv4!
      }
   }

   // ====== Get protocol ===================================================
   int socketProtocol = protocol;
#ifdef HAVE_MPTCP
   if(socketProtocol == IPPROTO_MPTCP) {
      if(localAddresses > 1) {
         printf("WARNING: Currently, MPTCP does not support TCP_MULTIPATH_ADD. Binding to ANY address instead ...\n");
      }
   }
#endif

   // ====== Create socket ==================================================
   int sd = ext_socket(socketFamily, type, socketProtocol);
   if(sd < 0) {
      return -2;
   }
   return sd;
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

   // ====== Get family =====================================================
   int socketFamily = family;
   if(socketFamily == AF_UNSPEC) {
      socketFamily = checkIPv6() ? AF_INET6 : AF_INET;
      if( (localAddresses == 1) &&
          (protocol != IPPROTO_SCTP) &&
          (localAddressArray[0].sa.sa_family == AF_INET) ) {
         socketFamily = AF_INET;   // Restrict to IPv4!
      }
   }

   // Prepare ANY address for the corresponding family
   sockaddr_union anyAddress;
   memset(&anyAddress, 0, sizeof(anyAddress));
   if(socketFamily == AF_INET6) {
      anyAddress.in6.sin6_family = AF_INET6;
      anyAddress.in6.sin6_port   = htons(localPort);
   }
   else {
      anyAddress.in.sin_family = AF_INET;
      anyAddress.in.sin_port   = htons(localPort);
   }

   // ====== Get protocol ===================================================
   int socketProtocol = protocol;
#ifdef HAVE_MPTCP
   if(socketProtocol == IPPROTO_MPTCP) {
      if(localAddresses > 1) {
         printf("WARNING: Currently, MPTCP does not support TCP_MULTIPATH_ADD. Binding to ANY address instead ...\n");
         localAddressCount = 0;
      }
   }
#endif

#ifdef IPV6_V6ONLY
   if(socketFamily == AF_INET6) {
      // Accept IPv4 and IPv6 connections.
      const int on = (bindV6Only == true) ? 1 : 0;
      // printf("IPV6_V6ONLY=%d\n", on);
      if(ext_setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0) {
         return -2;
      }
   }
#else
#error IPV6_V6ONLY not defined?! Please create a bug report and provide some information about your OS!
#endif

   // ====== Bind socket ====================================================
   if(localAddressCount == 0) {
      if(ext_bind(sd, &anyAddress.sa, getSocklen(&anyAddress.sa)) != 0) {
         return -3;
      }
   }
   else {
      if(socketProtocol == IPPROTO_SCTP) {
         // ====== SCTP bind: bind to specified addresses ===================
         char   buffer[localAddressCount * sizeof(sockaddr_union)];
         char*  ptr = (char*)&buffer[0];
         for(unsigned int i = 0;i < localAddressCount;i++) {
            if(localAddressArray[i].sa.sa_family == AF_INET) {
               memcpy(ptr, (void*)&localAddressArray[i].in, sizeof(sockaddr_in));
               ((sockaddr_in*)ptr)->sin_port = htons(localPort);
               ptr += sizeof(sockaddr_in);
            }
            else if(localAddressArray[i].sa.sa_family == AF_INET6) {
               memcpy(ptr, (void*)&localAddressArray[i].in6, sizeof(sockaddr_in6));
               ((sockaddr_in6*)ptr)->sin6_port = htons(localPort);
               ptr += sizeof(sockaddr_in6);
            }
            else {
               assert(false);
            }
         }
         if(sctp_bindx(sd, (sockaddr*)&buffer, localAddressCount,
                       SCTP_BINDX_ADD_ADDR) != 0) {
            return -3;
         }
      }
      else {
         // ====== Non-SCTP bind: bind to ANY address =======================
         sockaddr_union localAddress;
         if(localAddressArray[0].sa.sa_family == AF_INET) {
            memcpy(&localAddress, (void*)&localAddressArray[0].in, sizeof(sockaddr_in));
            ((sockaddr_in*)&localAddress)->sin_port = htons(localPort);
         }
         else if(localAddressArray[0].sa.sa_family == AF_INET6) {
            memcpy(&localAddress, (void*)&localAddressArray[0].in6, sizeof(sockaddr_in6));
            ((sockaddr_in6*)&localAddress)->sin6_port = htons(localPort);
         }
         else {
            assert(false);
         }

         if(ext_bind(sd, &localAddress.sa, getSocklen(&localAddress.sa)) != 0) {
            return -3;
         }
      }
   }

   // ====== Put socket into listening mode =================================
   if(listenMode) {
      ext_listen(sd, 10);
   }
   return sd;
}


// ###### Send SCTP ABORT ###################################################
bool sendAbort(int sd, sctp_assoc_t assocID)
{
   sctp_sndrcvinfo sinfo;
   memset(&sinfo, 0, sizeof(sinfo));
   sinfo.sinfo_assoc_id = assocID;
   sinfo.sinfo_flags    = SCTP_ABORT;

   return sctp_send(sd, nullptr, 0, &sinfo, 0) >= 0;
}


// ###### Convert byte order of 64 bit value ################################
static uint64_t byteswap64(const uint64_t value)
{
#if BYTE_ORDER == LITTLE_ENDIAN
   const uint32_t a = (uint32_t)(value >> 32);
   const uint32_t b = (uint32_t)(value & 0xffffffff);
   return( (int64_t)((a << 24) | ((a & 0x0000ff00) << 8) |
           ((a & 0x00ff0000) >> 8) | (a >> 24)) |
           ((int64_t)((b << 24) | ((b & 0x0000ff00) << 8) |
           ((b & 0x00ff0000) >> 8) | (b >> 24)) << 32) );
#elif BYTE_ORDER == BIG_ENDIAN
   return value;
#else
#error Byte order undefined!
#endif
}


// ###### Convert byte order of 64 bit value ################################
uint64_t hton64(const uint64_t value)
{
   return byteswap64(value);
}


// ###### Convert byte order of 64 bit value ################################
uint64_t ntoh64(const uint64_t value)
{
   return byteswap64(value);
}



#ifndef HAVE_IEEE_FP
#warning Is this code really working correctly?

#define DBL_EXP_BITS  11
#define DBL_EXP_BIAS  1023
#define DBL_EXP_MAX   ((1L << DBL_EXP_BITS) - 1 - DBL_EXP_BIAS)
#define DBL_EXP_MIN   (1 - DBL_EXP_BIAS)
#define DBL_FRC1_BITS 20
#define DBL_FRC2_BITS 32
#define DBL_FRC_BITS  (DBL_FRC1_BITS + DBL_FRC2_BITS)


struct IeeeDouble {
#if __BYTE_ORDER == __BIG_ENDIAN
   unsigned int s : 1;
   unsigned int e : 11;
   unsigned int f1 : 20;
   unsigned int f2 : 32;
#elif  __BYTE_ORDER == __LITTLE_ENDIAN
   unsigned int f2 : 32;
   unsigned int f1 : 20;
   unsigned int e : 11;
   unsigned int s : 1;
#else
#error Unknown byteorder settings!
#endif
};


// ###### Convert double to machine-independent form ########################
network_double_t doubleToNetwork(const double d)
{
   struct IeeeDouble ieee;

   if(isnan(d)) {
      // NaN
      ieee.s = 0;
      ieee.e = DBL_EXP_MAX + DBL_EXP_BIAS;
      ieee.f1 = 1;
      ieee.f2 = 1;
   } else if(isinf(d)) {
      // +/- infinity
      ieee.s = (d < 0);
      ieee.e = DBL_EXP_MAX + DBL_EXP_BIAS;
      ieee.f1 = 0;
      ieee.f2 = 0;
   } else if(d == 0.0) {
      // zero
      ieee.s = 0;
      ieee.e = 0;
      ieee.f1 = 0;
      ieee.f2 = 0;
   } else {
      // finite number
      int exp;
      double frac = frexp (fabs (d), &exp);

      while (frac < 1.0 && exp >= DBL_EXP_MIN) {
         frac = ldexp (frac, 1);
         --exp;
      }
      if (exp < DBL_EXP_MIN) {
          // denormalized number (or zero)
          frac = ldexp (frac, exp - DBL_EXP_MIN);
          exp = 0;
      } else {
         // normalized number
         assert((1.0 <= frac) && (frac < 2.0));
         assert((DBL_EXP_MIN <= exp) && (exp <= DBL_EXP_MAX));

         exp += DBL_EXP_BIAS;
         frac -= 1.0;
      }
      ieee.s = (d < 0);
      ieee.e = exp;
      ieee.f1 = (unsigned long)ldexp (frac, DBL_FRC1_BITS);
      ieee.f2 = (unsigned long)ldexp (frac, DBL_FRC_BITS);
   }
   return hton64(*((network_double_t*)&ieee));
}


// ###### Convert machine-independent form to double ########################
double networkToDouble(network_double_t value)
{
   network_double_t   hValue;
   struct IeeeDouble* ieee;
   double             d;

   hValue = ntoh64(value);
   ieee = (struct IeeeDouble*)&hValue;
   if(ieee->e == 0) {
      if((ieee->f1 == 0) && (ieee->f2 == 0)) {
         // zero
         d = 0.0;
      } else {
         // denormalized number
         d  = ldexp((double)ieee->f1, -DBL_FRC1_BITS + DBL_EXP_MIN);
         d += ldexp((double)ieee->f2, -DBL_FRC_BITS  + DBL_EXP_MIN);
         if (ieee->s) {
            d = -d;
         }
      }
   } else if(ieee->e == DBL_EXP_MAX + DBL_EXP_BIAS) {
      if((ieee->f1 == 0) && (ieee->f2 == 0)) {
         // +/- infinity
         d = (ieee->s) ? -INFINITY : INFINITY;
      } else {
         // not a number
         d = NAN;
      }
   } else {
      // normalized number
      d = ldexp(ldexp((double)ieee->f1, -DBL_FRC1_BITS) +
                ldexp((double)ieee->f2, -DBL_FRC_BITS) + 1.0,
                      ieee->e - DBL_EXP_BIAS);
      if(ieee->s) {
         d = -d;
      }
   }
   return d;
}

#else

union DoubleIntUnion
{
   double             Double;
   unsigned long long Integer;
};

// ###### Convert double to machine-independent form ########################
network_double_t doubleToNetwork(const double d)
{
   union DoubleIntUnion valueUnion;
   valueUnion.Double = d;
   return hton64(valueUnion.Integer);
}

// ###### Convert machine-independent form to double ########################
double networkToDouble(network_double_t value)
{
   union DoubleIntUnion valueUnion;
   valueUnion.Integer = ntoh64(value);
   return valueUnion.Double;
}

#endif



/* Kill after timeout: Send kill signal, if Ctrl-C is pressed again
   after more than KILL_TIMEOUT microseconds */
#define KILL_AFTER_TIMEOUT
#define KILL_TIMEOUT 2000000


// ###### Global variables ##################################################
static bool   DetectedBreak = false;
static bool   PrintedBreak  = false;
static bool   Quiet         = false;
static pid_t  MainThreadPID = 0;
#ifdef KILL_AFTER_TIMEOUT
static bool               PrintedKill   = false;
static unsigned long long LastDetection = (unsigned long long)-1;
#endif


// ###### Handler for SIGINT ################################################
void breakDetector(int signum)
{
   DetectedBreak = true;

#ifdef KILL_AFTER_TIMEOUT
   if(!PrintedKill) {
      const unsigned long long now = getMicroTime();
      if(LastDetection == (unsigned long long)-1) {
         LastDetection = now;
      }
      else if(now - LastDetection >= 2000000) {
         PrintedKill = true;
         fprintf(stderr,"\x1b[0m\n*** Kill ***\n\n");
         kill(MainThreadPID,SIGKILL);
      }
   }
#endif
}


// ###### Install break detector ############################################
void installBreakDetector()
{
   DetectedBreak = false;
   PrintedBreak  = false;
   Quiet         = false;
   MainThreadPID = getpid();
#ifdef KILL_AFTER_TIMEOUT
   PrintedKill   = false;
   LastDetection = (unsigned long long)-1;
#endif
   signal(SIGINT,&breakDetector);
}


// ###### Unnstall break detector ###########################################
void uninstallBreakDetector()
{
   signal(SIGINT,SIG_DFL);
#ifdef KILL_AFTER_TIMEOUT
   PrintedKill   = false;
   LastDetection = (unsigned long long)-1;
#endif
   /* No reset here!
      DetectedBreak = false;
      PrintedBreak  = false; */
   Quiet         = false;
}


// ###### Check, if break has been detected #################################
bool breakDetected()
{
   if((DetectedBreak) && (!PrintedBreak)) {
      if(!Quiet) {
         fprintf(stderr,"\x1b[0m\n*** Break ***    Signal #%d\n\n",SIGINT);
      }
      PrintedBreak = getMicroTime();
   }
   return DetectedBreak;
}


// ###### Send break to main thread #########################################
void sendBreak(const bool quiet)
{
   Quiet = quiet;
   kill(MainThreadPID,SIGINT);
}



// ###### Get random value using specified random number generator ##########
double getRandomValue(const double* valueArray, const uint8_t rng)
{
   double value;
   switch(rng) {
      case RANDOM_CONSTANT:
         value = valueArray[0];
       break;
      case RANDOM_UNIFORM:
         value = valueArray[0] + (randomDouble() * (valueArray[1] - valueArray[0]));
       break;
      case RANDOM_EXPONENTIAL:
         value = randomExpDouble(valueArray[0]);
       break;
      case RANDOM_PARETO:
         value = randomParetoDouble(valueArray[0], valueArray[1]);
       break;
      case RANDOM_NORMAL:
         value = randomNormal(valueArray[0], valueArray[1]);
       break;
      case RANDOM_TRUNCATED_NORMAL:
         value = randomTruncNormal(valueArray[0], valueArray[1]);
       break;
      default:
         abort();
       break;
   }
   return value;
}


// ###### Get name of specified random number generator #####################
const char* getRandomGeneratorName(const uint8_t rng)
{
   switch(rng) {
      case RANDOM_CONSTANT:
         return "constant";
      case RANDOM_UNIFORM:
         return "uniform";
      case RANDOM_EXPONENTIAL:
         return "exponential";
      case RANDOM_PARETO:
         return "pareto";
   }
   return "(invalid!)";
}


/*
   It is tried to use /dev/urandom as random source first, since
   it provides high-quality random numbers. If /dev/urandom is not
   available, use the clib's random() function with a seed given
   by the current microseconds time. However, the random number
   quality is much lower since the seed may be easily predictable.
*/

#define RS_TRY_DEVICE 0
#define RS_DEVICE     1
#define RS_CLIB       2

static int   RandomSource = RS_TRY_DEVICE;
static FILE* RandomDevice = nullptr;


// ###### Get 8-bit random value ############################################
uint8_t random8()
{
   return (uint8_t)random32();
}


// ###### Get 16-bit random value ###########################################
uint16_t random16()
{
   return (uint16_t)random32();
}


// ###### Get 64-bit random value ###########################################
uint64_t random64()
{
   return (((uint64_t)random32()) << 32) | (uint64_t)random32();
}


// ###### Get 32-bit random value ###########################################
uint32_t random32()
{
#if defined(SIM_IMPORT) || defined(OMNETPPLIBS_IMPORT)
#warning Using OMNeT++ random generator instead of time-seeded one!
   const double value = uniform(0.0, (double)0xffffffff);
   return (uint32_t)rint(value);
#else
   uint32_t number;

   switch(RandomSource) {
      case RS_DEVICE:
         if(fread(&number, sizeof(number), 1, RandomDevice) == 1) {
            return number;
         }
         RandomSource = RS_CLIB;
      case RS_TRY_DEVICE:
         RandomDevice = fopen("/dev/urandom", "r");
         if(RandomDevice != nullptr) {
            if(fread(&number, sizeof(number), 1, RandomDevice) == 1) {
               srandom(number);
               RandomSource = RS_DEVICE;
               return number;
            }
            fclose(RandomDevice);
         }
         RandomSource = RS_CLIB;
         srandom((unsigned int)(getMicroTime() & (uint64_t)0xffffffff));
       break;
   }
   const uint16_t a = random() & 0xffff;
   const uint16_t b = random() & 0xffff;
   return (((uint32_t)a) << 16) | (uint32_t)b;
#endif
}


// ###### Get double random value ###########################################
double randomDouble()
{
   // Resulting range: [0, 1)
   return (double)random32() / (double)0x100000000;
}


// ###### Get exponential-distributed double random value ###################
double randomExpDouble(const double mean)
{
   // randomDouble() returns value in [0, 1), i.e. 0 is included. log(0) = -∞.
   // => Using 1.0 - randomDouble() to prevent this issue:
   return -mean * log(1.0 - randomDouble());
}


// ###### Get normal-distributed double random value ########################
double randomNormal(const double mean, const double stddev)
{
    const double u = randomDouble();
    const double v = randomDouble();
    return mean + stddev * sqrt(-2.0*log(u)) * cos(M_PI*2.0*v);
}


// ###### Get truncated-normal-distributed double random value ##############
double randomTruncNormal(const double mean, const double stddev)
{
   double result;
   do {
       result = randomNormal(mean, stddev);
   } while(result < 0);
   return result;
}


// ###### Get pareto-distributed double random value ########################
// Parameters:
// location = the location parameter (also: scale parameter): x_m, x_min or m
// shape    = the shape parameter: alpha or k
//
// Based on rpareto from GNU R's VGAM package
// (http://cran.r-project.org/web/packages/VGAM/index.html):
// rpareto <- function (n, location, shape)
// {
//     ans <- location/runif(n)^(1/shape)
//     ans[location <= 0] <- NaN
//     ans[shape <= 0] <- NaN
//     ans
// }
//
// Some description:
// http://en.wikipedia.org/wiki/Pareto_distribution
//
// Mean: E(X) = shape*location / (shape - 1) for alpha > 1
// => location = E(X)*(shape - 1) / shape
//
double randomParetoDouble(const double location, const double shape)
{
   assert(shape > 0.0);

   double r = randomDouble();
   while ((r <= 0.0) || (r >= 1.0)) {
      r = randomDouble();
   }
   return location / pow(r, 1.0 / shape);
}


// ###### poll() to select() wrapper to work around broken Apple poll() #####
#ifdef __APPLE__
#warning Using poll() to select() wrapper to work around broken Apple poll().
int ext_poll_wrapper(struct pollfd* fdlist, long unsigned int count, int time)
{
   // ====== Prepare timeout setting ========================================
   struct       timeval  timeout;
   struct       timeval* to;
   int          fdcount = 0;
   int          n;
   int          result;
   unsigned int i;

   if(time < 0)
      to = nullptr;
   else {
      to = &timeout;
      timeout.tv_sec  = time / 1000;
      timeout.tv_usec = (time % 1000) * 1000;
   }

   // ====== Prepare FD settings ============================================
   fd_set readfdset;
   fd_set writefdset;
   fd_set exceptfdset;
   FD_ZERO(&readfdset);
   FD_ZERO(&writefdset);
   FD_ZERO(&exceptfdset);
   n = 0;
   for(i = 0; i < count; i++) {
      if(fdlist[i].fd < 0) {
         continue;
      }
      if(fdlist[i].events & POLLIN) {
         FD_SET(fdlist[i].fd, &readfdset);
      }
      if(fdlist[i].events & POLLOUT) {
         FD_SET(fdlist[i].fd, &writefdset);
      }
      FD_SET(fdlist[i].fd, &exceptfdset);
      n = std::max(n, fdlist[i].fd);
      fdcount++;
   }
   for(i = 0;i < count;i++) {
      fdlist[i].revents = 0;
   }

   // ====== Do ext_select() ================================================
   result = ext_select(n + 1, &readfdset, &writefdset ,&exceptfdset, to);
   if(result < 0) {
      return result;
   }

   // ====== Set result flags ===============================================
   for(i = 0;i < count;i++) {
      if(FD_ISSET(fdlist[i].fd,&readfdset) && (fdlist[i].events & POLLIN)) {
         fdlist[i].revents |= POLLIN;
      }
      if(FD_ISSET(fdlist[i].fd,&writefdset) && (fdlist[i].events & POLLOUT)) {
         fdlist[i].revents |= POLLOUT;
      }
      if(FD_ISSET(fdlist[i].fd,&exceptfdset)) {
         fdlist[i].revents |= POLLERR;
      }
   }
   return result;
}
#endif
