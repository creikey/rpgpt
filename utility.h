#pragma once

#include <stdio.h>


#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x

#ifdef WEB
#define assert_impl(cond, str) ((cond) || (EM_ASM({ assert(0, UTF8ToString($0) + UTF8ToString($1)); }, (__func__), (str)), 0))
#elif defined(DESKTOP)
#define assert_impl(cond, str) ((cond) || (fputs("Assertion failed: " __FUNCTION__ str "\n", stderr), __debugbreak(), 0))
#else
#error "Don't know how to assert for current platform configuration"
#endif

#ifdef NDEBUG
#define game_assert(cond) ((void)0)
#else
#define game_assert(cond) assert_impl(cond, "(" __FILE__ ":" STRINGIZE(__LINE__) "): \"" #cond "\"")
#endif

#ifdef assert
#undef assert
#endif
#define assert game_assert

#define Log(...) { printf("%s Log %d | ", __FILE__, __LINE__); printf(__VA_ARGS__); }

