#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <uthash.h>
#include "ubx_types.h"
#include "ubx_time.h"

static int64_t ts_to_ns(struct timespec *ts)
{
	return (long long)ts->tv_sec * 1000000000LL + ts->tv_nsec;
}

static int64_t timespec_diff_ns(struct timespec *start, struct timespec *end)
{
	return ts_to_ns(end) - ts_to_ns(start);
}

static int64_t ubx_timespec_diff_ns(struct ubx_timespec *start, struct ubx_timespec *end)
{
	return ubx_ts_to_ns(end) - ubx_ts_to_ns(start);
}

static int64_t random_sleep_ns(int64_t max_ns)
{
	return (int64_t)((double)rand() / RAND_MAX * max_ns);
}

int main(int argc, char *argv[])
{
	int num_samples = 1000;
	int use_monotonic = 1;
	int64_t max_sleep_ns = 100000000LL; /* 100ms default */

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == 'n' && i + 1 < argc) {
			num_samples = atoi(argv[++i]);
		} else if (argv[i][0] == '-' && argv[i][1] == 'r') {
			use_monotonic = 0;
		} else if (argv[i][0] == '-' && argv[i][1] == 's' && i + 1 < argc) {
			max_sleep_ns = (int64_t)(atof(argv[++i]) * 1000000000.0);
		} else if (argv[i][0] == '-' && argv[i][1] == 'h') {
			printf("Usage: %s [-n samples] [-m] [-s max_sleep_sec]\n", argv[0]);
			printf("  -n samples        number of samples to take (default: 1000)\n");
			printf("  -r                use CLOCK_REALTIME instead of CLOCK_MONOTONIC\n");
			printf("  -s max_sleep_sec  max rand sleep in seconds (default: 0.1)\n");
			return 0;
		}
	}

	clockid_t clock_id = use_monotonic ? CLOCK_MONOTONIC : CLOCK_REALTIME;
	const char *clock_name = use_monotonic ? "CLOCK_MONOTONIC" : "CLOCK_REALTIME";

	srand(time(NULL));

	printf("clock: %s\n", clock_name);
	printf("samples: %d\n", num_samples);
	printf("max sleep: %.3f seconds\n\n", max_sleep_ns / 1000000000.0);

	long long total_error = 0;
	long long max_error = 0;
	long long min_error = LLONG_MAX;
	double total_percent = 0.0;
	int errors = 0;

	for (int i = 0; i < num_samples; i++) {
		struct timespec posix_start, posix_end;
		struct ubx_timespec ubx_start, ubx_end;

		if (clock_gettime(clock_id, &posix_start) != 0) {
			perror("clock_gettime failed (start)");
			errors++;
			continue;
		}

		if (ubx_gettime(&ubx_start) != 0) {
			fprintf(stderr, "ubx_gettime failed (start) at sample %d\n", i);
			errors++;
			continue;
		}

		int64_t sleep_ns = random_sleep_ns(max_sleep_ns);
		struct timespec sleep_time = {
			.tv_sec = sleep_ns / 1000000000LL,
			.tv_nsec = sleep_ns % 1000000000LL
		};
		nanosleep(&sleep_time, NULL);

		if (clock_gettime(clock_id, &posix_end) != 0) {
			perror("clock_gettime failed (end)");
			errors++;
			continue;
		}

		if (ubx_gettime(&ubx_end) != 0) {
			fprintf(stderr, "ubx_gettime failed (end) at sample %d\n", i);
			errors++;
			continue;
		}

		int64_t posix_diff = timespec_diff_ns(&posix_start, &posix_end);
		int64_t ubx_diff = ubx_timespec_diff_ns(&ubx_start, &ubx_end);

		int64_t error_ns = llabs(posix_diff - ubx_diff);
		double percent_error = posix_diff > 0 ?
			((double)error_ns / (double)posix_diff) * 100.0 : 0.0;

		total_error += error_ns;
		total_percent += percent_error;

		if (error_ns > max_error)
			max_error = error_ns;
		if (error_ns < min_error)
			min_error = error_ns;

		if (i % (num_samples/10) == 0) {
			printf("Sample %4d (sleep: %.6f s):\n", i, sleep_ns / 1000000000.0);
			printf("  POSIX diff: %ld ns (%.6f s)\n", posix_diff, posix_diff / 1000000000.0);
			printf("  ubx diff:   %ld ns (%.6f s)\n", ubx_diff, ubx_diff / 1000000000.0);
			printf("  Error:      %ld ns (%.4f%%)\n\n", error_ns, percent_error);
		}
	}

	int valid_samples = num_samples - errors;

	if (valid_samples > 0) {
		printf("\nResults Summary:\n");
		printf("================\n");
		printf("Valid samples:     %d / %d\n", valid_samples, num_samples);
		printf("Failed calls:      %d\n", errors);
		printf("\nAbsolute Error:\n");
		printf("  avg:         %lld ns\n", total_error / valid_samples);
		printf("  min:         %lld ns\n", min_error);
		printf("  max:         %lld ns\n", max_error);
		printf("\nPercent Error:\n");
		printf("  Average:         %.4f%%\n", total_percent / valid_samples);
	} else {
		printf("\nNo valid samples collected!\n");
		return 1;
	}

	return 0;
}
