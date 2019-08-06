Name: netperfmeter
Version: 1.8.4~rc1.0
Release: 1
Summary: Network performance meter for the UDP, TCP, MPTCP, SCTP and DCCP protocols
License: GPL-3.0
Group: Applications/Internet
URL: https://www.uni-due.de/~be0001/netperfmeter/
Source: https://www.uni-due.de/~be0001/netperfmeter/download/%{name}-%{version}.tar.xz

AutoReqProv: on
BuildRequires: bzip2-devel
BuildRequires: cmake
BuildRequires: gcc-c++
BuildRequires: lksctp-tools-devel
BuildRequires: valgrind-devel
BuildRoot: %{_tmppath}/%{name}-%{version}-build

Recommends: %{name}-plotting = %{version}-%{release}
Recommends: bc
Recommends: hipercontracer
Recommends: iputils
Recommends: subnetcalc >= 2.0.2


%description
NetPerfMeter is a network performance meter for the UDP, TCP, SCTP and DCCP transport protocols over IPv4 and IPv6. It simultaneously transmits bidirectional flows to an endpoint and measures the resulting flow bandwidths and QoS. The results are written as vector and scalar files. The vector files can e.g. be used to create plots of the results.

%prep
%setup -q

%build
%cmake -DCMAKE_INSTALL_PREFIX=/usr -DWITH_NEAT=0 -DBUILD_TEST_PROGRAMS=1 -DBUILD_PLOT_PROGRAMS=1 .
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%clean
rm -rf "$RPM_BUILD_ROOT"

%files
%{_bindir}/combinesummaries
%{_bindir}/createsummary
%{_bindir}/extractvectors
%{_bindir}/getabstime
%{_bindir}/netperfmeter
%{_bindir}/runtimeestimator
%{_mandir}/man1/combinesummaries.1.gz
%{_mandir}/man1/createsummary.1.gz
%{_mandir}/man1/extractvectors.1.gz
%{_mandir}/man1/getabstime.1.gz
%{_mandir}/man1/netperfmeter.1.gz
%{_mandir}/man1/runtimeestimator.1.gz


%package pdfproctools
Summary: PDF Processing Tools
Group: Applications/Internet
BuildArch: noarch
Requires: ghostscript
Requires: perl >= 5.8.0
Requires: perl-PDF-API2
Requires: qpdf

%description pdfproctools
This package contains tools for PDF file processing.
SetPDFMetadata updates the metadata of a PDF file. In particular, it can be
used to add outlines (bookmarks) to a document. Furthermore, it can set the
document properties (e.g. author, title, keywords, creator, producer).
PDFEmbedFonts embeds all referenced fonts into a PDF file. Optionally, it
can also linearize the PDF file for online publication
("fast web view", "optimized").

%files pdfproctools
%{_bindir}/pdfembedfonts
%{_bindir}/setpdfmetadata
%{_mandir}/man1/pdfembedfonts.1.gz
%{_mandir}/man1/setpdfmetadata.1.gz


%package plotting
Summary: Network Performance Meter (plotting program)
Group: Applications/Internet
BuildArch: noarch
Requires: %{name} = %{version}-%{release}
Requires: %{name}-pdfproctools = %{version}-%{release}
Requires: R-core

%description plotting
NetPerfMeter is a network performance meter for the UDP,
TCP, MPTCP, SCTP and DCCP transport protocols over IPv4 and IPv6.
It simultaneously transmits bidirectional flows to an endpoint
and measures the resulting flow bandwidths and QoS. The
results are written as vector and scalar files. The vector
files can e.g. be used to create plots of the results.
This package contains a plotting program for the results.

%files plotting
%{_bindir}/plot-netperfmeter-results
%{_datadir}/netperfmeter/plot-netperfmeter-results.R
%{_datadir}/netperfmeter/plotter.R
%{_mandir}/man1/plot-netperfmeter-results.1.gz


%changelog
* Fri Jul 26 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.8.3
- New upstream release.
* Mon Jul 22 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.8.2
- New upstream release.
* Tue May 21 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.8.1
- New upstream release.
* Thu May 18 2017 Thomas Dreibholz <dreibh@simula.no> 1.7.1
- Initial RPM release
