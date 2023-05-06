#include <stdio.h>

#ifdef WEB
#include <assert.h>
#endif

static inline void assert_impl(bool cond, const char *expression)
{
	if(!cond)
	{
		fprintf(stderr, "Assertion failed: %s\n", expression);
#ifdef WEB
		assert(false);
#elif defined(DESKTOP)
		__debugbreak();
#else
#error "Don't know how to assert for current platform configuration"
#endif
	}
}
#ifdef assert
#undef assert
#endif

#define assert(cond) assert_impl(cond, #cond)
