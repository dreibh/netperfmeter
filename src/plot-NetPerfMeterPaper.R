# ###########################################################################
# Revision:    $Id$
# ###########################################################################

source("plot-version1.R")


# ------ Plotter Settings ---------------------------------------------------
# simulationDirectory  <- "wp3-dsl-unordered-bandwidthI"
plotColorMode        <- cmColor
plotHideLegend       <- FALSE
plotLegendSizeFactor <- 0.8
plotOwnOutput        <- FALSE
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

   list("wp3-dsl-ccI", "NetPerfMeterPaper-wp3-dsl-ccI-SCTP-Main-SCTP.pdf",
        "CMT-SCTP Flow", seq(0,20,2), seq(0,16,2), list(0,1),
        "RateNorthernTrail-Mbit", "passive.flow-ReceivedBitRate-Mbit",
        "CMTCCVariant", "OptionBufferSplitting", "OptionNRSACK",
        "passive.flow", "ReferenceFlowProtocol", "",
        "(data1$ReferenceFlowProtocol == \"SCTP\") &
         (data1$passive.flow == 1)",
        "vSortAscending<-FALSE"),
   list("wp3-dsl-ccI", "NetPerfMeterPaper-wp3-dsl-ccI-SCTP-Reference.pdf",
        "SCTP Reference Flow", seq(0,20,2), seq(0,16,2), list(0,1),
        "RateNorthernTrail-Mbit", "passive.flow-ReceivedBitRate-Mbit",
        "CMTCCVariant", "OptionBufferSplitting", "OptionNRSACK",
        "passive.flow", "ReferenceFlowProtocol", "",
        "(data1$ReferenceFlowProtocol == \"SCTP\") &
         (data1$passive.flow == 1001)",
        "vSortAscending<-FALSE"),

   list("wp3-dsl-ccI", "NetPerfMeterPaper-wp3-dsl-ccI-TCP-Main-TCP.pdf",
        "CMT-SCTP Flow", seq(0,20,2), seq(0,16,2), list(0,1),
        "RateNorthernTrail-Mbit", "passive.flow-ReceivedBitRate-Mbit",
        "CMTCCVariant", "OptionBufferSplitting", "OptionNRSACK",
        "passive.flow", "ReferenceFlowProtocol", "",
        "(data1$ReferenceFlowProtocol == \"TCP\") &
         (data1$passive.flow == 1)",
        "vSortAscending<-FALSE"),
   list("wp3-dsl-ccI", "NetPerfMeterPaper-wp3-dsl-ccI-TCP-Reference.pdf",
        "TCP Reference Flow", seq(0,20,2), seq(0,16,2), list(0,1),
        "RateNorthernTrail-Mbit", "passive.flow-ReceivedBitRate-Mbit",
        "CMTCCVariant", "OptionBufferSplitting", "OptionNRSACK",
        "passive.flow", "ReferenceFlowProtocol", "",
        "(data1$ReferenceFlowProtocol == \"TCP\") &
         (data1$passive.flow == 1001)",
        "vSortAscending<-FALSE")
   
)


# ------ Variable templates -------------------------------------------------
plotVariables <- append(netPerfMeterPlotVariables, list(
   list("passive.flow-ReceivedBitRate-Mbit",
           "Application Payload Throughput[Mbit/s]",
           "8 * data1$passive.flow.ReceivedByteRate / 1000000",
           "blue4",
           list("passive.flow-ReceivedByteRate"))
))

# ###########################################################################

createPlots("NetPerfMeterPaper", plotConfigurations)
