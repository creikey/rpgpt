#include <stdio.h>
#include <stdbool.h>

#define assert(cond, explanation) { if(!cond) { printf("Codegen assertion line %d %s failed: %.*s\n", __LINE__, #cond, MD_S8VArg(explanation)); __debugbreak(); exit(1); } }

#pragma warning(disable : 4996) // nonsense about fopen being insecure

#pragma warning(push)
#pragma warning(disable : 4244) // loss of data warning
#pragma warning(disable : 4101) // unreferenced local variable
#include "md.h"
#include "md.c"
#pragma warning(pop)

#include "jsmn.h"


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

MD_Arena *cg_arena = NULL;

static bool jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return true;
  }
  return false;
}

static int getint(const char *json, jsmntok_t *tok) {
    assert(tok->type == JSMN_PRIMITIVE, MD_S8Lit("not primitive"));
    char tmp[256] = {0};
    strncpy(tmp, json + tok->start, tok->end - tok->start);
    return atoi(tmp);
}

// will look over into random memory if can't find...
static int getnextofname(const char *json, jsmntok_t *starting_at, const char *name) {
    jsmntok_t *cur = starting_at;
    // max lookahead, bad
    for(int i = 0; i < 1024*10; i++) {
        if(jsoneq(json, cur, name)) {
            return getint(json, cur+1);
        }
        cur++;
    }
    assert(false, MD_S8Fmt(cg_arena, "Could not find integer for %s\n", name));
    return -1;
}


MD_String8 ChildValue(MD_Node *n, MD_String8 name) {
    MD_Node *child_with_value = MD_ChildFromString(n, name, 0);
    assert(child_with_value, MD_S8Fmt(cg_arena, "Could not find child named '%.*s' of node '%.*s'", MD_S8VArg(name), MD_S8VArg(n->string)));
    assert(child_with_value->first_child, MD_S8Lit("Must have child"));
    return child_with_value->first_child->string;
}

MD_String8 asset_file_path(MD_String8 filename) {
    return MD_S8Fmt(cg_arena, "%.*s/%.*s", MD_S8VArg(ASSETS_FOLDER), MD_S8VArg(filename));
}

int main(int argc, char **argv) {
    cg_arena = MD_ArenaAlloc();
    assert(cg_arena, MD_S8Lit("Memory"));

    // I hope to God MD_String8's are null terminated...
    MD_String8 writeto = MD_S8Fmt(cg_arena, "%.*s/assets.gen.c", MD_S8VArg(OUTPUT_FOLDER));
    log("Writing to %.*s\n", MD_S8VArg(writeto));
    FILE *output = fopen(writeto.str, "w");

    MD_ParseResult parse = MD_ParseWholeFile(cg_arena, MD_S8Lit("assets.mdesk"));

    //dump(parse);

    MD_String8List declarations_list = {0};
    MD_String8List load_list = {0};
    MD_String8List level_decl_list = {0};
    for(MD_EachNode(node, parse.node->first_child)) {
        if(MD_S8Match(node->first_tag->string, MD_S8Lit("image"), 0)) {
            MD_String8 variable_name = MD_S8Fmt(cg_arena, "image_%.*s", MD_S8VArg(node->string));
            log("New image variable %.*s\n", MD_S8VArg(variable_name));
            MD_String8 filepath = ChildValue(node, MD_S8Lit("filepath"));
            filepath = asset_file_path(filepath);
            assert(filepath.str != 0, MD_S8Fmt(cg_arena, "No filepath specified for image '%.*s'", MD_S8VArg(node->string)));
            FILE *asset_file = fopen(filepath.str, "r");
            assert(asset_file, MD_S8Fmt(cg_arena, "Could not open filepath %.*s for asset '%.*s'", MD_S8VArg(filepath), MD_S8VArg(node->string)));
            fclose(asset_file);

            MD_S8ListPush(cg_arena, &declarations_list, MD_S8Fmt(cg_arena, "sg_image %.*s = {0};\n", MD_S8VArg(variable_name)));
            MD_S8ListPush(cg_arena, &load_list, MD_S8Fmt(cg_arena, "%.*s = load_image(\"%.*s\");\n", MD_S8VArg(variable_name), MD_S8VArg(filepath)));
        }
        if(MD_S8Match(node->first_tag->string, MD_S8Lit("level"), 0)) {
            MD_String8 variable_name = MD_S8Fmt(cg_arena, "level_%.*s", MD_S8VArg(node->string));
            MD_String8 filepath = asset_file_path(ChildValue(node, MD_S8Lit("filepath")));
            MD_String8 level_file_contents = MD_LoadEntireFile(cg_arena, filepath);
            assert(level_file_contents.str != 0 && level_file_contents.size > 1, MD_S8Lit("Failed to load level file"));
            jsmn_parser p;
            jsmntok_t t[1024*5];
            jsmn_init(&p);
            const char *json_string = level_file_contents.str;
            int r = jsmn_parse(&p, json_string, level_file_contents.size, t, sizeof(t) / sizeof(t[0]));
            assert(r > 0, MD_S8Lit("Failed to parse json"));

            int layer_obj_token = 0;
            fprintf(output, "Level %.*s = {\n.tiles = {\n", MD_S8VArg(variable_name));
            // this code is absolutely disgusting but I think it's fine because this is meta code, I guess time will tell if this needs to be cleaner or not... sorry future self!
            for(int i = 0; i < r; i++) {
                if(jsoneq(json_string, &t[i], "data")) {
                    jsmntok_t *arr = &t[i+1];
                    assert(arr->type == JSMN_ARRAY, MD_S8Lit("Expected array after data"));
                    int width = getnextofname(json_string, &t[i+1], "width");
                    int height = getnextofname(json_string, &t[i+1], "height");

                    fprintf(output, "{ ");
                    int last_token_index = i + 2 + arr->size;
                    for(int ii = i + 2; ii < last_token_index; ii++) {
                        jsmntok_t *num_to_add = &t[ii];
                        assert(num_to_add->type == JSMN_PRIMITIVE, MD_S8Lit("not a number"));
                        int num_index = ii - (i + 2);
                        fprintf(output, "%.*s, ", num_to_add->end - num_to_add->start, json_string + num_to_add->start);
                        if(num_index % width == width - 1) {
                            if(ii == last_token_index - 1) {
                                fprintf(output, "},\n}\n}; // %.*s\n", MD_S8VArg(variable_name));
                            } else {
                                fprintf(output, "},\n{ ");
                            }
                        }
                    }
                }
            }

        }
    }

    MD_StringJoin join = MD_ZERO_STRUCT;
    MD_String8 declarations = MD_S8ListJoin(cg_arena, declarations_list, &join);
    MD_String8 loads = MD_S8ListJoin(cg_arena, load_list, &join);
    fprintf(output, "%.*s\nvoid load_assets() {\n%.*s\n}", MD_S8VArg(declarations), MD_S8VArg(loads));

    fclose(output);
    return 0;
}
