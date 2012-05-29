# Usage : Rscript describe.R <trace1 filename> <trace1 name> <trace2 filename> <trace2 name> ...


library(ggplot2)
library(reshape2)

#[ parse arguments
args = commandArgs()
input_file_name <- c()
names <- c()
i <- 0
for(arg in args[6:length(args)]) {
  if(i %% 2 == 0) {
    input_file_name <- cbind(input_file_name, arg)
  } else {
    names <- cbind(names, arg)  
  }
  i <- i + 1
}
percentile_marks <- c(0.5, 0.6, 0.7, 0.8, 0.9, 0.95, 0.99, 0.995)
#]

i <- 1
for(filename in input_file_name) {
  current_trace <- read.table(filename)  
  current_trace <- cbind(current_trace, rep(names[[i]], length(current_trace[,1])))
  current_trace[,1] <- ( current_trace[,1] - min(current_trace[,1]) ) / 1e9
  
  if(i == 1) {
     trace <- current_trace;
  } else {
     trace <- rbind(trace, current_trace)
  }
  
  i <- i + 1
}

colnames(trace) <- c("time", "throughput_tps", "latency_mean", "latency_min", "latency_max", paste("latency_p", percentile_marks), "source_file" )

##########################################################

THOUGHPUT_TIME_PLOT_HEIGHT = 600
THOUGHPUT_TIME_PLOT_WIDTH = 1000
png(filename="throughput_time_plot.png", height=THOUGHPUT_TIME_PLOT_HEIGHT, width=THOUGHPUT_TIME_PLOT_WIDTH, bg="white")
throughput_data <- data.frame(time_sec=trace$time, throughput_tps=trace$throughput_tps, source_file=trace$source_file)
qplot(time_sec, throughput_tps, data=throughput_data, colour=source_file, geom = c("point", "line")) + 
  ylab("Throughput, IOPS") + xlab("Time, sec")
dev.off()

##########################################################

LATENCY_TIME_PLOT_HEIGHT = 600
LATENCY_TIME_PLOT_WIDTH = 1000
png(filename="latency_time_plot.png", height=LATENCY_TIME_PLOT_HEIGHT, width=LATENCY_TIME_PLOT_WIDTH, bg="white")
latency_data <- data.frame(time_sec=trace$time, latency_us=trace$latency_mean/1e3, source_file=trace$source_file)
qplot(time_sec, latency_us, data=latency_data, colour=source_file, geom = c("point", "line")) + 
  ylab("Latency, usec") + xlab("Time, sec")
dev.off()

##########################################################

LATENCY_DIST_TIME_PLOT_HEIGHT = 600
LATENCY_DIST_TIME_PLOT_WIDTH = 1000
png(filename="latency_dist_time_plot.png", height=LATENCY_DIST_TIME_PLOT_HEIGHT, width=LATENCY_DIST_TIME_PLOT_WIDTH, bg="white")
latency_dist_data <- melt( trace[,c(-2,-4,-5)], id=c("time", "source_file") )
latency_dist_data$value <- latency_dist_data$value/1e3 #to usec
latency_dist_data$qualifier <- paste(latency_dist_data$source_file, latency_dist_data$variable)
qplot(time, value, data=latency_dist_data, colour=qualifier, geom = c("point", "line")) + 
  ylab("Latency, usec") + xlab("Time, sec")
dev.off()

##########################################################

LATENCY_HIST_TIME_PLOT_HEIGHT = 2000
LATENCY_HIST_TIME_PLOT_WIDTH = 1000
png(filename="latency_hist_time_plot.png", height=LATENCY_HIST_TIME_PLOT_HEIGHT, width=LATENCY_HIST_TIME_PLOT_WIDTH, bg="white")
latency_dist_data <- melt( trace[,c(-2)], id=c("time", "source_file") )
latency_dist_data$qualifier <- paste(latency_dist_data$source_file, latency_dist_data$variable)
latency_dist_data$value <- latency_dist_data$value/1e3 #to usec
ggplot(latency_dist_data, aes(x=source_file, y=value, colour=source_file)) + stat_boxplot() + geom_jitter() + facet_grid(variable ~ ., scales = "free") + 
  ylab("Latency, usec") + xlab("Source File")
dev.off()

##########################################################

THROUGHPUT_HIST_TIME_PLOT_HEIGHT = 600
THROUGHPUT_HIST_TIME_PLOT_WIDTH = 1000
png(filename="throughput_hist_time_plot.png", height=THROUGHPUT_HIST_TIME_PLOT_HEIGHT, width=THROUGHPUT_HIST_TIME_PLOT_WIDTH, bg="white")
throughput_dist_data <- melt( trace[,c(1,2,length(trace))], id=c("time", "source_file") )
throughput_dist_data$qualifier <- paste(throughput_dist_data$source_file, throughput_dist_data$variable)
ggplot(throughput_dist_data, aes(x=source_file, y=value, colour=source_file)) + stat_boxplot() + geom_jitter() +
  ylab("Throughput, IOPS") + xlab("Source File")
dev.off()