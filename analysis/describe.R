library(ggplot2)
library(reshape2)

#[ parse arguments
args = commandArgs()
input_file_name = "/home/ikatsov/AWORK/SANDBOX/HighlyScalable/SSD/RebenchR/trace.txt" #args[6]
aggregation_interval_us = 1e6 #as.integer(args[7]) * 1e6
#]

print_trace_stat <- function(data_vector, header, dimensionality) {
	stats <- data.frame(
		stat = c("samples", "max", "min", "mean", "std_dev"), 
		value = c( length(data_vector), max(data_vector), min(data_vector), mean(data_vector), sd(data_vector) ),
		dim = c("", rep.int(dimensionality, 5-1))
	)
	percentiles <- c(.5, .6, .7, .75, .8, .85, .9, .95, .99, .995, .999)
	percentile_values <- data.frame(percentile=percentiles, value=quantile(data_vector, percentiles, names = FALSE), dim = rep.int(dimensionality, length(percentiles)))

	print(paste(header, "STATISTICS:"))
	print(stats)
	print(paste(header, "PERCENTILES:"))
	print(percentile_values)
}

load_trace <- function(file_name) {
	trace <- read.table(file_name)
	trace <- data.frame(time=trace[,1], latency=trace[,2])
	return(trace[order(trace$time), ])	
}

####### Load Data

trace <- load_trace(input_file_name)

####### Numerical Statistics

#[ test duration
test_duration <- max(trace$time) - min(trace$time)
print("TEST DURATION:")
sprintf("%.2f sec = %.2f min", test_duration / (1e6), test_duration / (1e6*60))
#]

#[ latency statistics
print_trace_stat(trace$latency, "LATENCY", "usec")
#]

#[ throughput statistics
total_throughput = length(trace$latency) / test_duration
trace_grouped = data.frame(time=round(trace[,1] / aggregation_interval_us), latency=trace[,2])
operations_per_frame <- aggregate(latency ~ time, data=trace_grouped, FUN=length)
print_trace_stat(operations_per_frame[,2] / (aggregation_interval_us * 1e-6), "THROUGPUT", "IOPS")
#]

####### Plots

#[ latency histogram
LATENCY_HIST_PLOT_HEIGHT = 600
LATENCY_HIST_PLOT_WIDTH = 1000
LATENCY_HIST_PLOT_OUTLIERS_PERCENTILE = .99
LATENCY_HIST_PLOT_BINS =  100
prange = quantile(trace$latency, c(1 - LATENCY_HIST_PLOT_OUTLIERS_PERCENTILE, LATENCY_HIST_PLOT_OUTLIERS_PERCENTILE))
chopped_trace <- trace[trace$latency > prange[1] & trace$latency < prange[2],]
png(filename="latency_histogram.png", height=LATENCY_HIST_PLOT_HEIGHT, width=LATENCY_HIST_PLOT_WIDTH , bg="white")
ggplot(data=chopped_trace, aes(x=latency) ) + 
  geom_histogram(aes(fill = ..count..), binwidth = max(chopped_trace$latency)/LATENCY_HIST_PLOT_BINS) + 
  xlab("Latency, ms") 
dev.off()
#]

#[ throughput histogram
THROUGHPUT_HIST_PLOT_HEIGHT = 600
THROUGHPUT_HIST_PLOT_WIDTH = 1000
THROUGHPUT_HIST_PLOT_OUTLIERS_PERCENTILE = .99
THROUGHPUT_HIST_PLOT_BINS =  100
prange = quantile(operations_per_frame$latency, c(1 - THROUGHPUT_HIST_PLOT_OUTLIERS_PERCENTILE, THROUGHPUT_HIST_PLOT_OUTLIERS_PERCENTILE))
chopped_operations_per_frame <- operations_per_frame[operations_per_frame$latency > prange[1] & operations_per_frame$latency < prange[2],]
png(filename="throughput_histogram.png", height=THROUGHPUT_HIST_PLOT_HEIGHT, width=THROUGHPUT_HIST_PLOT_WIDTH , bg="white")
ggplot(data=chopped_operations_per_frame, aes(x=latency)) + 
  geom_histogram(aes(fill = ..count..), binwidth = max(chopped_operations_per_frame$latency)/THROUGHPUT_HIST_PLOT_BINS) + 
  xlab("Througput, IOPS") 
dev.off()
#]

#[ latency time plot
LATENCY_TIME_PLOT_PERCENTILES = c(0, .5, .6, .7, .8, .9, .95, .99)
LATENCY_TIME_PLOT_HEIGHT = 600
LATENCY_TIME_PLOT_WIDTH = 1000
q <- function(x) {
  return(c(mean(x), quantile(x, LATENCY_TIME_PLOT_PERCENTILES, names=FALSE)))  
}
latency_percentile_per_frame <- aggregate(latency ~ time, data=trace_grouped, FUN=q)
prange <- c(min(latency_percentile_per_frame[,2][,2]), max(latency_percentile_per_frame[,2][,1 + length(LATENCY_TIME_PLOT_PERCENTILES)]))
time_axis <- (latency_percentile_per_frame$time - min(latency_percentile_per_frame$time))*aggregation_interval_us/1e6
latency_data <- data.frame(time=time_axis)
latency_data$mean <- latency_percentile_per_frame[,2][,1]
i <- 2
for (p in LATENCY_TIME_PLOT_PERCENTILES) {
  latency_data <- cbind(latency_data, latency_percentile_per_frame[,2][,i])
  i <- i + 1
}
colnames(latency_data) <- c("time", "mean", paste("per", LATENCY_TIME_PLOT_PERCENTILES) )
latency_data <- melt(latency_data, id=c("time"))
png(filename="latency_time_plot.png", height=LATENCY_TIME_PLOT_HEIGHT, width=LATENCY_TIME_PLOT_WIDTH, bg="white")
qplot(time, value, data=latency_data, geom = c("point", "line"), ylim=prange, colour=variable) + 
  ylab("Latency, ms") + xlab("Time, sec")
dev.off()
#]

#[ throughpt time plot
THOUGHPUT_TIME_PLOT_HEIGHT = 600
THOUGHPUT_TIME_PLOT_WIDTH = 1000
png(filename="throughput_time_plot.png", height=THOUGHPUT_TIME_PLOT_HEIGHT, width=THOUGHPUT_TIME_PLOT_WIDTH, bg="white")
throughput_data <- data.frame(time_sec=((operations_per_frame$time - min(operations_per_frame$time))*aggregation_interval_us/1e6), throughput_tps=operations_per_frame[,2] / (aggregation_interval_us * 1e-6))
qplot(time_sec, throughput_tps, data=throughput_data, geom = c("point", "line")) + 
  ylab("Throughput, IOPS") + xlab("Time, sec")
dev.off()
#]

q()
