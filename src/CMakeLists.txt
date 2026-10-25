# ==========================================================================
#         _   _      _   ____            __ __  __      _
#        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
#        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
#        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
#        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
#
#                  NetPerfMeter -- Network Performance Meter
#                 Copyright (C) 2009-2026 by Thomas Dreibholz
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


#############################################################################
#### VERSION FILE                                                        ####
#############################################################################

CONFIGURE_FILE (
   "${CMAKE_CURRENT_SOURCE_DIR}/package-version.h.in"
   "${CMAKE_CURRENT_BINARY_DIR}/package-version.h"
)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})


#############################################################################
#### LIBRARIES                                                           ####
#############################################################################

# ====== libfileio ==========================================================
LIST(APPEND libfileio_headers
   fileio.h
   inputfile.h
   outputfile.h
)
LIST(APPEND libfileio_sources
   fileio.cc
   inputfile.cc
   outputfile.cc
)
INSTALL(FILES       ${libfileio_headers}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fileio)

# --- Assemble library dependencies --------------------------------------
SET(libfileio_dependencies
   ${ZSTD_LIBRARY}
   ${LZMA_LIBRARY}
   ${BZ2_LIBRARY}
   ${Z_LIBRARY}
)

# --- Library targets ----------------------------------------------------
IF (WITH_STATIC_LIBRARIES)
   ADD_LIBRARY(libfileio-static STATIC ${libfileio_sources})
ENDIF()
IF (WITH_SHARED_LIBRARIES)
   ADD_LIBRARY(libfileio-shared SHARED ${libfileio_sources})
ENDIF()

FOREACH(TYPE shared;static)
   IF ( (("${TYPE}" STREQUAL "static") AND (WITH_STATIC_LIBRARIES)) OR
        (("${TYPE}" STREQUAL "shared") AND (WITH_SHARED_LIBRARIES)) )
      SET_TARGET_PROPERTIES(libfileio-${TYPE} PROPERTIES
         OUTPUT_NAME                fileio
         CLEAN_DIRECT_OUTPUT        1
         VERSION                    ${PROJECT_VERSION}
         SOVERSION                  ${PROJECT_VERSION_MAJOR}
         WINDOWS_EXPORT_ALL_SYMBOLS ON
      )
      TARGET_INCLUDE_DIRECTORIES(libfileio-${TYPE}
                                 PRIVATE ${ZSTD_INCLUDE_DIR}
                                         ${LZMA_INCLUDE_DIR}
                                         ${BZ2_INCLUDE_DIR}
                                         ${Z_INCLUDE_DIR})
      TARGET_LINK_LIBRARIES(libfileio-${TYPE}
                            PRIVATE ${libfileio_dependencies})
      INSTALL(TARGETS             libfileio-${TYPE}
              RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
              ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
              LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
   ENDIF()
ENDFOREACH()

# --- Generate pkg-config metadata ------------------------------------------
SET(PC_LIBS_PRIVATE "")
FOREACH(dependency ${libfileio_dependencies})
   IF (dependency MATCHES "^-")
      STRING(APPEND PC_LIBS_PRIVATE " ${dependency}")
   ELSEIF (IS_ABSOLUTE "${dependency}")
      # Strip path, extension (.so, .a, .dylib), and leading "lib" to yield -l<name>
      GET_FILENAME_COMPONENT(dep_name "${dependency}" NAME_WE)
      STRING(REGEX REPLACE "^lib" "" dep_name "${dep_name}")
      STRING(APPEND PC_LIBS_PRIVATE " -l${dep_name}")
   ELSE()
      STRING(APPEND PC_LIBS_PRIVATE " -l${dependency}")
   ENDIF()
ENDFOREACH()
STRING(STRIP "${PC_LIBS_PRIVATE}" PC_LIBS_PRIVATE)

CONFIGURE_FILE(
   ${CMAKE_CURRENT_SOURCE_DIR}/libfileio.pc.in
   ${CMAKE_CURRENT_BINARY_DIR}/libfileio.pc
   @ONLY
)
INSTALL(FILES       ${CMAKE_CURRENT_BINARY_DIR}/libfileio.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)


#############################################################################
#### PROGRAMS                                                            ####
#############################################################################

IF (STATIC_BUILD)
   SET(libraryType static)
ELSE()
   SET(libraryType shared)
ENDIF()

# ====== NetPerfMeter =======================================================
ADD_EXECUTABLE(netperfmeter
   assure.h
   assure.cc
   control.cc
   control.h
   cpustatus.cc
   cpustatus.h
   defragmenter.cc
   defragmenter.h
   ext_socket.h
   flowbandwidthstats.cc
   flowbandwidthstats.h
   flow.cc
   flow.h
   flowmanager.cc
   flowmanager.h
   flowtrafficspec.cc
   flowtrafficspec.h
   loglevel.cc
   loglevel.h
   measurement.cc
   measurement.h
   messagereader.cc
   messagereader.h
   mutex.cc
   mutex.h
   netperfmeter.cc
   netperfmeterpackets.h
   thread.cc
   thread.h
   tools.cc
   tools.h
   transfer.cc
   transfer.h
)
TARGET_INCLUDE_DIRECTORIES(netperfmeter
                           PRIVATE ${SCTP_INCLUDE_DIR}
                                   ${QUIC_INCLUDE_DIR}
                                   ${Intl_INCLUDE_DIRS}
                                   ${LIBIBERTY_INCLUDE_DIR})
TARGET_LINK_LIBRARIES(netperfmeter
                      PRIVATE libfileio-${libraryType}
                              ${SCTP_LIBRARY}
                              ${QUIC_LIBRARY}
                              ${Intl_LIBRARIES}
                              ${LIBIBERTY_LIBRARY}
                              ${KSTAT_LIBRARY}
                              ${SOCKET_LIBRARY}
                              ${CMAKE_THREAD_LIBS_INIT})
INSTALL(TARGETS netperfmeter     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES   netperfmeter.1   DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
INSTALL(FILES   netperfmeter.xml DESTINATION         ${CMAKE_INSTALL_DATAROOTDIR}/mime/packages)
INSTALL(FILES   netperfmeter.bib DESTINATION         ${CMAKE_INSTALL_DATADIR}/netperfmeter)
INSTALL(FILES       netperfmeter.bash-completion
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME      netperfmeter)
IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
   INSTALL(FILES    netperfmeter.service               DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
   INSTALL(FILES    netperfmeter.conf                  DESTINATION ${CMAKE_INSTALL_SYSCONFDIR})
ELSEIF ( (CMAKE_SYSTEM_NAME MATCHES "FreeBSD") OR
         (CMAKE_SYSTEM_NAME MATCHES "NetBSD") )
   INSTALL(PROGRAMS netperfmeter.rc                    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/rc.d
           RENAME   netperfmeter)
ENDIF()

CONFIGURE_FILE(netperfmeter-module-loader netperfmeter-module-loader.versioned @ONLY)
SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES netperfmeter-module-loader.versioned)
INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/netperfmeter-module-loader.versioned
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME      netperfmeter-module-loader)
INSTALL(FILES       netperfmeter-module-loader.1       DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
INSTALL(FILES       netperfmeter-module-loader.bash-completion
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME      netperfmeter-module-loader)
IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
   INSTALL(FILES    netperfmeter-module-loader.service DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
ELSEIF ( (CMAKE_SYSTEM_NAME MATCHES "FreeBSD") OR
         (CMAKE_SYSTEM_NAME MATCHES "NetBSD") )
   INSTALL(PROGRAMS netperfmeter-module-loader.rc      DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/rc.d
           RENAME   netperfmeter-module-loader)
ENDIF()

# ====== Create Summary Tool ================================================
ADD_EXECUTABLE(createsummary
   createsummary.cc
   debug.h
)
TARGET_INCLUDE_DIRECTORIES(createsummary
                           PRIVATE ${LIBIBERTY_INCLUDE_DIR})
TARGET_LINK_LIBRARIES(createsummary
                      PRIVATE libfileio-${libraryType}
                              ${LIBIBERTY_LIBRARY})
INSTALL(TARGETS     createsummary   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES       createsummary.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
INSTALL(FILES       createsummary.bash-completion
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME      createsummary)

# ====== Combine Summaries Tool =============================================
ADD_EXECUTABLE(combinesummaries
   combinesummaries.cc
)
TARGET_INCLUDE_DIRECTORIES(combinesummaries
                           PRIVATE ${LIBIBERTY_INCLUDE_DIR})
TARGET_LINK_LIBRARIES(combinesummaries
                      PRIVATE libfileio-${libraryType}
                              ${LIBIBERTY_LIBRARY})
INSTALL(TARGETS     combinesummaries   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES       combinesummaries.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
INSTALL(FILES       combinesummaries.bash-completion
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME      combinesummaries)

# ====== Extract Vectors Tool ===============================================
ADD_EXECUTABLE(extractvectors
   extractvectors.cc
)
TARGET_INCLUDE_DIRECTORIES(extractvectors
                           PRIVATE ${LIBIBERTY_INCLUDE_DIR})
TARGET_LINK_LIBRARIES(extractvectors
                      PRIVATE libfileio-${libraryType}
                              ${LIBIBERTY_LIBRARY})
INSTALL(TARGETS     extractvectors  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES       extractvectors.1 DESTINATION        ${CMAKE_INSTALL_MANDIR}/man1)
INSTALL(FILES       extractvectors.bash-completion
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME      extractvectors)

# ====== Runtime Estimator Tool =============================================
ADD_EXECUTABLE(runtimeestimator runtimeestimator.cc)
TARGET_INCLUDE_DIRECTORIES(runtimeestimator PRIVATE ${LIBIBERTY_INCLUDE_DIR})
TARGET_LINK_LIBRARIES(runtimeestimator PRIVATE ${LIBIBERTY_LIBRARY})
INSTALL(TARGETS runtimeestimator   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES   runtimeestimator.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
INSTALL(FILES       runtimeestimator.bash-completion
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME      runtimeestimator)

# ====== getabstime =========================================================
ADD_EXECUTABLE(getabstime getabstime.cc)
TARGET_INCLUDE_DIRECTORIES(getabstime PRIVATE ${LIBIBERTY_INCLUDE_DIR})
TARGET_LINK_LIBRARIES(getabstime PRIVATE ${LIBIBERTY_LIBRARY})
INSTALL(TARGETS getabstime   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES   getabstime.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
INSTALL(FILES       getabstime.bash-completion
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME      getabstime)

# ====== Plot Programs ======================================================
IF (WITH_PLOT_PROGRAMS)
   CONFIGURE_FILE(plot-netperfmeter-results plot-netperfmeter-results.versioned @ONLY)
   SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES plot-netperfmeter-results.versioned)
   INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/plot-netperfmeter-results.versioned
           DESTINATION ${CMAKE_INSTALL_BINDIR}
           RENAME      plot-netperfmeter-results)
   INSTALL(FILES       plot-netperfmeter-results.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       plot-netperfmeter-results.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      plot-netperfmeter-results)

   CONFIGURE_FILE(pdfembedfonts pdfembedfonts.versioned @ONLY)
   SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES pdfembedfonts.versioned)
   INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/pdfembedfonts.versioned
           DESTINATION ${CMAKE_INSTALL_BINDIR}
           RENAME      pdfembedfonts)
   INSTALL(FILES       pdfembedfonts.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       pdfembedfonts.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      pdfembedfonts)


   CONFIGURE_FILE(setpdfmetadata setpdfmetadata.versioned @ONLY)
   SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES setpdfmetadata.versioned)
   INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/setpdfmetadata.versioned
           DESTINATION ${CMAKE_INSTALL_BINDIR}
           RENAME      setpdfmetadata)
   INSTALL(FILES       setpdfmetadata.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       setpdfmetadata.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      setpdfmetadata)

   INSTALL(PROGRAMS plot-netperfmeter-results.R
           DESTINATION ${CMAKE_INSTALL_DATADIR}/netperfmeter)
   INSTALL(FILES plotter.R
           DESTINATION ${CMAKE_INSTALL_DATADIR}/netperfmeter)
ENDIF()


#############################################################################
#### SUBDIRECTORIES                                                      ####
#############################################################################

ADD_SUBDIRECTORY(examples)
ADD_SUBDIRECTORY(logo)
ADD_SUBDIRECTORY(results-examples)


#############################################################################
#### TESTING                                                             ####
#############################################################################

IF (BUILD_TESTING)
   ADD_SUBDIRECTORY(tests)
ENDIF()
