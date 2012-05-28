
#include <math.h>
#include <stdlib.h>
#include "stream_stat.hpp"


stream_stat_t::stream_stat_t(int _buckets, int _bucket_size) : buckets(_buckets), bucket_size(_bucket_size) {
	histogram = (long*)calloc(buckets * pow(10, bucket_size), sizeof(long));
}

void stream_stat_t::add(ticks_t value) {
	int b = floor(log10(double(value) / _bucket_size));
	int compressed_value = floor((double(value) - pow(10, b)*bucket_size) / pow(10, b) + 0.5);
}
