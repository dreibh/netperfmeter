# ###########################################################################
# Name:        wp0-links-symmetrischI
# Description:
# Revision:    $Id$
# ###########################################################################

source("plot-version1.R")


# ------ Plotter Settings ---------------------------------------------------
simulationDirectory  <- "wp0-links-symmetrischI"
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
        "Receiver's Perspective", NA, NA, list(0,1),
        "RateNorthernTrail-Mbit", "passive.flow-ReceivedBitRate-Mbit",
        "CMTCCVariant", "OptionNRSACK", "",
        "passive.flow", "", "Unordered",
        "TRUE",
        "zSortAscending<-FALSE;"),

   list(simulationDirectory, paste(sep="", simulationDirectory, "-ActiveCPUUtilization.pdf"),
        "Sender's Perspective", NA, seq(0, 100, 10), list(0,1),
        "RateNorthernTrail-Mbit", "active.CPU-Utilization",
        "CMTCCVariant", "OptionNRSACK", "",
        "active.CPU", "", "",
        "TRUE",
        "zSortAscending<-FALSE;"),
   list(simulationDirectory, paste(sep="", simulationDirectory, "-PassiveCPUUtilization.pdf"),
        "Receiver's Perspective", NA, seq(0, 100, 10), list(0,1),
        "RateNorthernTrail-Mbit", "passive.CPU-Utilization",
        "CMTCCVariant", "OptionNRSACK", "",
        "passive.CPU", "", "",
        "TRUE",
        "zSortAscending<-FALSE;")
)


# ------ Variable templates -------------------------------------------------
plotVariables <- append(list(
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

), netPerfMeterPlotVariables)

# ###########################################################################

createPlots(simulationDirectory, plotConfigurations)
