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


# ###### Create a plot ######################################################
createPlot <- function(dataSet, title, ySet, baseColor, yTitle)
{
   cat(sep="", "Plotting ", title, " ...\n")

   xSet <- dataSet$RelTime
   xTitle <- "Time{t}[s]"

   zSet <- dataSet$Description
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

   if(plotColorMode != cmColor) {
      baseColor <- par("fg")
   }

   xAxisTicks <- getIntegerTicks(xSet)   # Set to c() for automatic setting
   yAxisTicks <- getIntegerTicks(ySet)   # Set to c() for automatic setting

   opar <- par(fg=baseColor)
   if(plotOwnFile) {
      names   <- unlist(strsplit(title, " "))
      pdfName <- paste(sep="", pdfFilePrefix, "-")
      for(i in 1:length(names)) {
         pdfName <- paste(sep="", pdfName, names[i])
      }
      pdfName <- paste(sep="", pdfName, ".pdf")
      cat(pdfName,"\n")
      pdf(pdfName, width=plotWidth, height=plotHeight, family=plotFontFamily, pointsize=plotPointSize)
   }
   plotstd6(title,
            pTitle, aTitle, bTitle, xTitle, yTitle, zTitle,
            pSet, aSet, bSet, xSet, ySet, zSet,
            vSet, wSet, vTitle, wTitle,
            xAxisTicks=xAxisTicks,
            yAxisTicks=yAxisTicks,
            type="linesx",
            colorMode=plotColorMode,
            hideLegend=FALSE,
            simulationName="Name der Simulation",
            pStart=0)
   par(opar)
   if(plotOwnFile) {
      dev.off()
      processPDFbyGhostscript(pdfName)
   }
}


# ###### Create plot label ##################################################
directedLabel <- function(title, a, b)
{
   return(paste(sep="", title, " (" ,a, " -> ", b, ")"))
}


# ###### Default Settings ###################################################
plotColorMode  <- 2   # == cmColor
plotOwnFile    <- TRUE
plotFontFamily <- "Helvetica"
plotPointSize  <- 22
plotWidth      <- 10
plotHeight     <- 10


# ###### Command-Line Arguments #############################################
args=commandArgs(TRUE)
print(args)

for(i in 1:length(args)) {
   eval(parse(text=args[i]))
   if(i == 2) {
      # ------ Include plotting functions -----------------------------------
      # The first parameter must set "programDirectory". The further
      # parameters may use constants defined in plotter.R. Therefore, plotter.R
      # must be included before!
      source(paste(sep="", programDirectory, "/plotter.R"))
   }
}

cat(sep="", "programDirectory=", programDirectory,    "\n")
cat(sep="", "vectorFile=",       vectorFile,    "\n")
cat(sep="", "activeName=",       activeName,    "\n")
cat(sep="", "passiveName=",      passiveName,    "\n")
cat(sep="", "pdfFilePrefix=",    pdfFilePrefix, "\n")
cat(sep="", "plotOwnFile=",      plotOwnFile,   "\n")



# ###### Create plots as PDF file(s) ########################################
completeData <- loadResults(vectorFile)
completeData <- subset(completeData, (completeData$Interval > 0))   # Avoids "divide by zero" on first entry
if(!plotOwnFile) {
   pdf(paste(sep="", pdfFilePrefix, ".pdf"),
       width=plotWidth, height=plotHeight, family=plotFontFamily, pointsize=plotPointSize)
}
activeNodeData  <- subset(completeData, (completeData$Active == 1))
passiveNodeData <- subset(completeData, (completeData$Active != 1))


# ====== Transmission Bandwidth =============================================
createPlot(activeNodeData, directedLabel("Transmitted Bit Rate", activeName, passiveName),
           (activeNodeData$RelTransmittedBytes * 8 / 1000) / activeNodeData$Interval,
           "blue4", "Bit Rate [Kbit/s]")
createPlot(passiveNodeData, directedLabel("Transmitted Bit Rate", passiveName, passiveName),
           (passiveNodeData$RelTransmittedBytes * 8 / 1000) / passiveNodeData$Interval,
           "blue4", "Bit Rate [Kbit/s]")

# createPlot("Transmitted Byte Rate",
#            (data$RelTransmittedBytes / 1024) / data$Interval,
#            "blue2", "Byte Rate [KiB/s]")
# createPlot("Transmitted Packet Rate",
#            data$RelTransmittedPackets / data$Interval,
#            "green4", "Packet Rate [Packets/s]")
# createPlot("Transmitted Frame Rate",
#            data$RelTransmittedFrames / data$Interval,
#            "yellow4", "Frame Rate [Frames/s]")
# 
# # ====== Transmission Volume ================================================
# createPlot("Number of Transmitted Bits",
#            data$AbsTransmittedBytes / 1000000,
#            "blue4", "Bits [Mbit]")
# createPlot("Number of Transmitted Bytes",
#            data$AbsTransmittedBytes / (1024 * 1024),
#            "blue2", "Bytes [MiB]")
# createPlot("Number of Transmitted Packets",
#            data$AbsTransmittedPackets,
#            "green4", "Packets [1]")
# createPlot("Number of Transmitted Frames",
#            data$AbsTransmittedFrames,
#            "yellow4", "Frames [1]")
# 
# 
# # ====== Reception Bandwidth =============================================
# createPlot("Received Bit Rate",
#            (data$RelReceivedBytes * 8 / 1000) / data$Interval,
#            "blue4", "Bit Rate [Kbit/s]")
# createPlot("Received Byte Rate",
#            (data$RelReceivedBytes / 1024) / data$Interval,
#            "blue2", "Byte Rate [KiB/s]")
# createPlot("Received Packet Rate",
#            data$AbsReceivedPackets/ data$Interval,
#            "green4", "Packet Rate [Packets/s]")
# createPlot("Received Frame Rate",
#            data$AbsReceivedFrames/ data$Interval,
#            "yellow4", "Frame Rate [Frames/s]")
# 
# # ====== Reception Volume ================================================
# createPlot("Number of Received Bits",
#            data$AbsReceivedBytes / 1000000,
#            "blue4", "Bits [Mbit]")
# createPlot("Number of Received Bytes",
#            data$AbsReceivedBytes / (1024 * 1024),
#            "blue2", "Bytes [MiB]")
# createPlot("Number of Received Packets",
#            data$AbsReceivedPackets,
#            "green4", "Packets [1]")
# createPlot("Number of Received Frames",
#            data$AbsReceivedFrames,
#            "yellow4", "Frames [1]")
# 
# # ====== Reception Jitter ================================================
# createPlot("Received Interarrival Jitter",
#            data$AbsReceivedFrames/ data$Interval,
#            "gold4", "Interarrival Jitter [ms]")


if(!plotOwnFile) {
   dev.off()
   processPDFbyGhostscript(paste(sep="", pdfFilePrefix, ".pdf"))
}
