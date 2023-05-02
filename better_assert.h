#include <stdio.h>

static inline void assert_impl(bool cond, const char *expression)
{
	if(!cond)
	{
		fprintf(stderr, "Assertion failed: %s\n", expression);
		__debugbreak();
	}
}
#ifdef assert
#undef assert
#endif

#define assert(cond) assert_impl(cond, #cond)
