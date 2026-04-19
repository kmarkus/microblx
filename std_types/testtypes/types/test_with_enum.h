#ifndef _TEST_WITH_ENUM_H
#define _TEST_WITH_ENUM_H

/* named enum */
enum test_color { RED=0, GREEN=1, BLUE=2 };

/* struct with named enum field */
struct test_with_enum {
	enum test_color col;
	int val;
};

#endif /* _TEST_WITH_ENUM_H */
