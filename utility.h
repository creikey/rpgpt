#pragma once

#include <stdio.h>
#include <stdbool.h>

#ifdef WEB
#include <signal.h>
#define game_debugbreak() raise(SIGTRAP)

#define game_assert_4127_push
#define game_assert_4127_pop

#elif defined(DESKTOP)
#define game_debugbreak() __debugbreak()

#define game_assert_4127_push __pragma(warning(push)) __pragma(warning(disable:4127))
#define game_assert_4127_pop  __pragma(warning(pop))

#else
#error "Don't know how to assert for current platform configuration"
#define game_debugbreak() (void)(0)
#endif

static void assert_impl(const char *func, const char *file, long line, const char *expression)
{
	fprintf(stderr, "Assert fail in %s(%s:%ld):\n    \"%s\"\n", func, file, line, expression);
}

#ifdef NDEBUG
#define game_assert(cond) game_assert_4127_push do { (void)0; } while (0) game_assert_4127_pop
#else
#define game_assert(cond) game_assert_4127_push do { \
	if (!(cond)) { \
		assert_impl(__func__, __FILE__, __LINE__, #cond); \
		game_debugbreak(); \
	} \
} while (0) game_assert_4127_pop
#endif


#ifdef assert
#undef assert
#endif
#define assert game_assert

#define Log(...) { printf("%s Log %d | ", __FILE__, __LINE__); printf(__VA_ARGS__); }
