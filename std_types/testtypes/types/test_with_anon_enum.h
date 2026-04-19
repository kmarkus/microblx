#ifndef _TEST_WITH_ANON_ENUM_H
#define _TEST_WITH_ANON_ENUM_H

/* struct with anonymous enum field (constants scoped to the enclosing type) */
struct test_with_anon_enum {
	enum { KIND_INT=0, KIND_FLOAT=1 } kind;
	int value;
};

#endif /* _TEST_WITH_ANON_ENUM_H */
