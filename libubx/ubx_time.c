/* Time handling */

#include "ubx.h"
#include <config.h>

#if defined(TIMESRC_TSC)
/**
 * rdtscp
 *
 * @return current tsc
 */
static uint64_t rdtscp(void)
{
	uint64_t tsc;

	__asm__ __volatile__(
		"rdtscp;"
		"shl $32, %%rdx;"
		"or %%rdx, %%rax"
		: "=a"(tsc)
		:
		: "%rcx", "%rdx");

	return tsc;
}

/**
 * ubx_clock_gettime - get elapsed using TSC counters
 * @param uts
 * @return 0 or EINVALID_ARG
 */
int ubx_gettime(struct ubx_timespec *uts)
{
	if (uts == NULL)
		return EINVALID_ARG;

	uint64_t tsc = rdtscp();

	uts->sec = tsc / CPU_HZ;
	uts->nsec = ((tsc % CPU_HZ) * NSEC_PER_SEC) / CPU_HZ;

	return 0;
}

#elif defined(TIMESRC_CNTVCT)

#if !defined(__aarch64__) && !defined(__arm64__)
#error "TIMESRC_CNTVCT: only support on aarach64/arm64"
#endif

int ubx_gettime(struct ubx_timespec *uts)
{
	uint64_t cnt;
	static uint64_t cntfrq = 0;

	if (uts == NULL)
		return EINVALID_ARG;

	if (!cntfrq)
		asm volatile("mrs %0, cntfrq_el0" : "=r"(cntfrq));

	asm volatile("mrs %0, cntvct_el0" : "=r"(cnt));

	uts->sec = cnt / cntfrq;
	uint64_t rem = cnt % cntfrq;
	uts->nsec = (rem * 1000000000ULL) / cntfrq;

	return 0;
}

#else /* no HW timestamps */
/**
 * ubx_clock_gettime
 * get current time using clock_gettime(CLOCK_MONOTONIC).
 *
 * @param uts
 *
 * @return non-zero in case of error, 0 otherwise.
 */
int ubx_gettime(struct ubx_timespec *uts)
{
	if (uts == NULL)
		return EINVALID_ARG;

	return clock_gettime(CLOCK_MONOTONIC, (struct timespec *)uts);
}

#endif /* TIMESRC_* */

#if defined(TIMESRC_TSC) || defined(TIMESRC_CNTVCT)
/**
 * ubx_clock_nanosleep - get elapsed using tsc counter
 *
 * @param flags	(same flags as clock_nanosleep)
 * @param request abs or relative time to sleep
 * @return 0 or error
 */
int ubx_nanosleep(int flags, struct ubx_timespec *request)
{
	int ret;
	struct ubx_timespec *endp, end, now;

	if (flags & TIMER_ABSTIME) {
		endp = request;
	} else {
		ret = ubx_gettime(&end);

		if (ret)
			goto out;

		ubx_ts_add(&end, request, &end);
		endp = &end;
	}

	for (;;) {
		ret = ubx_gettime(&now);

		if (ret)
			goto out;

		if (ubx_ts_cmp(&now, endp) == 1)
			break;
	}
out:
	return ret;
}

#else /* use POSIX clock_nanosleep */

int ubx_nanosleep(int flags, struct ubx_timespec *request)
{
	struct timespec *ts = (struct timespec *)request;
	return clock_nanosleep(CLOCK_MONOTONIC, flags, ts, NULL);
}

#endif

/**
 * Compare two ubx_timespecs
 *
 * @param ts1
 * @param ts2
 *
 * @return return 1 if t1 is greater than t2, -1 if t1 is less than t2
 *         and 0 if t1 and t2 are equal.
 */
int ubx_ts_cmp(const struct ubx_timespec *ts1, const struct ubx_timespec *ts2)
{
	if (ts1->sec > ts2->sec)
		return 1;
	else if (ts1->sec < ts2->sec)
		return -1;
	else if (ts1->nsec > ts2->nsec)
		return 1;
	else if (ts1->nsec < ts2->nsec)
		return -1;
	else
		return 0;
}

/**
 * Normalize a timespec.
 *
 * @param ts
 */
void ubx_ts_norm(struct ubx_timespec *ts)
{
	if (ts->nsec >= NSEC_PER_SEC) {
		ts->sec += ts->nsec / NSEC_PER_SEC;
		ts->nsec = ts->nsec % NSEC_PER_SEC;
	}

	/* normalize negative nsec */
	if (ts->nsec <= -NSEC_PER_SEC) {
		ts->sec += ts->nsec / NSEC_PER_SEC;
		ts->nsec = ts->nsec % -NSEC_PER_SEC;
	}

	if (ts->sec > 0 && ts->nsec < 0) {
		ts->sec -= 1;
		ts->nsec += NSEC_PER_SEC;
	}

	if (ts->sec < 0 && ts->nsec > 0) {
		ts->sec++;
		ts->nsec -= NSEC_PER_SEC;
	}
}

/**
 * ubx_ts_sub - substract ts2 from ts1 and store the result in out
 *
 * @param ts1
 * @param ts2
 * @param out
 */
void ubx_ts_sub(const struct ubx_timespec *ts1, const struct ubx_timespec *ts2,
		struct ubx_timespec *out)
{
	out->sec = ts1->sec - ts2->sec;
	out->nsec = ts1->nsec - ts2->nsec;
	ubx_ts_norm(out);
}

/**
 * ubx_ts_add - compute the sum of two timespecs and store the result in out
 *
 * @param ts1
 * @param ts2
 * @param out
 */
void ubx_ts_add(const struct ubx_timespec *ts1, const struct ubx_timespec *ts2,
		struct ubx_timespec *out)
{
	out->sec = ts1->sec + ts2->sec;
	out->nsec = ts1->nsec + ts2->nsec;
	ubx_ts_norm(out);
}

/**
 * ubx_ts_div - divide the value of ts by div and store the result in out
 *
 * @param ts
 * @param div
 * @param out
 */
void ubx_ts_div(const struct ubx_timespec *ts, const long div,
		struct ubx_timespec *out)
{
	int64_t tmp_nsec = (ts->sec * NSEC_PER_SEC) + ts->nsec;

	tmp_nsec /= div;
	out->sec = tmp_nsec / NSEC_PER_SEC;
	out->nsec = tmp_nsec % NSEC_PER_SEC;
}

/**
 * ubx_ts_to_double - convert ubx_timespec to double [s]
 *
 * @param ts
 *
 * @return time in seconds
 */
double ubx_ts_to_double(const struct ubx_timespec *ts)
{
	return ((double)ts->sec) + ((double)ts->nsec / NSEC_PER_SEC);
}

/**
 * ubx_ts_to_ns - convert ubx_timespec to uint64_t [ns]
 *
 * @param ts
 *
 * @return time in nanoseconds
 */
uint64_t ubx_ts_to_ns(const struct ubx_timespec *ts)
{
	return ts->sec * (uint64_t)NSEC_PER_SEC + ts->nsec;
}

/**
 * ubx_ts_to_us - convert ubx_timespec to uint64_t [us]
 *
 * @param ts
 *
 * @return time in microseconds
 */
uint64_t ubx_ts_to_us(const struct ubx_timespec *ts)
{
	return ts->sec * (uint64_t)USEC_PER_SEC + ts->nsec / NSEC_PER_USEC;
}
