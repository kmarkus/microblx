#ifndef IIO_CONFIG_H
#define IIO_CONFIG_H

struct ubx_iio_channel_config {
	char   device[64];         /* device: name attr (e.g. "48300000.adc") or "iio:deviceN" */
	char   channel[64];        /* channel ID (e.g. "voltage0", "accel_x", "temp") */
	int    direction;          /* 0=device-input → ubx out-port (default), 1=device-output → ubx in-port */
	double sampling_frequency; /* Hz; 0=disabled (use driver default) */
	double scale;              /* 0=use sysfs (default); nonzero overrides sysfs scale+offset */
	double offset;             /* used only when scale override is active (scale != 0) */
};

#endif /* IIO_CONFIG_H */
