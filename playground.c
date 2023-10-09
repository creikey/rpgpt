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

void error_impl(Arena *arena, String8List *errors, String8 message)
{
    S8ListPush(arena, errors, message);
    // this is a function so you can breakpoint here and discover when errors occur
}

#define error(fmt_str, ...) error_impl(arena, errors, S8Fmt(arena, fmt_str, __VA_ARGS__))

// Allows you to not need to quote children of a parent.
String8 all_children_as_string(Arena *arena, Node *parent)
{
    String8List children = {0};
    for (Node *cur = parent->first_child; !NodeIsNil(cur); cur = cur->next)
    {
        S8ListPush(arena, &children, cur->string);
    }
    return S8ListJoin(arena, children, &(StringJoin){.mid = S8Lit(" ")});
}

Node *get_child_should_exist(Arena *arena, String8List *errors, Node *parent, String8 child_name)
{
    if (errors->node_count > 0)
        return NilNode();

    Node *child = MD_ChildFromString(parent, child_name, StringMatchFlag_CaseInsensitive);
    if (NodeIsNil(child))
    {
        error("Couldn't find child with name '%.*s' on node '%.*s'", S8VArg(child_name), S8VArg(parent->string));
    }
    return child;
}

typedef struct {

}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    Arena *arena = ArenaAlloc();
    ParseResult result = ParseWholeFile(arena, S8Lit("playground.mdesk"));
    if (result.errors.node_count > 0)
    {
        printf("Failed to parse file:\n");
        for (Message *cur = result.errors.first; cur->next; cur = cur->next)
        {
            printf("%.*s\n", S8VArg(cur->string));
        }
    }
    else
    {
        Node *node = result.node;
        String8List errors_list = {0};
        String8List *errors = &errors_list;

        Npc out;
        chunk_from_s8(&out.name, all_children_as_string(arena, get_child_should_exist(arena, errors, node, S8Lit("name"))));

        if (errors->node_count == 0)
        {
            // unit testing asserts
            assert(S8Match(TextChunkString8(out.name), S8Lit("Roger Penrose"), 0));
        }
        else
        {
            printf("Corrupt character soul:\n");
            for (String8Node *cur = errors->first; cur->next; cur = cur->next)
            {
                printf("%.*s\n", S8VArg(cur->string));
            }
            assert(false);
        }

    }

    printf("Success.\n");
    __debugbreak();
}