
#include <math.h>
#include <stdlib.h>
#include <vector>
#include <limits>
#include <string.h>
#include <stdio.h>
#include "stream_stat.hpp"

stream_stat_t::stream_stat_t(int _buckets, int _bucket_size_exp) : buckets(_buckets), bucket_size_exp(_bucket_size_exp), bucket_size(pow(10, _bucket_size_exp)) {	

	default_percentile_marks.push_back(0.50); // marks must be sorted
	default_percentile_marks.push_back(0.60);
	default_percentile_marks.push_back(0.70);
	default_percentile_marks.push_back(0.80);
	default_percentile_marks.push_back(0.90);
	default_percentile_marks.push_back(0.95);
	default_percentile_marks.push_back(0.99);

	active_stat = new stat_counters_t();
	init_stat_counters(active_stat);
	snapshot_stat = new stat_counters_t();
	init_stat_counters(snapshot_stat);
}

stream_stat_t::~stream_stat_t() {	
	destroy_stat_counters(active_stat);
	destroy_stat_counters(snapshot_stat);
}

void stream_stat_t::init_stat_counters(stat_counters_t *stat_counters) {	
	stat_counters->sum_values = 0;
	stat_counters->min_value = std::numeric_limits<ticks_t>::max();
	stat_counters->max_value = 0;
	stat_counters->count = 0;	
	stat_counters->histogram = (long*)calloc(buckets * bucket_size, sizeof(long));
}

void stream_stat_t::destroy_stat_counters(stat_counters_t *stat_counters) {
	free(stat_counters->histogram);
	free(stat_counters);	
}

void stream_stat_t::add(ticks_t value) {
	int b = ceil(log10(double(value / bucket_size)));		
	if(b < buckets) {
		int compressed_value = value;
		if(b != 0) {
			compressed_value = floor( double(value) / pow(10, b) + 0.5);
		}		

		active_stat->histogram[b * bucket_size + compressed_value]++;

		active_stat->count++;
		active_stat->sum_values += value;
		active_stat->max_value = std::max(active_stat->max_value, value);
		active_stat->min_value = std::min(active_stat->min_value, value);
	}
}

void stream_stat_t::snapshot() {
	destroy_stat_counters(snapshot_stat);	
	snapshot_stat = active_stat;

	stat_counters_t *active_new_stat = new stat_counters_t();
	init_stat_counters(active_new_stat);

	active_stat = active_new_stat;
}

stat_data_t stream_stat_t::get_percentiles() {
	return get_percentiles(default_percentile_marks);
}

stat_data_t stream_stat_t::get_percentiles(std::vector<double> &percentile_marks) {
	int mark_pointer = 0;
	
	std::map<double, ticks_t> percentiles;
	long total = 0;	
	for(int i = 0; total < snapshot_stat->count && i < buckets*bucket_size && mark_pointer < percentile_marks.size(); i++) {	
		if(total >= percentile_marks[mark_pointer] * snapshot_stat->count) {
			int b = i / bucket_size;
			ticks_t latency = (i - b*bucket_size) * pow(10, b);
			//printf(">>> total=%d b=%d i=%d latency=%d\n", total, b, i, latency);
			percentiles.insert( std::pair<double, ticks_t>(percentile_marks[mark_pointer], latency) );

			mark_pointer++;
		}
		total += snapshot_stat->histogram[i];
	}
	while(mark_pointer < percentile_marks.size()) {
		percentiles.insert( std::pair<double, ticks_t>(percentile_marks[mark_pointer], snapshot_stat->max_value));
		mark_pointer++;
	}

	stat_data_t result;
	result.percentiles = percentiles;
	result.count = snapshot_stat->count;
	result.mean = double(snapshot_stat->sum_values) / snapshot_stat->count;
	result.min_value = snapshot_stat->min_value;
	result.max_value = snapshot_stat->max_value;
	
	return result;
}
