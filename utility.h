#pragma once

#include <stdio.h>
#include <stdbool.h>

#ifdef WEB
//#include <assert.h>
#include <signal.h>
#endif

static void assert_impl(bool cond, const char *expression)
{
	if(!cond)
	{
		fprintf(stderr, "Assertion failed: %s\n", expression);
#ifdef WEB
		raise(SIGTRAP);
		//assert(false);
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

#define Log(...) { printf("%s Log %d | ", __FILE__, __LINE__); printf(__VA_ARGS__); }