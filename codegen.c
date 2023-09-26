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
#define FUNCTION no_ubsan
#include "md.h"
#include "md.c"
#pragma warning(pop)


void dump(Node* from) {
	printf("/ %.*s\n", S8VArg(from->string));
	int d = 0;
	for (EachNode(child, from->first_child))
	{
		printf("|-- Child %d Tag [%.*s] string[%.*s] first child string[%.*s]\n", d, S8VArg(child->first_tag->string), S8VArg(child->string), S8VArg(child->first_child->string));
		d += 1;
	}
}
bool has_decimal(String8 s)
{
	for (int i = 0; i < s.size; i++)
	{
		if (s.str[i] == '.') return true;
	}
	return false;
}

Arena *cg_arena = NULL;

#define S8(s) S8Lit(s)
#define S8V(s) S8VArg(s)

String8 ChildValue(Node *n, String8 name) {
	Node *child_with_value = MD_ChildFromString(n, name, 0);
	assert(child_with_value);
	assert(!NodeIsNil(child_with_value->first_child)); // S8Lit("Must have child"));
	return child_with_value->first_child->string;
}

String8 asset_file_path(String8 filename) {
	return S8Fmt(cg_arena, "%.*s/%.*s", S8VArg(S8("assets")), S8VArg(filename));
}

char *nullterm(String8 s) {
	char *to_return = ArenaPush(cg_arena, s.size + 1);
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
	assert(false); // S8Lit("Couldn't find char"));
	return NULL;
}

#define StrSame(s1, s2) S8Match((s1), (s2), 0)
#define EachString(it, first) String8Node *it = (first); it != 0; it = it->next

typedef BUFF(Node*, 256) Nodes;
Node* find_by_name(Nodes *n, String8 name)
{
	Node *node_with = 0;
	BUFF_ITER(Node *, n)
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

#define list_printf(list_ptr, ...) S8ListPush(cg_arena, list_ptr, S8Fmt(cg_arena, __VA_ARGS__))
void dump_full(Node* from)
{
	for (EachNode(node, from))
	{
		printf("@%.*s %.*s\n", S8VArg(node->first_tag->string), S8VArg(node->string));
	}
	/* String8List output_list = {0};
		 DebugDumpFromNode(cg_arena, &output_list, from, 4, S8("  "), 0);
		 StringJoin join = (StringJoin){0};
		 String8 debugged = S8ListJoin(cg_arena, output_list , &join);
		 printf("%.*s\n", S8VArg(debugged));*/
}

#include "character_info.h"

int main(int argc, char **argv)
{
	cg_arena = ArenaAlloc();
	assert(cg_arena);

	String8 test = S8Lit("*testing*other");
	String8 to_split = S8Lit("*");
	String8List split_up = S8Split(cg_arena, test, 1, &to_split);

	printf("Split up: ");
	for(String8Node * cur = split_up.first; cur; cur = cur->next)
	{
		printf("'%.*s', ", S8VArg(cur->string));
	}
	printf("\n");

	// do characters

	FILE *char_header = fopen("gen/characters.gen.h", "w");
	fprintf(char_header, top_of_header);

#define GEN_TABLE(arr_elem_type, table_name, arr, str_access) { fprintf(char_header, "char *%s[] = {\n", table_name); ARR_ITER(arr_elem_type, arr) fprintf(char_header, "\"%s\",\n", str_access); fprintf(char_header, "}; // %s\n", table_name); }
#define GEN_ENUM(arr_elem_type, arr, enum_type_name, table_name, enum_name_access, fmt_str) { fprintf(char_header, "typedef enum\n{\n"); ARR_ITER(arr_elem_type, arr) fprintf(char_header, fmt_str, enum_name_access); fprintf(char_header, "} %s;\n", enum_type_name); GEN_TABLE(arr_elem_type, table_name, arr, enum_name_access); }
	GEN_ENUM(ActionInfo, actions, "ActionKind", "ActionKind_names", it->name, "ACT_%s,\n");

	fclose(char_header);

	// do assets

	String8 writeto = S8Fmt(cg_arena, "gen/assets.gen.c");
	Log("Writing to %.*s\n", S8VArg(writeto));
	FILE *output = fopen(nullterm(writeto), "w");

	ParseResult parse = ParseWholeFile(cg_arena, S8Lit("assets.mdesk"));

	String8List declarations_list = { 0 };
	String8List load_list = { 0 };
	String8List level_decl_list = { 0 };
	String8List tileset_decls = { 0 };
	for (EachNode(node, parse.node->first_child)) {
		if (S8Match(node->first_tag->string, S8Lit("sound"), 0)) {
			String8 variable_name = S8Fmt(cg_arena, "sound_%.*s", S8VArg(node->string));
			Log("New sound variable %.*s\n", S8VArg(variable_name));
			String8 filepath = ChildValue(node, S8Lit("filepath"));
			filepath = asset_file_path(filepath);
			assert(filepath.str != 0); // S8Fmt(cg_arena, "No filepath specified for sound '%.*s'", S8VArg(node->string)));
			FILE *asset_file = fopen(nullterm(filepath), "r");
			assert(asset_file); //  S8Fmt(cg_arena, "Could not open filepath %.*s for asset '%.*s'", S8VArg(filepath), S8VArg(node->string)));
			fclose(asset_file);

			S8ListPush(cg_arena, &declarations_list, S8Fmt(cg_arena, "AudioSample %.*s = {0};\n", S8VArg(variable_name)));
			S8ListPush(cg_arena, &load_list, S8Fmt(cg_arena, "%.*s = load_wav_audio(\"%.*s\");\n", S8VArg(variable_name), S8VArg(filepath)));
		}
		if (S8Match(node->first_tag->string, S8Lit("image"), 0)) {
			String8 variable_name = S8Fmt(cg_arena, "image_%.*s", S8VArg(node->string));
			//Log("New image variable %.*s\n", S8VArg(variable_name));
			String8 filepath = ChildValue(node, S8Lit("filepath"));
			filepath = asset_file_path(filepath);
			assert(filepath.str != 0); // , S8Fmt(cg_arena, "No filepath specified for image '%.*s'", S8VArg(node->string)));
			FILE *asset_file = fopen(nullterm(filepath), "rb");
			assert(asset_file); // , S8Fmt(cg_arena, "Could not open filepath %.*s for asset '%.*s'", S8VArg(filepath), S8VArg(node->string)));
			fclose(asset_file);

			S8ListPush(cg_arena, &declarations_list, S8Fmt(cg_arena, "sg_image %.*s = {0};\n", S8VArg(variable_name)));
			S8ListPush(cg_arena, &load_list, S8Fmt(cg_arena, "%.*s = load_image(S8Lit(\"%.*s\"));\n", S8VArg(variable_name), S8VArg(filepath)));
		}
	}


	StringJoin join = { 0 };
	String8 declarations = S8ListJoin(cg_arena, declarations_list, &join);
	String8 loads = S8ListJoin(cg_arena, load_list, &join);
	fprintf(output, "%.*s\nvoid load_assets() {\n%.*s\n}\n", S8VArg(declarations), S8VArg(loads));

	fclose(output);

	return 0;
}
