# $Id: plot-version1.R 1533 2012-11-13 09:50:00Z adhari $
#
# Network Performance Meter
# Copyright (C) 2012 by Thomas Dreibholz
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Contact: dreibh@iem.uni-due.de
#

source("plotter-version1.R")


netPerfMeterPlotVariables <- list(
   # ------ Format example --------------------------------------------------
   # list("Variable",
   #         "Unit[x]{v]"
   #          "100.0 * data1$x / data1$y", <- Manipulator expression:
   #                                           "data" is the data table
   #                                        NA here means: use data1$Variable.
   #          "myColor",
   #          list("InputFile1", "InputFile2", ...))
   #             (simulationDirectory/Results/....data.tar.bz2 is added!)
   # ------------------------------------------------------------------------
   list("simDir",   		 "Path {Pa}"),
   list("Flows",                 "Number of Flows{n}[1]"),
   list("Reference Flows",       "Number of Reference Flows{n}[1]"),
   list("ReferenceFlowProtocol", "Reference Flow Protocol{P}"),
   list("OnlyOneAssoc",          "Only One Assoc{A}"),
   list("Unordered",             "Unordered{U}"),
   list("CMTCCVariant",          "Protocol {Pr}"),
   list("OptionDAC",             "Delayed Ack CMT{:D:}"),
   list("OptionNRSACK",          "Use NR-SACK{:n:}"),
   list("SndBuf",                "Send Buffer{S}[Bytes]"),
   list("RcvBuf",                "Receive Buffer{R}[Bytes]"),
   list("InitialCwnd",           "Initial Cwnd{:i:}[Bytes]"),
   list("OptionBufferSplitting", "Initial Path {Pa}"),
   list("BidirectionalQoS",      "Bidirectional QoS{Q}"),
   list("CwndMaxBurst",          "Cwnd MaxBurst{c}"),
   list("Runtime",               "Runtime{R}[s]"),
   list("Protocol",          	 "Protocol {Pr}"),
   list("SndBuf-Kbyte",                
	    "Send Buffer [KBytes]", "data1$SndBuf / 1000"),
   list("RateNorthernTrail",
           "Data Rate on Northern Trail{:R[North]:}[Kbit/s]"),
   list("RateNorthernTrail-Mbit",
           "Common Data Rate{R}[Mbit/s]", "data1$RateNorthernTrail / 1000"),
   list("DelayNorthernTrail",
           "Delay on Northern Trail{:D[North]:}[ms]"),
   list("LossNorthernTrail",
           "Loss Rate on Northern Trail{:E[North]:}"),
   list("RateSouthernTrail",
           "Data Rate on Southern Trail{:R[South]:}[Kbit/s]"),
   list("RateSouthernTrail-Mbit",
           "Data Rate on Southern Trail{:R[South]:}[Mbit/s]", "data1$RateSouthernTrail / 1000"),
   list("DelaySouthernTrail",
           "Delay on Southern Trail{:D[South]:}[ms]"),
   list("LossSouthernTrail",
           "Loss Rate on Southern Trail{:E[South]:}"),

   list("passive.flow",
           "Flow {F}"),
   list("passive.flow-ReceivedPackets",
           "Received Packets [1]",
           "data1$passive.flow.ReceivedPackets",
           "green4",
           list("passive.flow-ReceivedPackets")),
   list("passive.flow-ReceivedBytes",
           "Received Bytes [MiB]",
           "data1$passive.flow.ReceivedBytes / (1024 * 1024)",
           "blue4",
           list("passive.flow-ReceivedBytes")),
   list("passive.flow-ReceivedBitRate",
           "Received Bit Rate[Kbit/s]",
           "8 * data1$passive.flow.ReceivedByteRate / 1000",
           "blue4",
           list("passive.flow-ReceivedByteRate")),
   list("passive.flow-ReceivedBitRate-Mbit",
           "Received Bit Rate[Mbit/s]",
           "8 * data1$passive.flow.ReceivedByteRate / 1000000",
           "blue4",
           list("passive.flow-ReceivedByteRate")),
   list("passive.flow-ReceivedByteRate",
           "Received Byte Rate[KiB/s]",
           "data1$passive.flow.ReceivedByteRate / 1024",
           "blue2",
           list("passive.flow-ReceivedByteRate")),

   list("passive.total-ReceivedBitRate",
           "Received Bit Rate[Kbit/s]",
           "8 * data1$passive.total.ReceivedByteRate / 1000",
           "blue4",
           list("passive.total-ReceivedByteRate")),
   list("passive.total-ReceivedBitRate-Mbit",
           "Received Bit Rate[Mbit/s]",
           "8 * data1$passive.total.ReceivedByteRate / 1000000",
           "blue4",
           list("passive.total-ReceivedByteRate")),
   list("passive.total-ReceivedByteRate",
           "Received Byte Rate[KiB/s]",
           "data1$passive.total.ReceivedByteRate / 1024",
           "blue2",
           list("passive.total-ReceivedByteRate")),

   list("passive.CPU", "CPU Number{Z}"),
   list("passive.CPU-Utilization",
           "CPU Utilization[%]",
           "100.0 - data1$passive.CPU.Idle",
           "red4",
           list("passive.CPU-Idle")),
   list("passive.CPU-System",
           "CPU Load by System[%]",
           "data1$passive.CPU.System",
           "cyan4",
           list("passive.CPU-System")),

   list("active.CPU", "CPU Number{Z}", NA),
   list("active.CPU-Utilization",
           "CPU Utilization[%]",
           "100.0 - data1$active.CPU.Idle",
           "red4",
           list("active.CPU-Idle")),
   list("active.CPU-System",
           "CPU Load by System[%]",
           "data1$active.CPU.System",
           "cyan4",
           list("active.CPU-System"))
)
