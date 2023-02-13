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


MD_String8 OUTPUT_FOLDER = MD_S8LitComp("gen"); // no trailing slash
MD_String8 ASSETS_FOLDER = MD_S8LitComp("assets");

#define log(...) { printf("Codegen: "); printf(__VA_ARGS__); }

void dump(MD_Node* from) {
    printf("/ %.*s\n", MD_S8VArg(from->string));
    int d = 0;
    for(MD_EachNode(child, from->first_child))
    {
        printf("|-- Child %d %.*s\n", d, MD_S8VArg(child->string));
        d += 1;
    }
}

void dump_root(MD_Node* from) {
    // Iterate through each top-level node
    for(MD_EachNode(node, from->first_child))
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

MD_String8 ChildValue(MD_Node *n, MD_String8 name) {
    MD_Node *child_with_value = MD_ChildFromString(n, name, 0);
    assert(child_with_value, MD_S8Fmt(cg_arena, "Could not find child named '%.*s' of node '%.*s'", MD_S8VArg(name), MD_S8VArg(n->string)));
    assert(child_with_value->first_child, MD_S8Lit("Must have child"));
    return child_with_value->first_child->string;
}

MD_String8 asset_file_path(MD_String8 filename) {
    return MD_S8Fmt(cg_arena, "%.*s/%.*s", MD_S8VArg(ASSETS_FOLDER), MD_S8VArg(filename));
}

char *nullterm(MD_String8 s) {
    char *to_return = malloc(s.size + 1);
    memcpy(to_return, s.str, s.size);
    to_return[s.size] = '\0';
    return to_return;
}

char* fillnull(char *s, char c) {
    while(*s != '\0') {
        if(*s == c) {
            *s = '\0';
            return s + 1;
        }
        s++;
    }
    assert(false, MD_S8Lit("Couldn't find char"));
    return NULL;
}

char* goto_end_of(char *tomove, size_t max_move, char *pattern) {
    size_t pattern_len = strlen(pattern);
    for(int i = 0; i < max_move; i++) {
        if(strncmp(tomove, pattern, pattern_len) == 0) {
            tomove += pattern_len;
            return tomove;
        }
        tomove++;
    }
    return NULL;
}

#define list_printf(list_ptr, ...) MD_S8ListPush(cg_arena, list_ptr, MD_S8Fmt(cg_arena, __VA_ARGS__))


int main(int argc, char **argv) {
    cg_arena = MD_ArenaAlloc();
    assert(cg_arena, MD_S8Lit("Memory"));

    char *nulled = nullterm(MD_S8Lit("test"));

    // I hope to God MD_String8's are null terminated...
    MD_String8 writeto = MD_S8Fmt(cg_arena, "%.*s/assets.gen.c", MD_S8VArg(OUTPUT_FOLDER));
    log("Writing to %.*s\n", MD_S8VArg(writeto));
    FILE *output = fopen(writeto.str, "w");

    MD_ParseResult parse = MD_ParseWholeFile(cg_arena, MD_S8Lit("assets.mdesk"));

    //dump(parse.node);

    MD_String8List declarations_list = {0};
    MD_String8List load_list = {0};
    MD_String8List level_decl_list = {0};
    MD_String8List tileset_decls = {0};
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
        if(MD_S8Match(node->first_tag->string, MD_S8Lit("tileset"), 0)) {
            MD_String8 variable_name = MD_S8Fmt(cg_arena, "tileset_%.*s", MD_S8VArg(node->string));
            log("New tileset %.*s\n", MD_S8VArg(variable_name));
            MD_String8 filepath = asset_file_path(ChildValue(node, MD_S8Lit("filepath")));

            MD_String8 tileset_file_contents = MD_LoadEntireFile(cg_arena, filepath);
            list_printf(&tileset_decls, "TileSet %.*s = {\n", MD_S8VArg(variable_name));
            list_printf(&tileset_decls, ".img = &%.*s,\n", MD_S8VArg(ChildValue(node, MD_S8Lit("image"))));

            list_printf(&tileset_decls, ".animated = {\n");
            char *end = tileset_file_contents.str + tileset_file_contents.size;
            char *cur = tileset_file_contents.str;
            while(cur < end) {
                cur = goto_end_of(cur, end - cur, "<tile id=\""); 
                if(cur == NULL) break;
                char *new_cur = fillnull(cur, '"');
                int frame_from = atoi(cur);
                cur = new_cur;
                list_printf(&tileset_decls, "{ .id_from = %d, .frames = { ", frame_from);

                char *end_of_anim = goto_end_of(cur, end - cur, "</animation>");

                int num_frames = 0;
                while(true) {
                    char *next_frame = goto_end_of(cur, end - cur, "<frame tileid=\"");
                    if(next_frame == NULL || next_frame > end_of_anim) break;
                    char *new_cur = fillnull(next_frame, '"');
                    int frame = atoi(next_frame);
                        
                    list_printf(&tileset_decls, "%d, ", frame);
                    num_frames++;

                    cur = new_cur;
                }
                list_printf(&tileset_decls, "}, .num_frames = %d },\n", num_frames);
            }
            list_printf(&tileset_decls, "}};\n");
        }
        if(MD_S8Match(node->first_tag->string, MD_S8Lit("level"), 0)) {
            MD_String8 variable_name = MD_S8Fmt(cg_arena, "level_%.*s", MD_S8VArg(node->string));
            log("New level variable %.*s\n", MD_S8VArg(variable_name));
            MD_String8 filepath = asset_file_path(ChildValue(node, MD_S8Lit("filepath")));
            MD_ParseResult level_parse = MD_ParseWholeFile(cg_arena, filepath);
            assert(level_parse.node != 0, MD_S8Lit("Failed to load level file"));

            MD_Node *layers = MD_ChildFromString(level_parse.node->first_child, MD_S8Lit("layers"), 0);
            fprintf(output, "Level %.*s = {\n", MD_S8VArg(variable_name));
            for(MD_EachNode(lay, layers->first_child)) {
                MD_String8 type = MD_ChildFromString(lay, MD_S8Lit("type"), 0)->first_child->string;
                if(MD_S8Match(type, MD_S8Lit("objectgroup"), 0)) {
                    for(MD_EachNode(object, MD_ChildFromString(lay, MD_S8Lit("objects"), 0)->first_child)) {
                        dump(object);
                        if(MD_S8Match(MD_ChildFromString(object, MD_S8Lit("name"), 0)->first_child->string, MD_S8Lit("spawn"), 0)) {
                            MD_String8 x_string = MD_ChildFromString(object, MD_S8Lit("x"), 0)->first_child->string;
                            MD_String8 y_string = MD_ChildFromString(object, MD_S8Lit("y"), 0)->first_child->string;
                            fprintf(output, ".spawnpoint = { %.*s, %.*s },\n", MD_S8VArg(x_string), MD_S8VArg(y_string));
                        }
                    }
                }
                if(MD_S8Match(type, MD_S8Lit("tilelayer"), 0)) {
                    int width = atoi(nullterm(MD_ChildFromString(layers->first_child, MD_S8Lit("width"), 0)->first_child->string));
                    int height = atoi(nullterm(MD_ChildFromString(layers->first_child, MD_S8Lit("height"), 0)->first_child->string));
                    MD_Node *data = MD_ChildFromString(layers->first_child, MD_S8Lit("data"), 0);

                    int num_index = 0;
                    fprintf(output, ".tiles = {\n");
                    fprintf(output, "{ ");
                    for(MD_EachNode(tile_id_node, data->first_child)) {
                        fprintf(output, "%.*s, ", MD_S8VArg(tile_id_node->string));

                        if(num_index % width == width - 1) {
                            if(MD_NodeIsNil(tile_id_node->next)) {
                                fprintf(output, "},\n},\n");
                            } else {
                                fprintf(output, "},\n{ ");
                            }
                        }
                        num_index += 1;
                    }
                }
            }
            fprintf(output, "\n}; // %.*s\n", MD_S8VArg(variable_name));
        }
    }

    MD_StringJoin join = MD_ZERO_STRUCT;
    MD_String8 declarations = MD_S8ListJoin(cg_arena, declarations_list, &join);
    MD_String8 loads = MD_S8ListJoin(cg_arena, load_list, &join);
    fprintf(output, "%.*s\nvoid load_assets() {\n%.*s\n}\n", MD_S8VArg(declarations), MD_S8VArg(loads));
    fprintf(output, "%.*s\n", MD_S8VArg(MD_S8ListJoin(cg_arena, tileset_decls, &join)));

    fclose(output);
    return 0;
}
