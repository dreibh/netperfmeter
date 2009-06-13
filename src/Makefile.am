bin_PROGRAMS = netperfmeter

netperfmeter_SOURCES = netperfmeter.cc ext_socket.h netperfmeterpackets.h tools.h tools.cc flowspec.h flowspec.cc messagereader.h messagereader.cc control.h control.cc transfer.h transfer.cc statisticswriter.h statisticswriter.cc
netperfmeter_LDADD   = $(bz2_LIBS)


dist_bin_SCRIPTS = plot-netperfmeter-vectors


dist_man_MANS = netperfmeter.8 plot-netperfmeter-vectors.8


EXTRA_DIST = plotter.R plot-netperfmeter-vectors.R

pkgdata_DATA = plotter.R plot-netperfmeter-vectors.R
