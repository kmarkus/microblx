/*
 * rtlog_client: client library for reading log data from shared memory buffer
 *
 * Copyright (C) 2018-2020 Markus Klotzbuecher <mk@mkio.de>
 * Copyright (C) 2019 Hamish Guthrie <hamish.guthrie@kistler.com>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#undef DEBUG

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <limits.h>
#include <unistd.h>

#include "rtlog_client.h"

#ifdef DEBUG
# define DBG(fmt, args...) (fprintf(stderr, "%s: ", __func__), \
			    fprintf(stderr, fmt, ##args),	    \
			    fprintf(stderr, "\n"))
#else
# define DBG(fmt, args...)  do {} while (0)
#endif

/**
 * get_shm_file_size - get size of shm file
 * this also ensures it exists
 *
 * @param shm_file filename of shm file
 * @return 0 if successful, non-zero (errno) in case of failure.
 */
static int get_shm_file_size(const char *shm_file, int *sz)
{
	int ret = EINVAL;
	struct stat st;
	char shm_path[NAME_MAX];

	if (snprintf(shm_path,
		    NAME_MAX,
		    "/dev/shm/%s",
		    shm_file) < 0) {
		DBG("failed to create shm file path\n");
		ret = ENAMETOOLONG;
		goto out;
	}

	if (stat(shm_path, &st) == -1) {
		ret = errno;
		DBG("failed to stat file %s: %s\n",
		    shm_path,
		    strerror(errno));
		goto out;
	}

	*sz = st.st_size;
	ret = 0;
out:
	return ret;
}


/**
 * logc_seek_to_oldest - move the read ptr to the oldest valid log
 * message (keeping at least LOGC_SEEK_OLDEST_CRUSH_ZONE distance from the
 * wptr).
 *
 * @param inf pointer to logc_info_t
 */
void logc_seek_to_oldest(logc_info_t *inf)
{
	log_wrap_off_t new = inf->buf_ptr->w;

	/*
	 * advance by LOGC_SEEK_OLDEST_CRUSH_ZONE elem. Moving beyond
	 * wptr means we need to remember to decrement wrap.
	 */
	new.off += LOGC_SEEK_OLDEST_CRUSH_ZONE * inf->frame_size;

	/* wrap? */
	if (new.off > inf->shm_size - inf->frame_size - sizeof(log_buf_t)) {
		/*
		 * yes. No need to decrement wrap because we moved
		 * ahead of wptr, which already "decremented" it.
		 */
		new.off = new.off - inf->shm_size + sizeof(log_buf_t);
		DBG("wrapped");
	} else {
		/*
		 * no wrap occurred and buffer has never wrapped
		 * before. This means 0 is our oldest message (if its
		 * there at all).
		 */
		if (new.wrap == 0) {
			DBG("no wrap and wrap=0. starting at off=0");
			new.off = 0;
		} else {
			/*
			 * no wrap occurreed and buffer is full (has
			 * wrapped before), so we need to decrement
			 * wrap for moving beyond wptr
			 */
			DBG("no wrap and wrap=%u. decrementing wrap", new.wrap);
			new.wrap--;
		}
	}

	DBG("idx of oldest: %u", new.off / inf->frame_size);
	inf->r = new;
}

/**
 * logc_reset_read - reset the read ptr to the write ptr
 *
 * @param inf pointer to logc_info_t
 */
void logc_reset_read(logc_info_t *inf)
{
	inf->r = inf->buf_ptr->w;
}


/**
 * logc_init - open shm file and initalize client info
 *
 * @param inf local data
 * @param filename name of shm file created by aggregator block.
 * @param frame_size total frame size (from JSON meta-data)
 *
 * @return 0 if successfull, non-zero (errno) in case of failure.
 */
int logc_init(logc_info_t *inf,
	     const char *filename,
	     uint32_t frame_size)
{
	int ret;

	inf->frame_size = frame_size;

	ret = get_shm_file_size(filename, &inf->shm_size);

	if (ret != 0)
		goto out;

	inf->shm_fd = shm_open(filename, O_RDONLY, 0640);

	if (inf->shm_fd == -1) {
		ret = errno;
		DBG("shm_open failed: %s", strerror(errno));
		goto out;
	}

	inf->buf_ptr = mmap(0, inf->shm_size,
			    PROT_READ, MAP_SHARED,
			    inf->shm_fd, 0);
	if (inf->buf_ptr == MAP_FAILED) {
		ret = errno;
		DBG("mmap failed: %s", strerror(errno));
		goto out;
	}

	logc_reset_read(inf);

	DBG("inf->buf_ptr:          %p", inf->buf_ptr);
	DBG("inf->buf_ptr->data:    %p", inf->buf_ptr->data);
	DBG("inf->frame_size:       %u", inf->frame_size);
	DBG("inf->buf_ptr->w.off:   %u", inf->buf_ptr->w.off);
	DBG("inf->r.off:            %u", inf->r.off);

	/* all OK */
	ret = 0;
out:
	return ret;
}

/**
 * logc_close - close shm file
 *
 * @param inf local data
 */
void logc_close(logc_info_t *inf)
{
	munmap((void *) inf->buf_ptr, inf->shm_size);
	close(inf->shm_fd);
}

/**
 * calc_next_roff  - calculate the next read offset
 *
 * Handles wrapping. Does not consider the write state or whether
 * there is actually valid data at the new address.
 *
 * @param inf
 * @return new wrap_woff write state
 */
static log_wrap_off_t calc_next_roff(logc_info_t *inf)
{
	log_wrap_off_t new = inf->r;

	new.off = inf->r.off + inf->frame_size;

	/* check for wrapping */
	if (new.off > inf->shm_size - inf->frame_size - sizeof(log_buf_t)) {
		new.off = 0;
		new.wrap++;
	}

	return new;
}

/**
 * logc_has_data - is new data available?
 *
 * @param inf
 * @return READ_STATUS
 */
enum READ_STATUS logc_has_data(const logc_info_t *inf)
{
	int ret = NO_DATA;
	log_wrap_off_t w, r;	/* write and read wrap and offsets */

	/* atomic */
	w = inf->buf_ptr->w;
	r = inf->r;

	if (w.off == r.off && w.wrap == r.wrap) {
		ret = NO_DATA;
		goto out;
	} else if ((w.off > r.off && w.wrap == r.wrap) ||
		  (w.off < r.off && w.wrap - r.wrap == 1)) {
		ret = NEW_DATA;
		goto new_data;
	} else if (w.wrap - r.wrap >= 2 ||
		  (w.off >= r.off && w.wrap - r.wrap == 1)) {
		ret = OVERRUN;
		goto out;
	} else {
		ret = ERROR;
		goto out;
	}

new_data:
	/* all OK */
	ret = NEW_DATA;

out:
	return ret;
}


/**
 * logc_read_frame - read the next frame if available
 *
 * this is a consuming read, in that the readptr is advanced.
 *
 * @param inf
 * @param frame outvalue to store the read frame.
 * @return READ_STATUS
 */
enum READ_STATUS logc_read_frame(logc_info_t *inf, volatile log_frame_t **frame)
{
	int ret = NO_DATA;

	ret = logc_has_data(inf);

	/*
	 * if we have anything but NEW_DATA (NO_DATA, OVERRUN),
	 * return that there is nothing to read.
	 */
	if (ret != NEW_DATA)
		goto out;

	/* we have valid new data, return current and advance roff */
	*frame = (volatile log_frame_t *)&inf->buf_ptr->data[inf->r.off];
	inf->r = calc_next_roff(inf);
out:
	return ret;
}

/**
 * logc_dataptr_get - get a pointer to the frame payload.
 *
 * @param frame frame for which calculate the payload ptr
 * @param inf local data
 * @return pointer to the frame payload
 */
void *logc_dataptr_get(volatile log_frame_t *frame)
{
	return (void *)frame;
}

void logc_print_stat(const logc_info_t *inf)
{
	(void)(inf);

	DBG("w.wrap: %u, w.off: %u, r.wrap: %u, r.off: %u, rptr: %p",
	    inf->buf_ptr->w.wrap,
	    inf->buf_ptr->w.off,
	    inf->r.wrap,
	    inf->r.off,
	    &inf->buf_ptr->data[inf->r.off]);
}
