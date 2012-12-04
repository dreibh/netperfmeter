# ###########################################################################
# Name:        wp1-cc-disjoint-bandwidth-betaI
# Description: CCs in DSL setup
# Revision:    $Id: plot-wp1-cc-disjoint-bandwidth-betaI.R 1255 2011-12-16 12:08:15Z adhari $
# ###########################################################################

source("plot-version1-multiple-folders.R")


# ------ Plotter Settings ---------------------------------------------------
#simulationDirectorySet  <- list("../../netperfmeter-original/src/haikou-cmt-throughputI", "../../netperfmeter-original/src/haikou-cmt-DFN-unicom-mpctp-west-SP", "../../netperfmeter-original/src/haikou-cmt-Versatel-unicom-mpctp-west-SP" )
# haikou-cmt-DFN-unicom-mpctp-west-SP haikou-cmt-DFN-uninet-mpctp-west-SP haikou-cmt-Versatel-unicom-mpctp-west-SP haikou-cmt-Versatel-uninet-mpctp-west-SP 
simulationDirectorySet  <- list("../../netperfmeter-original/src/haikou-cmt-throughputI", "../../netperfmeter-original/src/haikou-cmt-DFN-unicom-mpctp-west-SP", "../../netperfmeter-original/src/haikou-cmt-DFN-uninet-mpctp-west-SP", "../../netperfmeter-original/src/haikou-cmt-Versatel-unicom-mpctp-west-SP", "../../netperfmeter-original/src/haikou-cmt-Versatel-uninet-mpctp-west-SP")

plotColorMode        <- cmColor
plotHideLegend       <- FALSE
plotLegendSizeFactor <- 0.5
plotOwnOutput        <- TRUE
plotFontFamily       <- "Helvetica"
plotFontPointsize    <- 22
plotWidth            <- 10
plotHeight           <- 10
plotConfidence       <- 0.95

# ###########################################################################

# ------ Plots --------------------------------------------------------------
plotConfigurations <- list(

   list(simulationDirectorySet, "haikou-essen_90s.pdf",
        "90 s laufzeit", seq(0, 20000, 5000), seq(0,2,0.5), list(1,1),
        "SndBuf-Kbyte", "passive.flow-ReceivedBitRate-Mbit",
        "Protocol", "CMTCCVariant", "simDir",
        #"simulationDirectory", "", "",
        "", "", "",
        "    (data1$Runtime == 90) ", 
	 "zColorArray <- c(\"darkgreen\", \"orange\", \"red\", \"blue\", \"gray50\")")
# 	 ,
# 	 
#    list(simulationDirectorySet, "haikou-essen_300s.pdf",
#         "300 s laufzeit", seq(0, 20000, 5000), NA, list(1,1),
#         "SndBuf-Kbyte", "passive.flow-ReceivedBitRate-Mbit",
#         "Protocol", "CMTCCVariant", "",
#         "", "", "",
#         "    (data1$Runtime == 300) ", 
# 	 "zColorArray <- c(\"darkgreen\", \"orange\", \"red\", \"blue\", \"gray50\")")
)
#RateNorthernTrail-Mbit

# ------ Variable templates -------------------------------------------------
plotVariables <- append(list(), netPerfMeterPlotVariables)

# ###########################################################################

createPlots(simulationDirectory, plotConfigurations)

