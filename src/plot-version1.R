#!/bin/bash
# $Id$
#
# Network Performance Meter
# Copyright (C) 2009 by Thomas Dreibholz
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

source("plotter.R")


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

   list("Flows",        "Number of Flows{n}[1]",     NA),
   list("OnlyOneAssoc", "Only One Assoc{A}",         NA),
   list("Unordered",    "Unordered{U}",              NA),
   list("OptionRP",     "CMT-RP {:Gamma:}",          NA),
   list("OptionCMT",    "Allow CMT {:mu:}",          NA),
   list("OptionDAC",    "Delayed Ack CMT {:delta:}", NA),
   list("OptionNRSACK", "Use NR-SACK{:nu:}",         NA),
   list("SndBuf",       "Send Buffer{S}[Bytes]",     NA),
   list("RcvBuf",       "Receive Buffer{R}[Bytes]",  NA),

   list("RateNorthernTrail",
           "Data Rate on Northern Trail {:rho[North]:}[Kbit/s]", NA),
   list("RateNorthernTrail-Mbit",
           "Common Data Rate{:rho:}[Mbit/s]", "data1$RateNorthernTrail / 1000"),
   list("DelayNorthernTrail",
           "Delay on Northern Trail {:delta[North]:}[ms]",
           NA, "black"),
   list("LossNorthernTrail",
           "Loss Rate on Northern Trail {:epsilon[North]:}"),
   list("RateSouthernTrail",
           "Data Rate on Southern Trail {:rho[South]:}[Kbit/s]", NA),
   list("DelaySouthernTrail",
           "Delay on Southern Trail {:delta[South]:}[ms]",
           NA, "black"),
   list("LossSouthernTrail",
           "Loss Rate on Southern Trail {:epsilon[South]:}"),

   list("passive.flow",
           "Flow Number{F}", NA),
   list("passive.flow-ReceivedBytes",
           "Received Bytes [MiB]",
           "data1$passive.flow.ReceivedBytes / (1024 * 1024)",
           "blue4",
           list("passive.flow-ReceivedBytes")),
   list("passive.flow-ReceivedByteRate",
           "Received Byte Rate[Kbit/s]",
           "8* data1$passive.flow.ReceivedByteRate / 1024",
           "blue2",
           list("passive.flow-ReceivedByteRate")),

   list("passive.total-ReceivedBitRate",
           "Received Bit Rate[Kbit/s]",
           "8 * data1$passive.total.ReceivedByteRate / 1000",
           "blue4",
           list("passive.total-ReceivedByteRate")),
   list("passive.total-ReceivedByteRate",
           "Received Byte Rate[KiB/s]",
           "data1$passive.total.ReceivedByteRate / 1024",
           "blue2",
           list("passive.total-ReceivedByteRate"))
)
