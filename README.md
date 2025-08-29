<p align="center">
 <a href="https://www.nntb.no/~dreibh/netperfmeter/">
 <img alt="NetPerfMeter Logo" src="https://www.nntb.no/~dreibh/netperfmeter/EN-Logo-NetPerfMeter.svg" width="25%" /><br />
 https://www.nntb.no/~dreibh/netperfmeter/
 </a>
</p>


# What is Network Performance Meter&nbsp;(NetPerfMeter)?

NetPerfMeter is a network performance meter for the TCP, MPTCP, UDP, DCCP and SCTP transport protocols over IPv4 and IPv6. It simultaneously transmits bidirectional flows to an endpoint and measures the resulting flow bandwidths and QoS. The results can be written as scalar files (summary of the run) and vector files (details per frame). These files can be processed further, e.g.&nbsp;for analysis and plotting of the results.

<p class="center">
 <a href="screenshot-mptcp.webp"><img alt="Screenshot of NetPerfMeter run" src="screenshot-mptcp.webp" width="75%" /></a><br />
 A NetPerfMeter Run
</p>


## Design Goals and Features##

The key goal of NetPerfMeter is to provide a tool for the performance comparison of multiple transport connections, which are further denoted as *Flows*. That is, it is possible to configure different flows between two systems using varying parameters, in order run a configured measurement, collect the obtained results and post-process them for statistical analyses. Particularly, all five relevant IETF Transport Layer protocols are supported:

   * [UDP](https://en.wikipedia.org/wiki/User_Datagram_Protocol) (User Datagram Protocol; see [RFC&nbsp;768](https://www.rfc-editor.org/rfc/rfc768.html)),
   * [DCCP](https://en.wikipedia.org/wiki/DCCP) (Datagram Congestion Control Protocol; see [RFC&nbsp;4340](https://www.rfc-editor.org/rfc/rfc4340.html)),
   * [TCP](https://en.wikipedia.org/wiki/Transmission_Control_Protocol) (Transmission Control Protocol; see [RFC&nbsp;793](https://www.rfc-editor.org/rfc/rfc793.html)),
   * [MPTCP](https://en.wikipedia.org/wiki/MPTCP) (Multipath TCP; see [RFC&nbsp;8684](https://www.rfc-editor.org/rfc/rfc8684.html)),
   * [SCTP](https://en.wikipedia.org/wiki/Stream_Control_Transmission_Protocol) (Stream Control Transmission Protocol; see [RFC&nbsp;9260](https://www.rfc-editor.org/rfc/rfc9260.html)).

Of course, this support includes the possibility to parametrise various protocol-specific options. Note, that the protocol support by NetPerfMeter depends on the underlying operating system. DCCP, MPTCP, as well as some SCTP extensions are not available on all platforms, yet.

Furthermore, each flow is able to apply its specific traffic behaviour:

   * Each flow may use its own Transport Layer protocol (i.e.&nbsp;UDP, DCCP, TCP, MTCP or SCTP).
   * Bidirectional data transfer is possible, with individual parameters for each direction.
   * Flows may either be saturated (i.e.&nbsp;try to send as much as possible) or non-saturated. In the latter case, a frame rate and a frame size have to be configured. Both may be distributed randomly, using a certain distribution (like uniform, negative exponential, etc.). This feature allows to mimic multimedia traffic.
   * For the stream-oriented SCTP, an independent traffic configuration is possible for each stream.
   * Support for on-off traffic is provided by allowing to specify a sequence of time stamps when to start, stop and restart a flow or stream.
   * Also, for SCTP, it is possible to configure partial reliability (see [RFC&nbsp;3758](https://www.rfc-editor.org/rfc/rfc3758.html)) as well as ordered and unordered delivery (see [RFC&nbsp;9260](https://www.rfc-editor.org/rfc/rfc9260.html)).

Clearly, the NetPerfMeter application provides features similar to the [NetPerfMeter simulation model in OMNeT++](https://doc.omnetpp.org/inet/api-4.4.0/neddoc/inet.applications.netperfmeter.NetPerfMeter.html). It is therefore relatively easy – from the parametrisation perspective – to reproduce NetPerfMeter simulation scenarios in reality.


## Instances and Protocols

<p align="center">
 <a href="src/figures/EN-NetPerfMeter-MeasurementScenario.svg"><img alt="Figure of the Concept of a NetPerfMeter Measurement" src="src/figures/EN-NetPerfMeter-MeasurementScenario.svg" width="512pt" /></a><br />
 The Concept of a NetPerfMeter Measurement
</p>

Similar to the [NetPerfMeter simulation model in OMNeT++](https://doc.omnetpp.org/inet/api-4.4.0/neddoc/inet.applications.netperfmeter.NetPerfMeter.html), an application instance may either be in *Active Mode* (client side) or *Passive Mode* (server side). The figure above illustrates the concept of a NetPerfMeter measurement. The passive instance accepts incoming NetPerfMeter connections from the active instance. The active instance controls the passive instance, by using a control protocol denoted as NetPerfMeter Control Protocol&nbsp;(NPMP-CONTROL). That is, the passive instance may run as a daemon; no manual interaction by the user – e.g.&nbsp;to restart it before a new measurement run – is required. This feature is highly practical for a setup distributed over multiple Internet sites (e.g.&nbsp;like the [NorNet Testbed](https://www.nntb.no/)) and allows for parameter studies consisting of many measurement runs. The payload data between active and passive instances is transported using the NetPerfMeter Data Protocol&nbsp;(NPMP-DATA). The figure below shows the protocol stack of a NetPerfMeter node.

<p align="center">
 <a href="src/figures/EN-NetPerfMeter-ProtocolStack.svg"><img alt="Figure of the NetPerfMeter Protocol Stack" src="src/figures/EN-NetPerfMeter-ProtocolStack.svg" width="384pt" /></a><br />
 The NetPerfMeter Protocol Stack
</p>


## Measurement Processing

The following figure presents the message sequence of a NetPerfMeter measurement run:

<p align="center">
 <a href="src/figures/EN-NetPerfMeter-Measurement-SeqDiagram.svg"><img alt="Figure of a Measurement Run with NetPerfMeter" src="src/figures/EN-NetPerfMeter-Measurement-SeqDiagram.svg" width="512pt" /></a><br />
 A Measurement Run with NetPerfMeter
</p>

Note that the [Wireshark](https://www.wireshark.org/) network protocol analyser provides out-of-the-box support for NetPerfMeter. That is, it is able to dissect and further analyse NPMP-CONTROL and NPMP-DATA packets using all supported Transport Layer protocols.


### Measurement Setup

A new measurement run setup is initiated by the active NetPerfMeter instance by establishing an NPMP-CONTROL association to the passive instance first. The NPMP-CONTROL association by default uses SCTP for transport. If SCTP is not possible in the underlying networks (e.g.&nbsp;due to firewalling restrictions), it is optionally possible to use TCP for the NPMP-CONTROL association instead. Then, the configured NPMP-DATA connections are established by their configured Transport Layer protocols. For the connection-less UDP, the message transfer is just started. The passive NetPerfMeter instance is informed about the identification and parameters of each new flow by using NPMP-CONTROL&nbsp;Add&nbsp;Flow messages. On startup of the NPMP-DATA flow, an NPMP-DATA&nbsp;Identify message allows the mapping of a newly incoming connection to a configured flow by the passive instance. It acknowledges each newly set up flow by an NPMP-CONTROL&nbsp;Acknowledge message. After setting up all flows, the scenario is ready to start the measurement run.


### Measurement Run

The actual measurement run is initiated from the active NetPerfMeter instance using an NPMP-CONTROL&nbsp;Start&nbsp;Measurement message, which is also acknowledged by an NPMP-CONTROL&nbsp;Acknowledge message. Then, both instances start running the configured scenario by transmitting NPMP-DATA&nbsp;Data messages over their configured flows.

During the measurement run, incoming and outgoing flow bandwidths may be recorded as vectors – i.e.&nbsp;time series – at both instances, since NPMP-DATA&nbsp;Data traffic may be bidirectional. Furthermore, the CPU utilisations – separately for each CPU and CPU&nbsp;core – are also tracked. This allows to identify performance bottlenecks, which is particularly useful when debugging and comparing transport protocol implementation performance. Furthermore, the one-way delay of messages can be recorded. Of course, in order to use this feature, the clocks of both nodes need to be appropriately synchronised, e.g.&nbsp;by using the [Network Time Protocol&nbsp;(NTP)](https://en.wikipedia.org/wiki/Network_Time_Protocol).


### Measurement Termination

The end of a measurement run is initiated – from the active NetPerfMeter instance – by using an NPMP-CONTROL&nbsp;Stop&nbsp;Measurement message. Again, it is acknowledged by an NPMP-CONTROL&nbsp;Acknowledge message. At the end of the measurement, average bandwidth and one-way delay of each flow and stream are recorded as scalars (i.e.&nbsp;single values). They may provide an overview of the long-term system performance.


## Result Collection

After stopping the measurement, the passive NetPerfMeter instance sends its global vector and scalar results (i.e.&nbsp;over all flows) to the active instance, by using one or more NPMP-CONTROL&nbsp;Results messages.
Then, the active NetPerfMeter instance sequentially removes the flows by using NPMP-CONTROL&nbsp;Remove&nbsp;Flow messages, which are acknowledged by NPMP-CONTROL Acknowledge messages. On flow removal, the passive instance sends its per-flow results for the corresponding flow, again by using NPMP-CONTROL&nbsp;Results messages.

The active instance, as well, archives its local vector and scalar results data and stores them – together with the results received from its peer – locally.
All result data is compressed by using BZip2 compression (see [bzip2](https://sourceware.org/bzip2/)), which may save a significant amount of bandwidth (of course, the passive node compresses the data *before* transfer) and disk space.


## Measurement Execution, Result Post-Processing and Visualisation##

By using shell scripts, it is possible to apply NetPerfMeter for parameter studies, i.e.&nbsp;to create a set of runs for each input parameter combination. For example, a script could iterate over a send buffer size&nbsp;σ from&nbsp;64&nbsp;KiB to 192&nbsp;KiB in steps of 64&nbsp;KiB as well as a path bandwidth&nbsp;ρ from&nbsp;10&nbsp;Mbit/s to 100&nbsp;Mbit/s in steps of&nbsp;10&nbsp;Mbit/s and perform 5&nbsp;measurement runs for each parameter combination.

When all measurement runs have eventually been processed, the results have to be visualised for analysis and interpretation. The NetPerfMeter package provides support to visualise the scalar results, which are distributed over the scalar files written by measurement runs. Therefore, the first step necessary is to bring the data from the various scalar files into an appropriate form for further post-processing. This step is denoted as *Summarisation*; an introduction is also provided in "[SimProcTC – The Design and Realization of a Powerful Tool-Chain for OMNeT++ Simulations](https://www.nntb.no/~dreibh/netperfmeter/#Publications-OMNeT__Workshop2009)".

The summarisation task is performed by the tool <tt>createsummary</tt>. An external program – instead of just using [GNU&nbsp;R](https://www.r-project.org/) itself to perform this step – is used due to the requirements on memory and CPU power. <tt>createsummary</tt> iterates over all scalar files of a measurement&nbsp;M. Each file is read – with on-the-fly BZip2-decompression – and each scalar value as well as the configuration&nbsp;m∈M having led to this value – are stored in memory. Depending on the number of scalars, the required storage space may have a size of multiple&nbsp;GiB.

Since usually not all scalars of a measurement are required for analysis (e.g.&nbsp;for an SCTP measurement, it may be unnecessary to include unrelated statistics), a list of scalar name prefixes to be excluded from summarisation can be provided to <tt>createsummary</tt>, in form of the so-called *Summary Skip List*. This feature may significantly reduce the memory and disk space requirements of the summarisation step. Since the skipped scalars still remain stored in the scalar files themselves, it is possible to simply re-run <tt>createsummary</tt> with updated summary skip list later, in order to also include them.

Having all relevant scalars stored in memory, a data file – which can be processed by [GNU&nbsp;R](https://www.r-project.org/) or other programs – is written for each scalar. The data file is simply a table in text form, containing the column names on the first line. Each following line contains the data, with line number and an entry for each column (all separated by spaces); an example is provided in Listing&nbsp;3 of "[SimProcTC – The Design and Realization of a Powerful Tool-Chain for OMNeT++ Simulations](https://www.nntb.no/~dreibh/netperfmeter/#Publications-OMNeT__Workshop2009)". That is, each line consists of the settings of all parameters and the resulting scalar value. The data files are also BZip2-compressed on the fly, in order to reduce the storage space requirements.


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

NetPerfMeter is released under the GNU General Public Licence&nbsp;(GPL).

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
