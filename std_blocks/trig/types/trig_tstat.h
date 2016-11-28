struct trig_tstat
{
	int block;
	struct ubx_timespec dur;
	struct ubx_timespec min;
	struct ubx_timespec max;
	struct ubx_timespec total;
	unsigned long cnt;
};

