/*
 * ubx_log: microblx logging client
 *
 * Copyright (C) 2019 Hamish Guthrie <hamish.guthrie@kistler.com>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <termios.h>
#include <sys/inotify.h>

#ifdef HAVE_LIBDAEMON
# include <libdaemon/dfork.h>
# include <libdaemon/dpid.h>
#endif

#include "ubx.h"
#include "rtlog_client.h"

const char *loglevel_str[] = {
	"EMERG", "ALERT", "CRIT", "ERROR",
	"WARN", "NOTICE", "INFO", "DEBUG"
};

#define REOPEN_RETRY_NUM	10
#define REOPEN_RETRY_TIMEOUT_US	200000

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

const char *loglevel_color[] = {
	RED, RED, RED, RED,
	YEL, CYN, WHT, MAG
};

/* syslog priority values for UBX log levels (values are identical) */
static const int syslog_prio[] = {
	LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR,
	LOG_WARNING, LOG_NOTICE, LOG_INFO, LOG_DEBUG
};

static int use_syslog = 0;
static int syslog_facility = LOG_LOCAL0;
static int daemon_mode = 0;
static volatile sig_atomic_t quit = 0;
static struct termios orig_tp;
static int orig_tp_saved = 0;

static void sig_handler(int sig)
{
	(void)sig;
	quit = 1;
}

/*
 * diag_log - write a diagnostic message to stderr (normal mode) or syslog
 * (daemon mode, where stderr is /dev/null)
 */
static __attribute__((format(printf, 3, 4)))
void diag_log(int prio, int color, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (daemon_mode) {
		vsyslog(prio, fmt, ap);
	} else {
		if (color) fprintf(stderr, "%s", RED);
		vfprintf(stderr, fmt, ap);
		if (color) fprintf(stderr, "%s", RESET);
	}
	va_end(ap);
}

static void log_data(logc_info_t *inf, int color)
{
	struct ubx_log_msg *msg;
	int ret;
	const char *level_str;
	int prio;

	ret = logc_read_frame(inf, (volatile log_frame_t **)&msg);
	switch (ret) {
	case NO_DATA:
		break;

	case NEW_DATA:
		if (msg->level > UBX_LOGLEVEL_DEBUG ||
		    msg->level < UBX_LOGLEVEL_EMERG) {
			level_str = "INVALID";
			prio = LOG_ERR;
		} else {
			level_str = loglevel_str[msg->level];
			prio = syslog_prio[msg->level];
		}

		if (color)
			fprintf(stdout, GRN "[%li.%06li] " YEL "%s %s%s: %s\n" RESET,
				msg->ts.sec, msg->ts.nsec / NSEC_PER_USEC,
				msg->src,
				loglevel_color[msg->level],
				level_str, msg->msg);
		else
			fprintf(stdout, "[%li.%06li] %s %s: %s\n",
				msg->ts.sec, msg->ts.nsec / NSEC_PER_USEC,
				msg->src, level_str, msg->msg);

		fflush(stdout);

		if (use_syslog)
			syslog(prio, "%s %s: %s", msg->src, level_str, msg->msg);
		break;
	}
}

const char* sepstr = "--------------------------------------------------------------------------------\n";
#define SEP(color) fprintf(stderr, "%s%s%s", (color==1) ? MAG : "", sepstr, (color==1) ? RESET : "")

/*
 * The code below utilises inotify to detect when a shm becomes
 * available if the logger is launched before the function blocks. At
 * runtime it can also detect when the shm is re-created in order to
 * allow the client to close the old shm and reopen the new shm. It
 * can easily be cleanly separated from this client and implemented as
 * a library for re-use by other aggregators
 */

#define EVENT_SIZE sizeof(struct inotify_event)
#define BUF_LEN (10 * (EVENT_SIZE + NAME_MAX + 1))

/**
 * uin_info - inotify local data struct
 *
 * path:	direcory path to monitor
 * file:	filename to monitor
 * infd:	inotify file descriptor
 * wd:		inotify watch descriptor
 * mask:	inotify event mask to monitor
 * inbuf:	buffer of inotify events
 */
struct uin_info {
	const char *path;
	const char *file;
	int infd;
	int wd;
	uint32_t mask;
	char inbuf[BUF_LEN] __attribute__ ((aligned(8)));
};

/**
 * start_inotify - start inotify
 *
 * @param inf:		local data
 * @param path:		path of direcory to monitor
 * @param file:		file to monitor
 * @param flags:	0 - blocking, IN_NONBLOCK - nonblocking
 * @param mask:		inotify events to monitor
 *
 * @return:		0 - success, non-zero -errno on failure
 */
static int start_inotify(struct uin_info *inf, const char *path, const char *file,
			 int flags, uint32_t mask)
{
	int ret = 0;

	inf->path = path;
	inf->file = file;
	inf->mask = mask;
	inf->infd = inotify_init1(flags);
	if (inf->infd == -1) {
		ret = -errno;
		fprintf(stderr, "failed to initialise inotify: %d: %s\n",
			errno, strerror(errno));
		goto out;
	}

	inf->wd = inotify_add_watch(inf->infd, inf->path, inf->mask);
	if (inf->wd == -1) {
		ret = -errno;
		fprintf(stderr, "failed to add inotify watch: %d: %s\n",
			errno, strerror(errno));
		goto out_close_infd;
	}

	goto out;

out_close_infd:
	close(inf->infd);

out:
	return ret;
}

/**
 * check_inotify - check inotify events
 *		   NOTE: depending on the flags parameter for start_inotify
 *		         this function will block or nonblock hence may be
 *			 used for both waiting until a shm becomes available
 *			 or to detect if the shm has been reopened
 *
 * @param inf:		local data
 *
 * @return:		1 - event to be monitored for detected
 *			0 - event not detected
 *			-ve value, -errno in case of error
 */
static int check_inotify(struct uin_info *inf)
{
	int ret = 0;
	ssize_t nr;
	struct inotify_event *event;
	char *p;

	nr = read(inf->infd, inf->inbuf, BUF_LEN);
	if (nr < 0) {
		ret = -errno;
		goto out;
	}

	for (p = inf->inbuf; p < inf->inbuf + nr; ) {
		event = (struct inotify_event *)p;

		if ((strncmp(inf->file, event->name, strlen(inf->file)) == 0)
		     && (event->mask & inf->mask)) {
			ret = 1;
			break;
		}

		p += sizeof(struct inotify_event) + event->len;
	}
out:
	return ret;
}

/*
 * end of inotify 'library' code
 */

#define SHM_DIRPATH "/dev/shm"

/**
 * ubx_log_info - local data struct to be used by logger client
 *
 * lcinf:	log client local data structure
 * uininf:	inotify local data structure
 */
struct ubx_log_info {
	logc_info_t *lcinf;
	struct uin_info *uininf;
};

/**
 * lc_init - log client initialisation
 *
 * @param inf:	log client local data
 */
static int lc_init(struct ubx_log_info *inf)
{
	int ret = EOUTOFMEM;

	inf->lcinf = calloc(1, sizeof(logc_info_t));

	if (inf->lcinf == NULL) {
		fprintf(stderr, "failed to alloc logc_info_t\n");
		goto out;
	}

	inf->uininf = calloc(1, sizeof(struct uin_info));

	if (inf->uininf == NULL) {
		fprintf(stderr, "failed to alloc uin_info\n");
		goto out_free_logc_info;
	}

	ret = start_inotify(inf->uininf, SHM_DIRPATH, LOG_SHM_FILENAME,
			    0, IN_CREATE);
	if (ret < 0)
		goto out_free_uin_info;

	while (1) {
		ret = logc_init(inf->lcinf, LOG_SHM_FILENAME,
				sizeof(struct ubx_log_msg));
		if (ret == 0) {
			break;
		}

		if (ret == ENAMETOOLONG) {
			fprintf(stderr, "failed to initialise logging library\n");
			goto out_close_infd;
		}

		if (ret == ENOENT) {
			int done = 0;

			diag_log(LOG_INFO, 0, "waiting for %s to appear\n",
				 LOG_SHM_FILENAME);
			while(!done) {
				done = check_inotify(inf->uininf);
				if (done < 0) {
					ret = done;
					goto out;
				}
			}
		}
	}

	/* the shm file has appeared. close the blocking inotify fd
	 * and reopen in non-blocking mode for monitoring during
	 * regular logging operation. The following close will also
	 * remove all notification structures in the kernel
	 */
	close(inf->uininf->infd);

	ret = start_inotify(inf->uininf, SHM_DIRPATH, LOG_SHM_FILENAME,
			    IN_NONBLOCK, IN_CREATE | IN_MODIFY);
	if (ret != 0) {
		fprintf(stderr, "start_inotify failed: %d: %s\n", ret,
			strerror(-ret));
		goto out_free_logc_info;
	}
	goto out;

out_close_infd:
	close(inf->uininf->infd);

out_free_uin_info:
	free(inf->uininf);

out_free_logc_info:
	free(inf->lcinf);

out:
	return ret;
}

/**
 * check_new_shm - check if the shm has been (re)created
 *
 * @param inf:	log client local data
 */
static int check_new_shm(struct ubx_log_info *inf, int show_old, int color)
{
	int ret = 0;
	int retries = REOPEN_RETRY_NUM;

	ret = check_inotify(inf->uininf);

	switch(ret) {
	case 0:
	case -EAGAIN:
		ret = 0;
		break;

	case 1:
		diag_log(LOG_INFO, color,
			 "ubx log shm recreation/truncation detected - reopening\n");

		while(retries-- >= 0) {
				logc_close(inf->lcinf);
				ret = logc_init(inf->lcinf, LOG_SHM_FILENAME,
						sizeof(struct ubx_log_msg));

				if(ret == 0)
					break;

				usleep(REOPEN_RETRY_TIMEOUT_US);
		}

		if (ret != 0) {
			diag_log(LOG_ERR, color, "logc_init failed reinitialize\n");
			break;
		}

		if(show_old)
			logc_seek_to_oldest(inf->lcinf);

		break;

	default:
		diag_log(LOG_ERR, color,
			 "error %d ocurred checking inotify: %s\n",
			 ret, strerror(-ret));
		break;
	}

	return ret;
}

static ssize_t ngetc(char *c)
{
	return read (0, c, 1);
}

/*
 * parse_facility - parse a syslog LOCAL facility name (LOCAL0..LOCAL7)
 *
 * @param s:	facility name string
 * @return:	LOG_LOCAL* constant, or -1 on invalid input
 */
static int parse_facility(const char *s)
{
	static const int facilities[] = {
		LOG_LOCAL0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3,
		LOG_LOCAL4, LOG_LOCAL5, LOG_LOCAL6, LOG_LOCAL7
	};
	char *end;
	long n;

	if (strncmp(s, "LOCAL", 5) != 0)
		return -1;

	n = strtol(s + 5, &end, 10);
	if (*end != '\0' || n < 0 || n > 7)
		return -1;

	return facilities[n];
}

static void print_help(char **argv)
{
	printf("usage:\n");
	printf(" %s [options]\n", argv[0]);
	printf("   show ubx log messages\n\n");
	printf("Options:\n");
	printf("  -N         don't use colors\n");
	printf("  -O         don't show old messages upon startup\n");
	printf("  -s         forward messages to syslog (in addition to stdout)\n");
	printf("  -f LOCAL<n> syslog facility LOCAL0..LOCAL7 (default: LOCAL0)\n");
#ifdef HAVE_LIBDAEMON
	printf("  -d         run as daemon (requires -s; implies -O is recommended)\n");
#endif
	printf("  -h         show this help and exit\n");
}

int main(int argc, char **argv)
{
	int opt, color = 1, show_old = 1, ret = EOUTOFMEM;
	struct ubx_log_info *inf;
	struct termios tp;
	char c;

	while ((opt = getopt(argc, argv, "ONhsf:"
#ifdef HAVE_LIBDAEMON
			     "d"
#endif
			     )) != -1) {
		switch (opt) {
		case 'N':
			color = 0;
			break;
		case 'O':
			show_old = 0;
			break;
		case 's':
			use_syslog = 1;
			break;
		case 'f':
			syslog_facility = parse_facility(optarg);
			if (syslog_facility < 0) {
				fprintf(stderr, "invalid facility '%s': expected LOCAL0..LOCAL7\n",
					optarg);
				exit(EXIT_FAILURE);
			}
			break;
#ifdef HAVE_LIBDAEMON
		case 'd':
			daemon_mode = 1;
			break;
#endif
		case 'h':
		default: /* '?' */
			print_help(argv);
			exit(EXIT_FAILURE);
		}
	}

	if (daemon_mode && !use_syslog) {
		fprintf(stderr, "daemon mode requires syslog forwarding (-s)\n");
		exit(EXIT_FAILURE);
	}

#ifdef HAVE_LIBDAEMON
	if (daemon_mode) {
		pid_t pid;

		daemon_pid_file_ident = "ubx-log";

		if (daemon_pid_file_is_running() >= 0) {
			fprintf(stderr, "ubx-log daemon is already running\n");
			exit(EXIT_FAILURE);
		}

		daemon_retval_init();

		pid = daemon_fork();
		if (pid < 0) {
			daemon_retval_done();
			fprintf(stderr, "daemon_fork failed: %m\n");
			exit(EXIT_FAILURE);
		}

		if (pid > 0) {
			/* parent: wait for daemon to signal it started */
			ret = daemon_retval_wait(10);
			exit(ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
		}

		/* daemon child: create PID file, then release parent */
		if (daemon_pid_file_create() < 0) {
			syslog(LOG_ERR, "failed to create PID file: %m");
			daemon_retval_send(1);
			exit(EXIT_FAILURE);
		}

		/*
		 * Signal the parent that startup succeeded. lc_init may
		 * block waiting for the shm, so we release the parent first
		 * and let the daemon wait in the background.
		 */
		daemon_retval_send(0);
		color = 0;
	}
#endif

	if (use_syslog)
		openlog("ubx-log", LOG_PID, syslog_facility);

	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);

	inf = calloc(1, sizeof(struct ubx_log_info));
	if (inf == NULL) {
		fprintf(stderr, "failed to alloc ubx_log_info\n");
		goto out;
	}
	inf->lcinf = NULL;
	inf->uininf = NULL;

	if (!daemon_mode) {
		/* make stdin nonblocking */
		fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

		/* turn echo off, saving original settings for restoration on exit */
		if (tcgetattr(STDIN_FILENO, &orig_tp) == 0) {
			orig_tp_saved = 1;
			tp = orig_tp;
			tp.c_lflag &= ~ECHO;
			if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tp) == -1)
				fprintf(stderr, "tcsetattr: %m");
		} else {
			fprintf(stderr, "tcgetattr: %m");
		}
	}

	ret = lc_init(inf);
	if (ret != 0)
		goto out_free;

	if(show_old)
		logc_seek_to_oldest(inf->lcinf);

	while (!quit) {
		/* check for create shm event */
		ret = check_new_shm(inf, show_old, color);
		if (ret != 0)
			goto out_close;

		ret = logc_has_data(inf->lcinf);
		switch (ret) {
		case NO_DATA:
			if (!daemon_mode && ngetc(&c) > 0) {
				if (c == '\n')
					SEP(color);
			}

			usleep(100000);
			continue;

		case NEW_DATA:
			log_data(inf->lcinf, color);
			break;

		case OVERRUN:
			diag_log(LOG_WARNING, color, "OVERRUN - reset read side\n");
			logc_reset_read(inf->lcinf);
			break;

		case ERROR:
			diag_log(LOG_ERR, color, "ERROR checking for data\n");
			logc_reset_read(inf->lcinf);
			usleep(100000);
			break;
		}
	}

out_close:
	logc_close(inf->lcinf);
	close(inf->uininf->infd);

out_free:
	if (inf->lcinf)
		free(inf->lcinf);
	if (inf->uininf)
		free(inf->uininf);
	free(inf);

out:
	if (orig_tp_saved)
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_tp);
#ifdef HAVE_LIBDAEMON
	if (daemon_mode)
		daemon_pid_file_remove();
#endif
	if (use_syslog)
		closelog();

	return ret;
}
