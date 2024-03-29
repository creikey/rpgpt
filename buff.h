#pragma once

#include <stddef.h>

#define ARRLEN(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
#define ARR_ITER(type, arr) for(type *it = &arr[0]; it < &arr[ARRLEN(arr)]; it++)
#define ARR_ITER_I(type, arr, i_var) ARR_ITER(type, arr) for(int i_var = (int)(it - &arr[0]); i_var != -1; i_var = -1)
#define SLICE_ITER(type, slice_ptr) for(type *it = &slice_ptr[0]; it < &(slice_ptr[slice_ptr ##_length]); it++)
#define GET_TABLE(table, index) (assert(index < ARRLEN(table) && index >= 0), table[index])
// you can't do &(some func, variable) to get the variable's address in C. Sad!
#define GET_TABLE_PTR(table, index) (assert(index < ARRLEN(table) && index >= 0), &table[index])

#define RANGE_ITER(from, to) for(int i = from; i < to; i++)

// null terminator always built into buffers so can read properly from data
#define BUFF_VALID(buff_ptr) assert((buff_ptr)->cur_index <= ARRLEN((buff_ptr)->data))
#define BUFF(type, max_size) struct { int cur_index; type data[max_size]; char null_terminator; }
#define BUFF_HAS_SPACE(buff_ptr) ( (buff_ptr)->cur_index < ARRLEN((buff_ptr)->data) )
#define BUFF_EMPTY(buff_ptr) ((buff_ptr)->cur_index == 0)
#define BUFF_APPEND(buff_ptr, element)  { (buff_ptr)->data[(buff_ptr)->cur_index++] = element; BUFF_VALID(buff_ptr); }
#define BUFF_ITER(type, buff_ptr) for(type *it = &((buff_ptr)->data[0]); it < &((buff_ptr)->data[(buff_ptr)->cur_index]); it++)
#define BUFF_ITER_I(type, buff_ptr, i_var) BUFF_ITER(type, buff_ptr) for(int i_var = (int)(it - &((buff_ptr)->data[0])); i_var != -1; i_var = -1)
#define BUFF_PUSH_FRONT(buff_ptr, value) { (buff_ptr)->cur_index++; BUFF_VALID(buff_ptr); for(int i = (buff_ptr)->cur_index - 1; i > 0; i--) { (buff_ptr)->data[i] = (buff_ptr)->data[i - 1]; }; (buff_ptr)->data[0] = value; }
#define BUFF_REMOVE_BACK(buff_ptr) {assert( (buff_ptr)->cur_index > 0); (buff_ptr)->cur_index--;}
#define BUFF_REMOVE_FRONT(buff_ptr) {if((buff_ptr)->cur_index > 0) {for(int i = 0; i < (buff_ptr)->cur_index - 1; i++) { (buff_ptr)->data[i] = (buff_ptr)->data[i+1]; }; (buff_ptr)->cur_index--;}}
#define BUFF_REMOVE_AT_INDEX(buff_ptr, index) { BUFF_VALID(buff_ptr); assert(index >= 0); assert(index < (buff_ptr)->cur_index); for(int i = index; i < (buff_ptr)->cur_index - 1; i++) (buff_ptr)->data[i] = (buff_ptr)->data[i + 1]; (buff_ptr)->cur_index -= 1; }
#define BUFF_CLEAR(buff_ptr) {memset((buff_ptr), 0, sizeof(*(buff_ptr)));  ((buff_ptr)->cur_index = 0);}

#define BUFF_QUEUE_APPEND(buff_ptr, element) { if(!BUFF_HAS_SPACE(buff_ptr)) BUFF_REMOVE_FRONT(buff_ptr); BUFF_APPEND(buff_ptr, element); }

// must have at least one element
#define BUFF_LAST(buff_ptr) (buff_ptr)->data[(buff_ptr)->cur_index - 1]

typedef struct
{
 void *data;
 size_t max_data_elems;
 size_t data_elem_size;
 int *cur_index;
} BuffRef;

#define BUFF_MAKEREF(buff_ptr) ((BuffRef){.data = (void*)((buff_ptr)->data), .max_data_elems = ARRLEN((buff_ptr)->data), .data_elem_size = sizeof((buff_ptr)->data[0]), .cur_index = &((buff_ptr)->cur_index)})

#define APPEND_TO_NAME(data, cur, cap, elem) { data[cur] = elem; cur += 1; assert(cur <= cap); }
