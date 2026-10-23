#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>

#define TEST_ASSERT(expr)                                          \
	do {                                                           \
		if(!(expr)) {                                              \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			exit(1);                                               \
		} else {                                                   \
			printf("PASS: %s\n", #expr);                           \
		}                                                          \
	} while(0)

#define RUN_TEST(fn)                    \
	do {                                \
		printf("Running %s...\n", #fn); \
		fn();                           \
	} while(0)

#endif

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>

#define TEST_ASSERT(expr)                                          \
	do {                                                           \
		if(!(expr)) {                                              \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			exit(1);                                               \
		} else {                                                   \
			printf("PASS: %s\n", #expr);                           \
		}                                                          \
	} while(0)

#define RUN_TEST(fn)                    \
	do {                                \
		printf("Running %s...\n", #fn); \
		fn();                           \
	} while(0)

#endif
