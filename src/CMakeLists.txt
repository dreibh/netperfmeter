# ==========================================================================
#         _   _      _   ____            __ __  __      _
#        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
#        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
#        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
#        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
#
#                  NetPerfMeter -- Network Performance Meter
#                 Copyright (C) 2009-2025 by Thomas Dreibholz
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

SET(CMAKE_INCLUDE_CURRENT_DIR ON)


#############################################################################
#### VERSION FILE                                                        ####
#############################################################################

CONFIGURE_FILE (
   "${CMAKE_CURRENT_SOURCE_DIR}/package-version.h.in"
   "${CMAKE_CURRENT_BINARY_DIR}/package-version.h"
)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})


#############################################################################
#### PROGRAMS                                                            ####
#############################################################################

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
   outputfile.cc
   outputfile.h
   thread.cc
   thread.h
   tools.cc
   tools.h
   transfer.cc
   transfer.h
)
TARGET_LINK_LIBRARIES(netperfmeter ${BZ2_LIBRARY} ${SCTP_LIBRARY} ${QUIC_LIBRARY} "${CMAKE_THREAD_LIBS_INIT}")
INSTALL(TARGETS netperfmeter     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES   netperfmeter.1   DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
INSTALL(FILES   netperfmeter.xml DESTINATION         ${CMAKE_INSTALL_DATAROOTDIR}/mime/packages)
INSTALL(FILES   netperfmeter.bib DESTINATION         ${CMAKE_INSTALL_DATADIR}/netperfmeter)
INSTALL(FILES       netperfmeter.bash-completion
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME      netperfmeter)
IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
   INSTALL(FILES    netperfmeter.service               DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
   INSTALL(FILES    netperfmeter.conf                  DESTINATION /etc)
   INSTALL(FILES    netperfmeter-module-loader.service DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
   INSTALL(PROGRAMS netperfmeter-module-loader         DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES    netperfmeter-module-loader.1       DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
ENDIF()

# ====== Create Summary Tool ================================================
ADD_EXECUTABLE(createsummary
   createsummary.cc
   debug.h
   inputfile.cc
   inputfile.h
   outputfile.cc
   outputfile.h
   redblacktree.h
   redblacktree_impl.h
   simpleredblacktree.c
   simpleredblacktree.h
)
TARGET_LINK_LIBRARIES(createsummary ${BZ2_LIBRARY} "${CMAKE_THREAD_LIBS_INIT}")
INSTALL(TARGETS createsummary   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES   createsummary.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)

# ====== Combine Summaries Tool =============================================
ADD_EXECUTABLE(combinesummaries
   combinesummaries.cc
   inputfile.h
   inputfile.cc
   outputfile.h
   outputfile.cc
)
TARGET_LINK_LIBRARIES(combinesummaries ${BZ2_LIBRARY} "${CMAKE_THREAD_LIBS_INIT}")
INSTALL(TARGETS combinesummaries   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES   combinesummaries.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)

# ====== Extract Vectors Tool ===============================================
ADD_EXECUTABLE(extractvectors
   extractvectors.cc
   inputfile.h
   inputfile.cc
   outputfile.h
   outputfile.cc
)
TARGET_LINK_LIBRARIES(extractvectors ${BZ2_LIBRARY} "${CMAKE_THREAD_LIBS_INIT}")
INSTALL(TARGETS extractvectors  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES   extractvectors.1 DESTINATION        ${CMAKE_INSTALL_MANDIR}/man1)

# ====== Runtime Estimator Tool =============================================
ADD_EXECUTABLE(runtimeestimator runtimeestimator.cc)
TARGET_LINK_LIBRARIES(runtimeestimator)
INSTALL(TARGETS runtimeestimator   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES   runtimeestimator.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)

# ====== getabstime =========================================================
ADD_EXECUTABLE(getabstime getabstime.cc)
TARGET_LINK_LIBRARIES(getabstime)
INSTALL(TARGETS getabstime   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES   getabstime.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)

# ====== Plot Programs ======================================================
IF (WITH_PLOT_PROGRAMS)
   INSTALL(PROGRAMS plot-netperfmeter-results setpdfmetadata pdfembedfonts
           DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES plotter.R plot-netperfmeter-results.R
           DESTINATION ${CMAKE_INSTALL_DATADIR}/netperfmeter)
   INSTALL(FILES plot-netperfmeter-results.1 setpdfmetadata.1 pdfembedfonts.1
           DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
ENDIF()

# ====== Test Programs ======================================================
IF (WITH_TEST_PROGRAMS)
   ADD_EXECUTABLE(rootshell rootshell.c)
   TARGET_LINK_LIBRARIES(rootshell)
ENDIF()


# ADD_EXECUTABLE(t1 t1.cc assure.cc loglevel.cc tools.cc mutex.cc)
# TARGET_LINK_LIBRARIES(t1 ${SCTP_LIBRARY} "${CMAKE_THREAD_LIBS_INIT}")

ADD_EXECUTABLE(qs qs.cc assure.cc loglevel.cc tools.cc mutex.cc)
TARGET_LINK_LIBRARIES(qs ${SCTP_LIBRARY} ${QUIC_LIBRARY} "${CMAKE_THREAD_LIBS_INIT}")
ADD_EXECUTABLE(qc qc.cc assure.cc loglevel.cc tools.cc mutex.cc)
TARGET_LINK_LIBRARIES(qc ${SCTP_LIBRARY} ${QUIC_LIBRARY} "${CMAKE_THREAD_LIBS_INIT}")


#############################################################################
#### SUBDIRECTORIES                                                      ####
#############################################################################

ADD_SUBDIRECTORY(examples)
ADD_SUBDIRECTORY(logo)
ADD_SUBDIRECTORY(results-examples)
