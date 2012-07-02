if BUILD_PLOTTING_PROGRAMS
bin_PROGRAMS = netperfmeter createsummary combinesummaries extractvectors ldcalc runtimeestimator
else
bin_PROGRAMS = netperfmeter
endif


# ###### NetPerfMeter program ###############################################
netperfmeter_SOURCES = netperfmeter.cc ext_socket.h netperfmeterpackets.h thread.cc thread.h mutex.cc mutex.h tools.h tools.cc messagereader.h messagereader.cc control.h control.cc transfer.h transfer.cc outputfile.h outputfile.cc flowtrafficspec.h flowtrafficspec.cc flowbandwidthstats.h flowbandwidthstats.cc flow.h flow.cc cpustatus.h cpustatus.cc measurement.h measurement.cc defragmenter.h defragmenter.cc
netperfmeter_LDADD   = $(bz2_LIBS) $(socketapi_LIBS) $(sctplib_LIBS) -lpthread


# ###### Plotting programs ##################################################
dist_man_MANS = netperfmeter.1

if BUILD_PLOTTING_PROGRAMS

nodist_bin_SCRIPTS  = plot-netperfmeter-results pdfmetadata pdfembedfonts
nodist_man_MANS     = plot-netperfmeter-results.1 createsummary.1 combinesummaries.1 extractvectors.1 pdfmetadata.1 pdfembedfonts.1
nodist_EXTRA_DIST   = plotter.R plot-netperfmeter-results.R
# nodist_pkgdata_DATA = plotter.R plot-netperfmeter-results.R

nodist_createsummary_SOURCES = createsummary.cc inputfile.h inputfile.cc outputfile.h outputfile.cc debug.h redblacktree.h redblacktree_impl.h simpleredblacktree.h simpleredblacktree.c
createsummary_LDADD          = $(bz2_LIBS)

nodist_combinesummaries_SOURCES = combinesummaries.cc inputfile.h inputfile.cc outputfile.h outputfile.cc
combinesummaries_LDADD          = $(bz2_LIBS)

nodist_extractvectors_SOURCES = extractvectors.cc inputfile.h inputfile.cc outputfile.h outputfile.cc
extractvectors_LDADD          = $(bz2_LIBS)

nodist_ldcalc_SOURCES = ldcalc.cc
ldcalc_LDADD          =

nodist_runtimeestimator_SOURCES = runtimeestimator.cc
runtimeestimator_LDADD          =

#nodist_t6_SOURCES = t6.cc tools.cc outputfile.cc
#t6_LDADD          = $(bz2_LIBS) $(socketapi_LIBS) $(sctplib_LIBS) -lpthread

endif


# ###### Test programs ######################################################
if BUILD_TEST_PROGRAMS
noinst_PROGRAMS = rootshell

rootshell_SOURCES = rootshell.c
rootshell_LDADD   =
else
noinst_PROGRAMS =
endif
