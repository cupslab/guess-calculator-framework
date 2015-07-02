# ApplyQuantizer.R
# Version: 0.7
#
# Author: Saranga Komanduri (sarangak@cmu.edu)
#
# Execution: This script is meant to be executed from the command-line as:
#   Rscript --vanilla ApplyQuantizer.R  [Total Quantizer Levels] [Number of iterations]
#
# Input: This script will read the grammar directory in the current directory.
#   It can also read some parameters from the command line.
#
# Note: You will need to set the kRAMSize parameter if this script will be used
#   on large grammars.
#
# Output: The grammar/terminalRulesold directory will be replaced with a 
#   quantized version in grammar/terminalRules. Statistics regarding the
#   quantization will be written to stderr.
#
# The "Total Quantizer Levels" are assigned across all groups of terminals
#  based on their probabilities.
#
# Note: The testthat package is used to test for various things. If any test
#   fails, execution will halt and output files will never get written, so
#   subsequent steps will fail.
#
#
###############################################################################

# Check for necessary packages
for (pkg in c("plyr", "stringr", "testthat", "hash")) {
  if (!(pkg %in% installed.packages()[,1])) {
    # Try installing package and stop if that fails (install.packages does not return an error level)
    install.packages(pkg, 
                     repos = "http://lib.stat.cmu.edu/R/CRAN/",
                     dependencies = T)
    if (!(pkg %in% installed.packages()[,1])) {
      stop(paste("Package not found:", pkg))
    }
  }
}

# Packages required by this script
library(plyr)
library(stringr)
library(testthat)
library(hash)
library(parallel)


# Options - I always include these options.
options(stringsAsFactors = F,  # If I need factors I explicitly define them
        width = 1000)  # When output is written to a file, it is weird to have lines wrapped at 80

kRAMSize <- 256 * (2^30)  # Bytes of RAM on the machine
kReserveRAM <- 16 * (2^30)  # It is recommended to reserve a good amount of RAM for
                            # system operations

################################################################################
# Initialize data, set constants (file paths, etc.)

# For deterministic results, keep the following line uncommented
set.seed(1)

STRUCTUREBREAKCHAR <- "E"
TOTALQUANTLEVELS <- 500  # In my testing, this was a good number
OUTER_ITER <- 1000    # Use high number of iterations to try to find a global minimum

# Check arguments
cargs <- commandArgs(T)
if (length(cargs) > 0) {
  TOTALQUANTLEVELS <- as.numeric(cargs[1])
  # Make sure command-line arguments were read correctly
  cat("Total levels: ", TOTALQUANTLEVELS, "\n", file = stderr())
}
if (length(cargs) > 1) {
  OUTER_ITER <- as.numeric(cargs[2])
  # Make sure command-line arguments were read correctly
  cat("Quantizer iterations: ", OUTER_ITER, "\n", file = stderr())
}


################################################################################
# Helper functions
# Typical helper functions (might or might not be used by this script)
caterr <- function (...) { 
  # Trivial function for writing output to stderr
  cat(..., file = stderr()) 
}

cerr <- function (...) { 
  # Trivial function for redirecting output from a print function to stderr
  capture.output(..., file = stderr()) 
}

FormatPropTable <- function(intable, dec = 2) {
  # Print a prop.table with given number of decimal places and percent sign
  y <- apply(round(intable * 100, dec), c(1,2), paste, "%", sep = "")
  return(y)
}

RecodeVector <- function(vector, oldvalues, newvalues) {
  # Function for recoding values of a vector based on a vector of matching oldvalues and newvalues
  # Ex. oldvalues = c("Male", "Female") and newvalues = c("M", "F") will change all instances of "Male" and "Female" in the given vector with "M" and "F", leaving the other values alone
  if (length(oldvalues) != length(newvalues)) {
    stop("oldvalues and newvalues must be the same length!")
  }
  
  # Make copy of vector and replace values
  vec2 <- vector  
  for (i in seq_along(oldvalues)) {
    vec2[which(vec2 %in% oldvalues[i])] <- newvalues[i]
  }
  
  return(vec2)
}

trim <- function (x) gsub("^\\s+|\\s+$", "", x)  # Function for trimming whitespace from a string

# Helper functions required by this script
ReadTSVFile <- function(filename) {
  return(read.table(file = filename,
                    header = F,
                    sep = "\t",
                    quote = "",
                    comment.char = "",
                    colClasses = c("character", "numeric", "character"),  # Only treat the probability column as numeric, otherwise groups with digits are treated as numbers                    
                    fill = T,
                    blank.lines.skip = F,
                    fileEncoding = "UTF-8",
                    encoding = "UTF-8"))
}

WriteTSVFile <- function(df, filename) {
  # Write a TSV file from the given data frame, but convert the probability column to a hex flot before writing
  df2 <- df
  df2[["pr"]] <- sprintf("%a", df[["pr"]])  # Explicitly convert float to hex float, otherwise write.table will write it in decimal
  write.table(df2, file = filename, quote = F, sep = "\t", row.names = F, col.names = F, fileEncoding = "UTF-8")
}

WriteUnseenLine <- function(df, filename) {
  # Append the Unseen line to the given file, which is assumed to have already been written using WriteTSVFile
  # Blank line first
  cat("\n", file = filename, append = T)
  df2 <- df
  df2[["pr"]] <- sprintf("%a", df[["pr"]])
  write.table(df2, file = filename, append = T, quote = F, sep = "\t", row.names = F, col.names = F, fileEncoding = "UTF-8")
}


ReadStructureFile <- function(filename) {
  # The nonterminalRules files has a non-standard structure:
  #   LHS 1 ->
  #   <RHS string 1>\t<probability>\t<source ids>
  #   <RHS string 2>\t<probability>\t<source ids>
  #   ...
  #   [blank line]
  #   LHS 2 ->
  #   ...
  #   [blank line]
  # NOTE: We only care about the top LHS at this time, which should be the
  #   start symbol "S".
  #
  # Returns: a data frame with RHS strings, probabilities and source ids
  #  
  wholefile <- readLines(filename, warn = F, encoding = "UTF-8")
  
  # Using idea from http://stackoverflow.com/questions/3642535/creating-an-r-dataframe-row-by-row
  N <- 1e4  # some magic number, possibly an overestimate
  ReturnDF <- data.frame("structure" = character(N),
                         "pr" = numeric(N),
                         "ids" = character(N))
  
  
  # Check first line
  expect_that(wholefile[1], equals("S ->"))
  linectr <- 2
  # Read remaining lines until blank line and put result in data frame
  while (wholefile[linectr] != "") {
    linepieces <- strsplit(wholefile[linectr], split = "\t", fixed = T)[[1]]
    ReturnDF[linectr - 1, ] <- linepieces
    linectr <- linectr + 1
  }
  # If the magic number was not reached, shrink the data frame
  if (linectr - 2 < N) {
    ReturnDF <- head(ReturnDF, n = linectr - 2)
  }
  # For some reason, the second column is returning as character
  ReturnDF$pr <- as.numeric(ReturnDF$pr)
  return(ReturnDF)
}


QError <- function(pr, q) {
  # Calculate quantization error (MSE)
  # Given a vector of probabilities and a vector of associated representation points, what is the MSE?
  if (length(pr) == 0 || length(q) == 0) {
    stop("Empty vector sent as one of the arguments")
  }
  errors <- pr - q
  # Try to modify errors in-place
  errors <- errors^2
  errors <- errors * pr  
  return(sum(errors))
}


QuantizeWithBounds <- function(pr, inbounds, max.inner.iter) {
  # Run the inner loop of the Lloyd-Max algorithm to find quantized probabilities based on given bounds
  numlevels <- length(inbounds) - 1
  totalmass <- sum(pr)
  bounds <- inbounds
  prevMSE = Inf
  probs <- pr  
  for (iter in 1:max.inner.iter) {
    # Compute centroids for each region and adjust probabilities
    centroids <- rep(NA, numlevels)
    
    # Use cut to determine which items in prob are in which level
    cutvec <- cut(probs, bounds, labels = F)
    expect_that(range(cutvec)[1] < 1, is_false())
    expect_that(range(cutvec)[2] > numlevels, is_false())
    for (lev in 1:numlevels) {
      idx <- which(cutvec == lev)
      if (length(idx) == 0) {  
        # Don't quantize empty regions, but still calculate centroid otherwise the algorithm will not have an input in step 4
        centroids[lev] <- (bounds[lev] + bounds[lev + 1]) / 2
        next
      }
      qlev <- mean(probs[idx])
      probs[idx] <- qlev
      centroids[lev] <- qlev
    }
    expect_that(sum(probs), equals(totalmass))
    expect_that(any(is.na(centroids)), is_false())
    
    # 3. Calculate MSE for this quantizer
    curMSE <- QError(probs, pr)    
    
    # Check if done
    MSEdiff <- prevMSE - curMSE
    if (MSEdiff <= .Machine$double.eps) {  # Suitably small epsilon
      break
    }
    prevMSE <- curMSE
    
    # 4. Calculate new boundary points from centroids - this uses a spiffy formula for calculating midpoints using lagged differences
    bounds <- c(0, (centroids[-1] - (diff(centroids)/2)), 1)
    probs <- pr  
  }
  
  return(list(probs = probs,
              MSE = curMSE,
              iters = iter))
}


QuantizeGroup <- function(df, numlevels, max.inner.iter = 30, outer.iter = 30) {
  # Use the Lloyd-Max algorithm to quantize the probabilities in the given data frame to the given number of levels
  # The algorithm finds a local minimum, so to try to find a global minimum we restart it several times with random regions
  #
  expect_that(is.numeric(numlevels), is_true())
  expect_that(numlevels < 1, is_false())
  
  # Trivial cases:
  if (numlevels == 1) {
    totalmass <- sum(df[["pr"]])
    qlev <- mean(df[["pr"]])
    df[["pr"]] <- qlev
    expect_that(sum(df[["pr"]]), equals(totalmass))
    return(list(df = df,
                MSE = 0,
                inner.iters = NA))
  } else if (numlevels > length(unique(df[["pr"]]))) {
    # message("No quantization applied, not enough points in the data frame")
    return(list(df = df,
                MSE = 0,
                inner.iters = NA))
  }
  
  # Generate outer.iter vectors of random bounds for quantization
  dr <- range(df[["pr"]])
  inputdata <- lapply(as.list(1:outer.iter), function (ele) { 
    bp <- runif(numlevels - 1, min = dr[1], max = dr[2])
    bounds <- c(0, sort(bp), 1)
    return(bounds)
  })
  
  # Use lapply to store quantized bounds
  savedresults <- mclapply(inputdata, function (ele) {
    qresults <- QuantizeWithBounds(df[["pr"]], ele, max.inner.iter)
    return(list(bounds = ele,  # Only store bounds to save space; we can reproduce probabilities later
                MSE = qresults$MSE,
                iters = qresults$iters))
  }, mc.cores = min(max(detectCores() - 4, 1),
                    max(floor(as.numeric((kRAMSize - kReserveRAM - object.size(df))/(object.size(df[["pr"]]) * 2))),
                        1)  
  ))
  
  # Find the best result
  MSEs <- unlist(lapply(savedresults, function(ele) { return(ele$MSE) }))
  best <- which(MSEs == min(MSEs))[1]
  
  df.2 <- df
  df.2[["pr"]] <- QuantizeWithBounds(df[["pr"]], savedresults[[best]]$bounds, max.inner.iter)$probs
  return(list(df = df.2,
              MSE = savedresults[[best]]$MSE,
              inner.iters = savedresults[[best]]$iters))
}



################################################################################
# Main block 

# Check arguments
# cargs <- commandArgs(T)
# if (length(cargs) < 1) {
#   stop(paste("Usage:  Rscript --vanilla ApplyQuantizer.R <arguments>",
#              "Arguments:",
#              "?",
#              sep = "\n"))
# } else {
#   pfile <- cargs[1]
#   # Make sure command-line arguments were read correctly
#   cat("Input file: ", pfile, "\n", file = stderr())
# }

# Read structures file
caterr("Reading structures\n")
structs <- ReadStructureFile("grammar//nonterminalRules.txt")
colnames(structs) <- c("structure", "pr", "ids")

# Parse structures to determine group probabilities
# Luckily this is easy because all groups are delineated with break characters
groups <- hash()
for (rownum in 1:nrow(structs)) {
  mystruct <- structs[rownum, 1]
  mygroups <- strsplit(mystruct, STRUCTUREBREAKCHAR, fixed = T)[[1]]
  for (group in mygroups) {
    if (has.key(group, groups)) {
      groups[[group]] <- groups[[group]] + structs[rownum, 2]
    } else {
      groups[[group]] <- structs[rownum, 2]
    }
  }
}
# Normalize probabilities - since there are more than one group per structure and we just summed the probabilities, this will be more than one
v <- values(groups)
vsum <- sum(v)
expect_that(vsum < 1, is_false())
values(groups) <- v/vsum
v <- values(groups)
expect_that(sum(v), equals(1))

# Collapse the probabilities of same-length letter strings (LLLL,ULLL,UULL, etc. have their probabilities combined since they access the same terminal group: string) so we have accurate probabilities to use for adjusting the terminal groups
adjgroups <- hash()
for (key in keys(groups)) {
  # Normalize the name
  newkey <- gsub("U", "L", key, fixed = T)
  if (has.key(newkey, adjgroups)) {
    adjgroups[[newkey]] <- adjgroups[[newkey]] + groups[[key]]
  } else {
    adjgroups[[newkey]] <- groups[[key]]
  }
}
expect_that(length(keys(adjgroups)) > 0, is_true())

############## Quantize the group probabilities - Spread some total number
##############   of levels across all nonterminals
N <- length(keys(adjgroups)) + TOTALQUANTLEVELS
caterr("Begin quantization with", N, "total levels\n")

# The number of specified groups is spread across the terminal groups based on probability, which initially assigns a floating-point number of levels to each group that is rounded up so that each group is assigned at least one group.  This can cause more groups than desired to be assigned.  Therefore, we put this method in a root finder to get an approximately correct number of levels.
adjN <- uniroot(function(M) {
  levspergroup <- ceiling(M * values(adjgroups))
  return(sum(levspergroup) - N)
}, interval = c(1, 1e9))$root
levspergroup <- ceiling(adjN * values(adjgroups))
expect_that(abs(sum(levspergroup) - N) < 10, is_true())

if (sum(levspergroup) != N) {
  caterr("Actual total levels is:", sum(levspergroup), "\n")
}

#######################################################
# Load each terminal file, quantize, and write to disk
origlevels <- 0
totalmse <- 0.0
dir.create("grammar//terminalRules", showWarnings = FALSE)
caterr(paste("Reading terminalRules\n", sep = ""))
for (key in keys(adjgroups)) { 
  if (exists("unseenline")) {
    rm(unseenline)
  }    
  # Read terminal file
  filename <- paste("grammar//terminalRulesold//", key, ".txt", sep = "")
  if (file.exists(filename)) {
    foo <- ReadTSVFile(filename)
    colnames(foo) <- c("group", "pr", "ids")
    # Check for the unseen terminals line and quantize around it
    if (foo[nrow(foo), 1] == "<UNSEEN>") {
      # Save unseen line
      unseenline <- foo[nrow(foo),]
      # Truncate foo (the second to last line will be a blank line)
      foo <- foo[1:(nrow(foo)-2),]
    }
    origlevels <- origlevels + length(unique(foo[["pr"]]))
  } else {
    stop("Error: Missing file:", filename, "!\n")
  }
  
  caterr(paste("Read terminal rule", key, "with size", object.size(foo), "\n"))
  
  # Quantize
  results <- QuantizeGroup(foo, numlevels = levspergroup[[key]], outer.iter = OUTER_ITER)
  totalsum <- sum(results$df[["pr"]])
  if (exists("unseenline")) {
    totalsum <- totalsum + unseenline[["pr"]]
  }
  expect_that(totalsum, equals(1))
  
  # Write quantized file
  filename <- paste("grammar//terminalRules//", key, ".txt", sep = "")
  WriteTSVFile(results$df, filename)
  if (exists("unseenline")) {    
    WriteUnseenLine(unseenline, filename)
  }
  
  # Calculate MSE
  mse <- QError(results$df[["pr"]] * adjgroups[[key]], foo[["pr"]] * adjgroups[[key]])
  totalmse <- totalmse + mse
  
  rm(results)
  rm(foo)
  gc()
}

caterr("Total levels in original terminal groups:", origlevels, "\n\n")
caterr("MSE with", N, "levels total ( adjN =", adjN, "): ")
caterr(totalmse)
caterr("\n\n")
