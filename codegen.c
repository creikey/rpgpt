#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#define assert_cond(cond, explanation) { if(!cond) { printf("Codegen assert_condion line %d %s failed: %.*s\n", __LINE__, #cond, MD_S8VArg(explanation)); __debugbreak(); exit(1); } }

#include "buff.h"

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
  printf("|-- Child %d Tag [%.*s] string[%.*s] first child string[%.*s]\n", d, MD_S8VArg(child->first_tag->string), MD_S8VArg(child->string), MD_S8VArg(child->first_child->string));
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

bool has_decimal(MD_String8 s)
{
 for(int i = 0; i < s.size; i++)
 {
  if(s.str[i] == '.') return true;
 }
 return false;
}

MD_Arena *cg_arena = NULL;

MD_String8 ChildValue(MD_Node *n, MD_String8 name) {
 MD_Node *child_with_value = MD_ChildFromString(n, name, 0);
 assert_cond(child_with_value, MD_S8Fmt(cg_arena, "Could not find child named '%.*s' of node '%.*s'", MD_S8VArg(name), MD_S8VArg(n->string)));
 assert_cond(!MD_NodeIsNil(child_with_value->first_child), MD_S8Lit("Must have child"));
 //assert(child_with_value->first_child->string.str != 0 && child_with_value->first_child->string.size > 0);
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
 assert_cond(false, MD_S8Lit("Couldn't find char"));
 return NULL;
}

#define StrSame(s1, s2) MD_S8Match((s1), (s2), 0)

typedef BUFF(MD_Node*, 256) Nodes;
MD_Node* find_by_name(Nodes *n, MD_String8 name)
{
 MD_Node *node_with = 0;
 BUFF_ITER(MD_Node *, n)
 {
  if(StrSame((*it)->string, name))
  {
   assert(node_with == 0);
   node_with = (*it);
  }
 }
 assert(node_with);
 return node_with;
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
#define S8(s) MD_S8Lit(s)

int main(int argc, char **argv) {
 cg_arena = MD_ArenaAlloc();
 assert_cond(cg_arena, MD_S8Lit("Memory"));

 MD_ParseResult training_parse = MD_ParseWholeFile(cg_arena, MD_S8Lit("training.mdesk"));
 MD_String8 global_prompt = {0};
 for(MD_EachNode(node, training_parse.node->first_child))
 {
  if(MD_S8Match(node->first_tag->string, MD_S8Lit("global_prompt"), 0))
  {
   global_prompt = node->string;
  }
 }
 MD_String8 training_writeto = MD_S8Fmt(cg_arena, "%.*s/training_data.jsonl\0", MD_S8VArg(OUTPUT_FOLDER));
 FILE *train = fopen(training_writeto.str, "w");
 assert(train);

 Nodes characters = {0};
 for(MD_EachNode(node, training_parse.node->first_child))
 {
  if(MD_S8Match(node->first_tag->string, MD_S8Lit("character"), 0))
  {
   BUFF_APPEND(&characters, node);
  }
 }

 Nodes items = {0};
 for(MD_EachNode(node, training_parse.node->first_child))
 {
  if(MD_S8Match(node->first_tag->string, MD_S8Lit("item"), 0))
  {
   BUFF_APPEND(&items, node);
  }
 }
 
 for(MD_EachNode(node, training_parse.node->first_child))
 {
  if(MD_S8Match(node->first_tag->string, MD_S8Lit("training"), 0))
  {
   MD_Node *node_with = find_by_name(&characters, ChildValue(node, S8("with")));

   int upto_npc_line = 0;
   int cur_npc_line = 0;
   MD_Node *cur_sentence = MD_ChildFromString(node, S8("data"), 0)->first_child;
   MD_Node *has_item = 0;
   MD_String8List conversation = {0};
   while(!MD_NodeIsNil(cur_sentence))
   {
    assert(!MD_NodeIsNil(cur_sentence));
    bool is_player = MD_NodeHasTag(cur_sentence, S8("player"), 0);
    bool is_npc = MD_NodeHasTag(cur_sentence, S8("npc"), 0);
    bool is_item_possess = MD_NodeHasTag(cur_sentence, S8("item_possess"), 0);
    bool is_item_discard = MD_NodeHasTag(cur_sentence, S8("item_discard"), 0);
    if(is_player)
    {
     list_printf(&conversation, "Player: \\\"%.*s\\\"\\n", MD_S8VArg(cur_sentence->string));
    }
    if(is_item_possess)
    {
     MD_Node *item = find_by_name(&items, cur_sentence->string);
     assert(item);
     has_item = item;
     list_printf(&conversation, "%.*s\\n", MD_S8VArg(ChildValue(item, S8("possess_message"))));
    }
    if(is_item_discard)
    {
     MD_Node *item = find_by_name(&items, cur_sentence->string);
     assert(item);
     has_item = 0;
     list_printf(&conversation, "%.*s\\n", MD_S8VArg(ChildValue(item, S8("discard_message"))));
    }
    bool restarting = false;
    if(is_npc)
    {
     list_printf(&conversation, "%.*s: \\\"", MD_S8VArg(ChildValue(node_with, S8("name"))));
     if(upto_npc_line == cur_npc_line)
     {
      MD_String8 completion = cur_sentence->string;
      fprintf(train, "{\"prompt\": \"");
      fprintf(train, nullterm(global_prompt), MD_S8VArg(ChildValue(node_with, S8("actions_str"))));
      fprintf(train, "\\n");
      if(has_item) fprintf(train, "%.*s\\n", MD_S8VArg(ChildValue(has_item, S8("global_prompt_message"))));
      fprintf(train, "%.*s\\n", MD_S8VArg(ChildValue(node_with, S8("prompt"))));

      //MD_StringJoin join = (MD_StringJoin){.mid = S8("\\n")};
      MD_StringJoin join = (MD_StringJoin){0};
      MD_String8 conversation_string = MD_S8ListJoin(cg_arena, conversation, &join);
      fprintf(train, "%.*s\", \"completion\": \"%.*s\\\"\"}\n", MD_S8VArg(conversation_string), MD_S8VArg(completion));

      upto_npc_line += 1;
      cur_npc_line = 0;
      cur_sentence = MD_ChildFromString(node, S8("data"), 0)->first_child;
      conversation = (MD_String8List){0};
      restarting = true;
     }
     else
     {
      list_printf(&conversation, "%.*s\\\"\\n", MD_S8VArg(cur_sentence->string));
      cur_npc_line += 1;
     }
    }
    if(!restarting) cur_sentence = cur_sentence->next;
   }
  }
 }

 BUFF_ITER(MD_Node*, &characters)
 {
  printf("Character %.*s\n", MD_S8VArg((*it)->string));
 }


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
  if(MD_S8Match(node->first_tag->string, MD_S8Lit("sound"), 0)) {
   MD_String8 variable_name = MD_S8Fmt(cg_arena, "sound_%.*s", MD_S8VArg(node->string));
   log("New sound variable %.*s\n", MD_S8VArg(variable_name));
   MD_String8 filepath = ChildValue(node, MD_S8Lit("filepath"));
   filepath = asset_file_path(filepath);
   assert_cond(filepath.str != 0, MD_S8Fmt(cg_arena, "No filepath specified for sound '%.*s'", MD_S8VArg(node->string)));
   FILE *asset_file = fopen(filepath.str, "r");
   assert_cond(asset_file, MD_S8Fmt(cg_arena, "Could not open filepath %.*s for asset '%.*s'", MD_S8VArg(filepath), MD_S8VArg(node->string)));
   fclose(asset_file);

   MD_S8ListPush(cg_arena, &declarations_list, MD_S8Fmt(cg_arena, "AudioSample %.*s = {0};\n", MD_S8VArg(variable_name)));
   MD_S8ListPush(cg_arena, &load_list, MD_S8Fmt(cg_arena, "%.*s = load_wav_audio(\"%.*s\");\n", MD_S8VArg(variable_name), MD_S8VArg(filepath)));
  }
  if(MD_S8Match(node->first_tag->string, MD_S8Lit("image"), 0)) {
   MD_String8 variable_name = MD_S8Fmt(cg_arena, "image_%.*s", MD_S8VArg(node->string));
   log("New image variable %.*s\n", MD_S8VArg(variable_name));
   MD_String8 filepath = ChildValue(node, MD_S8Lit("filepath"));
   filepath = asset_file_path(filepath);
   assert_cond(filepath.str != 0, MD_S8Fmt(cg_arena, "No filepath specified for image '%.*s'", MD_S8VArg(node->string)));
   FILE *asset_file = fopen(filepath.str, "r");
   assert_cond(asset_file, MD_S8Fmt(cg_arena, "Could not open filepath %.*s for asset '%.*s'", MD_S8VArg(filepath), MD_S8VArg(node->string)));
   fclose(asset_file);

   MD_S8ListPush(cg_arena, &declarations_list, MD_S8Fmt(cg_arena, "sg_image %.*s = {0};\n", MD_S8VArg(variable_name)));
   MD_S8ListPush(cg_arena, &load_list, MD_S8Fmt(cg_arena, "%.*s = load_image(\"%.*s\");\n", MD_S8VArg(variable_name), MD_S8VArg(filepath)));
  }
  if(MD_S8Match(node->first_tag->string, MD_S8Lit("tileset"), 0)) {
   // not a variable anymore
   MD_String8 variable_name = MD_S8Fmt(cg_arena, "tileset_%.*s", MD_S8VArg(node->string));
   log("New tileset %.*s\n", MD_S8VArg(variable_name));
   MD_String8 filepath = asset_file_path(ChildValue(node, MD_S8Lit("filepath")));

   MD_String8 tileset_file_contents = MD_LoadEntireFile(cg_arena, filepath);
   list_printf(&tileset_decls, "{\n", MD_S8VArg(variable_name));
   list_printf(&tileset_decls, ".first_gid = %.*s,\n", MD_S8VArg(ChildValue(node, MD_S8Lit("firstgid"))));
   list_printf(&tileset_decls, ".img = &%.*s,\n", MD_S8VArg(ChildValue(node, MD_S8Lit("image"))));

   list_printf(&tileset_decls, ".animated = {\n");
   char *end = tileset_file_contents.str + tileset_file_contents.size;
   char *cur = tileset_file_contents.str;
   int num_animated_tiles = 0;
   while(cur < end) {
    cur = goto_end_of(cur, end - cur, "<tile id=\""); 
    if(cur == NULL) break;
    char *end_of_anim = goto_end_of(cur, end - cur, "</animation>");
    if(end_of_anim == NULL) break;
    char *new_cur = fillnull(cur, '"');
    int frame_from = atoi(cur);
    cur = new_cur;
    list_printf(&tileset_decls, "{ .id_from = %d, .frames = { ", frame_from);

    int num_frames = 0;
    while(true) {
     char *next_frame = goto_end_of(cur, end - cur, "<frame tileid=\"");
     if(end_of_anim == NULL || next_frame == NULL || next_frame > end_of_anim) break;
     char *new_cur = fillnull(next_frame, '"');
     int frame = atoi(next_frame);

     list_printf(&tileset_decls, "%d, ", frame);
     num_frames++;

     cur = new_cur;
    }
    list_printf(&tileset_decls, "}, .num_frames = %d },\n", num_frames);
    num_animated_tiles++;
   }
   if(num_animated_tiles == 0) list_printf(&tileset_decls, "0");
   list_printf(&tileset_decls, "}},\n");
  }
  if(MD_S8Match(node->first_tag->string, MD_S8Lit("level"), 0)) {
   MD_String8 variable_name = MD_S8Fmt(cg_arena, "level_%.*s", MD_S8VArg(node->string));
   log("New level variable %.*s\n", MD_S8VArg(variable_name));
   MD_String8 filepath = asset_file_path(ChildValue(node, MD_S8Lit("filepath")));
   MD_ParseResult level_parse = MD_ParseWholeFile(cg_arena, filepath);
   assert_cond(!MD_NodeIsNil(level_parse.node->first_child), MD_S8Lit("Failed to load level file"));

   MD_Node *layers = MD_ChildFromString(level_parse.node->first_child, MD_S8Lit("layers"), 0);
   fprintf(output, "Level %.*s = {\n", MD_S8VArg(variable_name));
   MD_String8List tile_layer_decls = {0};
   for(MD_EachNode(lay, layers->first_child)) {
    MD_String8 type = MD_ChildFromString(lay, MD_S8Lit("type"), 0)->first_child->string;
    if(MD_S8Match(type, MD_S8Lit("objectgroup"), 0)) {
     fprintf(output, ".initial_entities = {\n");
     for(MD_EachNode(object, MD_ChildFromString(lay, MD_S8Lit("objects"), 0)->first_child)) {
      //dump(object);
      // negative numbers for object position aren't supported here
      MD_String8 name = MD_ChildFromString(object, MD_S8Lit("name"), 0)->first_child->string;
      MD_String8 x_string = MD_ChildFromString(object, MD_S8Lit("x"), 0)->first_child->string;
      MD_String8 y_string = MD_ChildFromString(object, MD_S8Lit("y"), 0)->first_child->string;
      y_string = MD_S8Fmt(cg_arena, "-%.*s", MD_S8VArg(y_string));

      if(has_decimal(x_string)) x_string = MD_S8Fmt(cg_arena, "%.*sf", MD_S8VArg(x_string));
      if(has_decimal(y_string)) y_string = MD_S8Fmt(cg_arena, "%.*sf", MD_S8VArg(y_string));

      MD_String8 class = MD_ChildFromString(object, MD_S8Lit("class"), 0)->first_child->string;
      if(MD_S8Match(class, MD_S8Lit("PROP"), 0))
      {
       fprintf(output, "{ .exists = true, .is_prop = true, .prop_kind = %.*s, .pos = { .X=%.*s, .Y=%.*s }, }, ", MD_S8VArg(name), MD_S8VArg(x_string), MD_S8VArg(y_string));
      }
      else if(MD_S8Match(name, MD_S8Lit("PLAYER"), 0))
      {
       fprintf(output, "{ .exists = true, .is_character = true, .pos = { .X=%.*s, .Y=%.*s }, }, ", MD_S8VArg(x_string), MD_S8VArg(y_string));
      }
      else
      {
       fprintf(output, "{ .exists = true, .is_npc = true, .npc_kind = NPC_%.*s, .pos = { .X=%.*s, .Y=%.*s }, }, ", MD_S8VArg(name), MD_S8VArg(x_string), MD_S8VArg(y_string));
      }
     }
     fprintf(output, "\n}, // entities\n");
    }
    if(MD_S8Match(type, MD_S8Lit("tilelayer"), 0)) {
     int width = atoi(nullterm(MD_ChildFromString(layers->first_child, MD_S8Lit("width"), 0)->first_child->string));
     int height = atoi(nullterm(MD_ChildFromString(layers->first_child, MD_S8Lit("height"), 0)->first_child->string));
     MD_Node *data = MD_ChildFromString(lay, MD_S8Lit("data"), 0);

     int num_index = 0;
     MD_String8List cur_layer_decl = {0};
     list_printf(&cur_layer_decl, "{ \n");
     list_printf(&cur_layer_decl, "{ ");
     for(MD_EachNode(tile_id_node, data->first_child)) {
      list_printf(&cur_layer_decl, "%.*s, ", MD_S8VArg(tile_id_node->string));

      if(num_index % width == width - 1) {
       if(MD_NodeIsNil(tile_id_node->next)) {
        list_printf(&cur_layer_decl, "},\n}, // tiles for this layer\n");
       } else {
        list_printf(&cur_layer_decl, "},\n{ ");
       }
      }
      num_index += 1;
     }

     MD_StringJoin join = MD_ZERO_STRUCT;
     MD_String8 layer_decl_string = MD_S8ListJoin(cg_arena, cur_layer_decl, &join);
     MD_S8ListPush(cg_arena, &tile_layer_decls, layer_decl_string);
    }
   }

   fprintf(output, ".tiles = {\n");
   // layer decls
   {

    MD_StringJoin join = MD_ZERO_STRUCT;
    MD_String8 layers_string = MD_S8ListJoin(cg_arena, tile_layer_decls, &join);
    fprintf(output, "%.*s\n", MD_S8VArg(layers_string));
    //MD_S8ListPush(cg_arena, &tile_layer_delcs, layer_decl_string);
    
   }
   fprintf(output, "} // tiles\n");

   fprintf(output, "\n}; // %.*s\n", MD_S8VArg(variable_name));
  }
 }

 MD_StringJoin join = MD_ZERO_STRUCT;
 MD_String8 declarations = MD_S8ListJoin(cg_arena, declarations_list, &join);
 MD_String8 loads = MD_S8ListJoin(cg_arena, load_list, &join);
 fprintf(output, "%.*s\nvoid load_assets() {\n%.*s\n}\n", MD_S8VArg(declarations), MD_S8VArg(loads));

 fprintf(output, "TileSet tilesets[] = { %.*s\n };\n", MD_S8VArg(MD_S8ListJoin(cg_arena, tileset_decls, &join)));


 fclose(output);

 output = fopen(MD_S8Fmt(cg_arena, "%.*s/characters.gen.h\0", MD_S8VArg(OUTPUT_FOLDER)).str, "w");
 //fprintf(output, "char *global_prompt = \"%.*s\";\n", MD_S8VArg(global_prompt));

 fprintf(output, "char *general_prompt_table[] = {\n");
 BUFF_ITER(MD_Node*, &characters)
 {
  MD_String8 personalized_global_prompt = MD_S8Fmt(cg_arena, nullterm(global_prompt), MD_S8VArg(ChildValue(*it, S8("actions_str"))));
  fprintf(output, "\"%.*s\", // %.*s\n", MD_S8VArg(personalized_global_prompt), MD_S8VArg((*it)->string));
 }
 fprintf(output, "}; // general prompt table\n");


 fprintf(output, "char *prompt_table[] = {\n");
 BUFF_ITER(MD_Node*, &characters)
 {
  fprintf(output, "\"%.*s\", // %.*s\n", MD_S8VArg(ChildValue(*it, S8("prompt"))),MD_S8VArg((*it)->string));
 }
 fprintf(output, "}; // prompt table\n");

 fprintf(output, "typedef enum ItemKind {\nITEM_Invalid,\n");
 BUFF_ITER(MD_Node*, &items)
 {
  fprintf(output, "ITEM_%.*s,\n", MD_S8VArg((*it)->string));
 }
 fprintf(output, "} ItemKind;\n");

 fprintf(output, "char *item_prompt_table[] = {\n\"Invalid\",\n");
 BUFF_ITER(MD_Node*, &items)
 {
  fprintf(output, "\"%.*s\",\n", MD_S8VArg(ChildValue(*it, S8("global_prompt_message"))));
 }
 fprintf(output, "}; // item prompt table\n");

 fprintf(output, "char *item_possess_message_table[] = {\n\"Invalid\",\n");
 BUFF_ITER(MD_Node*, &items)
 {
  fprintf(output, "\"%.*s\",\n", MD_S8VArg(ChildValue(*it, S8("possess_message"))));
 }
 fprintf(output, "}; // item possess_message table\n");

 fprintf(output, "char *item_discard_message_table[] = {\n\"Invalid\",\n");
 BUFF_ITER(MD_Node*, &items)
 {
  fprintf(output, "\"%.*s\",\n", MD_S8VArg(ChildValue(*it, S8("discard_message"))));
 }
 fprintf(output, "}; // item discard_message table\n");

 fprintf(output, "char *name_table[] = {\n");
 BUFF_ITER(MD_Node*, &characters)
 {
  fprintf(output, "\"%.*s\", // %.*s\n", MD_S8VArg(ChildValue(*it, S8("name"))),MD_S8VArg((*it)->string));
 }
 fprintf(output, "}; // name table\n");

 fprintf(output, "typedef enum\n{ // character enums, not completed here so you can add more in the include\n");
 BUFF_ITER(MD_Node*, &characters)
 {
  fprintf(output, "NPC_%.*s,\n", MD_S8VArg((*it)->string));
 }
 //fprintf(output, "NPC_LAST_CHARACTER,\n};\n");


 fclose(output);

 return 0;
}
