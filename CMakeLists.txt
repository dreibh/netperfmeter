# ==========================================================================
#         _   _      _   ____            __ __  __      _
#        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
#        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
#        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
#        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
#
#                  NetPerfMeter -- Network Performance Meter
#                 Copyright (C) 2009-2024 by Thomas Dreibholz
# ==========================================================================
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Contact:  dreibh@simula.no
# Homepage: https://www.nntb.no/~dreibh/netperfmeter/

CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(netperfmeter LANGUAGES C CXX)

SET(BUILD_MAJOR "2")
SET(BUILD_MINOR "0")
SET(BUILD_PATCH "0~rc1.11")
SET(BUILD_VERSION ${BUILD_MAJOR}.${BUILD_MINOR}.${BUILD_PATCH})


#############################################################################
#### INSTALLATION_DIRECTORIES                                            ####
#############################################################################

# See: https://cmake.org/cmake/help/v3.0/module/GNUInstallDirs.html
INCLUDE(GNUInstallDirs)


#############################################################################
#### PACKAGING                                                           ####
#############################################################################

SET(CPACK_SOURCE_GENERATOR "TXZ")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME
    "${CMAKE_PROJECT_NAME}-${BUILD_MAJOR}.${BUILD_MINOR}.${BUILD_PATCH}")
SET(CPACK_SOURCE_IGNORE_FILES "\\\\.git;\\\\.swp$;~$;\\\\.\\\\#;/\\\\#")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "^${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}[_-]")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "\\\\.cmake$|\\\\.make$|\\\\.log$")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "/CMakeCache\\\\.txt$")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "/(CMakeFiles|CMakeScripts|_CPack_Packages)/")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "/package-version\\\\.h$")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "/packaging\\\\.conf$")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "^${PROJECT_SOURCE_DIR}/(po.*/|src.*/|)Makefile$")
INCLUDE(CPack)

ADD_CUSTOM_TARGET(dist COMMAND ${CMAKE_MAKE_PROGRAM} clean package_source)


#############################################################################
#### UNINSTALL                                                           ####
#############################################################################

IF(NOT TARGET uninstall)
   CONFIGURE_FILE(
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
      "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
      IMMEDIATE @ONLY)

   ADD_CUSTOM_TARGET(uninstall
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
ENDIF()


#############################################################################
#### OS DEPENDENT                                                        ####
#############################################################################

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
   MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
   FIND_LIBRARY(SCTP_LIBRARY sctp)
   IF (NOT SCTP_LIBRARY)
      MESSAGE(FATAL_ERROR
              " Cannot find Linux Kernel SCTP library! Try:\n"
              " * Ubuntu/Debian: sudo apt install -y libsctp-dev\n"
              " * Fedora:        sudo dnf install -y lksctp-tools-devel\n"
              " * SuSE:          sudo zypper install -y lksctp-tools-devel\n"
              " * Alpine:        sudo apk add lksctp-tools-dev\n")
   ENDIF()

ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
   MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
   SET(CMAKE_REQUIRED_INCLUDES "/usr/local/include" "/usr/include")
   SET(CMAKE_LIBRARY_PATH "/usr/local/lib")
   INCLUDE_DIRECTORIES("/usr/local/include")

ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
   MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
   SET(CMAKE_REQUIRED_INCLUDES "/usr/local/include" "/usr/include" "/usr/local/opt/openssl/include")
   SET(CMAKE_LIBRARY_PATH "/usr/local/lib")
   INCLUDE_DIRECTORIES("/usr/local/include" "/usr/local/opt/openssl/include")

ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
   MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
   SET(CMAKE_REQUIRED_INCLUDES "/usr/pkg/include" "/usr/include" "/usr/local/include")
   SET(CMAKE_LIBRARY_PATH "/usr/local/lib")
   INCLUDE_DIRECTORIES("/usr/pkg/include" "/usr/local/include")

ELSE()
   MESSAGE(FATAL_ERROR ${CMAKE_SYSTEM_NAME} " not supported (yet?)")

ENDIF()


#############################################################################
#### OPTIONS                                                             ####
#############################################################################

OPTION(WITH_ICONS          "Build NetPerfMeter icons and logo files" 1)
OPTION(WITH_KERNEL_SCTP    "Use Kernel SCTP"                         1)
OPTION(WITH_PLOT_PROGRAMS  "Build plot programs"                     0)
OPTION(WITH_TEST_PROGRAMS  "Build test programs"                     0)


#############################################################################
#### COMPILER FLAGS                                                      ####
#############################################################################

INCLUDE(CheckCCompilerFlag)

# ------ Check for "address-of-packed-member" -------------------------------
CHECK_C_COMPILER_FLAG("-Waddress-of-packed-member -Werror" HAS_ADDRESS_OF_PACKED_MEMBER)
IF (HAS_ADDRESS_OF_PACKED_MEMBER)
   # Disable "address-of-packed-member" warning:
   SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wno-address-of-packed-member")
   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-address-of-packed-member")
ENDIF()


#############################################################################
#### CHECK STRUCT MEMBERS                                                ####
#############################################################################

# ====== Structure members ==================================================
INCLUDE(CheckStructHasMember)

CHECK_STRUCT_HAS_MEMBER("struct sockaddr" "sa_len" "sys/types.h;sys/socket.h" HAVE_SA_LEN)
IF (HAVE_SA_LEN)
    MESSAGE(STATUS "HAVE_SA_LEN")
    ADD_DEFINITIONS(-DHAVE_SA_LEN)
ENDIF()

CHECK_STRUCT_HAS_MEMBER("struct sockaddr_in" "sin_len" "sys/types.h;netinet/in.h" HAVE_SIN_LEN)
IF (HAVE_SIN_LEN)
    MESSAGE(STATUS "HAVE_SIN_LEN")
    ADD_DEFINITIONS(-DHAVE_SIN_LEN)
ENDIF()

CHECK_STRUCT_HAS_MEMBER("struct sockaddr_in6" "sin6_len" "sys/types.h;netinet/in.h" HAVE_SIN6_LEN)
IF (HAVE_SIN6_LEN)
    MESSAGE(STATUS "HAVE_SIN6_LEN")
    ADD_DEFINITIONS(-DHAVE_SIN6_LEN)
ENDIF()

CHECK_STRUCT_HAS_MEMBER("struct sockaddr_storage" "ss_len" "sys/types.h;sys/socket.h" HAVE_SS_LEN)
IF (HAVE_SS_LEN)
    MESSAGE(STATUS "HAVE_SS_LEN")
    ADD_DEFINITIONS(-DHAVE_SS_LEN)
ENDIF()


# ====== IEEE floating-point numbers ========================================
INCLUDE(CheckCSourceRuns)
CHECK_C_SOURCE_RUNS("
#include <stdio.h>

union DoubleIntUnion
{
   double             Double;
   unsigned long long Integer;
};

int main (int argc, char** argv)
{
   union DoubleIntUnion value;

   value.Double = 0.0;
   if(value.Integer != 0ULL) {
      return(1);
   }
   value.Double = 1000.5;
   if(value.Integer != 0x408f440000000000ULL) {
      return(1);
   }
   value.Double = -1000.5;
   if(value.Integer != 0xc08f440000000000ULL) {
      return(1);
   }

   return(0);
}"
USES_IEEEFP_FP)

IF (USES_IEEEFP_FP)
   ADD_DEFINITIONS(-DHAVE_IEEE_FP)
ENDIF()


# ====== DCCP support =======================================================
CHECK_C_SOURCE_RUNS("
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/dccp.h>

int main (int argc, char** argv)
{
   volatile int domain = IPPROTO_DCCP;
   volatile int type   = SOCK_DCCP;
   volatile int opt    = DCCP_SOCKOPT_SERVICE;
   return(0);
}"
HAVE_DCCP)

IF (HAVE_DCCP)
   ADD_DEFINITIONS(-DHAVE_DCCP)
ENDIF()


# ====== MPTCP support ======================================================
CHECK_C_SOURCE_RUNS("
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/mptcp.h>

int main (int argc, char** argv)
{
   volatile int domain = IPPROTO_MPTCP;
   return(0);
}"
HAVE_MPTCP)

IF (HAVE_MPTCP)
   ADD_DEFINITIONS(-DHAVE_MPTCP)
ENDIF()


# ====== QUIC support =======================================================
CHECK_C_SOURCE_RUNS("
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/quic.h>

int main (int argc, char** argv)
{
   volatile int domain = IPPROTO_QUIC;
   return(0);
}"
HAVE_QUIC)

IF (HAVE_QUIC)
   FIND_LIBRARY(QUIC_LIBRARY NAMES quic)
   IF (QUIC_LIBRARY)
      ADD_DEFINITIONS(-DHAVE_QUIC)
   ELSE()
      MESSAGE(WARNING "QUIC is available, but libquic is not!")
   ENDIF()
ENDIF()


#############################################################################
#### REQUIREMENTS                                                        ####
#############################################################################

# ====== Threads ============================================================
FIND_PACKAGE(Threads REQUIRED)

# ====== libbz2 =============================================================
FIND_LIBRARY(BZ2_LIBRARY NAMES bz2)
IF (NOT BZ2_LIBRARY)
   MESSAGE(FATAL_ERROR
           " Cannot find BZ2 library! Try:\n"
           " * Ubuntu/Debian: sudo apt install -y libbz2-dev\n"
           " * Fedora:        sudo dnf install -y bzip2-devel\n"
           " * SuSE:          sudo zypper install -y libbz2-devel\n"
           " * Alpine:        sudo apk add bzip2-dev\n"
           " * FreeBSD:       sudo pkg install -y bzip2")
ENDIF()

# ====== SCTP ===============================================================
IF (WITH_KERNEL_SCTP)
   # ====== Kernel SCTP =====================================================
   CHECK_INCLUDE_FILE(netinet/sctp.h HAVE_NETINET_SCTP_H)
   IF (HAVE_NETINET_SCTP_H)
      SET(SCTP_INCLUDE "netinet/sctp.h")
      ADD_DEFINITIONS(-DHAVE_KERNEL_SCTP -DHAVE_NETINET_SCTP_H)
   ENDIF()

ELSE()
   # ====== SCTPLIB User-Space SCTP =========================================
   CHECK_INCLUDE_FILE(sctp.h HAVE_SCTPLIB_SCTP_H)
   FIND_LIBRARY(SCTPLIB_LIB sctplib)
   MESSAGE(STATUS "sctplib found: ${SCTPLIB_LIB}")
   IF (NOT HAVE_SCTPLIB_SCTP_H OR NOT SCTPLIB_LIB)
      MESSAGE(FATAL_ERROR
              " Cannot find libsctplib user-space SCTP library! Try -DWITH_KERNEL_SCTP=1?")
   ENDIF()

   CHECK_INCLUDE_FILE(sctp.h HAVE_SCTPSOCKET_SCTP_H)
   FIND_LIBRARY(SCTPSOCKET_LIB sctpsocket)
   MESSAGE(STATUS "sctpsocket found: ${SCTPSOCKET_LIB}")
   IF (NOT HAVE_SCTPSOCKET_SCTP_H OR NOT SCTPSOCKET_LIB)
      MESSAGE(FATAL_ERROR
              " Cannot find libsctpsocket sockets API for user-space SCTP library! Try -DWITH_KERNEL_SCTP=1?")
   ENDIF()

   FIND_PACKAGE(PkgConfig)
   PKG_CHECK_MODULES(GLIB glib-2.0)
   IF (NOT ${GLIB_LIBS})
      MESSAGE(FATAL_ERROR
              " Cannot find libglib-2.0!")
   ENDIF()

   SET(SCTP_LIBRARY ${SCTPSOCKET_LIB} ${SCTPLIB_LIB} ${GLIB_LIBRARIES})
ENDIF()

# ====== Graphics tools for icons and logo ==================================
IF (WITH_ICONS)
   FIND_PROGRAM(GS gs PATHS /bin /usr/bin /usr/local/bin)
   IF(NOT GS)
      MESSAGE(FATAL_ERROR
            " Cannot find GhostScript! Try:\n"
            " * Ubuntu/Debian: sudo apt install -y ghostscript\n"
            " * Fedora:        sudo dnf install -y ghostscript\n"
            " * FreeBSD:       sudo pkg install -y ghostscript10")
   ENDIF()

   FIND_PROGRAM(PDF2SVG pdf2svg PATHS /bin /usr/bin /usr/local/bin)
   IF(NOT PDF2SVG)
      MESSAGE(FATAL_ERROR
            " Cannot find PDF2SVG! Try:\n"
            " * Ubuntu/Debian: sudo apt install -y pdf2svg\n"
            " * Fedora:        sudo dnf install -y pdf2svg\n"
            " * FreeBSD:       sudo pkg install -y pdf2svg")
   ENDIF()

   FIND_PROGRAM(GM gm PATHS /bin /usr/bin /usr/local/bin)
   IF(NOT GM)
      MESSAGE(FATAL_ERROR
            " Cannot find GraphicsMagick! Try:\n"
            " * Ubuntu/Debian: sudo apt install -y graphicsmagick\n"
            " * Fedora:        sudo dnf install -y GraphicsMagick\n"
            " * FreeBSD:       sudo pkg install -y GraphicsMagick")
   ENDIF()
ENDIF()


#############################################################################
#### SUBDIRECTORIES                                                      ####
#############################################################################

ADD_SUBDIRECTORY(src)
