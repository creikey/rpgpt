#pragma once

// null terminator always built into buffers so can read properly from data
#define ARRLEN(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
#define BUFF_VALID(buff_ptr) assert((buff_ptr)->cur_index <= ARRLEN((buff_ptr)->data))
#define BUFF(type, max_size) struct { int cur_index; type data[max_size]; char null_terminator; }
#define BUFF_HAS_SPACE(buff_ptr) ( (buff_ptr)->cur_index < ARRLEN((buff_ptr)->data) )
#define BUFF_EMPTY(buff_ptr) ((buff_ptr)->cur_index == 0)
#define BUFF_APPEND(buff_ptr, element)  { (buff_ptr)->data[(buff_ptr)->cur_index++] = element; BUFF_VALID(buff_ptr); }
#define BUFF_ITER_EX(type, buff_ptr, begin_ind, cond, movement) for(type *it = &((buff_ptr)->data[begin_ind]); cond; movement)
#define BUFF_ITER(type, buff_ptr) BUFF_ITER_EX(type, (buff_ptr), 0, it < (buff_ptr)->data + (buff_ptr)->cur_index, it++)
#define BUFF_PUSH_FRONT(buff_ptr, value) { (buff_ptr)->cur_index++; BUFF_VALID(buff_ptr); for(int i = (buff_ptr)->cur_index - 1; i > 0; i--) { (buff_ptr)->data[i] = (buff_ptr)->data[i - 1]; }; (buff_ptr)->data[0] = value; }
#define BUFF_REMOVE_FRONT(buff_ptr) {if((buff_ptr)->cur_index > 0) {for(int i = 0; i < (buff_ptr)->cur_index - 1; i++) { (buff_ptr)->data[i] = (buff_ptr)->data[i+1]; }; (buff_ptr)->cur_index--;}}
#define BUFF_CLEAR(buff_ptr) {memset((buff_ptr), 0, sizeof(*(buff_ptr)));  ((buff_ptr)->cur_index = 0);}
