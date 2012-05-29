
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
	default_percentile_marks.push_back(0.995);

	global_stat = new stat_counters_t();
	init_stat_counters(global_stat);
	active_stat = new stat_counters_t();
	init_stat_counters(active_stat);
	snapshot_stat = new stat_counters_t();
	init_stat_counters(snapshot_stat);
}

stream_stat_t::~stream_stat_t() {	
	destroy_stat_counters(global_stat);
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

		add(active_stat, b, compressed_value, value);
		add(global_stat, b, compressed_value, value);
	}
}

inline void stream_stat_t::add(stat_counters_t *stat_counters, int b, int compressed_value, ticks_t value) {
	stat_counters->histogram[b * bucket_size + compressed_value]++;

	stat_counters->count++;
	stat_counters->sum_values += value;
	stat_counters->max_value = std::max(stat_counters->max_value, value);
	stat_counters->min_value = std::min(stat_counters->min_value, value);
}

void stream_stat_t::snapshot_and_reset() {
	destroy_stat_counters(snapshot_stat);	
	snapshot_stat = active_stat;

	stat_counters_t *active_new_stat = new stat_counters_t();
	init_stat_counters(active_new_stat);

	active_stat = active_new_stat;
}

stat_data_t stream_stat_t::get_snapshot_stat() {
	return get_snapshot_stat(default_percentile_marks);
}

stat_data_t stream_stat_t::get_snapshot_stat(std::vector<double> &percentile_marks) {
	return get_stat(snapshot_stat, percentile_marks);
}

stat_data_t stream_stat_t::get_global_stat() {
	return get_global_stat(default_percentile_marks);
}

stat_data_t stream_stat_t::get_global_stat(std::vector<double> &percentile_marks) {
	return get_stat(global_stat, percentile_marks);
}

stat_data_t stream_stat_t::get_stat(stat_counters_t *stat_counters, std::vector<double> &percentile_marks) {
	int mark_pointer = 0;
	
	std::map<double, ticks_t> percentiles;
	long total = 0;	
	for(int i = 0; total < stat_counters->count && i < buckets*bucket_size && mark_pointer < percentile_marks.size(); i++) {	
		if(total >= percentile_marks[mark_pointer] * stat_counters->count) {
			int b = i / bucket_size;
			ticks_t latency = (i - b*bucket_size) * pow(10, b);
			//printf(">>> total=%d b=%d i=%d latency=%d\n", total, b, i, latency);
			percentiles.insert( std::pair<double, ticks_t>(percentile_marks[mark_pointer], latency) );

			mark_pointer++;
		}
		total += stat_counters->histogram[i];
	}
	while(mark_pointer < percentile_marks.size()) {
		percentiles.insert( std::pair<double, ticks_t>(percentile_marks[mark_pointer], stat_counters->max_value));
		mark_pointer++;
	}

	stat_data_t result;
	result.percentiles = percentiles;
	result.count = stat_counters->count;
	result.mean = double(stat_counters->sum_values) / stat_counters->count;
	result.min_value = stat_counters->min_value;
	result.max_value = stat_counters->max_value;
	
	return result;	
}
