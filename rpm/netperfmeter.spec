Name: netperfmeter
Version: 2.0.0~rc1.3
Release: 1
Summary: Network performance meter for the UDP, TCP, MPTCP, SCTP and DCCP protocols
License: GPL-3.0-or-later
Group: Applications/Internet
URL: https://www.nntb.no/~dreibh/netperfmeter/
Source: https://www.nntb.no/~dreibh/netperfmeter/download/%{name}-%{version}.tar.xz

AutoReqProv: on
BuildRequires: bzip2-devel
BuildRequires: cmake
BuildRequires: gcc-c++
BuildRequires: ghostscript
BuildRequires: GraphicsMagick
BuildRequires: lksctp-tools-devel
BuildRequires: pdf2svg
BuildRequires: valgrind-devel
BuildRoot: %{_tmppath}/%{name}-%{version}-build

Requires: %{name}-common = %{version}-%{release}
Recommends: %{name}-plotting = %{version}-%{release}
Recommends: bc
Recommends: hipercontracer
Recommends: iputils
Recommends: wireshark-cli
Recommends: subnetcalc >= 2.0.2
Suggests: %{name}-service = %{version}-%{release}
Suggests: dynmhs
Suggests: td-system-info
Suggests: traceroute


%description
NetPerfMeter is a network performance meter for the TCP, MPTCP, SCTP, UDP, and
DCCP transport protocols over IPv4 and IPv6. It simultaneously transmits
bidirectional flows to an endpoint and measures the resulting flow bandwidths
and QoS. Flows can be saturated (i.e. “send as much as possible”) or
non-saturated with frame rate and frame sizes (like a multimedia transmission).
Non-saturated flows can be configured with constant or variable frame rate/frame
size, i.e. to realise Constant Bit Rate (CBR) or Variable Bit Rate (VBR)
traffic. For both, frame rate and frame size, it is not only possible to set
constant values but to also to use random distributions. Furthermore, flows can
be set up as on/off flows. Of course, the flow parameters can be configured
individually per flow and flow direction. The measurement results can be
recorded as scalar files (summary of the run) and vector files (time series).
These files can be processed further, e.g. for detailed analysis and plotting of
the results. The Wireshark network protocol analyser provides out-of-the-box
support for analysing NetPerfMeter packet traffic.

%prep
%setup -q

%build
%cmake -DCMAKE_INSTALL_PREFIX=/usr . -DWITH_ICONS=ON -DWITH_PLOT_PROGRAMS=ON -DWITH_TEST_PROGRAMS=OFF
%cmake_build

%install
%cmake_install

%files
%{_bindir}/combinesummaries
%{_bindir}/createsummary
%{_bindir}/extractvectors
%{_bindir}/getabstime
%{_bindir}/netperfmeter
%{_bindir}/runtimeestimator
%{_datadir}/bash-completion/completions/netperfmeter
%{_mandir}/man1/combinesummaries.1.gz
%{_mandir}/man1/createsummary.1.gz
%{_mandir}/man1/extractvectors.1.gz
%{_mandir}/man1/getabstime.1.gz
%{_mandir}/man1/netperfmeter.1.gz
%{_mandir}/man1/runtimeestimator.1.gz


%package common
Summary: Network Performance Meter (common files)
Group: Applications/Internet
BuildArch: noarch

%description common
NetPerfMeter is a network performance meter for the TCP, MPTCP, SCTP, UDP,
and DCCP transport protocols over IPv4 and IPv6. It simultaneously transmits
bidirectional flows to an endpoint and measures the resulting flow bandwidths
and QoS. Flows can be saturated (i.e. “send as much as possible”) or
non-saturated with frame rate and frame sizes (like a multimedia
transmission). Non-saturated flows can be configured with constant or
variable frame rate/frame size, i.e. to realise Constant Bit Rate (CBR) or
Variable Bit Rate (VBR) traffic. For both, frame rate and frame size, it is
not only possible to set constant values but to also to use random
distributions. Furthermore, flows can be set up as on/off flows. Of course,
the flow parameters can be configured individually per flow and flow
direction. The measurement results can be recorded as scalar files (summary
of the run) and vector files (time series). These files can be processed
further, e.g. for detailed analysis and plotting of the results. The
Wireshark network protocol analyser provides out-of-the-box support for
analysing NetPerfMeter packet traffic.

%files common
%{_datadir}/icons/hicolor/*x*/apps/netperfmeter.png
%{_datadir}/icons/hicolor/scalable/apps/netperfmeter.svg
%{_datadir}/mime/packages/netperfmeter.xml
%{_datadir}/netperfmeter/netperfmeter.bib
%{_datadir}/netperfmeter/netperfmeter.pdf
%{_datadir}/netperfmeter/netperfmeter.png
%{_datadir}/netperfmeter/results-examples/*.sca*
%{_datadir}/netperfmeter/results-examples/*.vec*


%package service
Summary: Network Performance Meter (common files)
Group: Applications/Internet
BuildArch: noarch
Requires: %{name} = %{version}-%{release}
Suggests: hipercontracer
Suggests: hipercontracer-trigger
Suggests: td-system-tools

%description service
NetPerfMeter is a network performance meter for the UDP,
TCP, MPTCP, SCTP and DCCP transport protocols over IPv4 and IPv6.
It simultaneously transmits bidirectional flows to an endpoint
and measures the resulting flow bandwidths and QoS. The
results are written as vector and scalar files. The vector
files can e.g. be used to create plots of the results.
This package sets up a service running a NetPerfMeter
server instance.

%files service
/etc/netperfmeter/netperfmeter.conf
/lib/systemd/system/netperfmeter.service


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
* Sat Feb 10 2024 Thomas Dreibholz <thomas.dreibholz@gmail.com> - 1.9.7
- New upstream release.
* Mon Dec 18 2023 Thomas Dreibholz <thomas.dreibholz@gmail.com> - 1.9.6
- New upstream release.
* Wed Dec 06 2023 Thomas Dreibholz <thomas.dreibholz@gmail.com> - 1.9.5
- New upstream release.
* Sun Jan 22 2023 Thomas Dreibholz <thomas.dreibholz@gmail.com> - 1.9.4
- New upstream release.
* Sun Sep 11 2022 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.9.3
- New upstream release.
* Sun Nov 07 2021 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.9.2
- New upstream release.
* Mon Nov 01 2021 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.9.1
- New upstream release.
* Sat Mar 06 2021 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.9.0
- New upstream release.
* Fri Nov 13 2020 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.8.7
- New upstream release.
* Fri Feb 07 2020 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.8.6
- New upstream release.
* Tue Sep 10 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.8.5
- New upstream release.
* Wed Aug 07 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.8.4
- New upstream release.
* Wed Aug 07 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.8.4
- New upstream release.
* Fri Jul 26 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.8.3
- New upstream release.
* Mon Jul 22 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.8.2
- New upstream release.
* Tue May 21 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.8.1
- New upstream release.
* Thu May 18 2017 Thomas Dreibholz <dreibh@simula.no> 1.7.1
- Initial RPM release
