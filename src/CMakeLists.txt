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

SET(CMAKE_INCLUDE_CURRENT_DIR ON)

#############################################################################
#### PROGRAMS                                                            ####
#############################################################################

ADD_EXECUTABLE(netperfmeter
netperfmeter.cc ext_socket.h netperfmeterpackets.h thread.cc thread.h mutex.cc mutex.h tools.h tools.cc messagereader.h messagereader.cc control.h control.cc transfer.h transfer.cc outputfile.h outputfile.cc flowtrafficspec.h flowtrafficspec.cc flowbandwidthstats.h flowbandwidthstats.cc flow.h flow.cc cpustatus.h cpustatus.cc measurement.h measurement.cc defragmenter.h defragmenter.cc)
TARGET_LINK_LIBRARIES(netperfmeter ${BZIP2_LIBRARIES} ${SCTP_LIB} "${CMAKE_THREAD_LIBS_INIT}")
INSTALL(TARGETS netperfmeter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES netperfmeter.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

ADD_EXECUTABLE(createsummary
createsummary.cc inputfile.h inputfile.cc outputfile.h outputfile.cc debug.h redblacktree.h redblacktree_impl.h simpleredblacktree.h simpleredblacktree.c)
TARGET_LINK_LIBRARIES(createsummary ${BZIP2_LIBRARIES} "${CMAKE_THREAD_LIBS_INIT}")
INSTALL(TARGETS createsummary RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES createsummary.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

ADD_EXECUTABLE(combinesummaries
combinesummaries.cc inputfile.h inputfile.cc outputfile.h outputfile.cc)
TARGET_LINK_LIBRARIES(combinesummaries ${BZIP2_LIBRARIES} "${CMAKE_THREAD_LIBS_INIT}")
INSTALL(TARGETS combinesummaries RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES combinesummaries.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

ADD_EXECUTABLE(extractvectors
extractvectors.cc inputfile.h inputfile.cc outputfile.h outputfile.cc)
TARGET_LINK_LIBRARIES(extractvectors ${BZIP2_LIBRARIES} "${CMAKE_THREAD_LIBS_INIT}")
INSTALL(TARGETS extractvectors RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES extractvectors.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

ADD_EXECUTABLE(runtimeestimator
runtimeestimator.cc)
TARGET_LINK_LIBRARIES(runtimeestimator)
INSTALL(TARGETS runtimeestimator RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES runtimeestimator.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

ADD_EXECUTABLE(getabstime
getabstime.cc)
TARGET_LINK_LIBRARIES(getabstime)
INSTALL(TARGETS getabstime RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES getabstime.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)


IF (BUILD_PLOT_PROGRAMS)
   INSTALL(FILES plot-netperfmeter-results setpdfmetadata pdfembedfonts
           DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES plotter.R plot-netperfmeter-results.R
           DESTINATION ${CMAKE_INSTALL_DATADIR}/netperfmeter)
   INSTALL(FILES plot-netperfmeter-results.1 setpdfmetadata.1 pdfembedfonts.1
           DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
ENDIF()

IF (BUILD_TEST_PROGRAMS)
   ADD_EXECUTABLE(rootshell rootshell.c)
   TARGET_LINK_LIBRARIES(rootshell)
ENDIF()
