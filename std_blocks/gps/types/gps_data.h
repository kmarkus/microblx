#ifndef GPS_DATA_H
#define GPS_DATA_H

#include <stdint.h>

/* GPS fix data output by ubx/gps.
 * Double fields follow gpsd convention: NaN when the value is not available.
 * Check 'mode' before trusting positional fields (≥ 2 = lat/lon valid, 3 = altitude valid). */
struct ubx_gps_data {
	int64_t time;              /* UTC, nanoseconds since Unix epoch */
	double latitude;           /* degrees north, WGS84 */
	double longitude;          /* degrees east, WGS84 */
	double altMSL;             /* altitude above MSL, meters */
	double altHAE;             /* altitude above ellipsoid, meters */
	double speed;              /* speed over ground, m/s */
	double track;              /* course, degrees true north */
	double climb;              /* vertical speed, m/s */
	double eph;                /* horizontal position uncertainty, m */
	double epv;                /* vertical position uncertainty, m */
	int    mode;               /* 0=not seen, 1=no fix, 2=2D, 3=3D */
	int    status;             /* 0=unknown, 1=GPS, 2=DGPS, 3=RTK fixed, 4=RTK float */
	int    satellites_used;    /* satellites used in solution */
	int    satellites_visible; /* satellites visible */
};

#endif /* GPS_DATA_H */
