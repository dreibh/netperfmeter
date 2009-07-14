# ###########################################################################
# Name:        test1
# Description:
# Revision:    $Id$
# ###########################################################################

source("plotter.R")


# ------ Plotter Settings ---------------------------------------------------
simulationDirectory  <- "TEST1"
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

   list(simulationDirectory, paste(sep="", simulationDirectory, "-ReceivedBytes.pdf"),
        "Receiver's Perspective", NA, NA, list(1,0),
        "Flows", "passive.flow-ReceivedBytes",
        "passive.flow", "OnlyOneAssoc", "",
        "", "", "")
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
           "Received Bytes",
           "data1$passive.flow.ReceivedBytes",
           "blue4",
           list("passive.flow-ReceivedBytes"))

)

# ###########################################################################

createPlots(simulationDirectory, plotConfigurations)
