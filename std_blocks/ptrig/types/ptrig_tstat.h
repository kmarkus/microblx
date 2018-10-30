struct ptrig_tstat
{
	char block_name[BLOCK_NAME_MAXLEN+1];
	struct ubx_timespec min;
	struct ubx_timespec max;
	struct ubx_timespec total;
	unsigned long cnt;
};

