# ###########################################################################
# Name:        wp3-dsl-bufferSplittingI
# Description: Buffer Splitting on DSL setup
# Revision:    $Id$
# ###########################################################################

source("plot-version1.R")


# ------ Plotter Settings ---------------------------------------------------
# simulationDirectory  <- "wp3-dsl-unordered-bandwidthI"
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

   list("wp3-dsl-unordered-bandwidthI", "PAMS2011-measurement-wp3-dsl-unordered-bandwidthI.pdf",
        "Measurement in Testbed Setup", seq(0,10,1), seq(0,11,1), list(0,1),
        "RateNorthernTrail-Mbit", "passive.flow-ReceivedBitRate-Mbit",
        "OptionBufferSplitting", "OptionNRSACK", "",
        "passive.flow", "OptionRP", "CwndMaxBurst",
        "
            ((data1$OptionBufferSplitting == \"none\") | (data1$OptionBufferSplitting == \"bothSides\"))
        ",
        "vSortAscending<-FALSE")
)


# ------ Variable templates -------------------------------------------------
plotVariables <- append(netPerfMeterPlotVariables, list(
   list("RateNorthernTrail-Mbit",
           "Bandwidth on High-Speed Path{:beta:}[Mbit/s]",
           "data1$RateNorthernTrail / 1000"),
   list("passive.flow-ReceivedBitRate-Mbit",
           "CMT-SCTP Payload Throughput [Mbit/s]",
           "8 * data1$passive.flow.ReceivedByteRate / 1000000",
           "blue4",
           list("passive.flow-ReceivedByteRate")),
   list("OptionBufferSplitting", "Buffer Splitting{:Pi:}"),
   list("OptionNRSACK",          "Use NR-SACK{:rho:}")
))

# ###########################################################################

createPlots("PAMS2011", plotConfigurations)
