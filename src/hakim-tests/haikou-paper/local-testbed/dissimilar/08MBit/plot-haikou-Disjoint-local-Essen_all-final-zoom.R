# ###########################################################################
# Name:        wp1-cc-disjoint-bandwidth-betaI
# Description: CCs in DSL setup
# Revision:    $Id: plot-wp1-cc-disjoint-bandwidth-betaI.R 1255 2011-12-16 12:08:15Z adhari $
# ###########################################################################

source("plot-version1-multiple-folders.R")


# ------ Plotter Settings ---------------------------------------------------
simulationDirectorySet  <- list("haikou-Disjoint-local-Essen", "haikou-Disjoint-local-Essen-MPTCP-Dissimilar", "haikou-local-Freebsd-SP")# 

plotColorMode        <- cmColor
plotHideLegend       <- FALSE
plotLegendSizeFactor <- 0.9
plotOwnOutput        <- TRUE
plotFontFamily       <- "Helvetica"
plotFontPointsize    <- 22
plotWidth            <- 10
plotHeight           <- 10
plotConfidence       <- 0.95

# ###########################################################################

# ------ Plots --------------------------------------------------------------
plotConfigurations <- list(
# 
#    list(simulationDirectorySet  , "dissimilar-0delay.pdf",
#         "", NA, seq(0,12,2), list(0,1),
#         "RateNorthernTrail-Mbit2", "passive.flow-ReceivedBitRate-Mbit",
#         "Protocol", "CMTCCVariant", "",
#         "", "SndBuf", "",
#         "
#            (data1$DelayNorthernTrail == 0) &
#            (data1$SndBuf == 6000000)
#         ", 
# 	 "zColorArray <- c(\"darkgreen\", \"red\", \"blue\", \"orange\", \"gray50\")"),
  list(simulationDirectorySet  , "dissimilar-200delay-zoom.pdf",
        "", seq(0,3,0.5), seq(0,3,0.5), list(0,1),
        "RateNorthernTrail-Mbit2", "passive.flow-ReceivedBitRate-Mbit",
        "Protocol", "CMTCCVariant", "",
        "", "SndBuf", "",
        "
           (data1$DelayNorthernTrail == 200) &
            (data1$SndBuf == 6000000)
        ", 
	 "zColorArray <- c(\"darkgreen\", \"red\", \"blue\", \"orange\", \"gray50\")")
)


# ------ Variable templates -------------------------------------------------
plotVariables <- append(list(), netPerfMeterPlotVariables)

# ###########################################################################

createPlots(simulationDirectorySet, plotConfigurations)