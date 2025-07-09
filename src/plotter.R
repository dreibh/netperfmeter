# ###########################################################################
#             Thomas Dreibholz's R Simulation Scripts Collection
#                  Copyright (C) 2005-2025 Thomas Dreibholz
#
#               Author: Thomas Dreibholz, thomas.dreibholz@gmail.com
# ###########################################################################
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Contact: thomas.dreibholz@gmail.com


# ###########################################################################
# #### Utility Functions                                                 ####
# ###########################################################################


# ====== Check existence of global variable (given as name string) ==========
existsGlobalVariable <- function(variable)
{
   globalEnv <- sys.frame()
   return(exists(variable, envir=globalEnv))
}


# ====== Get value of global variable (given as name string) ================
getGlobalVariable <- function(variable)
{
   globalEnv <- sys.frame()
   return(get(variable, env=globalEnv))
}


# ====== Set global variable (given as name string) to given value ==========
setGlobalVariable <- function(variable, value)
{
   globalEnv <- sys.frame()
   assign(variable, value, envir=globalEnv)
}


# ====== Safe division ======================================================
safeDiv <- function(a, b, zeroValue=0) {
   c <- a / b
   nullSet <- is.nan(c)
   result <- replace(c, nullSet, zeroValue)
   return(result)
}



# ###########################################################################
# #### Core Plotting Functions                                           ####
# ###########################################################################


# ====== Get array of gray tones
# (equivalent of rainbow() for b/w laser printing) ==========================
graybow <- function(n)
{
   maxGray <- 0.5   # Light gray
   minGray <- 0.0   # Black
   return(gray(seq(maxGray, minGray, -((maxGray - minGray) / n))))
}


# ====== Modified rainbow() with colors improved for readability ============
rainbow2 <- function(n)
{
   if(n == 2) {
      return(c("red", "blue"))   # rot/blau statt rot/türkis!
   }
   else if(n <= 3) {
      return(rainbow(n))
   }
#    else if(n == 4) {
#       return(c("red", "#00aa00", "black", "blue"))
#    }
#    else if(n == 5) {
#       return(c("red", "#00aa00", "black", "blue", "magenta"))
#    }
   return(rainbow(n))
}


# ====== Get background color ===============================================
cmColor <- 2
cmGrayScale <- 1
cmBlackAndWhite <- 0
getBackgroundColor <- function(index, colorMode = cmColor, pStart = 0)
{
   if(pStart >= 0) {
      if(colorMode == cmColor) {
         bgColorSet <- c("#ffffe0", "#ffe0ff", "#e0ffff", "#ffe0e0", "#e0ffe0", "#e0e0ff", "#e0e0c0", "#eeeeee", "#000000")
      }
      else if(colorMode == cmGrayScale) {
         bgColorSet <- c("#ffffff", "#f8f8f8", "#f0f0f0", "#e8e8e8", "#e0e0e0", "#d8d8d8", "#d0d0d0", "#c8c8c8", "#000000")
      }
      else {
         bgColorSet <- c("#ffffff", "#ffffff", "#ffffff", "#ffffff", "#ffffff", "#ffffff", "#ffffff", "#ffffff", "#ffffff")
      }
      bgColor <- bgColorSet[(pStart + (index - 1)) %% length(bgColorSet)]
   }
   else {
      bgColor <- "#ffffff"
   }
   return(bgColor)
}


# --- Mit Escapces ---   titleRegExpr <- "([^\[\{]*)([\{]([^\}]*)[\}]){0,1}([^\[]*)([\[]([^\]]*)[\]]){0,1}"
titleRegExpr <- "([^[{]*)([{]([^}]*)[}]){0,1}([^[]*)([[]([^]]*)[]]){0,1}"
removeSpaceRegExpr <- "([[:space:]]*)([^ ]*)([[:space:]]*)"
expressionExpr <- ":([^:]*):"


# ====== Strip whitespaces from front and back of string ====================
trim <- function(str)
{
   str <- sub("^ +", "", str)
   str <- sub(" +$", "", str)
   return(str)
}


# ====== Extract variable from axis title ===================================
getVariable <- function(title)
{
   result <- sub(titleRegExpr, "\\1", title)
   e <- sub(expressionExpr, "\\1", result)
   if(e == result) {
      return(paste(sep="", "paste(\"", trim(result), "\")"))
   }
   return(e)
}


# ====== Extract abbreviated variable name from axis title ==================
getAbbreviation <- function(title)
{
   result <- sub(titleRegExpr, "\\3", title)
   if(result == "") {
      return(getVariable(title))
   }
   e <- sub(expressionExpr, "\\1", result)
   if(e == result) {
      return(paste(sep="", "paste(\"", trim(result), "\")"))
   }
   return(e)
}


# ====== Extract unit from axis title =======================================
getUnit <- function(title)
{
   result <- sub(titleRegExpr, "\\6", title)
   e <- sub(expressionExpr, "\\1", result)
   if(e == result) {
      return(paste(sep="", "\"", trim(result), "\""))
   }
   return(e)
}


# ====== Extract label expression (as string!) from title ===================
getLabel <- function(title)
{
   label <- getVariable(title)
   if(getAbbreviation(title) != getVariable(title)) {
      label <- paste(sep="", "paste(sep=\"\", ", label, ", \" \", ", getAbbreviation(title), ")")
   }
   if(getUnit(title) != "\"\"") {   # unit is "empty string expression"
      label <- paste(sep="", "paste(sep=\"\", ", label, ", \" [\", ", getUnit(title), ", \"]\")")
   }

   return(label)
}


# ====== Get dot style from dot set =========================================
getDot <- function(dotSet, dot)
{
   if(length(dotSet) == 0) {
      dotSet <- setdiff(seq(1, 99, 1),
                        c(11))   # List of difficult to differentiate dots
   }
   selectedDot <- dotSet[1 + ((dot - 1) %% length(dotSet))]
   return(selectedDot)
}


# ====== Check sets =========================================================
checkSets <- function(data,
                      xSet=c(), ySet=c(), zSet=c(),
                      vSet=c(), wSet=c(),
                      aSet=c(), bSet=c(), pSet=c(),
                      runNoSet=c())
{
   if(length(xSet) < 1) {
      stop("ERROR: checkSets: xSet is empty!")
   }
   if( (length(xSet) != length(ySet)) ) {
      stop("ERROR: checkSets: xSet and ySet lengths differ!")
   }
   if( (length(zSet) > 0) && (length(xSet) != length(zSet)) ) {
      stop("ERROR: checkSets: xSet and zSet length differ!")
   }
   if( (length(vSet) > 0) && (length(xSet) != length(vSet)) ) {
      stop("ERROR: checkSets: xSet and vSet length differ!")
   }
   if( (length(wSet) > 0) && (length(xSet) != length(wSet)) ) {
      stop("ERROR: checkSets: xSet and wSet length differ!")
   }
   if( (length(aSet) > 0) && (length(xSet) != length(aSet)) ) {
      stop("ERROR: checkSets: xSet and aSet length differ!")
   }
   if( (length(bSet) > 0) && (length(xSet) != length(bSet)) ) {
      stop("ERROR: checkSets: xSet and bSet length differ!")
   }
   if( (length(pSet) > 0) && (length(xSet) != length(pSet)) ) {
      stop("ERROR: checkSets: xSet and pSet length differ!")
   }

   if(length(runNoSet) > 0) {
      runs <- length(levels(factor(runNoSet)))

      zFilter <- TRUE
      if(length(zSet) > 0) {
         zFilter <- (zSet == zSet[1])
      }
      vFilter <- TRUE
      if(length(vSet) > 0) {
         vFilter <- (vSet == vSet[1])
      }
      wFilter <- TRUE
      if(length(wSet) > 0) {
         wFilter <- (wSet == wSet[1])
      }
      aFilter <- TRUE
      if(length(aSet) > 0) {
         aFilter <- (aSet == aSet[1])
      }
      bFilter <- TRUE
      if(length(bSet) > 0) {
         bFilter <- (bSet == bSet[1])
      }
      pFilter <- TRUE
      if(length(pSet) > 0) {
         pFilter <- (pSet == pSet[1])
      }

      filter <- (xSet == xSet[1]) &
                zFilter & vFilter & wFilter & aFilter & bFilter & pFilter
      ySubset <- subset(data$ValueNo, filter)
      n <- length(ySubset)

      if(n != runs) {
         cat(sep="", "ERROR: checkSets: Number of values differs from number of runs!\n",
                     "      n=", n, " expected=", runs, "\n",
                     "      ySubset=")
         print(ySubset)
         stop("Aborted.")
      }
   }
}


# ====== Default hbar aggregator ============================================
hbarDefaultAggregator <- function(xSet, ySet, hbarSet, zValue, confidence)
{
   mSet <- ySet
   mMean <- mean(mSet)
   mMin <- min(mSet)
   mMax <- max(mSet)
   if(mMin != mMax) {
      mTest <- t.test(mSet, conf.level=confidence)
      mMin  <- mTest$conf.int[1]
      mMax  <- mTest$conf.int[2]
   }
   return(c(mMean,mMin,mMax))
}


# ====== Handling Speed aggregator ==========================================
hbarHandlingSpeedAggregator <- function(xSet, ySet, hbarSet, zValue, confidence)
{
   handlingTime  <- 60 * (xSet - hbarSet)
   handlingSpeed <- ySet
   totalHandlingTime <- 0
   totalJobSize <- 0
   for(i in seq(1, length(handlingTime))) {
      totalHandlingTime <- totalHandlingTime + handlingTime[i]
      totalJobSize <- totalJobSize + (handlingTime[i] * handlingSpeed[i])
   }
   mMean <- totalJobSize / totalHandlingTime
   mMin <- mMean
   mMax <- mMean
   return(c(mMean,mMin,mMax))
}


# ====== Default Pre-Plot Function ==========================================
defaultPrePlotFunction <- function(xRange, yRange, zColorArray,
                                   lineWidthScaleFactor, dotScaleFactor,
                                   xSet, ySet, zSet, vSet, wSet)
{
}


# ====== Default Pre-Plot Function ==========================================
defaultPostPlotFunction <- function(xRange, yRange, zColorArray,
                                   lineWidthScaleFactor, dotScaleFactor,
                                   xSet, ySet, zSet, vSet, wSet)
{
}


# Plot x/y plot with different curves as z with confidence intervals in
# y direction. x and z can be numeric or strings, y must be numeric since
# confidence intervals have to be computed.
plotstd3 <- function(mainTitle,
                     xTitle, yTitle, zTitle,
                     xSet, ySet, zSet,
                     vSet                 = c(),
                     wSet                 = c(),
                     vTitle               = "??vTitle??",
                     wTitle               = "??wTitle??",
                     xAxisTicks           = c(),
                     yAxisTicks           = c(),
                     type                 = "lines",
                     confidence           = 0.95,
                     pointsSize           = 2,
                     legendPos            = c(0,1),
                     legendSize           = 0.8,
                     lineWidthScaleFactor = 1.0,
                     colorMode            = cmColor,
                     frameColor           = par("fg"),
                     zColorArray          = c(),
                     zReverseColors       = FALSE,
                     zSortAscending       = TRUE,
                     vSortAscending       = TRUE,
                     wSortAscending       = TRUE,
                     dotSet               = c(),
                     dotScaleFactor       = 2,
                     hbarSet              = c(),
                     hbarMeanSteps        = 10,
                     hbarAggregator       = hbarDefaultAggregator,
                     xSeparatorsSet       = c(),
                     xSeparatorsTitles    = c(),
                     xSeparatorsColors    = c(),
                     rangeSet             = c(),
                     rangeColors          = c(),
                     hideCurves           = c(),
                     hideLegend           = FALSE,
                     legendOnly           = FALSE,
                     enumerateLines       = FALSE,
                     xValueFilter         = "%s",
                     yValueFilter         = "%s",
                     zValueFilter         = "%s",
                     vValueFilter         = "%s",
                     wValueFilter         = "%s",
                     prePlotFunction      = defaultPrePlotFunction,
                     postPlotFunction     = defaultPostPlotFunction,
                     writeMetadata        = TRUE,
                     largeMargins         = FALSE,
                     aLevels              = 1,
                     bLevels              = 1)
{
   if(length(zSet) < 1) {
      zSet   <- rep(0, length(ySet))
      zTitle <- ""
      hideLegend <- FALSE
   }
   if(length(wSet) < 1) {
      wSet <- rep(0, length(zSet))
   }
   if(length(vSet) < 1) {
      vSet <- rep(0, length(zSet))
   }

   xLevels <- levels(factor(xSet))
   yLevels <- levels(factor(ySet))
   zLevels <- levels(factor(zSet))
   vLevels <- levels(factor(vSet))
   wLevels <- levels(factor(wSet))
   if(length(xLevels) < 1) {
      cat("WARNING: plotstd3() - xLevels=c()\n")
      return(0);
   }
   if(length(yLevels) < 1) {
      cat("WARNING: plotstd3() - yLevels=c()\n")
      return(0);
   }
   if(length(zLevels) < 1) {
      cat("WARNING: plotstd3() - zLevels=c()\n")
      return(0);
   }

   if(length(zColorArray) == 0) {
      if(colorMode == cmColor) {
         zColorArray <- rainbow2(length(zLevels))
      }
      else if(colorMode == cmGrayScale) {
         zColorArray <- graybow(length(zLevels))
         frameColor  <- par("fg")
      }
      else {
         zColorArray <- rep(par("fg"), length(zLevels))
         frameColor  <- par("fg")
      }
   }

   xRange <- range(as.numeric(xSet), finite=TRUE)
   yRange <- range(as.numeric(ySet), finite=TRUE)
   if(missing(xAxisTicks) || (length(xAxisTicks) == 0) || nlevels(xSet)) {
      xAxisTicks <- xLevels
   }
   else {
      xRange <- range(as.numeric(xAxisTicks), finite=TRUE)
   }
   if(missing(yAxisTicks) || (length(yAxisTicks) == 0)) {
      yAxisTicks <- yLevels
   }
   else {
      yRange <- range(as.numeric(yAxisTicks), finite=TRUE)
   }


   # ------ Create plot window ----------------------------------------------
   if(!largeMargins) {
      margins <- c(3.25,3.25,3,0.5) + 0.0   # Margins as c(bottom, left, top, right)
                                            # Default is c(5, 4, 4, 2) + 0.1
      newCEX  <- par("cex")
   }
   else {
      margins <- c(4, 4, 2, 2) + 0.0   # For usage within plotstd6()
      q <- 0.5 * max(aLevels, bLevels)
      newCEX  <- par("cex") / q
   }
   opar <- par(mar = margins, cex=newCEX)

   if(writeMetadata) {
      nextPageInPDFMetadata()
   }
   plot.new()
   plot.window(xRange, yRange)

   if(!legendOnly) {
      if(nlevels(xSet)) {
         axis(1, seq(length(xLevels)), xLevels, col=frameColor, col.axis=frameColor)
      }
      else {
         axis(1, xAxisTicks, col=frameColor, col.axis=frameColor)
      }
      axis(2, yAxisTicks, col=frameColor, col.axis=frameColor)

      # ------ Range colors -------------------------------------------------
      if(colorMode == cmColor) {
         if(length(rangeSet) >= 2) {
            colorNumber <- 1
            xLow <- rangeSet[1]
            for(r in rangeSet[2:length(rangeSet)]) {
               rangeColor <- rangeColors[colorNumber %% (length(rangeColors) + 1)]
               rect(min(xRange) - (max(xRange) - min(xRange)), xLow,
                    max(xRange) + (max(xRange) - min(xRange)), r, col=rangeColor)
               xLow <- r

               colorNumber <- colorNumber + 1
            }
         }
      }

      # ------ Axis and labels ----------------------------------------------
      grid(20, 20, lty=1)
      # grid(NULL, NULL, lty=3)
      box(col=frameColor)
      xLabel <- getLabel(xTitle)
      yLabel <- getLabel(yTitle)

      mtext(parse(text=xLabel), col=frameColor,
            side = 1, adj=0.5, line=2.25,
            xpd = NA, font = par("font.main"), cex = par("cex"))
      mtext(parse(text=yLabel), col=frameColor,
            side = 2, adj=0.5, line=2.25,
            xpd = NA, font = par("font.main"), cex = par("cex"))
      mtext(parse(text=getLabel(mainTitle)), col=frameColor,
            side = 3, adj=0.5, line=1.75,
            xpd = NA, font = par("font.main"), cex = sqrt(2) * par("cex"))

      zvwLabel <- getLabel(zTitle)
      if(length(vLevels) > 1) {
         zvwLabel <- paste(sep="", "paste(", zvwLabel, ", \" / \", ", getLabel(vTitle), ")")
      }
      if(length(wLevels) > 1) {
         zvwLabel <- paste(sep="", "paste(", zvwLabel, ", \" / \", ", getLabel(wTitle), ")")
      }

      mtext(parse(text=zvwLabel), col=frameColor,
            side = 3, line=0.5, adj=1,
            xpd = NA, font = par("font.main"), cex = par("cex"))
   }
   lineWidth <- lineWidthScaleFactor

   # ------ Call to pre-plot function ---------------------------------------
   prePlotFunction(xRange, yRange, zColorArray, lineWidth, dotScaleFactor, xSet, ySet, zSet, vSet, wSet)


   # ------ Plot curves -----------------------------------------------------
   lineNum      <- 1
   legendTexts  <- c()
   legendColors <- c()
   legendStyles <- c()
   legendDots   <- c()
   legendDot    <- 1
   if(!zSortAscending) {
      zLevels <- rev(zLevels)
   }
   if(zReverseColors) {
      zColorArray <- rev(zColorArray)
   }
   if(!vSortAscending) {
      vLevels <- rev(vLevels)
   }
   if(!wSortAscending) {
      wLevels <- rev(wLevels)
   }
   for(zPosition in 1:length(zLevels)) {
      z <- zLevels[zPosition]
      legendColor <- zColorArray[zPosition]
      legendStyle <- 1
      for(vPosition in 1:length(vLevels)) {
         v <- vLevels[vPosition]
         for(wPosition in 1:length(wLevels)) {
            w <- wLevels[wPosition]
            # ----- Legend settings -----------------------------------------
            legendText <- ""
            zBinder <- ""
            if( (!is.null(zTitle)) && (zTitle != "") ) {
               zBinder <- "="
            }
            if( (zBinder != "") || (length(levels(factor(zSet))) > 1) ) {
               legendText <- paste(sep="", "paste(sep=\"\", ", getAbbreviation(zTitle), ", '", zBinder, gettextf(zValueFilter, z), "')")
            }
            vBinder <- ""
            if( (!is.null(vTitle)) && (vTitle != "") ) {
               vBinder <- "="
            }
            if(length(vLevels) > 1) {
               legendText <- paste(sep="", "paste(sep=\"\", ", legendText, ", \", \", ", getAbbreviation(vTitle), ", '", vBinder, gettextf(vValueFilter, v), "')")
            }
            wBinder <- ""
            if( (!is.null(wTitle)) && (wTitle != "") ) {
               wBinder <- "="
            }
            if(length(wLevels) > 1) {
               legendText <- paste(sep="", "paste(sep=\"\", ", legendText, ", \", \", ", getAbbreviation(wTitle), ", '", wBinder, gettextf(wValueFilter, w), "')")
            }
            if(enumerateLines) {
               lineNumText <- paste(sep="", lineNum)
               legendText <- paste(sep="", "paste(sep=\"\", ", ", \"", lineNumText, ": \", ", legendText, ")")
            }


            if( (length(hideCurves) > 0) && (length(intersect(c(lineNum), hideCurves)) > 0) ) {
               legendDot   <- legendDot + 1
               legendStyle <- (legendStyle + 1) %% 7
               lineNum     <- lineNum + 1
            }
            else if(!legendOnly) {
               # ----- Points plot ------------------------------------------
               if((type == "p") || (type=="points")) {
                  xSubset <- subset(xSet, (zSet == z) & (vSet == v) & (wSet == w))
                  ySubset <- subset(ySet, (zSet == z) & (vSet == v) & (wSet == w))
                  points(xSubset, ySubset, col=legendColor, cex=par("cex"), pch=getDot(dotSet, legendDot))

                  legendTexts  <- append(legendTexts,  legendText)
                  legendColors <- append(legendColors, legendColor)
                  legendStyles <- append(legendStyles, legendStyle)
                  legendDots   <- append(legendDots,   getDot(dotSet, legendDot))
                  legendDot    <- legendDot + 1
                  lineNum      <- lineNum + 1
               }

               # ----- Horizontal bars plot ---------------------------------
               else if((type == "h") || (type=="hbars")) {
                  xSubset <- subset(xSet, (zSet == z) & (vSet == v) & (wSet == w))
                  ySubset <- subset(ySet, (zSet == z) & (vSet == v) & (wSet == w))
                  hbarSubset <- subset(hbarSet, (zSet == z) & (vSet == v) & (wSet == w))
                  for(x in seq(1, length(xSubset))) {
                     # points(xSubset[x],ySubset[x], col=legendColor, cex=par("cex"), pch=getDot(dotSet, legendDot))
                     lines(c(hbarSubset[x], xSubset[x]),
                           c(ySubset[x], ySubset[x]),
                           col=legendColor, cex=par("cex"), pch=getDot(dotSet, legendDot))
                  }
                  legendTexts  <- append(legendTexts,  legendText)
                  legendColors <- append(legendColors, legendColor)
                  legendStyles <- append(legendStyles, legendStyle)
                  legendDots   <- append(legendDots,   getDot(dotSet, legendDot))
                  legendDot    <- legendDot + 1
                  lineNum      <- lineNum + 1
               }

               # ----- Lines or Steps plot without confidence intervals -----
               else if((type == "lx") || (type=="linesx") ||
                       (type == "sx") || (type=="stepsx")) {
                  xSubset <- subset(xSet, (zSet == z) & (vSet == v) & (wSet == w))
                  ySubset <- subset(ySet, (zSet == z) & (vSet == v) & (wSet == w))
                  if(length(xSubset) > 0) {
                     lineWidth <- 5*lineWidthScaleFactor
                     if((length(vLevels) > 1) || (length(wLevels) > 1)) {
                        lineWidth <- 3*lineWidthScaleFactor
                     }

                     if((type == "lx") || (type=="linesx")) {
                        lines(xSubset, ySubset,
                              lwd=par("cex"), col=legendColor, lty=legendStyle, lwd=lineWidth*par("cex"), pch=getDot(dotSet, legendDot))
                     }
                     else if((type == "sx") || (type=="stepsx")) {
                        lines(xSubset, ySubset, type="s",
                              col=legendColor, lty=legendStyle, lwd=lineWidth*par("cex"), pch=getDot(dotSet, legendDot))
                     }

                     pcex <- dotScaleFactor * par("cex")
                     points(xSubset, ySubset,
                           col=legendColor, lty=legendStyle, pch=getDot(dotSet, legendDot),
                           lwd=par("cex"),
                           cex=pcex, bg="yellow")

                     legendTexts  <- append(legendTexts,  legendText)
                     legendColors <- append(legendColors, legendColor)
                     legendStyles <- append(legendStyles, legendStyle)
                     legendDots   <- append(legendDots,   getDot(dotSet, legendDot))
                     legendDot    <- legendDot + 1
                     legendStyle  <- (legendStyle + 1) %% 7
                     lineNum      <- lineNum + 1
                  }
               }

               # ----- Lines or Steps plot ----------------------------------
               else if((type == "l") || (type=="lines") ||
                       (type == "s") || (type=="steps")) {
                  # These sets will contain mean y line and its confidence intervals
                  xPlotSet <- c()
                  yPlotMinSet <- c()
                  yPlotMeanSet <- c()
                  yPlotMaxSet <- c()
                  for(xPosition in seq(1, length(xLevels))) {
                     x <- xLevels[xPosition]

                     # ------ Calculate confidence intervals for (z,x) pos. ----
                     ySubset <- subset(ySet, (zSet == z) & (vSet == v) & (wSet == w) & (xSet == x))
                     if(length(ySubset) > 0) {
                        yMin  <- min(ySubset)
                        yMean <- mean(ySubset)
                        yMax  <- max(ySubset)
                        if(yMax - yMin > 0.000001) {
                           yTest <- t.test(ySubset, conf.level=confidence)
                           yMin <- yTest$conf.int[1]
                           yMax <- yTest$conf.int[2]
                        }

                        # ------ Add results to set ----------------------------
                        if(nlevels(xSet)) {
                           xPlotSet <- append(xPlotSet, xPosition)
                        }
                        else {
                           xPlotSet <- append(xPlotSet, as.numeric(x))
                        }
                        yPlotMinSet  <- append(yPlotMinSet, yMin)
                        yPlotMeanSet <- append(yPlotMeanSet, yMean)
                        yPlotMaxSet  <- append(yPlotMaxSet, yMax)
                     }
                  }

                  # ------ Plot line and confidence intervals ---------------
                  if(length(xPlotSet) > 0) {
                     lineWidth <- 5*lineWidthScaleFactor
                     if((length(vLevels) > 1) || (length(wLevels) > 1)) {
                        lineWidth <- 3*lineWidthScaleFactor
                     }
                     if((type == "l") || (type=="lines")) {
                        lines(xPlotSet, yPlotMeanSet,
                              col=legendColor, lty=legendStyle, lwd=lineWidth*par("cex"))
                     }
                     else if((type == "s") || (type=="steps")) {
                        for(i in seq(1, length(xPlotSet) - 1)) {
                           lines(c(xPlotSet[i], xPlotSet[i + 1]),
                                 c(yPlotMeanSet[i],yPlotMeanSet[i]),
                                 col=legendColor, lty=legendStyle, lwd=lineWidth*par("cex"))
                           lines(c(xPlotSet[i + 1], xPlotSet[i + 1]),
                                 c(yPlotMeanSet[i],yPlotMeanSet[i + 1]),
                                 col=legendColor, lty=legendStyle, lwd=lineWidth*par("cex"))
                        }
                     }

                     cintWidthFraction <- 75
                     cintWidth <- (max(xRange) - min(xRange)) / cintWidthFraction
                     for(xPosition in seq(1, length(xPlotSet))) {
                        x <- xPlotSet[xPosition]
                        lines(c(x, x),
                              c(yPlotMinSet[xPosition], yPlotMaxSet[xPosition]),
                              col=legendColor, lty=legendStyle, lwd=1*par("cex"))
                        lines(c(x - cintWidth, x + cintWidth),
                              c(yPlotMinSet[xPosition], yPlotMinSet[xPosition]),
                              col=legendColor, lty=legendStyle, lwd=1*par("cex"))
                        lines(c(x - cintWidth, x + cintWidth),
                              c(yPlotMaxSet[xPosition],yPlotMaxSet[xPosition]),
                              col=legendColor, lty=legendStyle, lwd=1*par("cex"))
                     }

                     points(xPlotSet, yPlotMeanSet,
                            col=legendColor, lty=legendStyle, pch=getDot(dotSet, legendDot),
                            lwd=par("cex"),
                            cex=pointsSize*par("cex"), bg="yellow")

                     legendTexts  <- append(legendTexts,  legendText)
                     legendColors <- append(legendColors, legendColor)
                     legendStyles <- append(legendStyles, legendStyle)
                     legendDots   <- append(legendDots,   getDot(dotSet, legendDot))
                     legendDot    <- legendDot + 1
                     legendStyle  <- (legendStyle + 1) %% 7
                     lineNum      <- lineNum + 1
                  }
               }

               # ----- Unknown plot type ------------------------------------
               else {
                  stop("plotstd3: Unknown plot type!")
               }
            }
         }
      }
   }


   # ----- Plot hbar mean line ----------------------------------------------
   if( ((type == "h") || (type=="hbars")) &&
       (hbarMeanSteps > 1) ) {
      xSteps <- hbarMeanSteps
      xWidth <- ((max(xSet) - min(xSet)) / xSteps)
      xSegments <- seq(min(xSet), max(xSet) - xWidth, by=xWidth)

      oldY <- c()
      for(xValue in xSegments) {
         filter <- (xSet >= xValue) &
                   (xSet <= xValue + xWidth)

         xAggSubset <- subset(xSet, filter)

         if(length(xAggSubset) > 0) {
            yAggSubset    <- subset(ySet, filter)
            hbarAggSubset <- subset(hbarSet, filter)

            aggregate <- hbarAggregator(
                           xAggSubset, yAggSubset,
                           hbarAggSubset, zValue,
                           confidence)
            mMean <- aggregate[1]
            mMin  <- aggregate[2]
            mMax  <- aggregate[3]

            # ------ Plot line segment -----------------------------------------
            if(colorMode == cmColor) {
               meanBarColor <- zColorArray
            }
            else {
               meanBarColor <- par("fg")
            }
            if(xValue > min(xSet)) {
               lines(c(xValue, xValue),
                     c(oldY, mMean),
                     col=meanBarColor, lwd=4*par("cex"))
            }
            lines(c(xValue, xValue + xWidth),
                  c(mMean, mMean),
                  col=meanBarColor, lwd=4*par("cex"))
            oldY <- mMean

            # ------ Plot confidence interval ----------------------------------
            if(mMin != mMax) {
               x <- xValue + (xWidth / 2)
               cintWidthFraction <- 75
               cintWidth <- (max(xSet) - min(ySet)) / cintWidthFraction
               lines(c(x, x), c(mMin, mMax),
                     col=meanBarColor, lwd=1*par("cex"))
               lines(c(x - cintWidth, x + cintWidth),
                     c(mMin, mMin),
                     col=meanBarColor, lwd=1*par("cex"))
               lines(c(x - cintWidth, x + cintWidth),
                     c(mMax, mMax),
                     col=meanBarColor, lwd=1*par("cex"))
            }
         }
      }
   }


   # ------ Plot separators -------------------------------------------------
   if(length(xSeparatorsSet) > 0) {
      if(length(xSeparatorsColors) < 1) {
         if(colorMode == cmColor) {
            xSeparatorsColors <- rainbow2(length(xSeparatorsSet))
         }
         else if(colorMode == cmGrayScale) {
            xSeparatorsColors <- graybow(length(xSeparatorsSet))
         }
         else {
            xSeparatorsColors <- rep(par("fg"), length(zLevels))
            frameColor  <- par("fg")
         }
      }
      legendBackground <- "gray95"
      if(colorMode == cmBlackAndWhite) {
         legendBackground <- "white"
      }

      i <- 1
      for(xValue in xSeparatorsSet) {
         lines(c(xValue, xValue),
               c(-9e99, 9e99), lwd=4*par("cex"),
               col=xSeparatorsColors[i])
         xAdjust <- -(strwidth(xSeparatorsTitles[i]) / 2)
         rect(xValue + xAdjust, max(yAxisTicks),
              xValue + xAdjust + strwidth(xSeparatorsTitles[i]) + 2 * strwidth("i"),
              max(yAxisTicks) - strheight(xSeparatorsTitles[i]) - 1.0 * strheight("i"),
              col=legendBackground)
         text(xValue + strwidth("i"),
              max(yAxisTicks) - 0.5 * strheight(xSeparatorsTitles[i]) - 0.5 * strheight("i"),
              xSeparatorsTitles[i],
              col=xSeparatorsColors[i],
              adj=c(0.5,0.5))
         i <- i + 1
      }
   }


   # ------ Call to post-plot function --------------------------------------
   postPlotFunction(xRange, yRange, zColorArray, lineWidth, dotScaleFactor, xSet, ySet, zSet, vSet, wSet)


   # ------ Plot legend -----------------------------------------------------
   if(!hideLegend) {
      lx <- min(xRange) + ((max(xRange) - min(xRange)) * legendPos[1])
      ly <- min(yRange) + ((max(yRange) - min(yRange)) * legendPos[2])
      lxjust <- 0.5
      lyjust <- 0.5
      if(legendPos[1] < 0.5) {
         lxjust <- 0
      }
      else if(legendPos[1] > 0.5) {
         lxjust <- 1
      }
      if(legendPos[2] < 0.5) {
         lyjust <- 0
      }
      else if(legendPos[2] > 0.5) {
         lyjust <- 1
      }

      legendBackground <- "gray95"
      if(colorMode == cmBlackAndWhite) {
         legendColors <- par("fg")
         legendBackground <- "white"
      }
      legend(lx, ly,
             xjust = lxjust,
             yjust = lyjust,
             parse(text=legendTexts),
             bg=legendBackground,
             col=legendColors,
             lty=legendStyles,
             pch=legendDots,
             text.col=legendColors,
             lwd=1, cex=par("cex")*legendSize)
   }

   par(opar)
   return(1)
}


# ====== Prepare layout for plotstd6() ======================================
makeLayout <- function(aSet, bSet, aTitle, bTitle, pTitle, pSubLabel,
                       pColor, frameColor, colorMode)
{
   aLevels <- levels(factor(aSet))
   bLevels <- rev(levels(factor(bSet)))   # Reverse B-set levels!
   w <- length(aLevels)
   h <- length(bLevels)

   if(colorMode == cmColor) {
      aLevelsColorArray <- rainbow(length(aLevels), start=0, end=3/6, s=0.3)
      bLevelsColorArray <- rainbow(length(bLevels), start=4/6, end=6/6, s=0.3)
   }
   else if(colorMode == cmGrayScale) {
      aLevelsColorArray <- graybow(length(aLevels))
      bLevelsColorArray <- graybow(length(bLevels))
   }
   else {
      aLevelsColorArray <- rep("#ffffff", length(aLevels))
      bLevelsColorArray <- rep("#ffffff", length(bLevels))
   }

   allocateALabels <- 0
   if(w > 1) {
      allocateALabels <- 2
   }
   allocateBLabels <- 0
   if(h > 1) {
      allocateBLabels <- 2
   }

   layoutWidth   <- w + allocateBLabels
   layoutHeight  <- 2 + h + allocateALabels


   # ------ Allocate IDs in layout ------------------------------------------
   # In this order, the parts have to be plotted!
   nextID <- 1
   if(allocateBLabels > 0) {
      bTitleStart <- nextID
      bTitleID    <- bTitleStart + h
      nextID      <- bTitleID + 1
   }
   if(allocateALabels > 0) {
      aTitleStart <- nextID
      aTitleID    <- aTitleStart + w
      nextID      <- aTitleID + 1
   }
   if((allocateBLabels > 0) & (allocateALabels > 0)) {
      abAxisCrossID <- nextID
      nextID        <- abAxisCrossID + 1
   }
   pTitleID    <- nextID
   pSubLabelID <- nextID + 1
   plotStart   <- nextID + 2


   # ------ Header (i.e. titles) --------------------------------------------
   header <- append(rep(pTitleID, layoutWidth),
                    rep(pSubLabelID, layoutWidth))
   layoutMatrix <- header

   # ------ Body ------------------------------------------------------------
   for(row in 0:(h-1)) {
      # ------ B-axis labels ------------------------------------------------
      rowLine <- c()
      if(allocateBLabels > 0) {
         rowLine <- c(bTitleID, bTitleStart + row)
      }
      rowLine <- append(rowLine, plotStart + seq(row * w, (row + 1) * w - 1, 1))

      layoutMatrix <- append(layoutMatrix, rowLine)
   }

   # ------ Footer (i.e. A-axis labels) -------------------------------------
   if(allocateALabels > 0) {
      footer1 <- c()
      footer2 <- c()
      if(allocateBLabels > 0) {
         footer1 <- c(abAxisCrossID, abAxisCrossID)
         footer2 <- c(abAxisCrossID, abAxisCrossID)
      }
      footer1 <- append(footer1, seq(aTitleStart, aTitleStart + w - 1, 1))
      footer2 <- append(footer2, rep(aTitleID, w))

      layoutMatrix <- append(layoutMatrix, footer1)
      layoutMatrix <- append(layoutMatrix, footer2)
   }

   # ------ Creation of layout matrix ---------------------------------------
   outputDimensions   <- graphics::par("din")
   outputWidth        <- outputDimensions[1]
   outputHeight       <- outputDimensions[2]
   outputAspectRatio  <- outputWidth / outputHeight
   widthArray <- c()
   if(allocateBLabels > 0) {
      widthArray <- c(lcm(1), lcm(1))
   }
   widthArray <- append(widthArray, rep(1.1*outputAspectRatio, w))
   if(pSubLabel != "") {
      heightArray <- append(c(lcm(1.5), lcm(1)), rep(1, h))
   }
   else {
      heightArray <- append(c(lcm(1.5), lcm(0.1)), rep(1, h))
   }
   if(allocateALabels > 0) {
      heightArray <- append(heightArray, c(lcm(1), lcm(1)))
   }

   m <- matrix(layoutMatrix, layoutHeight, layoutWidth, byrow = TRUE)
   # print(m)
   l <- layout(m, widths=widthArray, heights=heightArray, respect=TRUE)
   # cat("widthArray="); print(widthArray)
   # cat("heightArray="); print(heightArray)
   oldPar <- par(mar=c(0,0,0,0))

   # ------ Print B-axis labels ---------------------------------------------
   if(allocateBLabels > 0) {
      for(b in 1:length(bLevels)) {
         plot.new()
         plot.window(c(0, 1), c(0, 1))
         rect(0, 0, 1, 1, col=bLevelsColorArray[b])
         value <- paste(sep="", "paste(", getAbbreviation(bTitle), ", '=", bLevels[b], "')")
         text(0.5, 0.5, parse(text=value), srt=90)
      }

      # ------ B-axis title -------------------------------------------------
      plot.new()
      plot.window(c(0, 1), c(0, 1))
      text(0.5, 0.5, parse(text=getLabel(bTitle)), adj=0.5, srt=90, font=par("font.main"))
   }

   # ------ Print A-axis labels ---------------------------------------------
   if(allocateALabels > 0) {
      for(a in 1:length(aLevels)) {
         plot.new()
         plot.window(c(0, 1), c(0, 1))
         rect(0, 0, 1, 1, col=aLevelsColorArray[a])
         value <- paste(sep="", "paste(", getAbbreviation(aTitle), ", '=", aLevels[a], "')")
         text(0.5, 0.5, parse(text=value))
      }

      # ------ A-axis title -------------------------------------------------
      plot.new()
      plot.window(c(0, 1), c(0, 1))
      text(0.5, 0.5, parse(text=getLabel(aTitle)), adj=0.5, font=par("font.main"))
   }

   # ------ Print titles ----------------------------------------------------
   if((allocateBLabels > 0) & (allocateALabels > 0)) {
      plot.new()   # Corner A/B
   }

   plot.new()   # Title
   plot.window(c(0, 1), c(0, 1))
   text(0.5, 0.5, parse(text=getLabel(pTitle)), col=frameColor,
        adj=0.5, font=par("font.main"), cex=1.5*par("cex.main"))

   plot.new()   # Sub-title
   plot.window(c(0, 1), c(0, 1))
   if(pSubLabel != "") {
      rect(0, 0, 1.02, 1, col=pColor)
      text(0.5, 0.5, parse(text=pSubLabel), adj=0.5, font=3)
   }

   par(oldPar)
   return(l)
}


# ====== Multi-page 2-dimensional array of plotstd3 plots ===================
plotstd6 <- function(mainTitle, pTitle, aTitle, bTitle, xTitle, yTitle, zTitle,
                     pSet, aSet, bSet, xSet, ySet, zSet,
                     vSet                 = c(),
                     wSet                 = c(),
                     vTitle               = "??vTitle??",
                     wTitle               = "??wTitle??",
                     type                 = "lines",
                     confidence           = 0.95,
                     legendPos            = c(0,1),
                     legendSize           = 0.8,
                     lineWidthScaleFactor = 1.0,
                     colorMode            = cmColor,
                     zColorArray          = c(),
                     xAxisTicks           = c(),
                     yAxisTicks           = c(),
                     zReverseColors       = FALSE,
                     zSortAscending       = TRUE,
                     vSortAscending       = TRUE,
                     wSortAscending       = TRUE,
                     aSortAscending       = TRUE,
                     bSortAscending       = TRUE,
                     pSortAscending       = TRUE,
                     dotSet               = c(),
                     dotScaleFactor       = 2,
                     hbarSet              = c(),
                     hbarMeanSteps        = 10,
                     xSeparatorsSet       = c(),
                     xSeparatorsTitles    = c(),
                     xSeparatorsColors    = c(),
                     rangeSet             = c(),
                     rangeColors          = c(),
                     enumerateLines       = FALSE,
                     pStart               = 0,
                     hideCurves           = c(),
                     hideLegend           = FALSE,
                     frameColor           = par("fg"),
                     prePlotFunction      = defaultPrePlotFunction,
                     postPlotFunction     = defaultPostPlotFunction)
{
   if(length(pSet) == 0) {
      pSet <- rep(1, length(xSet))
   }
   if(length(aSet) == 0) {
      aSet <- rep(0, length(xSet))
   }
   if(length(bSet) == 0) {
      bSet <- rep(0, length(xSet))
   }
   aLevels   <- levels(factor(aSet))
   bLevels   <- levels(factor(bSet))
   pLevels   <- levels(factor(pSet))
   if(!aSortAscending) {
      aLevels <- rev(aLevels)
   }
   if(!bSortAscending) {
      bLevels <- rev(bLevels)
   }
   if(!pSortAscending) {
      pLevels <- rev(pLevels)
   }

   aLabel    <- getLabel(aTitle)
   bLabel    <- getLabel(bTitle)
   pLabel    <- getLabel(mainTitle)
   pSubLabel <- ""

   singlePlotTitle <- mainTitle
   if( ((length(aSet) > 0) && (length(aLevels) > 1)) ||
       ((length(bSet) > 0) && (length(bLevels) > 1)) ||
       ((length(pSet) > 0) && (length(pLevels) > 1)) ) {
      singlePlotTitle <- ""
   }

   page <- 1
   for(p in pLevels) {
      # ------ Prepare page -------------------------------------------------
      if(length(pLevels) > 1) {
         pSubLabel <- paste(sep="", "paste(sep=\"\", ", getLabel(pTitle), ", ' = ", p, "')")
      }

      pColor <- getBackgroundColor(page, colorMode, pStart)
      oldPar1 <- par(bg=getBackgroundColor(page, colorMode, pStart))
      if( (length(aLevels) > 1) || (length(bLevels) > 1) || (length(pLevels) > 1) ) {
          # For aLevels==1 and bLevels==1, there is no need to create the layout here!
          # Otherwise, it would reduce cex => too small fonts!
          l <- makeLayout(aSet, bSet, aTitle, bTitle, mainTitle, pSubLabel, pColor, frameColor, colorMode)
      }
      else {
         l <- layout(c(1))   # Dummy layout, in order to reset settings!
      }
      # layout.show(l)
      nextPageInPDFMetadata()

      # ------ Plot page ----------------------------------------------------
      useLargeMargins <- ((length(aLevels) > 1) || (length(bLevels) > 1))
      i<-1
      for(b in rev(bLevels)) {
         for(a in aLevels) {
            # ------ Get sets -----------------------------------------------
            xSubset <- subset(xSet, (pSet == p) & (aSet == a) & (bSet == b))
            ySubset <- subset(ySet, (pSet == p) & (aSet == a) & (bSet == b))
            zSubset <- subset(zSet, (pSet == p) & (aSet == a) & (bSet == b))
            vSubset <- subset(vSet, (pSet == p) & (aSet == a) & (bSet == b))
            wSubset <- subset(wSet, (pSet == p) & (aSet == a) & (bSet == b))

            # ------ Plot std3 figure ---------------------------------------
            if(plotstd3(singlePlotTitle,
                        xTitle, yTitle, zTitle,
                        xSubset, ySubset, zSubset,
                        vSubset, wSubset, vTitle, wTitle,
                        zReverseColors       = zReverseColors,
                        zSortAscending       = zSortAscending,
                        vSortAscending       = vSortAscending,
                        wSortAscending       = wSortAscending,
                        dotSet               = dotSet,
                        dotScaleFactor       = dotScaleFactor,
                        xAxisTicks           = xAxisTicks,
                        yAxisTicks           = yAxisTicks,
                        confidence           = confidence,
                        hbarSet              = hbarSet,
                        hbarMeanSteps        = hbarMeanSteps,
                        xSeparatorsSet       = xSeparatorsSet,
                        xSeparatorsTitles    = xSeparatorsTitles,
                        xSeparatorsColors    = xSeparatorsColors,
                        rangeSet             = rangeSet,
                        rangeColors          = rangeColors,
                        enumerateLines       = enumerateLines,
                        type                 = type,
                        hideCurves           = hideCurves,
                        hideLegend           = hideLegend,
                        legendSize           = legendSize,
                        legendPos            = legendPos,
                        lineWidthScaleFactor = lineWidthScaleFactor,
                        colorMode            = colorMode,
                        zColorArray          = zColorArray,
                        frameColor           = frameColor,
                        writeMetadata        = FALSE,
                        largeMargins         = useLargeMargins,
                        aLevels              = length(aLevels),
                        bLevels              = length(bLevels),
                        prePlotFunction      = prePlotFunction,
                        postPlotFunction     = postPlotFunction,
                        (singlePlotTitle == "")) < 1) {   # see below
               # If singlePlotTitle=="", we have multiple std3 plots on the
               # same page. Then, larger margins have to be used by plotstd3().
               plot.new()   # Must be here, otherwise the order will be wrong!
            }
         }
      }

      #  ------ Reset page settings -----------------------------------
      par(oldPar1)
      page <- page + 1
   }
   return(page - 1)
}


# ====== Value filter for printing histogram plot values ====================
plothist.valuefilter <- function(value, confidence)
{
   if(confidence != 0) {
      return(sprintf("%1.2f ± %1.0f%%", value, 100.0*confidence/value))
   }
   return(sprintf("%1.2f", value))
}


# ====== Plot a histogram. ==================================================
plothist <- function(mainTitle,
                     xTitle, yTitle, zTitle,
                     xSet, ySet, zSet,
                     cSet,
                     xAxisTicks       = getUsefulTicks(xSet),
                     yAxisTicks       = c(),
                     breakSpace       = 0.2,
                     freq             = TRUE,
                     hideLegend       = FALSE,
                     legendPos        = c(1,1),
                     colorMode        = cmColor,
                     zColorArray      = c(),
                     frameColor       = par("fg"),
                     legendSize = 0.8,
                     showMinMax       = FALSE,
                     showConfidence   = TRUE,
                     confidence       = 0.95,
                     valueFilter      = plothist.valuefilter)
{
   # ------ Initialize variables --------------------------------------------
   cLevels <- levels(factor(cSet))
   zLevels <- levels(factor(zSet))
   if(length(zColorArray) == 0) {
      if(colorMode == cmColor) {
         zColorArray <- rainbow2(length(zLevels))
      }
      else if(colorMode == cmGrayScale) {
         zColorArray <- graybow(length(zLevels))
         frameColor  <- par("fg")
      }
      else {
         zColorArray <- rep(par("fg"), length(zLevels))
         frameColor  <- par("fg")
      }
   }

   legendBackground <- "gray95"
   if(colorMode == cmBlackAndWhite) {
      legendBackground <- "white"
   }


   # ------ Initialize plot ----------------------------------------------------
   breakSet <- xAxisTicks
   if(min(xSet) < min(breakSet)) {
      breakSet <- append(breakSet, c(min(xSet)))
   }
   if(max(xSet) > max(breakSet)) {
      breakSet <- append(breakSet, c(max(xSet)))
   }
   breakSet <- sort(unique(breakSet))

   r <- hist(xSet, br=breakSet, plot=FALSE, freq=freq)

   margins <- c(3.25,3.25,3,0.25) + 0.0   # Margins as c(bottom, left, top, right)
                                          # Default is c(5, 4, 4, 2) + 0.1
   opar <- par(col.lab=frameColor, col.main=frameColor, mar=margins)

   xRange <- c(min(xAxisTicks), max(xAxisTicks))
   if(length(yAxisTicks) < 2) {
      if(freq == TRUE) {
         yAxisTicks <- getUsefulTicks(r$count)
      }
      else {
         yAxisTicks <- getUsefulTicks(r$density)
      }
   }
   yRange <- c(0, max(yAxisTicks))

   plot.new()
   plot.window(xRange, yRange)
   axis(1, xAxisTicks, col=frameColor, col.axis=frameColor)
   axis(2, yAxisTicks, col=frameColor, col.axis=frameColor)
   abline(v=xAxisTicks, lty=1, col="lightgray")
   abline(h=yAxisTicks, lty=1, col="lightgray")
   box(col=frameColor)

   xLabel <- getLabel(xTitle)
   yLabel <- getLabel(yTitle)

   mtext(parse(text=xLabel), col=frameColor,
         side = 1, adj=0.5, line=2.25,
         xpd = NA, font = par("font.main"), cex = par("cex"))
   mtext(parse(text=yLabel), col=frameColor,
         side = 2, adj=0.5, line=2.25,
         xpd = NA, font = par("font.main"), cex = par("cex"))
   mtext(parse(text=getLabel(mainTitle)), col=frameColor,
         side = 3, adj=0.5, line=1.75,
         xpd = NA, font = par("font.main"), cex = 1.2 * par("cex"))
   if(length(zLevels) > 1) {
      zLabel <- getLabel(zTitle)
      mtext(parse(text=zLabel), col=frameColor,
            side = 3, at = max(xRange), line=0.5, adj=1,
            xpd = NA, font = par("font.lab"), cex = par("cex"))
   }


   # ------ Plot bars -------------------------------------------------------
   zCount <- 0
   for(zValue in zLevels) {

      cBreakSet <- c()
      cResultSet <- c()

      for(cValue in cLevels) {
         xSubset <- subset(xSet, (zSet == zValue) & (cSet == cValue))

         r <- hist(xSubset, br=breakSet, plot=FALSE, freq=freq)
         cBreakSet <- append(cBreakSet, r$breaks[2:length(r$breaks)-1])
         if(freq == TRUE) {
            cResultSet <- append(cResultSet, r$count)
         }
         else {
            cResultSet <- append(cResultSet, r$density)
         }

         # cat("=> ",sum(r$density * diff(r$breaks)),"\n")   # Das ist immer gleich 1.

      }

      bCount <- 1
      bLevels <- breakSet
      for(bValue in bLevels[1:length(bLevels)-1]) {
         sSet <- subset(cResultSet, (cBreakSet == bValue))

         meanResult <- mean(sSet)
         minResult  <- min(sSet)
         maxResult  <- max(sSet)
         lowResult <- meanResult
         highResult <- meanResult
         if((showConfidence == TRUE) && (minResult != maxResult)) {
            testCount <- t.test(sSet, conf.level=0.95)
            lowResult  <- testCount$conf.int[1]
            highResult  <- testCount$conf.int[2]
         }

         barLeft  <- bValue
         barRight <- bLevels[bCount + 1]
         barWidth <- barRight - barLeft
         barWidth <- barWidth - breakSpace * barWidth

         barLeft <- (0.5 * breakSpace * barWidth) +
                        (barLeft + zCount * (barWidth / length(zLevels)))
         barRight <- barLeft + (barWidth / length(zLevels))


         rect(c(barLeft), c(0), c(barRight), meanResult,
               col=zColorArray[zCount + 1])
         if(showConfidence == TRUE) {
            rect(c(barLeft), c(lowResult),
                 c(barRight), c(highResult),
                 col=NA, lty=2, lwd=2, border="gray50")
         }
         if(showMinMax == TRUE) {
            rect(c(barLeft+0.075*barWidth), c(minResult),
                 c(barRight-0.075*barWidth), c(maxResult),
                 col=NA, lty=2, lwd=1, border="gray40")
         }

         textY <- meanResult
         if(showConfidence == TRUE) {
            textY <- highResult
         }
         if(showMinMax == TRUE) {
            textY <- max(textY, maxResult)
         }

         text(c(barLeft + (0.5 * barWidth / length(zLevels))),
              c(textY),
              valueFilter(meanResult,highResult-meanResult),
              adj=c(0,0),
              srt=80,
              col=zColorArray[zCount + 1])

         bCount <- bCount + 1
      }
      zCount <- zCount + 1
   }


   # ------ Plot legend -----------------------------------------------------
   if(!hideLegend) {
      lx <- min(xRange) + ((max(xRange) - min(xRange)) * legendPos[1])
      ly <- min(yRange) + ((max(yRange) - min(yRange)) * legendPos[2])
      lxjust <- 0.5
      lyjust <- 0.5
      if(legendPos[1] < 0.5) {
         lxjust <- 0
      }
      else if(legendPos[1] > 0.5) {
         lxjust <- 1
      }
      if(legendPos[2] < 0.5) {
         lyjust <- 0
      }
      else if(legendPos[2] > 0.5) {
         lyjust <- 1
      }

      legendBackground <- "gray95"
      if(colorMode == cmBlackAndWhite) {
         legendColors <- par("fg")
         legendBackground <- "white"
      }
      legend(lx, ly,
             xjust = lxjust,
             yjust = lyjust,
             zLevels,
             bg=legendBackground,
             col=zColorArray,
             text.col=zColorArray,
             lwd=10*par("cex"), cex=legendSize*par("cex"))
   }

   par(opar)
}


# ====== Get "useful" ticks from given data set =============================
getUsefulTicks <- function(set, count = 10, integerOnly = FALSE)
{
   if(length(set) < 1) {
      stop("getUsefulTicks: The set is empty!")
   }
   minValue <- min(set)
   maxValue <- max(set)
   if(minValue == maxValue) {
      minValue <- floor(minValue)
      maxValue <- ceiling(minValue)
      if(minValue == maxValue) {
         minValue <- minValue - 1
         maxValue <- maxValue + 1
      }
      set <- c(minValue, maxValue)
   }
   divSet <- c(1, 0.5, 0.25, 0.10)
   if(integerOnly) {
      minValue <- floor(minValue)
      maxValue <- ceiling(maxValue)
      divSet <- c(1)
   }
   difference <- maxValue - minValue
   stepOrder <- floor(log(difference, base=10))

   for(x in divSet) {
      stepBase <- x * (10^stepOrder)

      minFactor <- floor(minValue / stepBase)
      maxFactor <- ceiling(maxValue / stepBase)
      minStep <- minFactor * stepBase
      maxStep <- maxFactor * stepBase

      ticks <- seq(minStep, maxStep, by=stepBase)
      if(length(ticks) >= (count / 2)) {
         return(ticks)
      }
   }
   return(ticks)
}


# ====== Get "useful" integer ticks from given data set =====================
getIntegerTicks <- function(set, count = 10)
{
   if(length(set) < 1) {
      stop("getIntegerTicks: The set is empty!")
   }
   return(getUsefulTicks(set, count, integerOnly = TRUE))
}


# ====== Read table from results file =======================================
loadResults <- function(name, customFilter="", customFormatter=applyFormatter, quiet=FALSE)
{
   filter <- "cat"
   if(any(grep(".bz2", name))) {
      filter <- "bzcat"
   }
   else if(any(grep(".gz", name))) {
      filter <- "zcat"
   }
   filter <- paste(sep="", filter, " ", name)
   if(customFilter != "") {
      filter <- paste(sep="", filter, " | ", customFilter)
   }

   dataInputCommand <- filter
   if(!quiet) {
      cat(sep="", "Loading from pipe [", dataInputCommand, "] ...\n")
   }
   data <- read.table(pipe(dataInputCommand), header = TRUE)
   data <- customFormatter(data)
   return(data)
}



# ###########################################################################
# #### Plotting Toolkit                                                  ####
# ###########################################################################


# ====== Apply manipulator ==================================================
# A manipulator is an expression string which is evaluated in the return
# clause. It may use the variables data1 (containing the first results vector),
# data2 (containing the second), etc. in order to return any calculated result
# line. If manipulator is NA, the function returns the requested table column.
# filter is a string giving an expression for subset(). The resulting vector
# is subsetted using this filter expression.
applyManipulator <- function(manipulator, inputDataTable, columnName, filter)
{
   data1  <- unlist(inputDataTable[1], recursive=FALSE)
   data2  <- unlist(inputDataTable[2], recursive=FALSE)
   data3  <- unlist(inputDataTable[3], recursive=FALSE)
   data4  <- unlist(inputDataTable[4], recursive=FALSE)
   data5  <- unlist(inputDataTable[5], recursive=FALSE)
   data6  <- unlist(inputDataTable[6], recursive=FALSE)
   data7  <- unlist(inputDataTable[7], recursive=FALSE)
   data8  <- unlist(inputDataTable[8], recursive=FALSE)
   data9  <- unlist(inputDataTable[9], recursive=FALSE)
   data10 <- unlist(inputDataTable[10], recursive=FALSE)

   result <- c()
   if(is.na(manipulator)) {
      result <- eval(parse(text=paste(sep="", "data1$", columnName)))
   }
   else {
      result <- eval(parse(text=manipulator))
   }
   result <- subset(result, eval(filter))

   if(length(result) == 1) {   # No result, if result==c(NA)!
      if(is.na(result[1])) {
         result <- c()
      }
   }
   if(length(levels(factor(result))) == 0) {
      result <- c()
   }

   return(result)
}


# ====== Apply formatter ====================================================
applyFormatter <- function (data)
{
   return(data)
}


# ====== Create plots =======================================================
createPlots <- function(simulationDirectory,
                        plotConfigurations,
                        customFilter="",
                        customFormatter=applyFormatter,
                        zColorArray=c())
{
   inputDirectorySet <- c()
   inputFileSet      <- c()
   if(!plotOwnOutput) {
      pdf(paste(sep="", simulationDirectory, ".pdf"),
          width=plotWidth, height=plotHeight, onefile=TRUE,
          family=plotFontFamily, pointsize=plotFontPointsize,
          title=simulationDirectory)
   }
   for(i in 1:length(plotConfigurations)) {
      cat(sep="", "* Plot configuration ", i, ":\n")
      plotConfiguration <- unlist(plotConfigurations[i], recursive=FALSE)

      # ------ Get configuration --------------------------------------------
      configLength <- length(plotConfiguration)
      if(configLength < 8) {
         stop(paste(sep="", "ERROR: Invalid plot configuration! Use order: sim.directory/pdf name/title/results name/xticks/yticks/legend pos/x/y/z/v/w/a/b/p."))
      }
      resultsNameSet      <- c()
      simulationDirectory <- plotConfiguration[1]
      pdfName             <- as.character(plotConfiguration[2])
      title               <- plotConfiguration[3]
      xAxisTicks          <- unlist(plotConfiguration[4])
      yAxisTicks          <- unlist(plotConfiguration[5])
      legendPos           <- unlist(plotConfiguration[6])
      xColumn             <- as.character(plotConfiguration[7])
      yColumn             <- as.character(plotConfiguration[8])
      dotSet              <- c()
      dotScaleFactor      <- 2
      zColorArray         <- c()
      zReverseColors      <- FALSE
      zSortAscending      <- TRUE
      vSortAscending      <- TRUE
      wSortAscending      <- TRUE
      aSortAscending      <- TRUE
      bSortAscending      <- TRUE
      pSortAscending      <- TRUE
      prePlotFunction     <- defaultPrePlotFunction
      postPlotFunction    <- defaultPostPlotFunction
      hideCurves          <- c()

      frameColor   <- "black"
      yManipulator <- "set"
      xTitle <- "X-Axis" ; xFound <- FALSE
      yTitle <- "Y-Axis" ; yFound <- FALSE
      xManipulator <- NA; yManipulator <- NA; zManipulator <- NA
      vManipulator <- NA; wManipulator <- NA;
      aManipulator <- NA; bManipulator <- NA; pManipulator <- NA
      zColumn <- "" ; zSet <- c() ; zTitle <- "Z-Axis" ; zFound <- FALSE
      vColumn <- "" ; vSet <- c() ; vTitle <- "V-Axis" ; vFound <- FALSE
      wColumn <- "" ; wSet <- c() ; wTitle <- "W-Axis" ; wFound <- FALSE
      aColumn <- "" ; aSet <- c() ; aTitle <- "A-Axis" ; aFound <- FALSE
      bColumn <- "" ; bSet <- c() ; bTitle <- "B-Axis" ; bFound <- FALSE
      pColumn <- "" ; pSet <- c() ; pTitle <- "P-Axis" ; pFound <- FALSE
      rangeSet <- c()
      rangeColors <- c()
      filter <- parse(text="TRUE")
      if(configLength >= 9) {
         zColumn <- as.character(plotConfiguration[9])
      }
      if(configLength >= 10) {
         vColumn <- as.character(plotConfiguration[10])
      }
      if(configLength >= 11) {
         wColumn <- as.character(plotConfiguration[11])
      }
      if(configLength >= 12) {
         aColumn <- as.character(plotConfiguration[12])
      }
      if(configLength >= 13) {
         bColumn <- as.character(plotConfiguration[13])
      }
      if(configLength >= 14) {
         pColumn <- as.character(plotConfiguration[14])
      }
      if(configLength >= 15) {
         if(is.na(plotConfiguration[15])) {
            filter <- parse(text="TRUE")
         }
         else {
            filter <- parse(text=as.character(plotConfiguration[15]))
         }
      }
      if(configLength >= 16) {
         for(j in seq(16, configLength, 1)) {
            print(as.character(plotConfiguration[j]))
            eval(parse(text=as.character(plotConfiguration[j])))
         }
      }

      # ------ Get titles and manipulators ----------------------------------
      for(j in 1:length(plotVariables)) {
         plotVariable <- unlist(plotVariables[j], recursive=FALSE)
         variableName <- as.character(plotVariable[1])
         if(length(plotVariable) < 2) {
            cat("Invalid plot variable specification:\n")
            print(plotVariable)
            stop("ERROR: Bad plot variable specification!")
         }
         if(xColumn == variableName) {
            xFound       <- TRUE
            xTitle       <- as.character(plotVariable[2])
            if(length(plotVariable) >= 3) {
               xManipulator <- plotVariable[3]
            }
         }
         else if(yColumn == variableName) {
            if(length(plotVariable) < 4) {
               cat("Invalid plot variable specification for y-axis:\n")
               print(plotVariable)
               stop("ERROR: Bad plot variable specification for y-axis!")
            }
            yFound         <- TRUE
            yTitle         <- as.character(plotVariable[2])
            yManipulator   <- plotVariable[3]
            frameColor     <- as.character(plotVariable[4])
            if(length(plotVariable) >= 5) {
               resultsNameSet <- unlist(plotVariable[5], recursive=FALSE)
            }
            if(length(plotVariable) >= 6) {
               rangeSet       <- unlist(plotVariable[6], recursive=FALSE)
            }
            if(length(plotVariable) >= 7) {
               rangeColors    <- unlist(plotVariable[7], recursive=FALSE)
            }
         }
         else if(zColumn == variableName) {
            zFound       <- TRUE
            zTitle       <- as.character(plotVariable[2])
            if(length(plotVariable) >= 3) {
               zManipulator <- plotVariable[3]
            }
         }
         else if(vColumn == variableName) {
            vFound       <- TRUE
            vTitle       <- as.character(plotVariable[2])
            if(length(plotVariable) >= 3) {
               vManipulator <- plotVariable[3]
            }
         }
         else if(wColumn == variableName) {
            wFound       <- TRUE
            wTitle       <- as.character(plotVariable[2])
            if(length(plotVariable) >= 3) {
               wManipulator <- plotVariable[3]
            }
         }
         else if(aColumn == variableName) {
            aFound       <- TRUE
            aTitle       <- as.character(plotVariable[2])
            if(length(plotVariable) >= 3) {
               aManipulator <- plotVariable[3]
            }
         }
         else if(bColumn == variableName) {
            bFound       <- TRUE
            bTitle       <- as.character(plotVariable[2])
            if(length(plotVariable) >= 3) {
               bManipulator <- plotVariable[3]
            }
         }
         else if(pColumn == variableName) {
            pFound       <- TRUE
            pTitle       <- as.character(plotVariable[2])
            if(length(plotVariable) >= 3) {
               pManipulator <- plotVariable[3]
            }
         }
      }
      if( (xColumn != "") & (!is.na(xColumn)) & (xFound == FALSE) ) {
         stop(paste(sep="", "ERROR: xSet not mapped - setting \"", xColumn, "\" is unknown!"))
      }
      if( (yColumn != "") & (!is.na(yColumn)) & (yFound == FALSE) ) {
         stop(paste(sep="", "ERROR: ySet not mapped - setting \"", yColumn, "\" is unknown!"))
      }
      if( (zColumn != "") & (!is.na(zColumn)) & (zFound == FALSE) ) {
         warning(paste(sep="", "ERROR: zSet not mapped - setting \"", zColumn, "\" is unknown!"))
      }
      if( (vColumn != "") & (!is.na(vColumn)) & (vFound == FALSE) ) {
         warning(paste(sep="", "ERROR: vSet not mapped - setting \"", vColumn, "\" is unknown!"))
      }
      if( (wColumn != "") & (!is.na(wColumn)) & (wFound == FALSE) ) {
         warning(paste(sep="", "ERROR: wSet not mapped - setting \"", wColumn, "\" is unknown!"))
      }
      if( (aColumn != "") & (!is.na(aColumn)) & (aFound == FALSE) ) {
         warning(paste(sep="", "ERROR: aSet not mapped - setting \"", aColumn, "\" is unknown!"))
      }
      if( (bColumn != "") & (!is.na(bColumn)) & (bFound == FALSE) ) {
         warning(paste(sep="", "ERROR: bSet not mapped - setting \"", bColumn, "\" is unknown!"))
      }
      if( (pColumn != "") & (!is.na(pColumn)) & (pFound == FALSE) ) {
         warning(paste(sep="", "ERROR: pSet not mapped - setting \"", pColumn, "\" is unknown!"))
      }

      # ------ Fill data vectors (parse() transforms string to expression) --
      cat("   - Loading results for plot configuration", i, "...\n")
      data <- list()
      for(resultsName in resultsNameSet) {
         resultFileName  <- paste(sep="", simulationDirectory, "/Results/", resultsName, ".data.bz2")
         cat(sep="", "     + Loading results from ", resultFileName, " ...\n")
         data <- append(data, list(loadResults(resultFileName, quiet=FALSE,
                                               customFilter=customFilter,
                                               customFormatter=customFormatter)))
         inputDirectorySet <- unique(append(inputDirectorySet, c(paste(sep="", simulationDirectory, "/Results"))))
         inputFileSet      <- unique(append(inputFileSet, c(resultFileName)))
      }
      if(length(data) < 1) {
         stop("ERROR: No data has been loaded! Check plot configuration to ensure that some data is specified!\n")
      }


      cat(sep="", "   - Plotting ", yColumn, " with:\n")

      cat(sep="", "     + xSet = ", xColumn, "   ")
      xSet <- applyManipulator(xManipulator, data, xColumn, filter)
      cat(sep="", "(", length(xSet), " lines)\n")

      cat(sep="", "     + ySet = ", yColumn, "   ")
      ySet <- applyManipulator(yManipulator, data, yColumn, filter)
      cat(sep="", "(", length(ySet), " lines)\n")

      if(zColumn != "") {
         cat(sep="", "     + zSet = ", zColumn, "   ")
         zSet <- applyManipulator(zManipulator, data, zColumn, filter)
         cat(sep="", "(", length(zSet), " lines)\n")
      }
      if(vColumn != "") {
         cat(sep="", "     + vSet = ", vColumn, "   ")
         vSet <- applyManipulator(vManipulator, data, vColumn, filter)
         cat(sep="", "(", length(vSet), " lines)\n")
      }
      if(wColumn != "") {
         cat(sep="", "     + wSet = ", wColumn, "   ")
         wSet <- applyManipulator(wManipulator, data, wColumn, filter)
         cat(sep="", "(", length(wSet), " lines)\n")
      }
      if(aColumn != "") {
         cat(sep="", "     + aSet = ", aColumn, "   ")
         aSet <- applyManipulator(aManipulator, data, aColumn, filter)
         cat(sep="", "(", length(aSet), " lines)\n")
      }
      if(bColumn != "") {
         cat(sep="", "     + bSet = ", bColumn, "   ")
         bSet <- applyManipulator(bManipulator, data, bColumn, filter)
         cat(sep="", "(", length(bSet), " lines)\n")
      }
      if(pColumn != "") {
         cat(sep="", "     + pSet = ", pColumn, "   ")
         pSet <- applyManipulator(pManipulator, data, pColumn, filter)
         cat(sep="", "(", length(pSet), " lines)\n")
      }
      checkSets(data, xSet, ySet, zSet, vSet, wSet, aSet, bSet, pSet, data$ValueNo)


      # ------ Set x/y-axis ticks -------------------------------------------
      if(length(xAxisTicks) < 2) {
         xAxisTicks <- getUsefulTicks(xSet)
      }
      if(length(yAxisTicks) < 2) {
         yAxisTicks <- getUsefulTicks(ySet)
      }
      if(length(legendPos) < 2) {
         legendPos <- c(1,1)
      }


      # ------ Plot ---------------------------------------------------------
      if(plotOwnOutput) {
         pdf(pdfName,
             width=plotWidth, height=plotHeight, onefile=FALSE,
             family=plotFontFamily, pointsize=plotFontPointsize)
      }
      plotstd6(title,
               pTitle, aTitle, bTitle, xTitle, yTitle, zTitle,
               pSet, aSet, bSet, xSet, ySet, zSet,
               vSet, wSet, vTitle, wTitle,
               xAxisTicks       = xAxisTicks,
               yAxisTicks       = yAxisTicks,
               rangeSet         = rangeSet,
               rangeColors      = rangeColors,
               type             = "lines",
               frameColor       = frameColor,
               zColorArray      = zColorArray,
               confidence       = plotConfidence,
               colorMode        = plotColorMode,
               hideLegend       = plotHideLegend,
               legendPos        = legendPos,
               legendSize       = plotLegendSizeFactor,
               dotSet           = dotSet,
               dotScaleFactor   = dotScaleFactor,
               enumerateLines   = plotEnumerateLines,
               zReverseColors   = zReverseColors,
               zSortAscending   = zSortAscending,
               vSortAscending   = vSortAscending,
               wSortAscending   = wSortAscending,
               aSortAscending   = aSortAscending,
               bSortAscending   = bSortAscending,
               pSortAscending   = pSortAscending,
               prePlotFunction  = prePlotFunction,
               postPlotFunction = postPlotFunction,
               hideCurves       = hideCurves)
      if(plotOwnOutput) {
         dev.off()
      }
   }
   if(!plotOwnOutput) {
      dev.off()
   }
   cat(sep="", "* Commands to back up required input files:\n")
   cat(sep="", "   DESTINATION=<my backup path>\n")
   for(d in inputDirectorySet) {
      cat(sep="", "   mkdir -p $DESTINATION/", d, "\n")
   }
   for(f in inputFileSet) {
      cat(sep="", "   cp ", f, " \\\n      $DESTINATION/", f, "\n")
   }
}


# ====== Analyse results of a counter vector ================================
ACRT_Duration   <- 1
ACRT_Difference <- 2
ACRT_Normalized <- 3
analyseCounterResults <- function(data, lowerLimit, upperLimit,
                                  segmentLength, segments,
                                  columnName,
                                  type)
{
   lowestTimeStamp  <- min(data$RelTime)
   highestTimeStamp <- max(data$RelTime)
   resultsSet       <- c()

   cat("Take from: ", lowerLimit, "\n")
   cat("Take to:   ", upperLimit, "\n")
   cat("Duration:  ", segmentLength * segments, "\n")

   if(lowestTimeStamp > lowerLimit) {
      stop(paste(sep="", "ERROR: Data set underflow - to data before t=", lowerLimit, "!\n"))
   }
   if(highestTimeStamp < upperLimit) {
      stop(paste(sep="", "ERROR: Data set underflow - to data after t=", highestTimeStamp, "!\n"))
   }

   # ------ Cut off edges ---------------------------------------------------
   if(upperLimit - (segmentLength * segments) < lowerLimit) {
      stop(paste(sep="", "ERROR: Data set is too short for ",
                 columnName , "; ", ceiling(lowerLimit - (upperLimit - (segmentLength * segments))), "s more required!"))
   }

   # ------ Fetch segments --------------------------------------------------
   for(i in 1:segments) {
      start <- lowerLimit + ((i - 1) * segmentLength)
      end   <- lowerLimit + (i * segmentLength)
      subData <- subset(data,
                        (data$RelTime >= start) &
                        (data$RelTime < end))

      xSet <- subData$RelTime
      ySet <- eval(parse(text=paste(sep="", "subData$", columnName)))
      if(length(ySet) < 1) {
         stop(paste(sep="", "ERROR: Column \"", columnName, "\" is empty from ", start, " to ", end, "!\n"))
      }

      duration   <- max(xSet) - min(xSet)
      difference <- max(ySet) - min(ySet)
      normalized <- difference / duration
      # cat(start,end, "  ->  ", duration, difference, normalized, "\n")

      if(type == ACRT_Duration) {
         resultsSet <- append(resultsSet, c(duration))
      }
      else if(type == ACRT_Difference) {
         resultsSet <- append(resultsSet, c(difference))
      }
      else {
         resultsSet <- append(resultsSet, c(normalized))
      }
   }
   return(resultsSet)
}



# ###########################################################################
# #### PDF Metadata Handling                                             ####
# ###########################################################################

pdfMetadataFile <- NA
pdfMetadataPage <- NA

# ###### Create PDF info ####################################################
openPDFMetadata <- function(name)
{
   setGlobalVariable("pdfMetadataFile", file(paste(sep="", name, ".meta"), "w"))
   setGlobalVariable("pdfMetadataPage", 0)

   cat(sep="", "title NetPerfMeter Results Plots\n", file=pdfMetadataFile);
   cat(sep="", "subject Measurement Results\n", file=pdfMetadataFile);
   cat(sep="", "creator plot-netperfmeter-results\n", file=pdfMetadataFile);
   cat(sep="", "producer GNU R/Ghostscript\n", file=pdfMetadataFile);
   cat(sep="", "keywords: NetPerfMeter, Measurements, Results\n", file=pdfMetadataFile);
}


# ###### Finish current page and increase page counter ######################
nextPageInPDFMetadata <- function()
{
   setGlobalVariable("pdfMetadataPage", pdfMetadataPage + 1)
}


# ###### Add entry into PDF info ############################################
addBookmarkInPDFMetadata <- function(level, title)
{
   if(!is.na(pdfMetadataFile)) {
      cat(sep="", "outline ", level, " ", pdfMetadataPage + 1, " ", title, "\n", file=pdfMetadataFile)
    }
}


# ###### Close PDF info #####################################################
closePDFMetadata <- function()
{
   if(!is.na(pdfMetadataFile)) {
      close(pdfMetadataFile)
      setGlobalVariable("pdfMetadataFile", NA)
   }
   if(!is.na(pdfMetadataFile)) {
      close(pdfMetadataFile)
      setGlobalVariable("pdfMetadataFile", NA)
   }
}



# ###########################################################################
# #### Default Settings                                                  ####
# ###########################################################################

plotColorMode        <- cmColor
plotHideLegend       <- FALSE
plotLegendSizeFactor <- 0.8
plotOwnOutput        <- FALSE
plotFontFamily       <- "Helvetica"
plotFontPointsize    <- 22
plotWidth            <- 10
plotHeight           <- 10
plotConfidence       <- 0.95
plotEnumerateLines   <- TRUE
