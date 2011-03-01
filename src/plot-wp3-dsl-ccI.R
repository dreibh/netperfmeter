# ###########################################################################
# Name:        wp3-dsl-ccI
# Description: Buffer Splitting on DSL setup
# Revision:    $Id$
# ###########################################################################

source("plot-version1.R")


# ------ Plotter Settings ---------------------------------------------------
simulationDirectory  <- "wp3-dsl-ccI"
plotColorMode        <- cmColor
plotHideLegend       <- FALSE
plotLegendSizeFactor <- 0.8
plotOwnOutput        <- FALSE
plotFontFamily       <- "Helvetica"
plotFontPointsize    <- 22
plotWidth            <- 10
plotHeight           <- 10
plotConfidence       <- 0.95

# ###########################################################################

# ------ Plots --------------------------------------------------------------
plotConfigurations <- list(
   # ------ Format example --------------------------------------------------
   # list(simulationDirectory, "output.pdf",
   #      "Plot Title",
   #      list(xAxisTicks) or NA, list(yAxisTicks) or NA, list(legendPos) or NA,
   #      "x-Axis Variable", "y-Axis Variable",
   #      "z-Axis Variable", "v-Axis Variable", "w-Axis Variable",
   #      "a-Axis Variable", "b-Axis Variable", "p-Axis Variable")
   # ------------------------------------------------------------------------

   list(simulationDirectory, paste(sep="", simulationDirectory, "-ReceivedBitRate.pdf"),
        "Received Bit Rate per Flow", NA, NA, list(0,1),
        "RateNorthernTrail-Mbit", "passive.flow-ReceivedBitRate-Mbit",
        "CMTCCVariant", "OptionBufferSplitting", "OptionNRSACK",
        "passive.flow", "ReferenceFlowProtocol", "",
        "TRUE", "vSortAscending<-FALSE"),

   list(simulationDirectory, paste(sep="", simulationDirectory, "-ActiveCPUIdle.pdf"),
        "Sender's Perspective", NA, seq(0, 100, 10), list(0,1),
        "RateNorthernTrail-Mbit", "active.CPU-Utilization",
        "CMTCCVariant", "OptionBufferSplitting", "OptionNRSACK",
        "passive.flow", "ReferenceFlowProtocol", "",
        "TRUE", "vSortAscending<-FALSE"),
   list(simulationDirectory, paste(sep="", simulationDirectory, "-PassiveCPUIdle.pdf"),
        "Receiver's Perspective", NA, seq(0, 100, 10), list(0,1),
        "RateNorthernTrail-Mbit", "passive.CPU-Utilization",
        "CMTCCVariant", "OptionBufferSplitting", "OptionNRSACK",
        "passive.flow", "ReferenceFlowProtocol", "",
        "TRUE", "vSortAscending<-FALSE"),

   list(simulationDirectory, paste(sep="", simulationDirectory, "-ActiveCPUSystem.pdf"),
        "Sender's Perspective", NA, seq(0, 100, 10), list(0,1),
        "RateNorthernTrail-Mbit", "active.CPU-System",
        "CMTCCVariant", "OptionBufferSplitting", "OptionNRSACK",
        "passive.flow", "ReferenceFlowProtocol", "",
        "TRUE", "vSortAscending<-FALSE"),
   list(simulationDirectory, paste(sep="", simulationDirectory, "-ActiveCPUSystem.pdf"),
        "Receiver's Perspective", NA, seq(0, 100, 10), list(0,1),
        "RateNorthernTrail-Mbit", "passive.CPU-System",
        "CMTCCVariant", "OptionBufferSplitting", "OptionNRSACK",
        "passive.flow", "ReferenceFlowProtocol", "",
        "TRUE", "vSortAscending<-FALSE")
)


# ------ Variable templates -------------------------------------------------
plotVariables <- append(list(
), netPerfMeterPlotVariables)

# ###########################################################################

createPlots(simulationDirectory, plotConfigurations)
