include_directories(.)

#################################################
# PROGRAMS
#################################################

ADD_EXECUTABLE(netperfmeter
netperfmeter.cc ext_socket.h netperfmeterpackets.h thread.cc thread.h mutex.cc mutex.h tools.h tools.cc messagereader.h messagereader.cc control.h control.cc transfer.h transfer.cc outputfile.h outputfile.cc flowtrafficspec.h flowtrafficspec.cc flowbandwidthstats.h flowbandwidthstats.cc flow.h flow.cc cpustatus.h cpustatus.cc measurement.h measurement.cc defragmenter.h defragmenter.cc)
TARGET_LINK_LIBRARIES(netperfmeter ${BZIP2_LIBRARIES} ${SCTP_LIB} "${CMAKE_THREAD_LIBS_INIT}")
INSTALL(TARGETS netperfmeter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES netperfmeter.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man/man1)

ADD_EXECUTABLE(createsummary
createsummary.cc inputfile.h inputfile.cc outputfile.h outputfile.cc debug.h redblacktree.h redblacktree_impl.h simpleredblacktree.h simpleredblacktree.c)
TARGET_LINK_LIBRARIES(createsummary ${BZIP2_LIBRARIES} "${CMAKE_THREAD_LIBS_INIT}")
INSTALL(TARGETS createsummary RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES createsummary.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man/man1)

ADD_EXECUTABLE(combinesummaries
combinesummaries.cc inputfile.h inputfile.cc outputfile.h outputfile.cc)
TARGET_LINK_LIBRARIES(combinesummaries ${BZIP2_LIBRARIES} "${CMAKE_THREAD_LIBS_INIT}")
INSTALL(TARGETS combinesummaries RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES combinesummaries.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man/man1)

ADD_EXECUTABLE(extractvectors
extractvectors.cc inputfile.h inputfile.cc outputfile.h outputfile.cc)
TARGET_LINK_LIBRARIES(extractvectors ${BZIP2_LIBRARIES} "${CMAKE_THREAD_LIBS_INIT}")
INSTALL(TARGETS extractvectors RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES extractvectors.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man/man1)

ADD_EXECUTABLE(runtimeestimator
runtimeestimator.cc)
TARGET_LINK_LIBRARIES(runtimeestimator)
INSTALL(TARGETS runtimeestimator RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES runtimeestimator.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man/man1)

ADD_EXECUTABLE(getabstime
getabstime.cc)
TARGET_LINK_LIBRARIES(getabstime)
INSTALL(TARGETS getabstime RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES getabstime.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man/man1)


IF (BUILD_PLOT_PROGRAMS)
   INSTALL(FILES plot-netperfmeter-results pdfmetadata pdfembedfonts
           DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES plotter.R plot-netperfmeter-results.R
           DESTINATION ${CMAKE_INSTALL_DATADIR}/netperfmeter)
   INSTALL(FILES plot-netperfmeter-results.1 pdfmetadata.1 pdfembedfonts.1
           DESTINATION ${CMAKE_INSTALL_MANDIR}/man/man1)
ENDIF()

IF (BUILD_TEST_PROGRAMS)
   ADD_EXECUTABLE(rootshell
   rootshell.c)
   TARGET_LINK_LIBRARIES(rootshell)
ENDIF()
