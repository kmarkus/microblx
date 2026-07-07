#ifndef UBX_STAT_H
#define UBX_STAT_H

/**
 * struct ubx_stat - running statistics of a numeric signal
 *
 * All fields are computed as double, independent of the configured
 * input type, so that the type of the 'stats' port is fixed.
 *
 * @cnt: number of samples accumulated so far
 * @min: smallest sample seen
 * @max: largest sample seen
 * @mean: arithmetic mean (average) of all samples
 * @std: population standard deviation (divides by cnt)
 */
struct ubx_stat {
	unsigned long cnt;
	double min;
	double max;
	double mean;
	double std;
};

#endif /* UBX_STAT_H */
