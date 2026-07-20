/*
 * lfb_shm.h - robust POSIX shared memory lifecycle layer for lfb
 *
 * Copyright (C) 2026 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

/*
 * This optional layer places an lfb buffer in a POSIX shm segment
 * and makes its lifecycle robust against producer restarts, crashes
 * and geometry changes:
 *
 *  - a segment is never reused or resized in place: lfb_shm_create
 *    unlinks any leftover (e.g. from a crashed producer) and creates
 *    a fresh one under O_EXCL, so there is no inherited state. A
 *    restart with different frame_size/depth is therefore trivially
 *    supported.
 *
 *  - consumers hold their own read-only mapping. If the producer
 *    recreates the segment, an existing mapping stays valid (the
 *    kernel keeps unlinked pages alive) but no longer receives new
 *    data. lfb_shm_stale detects this cheaply via the segment's
 *    inode; the consumer then closes and re-opens.
 *
 *  - a consumer never maps a half-initialized segment: lfb_shm_open
 *    distinguishes "no producer yet" (-ENOENT), "producer mid-create"
 *    (-EAGAIN, retry) and "foreign or mismatching buffer" (-EPROTO,
 *    see lfb_attach).
 *
 * How consumers learn *when* to (re)open is left to the application:
 * poll on -ENOENT/-EAGAIN/stale, or use inotify on /dev/shm for
 * event-driven wakeup.
 *
 * Besides the default single-producer mode (lfb_shm_create), a
 * second lifecycle mode supports *cooperating co-producers*
 * serialized by an external lock: lfb_shm_join creates the segment
 * if absent (leaving it unpublished so the creator can initialize
 * the user area, e.g. with that shared lock) or attaches read-write
 * to an existing matching one. Note that the never-reuse robustness
 * guarantee belongs to lfb_shm_create only; with join, crash
 * recovery of the write side is the external lock's job (e.g. a
 * robust mutex).
 *
 * All functions return 0 or a negative errno, in keeping with
 * lfb.h. This header requires POSIX (shm_open, mmap); link with -lrt
 * on older glibc.
 */

#ifndef _LFB_SHM_H_
#define _LFB_SHM_H_

#include "lfb.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef LFB_SHM_NAME_MAX
#define LFB_SHM_NAME_MAX 128
#endif

#ifndef LFB_SHM_MODE
#define LFB_SHM_MODE 0644
#endif

/**
 * lfb shm segment handle
 *
 * One per producer (lfb_shm_create) or consumer (lfb_shm_open)
 * mapping. All fields are private to the library; access the buffer
 * via lfb_shm_lfb.
 */
typedef struct lfb_shm {
	lfb_t *b;
	void *map;
	size_t mapsz;
	dev_t dev;		/* segment identity for lfb_shm_stale */
	ino_t ino;
	char name[LFB_SHM_NAME_MAX];
} lfb_shm_t;

/**
 * @brief create a fresh shm segment holding an lfb buffer
 *
 * Producer-side. Unlinks any leftover segment of the same name (a
 * previous instance is never reused), creates a new one under
 * O_EXCL, sizes and maps it and initializes the buffer via lfb_init
 * (which publishes the header magic last, so concurrently opening
 * consumers never see a torn header).
 *
 * The buffer is published (attachable) on return, with a zeroed
 * user area; to initialize the user area before publication, use
 * lfb_shm_join (whose creator path returns unpublished) instead.
 *
 * @param s handle to initialize (caller-owned memory)
 * @param name shm segment name (as for shm_open, e.g. "mybuf";
 *        shorter than LFB_SHM_NAME_MAX)
 * @param frame_size size of one frame in bytes
 * @param depth number of frames (see lfb_memsz for limits)
 * @param user_sz size of the caller-owned user area (0 for none,
 *        see lfb_user)
 * @return 0 on success, negative errno on failure (-EINVAL on bad
 *         parameters, -ENAMETOOLONG, or the shm_open / ftruncate /
 *         mmap errno)
 */
static inline int lfb_shm_create(lfb_shm_t *s, const char *name,
				 uint32_t frame_size, lfb_word_t depth,
				 uint32_t user_sz)
{
	int fd, ret;
	void *map;
	struct stat st;
	size_t sz = lfb_memsz(frame_size, depth, user_sz);

	if (s == NULL || name == NULL || sz == 0)
		return -EINVAL;

	if (strlen(name) >= sizeof(s->name))
		return -ENAMETOOLONG;

	/* never reuse a leftover segment: a fresh instance starts empty */
	(void)shm_unlink(name);

	fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, LFB_SHM_MODE);

	if (fd == -1)
		return -errno;

	if (ftruncate(fd, (off_t)sz) != 0 || fstat(fd, &st) != 0) {
		ret = -errno;
		goto out_close_unlink;
	}

	map = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (map == MAP_FAILED) {
		ret = -errno;
		goto out_close_unlink;
	}

	close(fd);

	s->b = lfb_init(map, sz, frame_size, depth, user_sz);

	if (s->b == NULL) {
		munmap(map, sz);
		shm_unlink(name);
		return -EINVAL;
	}

	s->map = map;
	s->mapsz = sz;
	s->dev = st.st_dev;
	s->ino = st.st_ino;
	strcpy(s->name, name);
	return 0;

out_close_unlink:
	close(fd);
	shm_unlink(name);
	return ret;
}

/**
 * @brief create or attach to a segment as a cooperating co-producer
 *
 * For multiple writers (in one or more processes) sharing one
 * buffer, serialized by an external lock of the caller's (typically
 * a process-shared robust mutex placed in the user area). Unlike
 * lfb_shm_create, an existing matching segment is *joined*, not
 * replaced, so any number of parties can come and go.
 *
 * If no segment exists, it is created and prepared but NOT
 * published: the caller must initialize the user area and then call
 * lfb_shm_publish. Until then, other joiners and consumers get
 * -EAGAIN. If the segment exists, it is attached read-write after
 * validating that it is published and matches the requested
 * geometry exactly.
 *
 * On -EPROTO (mismatching geometry or foreign contents, e.g. after
 * a version upgrade), the caller decides the migration policy: if
 * it may discard the old buffer, shm_unlink(name) and join again.
 * Should the creating party die between join and lfb_shm_publish,
 * the segment stays unpublished and joiners see -EAGAIN forever;
 * callers should bound their retries and escalate the same way
 * (unlink and re-join).
 *
 * @param s handle to initialize (caller-owned memory)
 * @param name shm segment name
 * @param frame_size size of one frame in bytes
 * @param depth number of frames (see lfb_memsz for limits)
 * @param user_sz size of the caller-owned user area (0 for none)
 * @return 1 if the segment was created (initialize the user area,
 *         then lfb_shm_publish!), 0 if an existing segment was
 *         joined, negative errno on failure: -EAGAIN (exists but
 *         not yet published, retry), -EPROTO (incompatible), or the
 *         shm_open / ftruncate / mmap errno
 */
static inline int lfb_shm_join(lfb_shm_t *s, const char *name,
			       uint32_t frame_size, lfb_word_t depth,
			       uint32_t user_sz)
{
	int fd, ret, created = 1;
	void *map;
	struct stat st;
	lfb_t *b;
	uint32_t magic;
	size_t sz = lfb_memsz(frame_size, depth, user_sz);

	if (s == NULL || name == NULL || sz == 0)
		return -EINVAL;

	if (strlen(name) >= sizeof(s->name))
		return -ENAMETOOLONG;

	fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, LFB_SHM_MODE);

	if (fd == -1) {
		if (errno != EEXIST)
			return -errno;

		created = 0;
		fd = shm_open(name, O_RDWR, 0);

		/* raced a concurrent unlink: have the caller retry */
		if (fd == -1)
			return -errno;
	}

	if (created && ftruncate(fd, (off_t)sz) != 0) {
		ret = -errno;
		close(fd);
		shm_unlink(name);
		return ret;
	}

	if (fstat(fd, &st) != 0) {
		ret = -errno;
		close(fd);

		if (created)
			shm_unlink(name);

		return ret;
	}

	if (!created && (size_t)st.st_size < LFB_HDR_SZ) {
		/* creator mid-setup: not even header-sized yet */
		close(fd);
		return -EAGAIN;
	}

	map = mmap(NULL, (size_t)st.st_size, PROT_READ | PROT_WRITE,
		   MAP_SHARED, fd, 0);
	close(fd);

	if (map == MAP_FAILED) {
		ret = -errno;

		if (created)
			shm_unlink(name);

		return ret;
	}

	if (created) {
		b = lfb_prepare(map, sz, frame_size, depth, user_sz);

		if (b == NULL) {
			munmap(map, (size_t)st.st_size);
			shm_unlink(name);
			return -EINVAL;
		}
	} else {
		b = (lfb_t *)map;
		magic = atomic_load_explicit(&b->magic, memory_order_acquire);

		if (magic != lfb_fingerprint(frame_size, depth, user_sz)) {
			munmap(map, (size_t)st.st_size);
			/* magic 0: creator mid-setup, else a foreign
			 * segment or one of different geometry */
			return (magic == 0) ? -EAGAIN : -EPROTO;
		}

		if ((size_t)st.st_size < sz) {
			/* matching magic but truncated: corrupt */
			munmap(map, (size_t)st.st_size);
			return -EPROTO;
		}
	}

	s->b = b;
	s->map = map;
	s->mapsz = (size_t)st.st_size;
	s->dev = st.st_dev;
	s->ino = st.st_ino;
	strcpy(s->name, name);
	return created;
}

/**
 * @brief publish the buffer of a segment created via lfb_shm_join
 *
 * Call exactly once, after initializing the user area, when
 * lfb_shm_join returned 1. See lfb_publish.
 *
 * @param s the handle join created the segment under
 */
static inline void lfb_shm_publish(lfb_shm_t *s)
{
	lfb_publish(s->b);
}

/**
 * @brief open an existing lfb shm segment read-only
 *
 * Consumer-side. Maps the segment and validates the contained
 * buffer via lfb_attach. -ENOENT and -EAGAIN are benign "not ready
 * yet" conditions to retry (poll or wait for an inotify event);
 * -EPROTO means the segment holds no compatible lfb buffer (foreign
 * contents, or producer compiled with a different LFB_CTR_BITS /
 * LFB_OFF_BITS configuration).
 *
 * @param s handle to initialize (caller-owned memory)
 * @param name shm segment name (as passed to lfb_shm_create)
 * @return 0 on success, negative errno on failure: -ENOENT (no
 *         producer has created the segment), -EAGAIN (segment
 *         exists but is not yet initialized), -EPROTO (incompatible
 *         buffer), or the shm_open / fstat / mmap errno
 */
static inline int lfb_shm_open(lfb_shm_t *s, const char *name)
{
	int fd, ret;
	void *map;
	struct stat st;
	lfb_t *b;
	uint32_t magic;

	if (s == NULL || name == NULL)
		return -EINVAL;

	if (strlen(name) >= sizeof(s->name))
		return -ENAMETOOLONG;

	fd = shm_open(name, O_RDONLY, 0);

	if (fd == -1)
		return -errno;

	if (fstat(fd, &st) != 0) {
		ret = -errno;
		close(fd);
		return ret;
	}

	if ((size_t)st.st_size < LFB_HDR_SZ) {
		/* exists but not yet sized: producer mid-create */
		close(fd);
		return -EAGAIN;
	}

	map = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);

	if (map == MAP_FAILED)
		return -errno;

	b = lfb_attach(map, (size_t)st.st_size);

	if (b == NULL) {
		magic = atomic_load_explicit(&((lfb_t *)map)->magic,
					     memory_order_acquire);
		munmap(map, (size_t)st.st_size);
		/* header not yet published vs. foreign config/layout */
		return (magic == 0) ? -EAGAIN : -EPROTO;
	}

	s->b = b;
	s->map = map;
	s->mapsz = (size_t)st.st_size;
	s->dev = st.st_dev;
	s->ino = st.st_ino;
	strcpy(s->name, name);
	return 0;
}

/**
 * @brief check whether the mapped segment has been replaced
 *
 * Consumer-side. Compares the identity (inode) of the shm object
 * currently registered under the handle's name with the one mapped
 * at lfb_shm_open time. A mismatch (or absence) means the producer
 * has been restarted or is gone: the mapping remains safe to use
 * but receives no new data; close and re-open it.
 *
 * Cheap (one shm_open + fstat, no mapping), so it may be called
 * e.g. whenever a reader has seen no data for a while.
 *
 * @param s an opened handle
 * @return 1 if stale (segment gone or recreated), 0 if still current
 */
static inline int lfb_shm_stale(const lfb_shm_t *s)
{
	int fd, ret;
	struct stat st;

	if (s == NULL || s->map == NULL)
		return 1;

	fd = shm_open(s->name, O_RDONLY, 0);

	if (fd == -1)
		return 1;

	ret = fstat(fd, &st);
	close(fd);

	if (ret != 0)
		return 1;

	return (st.st_dev != s->dev) || (st.st_ino != s->ino);
}

/**
 * @brief unmap the segment without unlinking it
 *
 * Consumer-side counterpart of lfb_shm_destroy. Any lfb_rd_t
 * cursors into this mapping become invalid.
 *
 * @param s an opened handle (no-op if not open)
 */
static inline void lfb_shm_close(lfb_shm_t *s)
{
	if (s == NULL || s->map == NULL)
		return;

	munmap(s->map, s->mapsz);
	memset(s, 0, sizeof(*s));
}

/**
 * @brief unmap and unlink the segment
 *
 * Producer-side. Consumers holding a mapping keep it (kernel-side)
 * until they close; lfb_shm_stale reports the segment stale from
 * now on.
 *
 * @param s a created handle (no-op if not open)
 */
static inline void lfb_shm_destroy(lfb_shm_t *s)
{
	if (s == NULL || s->map == NULL)
		return;

	shm_unlink(s->name);
	lfb_shm_close(s);
}

/**
 * @brief the buffer contained in the mapped segment
 *
 * @param s an opened or created handle
 * @return the lfb buffer, or NULL if the handle is not open. Treat
 *         as const on consumer handles (the mapping is read-only)
 */
static inline lfb_t *lfb_shm_lfb(const lfb_shm_t *s)
{
	return s ? s->b : NULL;
}

#endif /* _LFB_SHM_H_ */
