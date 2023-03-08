#ifdef PROFILING_H
#error only include profiling.h once
#endif
#define PROFILING_H

#include <stdlib.h> // malloc the profiling buffer

#define DeferLoop(start, end) for (int _i_ = ((start), 0); _i_ == 0; _i_ += 1, (end))

#ifdef PROFILING
#define THREADLOCAL __declspec(thread)
#define PROFILING_BUFFER_SIZE (1 * 1024 * 1024)

#ifdef PROFILING_IMPL
#define SPALL_IMPLEMENTATION
#pragma warning(disable : 4996) // spall uses fopen
#include "spall.h"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <Windows.h>
// This is slow, if you can use RDTSC and set the multiplier in SpallInit, you'll have far better timing accuracy
double get_time_in_micros()
{
  static double invfreq;
  if (!invfreq)
  {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    invfreq = 1000000.0 / frequency.QuadPart;
  }
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return counter.QuadPart * invfreq;
}
SpallProfile spall_ctx;
bool profiling = false;
THREADLOCAL SpallBuffer spall_buffer;
THREADLOCAL unsigned char *buffer_data = NULL;
THREADLOCAL uint32_t my_thread_id = 0;

void init_profiling(const char *filename)
{
  spall_ctx = spall_init_file(filename, 1);
}

void init_profiling_mythread(uint32_t id)
{
  profiling = true;
  my_thread_id = id;
  if (buffer_data != NULL)
  {
    __debugbreak();
  }
  buffer_data = malloc(PROFILING_BUFFER_SIZE);
  spall_buffer = (SpallBuffer){
      .length = PROFILING_BUFFER_SIZE,
      .data = buffer_data,
  };
  spall_buffer_init(&spall_ctx, &spall_buffer);
}

void end_profiling_mythread()
{
  profiling = false;
  spall_buffer_quit(&spall_ctx, &spall_buffer);
  free(buffer_data);
  buffer_data = NULL;
}

void end_profiling()
{
  spall_quit(&spall_ctx);
}

#endif // PROFILING_IMPL

#include "spall.h"

extern SpallProfile spall_ctx;
extern THREADLOCAL SpallBuffer spall_buffer;
extern THREADLOCAL uint32_t my_thread_id;

double get_time_in_micros();
void init_profiling(const char *filename);
// you can pass anything to id as long as it's different from other threads
void init_profiling_mythread(uint32_t id);
void end_profiling();
void end_profiling_mythread();

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define PROFILE_SCOPE(name) DeferLoop(profiling ? spall_buffer_begin(&spall_ctx, &spall_buffer, "L" STRINGIZE(__LINE__) " " name , sizeof("L" STRINGIZE(__LINE__) " " name) - 1, get_time_in_micros()) : (void)0, profiling ? spall_buffer_end(&spall_ctx, &spall_buffer, get_time_in_micros()) : (void)0)

#else // PROFILING

void init_profiling(const char *filename) { (void)filename; }
// you can pass anything to id as long as it's different from other threads
void init_profiling_mythread(uint32_t id) { (void)id; }
void end_profiling() {}
void end_profiling_mythread() {}

#define PROFILE_SCOPE(name) for(int _i_ = 0; _i_ == 0; _i_ += 1)
#endif
