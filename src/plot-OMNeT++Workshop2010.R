# ###########################################################################
# Name:        plot-OMNeT++Workshop2010.R
# Description: Plots for the OMNeT++Workshop 2010 paper
# Revision:    $Id$
# ###########################################################################

source("plot-version1.R")


# ------ Plotter Settings ---------------------------------------------------
simulationDirectory  <- ""
plotColorMode        <- cmColor
plotHideLegend       <- FALSE
plotLegendSizeFactor <- 0.8
plotOwnOutput        <- TRUE
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
   #      "filter rule" or NA,
   #      "variable=Value", ...)
   # ------------------------------------------------------------------------

   list("wp0-links-symmetrischI", "OMNeT++Workshop2010-wp0-links-symmetrischI-Throughput.pdf",
        "Measurement Results", seq(0,100000,10000), NA, list(0,1),
        "RateNorthernTrail-CM", "passive.total-ReceivedBitRate",
        "OptionCMT", "OptionDAC", "OptionNRSACK",
        "", "", "Unordered",
        "(data1$OptionDAC == \"true\")",
        NA,
        "dotSet=c(1,5)")
)


# ------ Variable templates -------------------------------------------------
plotVariables <- append(netPerfMeterPlotVariables,
list(
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

   list("RateNorthernTrail-CM",
           "Common Data Rate {:rho:}[Kbit/s]", "data1$RateNorthernTrail"),

   list("OptionCMT",    "CMT", "data1$OptionCMT"),
   list("OptionDAC",    "DAC", "data1$OptionDAC")
))

# ###########################################################################

createPlots(simulationDirectory, plotConfigurations)
