<p align="center">
 <a href="https://www.nntb.no/~dreibh/netperfmeter/">
 <img alt="NetPerfMeter Logo" src="https://www.nntb.no/~dreibh/netperfmeter/EN-Logo-NetPerfMeter.svg" width="25%" /><br />
 https://www.nntb.no/~dreibh/netperfmeter/
 </a>
</p>

# What is Network Performance Meter&nbsp;(NetPerfMeter)?

NetPerfMeter is a network performance meter for the UDP, TCP, MPTCP, SCTP and DCCP transport protocols over IPv4 and IPv6. It simultaneously transmits bidirectional flows to an endpoint and measures the resulting flow bandwidths and QoS. The results are written as vector and scalar files. The vector files can e.g.&nbsp;be used to create plots of the results.


# Installation

## Binary Package Installation

Please use the issue tracker at [https://github.com/dreibh/netperfmeter/issues](https://github.com/dreibh/netperfmeter/issues) to report bugs and issues!

### Ubuntu Linux

For ready-to-install Ubuntu Linux packages of NetPerfMeter, see [Launchpad PPA for Thomas Dreibholz](https://launchpad.net/~dreibh/+archive/ubuntu/ppa/+packages?field.name_filter=netperfmeter&field.status_filter=published&field.series_filter=)!

```
sudo apt-add-repository -sy ppa:dreibh/ppa
sudo apt-get update
sudo apt-get install netperfmeter
```

### Fedora Linux

For ready-to-install Fedora Linux packages of NetPerfMeter, see [COPR PPA for Thomas Dreibholz](https://copr.fedorainfracloud.org/coprs/dreibh/ppa/package/netperfmeter/)!

```
sudo dnf copr enable -y dreibh/ppa
sudo dnf install netperfmeter
```

### FreeBSD

For ready-to-install FreeBSD packages of NetPerfMeter, it is included in the ports collection, see [FreeBSD ports tree index of benchmarks/netperfmeter/](https://cgit.freebsd.org/ports/tree/benchmarks/netperfmeter/)!

```
pkg install netperfmeter
```

Alternatively, to compile it from the ports sources:

```
cd /usr/ports/benchmarks/netperfmeter
make
make install
```

## Sources Download

NetPerfMeter is released under the GNU General Public Licence (GPL).

Please use the issue tracker at [https://github.com/dreibh/netperfmeter/issues](https://github.com/dreibh/netperfmeter/issues) to report bugs and issues!

### Development Version

The Git repository of the NetPerfMeter sources can be found at [https://github.com/dreibh/netperfmeter](https://github.com/dreibh/netperfmeter):

```
git clone https://github.com/dreibh/netperfmeter
cd netperfmeter
cmake .
make
```

Contributions:

- Issue tracker: [https://github.com/dreibh/netperfmeter/issues](https://github.com/dreibh/netperfmeter/issues).
  Please submit bug reports, issues, questions, etc. in the issue tracker!

- Pull Requests for NetPerfMeter: [https://github.com/dreibh/netperfmeter/pulls](https://github.com/dreibh/netperfmeter/pulls).
  Your contributions to NetPerfMeter are always welcome!

- CI build tests of NetPerfMeter: [https://github.com/dreibh/netperfmeter/actions](https://github.com/dreibh/netperfmeter/actions).

- Coverity Scan analysis of NetPerfMeter: [https://scan.coverity.com/projects/dreibh-netperfmeter](https://scan.coverity.com/projects/dreibh-netperfmeter).

### Current Stable Release

See [https://www.nntb.no/~dreibh/netperfmeter/#StableRelease](https://www.nntb.no/~dreibh/netperfmeter/#StableRelease)!


# Citing NetPerfMeter in Publications

NetPerfMeter BibTeX entries can be found in [src/netperfmeter.bib](src/netperfmeter.bib)!

1. Dreibholz, Thomas; Becke, Martin; Adhari, Hakim; Rathgeb, Erwin Paul: [Evaluation of A New Multipath Congestion Control Scheme using the NetPerfMeter Tool-Chain](https://www.wiwi.uni-due.de/fileadmin/fileupload/I-TDR/SCTP/Paper/SoftCOM2011.pdf), in Proceedings of the 19th IEEE International Conference on Software, Telecommunications and Computer Networks (SoftCOM), pp. 1–6, ISBN 978-953-290-027-9, Hvar, Dalmacija/Croatia, September 16, 2011.
2. Dreibholz, Thomas: [Evaluation and Optimisation of Multi-Path Transport using the Stream Control Transmission Protocol](https://duepublico2.uni-due.de/servlets/MCRFileNodeServlet/duepublico_derivate_00029737/Dre2012_final.pdf), University of Duisburg-Essen, Faculty of Economics, Institute for Computer Science and Business Information Systems, URN: urn:nbn:de:hbz:464-20120315-103208-1, March 13, 2012.
