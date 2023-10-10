#include <stdio.h>

#pragma warning(disable : 4820) // don't care about padding
#pragma warning(disable : 4388) // signed/unsigned mismatch probably doesn't matter
// #pragma warning(disable : 4100) // unused local doesn't matter, because sometimes I want to kee
#pragma warning(disable : 4053) // void operands are used for tricks like applying printf linting to non printf function calls
#pragma warning(disable : 4255) // strange no function prototype given thing?
#pragma warning(disable : 4456) // I shadow local declarations often and it's fine
#pragma warning(disable : 4061) // I don't need to *explicitly* handle everything, having a default: case should mean no more warnings
#pragma warning(disable : 4201) // nameless struct/union occurs
#pragma warning(disable : 4366) // I think unaligned memory addresses are fine to ignore
#pragma warning(disable : 4459) // Local function decl hiding global declaration I think is fine
#pragma warning(disable : 5045) // spectre mitigation??
#pragma warning(disable : 4244) // loss of data warning
#pragma warning(disable : 4101) // unreferenced local variable
#pragma warning(disable : 4100) // unreferenced local variable again?
#pragma warning(disable : 4189) // initialized and not referenced
#pragma warning(disable : 4242) // conversion
#pragma warning(disable : 4457) // hiding function variable happens
#pragma warning(disable : 4668) // __GNU_C__ macro undefined, fixing
#pragma warning(disable : 4996) // fopen is safe. I don't care about fopen_s
#include "md.h"
#include "md.c"

#define DESKTOP
#define WINDOWS
#define RND_IMPLEMENTATION
#include "makeprompt.h"

#include <windows.h> // for sleep.

typedef struct Error {
    struct Error *next, *prev;
    String8 message;
    int line;
} Error;

typedef struct ErrorList {
    int count;
    Error *first, *last;
} ErrorList;

void ErrorPush(Arena *arena, ErrorList *list, Error message)
{
    Error *new_err = PushArrayZero(arena, Error, 1);
    *new_err = message;
    
    QueuePush(list->first, list->last, new_err);
    list->count += 1;
}

void error_impl(Arena *arena, ErrorList *errors, int line_in_toparse, String8 message)
{
    ErrorPush(arena, errors, (Error){.line = line_in_toparse, .message = message});
    // this is a function so you can breakpoint here and discover when errors occur
}

#define error(line_in_toparse, fmt_str, ...) error_impl(arena, errors, line_in_toparse, S8Fmt(arena, fmt_str, __VA_ARGS__))

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    Arena *arena = ArenaAlloc();
    Sleep(200); // have to wait for console to pop up, super annoying

    // Prose is the name for this file format where you describe the souls of characters

    // tokenize
    typedef struct ProseToken
    {
        struct ProseToken *next, *prev;
        String8 field;
        int field_number; // this is -1 if no field_number, e.g if 'Field Text #0:' isn't specified and had no '#', then this would be -1
        String8 value; // may be an empty string, if it's trying to be like, an object
        int indentation;
        int line;
    } ProseToken;

    ErrorList errors_lit = {0};
    ErrorList *errors = &errors_lit;

    Npc out = {0};
    // all arena allocations done from here are temporary. As it just copies data into Npc
    // parse 'playground.txt' into 'out'
    {
        // read the file
        String8 to_parse = LoadEntireFile(arena, S8Lit("playground.txt"));

        // tokenize to_parse
        ProseToken *tokenized_first = 0;
        ProseToken *tokenized_last = 0;
        {
            String8List as_lines = S8Split(arena, to_parse, 1, &S8Lit("\n"));
            int line = 1; // lines start at 1
            for (String8Node *cur = as_lines.first; cur; cur = cur->next)
            {
                int indentation = 0;
                while(indentation < cur->string.size && cur->string.str[indentation] == '\t') indentation += 1;

                String8 no_funny_business = S8SkipWhitespace(S8ChopWhitespace(cur->string));
                if(no_funny_business.size == 0) continue;

                String8List along_colon = S8Split(arena, no_funny_business, 1, &S8Lit(":"));
                if(along_colon.node_count != 2 && along_colon.node_count != 1) {
                    error(line, "Requires exactly one ':' on the line to delimit the field and value. Got %d", along_colon.node_count - 1);
                } else {
                    ProseToken *token_out = PushArrayZero(arena, ProseToken, 1);
                    token_out->field_number = -1;
                    if(along_colon.node_count == 2)
                        token_out->value = along_colon.last->string;
                    token_out->line = line;
                    token_out->indentation = indentation;

                    DblPushBack(tokenized_first, tokenized_last, token_out);
                }
                line += 1;
            }
        }
    }

    if (errors->count > 0)
    {
        printf("Failed with errors:\n");
        for (Error *cur = errors->first; cur; cur = cur->next)
        {
            printf("On line %d of input: %.*s\n", cur->line, S8VArg(cur->message));
        }
        assert(false);
    }
    printf("Success.\n");
    __debugbreak();
}