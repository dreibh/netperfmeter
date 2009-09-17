# ###########################################################################
# Name:        test1
# Description:
# Revision:    $Id: plot-demo1.R 415 2009-07-15 07:34:23Z dreibh $
# ###########################################################################

source("plotter.R")


# ------ Plotter Settings ---------------------------------------------------
simulationDirectory  <- "TEST2"
plotColorMode        <- cmColor
plotHideLegend       <- FALSE
plotLegendSizeFactor <- 0.8
plotOwnOutput        <- FALSE
plotFontFamily       <- "Helvetica"
plotFontPointsize    <- 22
plotWidth            <- 20
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

   list(simulationDirectory, paste(sep="", simulationDirectory, "-ReceivedBytes.pdf"),
        "Receiver's Perspective", NA, NA, list(1,1),
        "Flows", "passive.flow-ReceivedBytes",
        "passive.flow", "OnlyOneAssoc", "",
        "", "", "",
        "(data1$passive.flow <= 2) | ((data1$passive.flow >= 1001) & (data1$passive.flow <= 1002))"),
   list(simulationDirectory, paste(sep="", simulationDirectory, "-ReceivedByteRate.pdf"),
        "Receiver's Perspective", NA, NA, list(1,1),
        "Flows", "passive.total-ReceivedByteRate",
        "RateNorthernTrail", "OptionNRSACK", "",
        "Unordered", "", "",
        ""
   )
)


# ------ Variable templates -------------------------------------------------
plotVariables <- list(
   # ------ Format example --------------------------------------------------
   # list("Variable",
   #         "Unit[x]{v]"
   #          "100.0 * data1$x / data1$y", <- Manipulator expression:
   #                                           "data" is the data table
   #                                        NA here means: use data1$Variable.
   #          "myColor",
   #          list("InputFile1", "InputFile2", ...))
   #             (simulationDirectory/Results/....data.tar.bz2 is added!)
   # ------------------------------------------------------------------------

   list("Flows",
           "Number of Flows{n}[1]", NA),
   list("OnlyOneAssoc",
           "Only One Assoc{A}", NA),

   list("passive.flow",
           "Flow Number{F}", NA),
   list("passive.flow-ReceivedBytes",
           "Received Bytes [MiB]",
           "data1$passive.flow.ReceivedBytes / (1024 * 1024)",
           "blue4",
           list("passive.flow-ReceivedBytes")),
   list("passive.flow-ReceivedByteRate",
           "Received Byte Rate[Kbit/s]",
           "data1$passive.flow.ReceivedByteRate / 1000",
           "blue2",
           list("passive.flow-ReceivedByteRate")),
   list("passive.total-ReceivedByteRate",
           "Received Byte Rate[Kbit/s]",
           "data1$passive.total.ReceivedByteRate / 1000",
           "blue2",
           list("passive.total-ReceivedByteRate"))

)

# ###########################################################################

createPlots(simulationDirectory, plotConfigurations)
