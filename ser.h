#pragma once

#include "md.h"

typedef struct
{
 MD_b8 failed;
 MD_String8 why;
} SerError;

typedef struct
{
 MD_u8 *data; // set to 0 to dry run and get maximum size. max doesn't matter in this case
 MD_u64 cur;
 MD_u64 max;

 MD_Arena *arena; // allocate everything new on this, so that if serialization fails allocations can be undone
 // Serializing should never allocate. So this can be null when you serialize

 int version;
 SerError cur_error;
 MD_Arena *error_arena; // all error messages are allocated here

 MD_b8 serializing;
} SerState;

void ser_bytes(SerState *ser, MD_u8 *bytes, MD_u64 bytes_size)
{
 if(!ser->cur_error.failed)
 {
  if(ser->data)
  {
   // maximum doesn't matter unless writing to data
   if(ser->cur + bytes_size > ser->max)
   {
    ser->cur_error = (SerError){.failed = true, .why = MD_S8Lit("Too big bro")};
   }
   else
   {
    if(ser->serializing)
    {
     memcpy(ser->data + ser->cur, bytes, bytes_size);
    }
    else
    {
     memcpy(bytes, ser->data + ser->cur, bytes_size);
    }
   }

  }

  ser->cur += bytes_size;
 }
}

#define SER_MAKE_FOR_TYPE(type) void ser_##type(SerState *ser, type *into) \
{ \
 ser_bytes(ser, (MD_u8*)into, sizeof(*into)); \
}

SER_MAKE_FOR_TYPE(int);
