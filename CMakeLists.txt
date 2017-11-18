CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(netperfmeter)

set(BUILD_MAJOR "1")
set(BUILD_MINOR "7")
set(BUILD_PATCH "6")
set(BUILD_VERSION ${BUILD_MAJOR}.${BUILD_MINOR}.${BUILD_PATCH})


#################################################
# INSTALLATION_DIRECTORIES
#################################################

SET(CMAKE_INSTALL_BINDIR_DOC        "user executables [PREFIX/bin]")
SET(CMAKE_INSTALL_SBINDIR_DOC       "system admin executables [EPREFIX/sbin]")
SET(CMAKE_INSTALL_DATADIR_DOC       "read-only architecture-independent data [PREFIX/share]")
SET(CMAKE_INSTALL_SYSCONFDIR_DOC    "read-only single-machine data [PREFIX/etc]")
SET(CMAKE_INSTALL_LOCALSTATEDIR_DOC "modifiable single-machine data [PREFIX/var]")
SET(CMAKE_INSTALL_LIBDIR_DOC        "object code libraries [PREFIX/lib]")
SET(CMAKE_INSTALL_INCLUDEDIR_DOC    "read-only includes [PREFIX/include]")
SET(CMAKE_INSTALL_MANDIR_DOC        "man documentation [PREFIX/share/man]")

SET(CMAKE_INSTALL_BINDIR "bin"
    CACHE PATH ${CMAKE_INSTALL_BINDIR_DOC})
SET(CMAKE_INSTALL_SBINDIR "sbin"
    CACHE PATH ${CMAKE_INSTALL_SBINDIR_DOC})
SET(CMAKE_INSTALL_DATADIR "share"
    CACHE PATH ${CMAKE_INSTALL_DATADIR_DOC})
SET(CMAKE_INSTALL_SYSCONFDIR "etc"
    CACHE PATH ${CMAKE_INSTALL_SYSCONFDIR_DOC})
SET(CMAKE_INSTALL_LOCALSTATEDIR "var"
    CACHE PATH ${CMAKE_INSTALL_LOCALSTATEDIR_DOC})
SET(CMAKE_INSTALL_LIBDIR "lib"
    CACHE PATH ${CMAKE_INSTALL_LIBDIR_DOC})
SET(CMAKE_INSTALL_INCLUDEDIR "include"
    CACHE PATH ${CMAKE_INSTALL_INCLUDEDIR_DOC})
SET(CMAKE_INSTALL_MANDIR "share/man"
    CACHE PATH ${CMAKE_INSTALL_MANDIR_DOC})


#################################################
# PACKAGING
#################################################

set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
   "${CMAKE_PROJECT_NAME}-${BUILD_MAJOR}.${BUILD_MINOR}.${BUILD_PATCH}")
set(CPACK_SOURCE_IGNORE_FILES
   "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}-*;${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}_*;/build/;/.git/;/config.log;/CMakeFiles/;CMakeCache.txt$;.cmake$;.tar.gz$;/_CPack_Packages;/Makefile$;~$;/build-deb;/clean-deb;/filter-empty-entries;/make-symbols;/make-ppa;/make-deb;/debian.conf;/make-rpm;/rpm.conf;${CPACK_SOURCE_IGNORE_FILES}")
include(CPack)

add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} clean package_source)


INCLUDE(CheckIncludeFile)
INCLUDE(CheckStructHasMember)


#################################################
# OS DEPENDENT
#################################################

SET(CMAKE_MACOSX_RPATH 1)

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
    ADD_DEFINITIONS("-D_DEFAULT_SOURCE -DLINUX")
    ADD_DEFINITIONS("-DHAVE_MPTCP")   # Any better solution for this?
    FIND_LIBRARY(SCTP_LIB sctp)

ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
    SET(CMAKE_REQUIRED_INCLUDES "/usr/local/include" "/usr/include")
    SET(CMAKE_LIBRARY_PATH "/usr/local/lib")
    INCLUDE_DIRECTORIES("/usr/local/include")
    ADD_DEFINITIONS("-DFreeBSD")

ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
    SET(CMAKE_REQUIRED_INCLUDES "/usr/local/include" "/usr/include" "/usr/local/opt/openssl/include")
    SET(CMAKE_LIBRARY_PATH "/usr/local/lib")
    INCLUDE_DIRECTORIES("/usr/local/include" "/usr/local/opt/openssl/include")
    ADD_DEFINITIONS("-DDARWIN")

ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
    MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
    LIST(APPEND neat_headers neat_bsd.h)
    LIST(APPEND neat_sources neat_bsd.c)
    SET(CMAKE_REQUIRED_INCLUDES "/usr/pkg/include" "/usr/include" "/usr/local/include")
    SET(CMAKE_LIBRARY_PATH "/usr/local/lib")
    INCLUDE_DIRECTORIES("/usr/pkg/include" "/usr/local/include")
    ADD_DEFINITIONS("-DNetBSD")

ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
    MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
    ADD_DEFINITIONS("-DSOLARIS")

ELSE()
    MESSAGE(FATAL_ERROR ${CMAKE_SYSTEM_NAME} " not supported (yet?)")

ENDIF()

find_package(Threads REQUIRED)


#################################################
# CHECK STRUCT MEMBERS
#################################################

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


#################################################
# CHECK OPTIONS
#################################################

# Debug build type as default
IF (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, using DEBUG")
    set(CMAKE_BUILD_TYPE "DEBUG")
    # _FORTIFY_SOURCE requires compiling with optimization
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O")
endif()


#################################################
# REQUIREMENTS
#################################################

OPTION(BUILD_TEST_PROGRAMS "Build test programs" 0)
OPTION(BUILD_PLOT_PROGRAMS "Build plot programs" 0)


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
include(CheckCSourceRuns)
check_c_source_runs("
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


#################################################
# COMPILER FLAGS
#################################################

SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -Wno-unused-function -Wno-unused-parameter -Wno-overlength-strings -Wno-array-bounds")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unused-function -Wno-unused-parameter -Wno-overlength-strings -Wno-array-bounds")

IF (CMAKE_BUILD_TYPE MATCHES DEBUG)
    MESSAGE(STATUS "Building in DEBUG mode")
    # _FORTIFY_SOURCE requires compiling with optimization
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O")
ENDIF()


#################################################
# SUBDIRECTORIES
#################################################

ADD_SUBDIRECTORY(src)
