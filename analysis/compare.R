library(ggplot2)
library(reshape2)

#[ parse arguments
args = commandArgs()
input_file_name_1 = "/home/ikatsov/AWORK/SANDBOX/HighlyScalable/SSD/RebenchR/trace.txt" #args[6]
input_file_name_2 = "/home/ikatsov/AWORK/SANDBOX/HighlyScalable/SSD/RebenchR/trace.txt" #args[6]
aggregation_interval_us = 1e6 #as.integer(args[7]) * 1e6
#]

q <- function(x) {
  return(quantile(x, LATENCY_TIME_PLOT_PERCENTILES, names=FALSE))  
}
percentiles_by_frames <- function(trace_grouped, percentiles) {
  latency_percentile_per_frame <- aggregate(latency ~ time, data=trace_grouped, FUN=q)
  latency_data = data.frame(time=latency_percentile_per_frame$time)
  i <- 1
  for (p in LATENCY_TIME_PLOT_PERCENTILES) {
    latency_data <- cbind(latency_data, latency_percentile_per_frame[,2][,i])
    i <- i + 1
  }
  colnames(latency_data) <- c("time", paste("per", percentiles) )
  latency_data <- melt(latency_data, id=c("time"))
}

load_trace <- function(file_name) {
  trace <- read.table(file_name)
  trace <- data.frame(time=trace[,1], latency=trace[,2])
  return(trace[order(trace$time), ])	
}

trace_1 <- load_trace(input_file_name_1)
trace_2 <- load_trace(input_file_name_2)

trace_grouped_1 = data.frame(time=round(trace_1[,1] / aggregation_interval_us), latency=trace_1[,2])
trace_grouped_2 = data.frame(time=round(trace_2[,1] / aggregation_interval_us), latency=trace_2[,2])

LATENCY_TIME_PLOT_PERCENTILES = c(0, .5, .6, .7, .8, .9, .95, .99)
pf1 = percentiles_by_frame(trace_grouped_1, LATENCY_TIME_PLOT_PERCENTILES)
pf2 = percentiles_by_frame(trace_grouped_2, LATENCY_TIME_PLOT_PERCENTILES)
  
  
png(filename="latency_distribution.png", height=LATENCY_TIME_PLOT_HEIGHT, width=LATENCY_TIME_PLOT_WIDTH , bg="white")
ggplot(latency_data_1, aes(x=variable, y=value)) + geom_boxplot() + geom_jitter() 
dev.off()