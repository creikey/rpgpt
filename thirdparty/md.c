// LICENSE AT END OF FILE (MIT).

/* 
** Overrides & Options Macros
**
** Overridable
**  "basic types" ** REQUIRED
**   #define/typedef i8, i16, i32, i64
**   #define/typedef u8, u16, u32, u64
**   #define/typedef f32, f64
**
**  "memset" ** REQUIRED
**   #define IMPL_Memset             (void*, int, uint64) -> void*
**   #define IMPL_Memmove            (void*, void*, uint64) -> void*
**
**  "file iteration" ** OPTIONAL (required for the metadesk FileIter helpers to work)
**   #define IMPL_FileIterBegin      (FileIter*, String8) -> Boolean
**   #define IMPL_FileIterNext       (Arena*, FileIter*) -> FileInfo
**   #define IMPL_FileIterEnd        (FileIter*) -> void
**
**  "file load" ** OPTIONAL (required for ParseWholeFile to work)
**   #define IMPL_LoadEntireFile     (Arena*, String8 filename) -> String8   
**
**  "low level memory" ** OPTIONAL (required when relying on the default arenas)
**   #define IMPL_Reserve            (uint64) -> void*
**   #define IMPL_Commit             (void*, uint64) -> b32
**   #define IMPL_Decommit           (void*, uint64) -> void
**   #define IMPL_Release            (void*, uint64) -> void
**
**
**  "arena" ** REQUIRED
**   #define IMPL_Arena              <type> (must set before including md.h)
**   #define IMPL_ArenaMinPos        uint64
**   #define IMPL_ArenaAlloc         () -> IMPL_Arena*
**   #define IMPL_ArenaRelease       (IMPL_Arena*) -> void
**   #define IMPL_ArenaGetPos        (IMPL_Arena*) -> uint64
**   #define IMPL_ArenaPush          (IMPL_Arena*, uint64) -> void*
**   #define IMPL_ArenaPopTo         (IMPL_Arena*, uint64) -> void
**   #define IMPL_ArenaSetAutoAlign  (IMPL_Arena*, uint64) -> void
**
**  "scratch" ** REQUIRED
**   #define IMPL_GetScratch         (IMPL_Arena**, uint64) -> IMPL_Arena*
**  "scratch constants" ** OPTIONAL (required for default scratch)
**   #define IMPL_ScratchCount       uint64 [default 2]
**
**  "sprintf" ** REQUIRED
**   #define IMPL_Vsnprintf          (char*, uint64, char const*, va_list) -> uint64
**
** Static Parameters to the Default Arena Implementation
**   #define DEFAULT_ARENA_RES_SIZE  uint64 [default 64 megabytes]
**   #define DEFAULT_ARENA_CMT_SIZE  uint64 [default 64 kilabytes]
**
** Default Implementation Controls
**  These controls default to '1' i.e. 'enabled'
**   #define DEFAULT_BASIC_TYPES -> construct "basic types" from stdint.h header
**   #define DEFAULT_MEMSET    -> construct "memset" from CRT
**   #define DEFAULT_FILE_ITER -> construct "file iteration" from OS headers
**   #define DEFAULT_MEMORY    -> construct "low level memory" from OS headers
**   #define DEFAULT_ARENA     -> construct "arena" from "low level memory"
**   #define DEFAULT_SCRATCH   -> construct "scratch" from "arena"
**   #define DEFAULT_SPRINTF   -> construct "vsnprintf" from internal implementaion
**
*/

#if !defined(C)
#define C

//~/////////////////////////////////////////////////////////////////////////////
/////////////////////////// CRT Implementation /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#if DEFAULT_MEMSET

#include <stdlib.h>
#include <string.h>

#if !defined(IMPL_Memset)
# define IMPL_Memset CRT_Memset
#endif
#if !defined(IMPL_Memmove)
# define IMPL_Memmove CRT_Memmove
#endif

#define CRT_Memset memset
#define CRT_Memmove memmove

#endif

#if DEFAULT_FILE_LOAD

#include <stdio.h>

#if !defined(IMPL_LoadEntireFile)
# define IMPL_LoadEntireFile CRT_LoadEntireFile
#endif

FUNCTION String8
CRT_LoadEntireFile(Arena *arena, String8 filename)
{
    ArenaTemp scratch = GetScratch(&arena, 1);
    String8 file_contents = ZERO_STRUCT;
    String8 filename_copy = S8Copy(scratch.arena, filename);
    FILE *file = fopen((char*)filename_copy.str, "rb");
    if(file != 0)
    {
        fseek(file, 0, SEEK_END);
        u64 file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        file_contents.str = PushArray(arena, u8, file_size+1);
        if(file_contents.str)
        {
            file_contents.size = file_size;
            fread(file_contents.str, 1, file_size, file);
            file_contents.str[file_contents.size] = 0;
        }
        fclose(file);
    }
    ReleaseScratch(scratch);
    return file_contents;
}

#endif


//~/////////////////////////////////////////////////////////////////////////////
/////////////////////////// Win32 Implementation ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//- win32 header
#if (DEFAULT_FILE_ITER || MD_2DEFAULT_MEMORY) && OS_WINDOWS
# include <Windows.h>
# pragma comment(lib, "User32.lib")
#endif

//- win32 "file iteration"
#if DEFAULT_FILE_ITER && OS_WINDOWS

#if !defined(IMPL_FileIterBegin)
# define IMPL_FileIterBegin WIN32_FileIterBegin
#endif
#if !defined(IMPL_FileIterNext)
# define IMPL_FileIterNext WIN32_FileIterNext
#endif
#if !defined(IMPL_FileIterEnd)
# define IMPL_FileIterEnd WIN32_FileIterEnd
#endif

typedef struct WIN32_FileIter{
    HANDLE state;
    u64 first;
    WIN32_FIND_DATAW find_data;
} WIN32_FileIter;

StaticAssert(sizeof(FileIter) >= sizeof(WIN32_FileIter), file_iter_size_check);

static b32
WIN32_FileIterBegin(FileIter *it, String8 path)
{
    //- init search
    ArenaTemp scratch = GetScratch(0, 0);
    
    u8 c = path.str[path.size - 1];
    b32 need_star = (c == '/' || c == '\\');
    String8 cpath = need_star ? S8Fmt(scratch.arena, "%.*s*", S8VArg(path)) : path;
    String16 cpath16 = S16FromS8(scratch.arena, cpath);
    
    WIN32_FIND_DATAW find_data = ZERO_STRUCT;
    HANDLE state = FindFirstFileW((WCHAR*)cpath16.str, &find_data);
    
    ReleaseScratch(scratch);
    
    //- fill results
    b32 result = !!state;
    if (result)
    {
        WIN32_FileIter *win32_it = (WIN32_FileIter*)it; 
        win32_it->state = state;
        win32_it->first = 1;
        MemoryCopy(&win32_it->find_data, &find_data, sizeof(find_data));
    }
    return(result);
}

static FileInfo
WIN32_FileIterNext(Arena *arena, FileIter *it)
{
    //- get low-level file info for this step
    b32 good = 0;
    
    WIN32_FileIter *win32_it = (WIN32_FileIter*)it; 
    WIN32_FIND_DATAW *find_data = &win32_it->find_data;
    if (win32_it->first)
    {
        win32_it->first = 0;
        good = 1;
    }
    else
    {
        good = FindNextFileW(win32_it->state, find_data);
    }
    
    //- convert to FileInfo
    FileInfo result = {0};
    if (good)
    {
        if (find_data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            result.flags |= FileFlag_Directory;
        }
        u16 *filename_base = (u16*)find_data->cFileName;
        u16 *ptr = filename_base;
        for (;*ptr != 0; ptr += 1);
        String16 filename16 = {filename_base, (u64)(ptr - filename_base)};
        result.filename = S8FromS16(arena, filename16);
        result.file_size = ((((u64)find_data->nFileSizeHigh) << 32) |
                            ((u64)find_data->nFileSizeLow));
    }
    return(result);
}

static void
WIN32_FileIterEnd(FileIter *it)
{
    WIN32_FileIter *win32_it = (WIN32_FileIter*)it; 
    CloseHandle(win32_it->state);
}

#endif

//- win32 "low level memory"
#if DEFAULT_MEMORY && OS_WINDOWS

#if !defined(IMPL_Reserve)
# define IMPL_Reserve WIN32_Reserve
#endif
#if !defined(IMPL_Commit)
# define IMPL_Commit WIN32_Commit
#endif
#if !defined(IMPL_Decommit)
# define IMPL_Decommit WIN32_Decommit
#endif
#if !defined(IMPL_Release)
# define IMPL_Release WIN32_Release
#endif

static void*
WIN32_Reserve(u64 size)
{
    void *result = VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
    return(result);
}

static b32
WIN32_Commit(void *ptr, u64 size)
{
    b32 result = (VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != 0);
    return(result);
}

static void
WIN32_Decommit(void *ptr, u64 size)
{
    VirtualFree(ptr, size, MEM_DECOMMIT);
}

static void
WIN32_Release(void *ptr, u64 size)
{
    VirtualFree(ptr, 0, MEM_RELEASE);
}

#endif

//~/////////////////////////////////////////////////////////////////////////////
////////////////////////// Linux Implementation ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//- linux headers
#if (DEFAULT_FILE_ITER || DEFAULT_MEMORY) && OS_LINUX
# include <dirent.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
# include <sys/syscall.h>
# include <sys/mman.h>
// NOTE(mal): To get these constants I need to #define _GNU_SOURCE,
// which invites non-POSIX behavior I'd rather avoid
# ifndef O_PATH
#  define O_PATH                010000000
# endif
# define AT_NO_AUTOMOUNT        0x800
# define AT_SYMLINK_NOFOLLOW    0x100
#endif

//- linux "file iteration"
#if DEFAULT_FILE_ITER && OS_LINUX

#if !defined(IMPL_FileIterIncrement)
# define IMPL_FileIterIncrement LINUX_FileIterIncrement
#endif

typedef struct LINUX_FileIter LINUX_FileIter;
struct LINUX_FileIter
{
    int dir_fd;
    DIR *dir;
};
StaticAssert(sizeof(LINUX_FileIter) <= sizeof(FileIter), file_iter_size_check);

static b32
LINUX_FileIterIncrement(Arena *arena, FileIter *opaque_it, String8 path,
                           FileInfo *out_info)
{
    b32 result = 0;
    
    LINUX_FileIter *it = (LINUX_FileIter *)opaque_it;
    if(it->dir == 0)
    {
        it->dir = opendir((char*)path.str);
        it->dir_fd = open((char *)path.str, O_PATH|O_CLOEXEC);
    }
    
    if(it->dir != 0 && it->dir_fd != -1)
    {
        struct dirent *dir_entry = readdir(it->dir);
        if(dir_entry)
        {
            out_info->filename = S8Fmt(arena, "%s", dir_entry->d_name);
            out_info->flags = 0;
            
            struct stat st; 
            if(fstatat(it->dir_fd, dir_entry->d_name, &st, AT_NO_AUTOMOUNT|AT_SYMLINK_NOFOLLOW) == 0)
            {
                if((st.st_mode & S_IFMT) == S_IFDIR)
                {
                    out_info->flags |= FileFlag_Directory;
                }
                out_info->file_size = st.st_size;
            }
            result = 1;
        }
    }
    
    if(result == 0)
    {
        if(it->dir != 0)
        {
            closedir(it->dir);
            it->dir = 0;
        }
        if(it->dir_fd != -1)
        {
            close(it->dir_fd);
            it->dir_fd = -1;
        }
    }
    
    return result;
}

#endif

//- linux "low level memory"
#if DEFAULT_MEMORY && OS_LINUX

#if !defined(IMPL_Reserve)
# define IMPL_Reserve LINUX_Reserve
#endif
#if !defined(IMPL_Commit)
# define IMPL_Commit LINUX_Commit
#endif
#if !defined(IMPL_Decommit)
# define IMPL_Decommit LINUX_Decommit
#endif
#if !defined(IMPL_Release)
# define IMPL_Release LINUX_Release
#endif

static void*
LINUX_Reserve(u64 size)
{
    void *result = mmap(0, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, (off_t)0);
    return(result);
}

static b32
LINUX_Commit(void *ptr, u64 size)
{
    b32 result = (mprotect(ptr, size, PROT_READ|PROT_WRITE) == 0);
    return(result);
}

static void
LINUX_Decommit(void *ptr, u64 size)
{
    mprotect(ptr, size, PROT_NONE);
    madvise(ptr, size, MADV_DONTNEED);
}

static void
LINUX_Release(void *ptr, u64 size)
{
    munmap(ptr, size);
}

#endif

//~/////////////////////////////////////////////////////////////////////////////
///////////// MD Arena From Reserve/Commit/Decommit/Release ////////////////////
////////////////////////////////////////////////////////////////////////////////

#if DEFAULT_ARENA

#if !defined(DEFAULT_ARENA_RES_SIZE)
# define DEFAULT_ARENA_RES_SIZE (64 << 20)
#endif
#if !defined(DEFAULT_ARENA_CMT_SIZE)
# define DEFAULT_ARENA_CMT_SIZE (64 << 10)
#endif

#define DEFAULT_ARENA_VERY_BIG (DEFAULT_ARENA_RES_SIZE - IMPL_ArenaMinPos)/2

//- "low level memory" implementation check
#if !defined(IMPL_Reserve)
# error Missing implementation for IMPL_Reserve
#endif
#if !defined(IMPL_Commit)
# error Missing implementation for IMPL_Commit
#endif
#if !defined(IMPL_Decommit)
# error Missing implementation for IMPL_Decommit
#endif
#if !defined(IMPL_Release)
# error Missing implementation for IMPL_Release
#endif

#define IMPL_ArenaMinPos 64
StaticAssert(sizeof(ArenaDefault) <= IMPL_ArenaMinPos, arena_def_size_check);

#define IMPL_ArenaAlloc     ArenaDefaultAlloc
#define IMPL_ArenaRelease   ArenaDefaultRelease
#define IMPL_ArenaGetPos    ArenaDefaultGetPos
#define IMPL_ArenaPush      ArenaDefaultPush
#define IMPL_ArenaPopTo     ArenaDefaultPopTo
#define IMPL_ArenaSetAutoAlign ArenaDefaultSetAutoAlign

static ArenaDefault*
ArenaDefaultAlloc__Size(u64 cmt, u64 res)
{
    Assert(IMPL_ArenaMinPos < cmt && cmt <= res);
    u64 cmt_clamped = ClampTop(cmt, res);
    ArenaDefault *result = 0;
    void *mem = IMPL_Reserve(res);
    if (IMPL_Commit(mem, cmt_clamped))
    {
        result = (ArenaDefault*)mem;
        result->prev = 0;
        result->current = result;
        result->base_pos = 0;
        result->pos = IMPL_ArenaMinPos;
        result->cmt = cmt_clamped;
        result->cap = res;
        result->align = 8;
    }
    return(result);
}

static ArenaDefault*
ArenaDefaultAlloc(void)
{
    ArenaDefault *result = ArenaDefaultAlloc__Size(DEFAULT_ARENA_CMT_SIZE,
                                                         DEFAULT_ARENA_RES_SIZE);
    return(result);
}

static void
ArenaDefaultRelease(ArenaDefault *arena)
{
    for (ArenaDefault *node = arena->current, *prev = 0;
         node != 0;
         node = prev)
    {
        prev = node->prev;
        IMPL_Release(node, node->cap);
    }
}

static u64
ArenaDefaultGetPos(ArenaDefault *arena)
{
    ArenaDefault *current = arena->current;
    u64 result = current->base_pos + current->pos;
    return(result);
}

static void*
ArenaDefaultPush(ArenaDefault *arena, u64 size)
{
    // try to be fast!
    ArenaDefault *current = arena->current;
    u64 align = arena->align;
    u64 pos = current->pos;
    u64 pos_aligned = AlignPow2(pos, align);
    u64 new_pos = pos_aligned + size;
    void *result = (u8*)current + pos_aligned;
    current->pos = new_pos;
    
    // if it's not going to work do the slow path
    if (new_pos > current->cmt)
    {
        result = 0;
        current->pos = pos;
        
        // new chunk if necessary
        if (new_pos > current->cap)
        {
            ArenaDefault *new_arena = 0;
            if (size > DEFAULT_ARENA_VERY_BIG)
            {
                u64 big_size_unrounded = size + IMPL_ArenaMinPos;
                u64 big_size = AlignPow2(big_size_unrounded, (4 << 10));
                new_arena = ArenaDefaultAlloc__Size(big_size, big_size);
            }
            else
            {
                new_arena = ArenaDefaultAlloc();
            }
            
            // link in new chunk & recompute new_pos
            if (new_arena != 0)
            {
                new_arena->base_pos = current->base_pos + current->cap;
                new_arena->prev = current;
                current = new_arena;
                pos_aligned = current->pos;
                new_pos = pos_aligned + size;
            }
        }
        
        // move ahead if the current chunk has enough reserve
        if (new_pos <= current->cap)
        {
            
            // extend commit if necessary
            if (new_pos > current->cmt)
            {
                u64 new_cmt_unclamped = AlignPow2(new_pos, DEFAULT_ARENA_CMT_SIZE);
                u64 new_cmt = ClampTop(new_cmt_unclamped, current->cap);
                u64 cmt_size = new_cmt - current->cmt;
                if (IMPL_Commit((u8*)current + current->cmt, cmt_size))
                {
                    current->cmt = new_cmt;
                }
            }
            
            // move ahead if the current chunk has enough commit
            if (new_pos <= current->cmt)
            {
                result = (u8*)current + current->pos;
                current->pos = new_pos;
            }
        }
    }
    
    return(result);
}

static void
ArenaDefaultPopTo(ArenaDefault *arena, u64 pos)
{
    // pop chunks in the chain
    u64 pos_clamped = ClampBot(IMPL_ArenaMinPos, pos);
    {
        ArenaDefault *node = arena->current;
        for (ArenaDefault *prev = 0;
             node != 0 && node->base_pos >= pos;
             node = prev)
        {
            prev = node->prev;
            IMPL_Release(node, node->cap);
        }
        arena->current = node;
    }
    
    // reset the pos of the current
    {
        ArenaDefault *current = arena->current;
        u64 local_pos_unclamped = pos - current->base_pos;
        u64 local_pos = ClampBot(local_pos_unclamped, IMPL_ArenaMinPos);
        current->pos = local_pos;
    }
}

static void
ArenaDefaultSetAutoAlign(ArenaDefault *arena, u64 align)
{
    arena->align = align;
}

static void
ArenaDefaultAbsorb(ArenaDefault *arena, ArenaDefault *sub_arena)
{
    ArenaDefault *current = arena->current;
    u64 base_pos_shift = current->base_pos + current->cap;
    for (ArenaDefault *node = sub_arena->current;
         node != 0;
         node = node->prev)
    {
        node->base_pos += base_pos_shift;
    }
    sub_arena->prev = arena->current;
    arena->current = sub_arena->current;
}

#endif

//- "arena" implementation checks
#if !defined(IMPL_ArenaAlloc)
# error Missing implementation for IMPL_ArenaAlloc
#endif
#if !defined(IMPL_ArenaRelease)
# error Missing implementation for IMPL_ArenaRelease
#endif
#if !defined(IMPL_ArenaGetPos)
# error Missing implementation for IMPL_ArenaGetPos
#endif
#if !defined(IMPL_ArenaPush)
# error Missing implementation for IMPL_ArenaPush
#endif
#if !defined(IMPL_ArenaPopTo)
# error Missing implementation for IMPL_ArenaPopTo
#endif
#if !defined(IMPL_ArenaSetAutoAlign)
# error Missing implementation for IMPL_ArenaSetAutoAlign
#endif
#if !defined(IMPL_ArenaMinPos)
# error Missing implementation for IMPL_ArenaMinPos
#endif

//~/////////////////////////////////////////////////////////////////////////////
///////////////////////////// MD Scratch Pool //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#if DEFAULT_SCRATCH

#if !defined(IMPL_ScratchCount)
# define IMPL_ScratchCount 2llu
#endif

#if !defined(IMPL_GetScratch)
# define IMPL_GetScratch GetScratchDefault
#endif

THREAD_LOCAL Arena *md_thread_scratch_pool[IMPL_ScratchCount] = {0, 0};

static Arena*
GetScratchDefault(Arena **conflicts, u64 count)
{
    Arena **scratch_pool = md_thread_scratch_pool;
    if (scratch_pool[0] == 0)
    {
        Arena **arena_ptr = scratch_pool;
        for (u64 i = 0; i < IMPL_ScratchCount; i += 1, arena_ptr += 1)
        {
            *arena_ptr = ArenaAlloc();
        }
    }
    Arena *result = 0;
    Arena **arena_ptr = scratch_pool;
    for (u64 i = 0; i < IMPL_ScratchCount; i += 1, arena_ptr += 1)
    {
        Arena *arena = *arena_ptr;
        Arena **conflict_ptr = conflicts;
        for (u32 j = 0; j < count; j += 1, conflict_ptr += 1)
        {
            if (arena == *conflict_ptr)
            {
                arena = 0;
                break;
            }
        }
        if (arena != 0)
        {
            result = arena;
            break;
        }
    }
    return(result);
}

#endif

//~/////////////////////////////////////////////////////////////////////////////
//////////////////////// MD Library Implementation /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#if DEFAULT_SPRINTF
#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_DECORATE(name) md_stbsp_##name
#include "md_stb_sprintf.h"
#endif


//~ Nil Node Definition

static Node _md_nil_node =
{
    &_md_nil_node,         // next
    &_md_nil_node,         // prev
    &_md_nil_node,         // parent
    &_md_nil_node,         // first_child
    &_md_nil_node,         // last_child
    &_md_nil_node,         // first_tag
    &_md_nil_node,         // last_tag
    NodeKind_Nil,       // kind
    0,                     // flags
    ZERO_STRUCT,        // string
    ZERO_STRUCT,        // raw_string
    0,                     // at
    &_md_nil_node,         // ref_target
    ZERO_STRUCT,        // prev_comment
    ZERO_STRUCT,        // next_comment
};

//~ Arena Functions

FUNCTION Arena*
ArenaAlloc(void)
{
    return(IMPL_ArenaAlloc());
}

FUNCTION void
ArenaRelease(Arena *arena)
{
    IMPL_ArenaRelease(arena);
}

FUNCTION void*
ArenaPush(Arena *arena, u64 size)
{
    void *result = IMPL_ArenaPush(arena, size);
    return(result);
}

FUNCTION void
ArenaPutBack(Arena *arena, u64 size)
{
    u64 pos = IMPL_ArenaGetPos(arena);
    u64 new_pos = pos - size;
    u64 new_pos_clamped = ClampBot(IMPL_ArenaMinPos, new_pos);
    IMPL_ArenaPopTo(arena, new_pos_clamped);
}

FUNCTION void
ArenaSetAlign(Arena *arena, u64 boundary)
{
    IMPL_ArenaSetAutoAlign(arena, boundary);
}

FUNCTION void
ArenaPushAlign(Arena *arena, u64 boundary)
{
    u64 pos = IMPL_ArenaGetPos(arena);
    u64 align_m1 = boundary - 1;
    u64 new_pos_aligned = (pos + align_m1)&(~align_m1);
    if (new_pos_aligned > pos)
    {
        u64 amt = new_pos_aligned - pos;
        MemoryZero(IMPL_ArenaPush(arena, amt), amt);
    }
}

FUNCTION void
ArenaClear(Arena *arena)
{
    IMPL_ArenaPopTo(arena, IMPL_ArenaMinPos);
}

FUNCTION ArenaTemp
ArenaBeginTemp(Arena *arena)
{
    ArenaTemp result;
    result.arena = arena;
    result.pos   = IMPL_ArenaGetPos(arena);
    return(result);
}

FUNCTION void
ArenaEndTemp(ArenaTemp temp)
{
    IMPL_ArenaPopTo(temp.arena, temp.pos);
}

//~ Arena Scratch Pool

FUNCTION ArenaTemp
GetScratch(Arena **conflicts, u64 count)
{
    Arena *arena = IMPL_GetScratch(conflicts, count);
    ArenaTemp result = ZERO_STRUCT;
    if (arena != 0)
    {
        result = ArenaBeginTemp(arena);
    }
    return(result);
}

//~ Characters

FUNCTION b32
CharIsAlpha(u8 c)
{
    return CharIsAlphaUpper(c) || CharIsAlphaLower(c);
}

FUNCTION b32
CharIsAlphaUpper(u8 c)
{
    return c >= 'A' && c <= 'Z';
}

FUNCTION b32
CharIsAlphaLower(u8 c)
{
    return c >= 'a' && c <= 'z';
}

FUNCTION b32
CharIsDigit(u8 c)
{
    return (c >= '0' && c <= '9');
}

FUNCTION b32
CharIsUnreservedSymbol(u8 c)
{
    return (c == '~' || c == '!' || c == '$' || c == '%' || c == '^' ||
            c == '&' || c == '*' || c == '-' || c == '=' || c == '+' ||
            c == '<' || c == '.' || c == '>' || c == '/' || c == '?' ||
            c == '|');
}

FUNCTION b32
CharIsReservedSymbol(u8 c)
{
    return (c == '{' || c == '}' || c == '(' || c == ')' || c == '\\' ||
            c == '[' || c == ']' || c == '#' || c == ',' || c == ';'  ||
            c == ':' || c == '@');
}

FUNCTION b32
CharIsSpace(u8 c)
{
    return c == ' ' || c == '\r' || c == '\t' || c == '\f' || c == '\v';
}

FUNCTION u8
CharToUpper(u8 c)
{
    return (c >= 'a' && c <= 'z') ? ('A' + (c - 'a')) : c;
}

FUNCTION u8
CharToLower(u8 c)
{
    return (c >= 'A' && c <= 'Z') ? ('a' + (c - 'A')) : c;
}

FUNCTION u8
CharToForwardSlash(u8 c)
{
    return (c == '\\' ? '/' : c);
}

//~ Strings

FUNCTION u64
CalculateCStringLength(char *cstr)
{
    u64 i = 0;
    for(; cstr[i]; i += 1);
    return i;
}

FUNCTION String8
S8(u8 *str, u64 size)
{
    String8 string;
    string.str = str;
    string.size = size;
    return string;
}

FUNCTION String8
S8Range(u8 *first, u8 *opl)
{
    String8 string;
    string.str = first;
    string.size = (u64)(opl - first);
    return string;
}

FUNCTION String8
S8Substring(String8 str, u64 min, u64 max)
{
    if(max > str.size)
    {
        max = str.size;
    }
    if(min > str.size)
    {
        min = str.size;
    }
    if(min > max)
    {
        u64 swap = min;
        min = max;
        max = swap;
    }
    str.size = max - min;
    str.str += min;
    return str;
}

FUNCTION String8
S8Skip(String8 str, u64 min)
{
    return S8Substring(str, min, str.size);
}

FUNCTION String8
S8Chop(String8 str, u64 nmax)
{
    return S8Substring(str, 0, str.size - nmax);
}

FUNCTION String8
S8Prefix(String8 str, u64 size)
{
    return S8Substring(str, 0, size);
}

FUNCTION String8
S8Suffix(String8 str, u64 size)
{
    return S8Substring(str, str.size - size, str.size);
}

FUNCTION b32
S8Match(String8 a, String8 b, MatchFlags flags)
{
    int result = 0;
    if(a.size == b.size || flags & StringMatchFlag_RightSideSloppy)
    {
        result = 1;
        for(u64 i = 0; i < a.size; i += 1)
        {
            b32 match = (a.str[i] == b.str[i]);
            if(flags & StringMatchFlag_CaseInsensitive)
            {
                match |= (CharToLower(a.str[i]) == CharToLower(b.str[i]));
            }
            if(flags & StringMatchFlag_SlashInsensitive)
            {
                match |= (CharToForwardSlash(a.str[i]) == CharToForwardSlash(b.str[i]));
            }
            if(match == 0)
            {
                result = 0;
                break;
            }
        }
    }
    return result;
}

FUNCTION u64
S8FindSubstring(String8 str, String8 substring, u64 start_pos, MatchFlags flags)
{
    b32 found = 0;
    u64 found_idx = str.size;
    for(u64 i = start_pos; i < str.size; i += 1)
    {
        if(i + substring.size <= str.size)
        {
            String8 substr_from_str = S8Substring(str, i, i+substring.size);
            if(S8Match(substr_from_str, substring, flags))
            {
                found_idx = i;
                found = 1;
                if(!(flags & MatchFlag_FindLast))
                {
                    break;
                }
            }
        }
    }
    return found_idx;
}

FUNCTION String8
S8Copy(Arena *arena, String8 string)
{
    String8 res;
    res.size = string.size;
    res.str = PushArray(arena, u8, string.size + 1);
    MemoryCopy(res.str, string.str, string.size);
    res.str[string.size] = 0;
    return(res);
}

FUNCTION String8
S8FmtV(Arena *arena, char *fmt, va_list args)
{
    String8 result = ZERO_STRUCT;
    va_list args2;
    va_copy(args2, args);
    u64 needed_bytes = IMPL_Vsnprintf(0, 0, fmt, args)+1;
    result.str = PushArray(arena, u8, needed_bytes);
    result.size = needed_bytes - 1;
    result.str[needed_bytes-1] = 0;
    IMPL_Vsnprintf((char*)result.str, needed_bytes, fmt, args2);
    return result;
}

FUNCTION String8
S8Fmt(Arena *arena, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String8 result = S8FmtV(arena, fmt, args);
    va_end(args);
    return result;
}

FUNCTION void
S8ListPush(Arena *arena, String8List *list, String8 string)
{
    String8Node *node = PushArrayZero(arena, String8Node, 1);
    node->string = string;
    
    QueuePush(list->first, list->last, node);
    list->node_count += 1;
    list->total_size += string.size;
}

FUNCTION void
S8ListPushFmt(Arena *arena, String8List *list, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String8 string = S8FmtV(arena, fmt, args);
    va_end(args);
    S8ListPush(arena, list, string);
}

FUNCTION void
S8ListConcat(String8List *list, String8List *to_push)
{
    if(to_push->first)
    {
        list->node_count += to_push->node_count;
        list->total_size += to_push->total_size;
        
        if(list->last == 0)
        {
            *list = *to_push;
        }
        else
        {
            list->last->next = to_push->first;
            list->last = to_push->last;
        }
    }
    MemoryZeroStruct(to_push);
}

FUNCTION String8List
S8Split(Arena *arena, String8 string, int split_count, String8 *splits)
{
    String8List list = ZERO_STRUCT;
    
    u64 split_start = 0;
    for(u64 i = 0; i < string.size; i += 1)
    {
        b32 was_split = 0;
        for(int split_idx = 0; split_idx < split_count; split_idx += 1)
        {
            b32 match = 0;
            if(i + splits[split_idx].size <= string.size)
            {
                match = 1;
                for(u64 split_i = 0; split_i < splits[split_idx].size && i + split_i < string.size; split_i += 1)
                {
                    if(splits[split_idx].str[split_i] != string.str[i + split_i])
                    {
                        match = 0;
                        break;
                    }
                }
            }
            if(match)
            {
                String8 split_string = S8(string.str + split_start, i - split_start);
                S8ListPush(arena, &list, split_string);
                split_start = i + splits[split_idx].size;
                i += splits[split_idx].size - 1;
                was_split = 1;
                break;
            }
        }
        
        if(was_split == 0 && i == string.size - 1)
        {
            String8 split_string = S8(string.str + split_start, i+1 - split_start);
            S8ListPush(arena, &list, split_string);
            break;
        }
    }
    
    return list;
}

FUNCTION String8
S8ListJoin(Arena *arena, String8List list, StringJoin *join_ptr)
{
    // setup join parameters
    StringJoin join = ZERO_STRUCT;
    if (join_ptr != 0)
    {
        MemoryCopy(&join, join_ptr, sizeof(join));
    }
    
    // calculate size & allocate
    u64 sep_count = 0;
    if (list.node_count > 1)
    {
        sep_count = list.node_count - 1;
    }
    String8 result = ZERO_STRUCT;
    result.size = (list.total_size + join.pre.size +
                   sep_count*join.mid.size + join.post.size);
    result.str = PushArrayZero(arena, u8, result.size);
    
    // fill
    u8 *ptr = result.str;
    MemoryCopy(ptr, join.pre.str, join.pre.size);
    ptr += join.pre.size;
    for(String8Node *node = list.first; node; node = node->next)
    {
        MemoryCopy(ptr, node->string.str, node->string.size);
        ptr += node->string.size;
        if (node != list.last)
        {
            MemoryCopy(ptr, join.mid.str, join.mid.size);
            ptr += join.mid.size;
        }
    }
    MemoryCopy(ptr, join.pre.str, join.pre.size);
    ptr += join.pre.size;
    
    return(result);
}

FUNCTION String8
S8Stylize(Arena *arena, String8 string, IdentifierStyle word_style,
             String8 separator)
{
    String8 result = ZERO_STRUCT;
    
    String8List words = ZERO_STRUCT;
    
    b32 break_on_uppercase = 0;
    {
        break_on_uppercase = 1;
        for(u64 i = 0; i < string.size; i += 1)
        {
            if(!CharIsAlpha(string.str[i]) && !CharIsDigit(string.str[i]))
            {
                break_on_uppercase = 0;
                break;
            }
        }
    }
    
    b32 making_word = 0;
    String8 word = ZERO_STRUCT;
    
    for(u64 i = 0; i < string.size;)
    {
        if(making_word)
        {
            if((break_on_uppercase && CharIsAlphaUpper(string.str[i])) ||
               string.str[i] == '_' || CharIsSpace(string.str[i]) ||
               i == string.size - 1)
            {
                if(i == string.size - 1)
                {
                    word.size += 1;
                }
                making_word = 0;
                S8ListPush(arena, &words, word);
            }
            else
            {
                word.size += 1;
                i += 1;
            }
        }
        else
        {
            if(CharIsAlpha(string.str[i]))
            {
                making_word = 1;
                word.str = string.str + i;
                word.size = 1;
            }
            i += 1;
        }
    }
    
    result.size = words.total_size;
    if(words.node_count > 1)
    {
        result.size += separator.size*(words.node_count-1);
    }
    result.str = PushArrayZero(arena, u8, result.size);
    
    {
        u64 write_pos = 0;
        for(String8Node *node = words.first; node; node = node->next)
        {
            
            // NOTE(rjf): Write word string to result.
            {
                MemoryCopy(result.str + write_pos, node->string.str, node->string.size);
                
                // NOTE(rjf): Transform string based on word style.
                switch(word_style)
                {
                    case IdentifierStyle_UpperCamelCase:
                    {
                        result.str[write_pos] = CharToUpper(result.str[write_pos]);
                        for(u64 i = write_pos+1; i < write_pos + node->string.size; i += 1)
                        {
                            result.str[i] = CharToLower(result.str[i]);
                        }
                    }break;
                    
                    case IdentifierStyle_LowerCamelCase:
                    {
                        b32 is_first = (node == words.first);
                        result.str[write_pos] = (is_first ?
                                                 CharToLower(result.str[write_pos]) :
                                                 CharToUpper(result.str[write_pos]));
                        for(u64 i = write_pos+1; i < write_pos + node->string.size; i += 1)
                        {
                            result.str[i] = CharToLower(result.str[i]);
                        }
                    }break;
                    
                    case IdentifierStyle_UpperCase:
                    {
                        for(u64 i = write_pos; i < write_pos + node->string.size; i += 1)
                        {
                            result.str[i] = CharToUpper(result.str[i]);
                        }
                    }break;
                    
                    case IdentifierStyle_LowerCase:
                    {
                        for(u64 i = write_pos; i < write_pos + node->string.size; i += 1)
                        {
                            result.str[i] = CharToLower(result.str[i]);
                        }
                    }break;
                    
                    default: break;
                }
                
                write_pos += node->string.size;
            }
            
            if(node->next)
            {
                MemoryCopy(result.str + write_pos, separator.str, separator.size);
                write_pos += separator.size;
            }
        }
    }
    
    return result;
}

//~ Unicode Conversions

GLOBAL u8 md_utf8_class[32] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,2,2,2,2,3,3,4,5,
};

FUNCTION DecodedCodepoint
DecodeCodepointFromUtf8(u8 *str, u64 max)
{
#define bitmask1 0x01
#define bitmask2 0x03
#define bitmask3 0x07
#define bitmask4 0x0F
#define bitmask5 0x1F
#define bitmask6 0x3F
#define bitmask7 0x7F
#define bitmask8 0xFF
#define bitmask9  0x01FF
#define bitmask10 0x03FF
    
    DecodedCodepoint result = {~((u32)0), 1};
    u8 byte = str[0];
    u8 byte_class = md_utf8_class[byte >> 3];
    switch (byte_class)
    {
        case 1:
        {
            result.codepoint = byte;
        }break;
        
        case 2:
        {
            if (2 <= max)
            {
                u8 cont_byte = str[1];
                if (md_utf8_class[cont_byte >> 3] == 0)
                {
                    result.codepoint = (byte & bitmask5) << 6;
                    result.codepoint |=  (cont_byte & bitmask6);
                    result.advance = 2;
                }
            }
        }break;
        
        case 3:
        {
            if (3 <= max)
            {
                u8 cont_byte[2] = {str[1], str[2]};
                if (md_utf8_class[cont_byte[0] >> 3] == 0 &&
                    md_utf8_class[cont_byte[1] >> 3] == 0)
                {
                    result.codepoint = (byte & bitmask4) << 12;
                    result.codepoint |= ((cont_byte[0] & bitmask6) << 6);
                    result.codepoint |=  (cont_byte[1] & bitmask6);
                    result.advance = 3;
                }
            }
        }break;
        
        case 4:
        {
            if (4 <= max)
            {
                u8 cont_byte[3] = {str[1], str[2], str[3]};
                if (md_utf8_class[cont_byte[0] >> 3] == 0 &&
                    md_utf8_class[cont_byte[1] >> 3] == 0 &&
                    md_utf8_class[cont_byte[2] >> 3] == 0)
                {
                    result.codepoint = (byte & bitmask3) << 18;
                    result.codepoint |= ((cont_byte[0] & bitmask6) << 12);
                    result.codepoint |= ((cont_byte[1] & bitmask6) <<  6);
                    result.codepoint |=  (cont_byte[2] & bitmask6);
                    result.advance = 4;
                }
            }
        }break;
    }
    
    return(result);
}

FUNCTION DecodedCodepoint
DecodeCodepointFromUtf16(u16 *out, u64 max)
{
    DecodedCodepoint result = {~((u32)0), 1};
    result.codepoint = out[0];
    result.advance = 1;
    if (1 < max && 0xD800 <= out[0] && out[0] < 0xDC00 && 0xDC00 <= out[1] && out[1] < 0xE000)
    {
        result.codepoint = ((out[0] - 0xD800) << 10) | (out[1] - 0xDC00);
        result.advance = 2;
    }
    return(result);
}

FUNCTION u32
Utf8FromCodepoint(u8 *out, u32 codepoint)
{
#define bit8 0x80
    u32 advance = 0;
    if (codepoint <= 0x7F)
    {
        out[0] = (u8)codepoint;
        advance = 1;
    }
    else if (codepoint <= 0x7FF)
    {
        out[0] = (bitmask2 << 6) | ((codepoint >> 6) & bitmask5);
        out[1] = bit8 | (codepoint & bitmask6);
        advance = 2;
    }
    else if (codepoint <= 0xFFFF)
    {
        out[0] = (bitmask3 << 5) | ((codepoint >> 12) & bitmask4);
        out[1] = bit8 | ((codepoint >> 6) & bitmask6);
        out[2] = bit8 | ( codepoint       & bitmask6);
        advance = 3;
    }
    else if (codepoint <= 0x10FFFF)
    {
        out[0] = (bitmask4 << 3) | ((codepoint >> 18) & bitmask3);
        out[1] = bit8 | ((codepoint >> 12) & bitmask6);
        out[2] = bit8 | ((codepoint >>  6) & bitmask6);
        out[3] = bit8 | ( codepoint        & bitmask6);
        advance = 4;
    }
    else
    {
        out[0] = '?';
        advance = 1;
    }
    return(advance);
}

FUNCTION u32
Utf16FromCodepoint(u16 *out, u32 codepoint)
{
    u32 advance = 1;
    if (codepoint == ~((u32)0))
    {
        out[0] = (u16)'?';
    }
    else if (codepoint < 0x10000)
    {
        out[0] = (u16)codepoint;
    }
    else
    {
        u64 v = codepoint - 0x10000;
        out[0] = 0xD800 + (v >> 10);
        out[1] = 0xDC00 + (v & bitmask10);
        advance = 2;
    }
    return(advance);
}

FUNCTION String8
S8FromS16(Arena *arena, String16 in)
{
    u64 cap = in.size*3;
    u8 *str = PushArrayZero(arena, u8, cap + 1);
    u16 *ptr = in.str;
    u16 *opl = ptr + in.size;
    u64 size = 0;
    DecodedCodepoint consume;
    for (;ptr < opl;)
    {
        consume = DecodeCodepointFromUtf16(ptr, opl - ptr);
        ptr += consume.advance;
        size += Utf8FromCodepoint(str + size, consume.codepoint);
    }
    str[size] = 0;
    ArenaPutBack(arena, cap - size); // := ((cap + 1) - (size + 1))
    return(S8(str, size));
}

FUNCTION String16
S16FromS8(Arena *arena, String8 in)
{
    u64 cap = in.size*2;
    u16 *str = PushArrayZero(arena, u16, cap + 1);
    u8 *ptr = in.str;
    u8 *opl = ptr + in.size;
    u64 size = 0;
    DecodedCodepoint consume;
    for (;ptr < opl;)
    {
        consume = DecodeCodepointFromUtf8(ptr, opl - ptr);
        ptr += consume.advance;
        size += Utf16FromCodepoint(str + size, consume.codepoint);
    }
    str[size] = 0;
    ArenaPutBack(arena, 2*(cap - size)); // := 2*((cap + 1) - (size + 1))
    String16 result = {str, size};
    return(result);
}

FUNCTION String8
S8FromS32(Arena *arena, String32 in)
{
    u64 cap = in.size*4;
    u8 *str = PushArrayZero(arena, u8, cap + 1);
    u32 *ptr = in.str;
    u32 *opl = ptr + in.size;
    u64 size = 0;
    DecodedCodepoint consume;
    for (;ptr < opl; ptr += 1)
    {
        size += Utf8FromCodepoint(str + size, *ptr);
    }
    str[size] = 0;
    ArenaPutBack(arena, cap - size); // := ((cap + 1) - (size + 1))
    return(S8(str, size));
}

FUNCTION String32
S32FromS8(Arena *arena, String8 in)
{
    u64 cap = in.size;
    u32 *str = PushArrayZero(arena, u32, cap + 1);
    u8 *ptr = in.str;
    u8 *opl = ptr + in.size;
    u64 size = 0;
    DecodedCodepoint consume;
    for (;ptr < opl;)
    {
        consume = DecodeCodepointFromUtf8(ptr, opl - ptr);
        ptr += consume.advance;
        str[size] = consume.codepoint;
        size += 1;
    }
    str[size] = 0;
    ArenaPutBack(arena, 4*(cap - size)); // := 4*((cap + 1) - (size + 1))
    String32 result = {str, size};
    return(result);
}

//~ File Name Strings

FUNCTION String8
PathChopLastPeriod(String8 string)
{
    u64 period_pos = S8FindSubstring(string, S8Lit("."), 0, MatchFlag_FindLast);
    if(period_pos < string.size)
    {
        string.size = period_pos;
    }
    return string;
}

FUNCTION String8
PathSkipLastSlash(String8 string)
{
    u64 slash_pos = S8FindSubstring(string, S8Lit("/"), 0,
                                          StringMatchFlag_SlashInsensitive|
                                          MatchFlag_FindLast);
    if(slash_pos < string.size)
    {
        string.str += slash_pos+1;
        string.size -= slash_pos+1;
    }
    return string;
}

FUNCTION String8
PathSkipLastPeriod(String8 string)
{
    u64 period_pos = S8FindSubstring(string, S8Lit("."), 0, MatchFlag_FindLast);
    if(period_pos < string.size)
    {
        string.str += period_pos+1;
        string.size -= period_pos+1;
    }
    return string;
}

FUNCTION String8
PathChopLastSlash(String8 string)
{
    u64 slash_pos = S8FindSubstring(string, S8Lit("/"), 0,
                                          StringMatchFlag_SlashInsensitive|
                                          MatchFlag_FindLast);
    if(slash_pos < string.size)
    {
        string.size = slash_pos;
    }
    return string;
}

FUNCTION String8
S8SkipWhitespace(String8 string)
{
    for(u64 i = 0; i < string.size; i += 1)
    {
        if(!CharIsSpace(string.str[i]))
        {
            string = S8Skip(string, i);
            break;
        }
    }
    return string;
}

FUNCTION String8
S8ChopWhitespace(String8 string)
{
    for(u64 i = string.size-1; i < string.size; i -= 1)
    {
        if(!CharIsSpace(string.str[i]))
        {
            string = S8Prefix(string, i+1);
            break;
        }
    }
    return string;
}

//~ Numeric Strings

GLOBAL u8 md_char_to_value[] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
};

GLOBAL u8 md_char_is_integer[] = {
    0,0,0,0,0,0,1,1,
    1,0,0,0,1,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
};

FUNCTION b32
StringIsU64(String8 string, u32 radix)
{
    b32 result = 0;
    if (string.size > 0)
    {
        result = 1;
        for (u8 *ptr = string.str, *opl = string.str + string.size;
             ptr < opl;
             ptr += 1)
        {
            u8 c = *ptr;
            if (!md_char_is_integer[c >> 3])
            {
                result = 0;
                break;
            }
            if (md_char_to_value[(c - 0x30)&0x1F] >= radix)
            {
                result = 0;
                break;
            }
        }
    }
    return(result);
}

FUNCTION b32
StringIsCStyleInt(String8 string)
{
    u8 *ptr = string.str;
    u8 *opl = string.str + string.size;
    
    // consume sign
    for (;ptr < opl && (*ptr == '+' || *ptr == '-'); ptr += 1);
    
    // radix from prefix
    u64 radix = 10;
    if (ptr < opl)
    {
        u8 c0 = *ptr;
        if (c0 == '0')
        {
            ptr += 1;
            radix = 8;
            if (ptr < opl)
            {
                u8 c1 = *ptr;
                if (c1 == 'x')
                {
                    ptr += 1;
                    radix = 0x10;
                }
                else if (c1 == 'b')
                {
                    ptr += 1;
                    radix = 2;
                }
            }
        }
    }
    
    // check integer "digits"
    String8 digits_substr = S8Range(ptr, opl);
    b32 result = StringIsU64(digits_substr, radix);
    
    return(result);
}

FUNCTION u64
U64FromString(String8 string, u32 radix)
{
    Assert(2 <= radix && radix <= 16);
    u64 value = 0;
    for (u64 i = 0; i < string.size; i += 1)
    {
        value *= radix;
        u8 c = string.str[i];
        value += md_char_to_value[(c - 0x30)&0x1F];
    }
    return(value);
}

FUNCTION i64
CStyleIntFromString(String8 string)
{
    u64 p = 0;
    
    // consume sign
    i64 sign = +1;
    if (p < string.size)
    {
        u8 c = string.str[p];
        if (c == '-')
        {
            sign = -1;
            p += 1;
        }
        else if (c == '+')
        {
            p += 1;
        }
    }
    
    // radix from prefix
    u64 radix = 10;
    if (p < string.size)
    {
        u8 c0 = string.str[p];
        if (c0 == '0')
        {
            p += 1;
            radix = 8;
            if (p < string.size)
            {
                u8 c1 = string.str[p];
                if (c1 == 'x')
                {
                    p += 1;
                    radix = 16;
                }
                else if (c1 == 'b')
                {
                    p += 1;
                    radix = 2;
                }
            }
        }
    }
    
    // consume integer "digits"
    String8 digits_substr = S8Skip(string, p);
    u64 n = U64FromString(digits_substr, radix);
    
    // combine result
    i64 result = sign*n;
    return(result);
}

FUNCTION f64
F64FromString(String8 string)
{
    char str[64];
    u64 str_size = string.size;
    if (str_size > sizeof(str) - 1)
    {
        str_size = sizeof(str) - 1;
    }
    MemoryCopy(str, string.str, str_size);
    str[str_size] = 0;
    return(atof(str));
}


FUNCTION String8
CStyleHexStringFromU64(Arena *arena, u64 x, b32 caps)
{
    static char md_int_value_to_char[] = "0123456789abcdef";
    u8 buffer[10];
    u8 *opl = buffer + 10;
    u8 *ptr = opl;
    if (x == 0)
    {
        ptr -= 1;
        *ptr = '0';
    }
    else
    {
        for (;;)
        {
            u32 val = x%16;
            x /= 16;
            u8 c = (u8)md_int_value_to_char[val];
            if (caps)
            {
                c = CharToUpper(c);
            }
            ptr -= 1;
            *ptr = c;
            if (x == 0)
            {
                break;
            }
        }
    }
    ptr -= 1;
    *ptr = 'x';
    ptr -= 1;
    *ptr = '0';
    
    String8 result = ZERO_STRUCT;
    result.size = (u64)(ptr - buffer);
    result.str = PushArray(arena, u8, result.size + 1);
    MemoryCopy(result.str, buffer, result.size);
    result.str[result.size] =0;
    return(result);
}

//~ Enum/Flag Strings

FUNCTION String8
StringFromNodeKind(NodeKind kind)
{
    // NOTE(rjf): @maintenance Must be kept in sync with NodeKind enum.
    static char *cstrs[NodeKind_COUNT] =
    {
        "Nil",
        
        "File",
        "ErrorMarker",
        
        "Main",
        "Tag",
        
        "List",
        "Reference",
    };
    return S8CString(cstrs[kind]);
}

FUNCTION String8List
StringListFromNodeFlags(Arena *arena, NodeFlags flags)
{
    // NOTE(rjf): @maintenance Must be kept in sync with NodeFlags enum.
    static char *flag_cstrs[] =
    {
        "HasParenLeft",
        "HasParenRight",
        "HasBracketLeft",
        "HasBracketRight",
        "HasBraceLeft",
        "HasBraceRight",
        
        "IsBeforeSemicolon",
        "IsAfterSemicolon",
        
        "IsBeforeComma",
        "IsAfterComma",
        
        "StringSingleQuote",
        "StringDoubleQuote",
        "StringTick",
        "StringTriplet",
        
        "Numeric",
        "Identifier",
        "StringLiteral",
    };
    
    String8List list = ZERO_STRUCT;
    u64 bits = sizeof(flags) * 8;
    for(u64 i = 0; i < bits && i < ArrayCount(flag_cstrs); i += 1)
    {
        if(flags & (1ull << i))
        {
            S8ListPush(arena, &list, S8CString(flag_cstrs[i]));
        }
    }
    return list;
}

//~ Map Table Data Structure

FUNCTION u64
HashStr(String8 string)
{
    u64 result = 5381;
    for(u64 i = 0; i < string.size; i += 1)
    {
        result = ((result << 5) + result) + string.str[i];
    }
    return result;
}

// NOTE(mal): Generic 64-bit hash function (https://nullprogram.com/blog/2018/07/31/)
//            Reversible, so no collisions. Assumes all bits of the pointer matter.
FUNCTION u64 
HashPtr(void *p)
{
    u64 h = (u64)p;
    h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9;
    h = (h ^ (h >> 27)) * 0x94d049bb133111eb;
    h = h ^ (h >> 31);
    return h;
}

FUNCTION Map
MapMakeBucketCount(Arena *arena, u64 bucket_count)
{
    Map result = {0};
    result.bucket_count = bucket_count;
    result.buckets = PushArrayZero(arena, MapBucket, bucket_count);
    return(result);
}

FUNCTION Map
MapMake(Arena *arena)
{
    Map result = MapMakeBucketCount(arena, 4093);
    return(result);
}

FUNCTION MapKey
MapKeyStr(String8 string)
{
    MapKey result = {0};
    if (string.size != 0)
    {
        result.hash = HashStr(string);
        result.size = string.size;
        if (string.size > 0)
        {
            result.ptr = string.str;
        }
    }
    return(result);
}

FUNCTION MapKey
MapKeyPtr(void *ptr)
{
    MapKey result = {0};
    if (ptr != 0)
    {
        result.hash = HashPtr(ptr);
        result.size = 0;
        result.ptr = ptr;
    }
    return(result);
}

FUNCTION MapSlot*
MapLookup(Map *map, MapKey key)
{
    MapSlot *result = 0;
    if (map->bucket_count > 0)
    {
        u64 index = key.hash%map->bucket_count;
        result = MapScan(map->buckets[index].first, key);
    }
    return(result);
}

FUNCTION MapSlot*
MapScan(MapSlot *first_slot, MapKey key)
{
    MapSlot *result = 0;
    if (first_slot != 0)
    {
        b32 ptr_kind = (key.size == 0);
        String8 key_string = S8((u8*)key.ptr, key.size);
        for (MapSlot *slot = first_slot;
             slot != 0;
             slot = slot->next)
        {
            if (slot->key.hash == key.hash)
            {
                if (ptr_kind)
                {
                    if (slot->key.size == 0 && slot->key.ptr == key.ptr)
                    {
                        result = slot;
                        break;
                    }
                }
                else
                {
                    String8 slot_string = S8((u8*)slot->key.ptr, slot->key.size);
                    if (S8Match(slot_string, key_string, 0))
                    {
                        result = slot;
                        break;
                    }
                }
            }
        }
    }
    return(result);
}

FUNCTION MapSlot*
MapInsert(Arena *arena, Map *map, MapKey key, void *val)
{
    MapSlot *result = 0;
    if (map->bucket_count > 0)
    {
        u64 index = key.hash%map->bucket_count;
        MapSlot *slot = PushArrayZero(arena, MapSlot, 1);
        MapBucket *bucket = &map->buckets[index];
        QueuePush(bucket->first, bucket->last, slot);
        slot->key = key;
        slot->val = val;
        result = slot;
    }
    return(result);
}

FUNCTION MapSlot*
MapOverwrite(Arena *arena, Map *map, MapKey key, void *val)
{
    MapSlot *result = MapLookup(map, key);
    if (result != 0)
    {
        result->val = val;
    }
    else
    {
        result = MapInsert(arena, map, key, val);
    }
    return(result);
}

//~ Parsing

FUNCTION Token
TokenFromString(String8 string)
{
    Token token = ZERO_STRUCT;
    
    u8 *one_past_last = string.str + string.size;
    u8 *first = string.str;
    
    if(first < one_past_last)
    {
        u8 *at = first;
        u32 skip_n = 0;
        u32 chop_n = 0;
        
#define TokenizerScan(cond) for (; at < one_past_last && (cond); at += 1)
        
        switch (*at)
        {
            // NOTE(allen): Whitespace parsing
            case '\n':
            {
                token.kind = TokenKind_Newline;
                at += 1;
            }break;
            
            case ' ': case '\r': case '\t': case '\f': case '\v':
            {
                token.kind = TokenKind_Whitespace;
                at += 1;
                TokenizerScan(*at == ' ' || *at == '\r' || *at == '\t' || *at == '\f' || *at == '\v');
            }break;
            
            // NOTE(allen): Comment parsing
            case '/':
            {
                if (at + 1 < one_past_last)
                {
                    if (at[1] == '/')
                    {
                        // trim off the first '//'
                        skip_n = 2;
                        at += 2;
                        token.kind = TokenKind_Comment;
                        TokenizerScan(*at != '\n' && *at != '\r');
                    }
                    else if (at[1] == '*')
                    {
                        // trim off the first '/*'
                        skip_n = 2;
                        at += 2;
                        token.kind = TokenKind_BrokenComment;
                        int counter = 1;
                        for (;at < one_past_last && counter > 0; at += 1)
                        {
                            if (at + 1 < one_past_last)
                            {
                                if (at[0] == '*' && at[1] == '/')
                                {
                                    at += 1;
                                    counter -= 1;
                                }
                                else if (at[0] == '/' && at[1] == '*')
                                {
                                    at += 1;
                                    counter += 1;
                                }
                            }
                        }
                        if(counter == 0)
                        {
                            token.kind = TokenKind_Comment;
                            chop_n = 2;
                        }
                    }
                }
                if (token.kind == 0) goto symbol_lex;
            }break;
            
            // NOTE(allen): Strings
            case '"':
            case '\'':
            case '`':
            {
                token.kind = TokenKind_BrokenStringLiteral;
                
                // determine delimiter setup
                u8 d = *at;
                b32 is_triplet = (at + 2 < one_past_last && at[1] == d && at[2] == d);
                
                // lex triple-delimiter string
                if (is_triplet)
                {
                    skip_n = 3;
                    at += 3;
                    u32 consecutive_d = 0;
                    for (;;)
                    {
                        // fail condition
                        if (at >= one_past_last)
                        {
                            break;
                        }
                        
                        if(at[0] == d)
                        {
                            consecutive_d += 1;
                            at += 1;
                            // close condition
                            if (consecutive_d == 3)
                            {
                                chop_n = 3;
                                token.kind = TokenKind_StringLiteral;
                                break;
                            }
                        }
                        else
                        {
                            consecutive_d = 0;
                            
                            // escaping rule
                            if(at[0] == '\\')
                            {
                                at += 1;
                                if(at < one_past_last && (at[0] == d || at[0] == '\\'))
                                {
                                    at += 1;
                                }
                            }
                            else{
                                at += 1;
                            }
                        }
                    }
                }
                
                // lex single-delimiter string
                if (!is_triplet)
                {
                    skip_n = 1;
                    at += 1;
                    for (;at < one_past_last;)
                    {
                        // close condition
                        if (*at == d)
                        {
                            at += 1;
                            chop_n = 1;
                            token.kind = TokenKind_StringLiteral;
                            break;
                        }
                        
                        // fail condition
                        if (*at == '\n')
                        {
                            break;
                        }
                        
                        // escaping rule
                        if (at[0] == '\\')
                        {
                            at += 1;
                            if (at < one_past_last && (at[0] == d || at[0] == '\\'))
                            {
                                at += 1;
                            }
                        }
                        else
                        {
                            at += 1;
                        }
                    }
                }
                
                //- rjf: set relevant node flags on token
                token.node_flags |= NodeFlag_StringLiteral;
                switch(d)
                {
                    case '\'': token.node_flags |= NodeFlag_StringSingleQuote; break;
                    case '"':  token.node_flags |= NodeFlag_StringDoubleQuote; break;
                    case '`':  token.node_flags |= NodeFlag_StringTick; break;
                    default: break;
                }
                if(is_triplet)
                {
                    token.node_flags |= NodeFlag_StringTriplet;
                }
                
            }break;
            
            // NOTE(allen): Identifiers, Numbers, Symbols
            default:
            {
                if (CharIsAlpha(*at) || *at == '_')
                {
                    token.node_flags |= NodeFlag_Identifier;
                    token.kind = TokenKind_Identifier;
                    at += 1;
                    TokenizerScan(CharIsAlpha(*at) || CharIsDigit(*at) || *at == '_');
                }
                
                else if (CharIsDigit(*at))
                {
                    token.node_flags |= NodeFlag_Numeric;
                    token.kind = TokenKind_Numeric;
                    at += 1;
                    
                    for (; at < one_past_last;)
                    {
                        b32 good = 0;
                        if (*at == 'e' || *at == 'E')
                        {
                            good = 1;
                            at += 1;
                            if (at < one_past_last && (*at == '+' || *at == '-'))
                            {
                                at += 1;
                            }
                        }
                        else if (CharIsAlpha(*at) || CharIsDigit(*at) || *at == '.' || *at == '_')
                        {
                            good = 1;
                            at += 1;
                        }
                        if (!good)
                        {
                            break;
                        }
                    }
                }
                
                else if (CharIsUnreservedSymbol(*at))
                {
                    symbol_lex:
                    
                    token.node_flags |= NodeFlag_Symbol;
                    token.kind = TokenKind_Symbol;
                    at += 1;
                    TokenizerScan(CharIsUnreservedSymbol(*at));
                }
                
                else if (CharIsReservedSymbol(*at))
                {
                    token.kind = TokenKind_Reserved;
                    at += 1;
                }
                
                else
                {
                    token.kind = TokenKind_BadCharacter;
                    at += 1;
                }
            }break;
        }
        
        token.raw_string = S8Range(first, at);
        token.string = S8Substring(token.raw_string, skip_n, token.raw_string.size - chop_n);
        
#undef TokenizerScan
        
    }
    
    return token;
}

FUNCTION u64
LexAdvanceFromSkips(String8 string, TokenKind skip_kinds)
{
    u64 result = string.size;
    u64 p = 0;
    for (;;)
    {
        Token token = TokenFromString(S8Skip(string, p));
        if ((skip_kinds & token.kind) == 0)
        {
            result = p;
            break;
        }
        p += token.raw_string.size;
    }
    return(result);
}

FUNCTION ParseResult
ParseResultZero(void)
{
    ParseResult result = ZERO_STRUCT;
    result.node = NilNode();
    return result;
}

FUNCTION ParseResult
ParseNodeSet(Arena *arena, String8 string, u64 offset, Node *parent,
                ParseSetRule rule)
{
    ParseResult result = ParseResultZero();
    u64 off = offset;
    
    //- rjf: fill data from set opener
    Token initial_token = TokenFromString(S8Skip(string, offset));
    u8 set_opener = 0;
    NodeFlags set_opener_flags = 0;
    b32 close_with_brace = 0;
    b32 close_with_paren = 0;
    b32 close_with_separator = 0;
    b32 parse_all = 0;
    switch(rule)
    {
        default: break;
        
        case ParseSetRule_EndOnDelimiter:
        {
            u64 opener_check_off = off;
            opener_check_off += LexAdvanceFromSkips(S8Skip(string, opener_check_off), TokenGroup_Irregular);
            initial_token = TokenFromString(S8Skip(string, opener_check_off));
            if(initial_token.kind == TokenKind_Reserved)
            {
                u8 c = initial_token.raw_string.str[0];
                if(c == '{')
                {
                    set_opener = '{';
                    set_opener_flags |= NodeFlag_HasBraceLeft;
                    opener_check_off += initial_token.raw_string.size;
                    off = opener_check_off;
                    close_with_brace = 1;
                }
                else if(c == '(')
                {
                    set_opener = '(';
                    set_opener_flags |= NodeFlag_HasParenLeft;
                    opener_check_off += initial_token.raw_string.size;
                    off = opener_check_off;
                    close_with_paren = 1;
                }
                else if(c == '[')
                {
                    set_opener = '[';
                    set_opener_flags |= NodeFlag_HasBracketLeft;
                    opener_check_off += initial_token.raw_string.size;
                    off = opener_check_off;
                    close_with_paren = 1;
                }
                else
                {
                    close_with_separator = 1;
                }
            }
            else
            {
                close_with_separator = 1;
            }
        }break;
        
        case ParseSetRule_Global:
        {
            parse_all = 1;
        }break;
    }
    
    //- rjf: fill parent data from opener
    parent->flags |= set_opener_flags;
    
    //- rjf: parse children
    b32 got_closer = 0;
    u64 parsed_child_count = 0;
    if(set_opener != 0 || close_with_separator || parse_all)
    {
        NodeFlags next_child_flags = 0;
        for(;off < string.size;)
        {
            
            //- rjf: check for separator closers
            if(close_with_separator)
            {
                u64 closer_check_off = off;
                
                //- rjf: check newlines
                {
                    Token potential_closer = TokenFromString(S8Skip(string, closer_check_off));
                    if(potential_closer.kind == TokenKind_Newline)
                    {
                        closer_check_off += potential_closer.raw_string.size;
                        off = closer_check_off;
                        
                        // NOTE(rjf): always terminate with a newline if we have >0 children
                        if(parsed_child_count > 0)
                        {
                            off = closer_check_off;
                            got_closer = 1;
                            break;
                        }
                        
                        // NOTE(rjf): terminate after double newline if we have 0 children
                        Token next_closer = TokenFromString(S8Skip(string, closer_check_off));
                        if(next_closer.kind == TokenKind_Newline)
                        {
                            closer_check_off += next_closer.raw_string.size;
                            off = closer_check_off;
                            got_closer = 1;
                            break;
                        }
                    }
                }
                
                //- rjf: check separators and possible braces from higher parents
                {
                    closer_check_off += LexAdvanceFromSkips(S8Skip(string, off), TokenGroup_Irregular);
                    Token potential_closer = TokenFromString(S8Skip(string, closer_check_off));
                    if(potential_closer.kind == TokenKind_Reserved)
                    {
                        u8 c = potential_closer.raw_string.str[0];
                        if(c == ',' || c == ';')
                        {
                            off = closer_check_off;
                            closer_check_off += potential_closer.raw_string.size;
                            break;
                        }
                        else if(c == '}' || c == ']'|| c == ')')
                        {
                            goto end_parse;
                        }
                    }
                }
                
            }
            
            //- rjf: check for non-separator closers
            if(!close_with_separator && !parse_all)
            {
                u64 closer_check_off = off;
                closer_check_off += LexAdvanceFromSkips(S8Skip(string, off), TokenGroup_Irregular);
                Token potential_closer = TokenFromString(S8Skip(string, closer_check_off));
                if(potential_closer.kind == TokenKind_Reserved)
                {
                    u8 c = potential_closer.raw_string.str[0];
                    if(close_with_brace && c == '}')
                    {
                        closer_check_off += potential_closer.raw_string.size;
                        off = closer_check_off;
                        parent->flags |= NodeFlag_HasBraceRight;
                        got_closer = 1;
                        break;
                    }
                    else if(close_with_paren && c == ']')
                    {
                        closer_check_off += potential_closer.raw_string.size;
                        off = closer_check_off;
                        parent->flags |= NodeFlag_HasBracketRight;
                        got_closer = 1;
                        break;
                    }
                    else if(close_with_paren && c == ')')
                    {
                        closer_check_off += potential_closer.raw_string.size;
                        off = closer_check_off;
                        parent->flags |= NodeFlag_HasParenRight;
                        got_closer = 1;
                        break;
                    }
                }
            }
            
            //- rjf: parse next child
            ParseResult child_parse = ParseOneNode(arena, string, off);
            MessageListConcat(&result.errors, &child_parse.errors);
            off += child_parse.string_advance;
            
            //- rjf: hook child into parent
            if(!NodeIsNil(child_parse.node))
            {
                // NOTE(rjf): @error No unnamed set children of implicitly-delimited sets
                if(close_with_separator &&
                   child_parse.node->string.size == 0 &&
                   child_parse.node->flags & (NodeFlag_HasParenLeft    |
                                              NodeFlag_HasParenRight   |
                                              NodeFlag_HasBracketLeft  |
                                              NodeFlag_HasBracketRight |
                                              NodeFlag_HasBraceLeft    |
                                              NodeFlag_HasBraceRight   ))
                {
                    String8 error_str = S8Lit("Unnamed set children of implicitly-delimited sets are not legal.");
                    Message *error = MakeNodeError(arena, child_parse.node, MessageKind_Warning,
                                                         error_str);
                    MessageListPush(&result.errors, error);
                }
                
                PushChild(parent, child_parse.node);
                parsed_child_count += 1;
            }
            
            //- rjf: check trailing separator
            NodeFlags trailing_separator_flags = 0;
            if(!close_with_separator)
            {
                off += LexAdvanceFromSkips(S8Skip(string, off), TokenGroup_Irregular);
                Token trailing_separator = TokenFromString(S8Skip(string, off));
                if (trailing_separator.kind == TokenKind_Reserved)
                {
                    u8 c = trailing_separator.string.str[0];
                    if(c == ',')
                    {
                        trailing_separator_flags |= NodeFlag_IsBeforeComma;
                        off += trailing_separator.raw_string.size;
                    }
                    else if(c == ';')
                    {
                        trailing_separator_flags |= NodeFlag_IsBeforeSemicolon;
                        off += trailing_separator.raw_string.size;
                    }
                }
            }
            
            //- rjf: fill child flags
            child_parse.node->flags |= next_child_flags | trailing_separator_flags;
            
            //- rjf: setup next_child_flags
            next_child_flags = NodeFlag_AfterFromBefore(trailing_separator_flags);
        }
    }
    end_parse:;
    
    //- rjf: push missing closer error, if we have one
    if(set_opener != 0 && got_closer == 0)
    {
        // NOTE(rjf): @error We didn't get a closer for the set
        String8 error_str = S8Fmt(arena, "Unbalanced \"%c\"", set_opener);
        Message *error = MakeTokenError(arena, string, initial_token,
                                              MessageKind_FatalError, error_str);
        MessageListPush(&result.errors, error);
    }
    
    //- rjf: push empty implicit set error,
    if(close_with_separator && parsed_child_count == 0)
    {
        // NOTE(rjf): @error No empty implicitly-delimited sets
        Message *error = MakeTokenError(arena, string, initial_token, MessageKind_Error,
                                              S8Lit("Empty implicitly-delimited node list"));
        MessageListPush(&result.errors, error);
    }
    
    //- rjf: fill result info
    result.node = parent;
    result.string_advance = off - offset;
    
    return result;
}

FUNCTION ParseResult
ParseOneNode(Arena *arena, String8 string, u64 offset)
{
    ParseResult result = ParseResultZero();
    u64 off = offset;
    
    //- rjf: parse pre-comment
    String8 prev_comment = ZERO_STRUCT;
    {
        Token comment_token = ZERO_STRUCT;
        for(;off < string.size;)
        {
            Token token = TokenFromString(S8Skip(string, off));
            if(token.kind == TokenKind_Comment)
            {
                off += token.raw_string.size;
                comment_token = token;
            }
            else if(token.kind == TokenKind_Newline)
            {
                off += token.raw_string.size;
                Token next_token = TokenFromString(S8Skip(string, off));
                if(next_token.kind == TokenKind_Comment)
                {
                    // NOTE(mal): If more than one comment, use the last comment
                    comment_token = next_token;
                }
                else if(next_token.kind == TokenKind_Newline)
                {
                    MemoryZeroStruct(&comment_token);
                }
            }
            else if((token.kind & TokenGroup_Whitespace) != 0)
            {
                off += token.raw_string.size;
            }
            else
            {
                break;
            }
            prev_comment = comment_token.string;
        }
    }
    
    //- rjf: parse tag list
    Node *first_tag = NilNode();
    Node *last_tag = NilNode();
    {
        for(;off < string.size;)
        {
            //- rjf: parse @ symbol, signifying start of tag
            off += LexAdvanceFromSkips(S8Skip(string, off), TokenGroup_Irregular);
            Token next_token = TokenFromString(S8Skip(string, off));
            if(next_token.kind != TokenKind_Reserved ||
               next_token.string.str[0] != '@')
            {
                break;
            }
            off += next_token.raw_string.size;
            
            //- rjf: parse string of tag node
            Token name = TokenFromString(S8Skip(string, off));
            u64 name_off = off;
            if((name.kind & TokenGroup_Label) == 0)
            {
                // NOTE(rjf): @error Improper token for tag string
                String8 error_str = S8Fmt(arena, "\"%.*s\" is not a proper tag label",
                                                S8VArg(name.raw_string));
                Message *error = MakeTokenError(arena, string, name, MessageKind_Error, error_str);
                MessageListPush(&result.errors, error);
                break;
            }
            off += name.raw_string.size;
            
            //- rjf: build tag
            Node *tag = MakeNode(arena, NodeKind_Tag, name.string, name.raw_string, name_off);
            
            //- rjf: parse tag arguments
            Token open_paren = TokenFromString(S8Skip(string, off));
            ParseResult args_parse = ParseResultZero();
            if(open_paren.kind == TokenKind_Reserved &&
               open_paren.string.str[0] == '(')
            {
                args_parse = ParseNodeSet(arena, string, off, tag, ParseSetRule_EndOnDelimiter);
                MessageListConcat(&result.errors, &args_parse.errors);
            }
            off += args_parse.string_advance;
            
            //- rjf: push tag to result
            NodeDblPushBack(first_tag, last_tag, tag);
        }
    }
    
    //- rjf: parse node
    Node *parsed_node = NilNode();
    ParseResult children_parse = ParseResultZero();
    retry:;
    {
        //- rjf: try to parse an unnamed set
        off += LexAdvanceFromSkips(S8Skip(string, off), TokenGroup_Irregular);
        Token unnamed_set_opener = TokenFromString(S8Skip(string, off));
        if(unnamed_set_opener.kind == TokenKind_Reserved)
        {
            u8 c = unnamed_set_opener.string.str[0];
            if (c == '(' || c == '{' || c == '[')
            {
                parsed_node = MakeNode(arena, NodeKind_Main, S8Lit(""), S8Lit(""),
                                          unnamed_set_opener.raw_string.str - string.str);
                children_parse = ParseNodeSet(arena, string, off, parsed_node,
                                                 ParseSetRule_EndOnDelimiter);
                off += children_parse.string_advance;
                MessageListConcat(&result.errors, &children_parse.errors);
            }
            else if (c == ')' || c == '}' || c == ']')
            {
                // NOTE(rjf): @error Unexpected set closing symbol
                String8 error_str = S8Fmt(arena, "Unbalanced \"%c\"", c);
                Message *error = MakeTokenError(arena, string, unnamed_set_opener,
                                                      MessageKind_FatalError, error_str);
                MessageListPush(&result.errors, error);
                off += unnamed_set_opener.raw_string.size;
            }
            else
            {
                // NOTE(rjf): @error Unexpected reserved symbol
                String8 error_str = S8Fmt(arena, "Unexpected reserved symbol \"%c\"", c);
                Message *error = MakeTokenError(arena, string, unnamed_set_opener,
                                                      MessageKind_Error, error_str);
                MessageListPush(&result.errors, error);
                off += unnamed_set_opener.raw_string.size;
            }
            goto end_parse;
            
        }
        
        //- rjf: try to parse regular node, with/without children
        off += LexAdvanceFromSkips(S8Skip(string, off), TokenGroup_Irregular);
        Token label_name = TokenFromString(S8Skip(string, off));
        if((label_name.kind & TokenGroup_Label) != 0)
        {
            off += label_name.raw_string.size;
            parsed_node = MakeNode(arena, NodeKind_Main, label_name.string, label_name.raw_string,
                                      label_name.raw_string.str - string.str);
            parsed_node->flags |= label_name.node_flags;
            
            //- rjf: try to parse children for this node
            u64 colon_check_off = off;
            colon_check_off += LexAdvanceFromSkips(S8Skip(string, colon_check_off), TokenGroup_Irregular);
            Token colon = TokenFromString(S8Skip(string, colon_check_off));
            if(colon.kind == TokenKind_Reserved &&
               colon.string.str[0] == ':')
            {
                colon_check_off += colon.raw_string.size;
                off = colon_check_off;
                
                children_parse = ParseNodeSet(arena, string, off, parsed_node,
                                                 ParseSetRule_EndOnDelimiter);
                off += children_parse.string_advance;
                MessageListConcat(&result.errors, &children_parse.errors);
            }
            goto end_parse;
        }
        
        //- rjf: collect bad token
        Token bad_token = TokenFromString(S8Skip(string, off));
        if(bad_token.kind & TokenGroup_Error)
        {
            off += bad_token.raw_string.size;
            
            switch (bad_token.kind)
            {
                case TokenKind_BadCharacter:
                {
                    String8List bytes = {0};
                    for(int i_byte = 0; i_byte < bad_token.raw_string.size; ++i_byte)
                    {
                        u8 b = bad_token.raw_string.str[i_byte];
                        S8ListPush(arena, &bytes, CStyleHexStringFromU64(arena, b, 1));
                    }
                    
                    StringJoin join = ZERO_STRUCT;
                    join.mid = S8Lit(" ");
                    String8 byte_string = S8ListJoin(arena, bytes, &join);
                    
                    // NOTE(rjf): @error Bad character
                    String8 error_str = S8Fmt(arena, "Non-ASCII character \"%.*s\"",
                                                    S8VArg(byte_string));
                    Message *error = MakeTokenError(arena, string, bad_token, MessageKind_Error,
                                                          error_str);
                    MessageListPush(&result.errors, error);
                }break;
                
                case TokenKind_BrokenComment:
                {
                    // NOTE(rjf): @error Broken Comments
                    Message *error = MakeTokenError(arena, string, bad_token, MessageKind_Error,
                                                          S8Lit("Unterminated comment"));
                    MessageListPush(&result.errors, error);
                }break;
                
                case TokenKind_BrokenStringLiteral:
                {
                    // NOTE(rjf): @error Broken String Literals
                    Message *error = MakeTokenError(arena, string, bad_token, MessageKind_Error,
                                                          S8Lit("Unterminated string literal"));
                    MessageListPush(&result.errors, error);
                }break;
            }
            goto retry;
        }
    }
    
    end_parse:;
    
    //- rjf: parse comments after nodes.
    String8 next_comment = ZERO_STRUCT;
    {
        Token comment_token = ZERO_STRUCT;
        for(;;)
        {
            Token token = TokenFromString(S8Skip(string, off));
            if(token.kind == TokenKind_Comment)
            {
                comment_token = token;
                off += token.raw_string.size;
                break;
            }
            
            else if(token.kind == TokenKind_Newline)
            {
                break;
            }
            else if((token.kind & TokenGroup_Whitespace) != 0)
            {
                off += token.raw_string.size;
            }
            else
            {
                break;
            }
        }
        next_comment = comment_token.string;
    }
    
    //- rjf: fill result
    parsed_node->prev_comment = prev_comment;
    parsed_node->next_comment = next_comment;
    result.node = parsed_node;
    if(!NodeIsNil(result.node))
    {
        result.node->first_tag = first_tag;
        result.node->last_tag = last_tag;
        for(Node *tag = first_tag; !NodeIsNil(tag); tag = tag->next)
        {
            tag->parent = result.node;
        }
    }
    result.string_advance = off - offset;
    
    return result;
}

FUNCTION ParseResult
ParseWholeString(Arena *arena, String8 filename, String8 contents)
{
    Node *root = MakeNode(arena, NodeKind_File, filename, contents, 0);
    ParseResult result = ParseNodeSet(arena, contents, 0, root, ParseSetRule_Global);
    result.node = root;
    for(Message *error = result.errors.first; error != 0; error = error->next)
    {
        if(NodeIsNil(error->node->parent))
        {
            error->node->parent = root;
        }
    }
    return result;
}

FUNCTION ParseResult
ParseWholeFile(Arena *arena, String8 filename)
{
    String8 file_contents = LoadEntireFile(arena, filename);
    ParseResult parse = ParseWholeString(arena, filename, file_contents);
    if(file_contents.str == 0)
    {
        // NOTE(rjf): @error File failing to load
        String8 error_str = S8Fmt(arena, "Could not read file \"%.*s\"", S8VArg(filename));
        Message *error = MakeNodeError(arena, parse.node, MessageKind_FatalError,
                                             error_str);
        MessageListPush(&parse.errors, error);
    }
    return parse;
}

//~ Messages (Errors/Warnings)

FUNCTION Node*
MakeErrorMarkerNode(Arena *arena, String8 parse_contents, u64 offset)
{
    Node *result = MakeNode(arena, NodeKind_ErrorMarker, S8Lit(""), parse_contents,
                                  offset);
    return(result);
}

FUNCTION Message*
MakeNodeError(Arena *arena, Node *node, MessageKind kind, String8 str)
{
    Message *error = PushArrayZero(arena, Message, 1);
    error->node = node;
    error->kind = kind;
    error->string = str;
    return error;
}

FUNCTION Message *
MakeTokenError(Arena *arena, String8 parse_contents, Token token,
                  MessageKind kind, String8 str)
{
    u64 offset = token.raw_string.str - parse_contents.str;
    Node *err_node = MakeErrorMarkerNode(arena, parse_contents, offset);
    return MakeNodeError(arena, err_node, kind, str);
}

FUNCTION void
MessageListPush(MessageList *list, Message *message)
{
    QueuePush(list->first, list->last, message);
    if(message->kind > list->max_message_kind)
    {
        list->max_message_kind = message->kind;
    }
    list->node_count += 1;
}

FUNCTION void
MessageListConcat(MessageList *list, MessageList *to_push)
{
    if(to_push->node_count != 0)
    {
        if(list->last != 0)
        {
            list->last->next = to_push->first;
            list->last = to_push->last;
            list->node_count += to_push->node_count;
            if(to_push->max_message_kind > list->max_message_kind)
            {
                list->max_message_kind = to_push->max_message_kind;
            }
        }
        else
        {
            *list = *to_push;
        }
        MemoryZeroStruct(to_push);
    }
}

//~ Location Conversions

FUNCTION CodeLoc
CodeLocFromFileOffset(String8 filename, u8 *base, u64 offset)
{
    CodeLoc loc;
    loc.filename = filename;
    loc.line = 1;
    loc.column = 1;
    if(base != 0)
    {
        u8 *at = base + offset;
        for(u64 i = 0; base+i < at && base[i]; i += 1)
        {
            if(base[i] == '\n')
            {
                loc.line += 1;
                loc.column = 1;
            }
            else
            {
                loc.column += 1;
            }
        }
    }
    return loc;
}

FUNCTION CodeLoc
CodeLocFromNode(Node *node)
{
    Node *file_root = NilNode();
    for(Node *parent = node->parent; !NodeIsNil(parent); parent = parent->parent)
    {
        if(parent->kind == NodeKind_File)
        {
            file_root = parent;
            break;
        }
    }
    Node *first_tag = file_root->first_tag;
    CodeLoc loc = {0};
    if(NodeIsNil(first_tag))
    {
        loc = CodeLocFromFileOffset(file_root->string, file_root->raw_string.str, node->offset);
    }
    else
    {
        loc = CodeLocFromFileOffset(file_root->string, first_tag->raw_string.str, node->offset);
    }
    return loc;
}

//~ Tree/List Building

FUNCTION b32
NodeIsNil(Node *node)
{
    return(node == 0 || node == &_md_nil_node || node->kind == NodeKind_Nil);
}

FUNCTION Node *
NilNode(void) { return &_md_nil_node; }

FUNCTION Node *
MakeNode(Arena *arena, NodeKind kind, String8 string, String8 raw_string,
            u64 offset)
{
    Node *node = PushArrayZero(arena, Node, 1);
    node->kind = kind;
    node->string = string;
    node->raw_string = raw_string;
    node->next = node->prev = node->parent =
        node->first_child = node->last_child =
        node->first_tag = node->last_tag = node->ref_target = NilNode();
    node->offset = offset;
    return node;
}

FUNCTION void
PushChild(Node *parent, Node *new_child)
{
    if (!NodeIsNil(new_child))
    {
        NodeDblPushBack(parent->first_child, parent->last_child, new_child);
        new_child->parent = parent;
    }
}

FUNCTION void
PushTag(Node *node, Node *tag)
{
    if (!NodeIsNil(tag))
    {
        NodeDblPushBack(node->first_tag, node->last_tag, tag);
        tag->parent = node;
    }
}

FUNCTION Node*
MakeList(Arena *arena)
{
    String8 empty = {0};
    Node *result = MakeNode(arena, NodeKind_List, empty, empty, 0);
    return(result);
}

FUNCTION void
ListConcatInPlace(Node *list, Node *to_push)
{
    if (!NodeIsNil(to_push->first_child))
    {
        if (!NodeIsNil(list->first_child))
        {
            list->last_child->next = to_push->first_child;
            list->last_child = to_push->last_child;
        }
        else
        {
            list->first_child = to_push->first_child;
            list->last_child = to_push->last_child;
        }
        to_push->first_child = to_push->last_child = NilNode();
    }
}

FUNCTION Node*
PushNewReference(Arena *arena, Node *list, Node *target)
{
    Node *n = MakeNode(arena, NodeKind_Reference, target->string, target->raw_string,
                             target->offset);
    n->ref_target = target;
    PushChild(list, n);
    return(n);
}

//~ Introspection Helpers

FUNCTION Node *
FirstNodeWithString(Node *first, String8 string, MatchFlags flags)
{
    Node *result = NilNode();
    for(Node *node = first; !NodeIsNil(node); node = node->next)
    {
        if(S8Match(string, node->string, flags))
        {
            result = node;
            break;
        }
    }
    return result;
}

FUNCTION Node *
NodeAtIndex(Node *first, int n)
{
    Node *result = NilNode();
    if(n >= 0)
    {
        int idx = 0;
        for(Node *node = first; !NodeIsNil(node); node = node->next, idx += 1)
        {
            if(idx == n)
            {
                result = node;
                break;
            }
        }
    }
    return result;
}

FUNCTION Node *
FirstNodeWithFlags(Node *first, NodeFlags flags)
{
    Node *result = NilNode();
    for(Node *n = first; !NodeIsNil(n); n = n->next)
    {
        if(n->flags & flags)
        {
            result = n;
            break;
        }
    }
    return result;
}

FUNCTION int
IndexFromNode(Node *node)
{
    int idx = 0;
    for(Node *last = node->prev; !NodeIsNil(last); last = last->prev, idx += 1);
    return idx;
}

FUNCTION Node *
RootFromNode(Node *node)
{
    Node *parent = node;
    for(Node *p = parent; !NodeIsNil(p); p = p->parent)
    {
        parent = p;
    }
    return parent;
}

FUNCTION Node *
MD_ChildFromString(Node *node, String8 child_string, MatchFlags flags)
{
    return FirstNodeWithString(node->first_child, child_string, flags);
}

FUNCTION Node *
TagFromString(Node *node, String8 tag_string, MatchFlags flags)
{
    return FirstNodeWithString(node->first_tag, tag_string, flags);
}

FUNCTION Node *
ChildFromIndex(Node *node, int n)
{
    return NodeAtIndex(node->first_child, n);
}

FUNCTION Node *
TagFromIndex(Node *node, int n)
{
    return NodeAtIndex(node->first_tag, n);
}

FUNCTION Node *
TagArgFromIndex(Node *node, String8 tag_string, MatchFlags flags, int n)
{
    Node *tag = TagFromString(node, tag_string, flags);
    return ChildFromIndex(tag, n);
}

FUNCTION Node *
TagArgFromString(Node *node, String8 tag_string, MatchFlags tag_str_flags, String8 arg_string, MatchFlags arg_str_flags)
{
    Node *tag = TagFromString(node, tag_string, tag_str_flags);
    Node *arg = MD_ChildFromString(tag, arg_string, arg_str_flags);
    return arg;
}

FUNCTION b32
NodeHasChild(Node *node, String8 string, MatchFlags flags)
{
    return !NodeIsNil(MD_ChildFromString(node, string, flags));
}

FUNCTION b32
NodeHasTag(Node *node, String8 string, MatchFlags flags)
{
    return !NodeIsNil(TagFromString(node, string, flags));
}

FUNCTION i64
ChildCountFromNode(Node *node)
{
    i64 result = 0;
    for(EachNode(child, node->first_child))
    {
        result += 1;
    }
    return result;
}

FUNCTION i64
TagCountFromNode(Node *node)
{
    i64 result = 0;
    for(EachNode(tag, node->first_tag))
    {
        result += 1;
    }
    return result;
}

FUNCTION Node *
ResolveNodeFromReference(Node *node)
{
    u64 safety = 100;
    for(; safety > 0 && node->kind == NodeKind_Reference;
        safety -= 1, node = node->ref_target);
    Node *result = node;
    return(result);
}

FUNCTION Node*
NodeNextWithLimit(Node *node, Node *opl)
{
    node = node->next;
    if (node == opl)
    {
        node = NilNode();
    }
    return(node);
}

FUNCTION String8
PrevCommentFromNode(Node *node)
{
    return(node->prev_comment);
}

FUNCTION String8
NextCommentFromNode(Node *node)
{
    return(node->next_comment);
}

//~ Error/Warning Helpers

FUNCTION String8
StringFromMessageKind(MessageKind kind)
{
    String8 result = ZERO_STRUCT;
    switch (kind)
    {
        default: break;
        case MessageKind_Note:       result = S8Lit("note");        break;
        case MessageKind_Warning:    result = S8Lit("warning");     break;
        case MessageKind_Error:      result = S8Lit("error");       break;
        case MessageKind_FatalError: result = S8Lit("fatal error"); break;
    }
    return(result);
}

FUNCTION String8
MD_FormatMessage(Arena *arena, CodeLoc loc, MessageKind kind, String8 string)
{
    String8 kind_string = StringFromMessageKind(kind);
    String8 result = S8Fmt(arena, "" FmtCodeLoc " %.*s: %.*s\n",
                                 CodeLocVArg(loc), S8VArg(kind_string), S8VArg(string));
    return(result);
}

#if !DISABLE_PRINT_HELPERS

FUNCTION void
PrintMessage(FILE *file, CodeLoc code_loc, MessageKind kind, String8 string)
{
    ArenaTemp scratch = GetScratch(0, 0);
    String8 message = MD_FormatMessage(scratch.arena, code_loc, kind, string);
    fwrite(message.str, message.size, 1, file);
    ReleaseScratch(scratch);
}

FUNCTION void
PrintMessageFmt(FILE *file, CodeLoc code_loc, MessageKind kind, char *fmt, ...)
{
    ArenaTemp scratch = GetScratch(0, 0);
    va_list args;
    va_start(args, fmt);
    String8 string = S8FmtV(scratch.arena, fmt, args);
    va_end(args);
    String8 message = MD_FormatMessage(scratch.arena, code_loc, kind, string);
    fwrite(message.str, message.size, 1, file);
    ReleaseScratch(scratch);
}

#endif

//~ Tree Comparison/Verification

FUNCTION b32
NodeMatch(Node *a, Node *b, MatchFlags flags)
{
    b32 result = 0;
    if(a->kind == b->kind && S8Match(a->string, b->string, flags))
    {
        result = 1;
        if(result && flags & NodeMatchFlag_NodeFlags)
        {
            result = result && a->flags == b->flags;
        }
        if(result && a->kind != NodeKind_Tag && (flags & NodeMatchFlag_Tags))
        {
            for(Node *a_tag = a->first_tag, *b_tag = b->first_tag;
                !NodeIsNil(a_tag) || !NodeIsNil(b_tag);
                a_tag = a_tag->next, b_tag = b_tag->next)
            {
                if(NodeMatch(a_tag, b_tag, flags))
                {
                    if(flags & NodeMatchFlag_TagArguments)
                    {
                        for(Node *a_tag_arg = a_tag->first_child, *b_tag_arg = b_tag->first_child;
                            !NodeIsNil(a_tag_arg) || !NodeIsNil(b_tag_arg);
                            a_tag_arg = a_tag_arg->next, b_tag_arg = b_tag_arg->next)
                        {
                            if(!NodeDeepMatch(a_tag_arg, b_tag_arg, flags))
                            {
                                result = 0;
                                goto end;
                            }
                        }
                    }
                }
                else
                {
                    result = 0;
                    goto end;
                }
            }
        }
    }
    end:;
    return result;
}

FUNCTION b32
NodeDeepMatch(Node *a, Node *b, MatchFlags flags)
{
    b32 result = NodeMatch(a, b, flags);
    if(result)
    {
        for(Node *a_child = a->first_child, *b_child = b->first_child;
            !NodeIsNil(a_child) || !NodeIsNil(b_child);
            a_child = a_child->next, b_child = b_child->next)
        {
            if(!NodeDeepMatch(a_child, b_child, flags))
            {
                result = 0;
                goto end;
            }
        }
    }
    end:;
    return result;
}

//~ Expression Parsing

FUNCTION void
ExprOprPush(Arena *arena, ExprOprList *list,
               ExprOprKind kind, u64 precedence, String8 string,
               u32 op_id, void *op_ptr)
{
    ExprOpr *op = PushArrayZero(arena, ExprOpr, 1);
    QueuePush(list->first, list->last, op);
    list->count += 1;
    op->op_id = op_id;
    op->kind = kind;
    op->precedence = precedence;
    op->string = string;
    op->op_ptr = op_ptr;
}

GLOBAL BakeOperatorErrorHandler md_bake_operator_error_handler = 0;

FUNCTION BakeOperatorErrorHandler
ExprSetBakeOperatorErrorHandler(BakeOperatorErrorHandler handler){
    BakeOperatorErrorHandler old_handler = md_bake_operator_error_handler;
    md_bake_operator_error_handler = handler;
    return old_handler;
}

FUNCTION ExprOprTable
ExprBakeOprTableFromList(Arena *arena, ExprOprList *list)
{
    ExprOprTable result = ZERO_STRUCT;
    
    // TODO(allen): @upgrade_potential(minor)
    
    for(ExprOpr *op = list->first;
        op != 0;
        op = op->next)
    {
        ExprOprKind op_kind = op->kind;
        String8 op_s = op->string;
        
        // error checking
        String8 error_str = ZERO_STRUCT;
        
        Token op_token = TokenFromString(op_s);
        b32 is_setlike_op =
        (op_s.size == 2 &&
         (S8Match(op_s, S8Lit("[]"), 0) || S8Match(op_s, S8Lit("()"), 0) ||
          S8Match(op_s, S8Lit("[)"), 0) || S8Match(op_s, S8Lit("(]"), 0) ||
          S8Match(op_s, S8Lit("{}"), 0)));
        
        if(op_kind != ExprOprKind_Prefix && op_kind != ExprOprKind_Postfix &&
           op_kind != ExprOprKind_Binary && op_kind != ExprOprKind_BinaryRightAssociative)
        {
            error_str = S8Fmt(arena, "Ignored operator \"%.*s\" because its kind value (%d) does not match "
                                 "any valid operator kind", S8VArg(op_s), op_kind);
        }
        else if(is_setlike_op && op_kind != ExprOprKind_Postfix)
        {
            error_str =
                S8Fmt(arena, "Ignored operator \"%.*s\". \"%.*s\" is only allowed as unary postfix",
                         S8VArg(op_s), S8VArg(op_s));
        }
        else if(!is_setlike_op &&
                (op_token.kind != TokenKind_Identifier && op_token.kind != TokenKind_Symbol))
        {
            error_str = S8Fmt(arena, "Ignored operator \"%.*s\" because it is neither a symbol "
                                 "nor an identifier token", S8VArg(op_s));
        }
        else if(!is_setlike_op && op_token.string.size < op_s.size)
        {
            error_str = S8Fmt(arena, "Ignored operator \"%.*s\" because its prefix \"%.*s\" "
                                 "constitutes a standalone operator",
                                 S8VArg(op_s), S8VArg(op_token.string));
        }
        else
        {
            for(ExprOpr *op2 = list->first;
                op2 != op;
                op2 = op2->next)
            {   // NOTE(mal): O(n^2)
                ExprOprKind op2_kind = op2->kind;
                String8 op2_s = op2->string;
                if(op->precedence == op2->precedence && 
                   ((op_kind == ExprOprKind_Binary &&
                     op2_kind == ExprOprKind_BinaryRightAssociative) ||
                    (op_kind == ExprOprKind_BinaryRightAssociative &&
                     op2_kind == ExprOprKind_Binary)))
                {
                    error_str =
                        S8Fmt(arena, "Ignored binary operator \"%.*s\" because another binary operator"
                                 "has the same precedence and different associativity", S8VArg(op_s));
                }
                else if(S8Match(op_s, op2_s, 0))
                {
                    if(op_kind == op2_kind)
                    {
                        error_str = S8Fmt(arena, "Ignored repeat operator \"%.*s\"", S8VArg(op_s));
                    }
                    else if(op_kind != ExprOprKind_Prefix && op2_kind != ExprOprKind_Prefix)
                    {
                        error_str =
                            S8Fmt(arena, "Ignored conflicting repeat operator \"%.*s\". There can't be"
                                     "more than one posfix/binary operator associated to the same token", 
                                     S8VArg(op_s));
                    }
                }
            }
        }
        
        // save error
        if(error_str.size != 0 && md_bake_operator_error_handler)
        {
            md_bake_operator_error_handler(MessageKind_Warning, error_str);
        }
        
        // save list
        else
        {
            ExprOprList *list = result.table + op_kind;
            ExprOpr *op_node_copy = PushArray(arena, ExprOpr, 1);
            *op_node_copy = *op;
            QueuePush(list->first, list->last, op_node_copy);
            list->count += 1;
        }
    }
    
    return(result);
}

FUNCTION ExprOpr*
ExprOprFromKindString(ExprOprTable *table, ExprOprKind kind, String8 s)
{
    // TODO(allen): @upgrade_potential
    
    // NOTE(mal): Look for operator on one or all (kind == ExprOprKind_Null) tables
    ExprOpr *result = 0;
    for(ExprOprKind cur_kind = (ExprOprKind)(ExprOprKind_Null + 1);
        cur_kind < ExprOprKind_COUNT;
        cur_kind = (ExprOprKind)(cur_kind + 1))
    {
        if(kind == ExprOprKind_Null || kind == cur_kind)
        {
            ExprOprList *op_list = table->table+cur_kind;
            for(ExprOpr *op = op_list->first;
                op != 0;
                op = op->next)
            {
                if(S8Match(op->string, s, 0))
                {
                    result = op;
                    goto dbl_break;
                }
            }
        }
    }
    dbl_break:;
    return result;
}

FUNCTION ExprParseResult
ExprParse(Arena *arena, ExprOprTable *op_table, Node *first, Node *opl)
{
    // setup a context
    ExprParseCtx ctx = ExprParse_MakeContext(op_table);
    
    // parse the top level
    Expr *expr = ExprParse_TopLevel(arena, &ctx, first, opl);
    
    // fill result
    ExprParseResult result = {0};
    result.expr = expr;
    result.errors = ctx.errors;
    return(result);
}

FUNCTION Expr*
Expr_NewLeaf(Arena *arena, Node *node)
{
    Expr *result = PushArrayZero(arena, Expr, 1);
    result->md_node = node;
    return(result);
}

FUNCTION Expr*
Expr_NewOpr(Arena *arena, ExprOpr *op, Node *op_node, Expr *l, Expr *r)
{
    Expr *result = PushArrayZero(arena, Expr, 1);
    result->op = op;
    result->md_node = op_node;
    result->parent = 0;
    result->left = l;
    result->right = r;
    if (l != 0)
    {
        Assert(l->parent == 0);
        l->parent = result;
    }
    if(r != 0)
    {
        Assert(r->parent == 0);
        r->parent = result;
    }
    return(result);
}

FUNCTION ExprParseCtx
ExprParse_MakeContext(ExprOprTable *op_table)
{
    ExprParseCtx result = ZERO_STRUCT;
    result.op_table = op_table;
    
    result.accel.postfix_set_ops[0] = ExprOprFromKindString(op_table, ExprOprKind_Postfix, S8Lit("()"));
    result.accel.postfix_set_flags[0] = NodeFlag_HasParenLeft | NodeFlag_HasParenRight;
    
    result.accel.postfix_set_ops[1] = ExprOprFromKindString(op_table, ExprOprKind_Postfix, S8Lit("[]"));
    result.accel.postfix_set_flags[1] = NodeFlag_HasBracketLeft | NodeFlag_HasBracketRight;
    
    result.accel.postfix_set_ops[2] = ExprOprFromKindString(op_table, ExprOprKind_Postfix, S8Lit("{}"));
    result.accel.postfix_set_flags[2] = NodeFlag_HasBraceLeft | NodeFlag_HasBraceRight;
    
    result.accel.postfix_set_ops[3] = ExprOprFromKindString(op_table, ExprOprKind_Postfix, S8Lit("[)"));
    result.accel.postfix_set_flags[3] = NodeFlag_HasBracketLeft | NodeFlag_HasParenRight;
    
    result.accel.postfix_set_ops[4] = ExprOprFromKindString(op_table, ExprOprKind_Postfix, S8Lit("(]"));
    result.accel.postfix_set_flags[4] = NodeFlag_HasParenLeft | NodeFlag_HasBracketRight;
    
    return(result);
}

FUNCTION Expr*
ExprParse_TopLevel(Arena *arena, ExprParseCtx *ctx, Node *first, Node *opl)
{
    // parse the node range
    Node *iter = first;
    Expr *expr = ExprParse_MinPrecedence(arena, ctx, &iter, first, opl, 0);
    
    // check for failed-to-reach-end error
    if(ctx->errors.max_message_kind == MessageKind_Null)
    {
        Node *stop_node = iter;
        if(!NodeIsNil(stop_node))
        {
            String8 error_str = S8Lit("Expected binary or unary postfix operator.");
            Message *error = MakeNodeError(arena,stop_node,MessageKind_FatalError,error_str);
            MessageListPush(&ctx->errors, error);
        }
    }
    
    return(expr);
}

FUNCTION b32
ExprParse_OprConsume(ExprParseCtx *ctx, Node **iter, Node *opl,
                        ExprOprKind kind, u32 min_precedence, ExprOpr **op_out)
{
    b32 result = 0;
    Node *node = *iter;
    if(!NodeIsNil(node))
    {
        ExprOpr *op = ExprOprFromKindString(ctx->op_table, kind, node->string);
        if(op != 0 && op->precedence >= min_precedence)
        {
            result = 1;
            *op_out = op;
            *iter= NodeNextWithLimit(*iter, opl);
        }
    }
    return result;
}

FUNCTION Expr*
ExprParse_Atom(Arena *arena, ExprParseCtx *ctx, Node **iter,
                  Node *first, Node *opl)
{
    // TODO(allen): nil
    Expr* result = 0;
    
    Node *node = *iter;
    ExprOpr *op = 0;
    
    if(NodeIsNil(node))
    {
        Node *last = first;
        for (;last->next != opl; last = last->next);
        
        Node *error_node = last->next;
        if (NodeIsNil(error_node))
        {
            Node *root = RootFromNode(node);
            String8 parse_contents = root->raw_string;
            u64 offset = last->offset + last->raw_string.size;
            error_node = MakeErrorMarkerNode(arena, parse_contents, offset);
        }
        
        String8 error_str = S8Lit("Unexpected end of expression.");
        Message *error = MakeNodeError(arena, error_node, MessageKind_FatalError,
                                             error_str);
        MessageListPush(&ctx->errors, error);
    }
    else if((node->flags & NodeFlag_HasParenLeft) &&
            (node->flags & NodeFlag_HasParenRight))
    { // NOTE(mal): Parens
        *iter = NodeNextWithLimit(*iter, opl);
        result = ExprParse_TopLevel(arena, ctx, node->first_child, NilNode());
    }
    else if(((node->flags & NodeFlag_HasBraceLeft)   && (node->flags & NodeFlag_HasBraceRight))   ||
            ((node->flags & NodeFlag_HasBracketLeft) && (node->flags & NodeFlag_HasBracketRight)) ||
            ((node->flags & NodeFlag_HasBracketLeft) && (node->flags & NodeFlag_HasParenRight))   ||
            ((node->flags & NodeFlag_HasParenLeft)   && (node->flags & NodeFlag_HasBracketRight)))
    { // NOTE(mal): Unparsed leaf sets ({...}, [...], [...), (...])
        *iter = NodeNextWithLimit(*iter, opl);
        result = Expr_NewLeaf(arena, node);
    }
    else if(ExprParse_OprConsume(ctx, iter, opl, ExprOprKind_Prefix, 1, &op))
    {
        u32 min_precedence = op->precedence + 1;
        Expr *sub_expr =
            ExprParse_MinPrecedence(arena, ctx, iter, first, opl, min_precedence);
        if(ctx->errors.max_message_kind == MessageKind_Null)
        {
            result = Expr_NewOpr(arena, op, node, sub_expr, 0);
        }
    }
    else if(ExprParse_OprConsume(ctx, iter, opl, ExprOprKind_Null, 1, &op))
    {
        String8 error_str = S8Fmt(arena, "Expected leaf. Got operator \"%.*s\".", S8VArg(node->string));
        
        Message *error = MakeNodeError(arena, node, MessageKind_FatalError, error_str);
        MessageListPush(&ctx->errors, error);
    }
    else if(node->flags &
            (NodeFlag_HasParenLeft|NodeFlag_HasParenRight|NodeFlag_HasBracketLeft|
             NodeFlag_HasBracketRight|NodeFlag_HasBraceLeft|NodeFlag_HasBraceRight))
    {
        String8 error_str = S8Fmt(arena, "Unexpected set.", S8VArg(node->string));
        Message *error = MakeNodeError(arena, node, MessageKind_FatalError, error_str);
        MessageListPush(&ctx->errors, error);
    }
    else{   // NOTE(mal): leaf
        *iter = NodeNextWithLimit(*iter, opl);
        result = Expr_NewLeaf(arena, node);
    }
    
    return(result);
}

FUNCTION Expr*
ExprParse_MinPrecedence(Arena *arena, ExprParseCtx *ctx,
                           Node **iter, Node *first, Node *opl,
                           u32 min_precedence)
{
    // TODO(allen): nil
    Expr* result = 0;
    
    result = ExprParse_Atom(arena, ctx, iter, first, opl);
    if(ctx->errors.max_message_kind == MessageKind_Null)
    {
        for (;!NodeIsNil(*iter);)
        {
            Node *node = *iter;
            ExprOpr *op = 0;
            
            if(ExprParse_OprConsume(ctx, iter, opl, ExprOprKind_Binary,
                                       min_precedence, &op) ||
               ExprParse_OprConsume(ctx, iter, opl, ExprOprKind_BinaryRightAssociative,
                                       min_precedence, &op))
            {
                u32 next_min_precedence = op->precedence + (op->kind == ExprOprKind_Binary);
                Expr *sub_expr =
                    ExprParse_MinPrecedence(arena, ctx, iter, first, opl, next_min_precedence);
                if(ctx->errors.max_message_kind == MessageKind_Null)
                {
                    result = Expr_NewOpr(arena, op, node, result, sub_expr);
                }
                else{
                    break;
                }
            }
            
            else
            {
                b32 found_postfix_setlike_operator = 0;
                for(u32 i_op = 0;
                    i_op < ArrayCount(ctx->accel.postfix_set_ops);
                    ++i_op)
                {
                    ExprOpr *op = ctx->accel.postfix_set_ops[i_op];
                    if(op && op->precedence >= min_precedence &&
                       node->flags == ctx->accel.postfix_set_flags[i_op])
                    {
                        *iter = NodeNextWithLimit(*iter, opl);
                        result = Expr_NewOpr(arena, op, node, result, 0);
                        found_postfix_setlike_operator = 1;
                        break;
                    }
                }
                
                if(!found_postfix_setlike_operator)
                {
                    if(ExprParse_OprConsume(ctx, iter, opl, ExprOprKind_Postfix,
                                               min_precedence, &op))
                    {
                        result = Expr_NewOpr(arena, op, node, result, 0);
                    }
                    else
                    {
                        break;  // NOTE: Due to lack of progress
                    }
                }
                
            }
            
        }
    }
    
    return(result);
}




//~ String Generation

FUNCTION void
DebugDumpFromNode(Arena *arena, String8List *out, Node *node,
                     int indent, String8 indent_string, GenerateFlags flags)
{
#define PrintIndent(_indent_level) do\
{\
for(int i = 0; i < (_indent_level); i += 1)\
{\
S8ListPush(arena, out, indent_string);\
}\
}while(0)
    
    //- rjf: prev-comment
    if(flags & GenerateFlag_Comments && node->prev_comment.size != 0)
    {
        PrintIndent(indent);
        S8ListPush(arena, out, S8Lit("/*\n"));
        PrintIndent(indent);
        S8ListPush(arena, out, node->prev_comment);
        PrintIndent(indent);
        S8ListPush(arena, out, S8Lit("\n"));
        PrintIndent(indent);
        S8ListPush(arena, out, S8Lit("*/\n"));
    }
    
    //- rjf: tags of node
    if(flags & GenerateFlag_Tags)
    {
        for(EachNode(tag, node->first_tag))
        {
            PrintIndent(indent);
            S8ListPush(arena, out, S8Lit("@"));
            S8ListPush(arena, out, tag->string);
            if(flags & GenerateFlag_TagArguments && !NodeIsNil(tag->first_child))
            {
                int tag_arg_indent = indent + 1 + tag->string.size + 1;
                S8ListPush(arena, out, S8Lit("("));
                for(EachNode(child, tag->first_child))
                {
                    int child_indent = tag_arg_indent;
                    if(NodeIsNil(child->prev))
                    {
                        child_indent = 0;
                    }
                    DebugDumpFromNode(arena, out, child, child_indent, S8Lit(" "), flags);
                    if(!NodeIsNil(child->next))
                    {
                        S8ListPush(arena, out, S8Lit(",\n"));
                    }
                }
                S8ListPush(arena, out, S8Lit(")\n"));
            }
            else
            {
                S8ListPush(arena, out, S8Lit("\n"));
            }
        }
    }
    
    //- rjf: node kind
    if(flags & GenerateFlag_NodeKind)
    {
        PrintIndent(indent);
        S8ListPush(arena, out, S8Lit("// kind: \""));
        S8ListPush(arena, out, StringFromNodeKind(node->kind));
        S8ListPush(arena, out, S8Lit("\"\n"));
    }
    
    //- rjf: node flags
    if(flags & GenerateFlag_NodeFlags)
    {
        PrintIndent(indent);
        ArenaTemp scratch = GetScratch(&arena, 1);
        String8List flag_strs = StringListFromNodeFlags(scratch.arena, node->flags);
        StringJoin join = { S8Lit(""), S8Lit("|"), S8Lit("") };
        String8 flag_str = S8ListJoin(arena, flag_strs, &join);
        S8ListPush(arena, out, S8Lit("// flags: \""));
        S8ListPush(arena, out, flag_str);
        S8ListPush(arena, out, S8Lit("\"\n"));
        ReleaseScratch(scratch);
    }
    
    //- rjf: location
    if(flags & GenerateFlag_Location)
    {
        PrintIndent(indent);
        CodeLoc loc = CodeLocFromNode(node);
        String8 string = S8Fmt(arena, "// location: %.*s:%i:%i\n", S8VArg(loc.filename), (int)loc.line, (int)loc.column);
        S8ListPush(arena, out, string);
    }
    
    //- rjf: name of node
    if(node->string.size != 0)
    {
        PrintIndent(indent);
        if(node->kind == NodeKind_File)
        {
            S8ListPush(arena, out, S8Lit("`"));
            S8ListPush(arena, out, node->string);
            S8ListPush(arena, out, S8Lit("`"));
        }
        else
        {
            S8ListPush(arena, out, node->raw_string);
        }
    }
    
    //- rjf: children list
    if(flags & GenerateFlag_Children && !NodeIsNil(node->first_child))
    {
        if(node->string.size != 0)
        {
            S8ListPush(arena, out, S8Lit(":\n"));
        }
        PrintIndent(indent);
        S8ListPush(arena, out, S8Lit("{\n"));
        for(EachNode(child, node->first_child))
        {
            DebugDumpFromNode(arena, out, child, indent+1, indent_string, flags);
            S8ListPush(arena, out, S8Lit(",\n"));
        }
        PrintIndent(indent);
        S8ListPush(arena, out, S8Lit("}"));
    }
    
    //- rjf: next-comment
    if(flags & GenerateFlag_Comments && node->next_comment.size != 0)
    {
        PrintIndent(indent);
        S8ListPush(arena, out, S8Lit("\n/*\n"));
        PrintIndent(indent);
        S8ListPush(arena, out, node->next_comment);
        PrintIndent(indent);
        S8ListPush(arena, out, S8Lit("\n"));
        PrintIndent(indent);
        S8ListPush(arena, out, S8Lit("*/\n"));
    }
    
#undef PrintIndent
}

FUNCTION void
ReconstructionFromNode(Arena *arena, String8List *out, Node *node,
                          int indent, String8 indent_string)
{
    CodeLoc code_loc = CodeLocFromNode(node);
    
#define PrintIndent(_indent_level) do\
{\
for(int i = 0; i < (_indent_level); i += 1)\
{\
S8ListPush(arena, out, indent_string);\
}\
}while(0)
    
    //- rjf: prev-comment
    if(node->prev_comment.size != 0)
    {
        String8 comment = S8SkipWhitespace(S8ChopWhitespace(node->prev_comment));
        b32 requires_multiline = S8FindSubstring(comment, S8Lit("\n"), 0, 0) < comment.size;
        PrintIndent(indent);
        if(requires_multiline)
        {
            S8ListPush(arena, out, S8Lit("/*\n"));
        }
        else
        {
            S8ListPush(arena, out, S8Lit("// "));
        }
        S8ListPush(arena, out, comment);
        if(requires_multiline)
        {
            S8ListPush(arena, out, S8Lit("\n*/\n"));
        }
        else
        {
            S8ListPush(arena, out, S8Lit("\n"));
        }
    }
    
    //- rjf: tags of node
    u32 tag_first_line = CodeLocFromNode(node->first_tag).line;
    u32 tag_last_line = tag_first_line;
    {
        for(EachNode(tag, node->first_tag))
        {
            u32 tag_line = CodeLocFromNode(tag).line;
            if(tag_line != tag_last_line)
            {
                S8ListPush(arena, out, S8Lit("\n"));
                tag_last_line = tag_line;
            }
            else if(!NodeIsNil(tag->prev))
            {
                S8ListPush(arena, out, S8Lit(" "));
            }
            
            PrintIndent(indent);
            S8ListPush(arena, out, S8Lit("@"));
            S8ListPush(arena, out, tag->string);
            if(!NodeIsNil(tag->first_child))
            {
                int tag_arg_indent = indent + 1 + tag->string.size + 1;
                S8ListPush(arena, out, S8Lit("("));
                u32 last_line = CodeLocFromNode(tag).line;
                for(EachNode(child, tag->first_child))
                {
                    CodeLoc child_loc = CodeLocFromNode(child);
                    if(child_loc.line != last_line)
                    {
                        S8ListPush(arena, out, S8Lit("\n"));
                        PrintIndent(indent);
                    }
                    last_line = child_loc.line;
                    
                    int child_indent = tag_arg_indent;
                    if(NodeIsNil(child->prev))
                    {
                        child_indent = 0;
                    }
                    ReconstructionFromNode(arena, out, child, child_indent, S8Lit(" "));
                    if(!NodeIsNil(child->next))
                    {
                        S8ListPush(arena, out, S8Lit(",\n"));
                    }
                }
                S8ListPush(arena, out, S8Lit(")"));
            }
        }
    }
    
    //- rjf: name of node
    if(node->string.size != 0)
    {
        if(tag_first_line != tag_last_line)
        {
            S8ListPush(arena, out, S8Lit("\n"));
            PrintIndent(indent);
        }
        else if(!NodeIsNil(node->first_tag) || !NodeIsNil(node->prev))
        {
            S8ListPush(arena, out, S8Lit(" "));
        }
        if(node->kind == NodeKind_File)
        {
            S8ListPush(arena, out, S8Lit("`"));
            S8ListPush(arena, out, node->string);
            S8ListPush(arena, out, S8Lit("`"));
        }
        else
        {
            S8ListPush(arena, out, node->raw_string);
        }
    }
    
    //- rjf: children list
    if(!NodeIsNil(node->first_child))
    {
        if(node->string.size != 0)
        {
            S8ListPush(arena, out, S8Lit(":"));
        }
        
        // rjf: figure out opener/closer symbols
        u8 opener_char = 0;
        u8 closer_char = 0;
        if(node->flags & NodeFlag_HasParenLeft)        { opener_char = '('; }
        else if(node->flags & NodeFlag_HasBracketLeft) { opener_char = '['; }
        else if(node->flags & NodeFlag_HasBraceLeft)   { opener_char = '{'; }
        if(node->flags & NodeFlag_HasParenRight)       { closer_char = ')'; }
        else if(node->flags & NodeFlag_HasBracketRight){ closer_char = ']'; }
        else if(node->flags & NodeFlag_HasBraceRight)  { closer_char = '}'; }
        
        b32 multiline = 0;
        for(EachNode(child, node->first_child))
        {
            CodeLoc child_loc = CodeLocFromNode(child);
            if(child_loc.line != code_loc.line)
            {
                multiline = 1;
                break;
            }
        }
        
        if(opener_char != 0)
        {
            if(multiline)
            {
                S8ListPush(arena, out, S8Lit("\n"));
                PrintIndent(indent);
            }
            else
            {
                S8ListPush(arena, out, S8Lit(" "));
            }
            S8ListPush(arena, out, S8(&opener_char, 1));
            if(multiline)
            {
                S8ListPush(arena, out, S8Lit("\n"));
                PrintIndent(indent+1);
            }
        }
        u32 last_line = CodeLocFromNode(node->first_child).line;
        for(EachNode(child, node->first_child))
        {
            int child_indent = 0;
            CodeLoc child_loc = CodeLocFromNode(child);
            if(child_loc.line != last_line)
            {
                S8ListPush(arena, out, S8Lit("\n"));
                PrintIndent(indent);
                child_indent = indent+1;
            }
            last_line = child_loc.line;
            ReconstructionFromNode(arena, out, child, child_indent, indent_string);
        }
        PrintIndent(indent);
        if(closer_char != 0)
        {
            if(last_line != code_loc.line)
            {
                S8ListPush(arena, out, S8Lit("\n"));
                PrintIndent(indent);
            }
            else
            {
                S8ListPush(arena, out, S8Lit(" "));
            }
            S8ListPush(arena, out, S8(&closer_char, 1));
        }
    }
    
    //- rjf: trailing separator symbols
    if(node->flags & NodeFlag_IsBeforeSemicolon)
    {
        S8ListPush(arena, out, S8Lit(";"));
    }
    else if(node->flags & NodeFlag_IsBeforeComma)
    {
        S8ListPush(arena, out, S8Lit(","));
    }
    
    //- rjf: next-comment
    // TODO(rjf): @node_comments
    if(node->next_comment.size != 0)
    {
        String8 comment = S8SkipWhitespace(S8ChopWhitespace(node->next_comment));
        b32 requires_multiline = S8FindSubstring(comment, S8Lit("\n"), 0, 0) < comment.size;
        PrintIndent(indent);
        if(requires_multiline)
        {
            S8ListPush(arena, out, S8Lit("/*\n"));
        }
        else
        {
            S8ListPush(arena, out, S8Lit("// "));
        }
        S8ListPush(arena, out, comment);
        if(requires_multiline)
        {
            S8ListPush(arena, out, S8Lit("\n*/\n"));
        }
        else
        {
            S8ListPush(arena, out, S8Lit("\n"));
        }
    }
    
#undef PrintIndent
}


#if !DISABLE_PRINT_HELPERS
FUNCTION void
PrintDebugDumpFromNode(FILE *file, Node *node, GenerateFlags flags)
{
    ArenaTemp scratch = GetScratch(0, 0);
    String8List list = {0};
    DebugDumpFromNode(scratch.arena, &list, node,
                         0, S8Lit(" "), flags);
    String8 string = S8ListJoin(scratch.arena, list, 0);
    fwrite(string.str, string.size, 1, file);
    ReleaseScratch(scratch);
}
#endif


//~ Command Line Argument Helper

FUNCTION String8List
StringListFromArgCV(Arena *arena, int argument_count, char **arguments)
{
    String8List options = ZERO_STRUCT;
    for(int i = 1; i < argument_count; i += 1)
    {
        S8ListPush(arena, &options, S8CString(arguments[i]));
    }
    return options;
}

FUNCTION CmdLine
MakeCmdLineFromOptions(Arena *arena, String8List options)
{
    CmdLine cmdln = ZERO_STRUCT;
    b32 parsing_only_inputs = 0;
    
    for(String8Node *n = options.first, *next = 0;
        n; n = next)
    {
        next = n->next;
        
        //- rjf: figure out whether or not this is an option by checking for `-` or `--`
        // from the beginning of the string
        String8 option_name = ZERO_STRUCT;
        if(S8Match(n->string, S8Lit("--"), 0))
        {
            parsing_only_inputs = 1;
        }
        else if(S8Match(S8Prefix(n->string, 2), S8Lit("--"), 0))
        {
            option_name = S8Skip(n->string, 2);
        }
        else if(S8Match(S8Prefix(n->string, 1), S8Lit("-"), 0))
        {
            option_name = S8Skip(n->string, 1);
        }
        
        //- rjf: trim off anything after a `:` or `=`, use that as the first value string
        String8 first_value = ZERO_STRUCT;
        b32 has_many_values = 0;
        if(option_name.size != 0)
        {
            u64 colon_signifier_pos = S8FindSubstring(option_name, S8Lit(":"), 0, 0);
            u64 equal_signifier_pos = S8FindSubstring(option_name, S8Lit("="), 0, 0);
            u64 signifier_pos = Min(colon_signifier_pos, equal_signifier_pos);
            if(signifier_pos < option_name.size)
            {
                first_value = S8Skip(option_name, signifier_pos+1);
                option_name = S8Prefix(option_name, signifier_pos);
                if(S8Match(S8Suffix(first_value, 1), S8Lit(","), 0))
                {
                    has_many_values = 1;
                }
            }
        }
        
        //- rjf: gather arguments
        if(option_name.size != 0 && !parsing_only_inputs)
        {
            String8List option_values = ZERO_STRUCT;
            
            //- rjf: push first value
            if(first_value.size != 0)
            {
                S8ListPush(arena, &option_values, first_value);
            }
            
            //- rjf: scan next string values, add them to option values until we hit a lack
            // of a ',' between values
            if(has_many_values)
            {
                for(String8Node *v = next; v; v = v->next, next = v)
                {
                    String8 value_str = v->string;
                    b32 next_has_arguments = S8Match(S8Suffix(value_str, 1), S8Lit(","), 0);
                    b32 in_quotes = 0;
                    u64 start = 0;
                    for(u64 i = 0; i <= value_str.size; i += 1)
                    {
                        if(i == value_str.size || (value_str.str[i] == ',' && in_quotes == 0))
                        {
                            if(start != i)
                            {
                                S8ListPush(arena, &option_values, S8Substring(value_str, start, i));
                            }
                            start = i+1;
                        }
                        else if(value_str.str[i] == '"')
                        {
                            in_quotes = !in_quotes;
                        }
                    }
                    if(next_has_arguments == 0)
                    {
                        break;
                    }
                }
            }
            
            //- rjf: insert the fully parsed option
            {
                CmdLineOption *opt = PushArrayZero(arena, CmdLineOption, 1);
                opt->name = option_name;
                opt->values = option_values;
                if(cmdln.last_option == 0)
                {
                    cmdln.first_option = cmdln.last_option = opt;
                }
                else
                {
                    cmdln.last_option->next = opt;
                    cmdln.last_option = cmdln.last_option->next;
                }
            }
        }
        
        //- rjf: this argument is not an option, push it to regular inputs list.
        else
        {
            S8ListPush(arena, &cmdln.inputs, n->string);
        }
    }
    
    return cmdln;
}

FUNCTION String8List
CmdLineValuesFromString(CmdLine cmdln, String8 name)
{
    String8List values = ZERO_STRUCT;
    for(CmdLineOption *opt = cmdln.first_option; opt; opt = opt->next)
    {
        if(S8Match(opt->name, name, 0))
        {
            values = opt->values;
            break;
        }
    }
    return values;
}

FUNCTION b32
CmdLineB32FromString(CmdLine cmdln, String8 name)
{
    b32 result = 0;
    for(CmdLineOption *opt = cmdln.first_option; opt; opt = opt->next)
    {
        if(S8Match(opt->name, name, 0))
        {
            result = 1;
            break;
        }
    }
    return result;
}

FUNCTION i64
CmdLineI64FromString(CmdLine cmdln, String8 name)
{
    String8List values = CmdLineValuesFromString(cmdln, name);
    ArenaTemp scratch = GetScratch(0, 0);
    String8 value_str = S8ListJoin(scratch.arena, values, 0);
    i64 result = CStyleIntFromString(value_str);
    ReleaseScratch(scratch);
    return(result);
}

//~ File System

FUNCTION String8
LoadEntireFile(Arena *arena, String8 filename)
{
    String8 result = ZERO_STRUCT;
#if defined(IMPL_LoadEntireFile)
    result = IMPL_LoadEntireFile(arena, filename);
#endif
    return(result);
}

FUNCTION b32
FileIterBegin(FileIter *it, String8 path)
{
#if !defined(IMPL_FileIterBegin)
    return(0);
#else
    return(IMPL_FileIterBegin(it, path));
#endif
}

FUNCTION FileInfo
FileIterNext(Arena *arena, FileIter *it)
{
#if !defined(IMPL_FileIterNext)
    FileInfo result = {0};
    return(result);
#else
    return(IMPL_FileIterNext(arena, it));
#endif
}

FUNCTION void
FileIterEnd(FileIter *it)
{
#if defined(IMPL_FileIterEnd)
    IMPL_FileIterEnd(it);
#endif
}

#endif // C

/*
Copyright 2021 Dion Systems LLC

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
