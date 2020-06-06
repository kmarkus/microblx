#ifndef TSTAT_H
#define TSTAT_H

struct ubx_tstat
{
	char block_name[UBX_BLOCK_NAME_MAXLEN + 1];
	struct ubx_timespec min;
	struct ubx_timespec max;
	struct ubx_timespec total;
	unsigned long cnt;
};

#endif /* TSTAT_H */
