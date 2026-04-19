#ifndef _TEST_WITH_ANON_UNION_H
#define _TEST_WITH_ANON_UNION_H

/* struct with anonymous union: sub-fields promoted to struct namespace */
struct test_with_anon_union {
	union {
		int   i;
		float f;
	};
	unsigned char selector;
};

#endif /* _TEST_WITH_ANON_UNION_H */
