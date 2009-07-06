bin_PROGRAMS = netperfmeter createsummary combinesummaries t4

createsummary_SOURCES = createsummary.cc redblacktree.h redblacktree_impl.h simpleredblacktree.h simpleredblacktree.c debug.h
createsummary_LDADD   = $(bz2_LIBS)

combinesummaries_SOURCES = combinesummaries.cc
combinesummaries_LDADD   = $(bz2_LIBS)

netperfmeter_SOURCES = netperfmeter.cc ext_socket.h netperfmeterpackets.h thread.cc thread.h mutex.cc mutex.h tools.h tools.cc flowspec.h flowspec.cc messagereader.h messagereader.cc control.h control.cc transfer.h transfer.cc statisticswriter.h statisticswriter.cc
netperfmeter_LDADD   = $(bz2_LIBS) -lpthread


t4_SOURCES = t4.cc t3.cc t3.h text_socket.h netperfmeterpackets.h thread.cc thread.h mutex.cc mutex.h tools.h tools.cc flowspec.h flowspec.cc messagereader.h messagereader.cc control.h control.cc transfer.h transfer.cc statisticswriter.h statisticswriter.cc
t4_LDADD   = $(bz2_LIBS) -lpthread


dist_bin_SCRIPTS = plot-netperfmeter-results pdfmetadata pdfembedfonts


dist_man_MANS = netperfmeter.8 plot-netperfmeter-results.8 createsummary.8 combinesummaries.8 pdfmetadata.8 pdfembedfonts.8


EXTRA_DIST = plotter.R plot-netperfmeter-results.R

pkgdata_DATA = plotter.R plot-netperfmeter-results.R
