/*
 * pack_shm.h - create, join and open ubx/pack segments
 *
 * Plain C on top of lfb, without ubx: used by ubx/pack and ubx/unpack,
 * and by any other client, which passes the full segment name, such
 * as "mynode.pk". Writers must use this, as it keeps a second one out.
 * Writing and reading frames is plain lfb on ubx_pack_shm.shm, and takes
 * no syscalls.
 *
 * Everybody who joins pins the segment, with a shared lock on it that
 * the kernel releases however they end:
 *
 *  - a segment nobody pins is a leftover: it is replaced
 *  - a segment somebody pins must match, else joining fails
 *  - the last one to leave removes it
 *
 * The first to join creates it. shm_unlink only happens with the pin
 * held exclusively, on the object the name refers to. A writer also
 * holds the writer lock, so there is one at most. Opening a segment
 * only to read it doesn't pin it: such a reader doesn't keep a new
 * layout out, and finds the segment replaced at lfb_shm_stale.
 *
 * Define _GNU_SOURCE and _FILE_OFFSET_BITS=64 before any system header,
 * for the OFD locks. All functions return 0 or a negative errno.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef UBX_PACK_SHM_H
#define UBX_PACK_SHM_H

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "lfb.h"
#include "lfb_shm.h"
#include "pack_layout.h"

#ifndef F_OFD_SETLK
#error "pack_shm.h needs _GNU_SOURCE defined before any system header"
#endif

_Static_assert(sizeof(off_t) == 8, "pack_shm.h needs _FILE_OFFSET_BITS=64");

#define UBX_PACK_FRAME_ALIGN	8
#define UBX_PACK_DEPTH_DEFAULT	2	/* buffer_len 1 */

/* the two lock bytes, and how often to retry when in the way of others */
#define UBX_PACK_LOCK_WRITER	0
#define UBX_PACK_LOCK_PIN	1
#define UBX_PACK_RETRIES	20000
#define UBX_PACK_RETRY_US	50

struct ubx_pack_shm {
	lfb_shm_t shm;
	int fd;			/* holds the locks, -1 if not pinned */
};

static inline uint64_t ubx_pack_align(uint64_t x)
{
	return (x + UBX_PACK_FRAME_ALIGN - 1) &
		~((uint64_t)UBX_PACK_FRAME_ALIGN - 1);
}

/**
 * @brief add a port to a layout, and compute its place in the frame
 *
 * Start with ubx_pack_layout_size(nports) zeroed bytes, with version and
 * nports set, and *off = UBX_PACK_FRAME_START(nports).
 *
 * @param lay the layout
 * @param i index of the port
 * @param id port name, shorter than UBX_PACK_ID_LEN
 * @param type type name, shorter than UBX_PACK_TYPE_LEN
 * @param len array length
 * @param size bytes of the type
 * @param off in: end of the previous port, out: end of this one
 */
static inline void ubx_pack_layout_add(struct ubx_pack_layout *lay, uint32_t i,
				       const char *id, const char *type,
				       uint32_t len, uint64_t size, uint64_t *off)
{
	struct ubx_pack_layout_port *lp = &lay->ports[i];

	*off = ubx_pack_align(*off);
	memcpy(lp->id, id, strnlen(id, sizeof(lp->id) - 1));
	memcpy(lp->type, type, strnlen(type, sizeof(lp->type) - 1));
	lp->len = len;
	lp->offset = (uint32_t)*off;
	lp->size = (uint32_t)(size * len);
	*off += size * len;
}

/* the frame size of a complete layout */
static inline uint32_t ubx_pack_frame_size(const struct ubx_pack_layout *lay)
{
	const struct ubx_pack_layout_port *lp = &lay->ports[lay->nports - 1];

	return (uint32_t)ubx_pack_align((uint64_t)lp->offset + lp->size);
}

/**
 * @brief the layout of a segment
 *
 * @param s an open segment
 * @return the layout, NULL if it holds no valid one
 */
static inline const struct ubx_pack_layout *ubx_pack_layout_get(const lfb_shm_t *s)
{
	const lfb_t *b = lfb_shm_lfb(s);
	const struct ubx_pack_layout *lay = lfb_cuser(b);

	if (lay == NULL || b->user_sz < sizeof(*lay) ||
	    lay->version != UBX_PACK_LAYOUT_VERSION || lay->nports == 0 ||
	    b->user_sz != ubx_pack_layout_size(lay->nports))
		return NULL;

	return lay;
}

/**
 * @brief does a segment have this layout?
 *
 * @param s an open segment
 * @param lay the layout
 * @return non-zero if identical
 */
static inline int ubx_pack_layout_same(const lfb_shm_t *s,
				       const struct ubx_pack_layout *lay)
{
	const struct ubx_pack_layout *cur = ubx_pack_layout_get(s);

	return cur != NULL && cur->nports == lay->nports &&
		memcmp(cur, lay, ubx_pack_layout_size(lay->nports)) == 0;
}

/* internal */
static inline int ubx_pack__lock(int fd, short type, off_t byte)
{
	struct flock fl = { .l_type = type, .l_whence = SEEK_SET,
			    .l_start = byte, .l_len = 1 };

	return (fcntl(fd, F_OFD_SETLK, &fl) != 0) ? -errno : 0;
}

/* internal: does name still refer to what fd is open on? */
static inline int ubx_pack__current(int fd, const char *name)
{
	struct stat a, b;
	int cur = shm_open(name, O_RDONLY, 0);

	if (cur < 0)
		return 0;

	if (fstat(cur, &b) != 0 || fstat(fd, &a) != 0)
		a.st_ino = b.st_ino + 1;

	close(cur);
	return a.st_ino == b.st_ino && a.st_dev == b.st_dev;
}

/* internal: map fd into the handle */
static inline int ubx_pack__map(lfb_shm_t *s, int fd, const char *name, int prot)
{
	struct stat st;

	if (fstat(fd, &st) != 0)
		return -errno;

	s->map = mmap(NULL, (size_t)st.st_size, prot, MAP_SHARED, fd, 0);

	if (s->map == MAP_FAILED) {
		s->map = NULL;
		return -errno;
	}

	s->b = (lfb_t *)s->map;
	s->mapsz = (size_t)st.st_size;
	s->dev = st.st_dev;
	s->ino = st.st_ino;
	strcpy(s->name, name);
	return 0;
}

/*
 * internal: may this be replaced? Names reach any shm object, so it must
 * be one of ours, or never have been published by a creator that died.
 */
static inline int ubx_pack__replaceable(int fd, const char *name)
{
	int ok, prot = PROT_READ;
	lfb_shm_t s = { 0 };
	struct stat st;

	if (fstat(fd, &st) != 0)
		return 0;

	if ((size_t)st.st_size < LFB_HDR_SZ)
		return 1;

	if (ubx_pack__map(&s, fd, name, prot) != 0)
		return 0;

	ok = atomic_load_explicit(&s.b->magic, memory_order_acquire) == 0 ||
		(lfb_attach(s.map, s.mapsz) != NULL &&
		 ubx_pack_layout_get(&s) != NULL);

	munmap(s.map, s.mapsz);
	return ok;
}

/* internal: first in, with the pin held exclusively */
static inline int ubx_pack__create(struct ubx_pack_shm *p, int fd, const char *name,
				   const struct ubx_pack_layout *lay,
				   lfb_word_t depth, int writer)
{
	int ret;
	lfb_t *b;
	uint32_t user_sz = ubx_pack_layout_size(lay->nports);
	uint32_t frame_size = ubx_pack_frame_size(lay);
	size_t sz = lfb_memsz(frame_size, depth, user_sz);

	if (sz == 0)
		return -EINVAL;

	if (ftruncate(fd, (off_t)sz) != 0)
		return -errno;

	ret = ubx_pack__map(&p->shm, fd, name, PROT_READ | PROT_WRITE);

	if (ret != 0)
		return ret;

	b = lfb_prepare(p->shm.map, sz, frame_size, depth, user_sz);

	if (b == NULL) {
		lfb_shm_close(&p->shm);
		return -EINVAL;
	}

	memcpy(lfb_user(b), lay, user_sz);

	/* allocate the pages now, and not by faulting in the first steps.
	 * This also turns a full /dev/shm into a SIGBUS here, not in step */
	memset(lfb_data(b), 0, (size_t)frame_size * depth);
	lfb_publish(b);

	if (writer) {
		ret = ubx_pack__lock(fd, F_WRLCK, UBX_PACK_LOCK_WRITER);

		if (ret != 0) {
			lfb_shm_close(&p->shm);
			return ret;
		}
	} else {
		(void)mprotect(p->shm.map, p->shm.mapsz, PROT_READ);
	}

	ret = ubx_pack__lock(fd, F_RDLCK, UBX_PACK_LOCK_PIN);

	if (ret != 0) {
		lfb_shm_close(&p->shm);
		return ret;
	}

	p->fd = fd;
	return 0;
}

/* internal: somebody else is in, with the pin held shared */
static inline int ubx_pack__attach(struct ubx_pack_shm *p, int fd, const char *name,
				   const struct ubx_pack_layout *lay,
				   lfb_word_t depth, int writer)
{
	int ret;
	long page = sysconf(_SC_PAGESIZE);

	ret = ubx_pack__map(&p->shm, fd, name,
			    writer ? PROT_READ | PROT_WRITE : PROT_READ);

	if (ret != 0)
		return ret;

	if (lfb_attach(p->shm.map, p->shm.mapsz) == NULL ||
	    !ubx_pack_layout_same(&p->shm, lay) ||
	    p->shm.b->frame_size != ubx_pack_frame_size(lay)) {
		ret = -EPROTO;
	} else if (depth != 0 && p->shm.b->depth != depth) {
		ret = -ERANGE;
	} else if (writer) {
		ret = ubx_pack__lock(fd, F_WRLCK, UBX_PACK_LOCK_WRITER);

		if (ret == -EAGAIN || ret == -EACCES)
			ret = -EBUSY;
	}

	if (ret != 0) {
		lfb_shm_close(&p->shm);
		return ret;
	}

	/* no first faults in step */
	for (size_t off = 0; off < p->shm.mapsz; off += (size_t)page)
		(void)*(volatile const uint8_t *)((const uint8_t *)p->shm.map + off);

	p->fd = fd;
	return 0;
}

/**
 * @brief join a segment, creating it if needed, and pin it
 *
 * @param p handle
 * @param name segment name
 * @param lay the layout, see ubx_pack_layout_add
 * @param depth number of frames: if not 0, a segment with another one
 *        is an error. 0 takes any, and creates UBX_PACK_DEPTH_DEFAULT.
 * @param writer non-zero to become its writer
 * @return 0, -EBUSY (has a writer), -EPROTO (pinned, with another
 *         layout), -ERANGE (pinned, with another depth), -EEXIST (no
 *         ubx/pack segment), or the errno of a system call
 */
static inline int ubx_pack_join(struct ubx_pack_shm *p, const char *name,
				const struct ubx_pack_layout *lay,
				lfb_word_t depth, int writer)
{
	int fd, ret;

	p->fd = -1;
	memset(&p->shm, 0, sizeof(p->shm));

	if (strlen(name) >= sizeof(p->shm.name))
		return -ENAMETOOLONG;

	for (int i = 0; i < UBX_PACK_RETRIES; i++) {
		fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, LFB_SHM_MODE);

		if (fd >= 0) {
			if (ubx_pack__lock(fd, F_WRLCK, UBX_PACK_LOCK_PIN) != 0 ||
			    !ubx_pack__current(fd, name)) {
				close(fd);
				continue;
			}

			ret = ubx_pack__create(p, fd, name, lay,
					       depth ? depth : UBX_PACK_DEPTH_DEFAULT,
					       writer);

			if (ret != 0) {
				shm_unlink(name);
				close(fd);
			}

			return ret;
		}

		if (errno != EEXIST)
			return -errno;

		fd = shm_open(name, O_RDWR, 0);

		/* of another user: reading is enough for a reader */
		if (fd < 0 && errno == EACCES && !writer)
			fd = shm_open(name, O_RDONLY, 0);

		if (fd < 0) {
			if (errno == ENOENT)
				continue;

			return -errno;
		}

		ret = ubx_pack__lock(fd, F_WRLCK, UBX_PACK_LOCK_PIN);

		if (ret == 0) {
			/* nobody pins it */
			if (ubx_pack__current(fd, name)) {
				if (!ubx_pack__replaceable(fd, name)) {
					close(fd);
					return -EEXIST;
				}

				if (shm_unlink(name) != 0) {
					ret = -errno;
					close(fd);
					return ret;
				}
			}

			close(fd);
			continue;
		}

		if (ubx_pack__lock(fd, F_RDLCK, UBX_PACK_LOCK_PIN) != 0 ||
		    !ubx_pack__current(fd, name)) {
			/* a creator or a leaver: wait for it */
			close(fd);
			usleep(UBX_PACK_RETRY_US);
			continue;
		}

		ret = ubx_pack__attach(p, fd, name, lay, depth, writer);

		if (ret != 0)
			close(fd);

		return ret;
	}

	return -ETIMEDOUT;
}

/**
 * @brief open a segment to read it, of whatever layout it has
 *
 * @param p handle
 * @param name segment name
 * @return 0, -ENOENT, -EAGAIN (being created), or -EPROTO (no layout)
 */
static inline int ubx_pack_open(struct ubx_pack_shm *p, const char *name)
{
	int ret = lfb_shm_open(&p->shm, name);

	p->fd = -1;

	if (ret != 0)
		return ret;

	if (ubx_pack_layout_get(&p->shm) == NULL) {
		lfb_shm_close(&p->shm);
		return -EPROTO;
	}

	return 0;
}

/**
 * @brief let go of a segment, and remove it if we are the last
 *
 * @param p handle, from a successful join or open
 */
static inline void ubx_pack_close(struct ubx_pack_shm *p)
{
	if (p->fd >= 0 &&
	    ubx_pack__lock(p->fd, F_WRLCK, UBX_PACK_LOCK_PIN) == 0 &&
	    ubx_pack__current(p->fd, p->shm.name))
		shm_unlink(p->shm.name);

	lfb_shm_close(&p->shm);

	if (p->fd >= 0)
		close(p->fd);

	p->fd = -1;
}

#endif /* UBX_PACK_SHM_H */
