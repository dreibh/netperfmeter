bin_PROGRAMS = netmeter

netmeter_SOURCES = netmeter.cc ext_socket.h netmeterpackets.h tools.h tools.cc flowspec.h flowspec.cc messagereader.h messagereader.cc control.h control.cc transfer.h transfer.cc statisticswriter.h statisticswriter.cc
netmeter_LDADD   = $(bz2_LIBS)


dist_bin_SCRIPTS = plot-netmeter-vectors


dist_man_MANS = netmeter.8 plot-netmeter-vectors.8


EXTRA_DIST = plotter.R plot-netmeter-vectors.R

pkgdata_DATA = plotter.R plot-netmeter-vectors.R
