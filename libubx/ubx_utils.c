/* miscellaneous */

#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "ubx_utils.h"

/**
 * Wait for SIGINT for some time
 * @param timeout_s timeout in seconds
 * @return 0 if SIGINT occurred, errno of sigwaitinfo otherwise
 */
int ubx_wait_sigint(unsigned int timeout_s)
{
	sigset_t set;
	struct timespec timeout;

	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	timeout.tv_sec = timeout_s;
	timeout.tv_nsec = 0;

	if (sigprocmask(SIG_BLOCK, &set, NULL) < 0) {
		perror("sigprocmask");
		return -1;
	}

	int ret;
	do {
		ret = sigtimedwait(&set, NULL, &timeout);
	} while (ret < 0 && errno == EINTR);

	return (ret < 0) ? errno : 0;
}

/**
 * char_replace - replace all occurrences of a character in a string
 * @param s string to modify in place
 * @param find character to search for
 * @param rep replacement character
 */
void char_replace(char *s, const char find, const char rep)
{
	size_t len = strlen(s);
	for(size_t i=0; i<len; i++) {
		if (s[i] == find)
			s[i] = rep;
	}
}
