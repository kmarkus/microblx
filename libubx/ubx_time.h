/* microblx time handling functions */

#ifndef _UBX_TIME_H
#define _UBX_TIME_H

int ubx_gettime(struct ubx_timespec *uts);
int ubx_nanosleep(const struct ubx_timespec *dur);
int ubx_nanowait(const struct ubx_timespec *dur);
int ubx_nanosleep_hybrid(const struct ubx_timespec *dur, uint64_t slack_ns);
int ubx_ts_cmp(const struct ubx_timespec *ts1, const struct ubx_timespec *ts2);
void ubx_ts_norm(struct ubx_timespec *ts);
void ubx_ts_sub(const struct ubx_timespec *ts1, const struct ubx_timespec *ts2, struct ubx_timespec *out);
void ubx_ts_add(const struct ubx_timespec *ts1, const struct ubx_timespec *ts2, struct ubx_timespec *out);
void ubx_ts_div(const struct ubx_timespec *ts, const long div, struct ubx_timespec *out);
double ubx_ts_to_double(const struct ubx_timespec *ts);
uint64_t ubx_ts_to_ns(const struct ubx_timespec *ts);
uint64_t ubx_gettime_ns(void);
uint64_t ubx_ts_to_us(const struct ubx_timespec *ts);

#endif /* _UBX_TIME_H */
