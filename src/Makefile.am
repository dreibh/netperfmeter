bin_PROGRAMS = netperfmeter createsummary combinesummaries extractvectors runtimeestimator


# ###### NetPerfMeter program ###############################################
netperfmeter_SOURCES = netperfmeter.cc ext_socket.h netperfmeterpackets.h thread.cc thread.h mutex.cc mutex.h tools.h tools.cc messagereader.h messagereader.cc control.h control.cc transfer.h transfer.cc outputfile.h outputfile.cc flowtrafficspec.h flowtrafficspec.cc flowbandwidthstats.h flowbandwidthstats.cc flow.h flow.cc cpustatus.h cpustatus.cc measurement.h measurement.cc defragmenter.h defragmenter.cc
netperfmeter_LDADD   = $(bz2_LIBS) $(socketapi_LIBS) $(sctplib_LIBS) -lpthread -lm


# ###### Plotting programs ##################################################
dist_man_MANS     = netperfmeter.1 plot-netperfmeter-results.1 createsummary.1 combinesummaries.1 extractvectors.1 pdfmetadata.1 pdfembedfonts.1 runtimeestimator.1
dist_bin_SCRIPTS  = plot-netperfmeter-results pdfmetadata pdfembedfonts
dist_pkgdata_DATA = plotter.R plot-netperfmeter-results.R

createsummary_SOURCES = createsummary.cc inputfile.h inputfile.cc outputfile.h outputfile.cc debug.h redblacktree.h redblacktree_impl.h simpleredblacktree.h simpleredblacktree.c
createsummary_LDADD   = $(bz2_LIBS)

combinesummaries_SOURCES = combinesummaries.cc inputfile.h inputfile.cc outputfile.h outputfile.cc
combinesummaries_LDADD   = $(bz2_LIBS)

extractvectors_SOURCES = extractvectors.cc inputfile.h inputfile.cc outputfile.h outputfile.cc
extractvectors_LDADD   = $(bz2_LIBS)

runtimeestimator_SOURCES = runtimeestimator.cc
runtimeestimator_LDADD   =

# nodist_t6_SOURCES = t6.cc tools.cc outputfile.cc
# t6_LDADD          = $(bz2_LIBS) $(socketapi_LIBS) $(sctplib_LIBS) -lpthread


# ###### Test programs ######################################################
if BUILD_TEST_PROGRAMS
noinst_PROGRAMS = rootshell

rootshell_SOURCES = rootshell.c
rootshell_LDADD   =
else
noinst_PROGRAMS =
endif
