// LICENSE AT END OF FILE (MIT).

/*
** Weclome to Metadesk!
** 
** Metadesk is a data description language designed to look like a programming
** language, and this is the accompanying parser library. While you are free to
** use it however you see fit, here are a couple of the uses we have intended
** to support:
**  + quickly writing a C or C++ metaprogram from scratch
**  + building "low budget" domain specific languages, such as marked-up
**    webpage content, or asset metadata
**  + creating robust and flexible config systems for applications
** 
** If it's your first time with Metadesk, check out the "How to Build" section
** below, and consider looking at the examples included with the library. The
** examples_directory.txt will help you find your way from the intro examples
** through all the more advanced aspects of the library you might like to
** learn about.
**
** Direct issues, questions, suggestions, requests, etc to:
** https://github.com/Dion-Systems/metadesk
** 
**
** How To Build:
**
** The library is set up as a direct source-include library, so if you have a
** single unit build you can just #include "md.h" and "md.c". If you have a
** multiple unit build you can #include "md.h" where necessary and add "md.c"
** as a separate compilation unit (extra care has to be taken if you intend to
** use overrides in a multiple unit build).
**
** See `bin/compile_flags.txt` for the flags to build with.
**
** The tests and examples can be built with the bash scripts in bin. There are
** a few things to know to use these scripts:
**  1. First you should run `bld_init.sh` which will initialize your copy of
**     Metadesk's build system.
**  2. On Linux the shell scripts should work as written. On Windows you will
**     need to use a bash interpreter specifically. Generally the `bash.exe`
**     that comes with an install of git on Windows works well for this.
**     Add it to your path or setup a batch script that calls it and then
**     pass the bash scripts to the interpreter to build.
**  3. You should be able to run the scripts:
**      `build_tests.sh`
**      `build_examples.sh`
**      `run_tests.sh`
**      `run_examples.sh`
**      `type_metadata_example.sh`
*/

#ifndef MD_H
#define MD_H

#define VERSION_MAJ 1
#define VERSION_MIN 0

//~ Set default values for controls
#if !defined(DEFAULT_BASIC_TYPES)
# define DEFAULT_BASIC_TYPES 1
#endif
#if !defined(DEFAULT_MEMSET)
# define DEFAULT_MEMSET 1
#endif
#if !defined(DEFAULT_FILE_LOAD)
# define DEFAULT_FILE_LOAD 1
#endif
#if !defined(DEFAULT_FILE_ITER)
# define DEFAULT_FILE_ITER 1
#endif
#if !defined(DEFAULT_MEMORY)
# define DEFAULT_MEMORY 1
#endif
#if !defined(DEFAULT_ARENA)
# define DEFAULT_ARENA 1
#endif
#if !defined(DEFAULT_SCRATCH)
# define DEFAULT_SCRATCH 1
#endif
#if !defined(DEFAULT_SPRINTF)
# define DEFAULT_SPRINTF 1
#endif

#if !defined(DISABLE_PRINT_HELPERS)
# define DISABLE_PRINT_HELPERS 0
#endif


//~/////////////////////////////////////////////////////////////////////////////
////////////////////////////// Context Cracking ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#if defined(__clang__)

# define COMPILER_CLANG 1

# if defined(__APPLE__) && defined(__MACH__)
#  define OS_MAC 1
# elif defined(__gnu_linux__)
#  define OS_LINUX 1
# elif defined(_WIN32)
#  define OS_WINDOWS 1
# else
#  error This compiler/platform combo is not supported yet
# endif

# if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
#  define ARCH_X64 1
# elif defined(i386) || defined(__i386) || defined(__i386__)
#  define ARCH_X86 1
# elif defined(__aarch64__)
#  define ARCH_ARM64 1
# elif defined(__arm__)
#  define ARCH_ARM32 1
# else
#  error architecture not supported yet
# endif

#elif defined(_MSC_VER)

# define COMPILER_CL 1

# if defined(_WIN32)
#  define OS_WINDOWS 1
# else
#  error This compiler/platform combo is not supported yet
# endif

# if defined(_M_AMD64)
#  define ARCH_X64 1
# elif defined(_M_IX86)
#  define ARCH_X86 1
# elif defined(_M_ARM64)
#  define ARCH_ARM64 1
# elif defined(_M_ARM)
#  define ARCH_ARM32 1
# else
#  error architecture not supported yet
# endif

# if _MSC_VER >= 1920
#  define COMPILER_CL_YEAR 2019
# elif _MSC_VER >= 1910
#  define COMPILER_CL_YEAR 2017
# elif _MSC_VER >= 1900
#  define COMPILER_CL_YEAR 2015
# elif _MSC_VER >= 1800
#  define COMPILER_CL_YEAR 2013
# elif _MSC_VER >= 1700
#  define COMPILER_CL_YEAR 2012
# elif _MSC_VER >= 1600
#  define COMPILER_CL_YEAR 2010
# elif _MSC_VER >= 1500
#  define COMPILER_CL_YEAR 2008
# elif _MSC_VER >= 1400
#  define COMPILER_CL_YEAR 2005
# else
#  define COMPILER_CL_YEAR 0
# endif

#elif defined(__GNUC__) || defined(__GNUG__)

# define COMPILER_GCC 1

# if defined(__gnu_linux__)
#  define OS_LINUX 1
# else
#  error This compiler/platform combo is not supported yet
# endif

# if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
#  define ARCH_X64 1
# elif defined(i386) || defined(__i386) || defined(__i386__)
#  define ARCH_X86 1
# elif defined(__aarch64__)
#  define ARCH_ARM64 1
# elif defined(__arm__)
#  define ARCH_ARM32 1
# else
#  error architecture not supported yet
# endif

#else
# error This compiler is not supported yet
#endif

#if defined(ARCH_X64)
# define ARCH_64BIT 1
#elif defined(ARCH_X86)
# define ARCH_32BIT 1
#endif

#if defined(__cplusplus)
# define LANG_CPP 1

// We can't get this 100% correct thanks to Microsoft's compiler.
// So this check lets us pre-define CPP_VERSION if we have to.
# if !defined(CPP_VERSION)
#  if defined(COMPILER_CL)
// CL is annoying and didn't update __cplusplus over time
// If it is available _MSVC_LANG serves the same role
#   if defined(_MSVC_LANG)
#    if _MSVC_LANG <= 199711L
#     define CPP_VERSION 98
#    elif _MSVC_LANG <= 201103L
#     define CPP_VERSION 11
#    elif _MSVC_LANG <= 201402L
#     define CPP_VERSION 14
#    elif _MSVC_LANG <= 201703L
#     define CPP_VERSION 17
#    elif _MSVC_LANG <= 202002L
#     define CPP_VERSION 20
#    else
#     define CPP_VERSION 23
#    endif
// If we don't have _MSVC_LANG we can guess from the compiler version
#   else
#    if COMPILER_CL_YEAR <= 2010
#     define CPP_VERSION 98
#    elif COMPILER_CL_YEAR <= 2015
#     define CPP_VERSION 11
#    else
#     define CPP_VERSION 17
#    endif
#   endif
#  else
// Other compilers use __cplusplus correctly
#   if __cplusplus <= 199711L
#    define CPP_VERSION 98
#   elif __cplusplus <= 201103L
#    define CPP_VERSION 11
#   elif __cplusplus <= 201402L
#    define CPP_VERSION 14
#   elif __cplusplus <= 201703L
#    define CPP_VERSION 17
#   elif __cplusplus <= 202002L
#    define CPP_VERSION 20
#   else
#    define CPP_VERSION 23
#   endif
#  endif
# endif

#else
# define LANG_C 1
#endif

// zeroify

#if !defined(ARCH_32BIT)
# define ARCH_32BIT 0
#endif
#if !defined(ARCH_64BIT)
# define ARCH_64BIT 0
#endif
#if !defined(ARCH_X64)
# define ARCH_X64 0
#endif
#if !defined(ARCH_X86)
# define ARCH_X86 0
#endif
#if !defined(ARCH_ARM64)
# define ARCH_ARM64 0
#endif
#if !defined(ARCH_ARM32)
# define ARCH_ARM32 0
#endif
#if !defined(COMPILER_CL)
# define COMPILER_CL 0
#endif
#if !defined(COMPILER_GCC)
# define COMPILER_GCC 0
#endif
#if !defined(COMPILER_CLANG)
# define COMPILER_CLANG 0
#endif
#if !defined(OS_WINDOWS)
# define OS_WINDOWS 0
#endif
#if !defined(OS_LINUX)
# define OS_LINUX 0
#endif
#if !defined(OS_MAC)
# define OS_MAC 0
#endif
#if !defined(LANG_C)
# define LANG_C 0
#endif
#if !defined(LANG_CPP)
# define LANG_CPP 0
#endif
#if !defined(CPP_VERSION)
# define CPP_VERSION 0
#endif

#if LANG_CPP
# define ZERO_STRUCT {}
#else
# define ZERO_STRUCT {0}
#endif

#if LANG_C
# define C_LINKAGE_BEGIN
# define C_LINKAGE_END
#else
# define C_LINKAGE_BEGIN extern "C"{
# define C_LINKAGE_END }
#endif

#if COMPILER_CL
# define THREAD_LOCAL __declspec(thread)
#elif COMPILER_GCC || COMPILER_CLANG
# define THREAD_LOCAL __thread
#endif

//~/////////////////////////////////////////////////////////////////////////////
///////////////////////////// Helpers, Macros, Etc /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//~ Linkage Wrappers

#if !defined(FUNCTION)
# define FUNCTION
#endif

#if !defined(GLOBAL)
# define GLOBAL static
#endif

//~ Basic Utilities

#define Assert(c) if (!(c)) { *(volatile u64 *)0 = 0; }
#define StaticAssert(c,label) u8 static_assert_##label[(c)?(1):(-1)]
#define ArrayCount(a) (sizeof(a) / sizeof((a)[0]))

#define Min(a,b) (((a)<(b))?(a):(b))
#define Max(a,b) (((a)>(b))?(a):(b))
#define ClampBot(a,b) Max(a,b)
#define ClampTop(a,b) Min(a,b)

#define AlignPow2(x,b) (((x)+((b)-1))&(~((b)-1)))

//~ Linked List Macros

// terminator modes
#define CheckNull(p) ((p)==0)
#define SetNull(p) ((p)=0)
#define CheckNil(p) (NodeIsNil(p))
#define SetNil(p) ((p)=NilNode())

// implementations
#define QueuePush_NZ(f,l,n,next,zchk,zset) (zchk(f)?\
(f)=(l)=(n):\
((l)->next=(n),(l)=(n),zset((n)->next)))
#define QueuePop_NZ(f,l,next,zset) ((f)==(l)?\
(zset(f),zset(l)):\
((f)=(f)->next))
#define StackPush_N(f,n,next) ((n)->next=(f),(f)=(n))
#define StackPop_NZ(f,next,zchk) (zchk(f)?0:((f)=(f)->next))

#define DblPushBack_NPZ(f,l,n,next,prev,zchk,zset) \
(zchk(f)?\
((f)=(l)=(n),zset((n)->next),zset((n)->prev)):\
((n)->prev=(l),(l)->next=(n),(l)=(n),zset((n)->next)))
#define DblRemove_NPZ(f,l,n,next,prev,zchk,zset) (((f)==(n))?\
((f)=(f)->next, (zchk(f) ? (zset(l)) : zset((f)->prev))):\
((l)==(n))?\
((l)=(l)->prev, (zchk(l) ? (zset(f)) : zset((l)->next))):\
((zchk((n)->next) ? (0) : ((n)->next->prev=(n)->prev)),\
(zchk((n)->prev) ? (0) : ((n)->prev->next=(n)->next))))


// compositions
#define QueuePush(f,l,n) QueuePush_NZ(f,l,n,next,CheckNull,SetNull)
#define QueuePop(f,l)    QueuePop_NZ(f,l,next,SetNull)
#define StackPush(f,n)   StackPush_N(f,n,next)
#define StackPop(f)      StackPop_NZ(f,next,CheckNull)
#define DblPushBack(f,l,n)  DblPushBack_NPZ(f,l,n,next,prev,CheckNull,SetNull)
#define DblPushFront(f,l,n) DblPushBack_NPZ(l,f,n,prev,next,CheckNull,SetNull)
#define DblRemove(f,l,n)    DblRemove_NPZ(f,l,n,next,prev,CheckNull,SetNull)

#define NodeDblPushBack(f,l,n)  DblPushBack_NPZ(f,l,n,next,prev,CheckNil,SetNil)
#define NodeDblPushFront(f,l,n) DblPushBack_NPZ(l,f,n,prev,next,CheckNil,SetNil)
#define NodeDblRemove(f,l,n)    DblRemove_NPZ(f,l,n,next,prev,CheckNil,SetNil)


//~ Memory Operations

#define MemorySet(p,v,z)    (IMPL_Memset(p,v,z))
#define MemoryZero(p,z)     (IMPL_Memset(p,0,z))
#define MemoryZeroStruct(p) (IMPL_Memset(p,0,sizeof(*(p))))
#define MemoryCopy(d,s,z)   (IMPL_Memmove(d,s,z))

//~ sprintf
#if DEFAULT_SPRINTF
#define STB_SPRINTF_DECORATE(name) md_stbsp_##name
#define IMPL_Vsnprintf md_stbsp_vsnprintf
#include "md_stb_sprintf.h"
#endif

//~/////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Types /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//~ Basic Types

#include <stdarg.h>

#if defined(DEFAULT_BASIC_TYPES)

#include <stdint.h>
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float    f32;
typedef double   f64;

#endif

typedef i8  b8;
typedef i16 b16;
typedef i32 b32;
typedef i64 b64;

//~ Default Arena

#if DEFAULT_ARENA

typedef struct ArenaDefault ArenaDefault;
struct ArenaDefault
{
    ArenaDefault *prev;
    ArenaDefault *current;
    u64 base_pos;
    u64 pos;
    u64 cmt;
    u64 cap;
    u64 align;
};
#define IMPL_Arena ArenaDefault

#endif

//~ Abstract Arena

#if !defined(IMPL_Arena)
# error Missing implementation for IMPL_Arena
#endif

typedef IMPL_Arena Arena;

//~ Arena Helpers

typedef struct ArenaTemp ArenaTemp;
struct ArenaTemp
{
    Arena *arena;
    u64 pos;
};

//~ Basic Unicode string types.

typedef struct String8 String8;
struct String8
{
    u8 *str;
    u64 size;
};

typedef struct String16 String16;
struct String16
{
    u16 *str;
    u64 size;
};

typedef struct String32 String32;
struct String32
{
    u32 *str;
    u64 size;
};

typedef struct String8Node String8Node;
struct String8Node
{
    String8Node *next;
    String8 string;
};

typedef struct String8List String8List;
struct String8List
{
    u64 node_count;
    u64 total_size;
    String8Node *first;
    String8Node *last;
};

typedef struct StringJoin StringJoin;
struct StringJoin
{
    String8 pre;
    String8 mid;
    String8 post;
};

// NOTE(rjf): @maintenance These three flag types must not overlap.
typedef u32 MatchFlags;
typedef u32 StringMatchFlags;
typedef u32 NodeMatchFlags;
enum
{
    MatchFlag_FindLast = (1<<0),
};
enum
{
    StringMatchFlag_CaseInsensitive  = (1<<4),
    StringMatchFlag_RightSideSloppy  = (1<<5),
    StringMatchFlag_SlashInsensitive = (1<<6),
};
enum
{
    NodeMatchFlag_Tags             = (1<<16),
    NodeMatchFlag_TagArguments     = (1<<17),
    NodeMatchFlag_NodeFlags        = (1<<18),
};

typedef struct DecodedCodepoint DecodedCodepoint;
struct DecodedCodepoint
{
    u32 codepoint;
    u32 advance;
};

typedef enum IdentifierStyle
{
    IdentifierStyle_UpperCamelCase,
    IdentifierStyle_LowerCamelCase,
    IdentifierStyle_UpperCase,
    IdentifierStyle_LowerCase,
}
IdentifierStyle;

//~ Node types that are used to build all ASTs.

typedef enum NodeKind
{
    // NOTE(rjf): @maintenance Must be kept in sync with StringFromNodeKind.
    
    NodeKind_Nil,
    
    // NOTE(rjf): Generated by parser
    NodeKind_File,
    NodeKind_ErrorMarker,
    
    // NOTE(rjf): Parsed from user Metadesk code
    NodeKind_Main,
    NodeKind_Tag,
    
    // NOTE(rjf): User-created data structures
    NodeKind_List,
    NodeKind_Reference,
    
    NodeKind_COUNT,
}
NodeKind;

typedef u64 NodeFlags;
#define NodeFlag_AfterFromBefore(f) ((f) << 1)
enum
{
    // NOTE(rjf): @maintenance Must be kept in sync with StringListFromNodeFlags.
    
    // NOTE(rjf): @maintenance Because of NodeFlag_AfterFromBefore, it is
    // *required* that every single pair of "Before*" or "After*" flags be in
    // the correct order which is that the Before* flag comes first, and the
    // After* flag comes immediately after (After* being the more significant
    // bit).
    
    NodeFlag_HasParenLeft               = (1<<0),
    NodeFlag_HasParenRight              = (1<<1),
    NodeFlag_HasBracketLeft             = (1<<2),
    NodeFlag_HasBracketRight            = (1<<3),
    NodeFlag_HasBraceLeft               = (1<<4),
    NodeFlag_HasBraceRight              = (1<<5),
    
    NodeFlag_MaskSetDelimiters          = (0x3F<<0),
    
    NodeFlag_IsBeforeSemicolon          = (1<<6),
    NodeFlag_IsAfterSemicolon           = (1<<7),
    NodeFlag_IsBeforeComma              = (1<<8),
    NodeFlag_IsAfterComma               = (1<<9),
    
    NodeFlag_MaskSeperators             = (0xF<<6),
    
    NodeFlag_StringSingleQuote       = (1<<10),
    NodeFlag_StringDoubleQuote       = (1<<11),
    NodeFlag_StringTick              = (1<<12),
    NodeFlag_StringTriplet           = (1<<13),
    
    NodeFlag_MaskStringDelimiters    = (0xF<<10),
    
    NodeFlag_Numeric                 = (1<<14),
    NodeFlag_Identifier              = (1<<15),
    NodeFlag_StringLiteral           = (1<<16),
    NodeFlag_Symbol                  = (1<<17),
    
    NodeFlag_MaskLabelKind           = (0xF<<14),
};

typedef struct Node Node;
struct Node
{
    // Tree relationship data.
    Node *next;
    Node *prev;
    Node *parent;
    Node *first_child;
    Node *last_child;
    
    // Tag list.
    Node *first_tag;
    Node *last_tag;
    
    // Node info.
    NodeKind kind;
    NodeFlags flags;
    String8 string;
    String8 raw_string;
    
    // Source code location information.
    u64 offset;
    
    // Reference.
    Node *ref_target;
    
    // Comments.
    // @usage prev_comment/next_comment should be considered "hidden". Rely on
    // the functions PrevCommentFromNode/NextCommentFromNode to access
    // these. Directly access to these is likely to break in a future version.
    String8 prev_comment;
    String8 next_comment;
};

//~ Code Location Info.

typedef struct CodeLoc CodeLoc;
struct CodeLoc
{
    String8 filename;
    u32 line;
    u32 column;
};

//~ String-To-Ptr and Ptr-To-Ptr tables

typedef struct MapKey MapKey;
struct MapKey
{
    u64 hash;
    u64 size;
    void *ptr;
};

typedef struct MapSlot MapSlot;
struct MapSlot
{
    MapSlot *next;
    MapKey key;
    void *val;
};

typedef struct MapBucket MapBucket;
struct MapBucket
{
    MapSlot *first;
    MapSlot *last;
};

typedef struct Map Map;
struct Map
{
    MapBucket *buckets;
    u64 bucket_count;
};

//~ Tokens

typedef u32 TokenKind;
enum
{
    TokenKind_Identifier          = (1<<0),
    TokenKind_Numeric             = (1<<1),
    TokenKind_StringLiteral       = (1<<2),
    TokenKind_Symbol              = (1<<3),
    TokenKind_Reserved            = (1<<4),
    TokenKind_Comment             = (1<<5),
    TokenKind_Whitespace          = (1<<6),
    TokenKind_Newline             = (1<<7),
    TokenKind_BrokenComment       = (1<<8),
    TokenKind_BrokenStringLiteral = (1<<9),
    TokenKind_BadCharacter        = (1<<10),
};

typedef u32 MD_TokenGroups;
enum
{
    TokenGroup_Comment = TokenKind_Comment,
    TokenGroup_Whitespace = (TokenKind_Whitespace|
                                TokenKind_Newline),
    TokenGroup_Irregular = (TokenGroup_Comment|
                               TokenGroup_Whitespace),
    TokenGroup_Regular = ~TokenGroup_Irregular,
    TokenGroup_Label   = (TokenKind_Identifier|
                             TokenKind_Numeric|
                             TokenKind_StringLiteral|
                             TokenKind_Symbol),
    TokenGroup_Error   = (TokenKind_BrokenComment|
                             TokenKind_BrokenStringLiteral|
                             TokenKind_BadCharacter),
};

typedef struct Token Token;
struct Token
{
    TokenKind kind;
    NodeFlags node_flags;
    String8 string;
    String8 raw_string;
};

//~ Parsing State

typedef enum MessageKind
{
    // NOTE(rjf): @maintenance This enum needs to be sorted in order of
    // severity.
    MessageKind_Null,
    MessageKind_Note,
    MessageKind_Warning,
    MessageKind_Error,
    MessageKind_FatalError,
}
MessageKind;

typedef struct Message Message;
struct Message
{
    Message *next;
    Node *node;
    MessageKind kind;
    String8 string;
    void *user_ptr;
};

typedef struct MessageList MessageList;
struct MessageList
{
    MessageKind max_message_kind;
    // TODO(allen): rename
    u64 node_count;
    Message *first;
    Message *last;
};

typedef enum ParseSetRule
{
    ParseSetRule_EndOnDelimiter,
    ParseSetRule_Global,
} ParseSetRule;

typedef struct ParseResult ParseResult;
struct ParseResult
{
    Node *node;
    u64 string_advance;
    MessageList errors;
};

//~ Expression Parsing

typedef enum ExprOprKind
{
    ExprOprKind_Null,
    ExprOprKind_Prefix,
    ExprOprKind_Postfix,
    ExprOprKind_Binary,
    ExprOprKind_BinaryRightAssociative,
    ExprOprKind_COUNT,
} ExprOprKind;

typedef struct ExprOpr ExprOpr;
struct ExprOpr
{
    struct ExprOpr *next;
    u32 op_id;
    ExprOprKind kind;
    u32 precedence;
    String8 string;
    void *op_ptr;
};

typedef struct ExprOprList ExprOprList;
struct ExprOprList
{
    ExprOpr *first;
    ExprOpr *last;
    u64 count;
};

typedef struct ExprOprTable ExprOprTable;
struct ExprOprTable
{
    // TODO(mal): @upgrade_potential Hash?
    ExprOprList table[ExprOprKind_COUNT];
};

typedef struct Expr Expr;
struct Expr
{
    struct Expr *parent;
    union
    {
        struct Expr *left;
        struct Expr *unary_operand;
    };
    struct Expr *right;
    ExprOpr *op;
    Node *md_node;
};

typedef struct ExprParseResult ExprParseResult;
struct ExprParseResult
{
    Expr *expr;
    MessageList errors;
};

// TODO(allen): nil Expr

typedef struct ExprParseCtx ExprParseCtx;
struct ExprParseCtx
{
    ExprOprTable *op_table;
    
#define POSTFIX_SETLIKE_OP_COUNT 5   // (), [], {}, [), (]
    struct
    {
        ExprOpr *postfix_set_ops[POSTFIX_SETLIKE_OP_COUNT];
        NodeFlags postfix_set_flags[POSTFIX_SETLIKE_OP_COUNT];
    } accel;
#undef POSTFIX_SETLIKE_OP_COUNT
    
    MessageList errors;
};

typedef void (*BakeOperatorErrorHandler)(MessageKind kind, String8 s);

//~ String Generation Types

typedef u32 GenerateFlags;
enum
{
    GenerateFlag_Tags         = (1<<0),
    GenerateFlag_TagArguments = (1<<1),
    GenerateFlag_Children     = (1<<2),
    GenerateFlag_Comments     = (1<<3),
    GenerateFlag_NodeKind     = (1<<4),
    GenerateFlag_NodeFlags    = (1<<5),
    GenerateFlag_Location     = (1<<6),
    
    GenerateFlags_Tree = (GenerateFlag_Tags |
                             GenerateFlag_TagArguments |
                             GenerateFlag_Children),
    GenerateFlags_All  = 0xffffffff,
};

//~ Command line parsing helper types.

typedef struct CmdLineOption CmdLineOption;
struct CmdLineOption
{
    CmdLineOption *next;
    String8 name;
    String8List values;
};

typedef struct CmdLine CmdLine;
struct CmdLine
{
    String8List inputs;
    CmdLineOption *first_option;
    CmdLineOption *last_option;
};

//~ File system access types.

typedef u32 FileFlags;
enum
{
    FileFlag_Directory = (1<<0),
};

typedef struct FileInfo FileInfo;
struct FileInfo
{
    FileFlags flags;
    String8 filename;
    u64 file_size;
};

typedef struct FileIter FileIter;
struct FileIter
{
    // This is opaque state to store OS-specific file-system iteration data.
    u8 opaque[640];
};

//~/////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Functions ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//~ Arena

FUNCTION Arena*    ArenaAlloc(void);
FUNCTION void         ArenaRelease(Arena *arena);

FUNCTION void*        ArenaPush(Arena *arena, u64 size);
FUNCTION void         ArenaPutBack(Arena *arena, u64 size);
FUNCTION void         ArenaSetAlign(Arena *arena, u64 boundary);
FUNCTION void         ArenaPushAlign(Arena *arena, u64 boundary);
FUNCTION void         ArenaClear(Arena *arena);

#define PushArray(a,T,c) (T*)(ArenaPush((a), sizeof(T)*(c)))
#define PushArrayZero(a,T,c) (T*)(MemoryZero(PushArray(a,T,c), sizeof(T)*(c)))

FUNCTION ArenaTemp ArenaBeginTemp(Arena *arena);
FUNCTION void         ArenaEndTemp(ArenaTemp temp);

//~ Arena Scratch Pool

FUNCTION ArenaTemp GetScratch(Arena **conflicts, u64 count);

#define ReleaseScratch(scratch) ArenaEndTemp(scratch)

//~ Characters

FUNCTION b32 CharIsAlpha(u8 c);
FUNCTION b32 CharIsAlphaUpper(u8 c);
FUNCTION b32 CharIsAlphaLower(u8 c);
FUNCTION b32 CharIsDigit(u8 c);
FUNCTION b32 CharIsUnreservedSymbol(u8 c);
FUNCTION b32 CharIsReservedSymbol(u8 c);
FUNCTION b32 CharIsSpace(u8 c);
FUNCTION u8  CharToUpper(u8 c);
FUNCTION u8  CharToLower(u8 c);
FUNCTION u8  CharToForwardSlash(u8 c);

//~ Strings

FUNCTION u64         CalculateCStringLength(char *cstr);

FUNCTION String8     S8(u8 *str, u64 size);
#define S8CString(s)    S8((u8 *)(s), CalculateCStringLength(s))

#if LANG_C
# define S8Lit(s)        (String8){(u8 *)(s), sizeof(s)-1}
#elif LANG_CPP
# define S8Lit(s)        S8((u8*)(s), sizeof(s) - 1)
#endif
#define S8LitComp(s)     {(u8 *)(s), sizeof(s)-1}

#if CPP_VERSION >= 11
static inline String8
operator "" _md(const char *s, size_t size)
{
    String8 str = S8((u8 *)s, (u64)size);
    return str;
}
#endif

FUNCTION String8     S8Range(u8 *first, u8 *opl);

FUNCTION String8     S8Substring(String8 str, u64 min, u64 max);
FUNCTION String8     S8Skip(String8 str, u64 min);
FUNCTION String8     S8Chop(String8 str, u64 nmax);
FUNCTION String8     S8Prefix(String8 str, u64 size);
FUNCTION String8     S8Suffix(String8 str, u64 size);

FUNCTION b32         S8Match(String8 a, String8 b, MatchFlags flags);
FUNCTION u64         S8FindSubstring(String8 str, String8 substring,
                                              u64 start_pos, MatchFlags flags);

FUNCTION String8     S8Copy(Arena *arena, String8 string);
FUNCTION String8     S8FmtV(Arena *arena, char *fmt, va_list args);

FUNCTION String8     S8Fmt(Arena *arena, char *fmt, ...);

#define S8VArg(s) (int)(s).size, (s).str

FUNCTION void           S8ListPush(Arena *arena, String8List *list,
                                         String8 string);
FUNCTION void           S8ListPushFmt(Arena *arena, String8List *list,
                                            char *fmt, ...);

FUNCTION void           S8ListConcat(String8List *list, String8List *to_push);
FUNCTION String8List S8Split(Arena *arena, String8 string, int split_count,
                                      String8 *splits);
FUNCTION String8     S8ListJoin(Arena *arena, String8List list,
                                         StringJoin *join);

FUNCTION String8     S8Stylize(Arena *arena, String8 string,
                                        IdentifierStyle style, String8 separator);

//~ Unicode Conversions

FUNCTION DecodedCodepoint DecodeCodepointFromUtf8(u8 *str, u64 max);
FUNCTION DecodedCodepoint DecodeCodepointFromUtf16(u16 *str, u64 max);
FUNCTION u32         Utf8FromCodepoint(u8 *out, u32 codepoint);
FUNCTION u32         Utf16FromCodepoint(u16 *out, u32 codepoint);
FUNCTION String8     S8FromS16(Arena *arena, String16 str);
FUNCTION String16    S16FromS8(Arena *arena, String8 str);
FUNCTION String8     S8FromS32(Arena *arena, String32 str);
FUNCTION String32    S32FromS8(Arena *arena, String8 str);

//~ String Skipping/Chopping Helpers

// This is intended for removing extensions.
FUNCTION String8 PathChopLastPeriod(String8 string);

// This is intended for removing everything but the filename.
FUNCTION String8 PathSkipLastSlash(String8 string);

// This is intended for getting an extension from a filename.
FUNCTION String8 PathSkipLastPeriod(String8 string);

// This is intended for getting the folder string from a full path.
FUNCTION String8 PathChopLastSlash(String8 string);

FUNCTION String8 S8SkipWhitespace(String8 string);
FUNCTION String8 S8ChopWhitespace(String8 string);

//~ Numeric Strings

FUNCTION b32     StringIsU64(String8 string, u32 radix);
FUNCTION b32     StringIsCStyleInt(String8 string);

FUNCTION u64     U64FromString(String8 string, u32 radix);
FUNCTION i64     CStyleIntFromString(String8 string);
FUNCTION f64     F64FromString(String8 string);

FUNCTION String8 CStyleHexStringFromU64(Arena *arena, u64 x, b32 caps);

//~ Enum/Flag Strings

FUNCTION String8     StringFromNodeKind(NodeKind kind);
FUNCTION String8List StringListFromNodeFlags(Arena *arena, NodeFlags flags);

//~ Map Table Data Structure

FUNCTION u64 HashStr(String8 string);
FUNCTION u64 HashPtr(void *p);

FUNCTION Map      MapMakeBucketCount(Arena *arena, u64 bucket_count);
FUNCTION Map      MapMake(Arena *arena);
FUNCTION MapKey   MapKeyStr(String8 string);
FUNCTION MapKey   MapKeyPtr(void *ptr);
FUNCTION MapSlot* MapLookup(Map *map, MapKey key);
FUNCTION MapSlot* MapScan(MapSlot *first_slot, MapKey key);
FUNCTION MapSlot* MapInsert(Arena *arena, Map *map, MapKey key, void *val);
FUNCTION MapSlot* MapOverwrite(Arena *arena, Map *map, MapKey key,
                                        void *val);

//~ Parsing

FUNCTION Token       TokenFromString(String8 string);
FUNCTION u64         LexAdvanceFromSkips(String8 string, TokenKind skip_kinds);
FUNCTION ParseResult ParseResultZero(void);
FUNCTION ParseResult ParseNodeSet(Arena *arena, String8 string, u64 offset, Node *parent,
                                           ParseSetRule rule);
FUNCTION ParseResult ParseOneNode(Arena *arena, String8 string, u64 offset);
FUNCTION ParseResult ParseWholeString(Arena *arena, String8 filename, String8 contents);

FUNCTION ParseResult ParseWholeFile(Arena *arena, String8 filename);

//~ Messages (Errors/Warnings)

FUNCTION Node*   MakeErrorMarkerNode(Arena *arena, String8 parse_contents,
                                              u64 offset);

FUNCTION Message*MakeNodeError(Arena *arena, Node *node,
                                        MessageKind kind, String8 str);
FUNCTION Message*MakeDetachedError(Arena *arena, MessageKind kind,
                                            String8 str, void *ptr);
FUNCTION Message*MakeTokenError(Arena *arena, String8 parse_contents,
                                         Token token, MessageKind kind,
                                         String8 str);

FUNCTION void       MessageListPush(MessageList *list, Message *message);
FUNCTION void       MessageListConcat(MessageList *list, MessageList *to_push);

//~ Location Conversion

FUNCTION CodeLoc CodeLocFromFileOffset(String8 filename, u8 *base, u64 offset);
FUNCTION CodeLoc CodeLocFromNode(Node *node);

//~ Tree/List Building

FUNCTION b32   NodeIsNil(Node *node);
FUNCTION Node *NilNode(void);
FUNCTION Node *MakeNode(Arena *arena, NodeKind kind, String8 string,
                                 String8 raw_string, u64 offset);
FUNCTION void     PushChild(Node *parent, Node *new_child);
FUNCTION void     PushTag(Node *node, Node *tag);

FUNCTION Node *MakeList(Arena *arena);
FUNCTION void     ListConcatInPlace(Node *list, Node *to_push);
FUNCTION Node *PushNewReference(Arena *arena, Node *list, Node *target);

//~ Introspection Helpers

// These calls are for getting info from nodes, and introspecting
// on trees that are returned to you by the parser.

FUNCTION Node *  FirstNodeWithString(Node *first, String8 string, MatchFlags flags);
FUNCTION Node *  NodeAtIndex(Node *first, int n);
FUNCTION Node *  FirstNodeWithFlags(Node *first, NodeFlags flags);
FUNCTION int        IndexFromNode(Node *node);
FUNCTION Node *  RootFromNode(Node *node);
FUNCTION Node *  MD_ChildFromString(Node *node, String8 child_string, MatchFlags flags);
FUNCTION Node *  TagFromString(Node *node, String8 tag_string, MatchFlags flags);
FUNCTION Node *  ChildFromIndex(Node *node, int n);
FUNCTION Node *  TagFromIndex(Node *node, int n);
FUNCTION Node *  TagArgFromIndex(Node *node, String8 tag_string, MatchFlags flags, int n);
FUNCTION Node *  TagArgFromString(Node *node, String8 tag_string, MatchFlags tag_str_flags, String8 arg_string, MatchFlags arg_str_flags);
FUNCTION b32     NodeHasChild(Node *node, String8 string, MatchFlags flags);
FUNCTION b32     NodeHasTag(Node *node, String8 string, MatchFlags flags);
FUNCTION i64     ChildCountFromNode(Node *node);
FUNCTION i64     TagCountFromNode(Node *node);
FUNCTION Node *  ResolveNodeFromReference(Node *node);
FUNCTION Node*   NodeNextWithLimit(Node *node, Node *opl);

FUNCTION String8 PrevCommentFromNode(Node *node);
FUNCTION String8 NextCommentFromNode(Node *node);

// NOTE(rjf): For-Loop Helpers
#define EachNode(it, first) Node *it = (first); !NodeIsNil(it); it = it->next

//~ Error/Warning Helpers

FUNCTION String8 StringFromMessageKind(MessageKind kind);

#define FmtCodeLoc "%.*s:%i:%i:"
#define CodeLocVArg(loc) S8VArg((loc).filename), (loc).line, (loc).column

FUNCTION String8 MD_FormatMessage(Arena *arena, CodeLoc loc, MessageKind kind,
                                        String8 string);

#if !DISABLE_PRINT_HELPERS
#include <stdio.h>
FUNCTION void PrintMessage(FILE *file, CodeLoc loc, MessageKind kind,
                                 String8 string);
FUNCTION void PrintMessageFmt(FILE *file, CodeLoc code_loc, MessageKind kind,
                                    char *fmt, ...);

#define PrintGenNoteCComment(f) fprintf((f), "// generated by %s:%d\n", __FILE__, __LINE__)
#endif

//~ Tree Comparison/Verification

FUNCTION b32 NodeMatch(Node *a, Node *b, MatchFlags flags);
FUNCTION b32 NodeDeepMatch(Node *a, Node *b, MatchFlags flags);

//~ Expression Parsing

FUNCTION void               ExprOprPush(Arena *arena, ExprOprList *list,
                                              ExprOprKind kind, u64 precedence,
                                              String8 op_string,
                                              u32 op_id, void *op_ptr);

FUNCTION ExprOprTable    ExprBakeOprTableFromList(Arena *arena,
                                                           ExprOprList *list);
FUNCTION ExprOpr*        ExprOprFromKindString(ExprOprTable *table,
                                                        ExprOprKind kind, String8 s);

FUNCTION ExprParseResult ExprParse(Arena *arena, ExprOprTable *op_table,
                                            Node *first, Node *one_past_last);

FUNCTION Expr* Expr_NewLeaf(Arena *arena, Node *node);
FUNCTION Expr* Expr_NewOpr(Arena *arena, ExprOpr *op, Node *op_node,
                                    Expr *left, Expr *right);

FUNCTION ExprParseCtx ExprParse_MakeContext(ExprOprTable *table);

FUNCTION Expr* ExprParse_TopLevel(Arena *arena, ExprParseCtx *ctx,
                                           Node *first, Node *opl);
FUNCTION b32   ExprParse_OprConsume(ExprParseCtx *ctx,
                                             Node **iter, Node *opl,
                                             ExprOprKind kind,
                                             u32 min_precedence,
                                             ExprOpr **op_out);
FUNCTION Expr* ExprParse_Atom(Arena *arena, ExprParseCtx *ctx,
                                       Node **iter, Node *first, Node *opl);
FUNCTION Expr* ExprParse_MinPrecedence(Arena *arena, ExprParseCtx *ctx,
                                                Node **iter, Node *first, Node *opl,
                                                u32 min_precedence);


//~ String Generation

FUNCTION void DebugDumpFromNode(Arena *arena, String8List *out, Node *node,
                                      int indent, String8 indent_string,
                                      GenerateFlags flags);
FUNCTION void ReconstructionFromNode(Arena *arena, String8List *out, Node *node,
                                           int indent, String8 indent_string);

//~ Command Line Argument Helper

FUNCTION String8List StringListFromArgCV(Arena *arena, int argument_count,
                                                  char **arguments);
FUNCTION CmdLine MakeCmdLineFromOptions(Arena *arena, String8List options);
FUNCTION String8List CmdLineValuesFromString(CmdLine cmdln, String8 name);
FUNCTION b32 CmdLineB32FromString(CmdLine cmdln, String8 name);
FUNCTION i64 CmdLineI64FromString(CmdLine cmdln, String8 name);

//~ File System

FUNCTION String8  LoadEntireFile(Arena *arena, String8 filename);
FUNCTION b32      FileIterBegin(FileIter *it, String8 path);
FUNCTION FileInfo FileIterNext(Arena *arena, FileIter *it);
FUNCTION void        FileIterEnd(FileIter *it);

#endif // MD_H

/*
Copyright 2021 Dion Systems LLC

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
