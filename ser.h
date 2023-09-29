#pragma once

#include "md.h"

typedef struct
{
    b8 failed;
    String8 why;
} SerError;

typedef struct
{
    u8 *data; // set to 0 to dry run and get maximum size. max doesn't matter in this case
    u64 cur;
    u64 max;

    Arena *arena; // allocate everything new on this, so that if serialization fails allocations can be undone
    // Serializing should never allocate. So this can be null when you serialize

    int version;
    SerError cur_error;
    Arena *error_arena; // all error messages are allocated here

    b8 serializing;
} SerState;

void ser_bytes(SerState *ser, u8 *bytes, u64 bytes_size)
{
    if (!ser->data && !ser->serializing)
    {
        ser->cur_error = (SerError){.failed = true, .why = S8Lit("Deserializing but the data is null")};
    }

    if (!ser->cur_error.failed)
    {
        if (ser->data)
        {
            // maximum doesn't matter unless writing to data
            if (ser->cur + bytes_size > ser->max)
            {
                ser->cur_error = (SerError){.failed = true, .why = S8Lit("Too big bro")};
            }
            else
            {
                if (ser->serializing)
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

#define SER_MAKE_FOR_TYPE(type)                    \
    void ser_##type(SerState *ser, type *into)     \
    {                                              \
        ser_bytes(ser, (u8 *)into, sizeof(*into)); \
    }

SER_MAKE_FOR_TYPE(int);
SER_MAKE_FOR_TYPE(u64);

// Embeds the string into the serialized binary. When deserializing copies the
// deserialized data onto a newly allocated buffer
void ser_String8(SerState *ser, String8 *s, Arena *allocate_onto)
{
    ser_u64(ser, &s->size);
    if (ser->serializing)
    {
        ser_bytes(ser, s->str, s->size);
    }
    else
    {
        s->str = ArenaPush(allocate_onto, s->size);
        ser_bytes(ser, s->str, s->size);
    }
}
