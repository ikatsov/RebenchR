# Make sure that ggplot is installed: install.packages("ggplot2")
library(ggplot2)

args = commandArgs()
input_file_name = args[6]
aggregation_interval_us = as.integer(args[7]) * 1e6

print_trace_stat <- function(data_vector, header, dimensionality){
	stats <- data.frame(
		stat = c("records", "max", "min", "mean", "std_dev"), 
		value = c( length(data_vector), max(data_vector), min(data_vector), mean(data_vector), sd(data_vector) ),
		dim = rep.int(dimensionality, 5)
	)
	percentiles <- c(.5, .6, .7, .75, .8, .85, .9, .95, .99, .995, .999)
	percentile_values <- data.frame(percentile=percentiles, value=quantile(data_vector, percentiles, names = FALSE), dim = rep.int(dimensionality, length(percentiles)))

	print(paste(header, "STATISTICS:"))
	print(stats)
	print(paste(header, "PERCENTILES:"))
	print(percentile_values)
}

trace <- read.table(input_file_name)
trace <- data.frame(time=trace[,1], latency=trace[,2])
trace <- trace[order(trace$time), ]

####### Statistics

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
print_trace_stat(operations_per_frame[,2] / (aggregation_interval_us * 1e-6), "THROUGPUT", "ops/sec")
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
ggplot(data=chopped_trace, aes(x=latency) ) + geom_histogram(aes(fill = ..count..), binwidth = max(chopped_trace$latency)/LATENCY_HIST_PLOT_BINS) 
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
ggplot(data=chopped_operations_per_frame, aes(x=latency) ) + geom_histogram(aes(fill = ..count..), binwidth = max(chopped_operations_per_frame$latency)/THROUGHPUT_HIST_PLOT_BINS) 
dev.off()
#]

#[ scatter latency time plot
SIMPLE_LATENCY_PLOT_HEIGHT = 600
SIMPLE_LATENCY_PLOT_WIDTH = 1000
SIMPLE_LATENCY_PLOT_OUTLIERS_PERCENTILE = .999
#png(filename="simple_latency_plot.png", height=SIMPLE_LATENCY_PLOT_HEIGHT, width=SIMPLE_LATENCY_PLOT_WIDTH, bg="white")
#plot(data.frame(time_sec=((trace$time - min(trace$time))/1e6), latency_usec=trace$latency), ylim=c(min(trace$latency), quantile(trace$latency, c(SIMPLE_LATENCY_PLOT_OUTLIERS_PERCENTILE))))
#dev.off()
#]

#[ throughpt time plot
png(filename="throughput_time_plot.png", height=SIMPLE_LATENCY_PLOT_HEIGHT, width=SIMPLE_LATENCY_PLOT_WIDTH, bg="white")
d <- data.frame(time_sec=((operations_per_frame$time - min(operations_per_frame$time))*aggregation_interval_us/1e6), throughput_tps=operations_per_frame[,2] / (aggregation_interval_us * 1e-6))
plot(d, type="o")
dev.off()
#]

#[ latency time plot
percentiles = c(0, .5, .6, .7, .8, .9, .95, .99)
latency_mean_per_frame <- aggregate(latency ~ time, data=trace_grouped, FUN=mean)
prange = c(
	min(aggregate(latency ~ time, data=trace_grouped, FUN=quantile, probs=min(percentiles))$latency),
	max(aggregate(latency ~ time, data=trace_grouped, FUN=quantile, probs=max(percentiles))$latency)
)
time_axis = (latency_mean_per_frame$time - min(latency_mean_per_frame$time))*aggregation_interval_us/1e6
png(filename="latency_time_plot.png", height=SIMPLE_LATENCY_PLOT_HEIGHT, width=SIMPLE_LATENCY_PLOT_WIDTH, bg="white")
plot( time_axis, latency_mean_per_frame$latency, type="o", ylim=prange )
colors <- rainbow(length(percentiles)) 
i <- 0
for (p in percentiles) { 
	latency_percentile_per_frame <- aggregate(latency ~ time, data=trace_grouped, FUN=quantile, probs=p)
	lines(time_axis, latency_percentile_per_frame$latency, type="b", col=colors[i])
	i = i + 1
}
dev.off()
#]

q()
