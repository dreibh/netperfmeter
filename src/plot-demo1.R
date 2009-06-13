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


# ###### Create a plot ######################################################
createPlot <- function(title, ySet, baseColor, yTitle)
{
   cat(sep="", "Plotting ", title, " ...\n")

   xSet <- data$RelTime
   xTitle <- "Time{t}[s]"

   zSet <- data$Description
   zTitle <- "Flow{F}"
   vSet <- c()
   vTitle <- ""
   wSet <- c()
   wTitle <- ""

   aSet <- c()
   aTitle <- ""
   bSet <- c()
   bTitle <- ""
   pSet <- c()
   pTitle <- ""

   if(colorMode != cmColor) {
      baseColor <- par("fg")
   }

   xAxisTicks <- getIntegerTicks(xSet)   # Set to c() for automatic setting
   yAxisTicks <- getIntegerTicks(ySet)   # Set to c() for automatic setting

   opar <- par(fg=baseColor)
   plotstd6(title,
            pTitle, aTitle, bTitle, xTitle, yTitle, zTitle,
            pSet, aSet, bSet, xSet, ySet, zSet,
            vSet, wSet, vTitle, wTitle,
            xAxisTicks=xAxisTicks,
            yAxisTicks=yAxisTicks,
            type="linesx",
            colorMode=cmColor,
            hideLegend=FALSE,
            simulationName="Name der Simulation",
            pStart=0)
   par(opar)
}



colorMode <- cmColor

data <- loadResults("x.vec")
data <- subset(data, (data$Interval > 0))   # Avoids "divide by zero" on first entry


pdf("demo1.pdf", width=10, height=10, family="Helvetica", pointsize=22)


# ====== Transmission Bandwidth =============================================
createPlot("Transmitted Bit Rate",
           (data$RelTransmittedBytes * 8 / 1000) / data$Interval,
           "blue4", "Bit Rate [Kbit/s]")
createPlot("Transmitted Byte Rate",
           (data$RelTransmittedBytes / 1024) / data$Interval,
           "blue2", "Byte Rate [KiB/s]")
createPlot("Transmitted Packet Rate",
           data$AbsTransmittedPackets/ data$Interval,
           "green4", "Packet Rate [Packets/s]")
createPlot("Transmitted Frame Rate",
           data$AbsTransmittedFrames/ data$Interval,
           "yellow4", "Frame Rate [Frames/s]")

# ====== Transmission Volume ================================================
createPlot("Number of Transmitted Bits",
           data$AbsTransmittedBytes / 1000000,
           "blue4", "Bits [Mbit]")
createPlot("Number of Transmitted Bytes",
           data$AbsTransmittedBytes / (1024 * 1024),
           "blue2", "Bytes [MiB]")
createPlot("Number of Transmitted Packets",
           data$AbsTransmittedPackets,
           "green4", "Packets [1]")
createPlot("Number of Transmitted Frames",
           data$AbsTransmittedFrames,
           "yellow4", "Frames [1]")


# ====== Reception Bandwidth =============================================
createPlot("Received Bit Rate",
           (data$RelReceivedBytes * 8 / 1000) / data$Interval,
           "blue4", "Bit Rate [Kbit/s]")
createPlot("Received Byte Rate",
           (data$RelReceivedBytes / 1024) / data$Interval,
           "blue2", "Byte Rate [KiB/s]")
createPlot("Received Packet Rate",
           data$AbsReceivedPackets/ data$Interval,
           "green4", "Packet Rate [Packets/s]")
createPlot("Received Frame Rate",
           data$AbsReceivedFrames/ data$Interval,
           "yellow4", "Frame Rate [Frames/s]")

# ====== Reception Volume ================================================
createPlot("Number of Received Bits",
           data$AbsReceivedBytes / 1000000,
           "blue4", "Bits [Mbit]")
createPlot("Number of Received Bytes",
           data$AbsReceivedBytes / (1024 * 1024),
           "blue2", "Bytes [MiB]")
createPlot("Number of Received Packets",
           data$AbsReceivedPackets,
           "green4", "Packets [1]")
createPlot("Number of Received Frames",
           data$AbsReceivedFrames,
           "yellow4", "Frames [1]")

dev.off()
