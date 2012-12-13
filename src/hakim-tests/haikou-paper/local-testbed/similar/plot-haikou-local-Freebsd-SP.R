# ###########################################################################
# Name:        wp1-cc-disjoint-bandwidth-betaI
# Description: CCs in DSL setup
# Revision:    $Id: plot-wp1-cc-disjoint-bandwidth-betaI.R 1255 2011-12-16 12:08:15Z adhari $
# ###########################################################################

source("plot-version1.R")


# ------ Plotter Settings ---------------------------------------------------
simulationDirectory  <- "haikou-local-Freebsd-SP"
plotColorMode        <- cmColor
plotHideLegend       <- TRUE
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

   list(simulationDirectory  , "similar-2x70MB_zoom.pdf",
        "", seq(0, 1000000, 200000), seq(40, 140, 20), list(1,0.5), 
        "SndBuf", "passive.flow-ReceivedBitRate-Mbit",
        "Protocol", "CMTCCVariant", "",
        "DelayNorthernTrail", "", "RateNorthernTrail",
        "
           (data1$RateNorthernTrail == 70000) &
           (data1$DelayNorthernTrail == 0)
        ", 
	 "zColorArray <- c(\"darkgreen\", \"red\", \"blue\", \"orange\", \"gray50\")"),
	 
    list(simulationDirectory  , "similar-2x70MB.pdf",
        "", NA, seq(40, 140, 20), list(1,0.5),
        "SndBuf", "passive.flow-ReceivedBitRate-Mbit",
        "Protocol", "CMTCCVariant", "",
        "DelayNorthernTrail", "", "RateNorthernTrail",
        "
           (data1$RateNorthernTrail == 70000) &
           (data1$DelayNorthernTrail == 0)
        ", 
	 "zColorArray <- c(\"darkgreen\", \"red\", \"blue\", \"orange\", \"gray50\")")
)


# ------ Variable templates -------------------------------------------------
plotVariables <- append(list(
), netPerfMeterPlotVariables)

# ###########################################################################

createPlots(simulationDirectory, plotConfigurations)
