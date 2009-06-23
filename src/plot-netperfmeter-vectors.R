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


# ###### Set global variable (given as name string) to given value ##########
setGlobalVariable <- function(variable, value)
{
   globalEnv <- sys.frame()
   assign(variable, value, envir=globalEnv)
}


pdfOutlineFile <- NA
pdfOutlinePage <- NA


# ###### Create PDF info ####################################################
openPDFInfo <- function(name)
{
   setGlobalVariable("pdfInfoFile", file(paste(sep="", name, ".in"), "w"))
   setGlobalVariable("pdfOutlineFile", file(paste(sep="", name, ".ou"), "w"))
   setGlobalVariable("pdfOutlinePage", 0)

   cat(sep="", "InfoKey: Title\nInfoValue: NetPerfMeter Results Plots\n", file=pdfInfoFile);
   cat(sep="", "InfoKey: Producer\nInfoValue: NetPerfMeter\n", file=pdfInfoFile);
}


# ###### Add page into PDF info #############################################
addPage <- function()
{
   setGlobalVariable("pdfOutlinePage", pdfOutlinePage + 1)
}


# ###### Add entry into PDF info ############################################
addBookmark <- function(level, title)
{
   if(!is.na(pdfOutlineFile)) {
      cat(sep="", level, " ", pdfOutlinePage, " ", title, "\n", file=pdfOutlineFile)
    }
}


# ###### Close PDF info #####################################################
closePDFInfo <- function()
{
   if(!is.na(pdfOutlineFile)) {
      close(pdfOutlineFile)
      setGlobalVariable("pdfOutlineFile", NA)
   }
   if(!is.na(pdfInfoFile)) {
      close(pdfInfoFile)
      setGlobalVariable("pdfInfoFile", NA)
   }
}


# ###### Create a plot ######################################################
createPlot <- function(dataSet, title, ySet, yTitle, baseColor, vSet=c(), vTitle="")
{
   cat(sep="", "Plotting ", title, " ...\n")

   xSet <- dataSet$RelTime
   xTitle <- "Time{t}[s]"

   zSet <- dataSet$Description
   zTitle <- "Flow{F}"
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
      pdf(pdfName, width=plotWidth, height=plotHeight, paper=plotPaper,
          family=plotFontFamily, pointsize=plotPointSize)
   }
   else {
      addPage()
      addBookmark(3, title)
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
            legendSize=plotLegendSize,
            simulationName="Name der Simulation",
            pStart=0)
   par(opar)
   if(plotOwnFile) {
      dev.off()
      processPDFbyGhostscript(pdfName)
   }
}


# ###### Create node plots ##################################################
plotNodeStats <- function(inputData, nodeName)
{
   addBookmark(1, paste(sep="", "Node ''", nodeName, "''"))

   # ====== Input/Output Rates ==============================================
   addBookmark(2, "Input/Output Rates")
   data <- subset(inputData, (inputData$Action != "Lost"))
   createPlot(data, paste(sep="", "Bit Rate Sent/Received at Node ''", nodeName, "''"),
              (data$RelBytes * 8 / 1000) / data$Interval, "Bit Rate [Kbit/s]", "blue4",
              data$Action, "Action{A}")
   createPlot(data, paste(sep="", "Byte Rate Sent/Received at Node ''", nodeName, "''"),
              (data$RelBytes / 1000) / data$Interval, "Byte Rate [KiB/s]", "blue2",
              data$Action, "Action{A}")
   createPlot(data, paste(sep="", "Packet Rate Sent/Received at Node ''", nodeName, "''"),
              data$RelPackets / data$Interval, "Packet Rate [Packets/s]", "green4",
              data$Action, "Action{A}")
   createPlot(data, paste(sep="", "Frame Rate Sent/Received at Node ''", nodeName, "''"),
              data$RelFrames / data$Interval, "Frame Rate [Frames/s]", "yellow4",
              data$Action, "Action{A}")

   # ====== Input/Output Absolute ===========================================
   addBookmark(2, "Input/Output Absolute")
   data <- subset(inputData, (inputData$Action != "Lost"))
   createPlot(data, paste(sep="", "Bits Sent/Received at Node ''", nodeName, "''"),
              (data$AbsBytes * 8 / 1000), "Bits [Kbit]", "blue4",
              data$Action, "Action{A}")
   createPlot(data, paste(sep="", "Bytes Sent/Received at Node ''", nodeName, "''"),
              (data$AbsBytes / 1000), "Bytes [KiB]", "blue2",
              data$Action, "Action{A}")
   createPlot(data, paste(sep="", "Packets Sent/Received at Node ''", nodeName, "''"),
              data$AbsPackets, "Packets [1]", "green4",
              data$Action, "Action{A}")
   createPlot(data, paste(sep="", "Frames Sent/Received at Node ''", nodeName, "''"),
              data$AbsFrames, "Frames [s]", "yellow4",
              data$Action, "Action{A}")

   # ====== Jitter ==========================================================
   addBookmark(2, "Quality of Service")
   data <- subset(inputData, (inputData$Action == "Received"))
   createPlot(data, paste(sep="", "Interarrival Jitter at Node ''", nodeName, "''"),
              data$Jitter, "Jitter [ms]", "gold4",
              data$Action, "Action{A}")

   # ====== Loss ============================================================
   data <- subset(inputData, (inputData$Action == "Lost"))
   createPlot(data, paste(sep="", "Byte Loss Rate at Node ''", nodeName, "''"),
              data$RelBytes / data$Interval, "Byte Loss Rate [KiB/s]", "red2")
   createPlot(data, paste(sep="", "Packet Loss Rate at Node ''", nodeName, "''"),
              data$RelPackets / data$Interval, "Packet Loss Rate [Packets/s]", "red4")
}





# ###### Default Settings ###################################################
plotColorMode  <- 2   # == cmColor
plotOwnFile    <- TRUE
plotFontFamily <- "Helvetica"
plotPointSize  <- 18      # Use 22 for 10x10 plots
plotLegendSize <- 0.80
plotWidth      <- -1
plotHeight     <- -1
plotPaper      <- "A4r"   # Use "special" for manual values! Or: A4/A4r.


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


# ====== Load input data ====================================================
completeData <- loadResults(vectorFile)
completeData <- subset(completeData, (completeData$Interval > 0))   # Avoids "divide by zero" on first entry


# ====== Begin writing PDF file =============================================
if(!plotOwnFile) { 
   pdfFileName <- paste(sep="", pdfFilePrefix, "-TEMP.pdf")
   pdf(pdfFileName, width=plotWidth, height=plotHeight, paper=plotPaper,
       family=plotFontFamily, pointsize=plotPointSize)
   openPDFInfo(pdfFileName)
}


# ====== Create plots for active and passive node ===========================
data <- subset(completeData, (completeData$Active == 1) &
                             (completeData$FlowID != -1))
plotNodeStats(data, activeName)

data <- subset(completeData, (completeData$Active == 0) &
                             (completeData$FlowID != -1))
plotNodeStats(data, passiveName)


# ====== Finish PDF file ====================================================
if(!plotOwnFile) {
   # ------ Close PDF and embed fonts (using Ghostscript) -------------------
   dev.off()
   closePDFInfo()
   processPDFbyGhostscript(pdfFileName)

   # ------ Add PDF bookmarks -----------------------------------------------
   cmd1 <- paste(sep="", "pdfoutline ", pdfFileName, " ", pdfFileName, ".ou ", pdfFileName)
   ret1 <- system(cmd1)
   if(ret1 != 0) {
      stop(gettextf("status %d in running command '%s'", ret1, cmd1))
   }

   # ------ Add PDF infos ---------------------------------------------------
   cmd2 <- paste(sep="", "pdftk ", pdfFileName, " update_info ", pdfFileName, ".in output ", pdfFilePrefix, ".pdf")
   ret2 <- system(cmd2)
   if(ret2 != 0) {
      stop(gettextf("status %d in running command '%s'", ret2, cmd2))
   }

   # ------ Remove temporary files ------------------------------------------
   unlink(pdfFileName)
   unlink(paste(sep="", pdfFileName, ".in"))
   unlink(paste(sep="", pdfFileName, ".ou"))
}
