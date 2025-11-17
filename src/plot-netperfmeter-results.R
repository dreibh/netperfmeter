# ==========================================================================
#         _   _      _   ____            __ __  __      _
#        | \ | | ___| |_|  _ \ ___ _ __ / _|  \/  | ___| |_ ___ _ __
#        |  \| |/ _ \ __| |_) / _ \ '__| |_| |\/| |/ _ \ __/ _ \ '__|
#        | |\  |  __/ |_|  __/  __/ |  |  _| |  | |  __/ ||  __/ |
#        |_| \_|\___|\__|_|   \___|_|  |_| |_|  |_|\___|\__\___|_|
#
#                  NetPerfMeter -- Network Performance Meter
#                 Copyright (C) 2009-2025 by Thomas Dreibholz
# ==========================================================================
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
# Contact:  dreibh@simula.no
# Homepage: https://www.nntb.no/~dreibh/netperfmeter/

library("data.table", warn.conflicts = FALSE)
library("dplyr",      warn.conflicts = FALSE)
library("ggplot2")


# ###########################################################################
# #### PDF Metadata Handling                                             ####
# ###########################################################################

pdfMetadataFile <- NA
pdfMetadataPage <- NA


# ###### Set global variable (given as name string) to given value ##########
setGlobalVariable <- function(variable, value)
{
   globalEnv <- sys.frame()
   assign(variable, value, envir=globalEnv)
}


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
# #### Plotting Themes                                                   ####
# ###########################################################################

# ###### Plot themes ########################################################
PLOTTHEME_PAPER <- theme(
   legend.position      = "bottom",
   legend.justification = "center",
   legend.title         = element_text(size=12, face = "bold", colour="black", hjust=0.5),
   legend.text          = element_text(size=12),
   legend.key.size      = unit(1.00, "cm"),
   legend.key.width     = unit(1.50, "cm"),
   legend.background    = element_blank(),

   plot.title           = element_blank(),
   axis.title           = element_text(size=12, face="bold"),
   axis.text.x          = element_text(size=10, colour="black"),
   axis.text.y          = element_text(size=10, angle=90, hjust=0.5, colour="black"),
   axis.ticks           = element_line(colour = "black"),

   strip.text           = element_text(size=10, face="bold"),

   panel.border         = element_rect(colour = "black", fill = NA),
   panel.background     = element_blank(),
   panel.grid.major     = element_line(color = "gray40", linewidth = 0.25, linetype = 1),
   panel.grid.minor     = element_line(color = "gray20", linewidth = 0.10, linetype = 2)
)

PLOTTHEME_FANCY <- theme(
   legend.position      = "bottom",
   legend.justification = "center",
   legend.title         = element_text(size=12, face = "bold", colour="black", hjust=0.5),
   legend.text          = element_text(size=12),
   legend.key.size      = unit(1.00, "cm"),
   legend.key.width     = unit(1.50, "cm"),
   # legend.background    = element_rect(colour = "black", fill = "#ffffaa55", linewidth=1),
   # legend.background    = element_rect(colour = "black", fill = NA, linewidth=1),
   legend.background    = element_blank(),

   plot.title           = element_text(size=16, hjust = 0.5, face="bold"),
   axis.title           = element_text(size=12, face="bold"),
   axis.text.x          = element_text(size=10, colour="black"),
   axis.text.y          = element_text(size=10, angle=90, hjust=0.5, colour="black"),
   axis.ticks           = element_line(colour = "black"),

   strip.text           = element_text(size=10, face="bold"),

   panel.border         = element_rect(colour = "black", fill = NA),
   # panel.background     = element_blank(),
   panel.grid.major     = element_line(color = "gray40", linewidth = 0.25, linetype = 1),
   panel.grid.minor     = element_line(color = "gray20", linewidth = 0.10, linetype = 2)
)



# ###########################################################################
# #### Plotting                                                          ####
# ###########################################################################

# ###### Create a plot ######################################################
createPlot <- function(dataSet, title, ySet, yTitle, baseColor, zSet, zTitle,
                       vSet = c(), vTitle = "", bookmarkLevel = 3)
{
   cat(sep="", pdfMetadataPage + 1, ": Plotting ", title, " ...\n")

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

   p <- ggplot(dataSet,
               aes(x     = RelTime,
                   y     = ySet,
                   color = zSet,
                   linetype = vSet)) +
           PLOTTHEME_FANCY +
           theme(panel.border = element_rect(colour = baseColor, fill = NA, linewidth = 2)) +
           geom_line(linewidth = 1) +
           labs(title = title,
                x     = "Time t [s]",
                y     = yTitle,
                color = zTitle,
                linetype = vTitle) +
           guides(color    = guide_legend(ncol = 3),
                  linetype = guide_legend(ncol = 1))
   print(p)

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
   data <- inputData %>% filter( Action != "Lost" )

   # ====== Input/Output Rates ==============================================
   addBookmarkInPDFMetadata(2, "Input/Output Rates")
   createPlot(data, paste(sep="", "Bit Rate Sent/Received on Node ''", nodeName, "''"),
              (data$RelBytes * 8 / 1000) / data$Interval, "Bit Rate [Kbit/s]", "blue4",
              data$Description, "Flow F", data$Action, "Action A")
   createPlot(data, paste(sep="", "Byte Rate Sent/Received on Node ''", nodeName, "''"),
              (data$RelBytes / 1000) / data$Interval, "Byte Rate [KiB/s]", "blue2",
              data$Description, "Flow F", data$Action, "Action A")
   createPlot(data, paste(sep="", "Packet Rate Sent/Received on Node ''", nodeName, "''"),
              data$RelPackets / data$Interval, "Packet Rate [Packets/s]", "green4",
              data$Description, "Flow F", data$Action, "Action A")
   createPlot(data, paste(sep="", "Frame Rate Sent/Received on Node ''", nodeName, "''"),
              data$RelFrames / data$Interval, "Frame Rate [Frames/s]", "yellow4",
              data$Description, "Flow F", data$Action, "Action A")

   # ====== Input/Output Volumes ============================================
   addBookmarkInPDFMetadata(2, "Input/Output Volumes")
   createPlot(data, paste(sep="", "Bits Sent/Received on Node ''", nodeName, "''"),
              (data$AbsBytes * 8 / 1000), "Bits [Kbit]", "blue4",
              data$Description, "Flow F", data$Action, "Action A")
   createPlot(data, paste(sep="", "Bytes Sent/Received on Node ''", nodeName, "''"),
              (data$AbsBytes / 1000), "Bytes [KiB]", "blue2",
              data$Description, "Flow F", data$Action, "Action A")
   createPlot(data, paste(sep="", "Packets Sent/Received on Node ''", nodeName, "''"),
              data$AbsPackets, "Packets [1]", "green4",
              data$Description, "Flow F", data$Action, "Action A")
   createPlot(data, paste(sep="", "Frames Sent/Received on Node ''", nodeName, "''"),
              data$AbsFrames, "Frames [s]", "yellow4",
              data$Description, "Flow F", data$Action, "Action A")

   # ====== Jitter ==========================================================
   addBookmarkInPDFMetadata(2, "Quality of Service")
   data <- inputData %>% filter( Action == "Received" )

   createPlot(data, paste(sep="", "Jitter on Node ''", nodeName, "''"),
              data$Jitter, "Jitter [ms]", "gold4",
              data$Description, "Flow F", data$Action, "Action A")

   # ====== Loss ============================================================
   data <- inputData %>% filter( Action == "Lost" )
   createPlot(data, paste(sep="", "Bit Loss Rate on Node ''", nodeName, "''"),
              (data$RelBytes * 8 / 1000) / data$Interval, "Byte Loss Rate [Kbit/s]", "red4",
              data$Description, "Flow F")
   createPlot(data, paste(sep="", "Byte Loss Rate on Node ''", nodeName, "''"),
              data$RelBytes / data$Interval, "Byte Loss Rate [KiB/s]", "red2",
              data$Description, "Flow F")
   createPlot(data, paste(sep="", "Packet Loss Rate on Node ''", nodeName, "''"),
              data$RelPackets / data$Interval, "Packet Loss Rate [Packets/s]", "red3",
              data$Description, "Flow F")
   createPlot(data, paste(sep="", "Frame Loss Rate on Node ''", nodeName, "''"),
              data$RelFrames / data$Interval, "Frame Loss Rate [Frames/s]", "red1",
              data$Description, "Flow F")
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
         createPlot(data, paste(sep="", "Bit Rate Sent/Received for Flow ''", flowName, "'' on Node ''", nodeName, "''"),
                    (data$RelBytes * 8 / 1000) / data$Interval, "Bit Rate [Kbit/s]", "blue4",
                    data$Action, "Action A", data$Description, "Flow F", bookmarkLevel=5)
         createPlot(data, paste(sep="", "Byte Rate Sent/Received for Flow ''", flowName, "'' on Node ''", nodeName, "''"),
                    (data$RelBytes / 1000) / data$Interval, "Byte Rate [KiB/s]", "blue2",
                    data$Action, "Action A", data$Description, "Flow F", bookmarkLevel=5)
         createPlot(data, paste(sep="", "Packet Rate Sent/Received for Flow ''", flowName, "'' on Node ''", nodeName, "''"),
                    data$RelPackets / data$Interval, "Packet Rate [Packets/s]", "green4",
                    data$Action, "Action A", data$Description, "Flow F", bookmarkLevel=5)
         createPlot(data, paste(sep="", "Frame Rate Sent/Received for Flow ''", flowName, "'' on Node ''", nodeName, "''"),
                    data$RelFrames / data$Interval, "Frame Rate [Frames/s]", "yellow4",
                    data$Action, "Action A", data$Description, "Flow F", bookmarkLevel=5)

         # ====== Input/Output Absolute =====================================
         addBookmarkInPDFMetadata(4, "Input/Output Absolute")
         data <- subset(inputData,
                        (inputData$FlowID == flowLevels[i]) &
                           (inputData$IsActive == isActive) & (inputData$FlowID != -1) )
                           # & (inputData$Action != "Lost"))
         createPlot(data, paste(sep="", "Bits Sent/Received for Flow ''", flowName, "'' on Node ''", nodeName, "''"),
                    (data$AbsBytes * 8 / 1000), "Bits [Kbit]", "blue4",
                    data$Action, "Action A", data$Description, "Flow F", bookmarkLevel=5)
         createPlot(data, paste(sep="", "Bytes Sent/Received for Flow ''", flowName, "'' on Node ''", nodeName, "''"),
                    (data$AbsBytes / 1000), "Bytes [KiB]", "blue2",
                    data$Action, "Action A", data$Description, "Flow F", bookmarkLevel=5)
         createPlot(data, paste(sep="", "Packets Sent/Received for Flow ''", flowName, "'' on Node ''", nodeName, "''"),
                    data$AbsPackets, "Packets [1]", "green4",
                    data$Action, "Action A", data$Description, "Flow F", bookmarkLevel=5)
         createPlot(data, paste(sep="", "Frames Sent/Received for Flow ''", flowName, "'' on Node ''", nodeName, "''"),
                    data$AbsFrames, "Frames [s]", "yellow4",
                    data$Action, "Action A", data$Description, "Flow F", bookmarkLevel=5)

         # ====== Quality of Service ========================================
         addBookmarkInPDFMetadata(4, "Quality of Service")
         data <- subset(flowSummaryData,
                        (flowSummaryData$FlowID == flowLevels[i]) &
                           (flowSummaryData$IsActive == isActive))
         if(length(data$FlowID) > 0) {   # May be empty, if no packets have been sent in this direction!
            createPlot(data, paste(sep="", "Per-Message Delay for Flow ''", flowName, "'' on Node ''", nodeName, "''"),
                       data$Delay, "Delay [ms]", "orange2",
                       c(), NA,   # data$NodeName, "Node{N}",
                       bookmarkLevel=5)
            createPlot(data, paste(sep="", "Per-Message Jitter for Flow ''", flowName, "'' on Node ''", nodeName, "''"),
                       data$Jitter, "Jitter [ms]", "gold4",
                       c(), NA,   # data$NodeName, "Node{N}",
                       bookmarkLevel=5)
         }
      }
   }
}



# ###########################################################################
# #### Main Program                                                      ####
# ###########################################################################

# ###### Default Settings ###################################################
plotOwnFile    <- TRUE
plotFontFamily <- "Helvetica"
plotPointSize  <- 12
plotWidth      <- -1
plotHeight     <- -1
plotPaper      <- "A4r"   # Use "special" for manual values! Or: A4/A4r.


# ###### Command-Line Arguments #############################################
args <- commandArgs(TRUE)
# print(args)
for(i in 1:length(args)) {
   eval(parse(text=args[i]))
}

# cat(sep="", "programDirectory=", programDirectory, "\n")
# cat(sep="", "configFile=",       configFile,       "\n")
# cat(sep="", "summaryFile=",      summaryFile,      "\n")
# cat(sep="", "pdfFilePrefix=",    pdfFilePrefix,    "\n")
# cat(sep="", "perFlowPlots=",     perFlowPlots,     "\n")
# cat(sep="", "plotOwnFile=",      plotOwnFile,      "\n")


# ====== Load input data ====================================================
source(configFile)
summaryData <- data.table(read.csv(summaryFile, sep="\t")) %>%
                  filter(Interval > 0)   # Avoids "divide by zero" on first entry
flowSummaryData <- data.table(read.csv(flowSummaryFile, sep="\t"))

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
data <- summaryData %>% filter( IsActive == 1, FlowID != -1 )
plotNodeStats(data, NAME_ACTIVE_NODE)

data <- summaryData %>% filter( IsActive == 0, FlowID != -1 )
plotNodeStats(data, NAME_PASSIVE_NODE)


# ====== Flow QoS Statistics ================================================
if(perFlowPlots == 1) {
   plotQoSStatistics(summaryData, flowSummaryData)
}


# ====== Finish PDF file ====================================================
if(!plotOwnFile) {
   # ------ Close PDF and embed fonts (using Ghostscript) -------------------
   dev.off()
   closePDFMetadata()

   # ------ Add PDF outlines and meta data ----------------------------------
   cmd1 <- paste(sep="", "setpdfmetadata ", pdfFileName, " ", pdfFileName, ".meta ", pdfFilePrefix, "-TEMP2.pdf",
                         " || mv ", pdfFileName, " ", pdfFilePrefix, "-TEMP2.pdf")
   # cat(cmd1,"\n")
   ret1 <- system(cmd1)
   if(ret1 != 0) {
      stop(gettextf("status %d in running command '%s'", ret1, cmd1))
   }

   # ------ Add PDF outlines and metadata -----------------------------------
   cmd2 <- paste(sep="", "pdfembedfonts ", pdfFilePrefix, "-TEMP2.pdf", " ", pdfFilePrefix, ".pdf -optimize",
                         " || mv ", pdfFilePrefix, "-TEMP2.pdf", " ", pdfFilePrefix, ".pdf")
   # cat(cmd2,"\n")
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
