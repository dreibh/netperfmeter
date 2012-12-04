# ###########################################################################
# Name:        wp1-cc-disjoint-bandwidth-betaI
# Description: CCs in DSL setup
# Revision:    $Id: plot-wp1-cc-disjoint-bandwidth-betaI.R 1255 2011-12-16 12:08:15Z adhari $
# ###########################################################################

source("plot-version1.R")


# ------ Plotter Settings ---------------------------------------------------
simulationDirectorySet  <- list("haikou-Disjoint-local-Essen")
plotColorMode        <- cmColor
plotHideLegend       <- FALSE
plotLegendSizeFactor <- 1.0
plotOwnOutput        <- TRUE
plotFontFamily       <- "Helvetica"
plotFontPointsize    <- 22
plotWidth            <- 10
plotHeight           <- 10
plotConfidence       <- 0.95

# ###########################################################################

# ------ Plots --------------------------------------------------------------
plotConfigurations <- list(


   list(simulationDirectorySet  , "all-protocols-ReceivedBitRate.pdf",
        "", NA, NA, list(1,1),
        "SndBuf", "passive.flow-ReceivedBitRate-Mbit",
        "CMTCCVariant", "Protocol", "ReferenceFlowProtocol",
        "DelayNorthernTrail", "", "RateNorthernTrail-Mbit",
        "TRUE", 
	 "zColorArray <- c(\"darkgreen\", \"orange\", \"red\", \"blue\", \"gray50\")")
)
#RateNorthernTrail-Mbit

# ------ Variable templates -------------------------------------------------
plotVariables <- append(list(
), netPerfMeterPlotVariables)

# ###########################################################################

createPlots(simulationDirectorySet, plotConfigurations)
