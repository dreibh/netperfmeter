# ###########################################################################
# Name:        wp1-cc-disjoint-bandwidth-betaI
# Description: CCs in DSL setup
# Revision:    $Id: plot-wp1-cc-disjoint-bandwidth-betaI.R 1255 2011-12-16 12:08:15Z adhari $
# ###########################################################################

source("plot-version1-multiple-folders.R")
#"DFN-Cernet-SP", "DFN-Unicom-SP", "Versatel-Cernet-SP", "Versatel-Unicom-SP", "DFN-Cernet-CMT", "DFN-Unicom-CMT", "Versatel-Cernet-CMT", "Versatel-unicom-CMT", "DFN-Cernet-mptcp", "DFN-Unicom-mptcp", "Versatel-Cernet-mptcp", "Versatel-Unicom-mptcp"

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

#   list(simulationDirectorySet  , "dissimilar-200delay.pdf",
#         "", NA, seq(0,10,1), list(0,1),
#         "RateNorthernTrail-Mbit", "passive.flow-ReceivedBitRate-Mbit",
#         "Protocol", "", "",
#         "", "SndBuf", "",
#         "
#            (data1$DelayNorthernTrail == 200) &
#             (data1$SndBuf == 6000000)
#         ", 
# 	"zColorArray <- c(\"darkgreen\", \"red\", \"blue\", \"orange\", \"gray50\")"),
	
	   list(simulationDirectorySet  , "dissimilar-0delay.pdf",
        "", NA, seq(0,10,1), list(0,1),
        "RateNorthernTrail-Mbit", "passive.flow-ReceivedBitRate-Mbit",
        "Protocol", "", "",
        "", "SndBuf", "",
        "
           (data1$DelayNorthernTrail == 0) &
            (data1$SndBuf == 6000000)
        ", 
	"zColorArray <- c(\"darkgreen\", \"red\", \"blue\", \"orange\", \"gray50\")")
)


# ------ Variable templates -------------------------------------------------
plotVariables <- append(list(), netPerfMeterPlotVariables)

# ###########################################################################

createPlots(simulationDirectorySet, plotConfigurations)

#"zColorArray <- c(\"darkgreen\", \"red\", \"blue\", \"orange\", \"gray50\")")