#include <stdio.h>
#include <stdbool.h>
#define DESKTOP
#include "utility.h"

#include "buff.h"

#pragma warning(disable : 4996) // nonsense about fopen being insecure

#if defined(__clang__)
#define no_ubsan __attribute__((no_sanitize("undefined")))
#else
#define no_ubsan 
#endif

#pragma warning(push)
#pragma warning(disable : 4244) // loss of data warning
#pragma warning(disable : 4101) // unreferenced local variable
#define STBSP_ADD_TO_FUNCTIONS no_ubsan
#define MD_FUNCTION no_ubsan
#include "md.h"
#include "md.c"
#pragma warning(pop)


void dump(MD_Node* from) {
	printf("/ %.*s\n", MD_S8VArg(from->string));
	int d = 0;
	for (MD_EachNode(child, from->first_child))
	{
		printf("|-- Child %d Tag [%.*s] string[%.*s] first child string[%.*s]\n", d, MD_S8VArg(child->first_tag->string), MD_S8VArg(child->string), MD_S8VArg(child->first_child->string));
		d += 1;
	}
}
bool has_decimal(MD_String8 s)
{
	for (int i = 0; i < s.size; i++)
	{
		if (s.str[i] == '.') return true;
	}
	return false;
}

MD_Arena *cg_arena = NULL;

#define S8(s) MD_S8Lit(s)
#define S8V(s) MD_S8VArg(s)

MD_String8 ChildValue(MD_Node *n, MD_String8 name) {
	MD_Node *child_with_value = MD_ChildFromString(n, name, 0);
	assert(child_with_value);
	assert(!MD_NodeIsNil(child_with_value->first_child)); // MD_S8Lit("Must have child"));
	return child_with_value->first_child->string;
}

MD_String8 asset_file_path(MD_String8 filename) {
	return MD_S8Fmt(cg_arena, "%.*s/%.*s", MD_S8VArg(S8("assets")), MD_S8VArg(filename));
}

char *nullterm(MD_String8 s) {
	char *to_return = MD_ArenaPush(cg_arena, s.size + 1);
	memcpy(to_return, s.str, s.size);
	to_return[s.size] = '\0';
	return to_return;
}

char* fillnull(char *s, char c) {
	while (*s != '\0') {
		if (*s == c) {
			*s = '\0';
			return s + 1;
		}
		s++;
	}
	assert(false); // MD_S8Lit("Couldn't find char"));
	return NULL;
}

#define StrSame(s1, s2) MD_S8Match((s1), (s2), 0)
#define EachString(it, first) MD_String8Node *it = (first); it != 0; it = it->next

typedef BUFF(MD_Node*, 256) Nodes;
MD_Node* find_by_name(Nodes *n, MD_String8 name)
{
	MD_Node *node_with = 0;
	BUFF_ITER(MD_Node *, n)
	{
		if (StrSame((*it)->string, name))
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
	for (int i = 0; i < max_move; i++) {
		if (strncmp(tomove, pattern, pattern_len) == 0) {
			tomove += pattern_len;
			return tomove;
		}
		tomove++;
	}
	return NULL;
}

#define list_printf(list_ptr, ...) MD_S8ListPush(cg_arena, list_ptr, MD_S8Fmt(cg_arena, __VA_ARGS__))
void dump_full(MD_Node* from)
{
	for (MD_EachNode(node, from))
	{
		printf("@%.*s %.*s\n", MD_S8VArg(node->first_tag->string), MD_S8VArg(node->string));
	}
	/* MD_String8List output_list = {0};
		 MD_DebugDumpFromNode(cg_arena, &output_list, from, 4, S8("  "), 0);
		 MD_StringJoin join = (MD_StringJoin){0};
		 MD_String8 debugged = MD_S8ListJoin(cg_arena, output_list , &join);
		 printf("%.*s\n", MD_S8VArg(debugged));*/
}

#include "character_info.h"

int main(int argc, char **argv)
{
	cg_arena = MD_ArenaAlloc();
	assert(cg_arena);

	MD_String8 test = MD_S8Lit("*testing*other");
	MD_String8 to_split = MD_S8Lit("*");
	MD_String8List split_up = MD_S8Split(cg_arena, test, 1, &to_split);

	printf("Split up: ");
	for(MD_String8Node * cur = split_up.first; cur; cur = cur->next)
	{
		printf("'%.*s', ", MD_S8VArg(cur->string));
	}
	printf("\n");

	// do characters

	FILE *char_header = fopen("gen/characters.gen.h", "w");
	fprintf(char_header, top_of_header);

#define GEN_TABLE(arr_elem_type, table_name, arr, str_access) { fprintf(char_header, "char *%s[] = {\n", table_name); ARR_ITER(arr_elem_type, arr) fprintf(char_header, "\"%s\",\n", str_access); fprintf(char_header, "}; // %s\n", table_name); }
#define GEN_ENUM(arr_elem_type, arr, enum_type_name, table_name, enum_name_access, fmt_str) { fprintf(char_header, "typedef enum\n{\n"); ARR_ITER(arr_elem_type, arr) fprintf(char_header, fmt_str, enum_name_access); fprintf(char_header, "} %s;\n", enum_type_name); GEN_TABLE(arr_elem_type, table_name, arr, enum_name_access); }
	GEN_ENUM(ActionInfo, actions, "ActionKind", "ActionKind_names", it->name, "ACT_%s,\n");
	GEN_ENUM(CharacterGen, characters, "NpcKind", "NpcKind_enum_names", it->enum_name, "NPC_%s,\n");
	GEN_TABLE(CharacterGen,"NpcKind_names", characters,it->name);

	fclose(char_header);

	// do assets

	MD_String8 writeto = MD_S8Fmt(cg_arena, "gen/assets.gen.c");
	Log("Writing to %.*s\n", MD_S8VArg(writeto));
	FILE *output = fopen(nullterm(writeto), "w");

	MD_ParseResult parse = MD_ParseWholeFile(cg_arena, MD_S8Lit("assets.mdesk"));

	MD_String8List declarations_list = { 0 };
	MD_String8List load_list = { 0 };
	MD_String8List level_decl_list = { 0 };
	MD_String8List tileset_decls = { 0 };
	for (MD_EachNode(node, parse.node->first_child)) {
		if (MD_S8Match(node->first_tag->string, MD_S8Lit("sound"), 0)) {
			MD_String8 variable_name = MD_S8Fmt(cg_arena, "sound_%.*s", MD_S8VArg(node->string));
			Log("New sound variable %.*s\n", MD_S8VArg(variable_name));
			MD_String8 filepath = ChildValue(node, MD_S8Lit("filepath"));
			filepath = asset_file_path(filepath);
			assert(filepath.str != 0); // MD_S8Fmt(cg_arena, "No filepath specified for sound '%.*s'", MD_S8VArg(node->string)));
			FILE *asset_file = fopen(nullterm(filepath), "r");
			assert(asset_file); //  MD_S8Fmt(cg_arena, "Could not open filepath %.*s for asset '%.*s'", MD_S8VArg(filepath), MD_S8VArg(node->string)));
			fclose(asset_file);

			MD_S8ListPush(cg_arena, &declarations_list, MD_S8Fmt(cg_arena, "AudioSample %.*s = {0};\n", MD_S8VArg(variable_name)));
			MD_S8ListPush(cg_arena, &load_list, MD_S8Fmt(cg_arena, "%.*s = load_wav_audio(\"%.*s\");\n", MD_S8VArg(variable_name), MD_S8VArg(filepath)));
		}
		if (MD_S8Match(node->first_tag->string, MD_S8Lit("image"), 0)) {
			MD_String8 variable_name = MD_S8Fmt(cg_arena, "image_%.*s", MD_S8VArg(node->string));
			//Log("New image variable %.*s\n", MD_S8VArg(variable_name));
			MD_String8 filepath = ChildValue(node, MD_S8Lit("filepath"));
			filepath = asset_file_path(filepath);
			assert(filepath.str != 0); // , MD_S8Fmt(cg_arena, "No filepath specified for image '%.*s'", MD_S8VArg(node->string)));
			FILE *asset_file = fopen(nullterm(filepath), "rb");
			assert(asset_file); // , MD_S8Fmt(cg_arena, "Could not open filepath %.*s for asset '%.*s'", MD_S8VArg(filepath), MD_S8VArg(node->string)));
			fclose(asset_file);

			MD_S8ListPush(cg_arena, &declarations_list, MD_S8Fmt(cg_arena, "sg_image %.*s = {0};\n", MD_S8VArg(variable_name)));
			MD_S8ListPush(cg_arena, &load_list, MD_S8Fmt(cg_arena, "%.*s = load_image(MD_S8Lit(\"%.*s\"));\n", MD_S8VArg(variable_name), MD_S8VArg(filepath)));
		}
	}


	MD_StringJoin join = { 0 };
	MD_String8 declarations = MD_S8ListJoin(cg_arena, declarations_list, &join);
	MD_String8 loads = MD_S8ListJoin(cg_arena, load_list, &join);
	fprintf(output, "%.*s\nvoid load_assets() {\n%.*s\n}\n", MD_S8VArg(declarations), MD_S8VArg(loads));

	fclose(output);

	return 0;
}
