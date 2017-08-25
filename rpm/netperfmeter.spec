Name: netperfmeter
Version: 1.7.5
Release: 1
Summary: Network performance meter for the UDP, TCP, MPTCP, SCTP and DCCP protocols
License: GPL-3.0
Group: Applications/Internet
URL: https://www.uni-due.de/~be0001/netperfmeter/
Source: https://www.uni-due.de/~be0001/netperfmeter/download/%{name}-%{version}.tar.gz

AutoReqProv: on
BuildRequires: cmake
BuildRequires: gcc-c++
BuildRequires: libtool
BuildRequires: lksctp-tools-devel
BuildRequires: valgrind-devel
BuildRequires: bzip2-devel
BuildRoot: %{_tmppath}/%{name}-%{version}-build

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
%defattr(-,root,root,-)
%{_bindir}/combinesummaries
%{_bindir}/createsummary
%{_bindir}/extractvectors
%{_bindir}/getabstime
%{_bindir}/netperfmeter
%{_bindir}/pdfembedfonts
%{_bindir}/pdfmetadata
%{_bindir}/plot-netperfmeter-results
%{_bindir}/runtimeestimator
%{_datadir}/man/man1/combinesummaries.1.gz
%{_datadir}/man/man1/createsummary.1.gz
%{_datadir}/man/man1/extractvectors.1.gz
%{_datadir}/man/man1/getabstime.1.gz
%{_datadir}/man/man1/netperfmeter.1.gz
%{_datadir}/man/man1/pdfembedfonts.1.gz
%{_datadir}/man/man1/pdfmetadata.1.gz
%{_datadir}/man/man1/plot-netperfmeter-results.1.gz
%{_datadir}/man/man1/runtimeestimator.1.gz
%{_datadir}/netperfmeter/plot-netperfmeter-results.R
%{_datadir}/netperfmeter/plotter.R

%changelog
* Thu May 18 2017 Thomas Dreibholz <dreibh@simula.no> 1.7.1
- Initial RPM release
