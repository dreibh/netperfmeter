
# ###########################################################################
# Name:        wp1-rp-sharedBottleneckI
# Description:
# Revision:    $Id$
# ###########################################################################

source("plot-version1.R")


# ------ Plotter Settings ---------------------------------------------------
simulationDirectory  <- "wp1-rp-sharedBottleneckI"
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
   # ------------------------------------------------------------------------


  list(simulationDirectory, "bottleneck-messung.pdf",
        "", NA, NA, list(0,1),
        "RateNorthernTrail-Mbit2", "passive.flow-ReceivedBitRate-Mbit",
        "CMTCCVariant", "passive.flow", "OptionNRSACK",
        "", "ReferenceFlowProtocol", "Runtime",
        "TRUE", 
	 "zColorArray <- c(\"darkgreen\", \"orange\", \"red\", \"blue\", \"gray50\")")
)


# ------ Variable templates -------------------------------------------------
plotVariables <- append(list(

   list("RateNorthernTrail-Mbit2",
           "Common Data Rate{:rho[B]:}[Mbit/s]", "data1$RateNorthernTrail / 1000")
), netPerfMeterPlotVariables)

# ###########################################################################

createPlots(simulationDirectory, plotConfigurations,
            customFilter="sed -f change_names_Flows.sed")

