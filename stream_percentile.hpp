#ifndef __STREAM_STAT_HPP__
#define __STREAM_STAT_HPP__

#include "utils.hpp"

class stream_stat_t {
public:
	stream_stat_t(int _buckets, int _bucket_size);
	
	void add(ticks_t value);
	//std::vector<ticks_t> get_percentiles(std::vector<ticks_t> percentile_marks);
	
private:
	long *histogram;
	int buckets;
	int bucket_size;
};

#endif // __STREAM_STAT_HPP__
