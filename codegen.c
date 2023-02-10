#include <stdio.h>

#define assert(cond, explanation) { if(!cond) { printf("Codegen assertion %s failed: %.*s\n", #cond, MD_S8VArg(explanation)); exit(1); } }

#pragma warning(disable : 4996) // nonsense about fopen being insecure

#pragma warning(push)
#pragma warning(disable : 4244) // loss of data warning
#pragma warning(disable : 4101) // unreferenced local variable
#include "md.h"
#include "md.c"
#pragma warning(pop)


MD_String8 OUTPUT_FOLDER = MD_S8LitComp("gen"); // no trailing slash
MD_String8 ASSETS_FOLDER = MD_S8LitComp("assets");

#define log(...) { printf("Codegen: "); printf(__VA_ARGS__); }

void dump(MD_ParseResult parse) {
    // Iterate through each top-level node
    for(MD_EachNode(node, parse.node->first_child))
    {
        printf("/ %.*s\n", MD_S8VArg(node->string));

        // Print the name of each of the node's tags
        for(MD_EachNode(tag, node->first_tag))
        {
            printf("|-- Tag %.*s\n", MD_S8VArg(tag->string));
        }

        // Print the name of each of the node's children
        for(MD_EachNode(child, node->first_child))
        {
            printf("|-- Child %.*s\n", MD_S8VArg(child->string));
        }
    }
}

int main(int argc, char **argv) {
    MD_Arena *cg_arena = MD_ArenaAlloc();
    assert(cg_arena, MD_S8Lit("Memory"));

    // I hope to God MD_String8's are null terminated...
    MD_String8 writeto = MD_S8Fmt(cg_arena, "%.*s/assets.gen.c", MD_S8VArg(OUTPUT_FOLDER));
    log("Writing to %.*s\n", MD_S8VArg(writeto));
    FILE *output = fopen(writeto.str, "w");

    MD_ParseResult parse = MD_ParseWholeFile(cg_arena, MD_S8Lit("assets.mdesk"));

    //dump(parse);

    MD_String8List declarations_list = {0};
    MD_String8List load_list = {0};
    for(MD_EachNode(node, parse.node->first_child)) {
        if(MD_S8Match(node->first_tag->string, MD_S8Lit("image"), 0)) {
            MD_String8 variable_name = MD_S8Fmt(cg_arena, "image_%.*s", MD_S8VArg(node->string));
            log("New image variable %.*s\n", MD_S8VArg(variable_name));
            MD_String8 filepath = {0};
            for(MD_EachNode(child, node->first_child)) {
                if(MD_S8Match(child->string, MD_S8Lit("filepath"), 0)) {
                    filepath = child->next->next->string;
                }
            }
            filepath = MD_S8Fmt(cg_arena, "%.*s/%.*s", MD_S8VArg(ASSETS_FOLDER), MD_S8VArg(filepath));
            assert(filepath.str != 0, MD_S8Fmt(cg_arena, "No filepath specified for image '%.*s'", MD_S8VArg(node->string)));
            FILE *asset_file = fopen(filepath.str, "r");
            assert(asset_file, MD_S8Fmt(cg_arena, "Could not open filepath %.*s for asset '%.*s'", MD_S8VArg(filepath), MD_S8VArg(node->string)));
            fclose(asset_file);

            MD_S8ListPush(cg_arena, &declarations_list, MD_S8Fmt(cg_arena, "sg_image %.*s = {0};\n", MD_S8VArg(variable_name)));
            MD_S8ListPush(cg_arena, &load_list, MD_S8Fmt(cg_arena, "%.*s = load_image(\"%.*s\");\n", MD_S8VArg(variable_name), MD_S8VArg(filepath)));
        }
    }

    MD_StringJoin join = MD_ZERO_STRUCT;
    MD_String8 declarations = MD_S8ListJoin(cg_arena, declarations_list, &join);
    MD_String8 loads = MD_S8ListJoin(cg_arena, load_list, &join);
    fprintf(output, "%.*s\nvoid load_assets() {\n%.*s\n}", MD_S8VArg(declarations), MD_S8VArg(loads));

    fclose(output);
    return 0;
}
