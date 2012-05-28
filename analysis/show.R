library(ggplot2)
library(reshape2)

#[ parse arguments
args = commandArgs()
input_file_name = "/home/ikatsov/AWORK/SANDBOX/HighlyScalable/SSD/RebenchR/trace.txt" #args[6]
#]

trace <- read.table(file_name)
THOUGHPUT_TIME_PLOT_HEIGHT = 600
THOUGHPUT_TIME_PLOT_WIDTH = 1000