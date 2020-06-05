#include <ubx/ubx.h>

#ifndef THRES_EVENT_H
#define THRES_EVENT_H

/**
 * struct thres_event - event
 * @dir: direction of threshold crossing
 *		1: rising
 *		0: falling
 * @ts: timestamp of event
 */
struct thres_event {
	int dir;
	struct ubx_timespec ts;
};

#endif
