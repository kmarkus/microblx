#ifndef GPIO_CONFIG_H
#define GPIO_CONFIG_H

enum ubx_gpio_dir {
	UBX_GPIO_DIR_IN  = 0,
	UBX_GPIO_DIR_OUT = 1,
};

enum ubx_gpio_pull {
	UBX_GPIO_PULL_DISABLED = 0,
	UBX_GPIO_PULL_UP       = 1,
	UBX_GPIO_PULL_DOWN     = 2,
};

struct ubx_gpio_config {
	char name[64];		/* GPIO line name as reported by the kernel */
	enum ubx_gpio_dir dir;	/* UBX_GPIO_DIR_IN or UBX_GPIO_DIR_OUT */
	int active_low;		/* 0: active-high (default), 1: active-low */
	enum ubx_gpio_pull pull;/* pull resistor setting */
	int emit_on_change;	/* for dir=in: 0=emit every step (default), 1=emit only on change */
};

#endif /* GPIO_CONFIG_H */
