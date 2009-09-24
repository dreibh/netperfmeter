# ###########################################################################
# Name:        test1
# Description:
# Revision:    $Id: plot-demo1.R 415 2009-07-15 07:34:23Z dreibh $
# ###########################################################################

source("plotter.R")


# ------ Plotter Settings ---------------------------------------------------
simulationDirectory  <- "wp0-links-symmetrischI"
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

   list(simulationDirectory, paste(sep="", simulationDirectory, "-ReceivedBitRate.pdf"),
        "Receiver's Perspective", NA, NA, list(1,0),
        "RateNorthernTrail", "passive.total-ReceivedBitRate",
        "OptionCMT", "Unordered", "OptionNRSACK",
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

   list("Flows",        "Number of Flows{n}[1]",     NA),
   list("OnlyOneAssoc", "Only One Assoc{A}",         NA),
   list("Unordered",    "Unordered{U}",              NA),
   list("OptionCMT",    "Allow CMT {:mu:}",          NA),
   list("OptionDAC",    "Delayed Ack CMT {:delta:}", NA),
   list("OptionNRSACK", "Use NR-SACK{:nu:}",         NA),
   list("SndBuf",       "Send Buffer{S}[Bytes]",     NA),
   list("RcvBuf",       "Receive Buffer{R}[Bytes]",  NA),

   list("RateNorthernTrail",
           "Data Rate on Northern Trail {:rho[North]:}[Kbit/s]", NA),
   list("DelayNorthernTrail",
           "Delay on Northern Trail {:delta[North]:}[ms]",
           NA, "black"),
   list("LossNorthernTrail",
           "Loss Rate on Northern Trail {:epsilon[North]:}"),
   list("RateSouthernTrail",
           "Data Rate on Southern Trail {:rho[South]:}[Kbit/s]", NA),
   list("DelaySouthernTrail",
           "Delay on Southern Trail {:delta[South]:}[ms]",
           NA, "black"),
   list("LossSouthernTrail",
           "Loss Rate on Southern Trail {:epsilon[South]:}"),

   list("passive.flow",
           "Flow Number{F}", NA),
   list("passive.flow-ReceivedBytes",
           "Received Bytes [MiB]",
           "data1$passive.flow.ReceivedBytes / (1024 * 1024)",
           "blue4",
           list("passive.flow-ReceivedBytes")),
   list("passive.flow-ReceivedByteRate",
           "Received Byte Rate[KiB/s]",
           "data1$passive.flow.ReceivedByteRate / 1024",
           "blue2",
           list("passive.flow-ReceivedByteRate")),

   list("passive.total-ReceivedByteRate",
           "Received Byte Rate[KiB/s]",
           "data1$passive.total.ReceivedByteRate / 1024",
           "blue2",
           list("passive.total-ReceivedByteRate")),
   list("passive.total-ReceivedBitRate",
           "Received Bit Rate[Kbit/s]",
           "8 * data1$passive.total.ReceivedByteRate / 1000",
           "blue2",
           list("passive.total-ReceivedByteRate"))
)

# ###########################################################################

createPlots(simulationDirectory, plotConfigurations)
