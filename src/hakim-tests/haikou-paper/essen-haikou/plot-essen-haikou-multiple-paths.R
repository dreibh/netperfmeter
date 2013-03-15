# ###########################################################################
# Name:        wp1-cc-disjoint-bandwidth-betaI
# Description: CCs in DSL setup
# Revision:    $Id: plot-wp1-cc-disjoint-bandwidth-betaI.R 1255 2011-12-16 12:08:15Z adhari $
# ###########################################################################

source("plot-version1-multiple-folders.R")


# ------ Plotter Settings ---------------------------------------------------
simulationDirectorySet  <- list("DFN-Cernet-SP", "DFN-Unicom-SP", "Versatel-Cernet-SP", "Versatel-Unicom-SP", "DFN-Cernet-CMT", "DFN-Unicom-CMT", "Versatel-Cernet-CMT", "Versatel-unicom-CMT", "DFN-Cernet-mptcp", "DFN-Unicom-mptcp", "Versatel-Cernet-mptcp", "Versatel-Unicom-mptcp")
#simulationDirectorySet  <- list("DFN-Cernet-SP", "DFN-Unicom-SP", "Versatel-Cernet-SP", "Versatel-Unicom-SP")
#simulationDirectorySet  <- list("/media/storage/Dropbox/Ubuntu-VM-Prince/essen-haikou/")


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

   list(simulationDirectorySet, "haikou-essen_90s.pdf",
        "", seq(0,10,0.5), seq(0,2.5,0.5), list(1,1),
        "OptionBufferSplitting", "passive.flow-ReceivedBitRate-Mbit",
        "CMTCCVariant", "", "",
        #"simulationDirectory", "", "",
        "", "", "",
        "data1$SndBuf==6000000", 
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


#OptionBufferSplitting  bothsides==>path
#CMTCCVariant like-mptcp==> Protocol

