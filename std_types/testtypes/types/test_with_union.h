#ifndef _TEST_WITH_UNION_H
#define _TEST_WITH_UNION_H

/* named union */
union test_variant { int i; float f; };

/* struct with named union */
struct test_with_union {
	union test_variant v;
	unsigned char tag;
};

#endif /* _TEST_WITH_UNION_H */
