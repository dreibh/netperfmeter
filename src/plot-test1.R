# ###########################################################################
# Name:        test1
# Description:
# Revision:    $Id$
# ###########################################################################

source("plot-version1.R")


# ------ Plotter Settings ---------------------------------------------------
simulationDirectory  <- "test1"
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
        "Receiver's Perspective", NA, NA, list(0,0),
        "RateNorthernTrail-Mbit", "passive.flow-ReceivedBitRate-Mbit",
        "passive.flow", "", "",
        "", "", ""),

   list(simulationDirectory, paste(sep="", simulationDirectory, "-ActiveCPUIdle.pdf"),
        "Sender's Perspective", NA, seq(0, 100, 10), list(0,0),
        "RateNorthernTrail-Mbit", "active.CPU-Utilization",
        "active.CPU", "", "",
        "", "", ""),
   list(simulationDirectory, paste(sep="", simulationDirectory, "-PassiveCPUIdle.pdf"),
        "Receiver's Perspective", NA, seq(0, 100, 10), list(0,0),
        "RateNorthernTrail-Mbit", "passive.CPU-Utilization",
        "passive.CPU", "", "",
        "", "", ""),

   list(simulationDirectory, paste(sep="", simulationDirectory, "-ActiveCPUSystem.pdf"),
        "Sender's Perspective", NA, seq(0, 100, 10), list(0,0),
        "RateNorthernTrail-Mbit", "active.CPU-System",
        "active.CPU", "", "",
        "", "", ""),
   list(simulationDirectory, paste(sep="", simulationDirectory, "-ActiveCPUSystem.pdf"),
        "Receiver's Perspective", NA, seq(0, 100, 10), list(0,0),
        "RateNorthernTrail-Mbit", "passive.CPU-System",
        "passive.CPU", "", "",
        "", "", "")
)


# ------ Variable templates -------------------------------------------------
plotVariables <- append(list(
), netPerfMeterPlotVariables)

# ###########################################################################

createPlots(simulationDirectory, plotConfigurations)
