/* miscellaneous */

#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

/**
 * Wait for SIGINT for some time
 * @param timeout_s timeout in seconds
 * @return 0 if SIGINT occurred, errno of sigwaitinfo otherwise
 */
int ubx_wait_sigint(unsigned int timeout_s)
{
	sigset_t set;
	sigset_t oldset;
	struct timespec timeout;

	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	timeout.tv_sec = timeout_s;
	timeout.tv_nsec = 0;

	if (sigprocmask(SIG_BLOCK, &set, &oldset) < 0) {
		perror ("sigprocmask");
		return -1;
	}

	if (sigtimedwait(&set, NULL, &timeout) < 0)
		return errno;

	return 0;
}

/**
 * replace char  in string
 */
void char_replace(char *s, const char find, const char rep)
{
	size_t len = strlen(s);
	for(size_t i=0; i<len; i++) {
		if (s[i] == find)
			s[i] = rep;
	}
}
