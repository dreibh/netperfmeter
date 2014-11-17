# $Id$
#
# Network Performance Meter
# Copyright (C) 2015 by Thomas Dreibholz
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



# ###########################################################################
# #### PDF Metadata Handling                                             ####
# ###########################################################################

pdfMetadataFile <- NA
pdfMetadataPage <- NA

# ###### Create PDF info ####################################################
openPDFMetadata <- function(name)
{
   setGlobalVariable("pdfMetadataFile", file(paste(sep="", name, ".meta"), "w"))
   setGlobalVariable("pdfMetadataPage", 0)

   cat(sep="", "title NetPerfMeter Results Plots\n", file=pdfMetadataFile);
   cat(sep="", "subject Measurement Results\n", file=pdfMetadataFile);
   cat(sep="", "creator plot-netperfmeter-results\n", file=pdfMetadataFile);
   cat(sep="", "producer GNU R/Ghostscript\n", file=pdfMetadataFile);
   cat(sep="", "keywords: NetPerfMeter, Measurements, Results\n", file=pdfMetadataFile);
}


# ###### Finish current page and increase page counter ######################
nextPageInPDFMetadata <- function()
{
   setGlobalVariable("pdfMetadataPage", pdfMetadataPage + 1)
}


# ###### Add entry into PDF info ############################################
addBookmarkInPDFMetadata <- function(level, title)
{
   if(!is.na(pdfMetadataFile)) {
      cat(sep="", "outline ", level, " ", pdfMetadataPage + 1, " ", title, "\n", file=pdfMetadataFile)
    }
}


# ###### Close PDF info #####################################################
closePDFMetadata <- function()
{
   if(!is.na(pdfMetadataFile)) {
      close(pdfMetadataFile)
      setGlobalVariable("pdfMetadataFile", NA)
   }
   if(!is.na(pdfMetadataFile)) {
      close(pdfMetadataFile)
      setGlobalVariable("pdfMetadataFile", NA)
   }
}



# ###########################################################################
# #### Plotting                                                          ####
# ###########################################################################

# ###### Create a plot ######################################################
createPlot <- function(dataSet, title, ySet, yTitle, baseColor, zSet, zTitle, vSet=c(), vTitle="",type="linesx", bookmarkLevel=3)
{
   cat(sep="", "Plotting ", title, " ...\n")

   xSet <- dataSet$RelTime
   xTitle <- "Time{t}[s]"

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

   xAxisTicks <- getIntegerTicks(c(0, gRuntime))   # Set to c() for automatic setting
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
      addBookmarkInPDFMetadata(bookmarkLevel, title)
   }
   plotstd6(title,
            pTitle, aTitle, bTitle, xTitle, yTitle, zTitle,
            pSet, aSet, bSet, xSet, ySet, zSet,
            vSet, wSet, vTitle, wTitle,
            xAxisTicks=xAxisTicks,
            yAxisTicks=yAxisTicks,
            type=type,
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
   else {
      nextPageInPDFMetadata()
   }
}



# ###########################################################################
# #### Statistics                                                        ####
# ###########################################################################

# ###### Create node plots ##################################################
plotNodeStats <- function(inputData, nodeName)
{
   addBookmarkInPDFMetadata(1, paste(sep="", "Node ''", nodeName, "''"))

   # ====== Input/Output Rates ==============================================
   addBookmarkInPDFMetadata(2, "Input/Output Rates")
   data <- subset(inputData, (inputData$Action != "Lost"))
   createPlot(data, paste(sep="", "Bit Rate Sent/Received at Node ''", nodeName, "''"),
              (data$RelBytes * 8 / 1000) / data$Interval, "Bit Rate [Kbit/s]", "blue4",
              data$Description, "Flow{F}", data$Action, "Action{A}")
   createPlot(data, paste(sep="", "Byte Rate Sent/Received at Node ''", nodeName, "''"),
              (data$RelBytes / 1000) / data$Interval, "Byte Rate [KiB/s]", "blue2",
              data$Description, "Flow{F}", data$Action, "Action{A}")
   createPlot(data, paste(sep="", "Packet Rate Sent/Received at Node ''", nodeName, "''"),
              data$RelPackets / data$Interval, "Packet Rate [Packets/s]", "green4",
              data$Description, "Flow{F}", data$Action, "Action{A}")
   createPlot(data, paste(sep="", "Frame Rate Sent/Received at Node ''", nodeName, "''"),
              data$RelFrames / data$Interval, "Frame Rate [Frames/s]", "yellow4",
              data$Description, "Flow{F}", data$Action, "Action{A}")

   # ====== Input/Output Absolute ===========================================
   addBookmarkInPDFMetadata(2, "Input/Output Absolute")
   data <- subset(inputData, TRUE) # (inputData$Action != "Lost"))
   createPlot(data, paste(sep="", "Bits Sent/Received at Node ''", nodeName, "''"),
              (data$AbsBytes * 8 / 1000), "Bits [Kbit]", "blue4",
              data$Description, "Flow{F}", data$Action, "Action{A}")
   createPlot(data, paste(sep="", "Bytes Sent/Received at Node ''", nodeName, "''"),
              (data$AbsBytes / 1000), "Bytes [KiB]", "blue2",
              data$Description, "Flow{F}", data$Action, "Action{A}")
   createPlot(data, paste(sep="", "Packets Sent/Received at Node ''", nodeName, "''"),
              data$AbsPackets, "Packets [1]", "green4",
              data$Description, "Flow{F}", data$Action, "Action{A}")
   createPlot(data, paste(sep="", "Frames Sent/Received at Node ''", nodeName, "''"),
              data$AbsFrames, "Frames [s]", "yellow4",
              data$Description, "Flow{F}", data$Action, "Action{A}")

   # ====== Jitter ==========================================================
   addBookmarkInPDFMetadata(2, "Quality of Service")
   data <- subset(inputData, (inputData$Action == "Received"))
   createPlot(data, paste(sep="", "Jitter at Node ''", nodeName, "''"),
              data$Jitter, "Jitter [ms]", "gold4",
              data$Description, "Flow{F}", data$Action, "Action{A}")

   # ====== Loss ============================================================
   data <- subset(inputData, (inputData$Action == "Lost"))
   createPlot(data, paste(sep="", "Bit Loss Rate at Node ''", nodeName, "''"),
              (data$RelBytes * 8 / 1000) / data$Interval, "Byte Loss Rate [Kbit/s]", "red4",
              data$Description, "Flow{F}")
   createPlot(data, paste(sep="", "Byte Loss Rate at Node ''", nodeName, "''"),
              data$RelBytes / data$Interval, "Byte Loss Rate [KiB/s]", "red2",
              data$Description, "Flow{F}")
   createPlot(data, paste(sep="", "Packet Loss Rate at Node ''", nodeName, "''"),
              data$RelPackets / data$Interval, "Packet Loss Rate [Packets/s]", "red3",
              data$Description, "Flow{F}")
   createPlot(data, paste(sep="", "Frame Loss Rate at Node ''", nodeName, "''"),
              data$RelFrames / data$Interval, "Frame Loss Rate [Frames/s]", "red1",
              data$Description, "Flow{F}")
}


# ###### Create flow QoS plots ##############################################
plotQoSStatistics <- function(inputData, flowSummaryData, nodeName) {
   addBookmarkInPDFMetadata(1, "Per-Flow Results")

   for(isActive in c(0, 1)) {
      if(isActive == 1) {
         nodeName = NAME_ACTIVE_NODE
      }
      else {
         nodeName <- NAME_PASSIVE_NODE
      }
      addBookmarkInPDFMetadata(2, paste(sep="", "Node ''", nodeName, "''"))

      flowLevels <- levels(factor(flowSummaryData$FlowID))
      for(i in seq(1, length(flowLevels))) {
         data <- subset(flowSummaryData, (flowSummaryData$FlowID == flowLevels[i]))
         flowNames <- levels(factor(data$FlowName))
         flowName <- flowNames[1]
         addBookmarkInPDFMetadata(3, paste(sep="", "Flow ''", flowName, "''"))


         # ====== Input/Output Rates ========================================
         addBookmarkInPDFMetadata(4, "Input/Output Rates")
         data <- subset(inputData,
                        (inputData$FlowID == flowLevels[i]) &
                           (inputData$IsActive == isActive) & (inputData$FlowID != -1) &
                           (inputData$Action != "Lost"))
         createPlot(data, paste(sep="", "Bit Rate Sent/Received for Flow ''", flowName, "'' at Node ''", nodeName, "''"),
                    (data$RelBytes * 8 / 1000) / data$Interval, "Bit Rate [Kbit/s]", "blue4",
                    data$Action, "Action{A}", data$Description, "Flow{F}", bookmarkLevel=5)
         createPlot(data, paste(sep="", "Byte Rate Sent/Received for Flow ''", flowName, "'' at Node ''", nodeName, "''"),
                    (data$RelBytes / 1000) / data$Interval, "Byte Rate [KiB/s]", "blue2",
                    data$Action, "Action{A}", data$Description, "Flow{F}", bookmarkLevel=5)
         createPlot(data, paste(sep="", "Packet Rate Sent/Received for Flow ''", flowName, "'' at Node ''", nodeName, "''"),
                    data$RelPackets / data$Interval, "Packet Rate [Packets/s]", "green4",
                    data$Action, "Action{A}", data$Description, "Flow{F}", bookmarkLevel=5)
         createPlot(data, paste(sep="", "Frame Rate Sent/Received for Flow ''", flowName, "'' at Node ''", nodeName, "''"),
                    data$RelFrames / data$Interval, "Frame Rate [Frames/s]", "yellow4",
                    data$Action, "Action{A}", data$Description, "Flow{F}", bookmarkLevel=5)

         # ====== Input/Output Absolute =====================================
         addBookmarkInPDFMetadata(4, "Input/Output Absolute")
         data <- subset(inputData,
                        (inputData$FlowID == flowLevels[i]) &
                           (inputData$IsActive == isActive) & (inputData$FlowID != -1) )
                           # & (inputData$Action != "Lost"))
         createPlot(data, paste(sep="", "Bits Sent/Received for Flow ''", flowName, "'' at Node ''", nodeName, "''"),
                    (data$AbsBytes * 8 / 1000), "Bits [Kbit]", "blue4",
                    data$Action, "Action{A}", data$Description, "Flow{F}", bookmarkLevel=5)
         createPlot(data, paste(sep="", "Bytes Sent/Received for Flow ''", flowName, "'' at Node ''", nodeName, "''"),
                    (data$AbsBytes / 1000), "Bytes [KiB]", "blue2",
                    data$Action, "Action{A}", data$Description, "Flow{F}", bookmarkLevel=5)
         createPlot(data, paste(sep="", "Packets Sent/Received for Flow ''", flowName, "'' at Node ''", nodeName, "''"),
                    data$AbsPackets, "Packets [1]", "green4",
                    data$Action, "Action{A}", data$Description, "Flow{F}", bookmarkLevel=5)
         createPlot(data, paste(sep="", "Frames Sent/Received for Flow ''", flowName, "'' at Node ''", nodeName, "''"),
                    data$AbsFrames, "Frames [s]", "yellow4",
                    data$Action, "Action{A}", data$Description, "Flow{F}", bookmarkLevel=5)

         # ====== Quality of Service ========================================
         addBookmarkInPDFMetadata(4, "Quality of Service")
         data <- subset(flowSummaryData,
                        (flowSummaryData$FlowID == flowLevels[i]) &
                           (flowSummaryData$IsActive == isActive))
         if(length(data$FlowID) > 0) {   # May be empty, if no packets have been sent in this direction!
            createPlot(data, paste(sep="", "Per-Message Delay for Flow ''", flowName, "'' at Node ''", nodeName, "''"),
                       data$Delay, "Delay [ms]", "orange2",
                       c(), NA,   # data$NodeName, "Node{N}",
                       type="p", bookmarkLevel=5)
            createPlot(data, paste(sep="", "Per-Message Jitter for Flow ''", flowName, "'' at Node ''", nodeName, "''"),
                       data$Jitter, "Jitter [ms]", "gold4",
                       c(), NA,   # data$NodeName, "Node{N}",
                       type="p", bookmarkLevel=5)
         }
      }
   }
}



# ###########################################################################
# #### Main Program                                                      ####
# ###########################################################################

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
# print(args)

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

# cat(sep="", "programDirectory=", programDirectory,    "\n")
# cat(sep="", "configFile=",       configFile,    "\n")
# cat(sep="", "summaryFile=",      summaryFile,    "\n")
# cat(sep="", "pdfFilePrefix=",    pdfFilePrefix, "\n")
# cat(sep="", "plotOwnFile=",      plotOwnFile,   "\n")


# ====== Load input data ====================================================
source(configFile)
summaryData <- loadResults(summaryFile)
summaryData <- subset(summaryData, (summaryData$Interval > 0))   # Avoids "divide by zero" on first entry
flowSummaryData <- loadResults(flowSummaryFile)

setGlobalVariable("gRuntime", round(max(summaryData$RelTime)))
cat(sep="", "Runtime=", gRuntime, "\n")


# ====== Begin writing PDF file =============================================
if(!plotOwnFile) { 
   pdfFileName <- paste(sep="", pdfFilePrefix, "-TEMP.pdf")
   pdf(pdfFileName, width=plotWidth, height=plotHeight, paper=plotPaper,
       family=plotFontFamily, pointsize=plotPointSize)
   openPDFMetadata(pdfFileName)
}


# ====== Create plots for active and passive node ===========================
data <- subset(summaryData, (summaryData$IsActive == 1) & (summaryData$FlowID != -1))
plotNodeStats(data, NAME_ACTIVE_NODE)

data <- subset(summaryData, (summaryData$IsActive == 0) & (summaryData$FlowID != -1))
plotNodeStats(data, NAME_PASSIVE_NODE)


# ====== Flow QoS Statistics ================================================
plotQoSStatistics(summaryData, flowSummaryData)


# ====== Finish PDF file ====================================================
if(!plotOwnFile) {
   # ------ Close PDF and embed fonts (using Ghostscript) -------------------
   dev.off()
   closePDFMetadata()

   # ------ Add PDF outlines and meta data ----------------------------------
   cmd1 <- paste(sep="", "pdfmetadata ", pdfFileName, " ", pdfFileName, ".meta ", pdfFilePrefix, "-TEMP2.pdf",
                         " || mv ", pdfFileName, " ", pdfFilePrefix, "-TEMP2.pdf")
#    cat(cmd1,"\n")
   ret1 <- system(cmd1)
   if(ret1 != 0) {
      stop(gettextf("status %d in running command '%s'", ret1, cmd1))
   }

   # ------ Add PDF outlines and meta data ----------------------------------
   cmd2 <- paste(sep="", "pdfembedfonts ", pdfFilePrefix, "-TEMP2.pdf", " ", pdfFilePrefix, ".pdf -optimize",
                         " || mv ", pdfFilePrefix, "-TEMP2.pdf", " ", pdfFilePrefix, ".pdf")
#    cat(cmd2,"\n")
   ret2 <- system(cmd2)
   if(ret2 != 0) {
      stop(gettextf("status %d in running command '%s'", ret2, cmd2))
   }

   # ------ Remove temporary files ------------------------------------------
   unlink(pdfFileName)
   unlink(paste(sep="", pdfFilePrefix, "-TEMP2.pdf"))
   unlink(paste(sep="", pdfFileName, ".meta"))
}
quit(runLast=FALSE)
