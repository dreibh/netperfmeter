# ###########################################################################
# Name:        wp1-cc-disjoint-bandwidth-betaI
# Description: CCs in DSL setup
# Revision:    $Id: plot-wp1-cc-disjoint-bandwidth-betaI.R 1255 2011-12-16 12:08:15Z adhari $
# ###########################################################################

source("plot-version1.R")


# ------ Plotter Settings ---------------------------------------------------
simulationDirectory  <- "haikou-Disjoint-local-Essen" 
plotColorMode        <- cmColor
plotHideLegend       <- FALSE
plotLegendSizeFactor <- 1.0
plotOwnOutput        <- FALSE
plotFontFamily       <- "Helvetica"
plotFontPointsize    <- 22
plotWidth            <- 10
plotHeight           <- 10
plotConfidence       <- 0.95

# ###########################################################################

# ------ Plots --------------------------------------------------------------
plotConfigurations <- list(

   list(simulationDirectory, paste(sep="", simulationDirectory, "-ReceivedBitRate.pdf"),
        "", NA, NA, list(1,0),
        "SndBuf", "passive.flow-ReceivedBitRate-Mbit",
        "DelayNorthernTrail", "CMTCCVariant", "ReferenceFlowProtocol",
        "RateSouthernTrail", "", "DelaySouthernTrail",
        "TRUE", 
	 "zColorArray <- c(\"darkgreen\", \"orange\", \"red\", \"blue\", \"gray50\")")
)
#RateNorthernTrail-Mbit, Runtime, DelayNorthernTrail, CMTCCVariant, RateSouthernTrail, RateNorthernTrail-Mbit, 

# ------ Variable templates -------------------------------------------------
plotVariables <- append(list(), netPerfMeterPlotVariables)

# ###########################################################################

createPlots(simulationDirectory, plotConfigurations)
