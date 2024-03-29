CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(netperfmeter LANGUAGES C CXX)

SET(BUILD_MAJOR "1")
SET(BUILD_MINOR "9")
SET(BUILD_PATCH "7")
SET(BUILD_VERSION ${BUILD_MAJOR}.${BUILD_MINOR}.${BUILD_PATCH})

SET(CMAKE_CXX_STANDARD 14)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_EXTENSIONS OFF)


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
SET(CPACK_SOURCE_IGNORE_FILES
   "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}-*;${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}_*;/build/;/.git/;/config.log;/CMakeFiles/;CMakeCache.txt$;.cmake$;.tar.gz$;/_CPack_Packages;/Makefile$;~$;/build-deb;/clean-deb;/filter-empty-entries;/make-symbols;/make-ppa;/make-deb;/debian.conf;/make-rpm;/rpm.conf;${CPACK_SOURCE_IGNORE_FILES}")
INCLUDE(CPack)

ADD_CUSTOM_TARGET(dist COMMAND ${CMAKE_MAKE_PROGRAM} clean package_source)


INCLUDE(CheckIncludeFile)
INCLUDE(CheckStructHasMember)
INCLUDE(CheckCCompilerFlag)


#############################################################################
#### OS DEPENDENT                                                        ####
#############################################################################

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
    ADD_DEFINITIONS("-DHAVE_MPTCP")   # Any better solution for this?
    FIND_LIBRARY(SCTP_LIB sctp)

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
#### CHECK STRUCT MEMBERS                                                ####
#############################################################################

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


#############################################################################
#### CHECK OPTIONS                                                       ####
#############################################################################

OPTION(BUILD_TEST_PROGRAMS "Build test programs" 0)
OPTION(BUILD_PLOT_PROGRAMS "Build plot programs" 0)


#############################################################################
#### REQUIREMENTS                                                        ####
#############################################################################

# ###### Threads ############################################################
FIND_PACKAGE(Threads REQUIRED)


# ###### SCTP ###############################################################
OPTION(USE_KERNEL_SCTP "Use Kernel SCTP" 1)
IF (USE_KERNEL_SCTP)
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
      MESSAGE(FATAL_ERROR "sctplib not found - libsctplib installed?")
   ENDIF()

   CHECK_INCLUDE_FILE(sctp.h HAVE_SCTPSOCKET_SCTP_H)
   FIND_LIBRARY(SCTPSOCKET_LIB sctpsocket)
   MESSAGE(STATUS "sctpsocket found: ${SCTPSOCKET_LIB}")
   IF (NOT HAVE_SCTPSOCKET_SCTP_H OR NOT SCTPSOCKET_LIB)
      MESSAGE(FATAL_ERROR "sctpsocket not found - libsctpsocket installed?")
   ENDIF()

   FIND_PACKAGE(PkgConfig)
   PKG_CHECK_MODULES(GLIB glib-2.0)
   IF (NOT ${GLIB_LIBS})
      MESSAGE(FATAL_ERROR "glib not found - glib-2.0 installed?")
   ENDIF()

   SET(SCTP_LIB ${SCTPSOCKET_LIB} ${SCTPLIB_LIB} ${GLIB_LIBRARIES})
ENDIF()


# ###### NEAT ###############################################################
OPTION(WITH_NEAT "Include NEAT support" 0)
IF (WITH_NEAT)
   CHECK_INCLUDE_FILE(neat-socketapi.h HAVE_NEAT_SOCKETAPI_H)
   FIND_LIBRARY(NEAT_SOCKETAPI_LIB neat-socketapi)
   MESSAGE(STATUS "neat-socketapi found: ${NEAT_SOCKETAPI_LIB}")
   IF (NOT HAVE_NEAT_SOCKETAPI_H OR NOT NEAT_SOCKETAPI_LIB)
      MESSAGE(FATAL_ERROR "neat-socketapi not found - neat-socketapi installed?")
   ENDIF()
ENDIF()


# ###### BZip2 ##############################################################
find_package(BZip2 REQUIRED)


# ###### IEEE Floating-Point ################################################
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


# ###### DCCP support #######################################################
CHECK_C_SOURCE_RUNS("
#include <stdio.h>
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


#############################################################################
#### COMPILER FLAGS                                                      ####
#############################################################################

SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -O2 -Wall")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall")

# ------ Check for "address-of-packed-member" -------------------------------
CHECK_C_COMPILER_FLAG("-Waddress-of-packed-member -Werror" HAS_ADDRESS_OF_PACKED_MEMBER)
IF (HAS_ADDRESS_OF_PACKED_MEMBER)
   # Disable "address-of-packed-member" warning:
   SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wno-address-of-packed-member")
   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-address-of-packed-member")
ENDIF()

# ------ Check for "array-bounds" -------------------------------------------
CHECK_C_COMPILER_FLAG("-Warray-bounds -Werror" HAS_ARRAY_BOUNDS)
IF (HAS_ARRAY_BOUNDS)
   # Disable "array-bounds" warning:
   SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wno-array-bounds")
   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-array-bounds")
ENDIF()

# ------ Check for "overlength-strings" -------------------------------------
CHECK_C_COMPILER_FLAG("-Woverlength-strings -Werror" HAS_OVERLENGTH_STRINGS)
IF (HAS_OVERLENGTH_STRINGS)
   # Disable "overlength-strings" warning:
   SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wno-overlength-strings")
   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-overlength-strings")
ENDIF()

# ------ Check for "unused-function" ----------------------------------------
CHECK_C_COMPILER_FLAG("-Wunused-function -Werror" HAS_UNUSED_FUNCTION)
IF (HAS_UNUSED_FUNCTION)
   # Disable "unused-function" warning:
   SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wno-unused-function")
   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function")
ENDIF()

# ------ Check for "unused-parameter" ---------------------------------------
CHECK_C_COMPILER_FLAG("-Wunused-parameter -Werror" HAS_UNUSED_PARAMETER)
IF (HAS_UNUSED_PARAMETER)
   # Disable "unused-parameter" warning:
   SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wno-unused-parameter")
   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
ENDIF()


#############################################################################
#### SUBDIRECTORIES                                                      ####
#############################################################################

ADD_SUBDIRECTORY(src)
