# ###########################################################################
# Name:        test1
# Description:
# Revision:    $Id: plot-wp0-links-symmetrischI.R 534 2010-01-22 12:29:37Z dreibh $
# ###########################################################################

source("plot-version1.R")


# ------ Plotter Settings ---------------------------------------------------
simulationDirectory  <- "TEST3"
plotColorMode        <- cmColor
plotHideLegend       <- FALSE
plotLegendSizeFactor <- 0.8
plotOwnOutput        <- FALSE
plotFontFamily       <- "Helvetica"
plotFontPointsize    <- 22
plotWidth            <- 20
plotHeight           <- 20
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
        "OptionRP", "OptionNRSACK", "BidirectionalQoS",
        "passive.flow", "OptionCMT", "Unordered"),
   list(simulationDirectory, paste(sep="", simulationDirectory, "-ReceivedBytes.pdf"),
        "Received Bytes per Flow", NA, NA, list(0,1),
        "RateNorthernTrail-Mbit", "passive.flow-ReceivedBytes",
        "OptionRP", "OptionNRSACK", "BidirectionalQoS",
        "passive.flow", "OptionCMT", "Unordered"),
   list(simulationDirectory, paste(sep="", simulationDirectory, "-ReceivedBytes.pdf"),
        "Received Packets per Flow", NA, NA, list(0,1),
        "RateNorthernTrail-Mbit", "passive.flow-ReceivedPackets",
        "OptionRP", "OptionNRSACK", "BidirectionalQoS",
        "passive.flow", "OptionCMT", "Unordered")
)


# ------ Variable templates -------------------------------------------------
plotVariables <- append(list(
), netPerfMeterPlotVariables)

# ###########################################################################

createPlots(simulationDirectory, plotConfigurations)
