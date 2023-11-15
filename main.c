#pragma warning(disable : 4820) // don't care about padding
#pragma warning(disable : 4388) // signed/unsigned mismatch probably doesn't matter
//#pragma warning(disable : 4100) // unused local doesn't matter, because sometimes I want to kee
#pragma warning(disable : 4053) // void operands are used for tricks like applying printf linting to non printf function calls
#pragma warning(disable : 4255) // strange no function prototype given thing?
#pragma warning(disable : 4456) // I shadow local declarations often and it's fine
#pragma warning(disable : 4061) // I don't need to *explicitly* handle everything, having a default: case should mean no more warnings
#pragma warning(disable : 4201) // nameless struct/union occurs
#pragma warning(disable : 4366) // I think unaligned memory addresses are fine to ignore
#pragma warning(disable : 4459) // Local function decl hiding global declaration I think is fine
#pragma warning(disable : 5045) // spectre mitigation??

#include "tuning.h"

#define SOKOL_IMPL

// ctags doesn't like the error macro so we do this instead. lame
#define ISANERROR(why) jfdskljfdsljfklja why

#if defined(WIN32) || defined(_WIN32)
#define DESKTOP
#define WINDOWS
#define SOKOL_GLCORE33
#define SAMPLE_COUNT 4
#endif

#if defined(__EMSCRIPTEN__)
#define WEB
#define SOKOL_GLES3
#define SAMPLE_COUNT 4
#endif

#define DRWAV_ASSERT game_assert
#define SOKOL_ASSERT game_assert
#define STBDS_ASSERT game_assert
#define STBI_ASSERT  game_assert
#define STBTT_assert game_assert

#include "utility.h"


#ifdef WINDOWS


#pragma warning(push, 3)
#include <Windows.h>
#include <stdint.h>

// https://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
// Tells nvidia to use dedicated graphics card if it can on laptops that also have integrated graphics
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
// Vice versa for AMD but I don't have the docs link on me at the moment
__declspec(dllexport) uint32_t AmdPowerXpressRequestHighPerformance = 0x00000001;

#endif

#include "buff.h"
#include "sokol_app.h"
#pragma warning(push)
#pragma warning(disable : 4191) // unsafe function calling
#ifdef WEB
# include <GLES3/gl3.h>
# undef glGetError
# define glGetError() (GL_NO_ERROR)
#endif
#include "sokol_gfx.h"
#pragma warning(pop)
#include "sokol_time.h"
#include "sokol_audio.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4242) // unsafe conversion 
#include "stb_image.h"
#pragma warning(pop)
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "HandmadeMath.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h" // placed last because it includes <assert.h>
#define CUTE_C2_IMPLEMENTATION
#include "cute_c2.h"

#undef assert
#define assert game_assert

#pragma warning(pop)

// web compatible metadesk
#ifdef WEB
#define __gnu_linux__
#define i386
#define DEFAULT_ARENA 0

typedef struct WebArena
{
	char *data;
	size_t cap;
	size_t pos;
	size_t align; // pls 
} WebArena;

static bool next_arena_big = false;

WebArena *web_arena_alloc()
{
	// the pointer here is assumed to be aligned to whatever the maximum
	// alignment you could want from the arena, because only the `pos` is 
	// modified to align with the requested alignment.
	WebArena *to_return = malloc(sizeof(to_return));

	size_t this_size = ARENA_SIZE;
	if(next_arena_big) this_size = BIG_ARENA_SIZE;

	*to_return = (WebArena) {
		.data = calloc(1, this_size),
			.cap = this_size,
			.align = 8,
			.pos = 0,
	};

	next_arena_big = false;

	return to_return;
}

void web_arena_release(WebArena *arena)
{
	free(arena->data);
	arena->data = 0;
	free(arena);
}

size_t web_arena_get_pos(WebArena *arena)
{
	return arena->pos;
}

void *web_arena_push(WebArena *arena, size_t amount)
{
	while(arena->pos % arena->align != 0)
	{
		arena->pos += 1;
		assert(arena->pos < arena->cap);
	}
	void *to_return = arena->data + arena->pos;
	arena->pos += amount;

	bool arena_ok = arena->pos < arena->cap;
	if(!arena_ok)
	{
		Log("Arena size: %lu\n", arena->cap);
		Log("Arena pos: %lu\n", arena->pos);
	}
	assert(arena_ok);
	return to_return;
}

void web_arena_pop_to(WebArena *arena, size_t new_pos)
{
	arena->pos = new_pos;
	assert(arena->pos < arena->cap);
}

void web_arena_set_auto_align(WebArena *arena, size_t align)
{
	arena->align = align;
}

#define IMPL_Arena WebArena
#define IMPL_ArenaAlloc web_arena_alloc
#define IMPL_ArenaRelease web_arena_release
#define IMPL_ArenaGetPos web_arena_get_pos
#define IMPL_ArenaPush web_arena_push
#define IMPL_ArenaPopTo web_arena_pop_to
#define IMPL_ArenaSetAutoAlign web_arena_set_auto_align
#define IMPL_ArenaMinPos 64 // no idea what this is honestly
#endif // web

#pragma warning(disable : 4996) // fopen is safe. I don't care about fopen_s

#pragma warning(push)
#pragma warning(disable : 4244) // loss of data warning
#pragma warning(disable : 4101) // unreferenced local variable
#pragma warning(disable : 4100) // unreferenced local variable again?
#pragma warning(disable : 4189) // initialized and not referenced
#pragma warning(disable : 4242) // conversion
#pragma warning(disable : 4457) // hiding function variable happens
#pragma warning(disable : 4668) // __GNU_C__ macro undefined, fixing
#define STBSP_ADD_TO_FUNCTIONS no_ubsan
#define FUNCTION no_ubsan
#include "md.h"
#undef  Assert
#define Assert assert
#include "md.c"
#pragma warning(pop)

#include "ser.h"
#include "json_interop.h"

#include <math.h>

#ifdef DEVTOOLS
#ifdef DESKTOP
#define PROFILING
#define PROFILING_IMPL
#endif
#endif
#include "profiling.h"

// the returned string's size doesn't include the null terminator.
String8 nullterm(Arena *copy_onto, String8 to_nullterm)
{
	String8 to_return = {0};
	to_return.str = PushArray(copy_onto, u8, to_nullterm.size + 1);
	to_return.size = to_nullterm.size;
	to_return.str[to_return.size] = '\0';
	memcpy(to_return.str, to_nullterm.str, to_nullterm.size);
	return to_return;
}

// all utilities that depend on md.h strings or look like md.h stuff in general
#define PushWithLint(arena, list,  ...) { S8ListPushFmt(arena, list,  __VA_ARGS__); if(false) printf( __VA_ARGS__); }
#define FmtWithLint(arena, ...) (0 ? printf(__VA_ARGS__) : (void)0, S8Fmt(arena, __VA_ARGS__))

void WriteEntireFile(char *null_terminated_path, String8 data) {
    FILE* file = fopen(null_terminated_path, "wb");
    if(file) {
        fwrite(data.str, 1, data.size, file);
        fclose(file);
    } else {
        Log("Failed to open file %s and save data.\n", null_terminated_path)
    }
}

double clamp(double d, double min, double max)
{
	const double t = d < min ? min : d;
	return t > max ? max : t;
}
float clampf(float d, float min, float max)
{
	const float t = d < min ? min : d;
	return t > max ? max : t;
}

float clamp01(float f)
{
	return clampf(f, 0.0f, 1.0f);
}

#ifdef min
#undef min
#endif

int min(int a, int b)
{
	if (a < b) return a;
	else return b;
}

// so can be grep'd and removed
#define dbgprint(...) { printf("Debug | %s:%d | ", __FILE__, __LINE__); printf(__VA_ARGS__); }
#define v3varg(v) v.x, v.y, v.z
#define v2varg(v) v.x, v.y
#define qvarg(v) v.x, v.y, v.z, v.w
Vec2 RotateV2(Vec2 v, float theta)
{
	return V2(
			v.X * cosf(theta) - v.Y * sinf(theta),
			v.X * sinf(theta) + v.Y * cosf(theta)
			);
}

Vec2 ReflectV2(Vec2 v, Vec2 normal)
{
	assert(fabsf(LenV2(normal) - 1.0f) < 0.01f); // must be normalized
	Vec2 to_return = SubV2(v, MulV2F(normal, 2.0f * DotV2(v, normal)));

	assert(!isnan(to_return.x));
	assert(!isnan(to_return.y));
	return to_return;
}


float AngleOfV2(Vec2 v)
{
	return atan2f(v.y, v.x);
}


#define TAU (PI*2.0)



float lerp_angle(float from, float t, float to)
{
	double difference = fmod(to - from, TAU);
	double distance = fmod(2.0 * difference, TAU) - difference;
	return (float)(from + distance * t);
}

typedef struct AABB
{
	Vec2 upper_left;
	Vec2 lower_right;
} AABB;

typedef struct Circle
{
	Vec2 center;
	float radius;
} Circle;

typedef struct Quad
{
	union
	{
		struct
		{
			Vec2 ul; // upper left
			Vec2 ur; // upper right
			Vec2 lr; // lower right
			Vec2 ll; // lower left
		};
		Vec2 points[4];
	};
} Quad;

// for intellisense in vscode I think?
#include "character_info.h"
#include "characters.gen.h"

#define RND_IMPLEMENTATION
#include "makeprompt.h"
typedef BUFF(Entity*, 16) Overlapping;

typedef struct AudioSample
{
	float *pcm_data; // allocated by loader, must be freed
	uint64_t pcm_data_length;
	unsigned int num_channels;
} AudioSample;

typedef struct AudioPlayer
{
	AudioSample *sample; // if not 0, exists
	double volume; // ZII, 1.0 + this again
	double pitch; // zero initialized, the pitch used to play is 1.0 + this
	double cursor_time; // in seconds, current audio sample is cursor_time * sample_rate
} AudioPlayer;

AudioPlayer playing_audio[128] = { 0 };

#define SAMPLE_RATE 44100

AudioSample load_wav_audio(const char *path)
{
	unsigned int sampleRate;
	AudioSample to_return = { 0 };
	to_return.pcm_data = drwav_open_file_and_read_pcm_frames_f32(path, &to_return.num_channels, &sampleRate, &to_return.pcm_data_length, 0);
	assert(to_return.num_channels == 1 || to_return.num_channels == 2);
	assert(sampleRate == SAMPLE_RATE);
	return to_return;
}

void cursor_pcm(AudioPlayer *p, uint64_t *integer, float *fractional)
{
	double sample_time = p->cursor_time * SAMPLE_RATE;
	*integer = (uint64_t)sample_time;
	*fractional = (float)(sample_time - *integer);
}
float float_rand(float min, float max)
{
	float scale = rand() / (float) RAND_MAX; /* [0, 1.0] */
	return min + scale * (max - min); /* [min, max] */
}
// always randomizes pitch
void play_audio(AudioSample *sample, float volume)
{
	AudioPlayer *to_use = 0;
	for (int i = 0; i < ARRLEN(playing_audio); i++)
	{
		if (playing_audio[i].sample == 0)
		{
			to_use = &playing_audio[i];
			break;
		}
	}
	assert(to_use);
	*to_use = (AudioPlayer) { 0 };
	to_use->sample = sample;
	to_use->volume = volume;
	to_use->pitch = float_rand(-0.1f, 0.1f);
}
// keydown needs to be referenced when begin text input,
// on web it disables event handling so the button up event isn't received
// directly accessing these should only be used for debugging purposes, and
// not in release. TODO make it so that this is enforced
// by leaving them out when devtools is turned off
#define SAPP_KEYCODE_MAX SAPP_KEYCODE_MENU
bool keydown[SAPP_KEYCODE_MAX] = { 0 };
bool keypressed[SAPP_KEYCODE_MAX] = { 0 };
Vec2 mouse_movement = { 0 };

typedef struct {
	bool open;
	bool for_giving;
} ItemgridState;
ItemgridState item_grid_state = {0};

struct { char *key; void *value; } *immediate_state = 0;

void init_immediate_state() {
	sh_new_strdup(immediate_state);
}

void cleanup_immediate_state() {
	hmfree(immediate_state);
}

void *get_state_function(char *key, size_t value_size) {
	assert(key);

	if(shgeti(immediate_state, key) == -1) {
		shput(immediate_state, key, calloc(1, value_size));
	}

	return shget(immediate_state, key);
}

#define get_state(variable_name, type, ...) type* variable_name = get_state_function((char*)tprint(__VA_ARGS__).str, sizeof(*variable_name))

// set to true when should receive text input from the web input box
// or desktop text input
bool receiving_text_input = false;
float text_input_fade = 0.0f;

// called from the web to see if should do the text input modal
bool is_receiving_text_input()
{
	return receiving_text_input;
}

#ifdef DESKTOP
TextChunk text_input = TextChunkLitC("");
#else
#ifdef WEB
	EMSCRIPTEN_KEEPALIVE
void stop_controlling_input()
{
	_sapp_emsc_unregister_eventhandlers(); // stop getting input, hand it off to text input
}

	EMSCRIPTEN_KEEPALIVE
void start_controlling_input()
{
	memset(keydown, 0, sizeof(keydown));
	_sapp_emsc_register_eventhandlers();
}
#else
ISANERROR("No platform defined for text input!")
#endif // web

#endif // desktop

TextChunk text_input_result = {0};
TextInputResultKey text_result_key = 1;
bool text_ready = false;
TextInputResultKey begin_text_input(String8 placeholder_text)
{
	receiving_text_input = true; // this notifies the web layer that it should show the modal. Currently placeholder text is unimplemented on the web
#ifdef DESKTOP
	chunk_from_s8(&text_input, placeholder_text);
#endif
	text_result_key += 1;
	text_ready = false;
	return text_result_key;
}

// doesn't last for more than after this is called! Don't rely on it
// e.g Once text is ready for you and this returns a non empty string, the next call to this will return an empty string
String8 text_ready_for_me(TextInputResultKey key) {
	if(key == text_result_key && text_ready) {
		text_ready = false;
		return TextChunkString8(text_input_result);
	}
	return S8Lit("");
}

Vec2 FloorV2(Vec2 v)
{
	return V2(floorf(v.x), floorf(v.y));
}

Arena *frame_arena = 0;
Arena *persistent_arena = 0; // watch out, arenas have limited size. 

#ifdef WINDOWS
// uses frame arena
LPCWSTR windows_string(String8 s)
{
	int num_characters = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s.str, (int)s.size, 0, 0);
	wchar_t *to_return = PushArray(frame_arena, wchar_t, num_characters + 1); // also allocate for null terminating character
	assert(MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s.str, (int)s.size, to_return, num_characters) == num_characters);
	to_return[num_characters] = '\0';
	return to_return;
}
#endif

typedef enum
{
	GEN_Deleted = -1,
	GEN_NotDoneYet = 0,
	GEN_Success = 1,
	GEN_Failed = 2,
} GenRequestStatus;

#ifdef DESKTOP
#ifdef WINDOWS
#pragma warning(push, 3)
#pragma comment(lib, "WinHttp")
#include <WinHttp.h>
#include <process.h>
#pragma warning(pop)

typedef struct ChatRequest
{
	struct ChatRequest *next;
	struct ChatRequest *prev;
	bool user_is_done_with_this_request;
	bool thread_is_done_with_this_request;
	bool should_close;
	int id;
	int status;
	String8 generated;
	uintptr_t thread_handle;
	Arena *arena;
	String8 post_req_body; // allocated on thread_arena
} ChatRequest;

ChatRequest *requests_first = 0;
ChatRequest *requests_last = 0;

int next_request_id = 1;
ChatRequest *requests_free_list = 0;

void generation_thread(void* my_request_voidptr)
{
	ChatRequest *my_request = (ChatRequest*)my_request_voidptr;

	bool succeeded = true;

#define WinAssertWithErrorCode(X) if( !( X ) ) { unsigned int error = GetLastError(); Log("Error %u in %s\n", error, #X); my_request->status = GEN_Failed; return; }

	HINTERNET hSession = WinHttpOpen(L"PlayGPT winhttp backend", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	WinAssertWithErrorCode(hSession);

	LPCWSTR windows_server_name = windows_string(S8Lit(SERVER_DOMAIN));
	HINTERNET hConnect = WinHttpConnect(hSession, windows_server_name, SERVER_PORT, 0);
	WinAssertWithErrorCode(hConnect);
	int security_flags = 0;
	if(IS_SERVER_SECURE)
	{
		security_flags = WINHTTP_FLAG_SECURE;
	}

	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"completion", 0, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, security_flags); 
	WinAssertWithErrorCode(hRequest);

	// @IMPORTANT @TODO the windows_string allocates on the frame arena, but
	// according to https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpsendrequest
	// the buffer needs to remain available as long as the http request is running, so to make this async and do the loading thing need some other way to allocate the winndows string.... arenas bad?
	succeeded = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)my_request->post_req_body.str, (DWORD)my_request->post_req_body.size, (DWORD)my_request->post_req_body.size, 0);
	if(my_request->should_close) return;
	if(!succeeded)
	{
		Log("Couldn't do the web: %lu\n", GetLastError());
		my_request->status = GEN_Failed;
	}
	if(succeeded)
	{
		WinAssertWithErrorCode(WinHttpReceiveResponse(hRequest, 0));

		DWORD status_code;
		DWORD status_code_size = sizeof(status_code);
		WinAssertWithErrorCode(WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_code_size, WINHTTP_NO_HEADER_INDEX));
		Log("Status code: %lu\n", status_code);

		WinAssertWithErrorCode(status_code != 500);

		DWORD dwSize = 0;
		String8List received_data_list = {0};
		do
		{
			dwSize = 0;
			WinAssertWithErrorCode(WinHttpQueryDataAvailable(hRequest, &dwSize));

			if(dwSize == 0)
			{
			}
			else
			{
				u8* out_buffer = PushArrayZero(my_request->arena, u8, dwSize + 1);
				DWORD dwDownloaded = 0;
				WinAssertWithErrorCode(WinHttpReadData(hRequest, (LPVOID)out_buffer, dwSize, &dwDownloaded));
				Log("Got this from http, size %lu: %s\n", dwDownloaded, out_buffer);
				S8ListPush(my_request->arena, &received_data_list, S8(out_buffer, dwDownloaded));
			}
		} while (dwSize > 0);
		my_request->generated = S8ListJoin(my_request->arena, received_data_list, &(StringJoin){0});
		my_request->status = GEN_Success;
	}
	my_request->thread_is_done_with_this_request = true; // @TODO Threads that finish and users who forget to mark them as done aren't collected right now, we should do that to prevent leaks
}

int make_generation_request(String8 prompt)
{
	ArenaTemp scratch = GetScratch(0,0);
	String8 post_req_body = prompt;

	ChatRequest *to_return = 0;
	if(requests_free_list)
	{
		to_return = requests_free_list;
		StackPop(requests_free_list);
		*to_return = (ChatRequest){0};
	}
	else
	{
		to_return = PushArrayZero(persistent_arena, ChatRequest, 1);
	}
	to_return->arena = ArenaAlloc();
	to_return->id = next_request_id;
	next_request_id += 1;

	to_return->post_req_body.str = PushArrayZero(to_return->arena, u8, post_req_body.size);
	to_return->post_req_body.size = post_req_body.size;
	memcpy(to_return->post_req_body.str, post_req_body.str, post_req_body.size);

	to_return->thread_handle = _beginthread(generation_thread, 0, to_return);
	assert(to_return->thread_handle);

	DblPushBack(requests_first, requests_last, to_return);

	ReleaseScratch(scratch);
	return to_return->id;
}

// should never return null
// @TODO @IMPORTANT this doesn't work with save games because it assumes the id is always
// valid but saved IDs won't be valid on reboot
ChatRequest *get_by_id(int id)
{
	for(ChatRequest *cur = requests_first; cur; cur = cur->next)
	{
		if(cur->id == id && !cur->user_is_done_with_this_request)
		{
			return cur;
		}
	}
	return 0;
}

void done_with_request(int id)
{
	ChatRequest *req = get_by_id(id);
	if(req)
	{
		if(req->thread_is_done_with_this_request)
		{
			ArenaRelease(req->arena);
			DblRemove(requests_first, requests_last, req);
			*req = (ChatRequest){0};
			StackPush(requests_free_list, req);
		}
		else
		{
			req->user_is_done_with_this_request = true;
		}
	}
}
GenRequestStatus gen_request_status(int id)
{
	ChatRequest *req = get_by_id(id);
	if(!req)
		return GEN_Deleted;
	else
		return req->status;
}
// string does not last. Freed with the generation request
String8 gen_request_content(Arena *arena, int id)
{
	assert(get_by_id(id));
	return S8Copy(arena, get_by_id(id)->generated);
}

#else
ISANERROR("Only know how to do desktop http requests on windows")
#endif // WINDOWS
#endif // DESKTOP

#ifdef WEB
int make_generation_request(String8 prompt_str)
{
	ArenaTemp scratch = GetScratch(0, 0);
	String8 terminated_completion_url = nullterm(scratch.arena, FmtWithLint(scratch.arena, "%s://%s:%d/completion", IS_SERVER_SECURE ? "https" : "http", SERVER_DOMAIN, SERVER_PORT));
	int req_id = EM_ASM_INT({
		return make_generation_request(UTF8ToString($0, $1), UTF8ToString($2, $3));
	},
							prompt_str.str, (int)prompt_str.size, terminated_completion_url.str, (int)terminated_completion_url.size);
	ReleaseScratch(scratch);
	return req_id;
}
GenRequestStatus gen_request_status(int id)
{
	int status = EM_ASM_INT({
		return get_generation_request_status($0);
	}, id);

	return status;
}
String8 gen_request_content(Arena *arena, int id)
{
	int length_in_bytes = EM_ASM_INT({
		let generation = get_generation_request_content($0);
		return lengthBytesUTF8(generation);
	}, id) + 1;

	String8 ret = {0};
	ret.size = length_in_bytes;
	ret.str = PushArrayZero(arena, u8, ret.size);

	Log("Making string with length: %d\n", length_in_bytes);

	EM_ASM({
		let generation = get_generation_request_content($0);
		stringToUTF8(generation, $1, $2);
	},
		   id, ret.str, ret.size);

	ret = S8Substring(ret, 0, ret.size - 1); // chop off the null terminator
	Log("Received string with content '%.*s'\n", S8VArg(ret));
	Log("The last character of the string is '%d'\n", ret.str[ret.size - 1]);

	return ret;
}
void done_with_request(int id)
{
	EM_ASM({
		done_with_generation_request($0);
	},
		   id);
}
#endif // WEB

// json_in isn't the file node, it's the first child. So should be an array
FullResponse *json_to_responses(Arena *arena, Node *json_in, String8 *error_out) {
	FullResponse *ret = PushArrayZero(arena, FullResponse, 1);
	Response *cur_response = PushArrayZero(frame_arena, Response, 1);
	for(Node *cur = json_in; !NodeIsNil(cur); cur = cur->next) {
		memset(cur_response, 0, sizeof(*cur_response));
		chunk_from_s8(&cur_response->action, get_string(arena, cur, S8Lit("action")));
		if(cur_response->action.text_length == 0) {
			*error_out = FmtWithLint(arena, "Every action in the actions array has to have a field called 'action' which is the case sensitive action you are to perform");
			return ret;
		}

		String8List args = get_string_array(arena, cur, S8Lit("arguments"));
		if(args.node_count >= MAX_ARGS) {
			*error_out = FmtWithLint(arena, "You supplied %d arguments to the action %.*s but the maximum number of arguments is %d", (int)args.node_count, TCVArg(cur_response->action), MAX_ARGS);
			return ret;
		}
		for(String8Node *cur = args.first; cur; cur = cur->next) {
			TextChunk into = {0};
			chunk_from_s8(&into, cur->string);
			BUFF_APPEND(&cur_response->arguments, into);
		}

		BUFF_APPEND(ret, *cur_response);
	}
	return ret;
}

String8 tprint(char *format, ...)
{
	String8 to_return = {0};
	va_list argptr;
	va_start(argptr, format);

	to_return = S8FmtV(frame_arena, format, argptr);

	va_end(argptr);
	return nullterm(frame_arena, to_return);
}

bool V2ApproxEq(Vec2 a, Vec2 b)
{
	return LenV2(SubV2(a, b)) <= 0.01f;
}

float max_coord(Vec2 v)
{
	return v.x > v.y ? v.x : v.y;
}

AABB grow_from_center(AABB base, Vec2 growth)
{
	AABB ret;
	ret.upper_left = AddV2(base.upper_left, V2(-growth.x, growth.y));
	ret.lower_right = AddV2(base.lower_right, V2(growth.x, -growth.y));
	return ret;
}
AABB grow_from_ml(Vec2 ml, Vec2 offset, Vec2 size) {
	AABB ret;
	ret.upper_left = AddV2(AddV2(ml, offset), V2(0.0f, size.y/2.0f));
	ret.lower_right = AddV2(ret.upper_left, V2(size.x, -size.y)); 
	return ret;
}


// aabb advice by iRadEntertainment
Vec2 entity_aabb_size(Entity *e)
{
	if (e->is_npc)
	{
		return V2(1,1);
	}
	else
	{
		assert(false);
		return (Vec2) { 0 };
	}
}

float entity_radius(Entity *e)
{
	if (e->is_npc)
	{
		return 0.35f;
	}
	else
	{
		assert(false);
		return 0;
	}
}

Vec2 rotate_counter_clockwise(Vec2 v)
{
	return V2(-v.Y, v.X);
}

Vec2 rotate_clockwise(Vec2 v)
{
	return V2(v.y, -v.x);
}


Vec2 aabb_center(AABB aabb)
{
	return MulV2F(AddV2(aabb.upper_left, aabb.lower_right), 0.5f);
}

AABB aabb_centered(Vec2 at, Vec2 size)
{
	return (AABB) {
		.upper_left = AddV2(at, V2(-size.X / 2.0f, size.Y / 2.0f)),
			.lower_right = AddV2(at, V2(size.X / 2.0f, -size.Y / 2.0f)),
	};
}


AABB entity_aabb_at(Entity *e, Vec2 at)
{
	return aabb_centered(at, entity_aabb_size(e));
}

AABB entity_aabb(Entity *e)
{
	Vec2 at = e->pos;
	/* following doesn't work because in move_and_slide I'm not using this function
		 if(e->is_character) // aabb near feet
		 {
		 at = AddV2(at, V2(0.0f, -50.0f));
		 }
		 */
	return entity_aabb_at(e, at);
}

Entity *player(GameState *gs) {
	ENTITIES_ITER(gs->entities) {
		if(it->npc_kind == NPC_player) return it;
	}
	return 0;
}

Entity *world(GameState *gs) {
	ENTITIES_ITER(gs->entities) {
		if(it->is_world) return it;
	}
	return 0;
}

typedef struct LoadedImage
{
	struct LoadedImage *next;
	String8 name;
	sg_image image;
} LoadedImage;

LoadedImage *loaded_images = 0;

sg_image load_image(String8 path)
{
	ArenaTemp scratch = GetScratch(0, 0);
	for(LoadedImage *cur = loaded_images; cur; cur = cur->next)
	{
		if(S8Match(cur->name, path, 0))
		{
			return cur->image;
		}
	}

	LoadedImage *loaded = PushArray(persistent_arena, LoadedImage, 1);
	loaded->name = S8Copy(persistent_arena, path);
	StackPush(loaded_images, loaded);

	sg_image to_return = { 0 };

	int png_width, png_height, num_channels;
	const int desired_channels = 4;
	stbi_uc* pixels = stbi_load(
			(const char*)nullterm(frame_arena, path).str,
			&png_width, &png_height,
			&num_channels, 0);

	bool free_the_pixels = true;
	if(num_channels == 3)
	{
		Log("Forced to flip pixels for image '%.*s'\n", S8VArg(path));
		stbi_uc *old_pixels = pixels;
		pixels = ArenaPush(scratch.arena, png_width * png_height * 4 * sizeof(stbi_uc));
		for(u64 pixel_i = 0; pixel_i < png_width * png_height; pixel_i++)
		{
			pixels[pixel_i*4 + 0] = old_pixels[pixel_i*3 + 0];
			pixels[pixel_i*4 + 1] = old_pixels[pixel_i*3 + 1];
			pixels[pixel_i*4 + 2] = old_pixels[pixel_i*3 + 2];
			pixels[pixel_i*4 + 3] = 255;
		}
		num_channels = 4;
		free_the_pixels = false;
		stbi_image_free(old_pixels);
	}
	
	Log("Path %.*s | Loading image with dimensions %d %d\n", S8VArg(path), png_width, png_height);
	assert(pixels);
	assert(desired_channels == num_channels);
	
	to_return = sg_make_image(&(sg_image_desc)
			{
			.width = png_width,
			.height = png_height,
			.pixel_format = sapp_sgcontext().color_format,
			.num_mipmaps = 1,
			.data.subimage[0][0] =
			{
			.ptr = pixels,
			.size = (size_t)(png_width * png_height * num_channels),
			}
			});

	loaded->image = to_return;
	ReleaseScratch(scratch);
	return to_return;
}

SER_MAKE_FOR_TYPE(uint64_t);
SER_MAKE_FOR_TYPE(bool);
SER_MAKE_FOR_TYPE(double);
SER_MAKE_FOR_TYPE(float);
SER_MAKE_FOR_TYPE(PropKind);
SER_MAKE_FOR_TYPE(NpcKind);
SER_MAKE_FOR_TYPE(ActionKind);
SER_MAKE_FOR_TYPE(Memory);
SER_MAKE_FOR_TYPE(Vec2);
SER_MAKE_FOR_TYPE(Vec3);
SER_MAKE_FOR_TYPE(EntityRef);
SER_MAKE_FOR_TYPE(NPCPlayerStanding);
SER_MAKE_FOR_TYPE(u16);

void ser_Quat(SerState *ser, Quat *q)
{
	ser_float(ser, &q->x);
	ser_float(ser, &q->y);
	ser_float(ser, &q->z);
	ser_float(ser, &q->w);
}

typedef struct
{
	Vec3 offset;
	Quat rotation;
	Vec3 scale;
} Transform;

#pragma pack(1)
typedef struct
{
	Vec3 pos;
	Vec2 uv; // CANNOT have struct padding
}  Vertex;

#pragma pack(1)
typedef struct
{
	Vec3 position;
	Vec2 uv;
	u16  joint_indices[4];
	float joint_weights[4];
} ArmatureVertex;

SER_MAKE_FOR_TYPE(Vertex);

typedef struct Mesh
{
	struct Mesh *next;

	Vertex *vertices;
	u64 num_vertices;

	sg_buffer loaded_buffer;
	sg_image image;
	String8 name;
} Mesh;

typedef struct PoseBone
{
	float time; // time through animation this pose occurs at
	Transform parent_space_pose;
} PoseBone;

typedef struct Bone
{
	struct Bone *parent;
	Mat4 matrix_local;
	Mat4 inverse_model_space_pos;
	String8 name;
	float length;
} Bone;

typedef struct AnimationTrack
{
	PoseBone *poses;
	u64 poses_length;
} AnimationTrack;

typedef struct Animation
{
	String8 name;
	// assumed to be the same as the number of bones in the armature the animation is in 
	AnimationTrack *tracks; 
} Animation;

typedef struct
{
	Vec3 pos;
	Vec3 euler_rotation;
	Vec3 scale;
} BlenderTransform;

typedef struct PlacedMesh
{
	struct PlacedMesh *next;
	Transform t;
	Mesh *draw_with;
	String8 name;
} PlacedMesh;

typedef struct PlacedEntity
{
	struct PlacedEntity *next;
	Transform t;
	NpcKind npc_kind;
} PlacedEntity;

// mesh_name is for debugging
// arena must last as long as the Mesh lasts. Internal data points to `arena`, such as
// the name of the mesh's buffer in sokol. The returned mesh doesn't point to the binary
// file anymore.
Mesh load_mesh(Arena *arena, String8 binary_file, String8 mesh_name)
{
	ArenaTemp scratch = GetScratch(&arena, 1);
	SerState ser = {
		.data = binary_file.str,
		.max = binary_file.size,
		.arena = arena,
		.error_arena = scratch.arena,
		.serializing = false,
	};
	Mesh to_return = {0};

	bool is_armature;
	ser_bool(&ser, &is_armature);
	assert(!is_armature);

	String8 image_filename;
	ser_String8(&ser, &image_filename, scratch.arena);
	to_return.image = load_image(S8Fmt(scratch.arena, "assets/exported_3d/%.*s", S8VArg(image_filename)));

	ser_u64(&ser, &to_return.num_vertices);
	//Log("Mesh %.*s has %llu vertices and image filename '%.*s'\n", S8VArg(mesh_name), to_return.num_vertices, S8VArg(image_filename));

	to_return.vertices = ArenaPush(arena, sizeof(*to_return.vertices) * to_return.num_vertices);
	for(u64 i = 0; i < to_return.num_vertices; i++)
	{
		ser_Vertex(&ser, &to_return.vertices[i]);
	}

	assert(!ser.cur_error.failed);
	ReleaseScratch(scratch);

	to_return.loaded_buffer = sg_make_buffer(&(sg_buffer_desc)
			{
			.usage = SG_USAGE_IMMUTABLE,
			.data = (sg_range){.ptr = to_return.vertices, .size = to_return.num_vertices * sizeof(Vertex)},
			.label = (const char*)nullterm(arena, S8Fmt(arena, "%.*s-vertices", S8VArg(mesh_name))).str,
			});

	to_return.name = mesh_name;


	return to_return;
}

// stored in row major
typedef struct
{
	float elems[4 * 4];
} BlenderMat;

void ser_BlenderMat(SerState *ser, BlenderMat *b)
{
	for(int i = 0; i < 4 * 4; i++)
	{
		ser_float(ser, &b->elems[i]);
	}
}
Mat4 blender_to_handmade_mat(BlenderMat b)
{
	Mat4 to_return;
	assert(sizeof(to_return) == sizeof(b));
	memcpy(&to_return, &b, sizeof(to_return));
	return TransposeM4(to_return);
}
Mat4 transform_to_matrix(Transform t)
{
	Mat4 to_return = M4D(1.0f);

	to_return = MulM4(Scale(t.scale), to_return);
	to_return = MulM4(QToM4(t.rotation), to_return);
  to_return = MulM4(Translate(t.offset), to_return);

	return to_return;
}
Transform lerp_transforms(Transform from, float t, Transform to)
{
	return (Transform) {
			.offset = LerpV3(from.offset,  t, to.offset),
			.rotation = SLerp(from.rotation, t, to.rotation),
			.scale = LerpV3(from.scale,  t, to.scale),
	};
}
AABB lerp_aabbs(AABB from, float t, AABB to) {
	return (AABB) {
		.upper_left = LerpV2(from.upper_left, t, to.upper_left),
		.lower_right = LerpV2(from.lower_right, t, to.lower_right),
	};
}
Transform default_transform()
{
	return (Transform){.rotation = Make_Q(0,0,0,1), .scale = V3(1,1,1)};
}

typedef struct
{
	String8 name;

	Bone *bones;
	u64 bones_length;

	Animation *animations;
	u64 animations_length;

	// when set, blends to that animation next time this armature is processed for that
	String8 go_to_animation;
	bool next_animation_isnt_looping;

	Transform *current_poses; // allocated on loading of the armature
	String8 currently_playing_animation; // CANNOT be null.
	bool currently_playing_isnt_looping;
	float animation_blend_t; // [0,1] how much between current_animation and target_animation. Once >= 1, current = target and target = null.
	double cur_animation_time; // used for non looping animations to play once

	Transform *anim_blended_poses; // recalculated once per frame depending on above parameters, which at the same code location are calculated. Is `bones_length` long

	ArmatureVertex *vertices;
	u64 vertices_length;
	sg_buffer loaded_buffer;

	uint64_t last_updated_bones_frame;
	sg_image bones_texture;
	sg_image image;
	int bones_texture_width;
	int bones_texture_height;
} Armature;

void initialize_animated_properties(Arena *arena, Armature *armature) {
	armature->bones_texture = sg_make_image(&(sg_image_desc) {
		.width = armature->bones_texture_width,
		.height = armature->bones_texture_height,
		.pixel_format = SG_PIXELFORMAT_RGBA8,
		.usage = SG_USAGE_STREAM,
	});
	armature->current_poses = PushArray(arena, Transform, armature->bones_length);
	armature->anim_blended_poses = PushArray(arena, Transform, armature->bones_length);
	for(int i = 0; i < armature->bones_length; i++)
	{
		armature->anim_blended_poses[i] = (Transform){.scale = V3(1,1,1), .rotation = Make_Q(1,0,0,1)};
	}
}

// sokol stuff needs to be cleaned up, because it's not allocated and managed on the gamestate arena...
// this cleans up the animated properties, not the images and vertex buffers, as those are loaded
// before the gamestate is created, and the purpose of this is to allow you to delete/recreate
// the gamestate without leaking resources.
void cleanup_armature(Armature *armature) {
	sg_destroy_image(armature->bones_texture);
	armature->bones_texture = (sg_image){0};
}

// still points to the source armature's vertex data, but all animation data is duplicated.
// the armature is allocated onto the arena
Armature *duplicate_armature(Arena *arena, Armature *source) {
	Armature *ret = PushArrayZero(arena, Armature, 1);
	*ret = *source;
	initialize_animated_properties(arena, ret);
	return ret;
}

// armature_name is used for debugging purposes, it has to effect on things
Armature load_armature(Arena *arena, String8 binary_file, String8 armature_name)
{
	assert(binary_file.str);
	ArenaTemp scratch = GetScratch(&arena, 1);
	SerState ser = {
		.data = binary_file.str,
		.max = binary_file.size,
		.arena = arena,
		.error_arena = scratch.arena,
		.serializing = false,
	};
	Armature to_return = {0};

	bool is_armature;
	ser_bool(&ser, &is_armature);
	assert(is_armature);

	ser_String8(&ser, &to_return.name, arena);

	String8 image_filename;
	ser_String8(&ser, &image_filename, scratch.arena);
	arena->align = 16; // SSE requires quaternions are 16 byte aligned
	to_return.image = load_image(S8Fmt(scratch.arena, "assets/exported_3d/%.*s", S8VArg(image_filename)));

	ser_u64(&ser, &to_return.bones_length);
	//Log("Armature %.*s has %llu bones\n", S8VArg(armature_name), to_return.bones_length);
	to_return.bones = PushArray(arena, Bone, to_return.bones_length);

	for(u64 i = 0; i < to_return.bones_length; i++)
	{
		Bone *next_bone = &to_return.bones[i];

		BlenderMat model_space_pose;
		BlenderMat inverse_model_space_pose;
		i32 parent_index;

		ser_String8(&ser, &next_bone->name, arena);
		ser_int(&ser, &parent_index);
		ser_BlenderMat(&ser, &model_space_pose);
		ser_BlenderMat(&ser, &inverse_model_space_pose);
		ser_float(&ser, &next_bone->length);

		next_bone->matrix_local = blender_to_handmade_mat(model_space_pose);
		next_bone->inverse_model_space_pos = blender_to_handmade_mat(inverse_model_space_pose);

		if(parent_index != -1)
		{
			if(parent_index < 0 || parent_index >= to_return.bones_length)
			{
				ser.cur_error = (SerError){.failed = true, .why = S8Fmt(arena, "Parent index deserialized %d is out of range of the pose bones, which has a size of %llu", parent_index, to_return.bones_length)};
			}
			else
			{
				next_bone->parent = &to_return.bones[parent_index];
			}
		}
	}


	ser_u64(&ser, &to_return.animations_length);
	//Log("Armature %.*s has  %llu animations\n", S8VArg(armature_name), to_return.animations_length);
	to_return.animations = PushArray(arena, Animation, to_return.animations_length);

	for(u64 i = 0; i < to_return.animations_length; i++)
	{
		Animation *new_anim = &to_return.animations[i];
		*new_anim = (Animation){0};

		ser_String8(&ser, &new_anim->name, arena);

		new_anim->tracks = PushArray(arena, AnimationTrack, to_return.bones_length);

		u64 frames_in_anim;
		ser_u64(&ser, &frames_in_anim);
		//Log("There are %llu animation frames in animation '%.*s'\n", frames_in_anim, S8VArg(new_anim->name));

		for(u64 i = 0; i < to_return.bones_length; i++)
		{
			new_anim->tracks[i].poses = PushArray(arena, PoseBone, frames_in_anim);
			new_anim->tracks[i].poses_length = frames_in_anim;
		}

		for(u64 anim_i = 0; anim_i < frames_in_anim; anim_i++)
		{
			float time_through;
			ser_float(&ser, &time_through);
			for(u64 pose_bone_i = 0; pose_bone_i < to_return.bones_length; pose_bone_i++)
			{
				PoseBone *next_pose_bone = &new_anim->tracks[pose_bone_i].poses[anim_i];

				ser_Vec3(&ser, &next_pose_bone->parent_space_pose.offset);
				ser_Quat(&ser, &next_pose_bone->parent_space_pose.rotation);
				ser_Vec3(&ser, &next_pose_bone->parent_space_pose.scale);

				next_pose_bone->time = time_through;
			}
		}
	}

	ser_u64(&ser, &to_return.vertices_length);
	to_return.vertices = PushArray(arena, ArmatureVertex, to_return.vertices_length);
	for(u64 i = 0; i < to_return.vertices_length; i++)
	{
		ser_Vec3(&ser, &to_return.vertices[i].position);
		ser_Vec2(&ser, &to_return.vertices[i].uv);
		u16 joint_indices[4];
		float joint_weights[4];
		for(int ii = 0; ii < 4; ii++)
			ser_u16(&ser, &joint_indices[ii]);
		for(int ii = 0; ii < 4; ii++)
			ser_float(&ser, &joint_weights[ii]);

		for(int ii = 0; ii < 4; ii++)
			to_return.vertices[i].joint_indices[ii] = joint_indices[ii];
		for(int ii = 0; ii < 4; ii++)
			to_return.vertices[i].joint_weights[ii] = joint_weights[ii];
	}
	//Log("Armature %.*s has %llu vertices\n", S8VArg(armature_name), to_return.vertices_length);

	assert(!ser.cur_error.failed);
	ReleaseScratch(scratch);

	to_return.loaded_buffer = sg_make_buffer(&(sg_buffer_desc)
			{
			.usage = SG_USAGE_IMMUTABLE,
			.data = (sg_range){.ptr = to_return.vertices, .size = to_return.vertices_length * sizeof(ArmatureVertex)},
			.label = (const char*)nullterm(arena, S8Fmt(arena, "%.*s-vertices", S8VArg(armature_name))).str,
			});

	to_return.bones_texture_width = 16;
	to_return.bones_texture_height = (int)to_return.bones_length;

	//Log("Armature %.*s has bones texture size (%d, %d)\n", S8VArg(armature_name), to_return.bones_texture_width, to_return.bones_texture_height);
	initialize_animated_properties(arena, &to_return);

	// a sanity check
	SLICE_ITER(Bone, to_return.bones)
	{
		Mat4 should_be_identity = MulM4(it->matrix_local, it->inverse_model_space_pos);
		for(int r = 0; r < 4; r++)
		{
			for(int c = 0; c < 4; c++)
			{
				const float eps = 0.0001f;
				if(r == c)
				{
					assert(fabsf(should_be_identity.Elements[c][r] - 1.0f) < eps);
				}
				else
				{
					assert(fabsf(should_be_identity.Elements[c][r] - 0.0f) < eps);
				}
			}
		}
	}

	return to_return;
}

typedef struct
{
	Vec2 ul; // upper left
	Vec2 um; // upper middle
	Vec2 ur; // upper right
	Vec2 mr; // middle right
	Vec2 lr; // lower right
	Vec2 lm; // lower middle
	Vec2 ll; // lower left
	Vec2 ml; // middle left
} AABBStats;

bool aabb_is_valid(AABB aabb)
{
	Vec2 size_vec = SubV2(aabb.lower_right, aabb.upper_left); // negative in vertical direction
	return size_vec.Y <= 0.0f && size_vec.X >= 0.0f;
}


// positive in both directions
Vec2 aabb_size(AABB aabb)
{
	assert(aabb_is_valid(aabb));
	Vec2 size_vec = SubV2(aabb.lower_right, aabb.upper_left); // negative in vertical direction
	size_vec.y *= -1.0;
	return size_vec;
}

AABBStats stats(AABB aabb)
{
	Vec2 size = aabb_size(aabb);
	return (AABBStats){
		.ul = aabb.upper_left,
		.um = AddV2(aabb.upper_left, V2(size.x / 2.0f, 0.0)),
		.ur = AddV2(aabb.upper_left, V2(size.x, 0.0)),
		.mr = AddV2(aabb.lower_right, V2(0.0, size.y / 2.0f)),
		.lr = aabb.lower_right,
		.lm = AddV2(aabb.lower_right, V2(-size.x / 2.0f, 0.0)),
		.ll = AddV2(aabb.lower_right, V2(-size.x, 0.0)),
		.ml = AddV2(aabb.upper_left, V2(0.0, -size.y / 2.0f)),
	};
}

typedef struct CollisionShape
{
	struct CollisionShape *next;
	bool is_rect;
	AABB aabb;
	Circle circle;
} CollisionShape;

c2v v2_to_c2(Vec2 v) {
	return (c2v){.x = v.x, .y = v.y};
}
Vec2 c2_to_v2(c2v v) {
	return V2(v.x, v.y);
}

c2Circle shape_circle(CollisionShape shape) {
	return (c2Circle) {
		.p = v2_to_c2(shape.circle.center),
		.r = shape.circle.radius,
	};
}

c2AABB shape_aabb(CollisionShape shape) {
	return (c2AABB) {
		.min = v2_to_c2(stats(shape.aabb).ll),
		.max = v2_to_c2(stats(shape.aabb).ur),
	};
}




typedef struct Room
{
	struct Room *next;
	struct Room *prev;

	bool camera_offset_is_overridden;
	Vec3 camera_offset;
	String8 name;
	u64 roomid;
	PlacedMesh *placed_mesh_list;
	CollisionShape *collision_list;
	PlacedEntity *placed_entity_list;
} Room;

typedef struct
{
	Mesh *mesh_list;
	Room *room_list_first;
	Room *room_list_last;
} ThreeDeeLevel;



void ser_BlenderTransform(SerState *ser, BlenderTransform *t)
{
	ser_Vec3(ser, &t->pos);
	ser_Vec3(ser, &t->euler_rotation);
	ser_Vec3(ser, &t->scale);
}


Transform blender_to_game_transform(BlenderTransform blender_transform)
{
	Transform to_return = {0};

	to_return.offset = blender_transform.pos;

	to_return.scale.x = blender_transform.scale.x;
	to_return.scale.y = blender_transform.scale.z;
	to_return.scale.z = blender_transform.scale.y;

	Mat4 rotation_matrix = M4D(1.0f);
	rotation_matrix = MulM4(Rotate_RH(AngleRad(blender_transform.euler_rotation.x), V3(1,0,0)), rotation_matrix);
	rotation_matrix = MulM4(Rotate_RH(AngleRad(blender_transform.euler_rotation.y), V3(0,0,-1)), rotation_matrix);
	rotation_matrix = MulM4(Rotate_RH(AngleRad(blender_transform.euler_rotation.z), V3(0,1,0)), rotation_matrix);
	Quat out_rotation = M4ToQ_RH(rotation_matrix);
	to_return.rotation = out_rotation;

	return to_return;
}

ThreeDeeLevel load_level(Arena *arena, String8 binary_file)
{
	ArenaTemp scratch = GetScratch(&arena, 1);
	SerState ser = {
		.data = binary_file.str,
		.max = binary_file.size,
		.arena = arena,
		.error_arena = scratch.arena,
		.serializing = false,
	};
	ThreeDeeLevel out = {0};

	u64 num_rooms = 0;
	ser_u64(&ser, &num_rooms);

	for(u64 i = 0; i < num_rooms; i++)
	{
		Room *new_room = PushArray(arena, Room, 1);
		ser_String8(&ser, &new_room->name, arena);
		ser_u64(&ser, &new_room->roomid);

		ser_bool(&ser, &new_room->camera_offset_is_overridden);
		if(new_room->camera_offset_is_overridden)
		{
			ser_Vec3(&ser, &new_room->camera_offset);
		}

		// placed meshes
		{
			u64 num_placed = 0;
			ser_u64(&ser, &num_placed);
			arena->align = 16; // SSE requires quaternions are 16 byte aligned
			for (u64 i = 0; i < num_placed; i++)
			{
				PlacedMesh *new_placed = PushArray(arena, PlacedMesh, 1);
				// PlacedMesh *new_placed = calloc(sizeof(PlacedMesh), 1);

				ser_String8(&ser, &new_placed->name, arena);

				BlenderTransform blender_transform = {0};
				ser_BlenderTransform(&ser, &blender_transform);

				new_placed->t = blender_to_game_transform(blender_transform);

				StackPush(new_room->placed_mesh_list, new_placed);

				// Log("Placed mesh '%.*s' pos %f %f %f rotation %f %f %f %f scale %f %f %f\n", S8VArg(placed_mesh_name), v3varg(new_placed->t.offset), qvarg(new_placed->t.rotation), v3varg(new_placed->t.scale));

				// load the mesh if we haven't already
				bool mesh_found = false;
				for (Mesh *cur = out.mesh_list; cur; cur = cur->next)
				{
					if (S8Match(cur->name, new_placed->name, 0))
					{
						mesh_found = true;
						new_placed->draw_with = cur;
						assert(cur->name.size > 0);
						break;
					}
				}

				if (!mesh_found)
				{
					String8 to_load_filepath = S8Fmt(scratch.arena, "assets/exported_3d/%.*s.bin", S8VArg(new_placed->name));
					// Log("Loading mesh '%.*s'...\n", S8VArg(to_load_filepath));
					String8 binary_mesh_file = LoadEntireFile(scratch.arena, to_load_filepath);
					if (!binary_mesh_file.str)
					{
						ser.cur_error = (SerError){.failed = true, .why = S8Fmt(ser.error_arena, "Couldn't load file '%.*s'", to_load_filepath)};
					}
					else
					{
						Mesh *new_mesh = PushArray(arena, Mesh, 1);
						*new_mesh = load_mesh(arena, binary_mesh_file, new_placed->name);
						StackPush(out.mesh_list, new_mesh);
						new_placed->draw_with = new_mesh;
					}
				}
			}
		}

		u64 num_collision_cubes;
		ser_u64(&ser, &num_collision_cubes);
		for (u64 i = 0; i < num_collision_cubes; i++)
		{
			CollisionShape *new_shape = PushArray(arena, CollisionShape, 1);
			Vec2 twodee_pos;
			Vec2 size;
			ser_Vec2(&ser, &twodee_pos);
			ser_Vec2(&ser, &size);
			new_shape->is_rect = true;
			new_shape->aabb.upper_left = AddV2(twodee_pos, V2(-size.x, size.y));
			new_shape->aabb.lower_right = AddV2(twodee_pos, V2(size.x, -size.y));
			// new_shape->circle.center = twodee_pos;
			// new_shape->circle.radius = (size.x + size.y) * 0.5f; // @TODO(Phillip): @Temporary
			StackPush(new_room->collision_list, new_shape);
		}

		// placed entities
		{
			u64 num_placed = 0;
			ser_u64(&ser, &num_placed);
			arena->align = 16; // SSE requires quaternions are 16 byte aligned
			assert(num_placed == 0); // not thinking about how to go from name to entity kind right now, but in the future this will be for like machines or interactible things like the fishing rod
		}

		DblPushBack(out.room_list_first, out.room_list_last, new_room);
	}

	assert(!ser.cur_error.failed);
	ReleaseScratch(scratch);

	return out;
}

#include "assets.gen.c"
#include "threedee.glsl.h"

AABB level_aabb = { .upper_left = { 0.0f, 0.0f }, .lower_right = { TILE_SIZE * LEVEL_TILES, -(TILE_SIZE * LEVEL_TILES) } };
ThreeDeeLevel level_threedee = {0};
GameState gs = { 0 };
bool flycam = false;
Vec3 flycam_pos = {0};
float flycam_horizontal_rotation = 0.0;
float flycam_vertical_rotation = 0.0;
float flycam_speed = 1.0f;
Mat4 view = {0}; 
Mat4 projection = {0};

Room mystery_room = {
	.name = S8LitC("???"),
};
Room *get_room(ThreeDeeLevel *level, u64 roomid) {
	Room *ret = &mystery_room;
	for(Room *cur = level->room_list_first; cur; cur = cur->next)
	{
		if(cur->roomid == roomid)
		{
			ret = cur;
			break;
		}
	}
	return ret;
}
Room *get_cur_room(GameState *gs, ThreeDeeLevel *level)
{
	Room *in_room = 0;
	if(gs->edit.enabled) {
		in_room = get_room(level, gs->edit.current_roomid);
	} else {
		in_room = get_room(level, gs->current_roomid);
	}
	assert(in_room);
	return in_room;
}
int num_rooms(ThreeDeeLevel *level) {
	int ret = 0;
	for(Room *cur = level->room_list_first; cur; cur = cur->next) {
		ret++;
	}
	return ret;
}

Entity *npcs_entity(NpcKind kind) {
	if(kind == NPC_nobody) return 0;
	Entity *ret = 0;
	ENTITIES_ITER(gs.entities)
	{
		if (it->is_npc && it->npc_kind == kind)
		{
			assert(!ret); // no duplicate entities for this character. Bad juju when more than one npc
			ret = it;
		}
	}
	return ret;
}
CutsceneEvent *event_free_first = 0;
CutsceneEvent *event_free_last = 0;

CutsceneEvent *make_cutscene_event() {
	CutsceneEvent *ret = 0;
	if(event_free_first) {
		ret = event_free_first;
		DblRemove(event_free_first, event_free_last, event_free_first);
		*ret = (CutsceneEvent){0};
	} else {
		ret = PushArrayZero(persistent_arena, CutsceneEvent, 1);
	}
	ret->id = gs.next_cutscene_id;
	gs.next_cutscene_id += 1;
	return ret;
}

Vec4 IsPoint(Vec3 point)
{
	return V4(point.x, point.y, point.z, 1.0f);
}

Vec3 MulM4V3(Mat4 m, Vec3 v)
{
	return MulM4V4(m, IsPoint(v)).xyz;
}

typedef struct
{
	Vec3 right; // X+
	Vec3 forward; // Z-
	Vec3 up;
} Basis;

void print_matrix(Mat4 m)
{
	for(int r = 0; r < 4; r++)
	{
		for(int c = 0; c < 4; c++)
		{
			printf("%f ", m.Elements[c][r]);
		}
		printf("\n");
		//printf("%f %f %f %f\n", m.Columns[i].x, m.Columns[i].y, m.Columns[i].z, m.Columns[i].w);
	}

	printf("\n");

	for(int r = 0; r < 4; r++)
	{

		printf("%f %f %f %f\n", m.Columns[0].Elements[r], m.Columns[1].Elements[r], m.Columns[2].Elements[r], m.Columns[3].Elements[r]);
	}
}

Basis flycam_basis()
{
	// This basis function is wrong. Do not use it. Something about the order of rotations for
	// each basis vector is screwey
	Basis to_return = {
		.forward = V3(0,0,-1),
		.right = V3(1,0,0),
		.up = V3(0, 1, 0),
	};
	Mat4 rotate_horizontal = Rotate_RH(flycam_horizontal_rotation, V3(0, 1, 0));
	Mat4 rotate_vertical = Rotate_RH(flycam_vertical_rotation, V3(1, 0, 0));

	to_return.forward = MulM4V4(rotate_horizontal, MulM4V4(rotate_vertical, IsPoint(to_return.forward))).xyz;
	to_return.right = MulM4V4(rotate_horizontal, MulM4V4(rotate_vertical, IsPoint(to_return.right))).xyz;
	to_return.up = MulM4V4(rotate_horizontal, MulM4V4(rotate_vertical, IsPoint(to_return.up))).xyz;

	return to_return;
}

Mat4 flycam_matrix()
{
	Basis basis = flycam_basis();
	Mat4 to_return = {0};
	to_return.Columns[0] = IsPoint(basis.right);
	to_return.Columns[1] = IsPoint(basis.up);
	to_return.Columns[2] = IsPoint(basis.forward);
	to_return.Columns[3] = IsPoint(flycam_pos);
	return to_return;
}

#define S8LitConst(s)            \
	{                            \
		(u8 *)(s), sizeof(s) - 1 \
	}

String8 showing_secret_str = S8LitConst("");
float showing_secret_alpha = 0.0f;

PathCache cached_paths[32] = {0};

bool is_path_cache_old(double elapsed_time, PathCache *cache)
{
	double time_delta = elapsed_time - cache->elapsed_time;
	if (time_delta < 0.0)
	{
		// path was cached in the future... likely from old save or something. Always invalidate
		return true;
	}
	else
	{
		return time_delta >= TIME_BETWEEN_PATH_GENS;
	}
}

PathCacheHandle cache_path(double elapsed_time, AStarPath *path)
{
	ARR_ITER_I(PathCache, cached_paths, i)
	{
		if (!it->exists || is_path_cache_old(elapsed_time, it))
		{
			int gen = it->generation;
			*it = (PathCache){0};
			it->generation = gen + 1;

			it->path = *path;
			it->elapsed_time = elapsed_time;
			it->exists = true;
			return (PathCacheHandle){.generation = it->generation, .index = i};
		}
	}
	return (PathCacheHandle){0};
}

// passes in the time to return 0 and invalidate if too old
PathCache *get_path_cache(double elapsed_time, PathCacheHandle handle)
{
	if (handle.generation == 0)
	{
		return 0;
	}
	else
	{
		assert(handle.index >= 0);
		assert(handle.index < ARRLEN(cached_paths));
		PathCache *to_return = &cached_paths[handle.index];
		if (to_return->exists && to_return->generation == handle.generation)
		{
			if (is_path_cache_old(elapsed_time, to_return))
			{
				to_return->exists = false;
				return 0;
			}
			else
			{
				return to_return;
			}
		}
		else
		{
			return 0;
		}
	}
}

double unprocessed_gameplay_time = 0.0;
#define MINIMUM_TIMESTEP (1.0 / 60.0)

EntityRef frome(Entity *e)
{
	if (e == 0)
	{
		return (EntityRef){0};
	}
	else
	{
		EntityRef to_return = {
			.index = (int)(e - gs.entities),
			.generation = e->generation,
		};
		assert(to_return.index >= 0);
		assert(to_return.index < ARRLEN(gs.entities));
		return to_return;
	}
}

Entity *gete(EntityRef ref)
{
	return gete_specified(&gs, ref);
}


String8 stringify_identity(Entity *whos_perspective, NpcKind kind) {
	if(whos_perspective->npc_kind == kind) {
		return S8Lit("I");
	} else {
		return TCS8(npc_data(&gs, kind)->name);
	}
}

void remember_action(Entity *who_should_remember, Entity *from, Action act) {
	assert(who_should_remember); assert(from);
	String8 from_identified = stringify_identity(who_should_remember, from->npc_kind);
	String8 memory = {0};
	switch(act.kind){
		case ACT_invalid:
		break;
		case ACT_none:
		break;
		case ACT_say_to:
		{
			String8 to_identified = stringify_identity(who_should_remember, act.args.data[0].character->npc_kind);
			memory = FmtWithLint(frame_arena, "%.*s said out loud '%.*s' to %.*s", S8VArg(from_identified), TCVArg(act.args.data[1].text), S8VArg(to_identified));
		}
		break;
	}
	if(memory.size > 0) {
		Memory new_memory = {0};
		chunk_from_s8(&new_memory.description_from_my_perspective, S8Chop(memory, MAX_SENTENCE_LENGTH));
		BUFF_QUEUE_APPEND(&who_should_remember->memories, new_memory);
		if(who_should_remember != from) who_should_remember->perceptions_dirty = true;
	}
}

void perform_action(Entity *from, Action act) {
	ENTITIES_ITER(gs.entities) {
		if(it->is_npc && it->current_roomid == from->current_roomid) {
			remember_action(it, from, act);
		}
	}
	
	// turn the action into a cutscene
	if(from->npc_kind != NPC_player) {
		CutsceneEvent *new_event = make_cutscene_event();
		new_event->action = act;
		new_event->author = frome(from);
		DblPushBack(gs.unprocessed_first, gs.unprocessed_last, new_event);
	}
}

String8 npc_name(Entity *e){
	if(e->npc_kind == NPC_player) return S8Lit("The Player");
	else if(e->is_npc) return TCS8(npc_data(&gs, e->npc_kind)->name);
	else return S8Lit("");
}

CanTalkTo get_can_talk_to(Entity *e)
{
	CanTalkTo to_return = {0};
	ENTITIES_ITER(gs.entities)
	{
		if (it != e && (it->is_npc) && it->current_roomid == e->current_roomid)
		{
			BUFF_APPEND(&to_return, it);
		}
	}
	return to_return;
}

Entity *get_targeted(Entity *from, NpcKind targeted)
{
	ENTITIES_ITER(gs.entities)
	{
		if (it != from && (it->is_npc) && it->npc_kind == targeted)
		{
			if (it->current_roomid == from->current_roomid)
			{
				return it;
			}
		}
	}
	return 0;
}

u8 CharToUpper(u8 c);

String8 npc_identifier(String8 name)
{
	String8 ret;
	ret.str = PushArray(frame_arena, u8, name.size);
	ret.size = name.size;
	for (u64 i = 0; i < ret.size; i++)
	{
		ret.str[i] = CharToUpper(name.str[i]);
	}
	return ret;
}
// bad helper for now.
String8 npc_identifier_chunk(TextChunk chunk)
{
	return npc_identifier(TextChunkString8(chunk));
}

// returns reason why allocated on arena if invalid
// to might be null here, from can't be null
String8 is_action_valid(Arena *arena, Entity *from, ActionOld a)
{
	assert(a.speech.text_length <= MAX_SENTENCE_LENGTH && a.speech.text_length >= 0);
	assert(a.kind >= 0 && a.kind < ARRLEN(gamecode_actions));
	assert(from);

	String8 error_message = (String8){0};

	if (error_message.size == 0 && a.speech.text_length > 0)
	{
		if (S8FindSubstring(TextChunkString8(a.speech), S8Lit("assist"), 0, StringMatchFlag_CaseInsensitive) != a.speech.text_length)
		{
			error_message = S8Lit("You cannot use the word 'assist' in any form, you are not an assistant, do not act overtly helpful");
		}
	}

	CanTalkTo talk = get_can_talk_to(from);
	if (error_message.size == 0 && a.talking_to_kind)
	{
		bool found = false;
		BUFF_ITER(Entity *, &talk)
		{
			if ((*it)->npc_kind == a.talking_to_kind)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			error_message = FmtWithLint(arena, "Character you're talking to, %.*s, isn't in the same room and so can't be talked to", S8VArg(npc_identifier_chunk(npc_data(&gs, a.talking_to_kind)->name)));
		}
	}

	assert(error_message.size < MAX_SENTENCE_LENGTH); // is copied into text chunks

	return error_message;
}

// from must not be null
// the action must have been validated to be valid if you're calling this
void cause_action_side_effects(Entity *from, ActionOld a)
{
	assert(from);
	ArenaTemp scratch = GetScratch(0, 0);

	String8 failure_reason = is_action_valid(scratch.arena, from, a);
	if (failure_reason.size > 0)
	{
		Log("Failed to process action, invalid action: `%.*s`\n", S8VArg(failure_reason));
		assert(false);
	}

	Entity *to = 0;
	if (a.talking_to_kind != NPC_nobody)
	{
		to = get_targeted(from, a.talking_to_kind);
		assert(to);
	}

	if (to)
	{
		from->looking_at = frome(to);
	}

	ReleaseScratch(scratch);
}


bool eq(EntityRef ref1, EntityRef ref2)
{
	return ref1.index == ref2.index && ref1.generation == ref2.generation;
}

Entity *new_entity(GameState *gs)
{
	for (int i = 0; i < ARRLEN(gs->entities); i++)
	{
		if (!gs->entities[i].exists)
		{
			Entity *to_return = &gs->entities[i];
			int gen = to_return->generation;
			*to_return = (Entity){0};
			to_return->exists = true;
			to_return->generation = gen + 1;
			return to_return;
		}
	}
	assert(false);
	return 0;
}

typedef struct ToVisit
{
	struct ToVisit *next;
	struct ToVisit *prev;
	Node *ptr;
	int depth;
} ToVisit;

bool in_arr(ToVisit *arr, Node *n)
{
	for (ToVisit *cur = arr; cur; cur = cur->next)
	{
		if (cur->ptr == n)
			return true;
	}
	return false;
}

void dump_nodes(Node *node)
{
	ArenaTemp scratch = GetScratch(0, 0);
	ToVisit *horizon_first = 0;
	ToVisit *horizon_last = 0;

	ToVisit *visited = 0;

	ToVisit *first = PushArrayZero(scratch.arena, ToVisit, 1);
	first->ptr = node;
	DblPushBack(horizon_first, horizon_last, first);

	while (horizon_first)
	{
		ToVisit *cur_tovisit = horizon_first;
		DblRemove(horizon_first, horizon_last, cur_tovisit);
		StackPush(visited, cur_tovisit);
		char *tagstr = "   ";
		if (cur_tovisit->ptr->kind == NodeKind_Tag)
			tagstr = "TAG";
		printf("%s", tagstr);

		for (int i = 0; i < cur_tovisit->depth; i++)
			printf(" |");

		printf(" `%.*s`\n", S8VArg(cur_tovisit->ptr->string));

		for (Node *cur = cur_tovisit->ptr->first_child; !NodeIsNil(cur); cur = cur->next)
		{
			if (!in_arr(visited, cur))
			{
				ToVisit *new = PushArrayZero(scratch.arena, ToVisit, 1);
				new->depth = cur_tovisit->depth + 1;
				new->ptr = cur;
				DblPushFront(horizon_first, horizon_last, new);
			}
		}
		for (Node *cur = cur_tovisit->ptr->first_tag; !NodeIsNil(cur); cur = cur->next)
		{
			if (!in_arr(visited, cur))
			{
				ToVisit *new = PushArrayZero(scratch.arena, ToVisit, 1);
				new->depth = cur_tovisit->depth + 1;
				new->ptr = cur;
				DblPushFront(horizon_first, horizon_last, new);
			}
		}
	}

	ReleaseScratch(scratch);
}

// allocates the error on the arena
Node *expect_childnode(Arena *arena, Node *parent, String8 string, String8List *errors)
{
	Node *to_return = NilNode();
	if (errors->node_count == 0)
	{
		Node *child_node = MD_ChildFromString(parent, string, 0);
		if (NodeIsNil(child_node))
		{
			PushWithLint(arena, errors, "Couldn't find expected field %.*s", S8VArg(string));
		}
		else
		{
			to_return = child_node;
		}
	}
	return to_return;
}

int parse_enumstr_impl(Arena *arena, String8 enum_str, char **enumstr_array, int enumstr_array_length, String8List *errors, char *enum_kind_name, char *prefix)
{
	ArenaTemp scratch = GetScratch(&arena, 1);
	int to_return = -1;
	if (errors->node_count == 0)
	{
		String8 enum_name_looking_for = enum_str;
		if (enum_name_looking_for.size == 0)
		{
			PushWithLint(arena, errors, "`%s` string must be of size greater than 0", enum_kind_name);
		}
		else
		{
			for (int i = 0; i < enumstr_array_length; i++)
			{
				if (S8Match(FmtWithLint(scratch.arena, "%s%s", prefix, enumstr_array[i]), enum_name_looking_for, 0))
				{
					to_return = i;
					break;
				}
			}
		}
	}

	if (to_return == -1)
	{
		PushWithLint(arena, errors, "The %s `%.*s` could not be recognized in the game", enum_kind_name, S8VArg(enum_str));
	}

	ReleaseScratch(scratch);

	return to_return;
}

Vec3 plane_point(Vec2 p)
{
	return V3(p.x, 0.0, p.y);
}

Vec2 point_plane(Vec3 p)
{
	return V2(p.x, p.z);
}

#define parse_enumstr(arena, enum_str, errors, string_array, enum_kind_name, prefix) parse_enumstr_impl(arena, enum_str, string_array, ARRLEN(string_array), errors, enum_kind_name, prefix)

void transition_to_room(GameState *gs, ThreeDeeLevel *level, u64 new_roomid)
{
	Room *new_room = get_room(level, new_roomid);
	Log("Transitioning to %.*s...\n", S8VArg(new_room->name));
	assert(gs);
	gs->current_roomid = new_roomid;
}

// no crash or anything bad if called on an already cleaned up gamestate
void cleanup_gamestate(GameState *gs) {
	if(gs) {
		ENTITIES_ITER(gs->entities) {
			if(it->armature) {
				cleanup_armature(it->armature);
				it->armature = 0;
			}
		}
		if(gs->arena) {
			ArenaRelease(gs->arena);
		}
		memset(gs, 0, sizeof(GameState));
	}
}

// this can be called more than once, it cleanly handles all states of the gamestate
void initialize_gamestate(GameState *gs, u64 roomid) {
	if(!gs->arena) gs->arena = ArenaAlloc();
	if(!world(gs)) {
		Entity *world = new_entity(gs);
		world->is_world = true;
	}
	gs->current_roomid = roomid;
	rnd_gamerand_seed(&gs->random, RANDOM_SEED);
}

void reset_level()
{
	Log("STUB\n");
	// you prob want to do something like all dead entities are alive and reset to their editor positions.
	// This means entities need an editor spawnpoint position and a gameplay position....
}

enum
{
	V0,
	V1,
	// V2-V4 skipped because they're vector macros lol
	V5,
	VEditingDialog,
	VDescription,
	VDead,

	VMax,
} Version;

#define SER_BUFF(ser, BuffElemType, buff_ptr)                                                                                                                                                                     \
	{                                                                                                                                                                                                             \
		ser_int(ser, &((buff_ptr)->cur_index));                                                                                                                                                                   \
		if ((buff_ptr)->cur_index > ARRLEN((buff_ptr)->data))                                                                                                                                                     \
		{                                                                                                                                                                                                         \
			ser->cur_error = (SerError){.failed = true, .why = S8Fmt(ser->error_arena, "Current index %d is more than the buffer %s's maximum, %d", (buff_ptr)->cur_index, #buff_ptr, ARRLEN((buff_ptr)->data))}; \
		}                                                                                                                                                                                                         \
		BUFF_ITER(BuffElemType, buff_ptr)                                                                                                                                                                         \
		{                                                                                                                                                                                                         \
			ser_##BuffElemType(ser, it);                                                                                                                                                                          \
		}                                                                                                                                                                                                         \
	}

void ser_TextChunk(SerState *ser, TextChunk *t)
{
	ser_int(ser, &t->text_length);
	if (t->text_length >= ARRLEN(t->text))
	{
		ser->cur_error = (SerError){.failed = true, .why = S8Fmt(ser->error_arena, "In text chunk, length %d is too big to fit into %d", t->text_length, ARRLEN(t->text))};
	}
	ser_bytes(ser, (u8 *)t->text, t->text_length);
}

void ser_Entity(SerState *ser, Entity *e)
{
	ser_bool(ser, &e->exists);
	ser_bool(ser, &e->destroy);
	ser_int(ser, &e->generation);

	ser_Vec2(ser, &e->pos);
	ser_Vec2(ser, &e->vel);
	ser_float(ser, &e->damage);
	if(ser->version >= VDead) {
		ser_bool(ser, &e->dead);
		ser_u64(ser, &e->current_roomid);
	}

	ser_bool(ser, &e->is_world);
	ser_bool(ser, &e->is_npc);

	ser_bool(ser, &e->being_hovered);
	ser_bool(ser, &e->perceptions_dirty);

	/* rememboring errorlist is disabled for being a bad idea because as game is improved the errors go out of date, and to begin with it's not even that necessary
	But also it's too hard to maintain
	if(ser->serializing)
	{
		RememberedError *cur = e->errorlist_first;
		bool more_errors = cur != 0;
		ser_bool(ser, &more_errors);
		while(more_errors)
		{
			ser_TextChunk(ser, &cur->reason_why_its_bad);
			ser_Memory(ser, &cur->offending_self_output)
			cur = cur->next;
			more_errors = cur != 0;
			ser_bool(ser, &more_errors);
		}
	}
	else
	{
		bool more_errors;
		ser_bool(ser, &more_errors);
		while(more_errors)
		{
			TextChunkList *new_chunk = PushArray(ser->arena, TextChunkList, 1);
			ser_TextChunk(ser, &new_chunk->text);
			DblPushBack(e->errorlist_first, e->errorlist_last, new_chunk);
			ser_bool(ser, &more_errors);
		}
	}
	*/

	ser_float(ser, &e->dialog_panel_opacity);

	ser_bool(ser, &e->undismissed_action);
	ser_uint64_t(ser, &e->undismissed_action_tick);
	ser_float(ser, &e->characters_of_word_animated);
	ser_int(ser, &e->words_said_on_page);
	ser_int(ser, &e->cur_page_index);

	ser_NpcKind(ser, &e->npc_kind);
	ser_int(ser, &e->gen_request_id);
	ser_Vec2(ser, &e->target_goto);

	SER_BUFF(ser, Vec2, &e->position_history);
	ser_EntityRef(ser, &e->talking_to);
}

void ser_Npc(SerState *ser, Npc *npc)
{
	ser_TextChunk(ser, &npc->name);
	if(ser->version >= VDescription)
		ser_TextChunk(ser, &npc->description);
	ser_int(ser, &npc->kind);
}

void ser_EditorState(SerState *ser, EditorState *ed) {
	ser_bool(ser, &ed->enabled);
	ser_u64(ser, &ed->current_roomid);
	ser_Vec2(ser, &ed->camera_panning_target);
	ser_Vec2(ser, &ed->camera_panning);
	ser_NpcKind(ser, &ed->placing_npc);
	ser_NpcKind(ser, &ed->editing_npc);
	if(ser->version >= VEditingDialog)
		ser_bool(ser, &ed->editing_dialog_open);

	ser_bool(ser, &ed->placing_spawn);
	ser_u64(ser, &ed->player_spawn_roomid);
	ser_Vec2(ser, &ed->player_spawn_position);
}

void ser_GameState(SerState *ser, GameState *gs)
{
	if (ser->serializing)
		ser->version = VMax - 1;
	ser_int(ser, &ser->version);
	if (ser->version >= VMax)
	{
		ser->cur_error = (SerError){.failed = true, .why = S8Fmt(ser->error_arena, "Version %d is beyond the current version, %d", ser->version, VMax - 1)};
	}

	ser_uint64_t(ser, &gs->tick);
	ser_bool(ser, &gs->won);

	ser_EditorState(ser, &gs->edit);

	ser_double(ser, &gs->time);
	SER_BUFF(ser, Npc, &gs->characters);

	int num_entities = MAX_ENTITIES;
	ser_int(ser, &num_entities);

	assert(num_entities <= MAX_ENTITIES);
	for (int i = 0; i < num_entities; i++)
	{
		ser_bool(ser, &gs->entities[i].exists);
		if (gs->entities[i].exists)
		{
			ser_Entity(ser, &(gs->entities[i]));
		}
	}

	if (!ser->cur_error.failed)
	{
		if (world(gs) == 0)
		{
			ser->cur_error = (SerError){.failed = true, .why = S8Lit("No world entity found in deserialized entities")};
		}
	}

	if(!ser->serializing && gs->arena == 0)
		initialize_gamestate(gs, gs->current_roomid);
}

// error_out is allocated onto arena if it fails
String8 save_to_string(Arena *output_bytes_arena, Arena *error_arena, String8 *error_out, GameState *gs)
{
	u8 *serialized_data = 0;
	u64 serialized_length = 0;
	{
		SerState ser = {
			.version = VMax - 1,
			.serializing = true,
			.error_arena = error_arena,
		};
		ser_GameState(&ser, gs);

		if (ser.cur_error.failed)
		{
			*error_out = ser.cur_error.why;
		}
		else
		{
			ser.arena = 0; // serialization should never require allocation
			ser.max = ser.cur;
			ser.cur = 0;
			ser.version = VMax - 1;
			ArenaTemp temp = ArenaBeginTemp(output_bytes_arena);
			serialized_data = ArenaPush(temp.arena, ser.max);
			ser.data = serialized_data;

			ser_GameState(&ser, gs);
			if (ser.cur_error.failed)
			{
				Log("Very weird that serialization fails a second time...\n");
				*error_out = S8Fmt(error_arena, "VERY BAD Serialization failed after it already had no error: %.*s", ser.cur_error.why);
				ArenaEndTemp(temp);
				serialized_data = 0;
			}
			else
			{
				serialized_length = ser.cur;
			}
		}
	}
	return S8(serialized_data, serialized_length);
}

// error strings are allocated on error_arena, probably scratch for that. If serialization fails,
// nothing is allocated onto arena, the allocations are rewound
// If there was an error, the gamestate returned might be partially constructed and bad. Don't use it
GameState *load_from_string(Arena *arena, Arena *error_arena, String8 data, String8 *error_out)
{
	ArenaTemp temp = ArenaBeginTemp(arena);

	GameState *to_return = PushArrayZero(temp.arena, GameState, 1);
	initialize_gamestate(to_return, level_threedee.room_list_first->roomid);

	SerState ser = {
		.serializing = false,
		.data = data.str,
		.max = data.size,
		.arena = to_return->arena,
		.error_arena = error_arena,
	};

	ser_GameState(&ser, to_return);
	if (ser.cur_error.failed)
	{
		cleanup_gamestate(to_return);
		ArenaEndTemp(temp); // no allocations if it fails
		*error_out = ser.cur_error.why;
	}
	return to_return;
}

#ifdef WEB
EMSCRIPTEN_KEEPALIVE
void dump_save_data()
{
	ArenaTemp scratch = GetScratch(0, 0);

	String8 error = {0};
	String8 saved = save_to_string(scratch.arena, scratch.arena, &error, &gs);

	if (error.size > 0)
	{
		Log("Failed to save game: %.*s\n", S8VArg(error));
	}
	else
	{
		EM_ASM({
			save_game_data = new Int8Array(Module.HEAP8.buffer, $0, $1);
		},
			   (char *)(saved.str), saved.size);
	}

	ReleaseScratch(scratch);
}

EMSCRIPTEN_KEEPALIVE
void read_from_save_data(char *data, size_t length)
{
	// this leaks the gs memory if called over and voer again

	ArenaTemp scratch = GetScratch(0, 0);
	String8 data_str = S8((u8 *)data, length);

	// cleanup_gamestate()
	// initialize_gamestate(&gs, level_threedee.room_list_first->roomid);

	String8 error = {0};
	GameState *new_gs = load_from_string(scratch.arena, scratch.arena, data_str, &error);

	if (error.size > 0)
	{
		Log("Failed to load from size %lu: %.*s\n", length, S8VArg(error));
	}
	else
	{
		gs = *new_gs;
	}

	ReleaseScratch(scratch);
}
#endif

// a callback, when 'text backend' has finished making text. End dialog
void end_text_input(char *what_was_entered_cstr)
{
	String8 gotten = S8CString(what_was_entered_cstr);
	if (gotten.size == 0)
	{
		// means cancelled, so can do nothing
	}
	else
	{
		String8 trimmed = S8Chop(gotten, MAX_SENTENCE_LENGTH);
		String8 without_newlines = S8ListJoin(frame_arena, S8Split(frame_arena, trimmed, 1, &S8Lit("\n")), &(StringJoin){0});
		text_ready = true;
		chunk_from_s8(&text_input_result, without_newlines);
	}
	receiving_text_input = false;

	/*
	*/
}
/*
	 AnimatedSprite moose_idle =
	 {
	 .img = &image_moose,
	 .time_per_frame = 0.15,
	 .num_frames = 8,
	 .start = {0.0, 0.0},
	 .horizontal_diff_btwn_frames = 347.0f,
	 .region_size = {347.0f, 160.0f},
	 .offset = {-1.5f, -10.0f},
	 };
	 */

typedef struct
{
	sg_pass_action pass_action;
	sg_pass pass;
	sg_pipeline pip;
	sg_image color_img;
	sg_image depth_img;

	sg_pipeline armature_pip;
} Shadow_State;
Shadow_State init_shadow_state();

// @Place(sokol state struct)
static struct
{
	sg_pass_action clear_everything_pass_action;
	sg_pass_action threedee_msaa_pass_action;
	sg_pass_action clear_depth_buffer_pass_action;
	sg_pipeline twodee_pip;
	sg_bindings bind;

	sg_pipeline threedee_pip;
	sg_pipeline threedee_alpha_blended_pip;
	sg_pipeline armature_pip;
	sg_bindings threedee_bind;

	sg_image outline_pass_image;
	sg_image outline_pass_resolve_image;
	sg_pass outline_pass;
	sg_pipeline outline_mesh_pip;
	sg_pipeline outline_armature_pip;

	sg_pass threedee_pass; // is a pass so I can do post processing in a shader
	sg_image threedee_pass_image;
	sg_image threedee_pass_resolve_image;
	sg_image threedee_pass_depth_image;

	sg_pipeline twodee_outline_pip;
	sg_pipeline twodee_colorcorrect_pip;

	sg_sampler sampler_linear;
	sg_sampler sampler_linear_border;
	sg_sampler sampler_nearest;

	Shadow_State shadows;
} state;

// is a function, because also called when window resized to recreate the pass and the image.
// its target image must be the same size as the viewport. Is the reason. Cowabunga!
void create_screenspace_gfx_state()
{
	// this prevents common bug of whats passed to destroy func not being the resource
	// you queried to see if it exists
#define MAYBE_DESTROY(resource, destroy_func) \
	if (resource.id != 0)                     \
		destroy_func(resource);
	MAYBE_DESTROY(state.outline_pass, sg_destroy_pass);
	MAYBE_DESTROY(state.threedee_pass, sg_destroy_pass);

	MAYBE_DESTROY(state.outline_pass_image, sg_destroy_image);
	MAYBE_DESTROY(state.outline_pass_resolve_image, sg_destroy_image);
	MAYBE_DESTROY(state.threedee_pass_image, sg_destroy_image);
	MAYBE_DESTROY(state.threedee_pass_resolve_image, sg_destroy_image);
	MAYBE_DESTROY(state.threedee_pass_depth_image, sg_destroy_image);
#undef MAYBE_DESTROY

	const sg_shader_desc *shd_desc = threedee_mesh_outline_shader_desc(sg_query_backend());
	assert(shd_desc);
	sg_shader shd = sg_make_shader(shd_desc);

	state.outline_mesh_pip = sg_make_pipeline(&(sg_pipeline_desc){
		.shader = shd,
		.depth = {
			.pixel_format = SG_PIXELFORMAT_NONE,
		},
		.sample_count = SAMPLE_COUNT,
		.layout = {.attrs = {
					   [ATTR_threedee_vs_pos_in].format = SG_VERTEXFORMAT_FLOAT3,
					   [ATTR_threedee_vs_uv_in].format = SG_VERTEXFORMAT_FLOAT2,
				   }},
		.colors[0].blend = (sg_blend_state){
			// allow transparency
			.enabled = true,
			.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
			.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_rgb = SG_BLENDOP_ADD,
			.src_factor_alpha = SG_BLENDFACTOR_ONE,
			.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_alpha = SG_BLENDOP_ADD,
		},
		.label = "outline-mesh-pipeline",
	});

	shd_desc = threedee_armature_outline_shader_desc(sg_query_backend());
	assert(shd_desc);
	shd = sg_make_shader(shd_desc);

	state.outline_armature_pip = sg_make_pipeline(&(sg_pipeline_desc){
		.shader = shd,
		.depth = {
			.pixel_format = SG_PIXELFORMAT_NONE,
		},
		.sample_count = SAMPLE_COUNT,
		.layout = {.attrs = {
					   [ATTR_threedee_vs_skeleton_pos_in].format = SG_VERTEXFORMAT_FLOAT3,
					   [ATTR_threedee_vs_skeleton_uv_in].format = SG_VERTEXFORMAT_FLOAT2,
					   [ATTR_threedee_vs_skeleton_indices_in].format = SG_VERTEXFORMAT_USHORT4N,
					   [ATTR_threedee_vs_skeleton_weights_in].format = SG_VERTEXFORMAT_FLOAT4,
				   }},
		.colors[0].blend = (sg_blend_state){
			// allow transparency
			.enabled = true,
			.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
			.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_rgb = SG_BLENDOP_ADD,
			.src_factor_alpha = SG_BLENDFACTOR_ONE,
			.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_alpha = SG_BLENDOP_ADD,
		},
		.label = "outline-armature-pipeline",
	});

	sg_image_desc desc = {
		.render_target = true,
		.width = sapp_width(),
		.height = sapp_height(),
		.pixel_format = sapp_sgcontext().color_format,
		.label = "outline-pass-render-target",
	};

	desc.sample_count = SAMPLE_COUNT;
	state.outline_pass_image = sg_make_image(&desc);
	desc.sample_count = 1;
	state.outline_pass_resolve_image = sg_make_image(&desc);
	state.outline_pass = sg_make_pass(&(sg_pass_desc){
		.color_attachments[0].image = state.outline_pass_image,
		.resolve_attachments[0].image = state.outline_pass_resolve_image,
		.depth_stencil_attachment = {
			0},
		.label = "outline-pass",
	});

	desc.sample_count = 1;

	desc.label = "threedee-pass-render-target";
	desc.sample_count = SAMPLE_COUNT;
	state.threedee_pass_image = sg_make_image(&desc);

	desc.label = "threedee-pass-resolve-render-target";
	desc.sample_count = 1;
	state.threedee_pass_resolve_image = sg_make_image(&desc);

	desc.label = "threedee-pass-depth-render-target";
	desc.pixel_format = SG_PIXELFORMAT_DEPTH;
	desc.sample_count = SAMPLE_COUNT;
	state.threedee_pass_depth_image = sg_make_image(&desc);

	state.threedee_pass = sg_make_pass(&(sg_pass_desc){
		.color_attachments[0].image = state.threedee_pass_image,
		.resolve_attachments[0].image = state.threedee_pass_resolve_image,
		.depth_stencil_attachment = (sg_pass_attachment_desc){
			.image = state.threedee_pass_depth_image,
			.mip_level = 0,
		},
		.label = "threedee-pass",
	});
}

int num_draw_calls = 0;
int num_vertices = 0;

// if it's an invalid anim name, it just returns the idle animation
Animation *get_anim_by_name(Armature *armature, String8 anim_name)
{
	String8List anims = {0};
	for (u64 i = 0; i < armature->animations_length; i++)
	{
		S8ListPush(frame_arena, &anims, armature->animations[i].name);
		if (S8Match(armature->animations[i].name, anim_name, 0))
		{
			return &armature->animations[i];
		}
	}

	if (anim_name.size > 0)
	{
		String8 anims_str = S8ListJoin(frame_arena, anims, &(StringJoin){.mid = S8Lit(", ")});
		Log("No animation found '%.*s', the animations: [%.*s]\n", S8VArg(anim_name), S8VArg(anims_str));
	}

	for (u64 i = 0; i < armature->animations_length; i++)
	{
		if (S8Match(armature->animations[i].name, S8Lit("Idle"), 0))
		{
			return &armature->animations[i];
		}
	}

	assert(false); // no animation named 'Idle'
	return 0;
}

// you can pass a time greater than the animation length, it's fmodded to wrap no matter what.
Transform get_animated_bone_transform(AnimationTrack *track, float time, bool dont_loop)
{
	assert(track);
	float total_anim_time = track->poses[track->poses_length - 1].time;
	assert(total_anim_time > 0.0f);
	if (dont_loop)
	{
		time = fminf(time, total_anim_time);
	}
	else
	{
		time = fmodf(time, total_anim_time);
	}
	for (u64 i = 0; i < track->poses_length - 1; i++)
	{
		if (track->poses[i].time <= time && time <= track->poses[i + 1].time)
		{
			PoseBone from = track->poses[i];
			PoseBone to = track->poses[i + 1];
			float gap_btwn_keyframes = to.time - from.time;
			float t = (time - from.time) / gap_btwn_keyframes;
			assert(t >= 0.0f);
			assert(t <= 1.0f);
			return lerp_transforms(from.parent_space_pose, t, to.parent_space_pose);
		}
	}
	assert(false);
	return default_transform();
}

typedef struct
{
	u8 rgba[4];
} PixelData;

PixelData encode_normalized_float32(float to_encode)
{
	Vec4 to_return_vector = {0};

	// x is just -1.0f or 1.0f, encoded as a [0,1] normalized float.
	if (to_encode < 0.0f)
		to_return_vector.x = -1.0f;
	else
		to_return_vector.x = 1.0f;
	to_return_vector.x = to_return_vector.x / 2.0f + 0.5f;

	float without_sign = fabsf(to_encode);
	to_return_vector.y = without_sign - floorf(without_sign);

	to_return_vector.z = fabsf(to_encode) - to_return_vector.y;
	assert(to_return_vector.z < 255.0f);
	to_return_vector.z /= 255.0f;

	// w is unused for now, but is 1.0f (and is the alpha channel in Vec4) so that it displays properly as a texture
	to_return_vector.w = 1.0f;

	PixelData to_return = {0};

	for (int i = 0; i < 4; i++)
	{
		assert(0.0f <= to_return_vector.Elements[i] && to_return_vector.Elements[i] <= 1.0f);
		to_return.rgba[i] = (u8)(to_return_vector.Elements[i] * 255.0f);
	}

	return to_return;
}

float decode_normalized_float32(PixelData encoded)
{
	Vec4 v = {0};
	for (int i = 0; i < 4; i++)
	{
		v.Elements[i] = (float)encoded.rgba[i] / 255.0f;
	}

	float sign = 2.0f * v.x - 1.0f;

	float to_return = sign * (v.z * 255.0f + v.y);

	return to_return;
}

void audio_stream_callback(float *buffer, int num_frames, int num_channels)
{
	assert(num_channels == 2);
	const int num_samples = num_frames * num_channels;
	double time_per_sample = 1.0 / (double)SAMPLE_RATE;
	for (int i = 0; i < num_samples; i += num_channels)
	{
		float output_frames[2] = {0};
		for (int audio_i = 0; audio_i < ARRLEN(playing_audio); audio_i++)
		{
			AudioPlayer *it = &playing_audio[audio_i];
			if (it->sample != 0)
			{
				uint64_t pcm_position_int;
				float pcm_position_frac;
				cursor_pcm(it, &pcm_position_int, &pcm_position_frac);
				if (pcm_position_int + 1 >= it->sample->pcm_data_length)
				{
					it->sample = 0;
				}
				else
				{
					const int source_num_channels = it->sample->num_channels;
					float volume = (float)(it->volume + 1.0);
					if (source_num_channels == 1)
					{
						float src = Lerp(it->sample->pcm_data[pcm_position_int], pcm_position_frac, it->sample->pcm_data[pcm_position_int + 1]) * volume;
						output_frames[0] += src;
						output_frames[1] += src;
					}
					else if (source_num_channels == 2)
					{
						float src[2];
						src[0] = Lerp(it->sample->pcm_data[pcm_position_int * 2 + 0], pcm_position_frac, it->sample->pcm_data[(pcm_position_int + 1) * 2 + 0]) * volume;
						src[1] = Lerp(it->sample->pcm_data[pcm_position_int * 2 + 1], pcm_position_frac, it->sample->pcm_data[(pcm_position_int + 1) * 2 + 1]) * volume;
						output_frames[0] += src[0];
						output_frames[1] += src[1];
					}
					else
					{
						assert(false);
					}
					it->cursor_time += time_per_sample * (it->pitch + 1.0);
				}
			}
		}
		if (num_channels == 1)
		{
			buffer[i] = (output_frames[0] + output_frames[1]) * 0.5f;
		}
		else if (num_channels == 2)
		{
			buffer[i + 0] = output_frames[0];
			buffer[i + 1] = output_frames[1];
		}
	}
}

#define WHITE ((Color){1.0f, 1.0f, 1.0f, 1.0f})
#define GREY ((Color){0.4f, 0.4f, 0.4f, 1.0f})
#define BLACK ((Color){0.0f, 0.0f, 0.0f, 1.0f})
#define RED ((Color){1.0f, 0.0f, 0.0f, 1.0f})
#define PINK ((Color){1.0f, 0.0f, 1.0f, 1.0f})
#define BLUE ((Color){0.0f, 0.0f, 1.0f, 1.0f})
#define LIGHTBLUE ((Color){0.2f, 0.2f, 0.8f, 1.0f})
#define GREEN ((Color){0.0f, 1.0f, 0.0f, 1.0f})
#define BROWN (colhex(0x4d3d25))
#define YELLOW (colhex(0xffdd00))

Color oflightness(float dark)
{
	return (Color){dark, dark, dark, 1.0f};
}

Color colhex(uint32_t hex)
{
	int r = (hex & 0xff0000) >> 16;
	int g = (hex & 0x00ff00) >> 8;
	int b = (hex & 0x0000ff) >> 0;

	return (Color){(float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 1.0f};
}

Color blendcolors(Color a, float t, Color b)
{
	return LerpV4(a, t, b);
}

Color blendalpha(Color c, float alpha)
{
	Color to_return = c;
	to_return.a = alpha;
	return to_return;
}

// in pixels
Vec2 img_size(sg_image img)
{
	sg_image_desc desc = sg_query_image_desc(img);
	return V2((float)desc.width, (float)desc.height);
}

#ifdef DEVTOOLS
void do_metadesk_tests()
{
	Log("Testing metadesk library...\n");
	Arena *arena = ArenaAlloc();
	String8 s = S8Lit("This is a testing|string");

	String8List split_up = S8Split(arena, s, 1, &S8Lit("|"));

	assert(split_up.node_count == 2);
	assert(S8Match(split_up.first->string, S8Lit("This is a testing"), 0));
	assert(S8Match(split_up.last->string, S8Lit("string"), 0));

	ArenaRelease(arena);

	Log("Testing passed!\n");
}

// these tests rely on the base level having been loaded
void do_serialization_tests()
{
	Log("(UNIMPLEMENTED) Testing serialization...\n");

	/*
	   ArenaTemp scratch = GetScratch(0, 0);

	   GameState gs = {0};
	   initialize_gamestate_from_threedee_level(&gs, &level_threedee);

	   player(&gs)->pos = V2(50.0f, 0.0);

	   String8 error = {0};
	   String8 saved = save_to_string(scratch.arena, scratch.arena, &error, &gs);

	   assert(error.size == 0);
	   assert(saved.size > 0);
	   assert(saved.str != 0);

	   initialize_gamestate_from_threedee_level(&gs, &level_threedee);
	   gs = load_from_string(persistent_arena, scratch.arena, saved, &error);
	   assert(player(&gs)->pos.x == 50.0f);
	   assert(error.size == 0);

	   Log("Default save data size is %lld bytes\n", saved.size);

	   ReleaseScratch(scratch);
   */
}

void do_float_encoding_tests()
{
	float to_test[] = {
		7.5f,
		-2.12f,
		100.2f,
		-5.35f,
	};
	ARR_ITER(float, to_test)
	{
		PixelData encoded = encode_normalized_float32(*it);
		float decoded = decode_normalized_float32(encoded);
		assert(fabsf(decoded - *it) < 0.01f);
	}
}
#endif

typedef struct
{
	float font_size;
	float font_line_advance;
	float font_scale;
	String8 font_buffer;
	stbtt_bakedchar cdata[96]; // ascii characters?
	stbtt_fontinfo font;

	sg_image image;
	Vec2 size; // this image's size is queried a lot, and img_size seems to be slow when profiled
} LoadedFont;

LoadedFont default_font;
LoadedFont font_for_text_input; // is bigger

LoadedFont load_font(Arena *arena, String8 font_filepath, float font_size)
{
	LoadedFont to_return = {0};

	to_return.font_buffer = LoadEntireFile(arena, font_filepath);
	to_return.font_size = font_size;

	const int font_bitmap_width = 512;

	unsigned char *font_bitmap = ArenaPush(arena, font_bitmap_width * font_bitmap_width  );

	stbtt_BakeFontBitmap(to_return.font_buffer.str, 0, to_return.font_size, font_bitmap, font_bitmap_width, font_bitmap_width, 32, 96, to_return.cdata);

	unsigned char *font_bitmap_rgba = ArenaPush(frame_arena, 4 * font_bitmap_width * font_bitmap_width);

	// also flip the image, because I think opengl or something I'm too tired
	for (int row = 0; row < font_bitmap_width; row++)
	{
		for (int col = 0; col < font_bitmap_width; col++)
		{
			int i = row * 512 + col;
			int flipped_row = (font_bitmap_width - row);
			int flipped_i = flipped_row * 512 + col;
			font_bitmap_rgba[i * 4 + 0] = 255;
			font_bitmap_rgba[i * 4 + 1] = 255;
			font_bitmap_rgba[i * 4 + 2] = 255;
			font_bitmap_rgba[i * 4 + 3] = font_bitmap[flipped_i];
		}
	}

	to_return.image = sg_make_image(&(sg_image_desc){
		.width = font_bitmap_width,
		.height = font_bitmap_width,
		.pixel_format = SG_PIXELFORMAT_RGBA8,
		.data.subimage[0][0] =
			{
				.ptr = font_bitmap_rgba,
				.size = (size_t)(font_bitmap_width * font_bitmap_width * 4),
			}});

	to_return.size = img_size(to_return.image);

	// does the font_buffer.str need to be null terminated? As far as I can tell, no. In the header strlen is never called on it
	stbtt_InitFont(&to_return.font, to_return.font_buffer.str, 0);
	int ascent = 0;
	int descent = 0;
	int lineGap = 0;
	to_return.font_scale = stbtt_ScaleForPixelHeight(&to_return.font, to_return.font_size);
	stbtt_GetFontVMetrics(&to_return.font, &ascent, &descent, &lineGap);

	// this is from the header, not exactly sure why it works though
	to_return.font_line_advance = (float)(ascent - descent + lineGap) * to_return.font_scale * 0.75f;

	return to_return;
}

Armature player_armature = {0};
Armature farmer_armature = {0};
Armature shifted_farmer_armature = {0};
Armature man_in_black_armature = {0};
Armature angel_armature = {0};

// armatureanimations are processed once every visual frame from this list
Armature *armatures[] = {
	&player_armature,
	&farmer_armature,
	&shifted_farmer_armature,
	&man_in_black_armature,
	&angel_armature,
};

Mesh mesh_simple_worm = {0};
Mesh mesh_shotgun = {0};
Mesh mesh_spawnring = {0};
Mesh mesh_angel_totem = {0};
Mesh mesh_tombstone = {0};

void stbi_flip_into_correct_direction(bool do_it)
{
	if (do_it)
		stbi_set_flip_vertically_on_load(true);
}

String8 make_devtools_help(Arena *arena)
{
	ArenaTemp scratch = GetScratch(&arena, 1);
	String8List list = {0};

#define P(...) PushWithLint(scratch.arena, &list, __VA_ARGS__)

	P("===============================================\n");
	P("===============================================\n");
	P("~~~~~~~~~~~--Devtools is Activated!--~~~~~~~~~~\n");
	P("This means more asserts are turned on, and generally you are just better setup to detect errors and do some introspection on the program\n");
	P("\nKeyboard bindings and things you can do:\n");

	P("7 - toggles the visibility of devtools debug drawing and debug text\n");
	P("F - toggles flycam\n");
	P("Q - if you're hovering over somebody, pressing this will print to the console a detailed overview of their state\n");
	P("T - toggles freezing the mouse\n");
	P("9 - instantly wins the game\n");
	P("P - toggles spall profiling on/off, don't leave on for very long as it consumes a lot of storage if you do that. The resulting spall trace is saved to the file '%s'\n", PROFILING_SAVE_FILENAME);
	P("If you hover over somebody it will display some parts of their memories, can be somewhat helpful\n");
	P("J - judges the player and outputs their verdict to the console\n");

#undef P
	String8 to_return = S8ListJoin(arena, list, &(StringJoin){0});
	ReleaseScratch(scratch);
	return to_return;
}

void init(void)
{
	stbi_flip_into_correct_direction(true);
	init_immediate_state();
#ifdef WEB
	EM_ASM({
		set_server_url(UTF8ToString($0));
	},
		   SERVER_DOMAIN);
#endif

	frame_arena = ArenaAlloc();
#ifdef WEB
	next_arena_big = true;
#endif
	persistent_arena = ArenaAlloc();

#ifdef DEVTOOLS
	Log("Devtools is on!\n");
	String8 devtools_help = make_devtools_help(frame_arena);
	printf("%.*s\n", S8VArg(devtools_help));
#else
	Log("Devtools is off!\n");
#endif

	Log("Size of entity struct: %zu\n", sizeof(Entity));
	Log("Size of %d gs.entities: %zu kb\n", (int)ARRLEN(gs.entities), sizeof(gs.entities) / 1024);
	Log("Size of gamestate struct: %zu kb\n", sizeof(gs) / 1024);
	sg_setup(&(sg_desc){
		.context = sapp_sgcontext(),
		.buffer_pool_size = 512,
		.logger.func = slog_func,
	});
	stm_setup();
	saudio_setup(&(saudio_desc){
		.stream_cb = audio_stream_callback,
		.logger.func = slog_func,
		.num_channels = 2,
	});

	Log("Loading assets...\n");
	load_assets();
	Log("Done.\n");

	Log("Loading 3D assets...\n");

	String8 binary_file;

	binary_file = LoadEntireFile(frame_arena, S8Lit("assets/exported_3d/rooms.bin"));
	level_threedee = load_level(persistent_arena, binary_file);

	binary_file = LoadEntireFile(frame_arena, S8Lit("assets/exported_3d/Shotgun.bin"));
	mesh_shotgun = load_mesh(persistent_arena, binary_file, S8Lit("Shotgun.bin"));

	binary_file = LoadEntireFile(frame_arena, S8Lit("assets/exported_3d/SpawnRing.bin"));
	mesh_spawnring = load_mesh(persistent_arena, binary_file, S8Lit("SpawnRing.bin"));

	binary_file = LoadEntireFile(frame_arena, S8Lit("assets/exported_3d/AngelTotem.bin"));
	mesh_angel_totem = load_mesh(persistent_arena, binary_file, S8Lit("AngelTotem.bin"));

	binary_file = LoadEntireFile(frame_arena, S8Lit("assets/exported_3d/Tombstone.bin"));
	mesh_tombstone = load_mesh(persistent_arena, binary_file, S8Lit("Tombstone.bin"));

	binary_file = LoadEntireFile(frame_arena, S8Lit("assets/exported_3d/NormalGuyArmature.bin"));
	player_armature = load_armature(persistent_arena, binary_file, S8Lit("NormalGuyArmature.bin"));

	man_in_black_armature = load_armature(persistent_arena, binary_file, S8Lit("Man In Black"));
	man_in_black_armature.image = image_man_in_black;

	angel_armature = load_armature(persistent_arena, binary_file, S8Lit("Angel"));
	angel_armature.image = image_angel;

	binary_file = LoadEntireFile(frame_arena, S8Lit("assets/exported_3d/FarmerArmature.bin"));
	farmer_armature = load_armature(persistent_arena, binary_file, S8Lit("FarmerArmature.bin"));

	shifted_farmer_armature = load_armature(persistent_arena, binary_file, S8Lit("Farmer.bin"));
	shifted_farmer_armature.image = image_shifted_farmer;

	Log("Done. Used %f of the frame arena, %d kB\n", (double)frame_arena->pos / (double)frame_arena->cap, (int)(frame_arena->pos / 1024));

	ArenaClear(frame_arena);

	cleanup_gamestate(&gs);
	bool loaded_from_file = false;
	{
		String8 game_file = LoadEntireFile(frame_arena, S8Lit("assets/main_game_level.bin"));
		String8 error = {0};
		GameState *deserialized_gs = load_from_string(frame_arena, frame_arena, game_file, &error);

		if(error.size == 0) {
			gs = *deserialized_gs;
			loaded_from_file = true;
		}  else {
			Log("Failed to load from saved gamestate: %.*s\n", S8VArg(error));
		}
	}
	if(!loaded_from_file)
		initialize_gamestate(&gs, level_threedee.room_list_first->roomid);

#ifdef DEVTOOLS
	do_metadesk_tests();
	do_serialization_tests();
	do_float_encoding_tests();
#endif

#ifdef WEB
	EM_ASM({
		load_all();
	});
#endif

	default_font = load_font(persistent_arena, S8Lit("assets/PalanquinDark-Regular.ttf"), 35.0f);
	font_for_text_input = load_font(persistent_arena, S8Lit("assets/PalanquinDark-Regular.ttf"), 64.0f);

	state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
		.usage = SG_USAGE_STREAM,
	//.data = SG_RANGE(vertices),
#ifdef DEVTOOLS
		// this is so the debug drawing has more vertices to work with
		.size = 1024 * 2500,
#else
		.size = 1024 * 700,
#endif
		.label = "quad-vertices"});

	create_screenspace_gfx_state();
	state.shadows = init_shadow_state();

	const sg_shader_desc *desc = threedee_twodee_shader_desc(sg_query_backend());
	assert(desc);
	sg_shader shd = sg_make_shader(desc);

	Color clearcol = colhex(0x98734c);
	state.twodee_pip = sg_make_pipeline(&(sg_pipeline_desc){
		.shader = shd,
		.depth = {
			.compare = SG_COMPAREFUNC_LESS_EQUAL,
			.write_enabled = true},
		.layout = {.attrs = {
					   [ATTR_threedee_vs_twodee_position].format = SG_VERTEXFORMAT_FLOAT3,
					   [ATTR_threedee_vs_twodee_texcoord0].format = SG_VERTEXFORMAT_FLOAT2,
				   }},
		.colors[0].blend = (sg_blend_state){
			// allow transparency
			.enabled = true,
			.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
			.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_rgb = SG_BLENDOP_ADD,
			.src_factor_alpha = SG_BLENDFACTOR_ONE,
			.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_alpha = SG_BLENDOP_ADD,
		},
		.label = "quad-pipeline",
	});

	state.twodee_outline_pip = sg_make_pipeline(&(sg_pipeline_desc){
		.shader = sg_make_shader(threedee_twodee_outline_shader_desc(sg_query_backend())),
		.depth = {
			.compare = SG_COMPAREFUNC_LESS_EQUAL,
			.write_enabled = true},
		.layout = {.attrs = {
					   [ATTR_threedee_vs_twodee_position].format = SG_VERTEXFORMAT_FLOAT3,
					   [ATTR_threedee_vs_twodee_texcoord0].format = SG_VERTEXFORMAT_FLOAT2,
				   }},
		.colors[0].blend = (sg_blend_state){
			// allow transparency
			.enabled = true,
			.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
			.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_rgb = SG_BLENDOP_ADD,
			.src_factor_alpha = SG_BLENDFACTOR_ONE,
			.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_alpha = SG_BLENDOP_ADD,
		},
		.label = "twodee-outline",
	});

	state.twodee_colorcorrect_pip = sg_make_pipeline(&(sg_pipeline_desc){
		.shader = sg_make_shader(threedee_twodee_colorcorrect_shader_desc(sg_query_backend())),
		.depth = {
			.compare = SG_COMPAREFUNC_LESS_EQUAL,
			.write_enabled = true},
		.layout = {.attrs = {
					   [ATTR_threedee_vs_twodee_position].format = SG_VERTEXFORMAT_FLOAT3,
					   [ATTR_threedee_vs_twodee_texcoord0].format = SG_VERTEXFORMAT_FLOAT2,
				   }},
		.colors[0].blend = (sg_blend_state){
			// allow transparency
			.enabled = true,
			.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
			.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_rgb = SG_BLENDOP_ADD,
			.src_factor_alpha = SG_BLENDFACTOR_ONE,
			.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_alpha = SG_BLENDOP_ADD,
		},
		.label = "twodee-color-correct",
	});

	desc = threedee_mesh_shader_desc(sg_query_backend());
	assert(desc);
	shd = sg_make_shader(desc);

	{
		sg_pipeline_desc threedee_pip_desc = {
			.shader = shd,
			.layout = {.attrs = {
						   [ATTR_threedee_vs_pos_in].format = SG_VERTEXFORMAT_FLOAT3,
						   [ATTR_threedee_vs_uv_in].format = SG_VERTEXFORMAT_FLOAT2,
					   }},
			.depth = {
				.pixel_format = SG_PIXELFORMAT_DEPTH,
				.compare = SG_COMPAREFUNC_LESS_EQUAL,
				.write_enabled = true,
			},
			.alpha_to_coverage_enabled = true,
			.sample_count = SAMPLE_COUNT,
			.colors[0].blend.enabled = false,
			.label = "threedee",
		};
		state.threedee_pip = sg_make_pipeline(&threedee_pip_desc);
		threedee_pip_desc.depth.write_enabled = false;
		threedee_pip_desc.alpha_to_coverage_enabled = false;
		threedee_pip_desc.colors[0].blend = (sg_blend_state){
			// allow transparency
			.enabled = true,
			.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
			.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_rgb = SG_BLENDOP_ADD,
			.src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA,
			.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_alpha = SG_BLENDOP_ADD,
		},
		state.threedee_alpha_blended_pip = sg_make_pipeline(&threedee_pip_desc);
	}

	desc = threedee_armature_shader_desc(sg_query_backend());
	assert(desc);
	shd = sg_make_shader(desc);

	state.armature_pip = sg_make_pipeline(&(sg_pipeline_desc){
		.shader = shd,
		.depth = {
			.pixel_format = SG_PIXELFORMAT_DEPTH,
			.compare = SG_COMPAREFUNC_LESS_EQUAL,
			.write_enabled = true},
		.sample_count = SAMPLE_COUNT,
		.layout = {.attrs = {
					   [ATTR_threedee_vs_skeleton_pos_in].format = SG_VERTEXFORMAT_FLOAT3,
					   [ATTR_threedee_vs_skeleton_uv_in].format = SG_VERTEXFORMAT_FLOAT2,
					   [ATTR_threedee_vs_skeleton_indices_in].format = SG_VERTEXFORMAT_USHORT4N,
					   [ATTR_threedee_vs_skeleton_weights_in].format = SG_VERTEXFORMAT_FLOAT4,
				   }},
		.colors[0].blend = (sg_blend_state){
			// allow transparency
			.enabled = true,
			.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
			.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_rgb = SG_BLENDOP_ADD,
			.src_factor_alpha = SG_BLENDFACTOR_ONE,
			.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_alpha = SG_BLENDOP_ADD,
		},
		.label = "armature",
	});

	state.clear_depth_buffer_pass_action = (sg_pass_action){
		.colors[0] = {.load_action = SG_LOADACTION_LOAD},
		.depth = {.load_action = SG_LOADACTION_CLEAR, .clear_value = 1.0f},
	};
	state.clear_everything_pass_action = state.clear_depth_buffer_pass_action;
	state.clear_everything_pass_action.colors[0] = (sg_color_attachment_action){.load_action = SG_LOADACTION_CLEAR, .clear_value = {clearcol.r, clearcol.g, clearcol.b, 1.0f}};
	state.threedee_msaa_pass_action = state.clear_everything_pass_action;
	state.threedee_msaa_pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
	state.threedee_msaa_pass_action.colors[0].store_action = SG_STOREACTION_DONTCARE;

	state.sampler_linear = sg_make_sampler(&(sg_sampler_desc){
		.min_filter = SG_FILTER_LINEAR,
		.mag_filter = SG_FILTER_LINEAR,
		// .mipmap_filter = SG_FILTER_LINEAR,
		.wrap_u = SG_WRAP_CLAMP_TO_EDGE,
		.wrap_v = SG_WRAP_CLAMP_TO_EDGE,
		.max_anisotropy = 16,
		.label = "sampler-linear",
	});

	state.sampler_linear_border = sg_make_sampler(&(sg_sampler_desc){
		.min_filter = SG_FILTER_LINEAR,
		.mag_filter = SG_FILTER_LINEAR,
		.wrap_u = SG_WRAP_CLAMP_TO_BORDER,
		.wrap_v = SG_WRAP_CLAMP_TO_BORDER,
		.border_color = SG_BORDERCOLOR_OPAQUE_WHITE,
		.max_anisotropy = 16,
		.label = "sampler-linear-border",
	});

	state.sampler_nearest = sg_make_sampler(&(sg_sampler_desc){
		.min_filter = SG_FILTER_NEAREST,
		.mag_filter = SG_FILTER_NEAREST,

		// for webgl NPOT texures https://developer.mozilla.org/en-US/docs/Web/API/WebGL_API/Tutorial/Using_textures_in_WebGL
		.wrap_u = SG_WRAP_CLAMP_TO_EDGE,
		.wrap_v = SG_WRAP_CLAMP_TO_EDGE,
		.wrap_w = SG_WRAP_CLAMP_TO_EDGE,

		.label = "sampler-nearest",
	});
}

Vec2 screen_size()
{
	return V2((float)sapp_width(), (float)sapp_height());
}

typedef struct Camera
{
	Vec2 pos;
	float scale;
} Camera;

bool mobile_controls = false;
Vec2 thumbstick_base_pos = {0};
Vec2 thumbstick_nub_pos = {0};
typedef struct TouchMemory
{
	// need this because uintptr_t = 0 *doesn't* mean no touching!
	bool active;
	uintptr_t identifier;
} TouchMemory;
TouchMemory activate(uintptr_t by)
{
	// Log("Activating %ld\n", by);
	return (TouchMemory){.active = true, .identifier = by};
}
// returns if deactivated
bool maybe_deactivate(TouchMemory *memory, uintptr_t ended_identifier)
{
	if (memory->active)
	{
		if (memory->identifier == ended_identifier)
		{
			// Log("Deactivating %ld\n", memory->identifier);
			*memory = (TouchMemory){0};
			return true;
		}
	}
	else
	{
		return false;
	}
	return false;
}
TouchMemory movement_touch = {0};
TouchMemory roll_pressed_by = {0};
TouchMemory attack_pressed_by = {0};
TouchMemory interact_pressed_by = {0};
bool mobile_roll_pressed = false;
bool mobile_attack_pressed = false;
bool mobile_interact_pressed = false;

float thumbstick_base_size()
{
	if (screen_size().x < screen_size().y)
	{
		return screen_size().x * 0.24f;
	}
	else
	{
		return screen_size().x * 0.14f;
	}
}

float mobile_button_size()
{
	if (screen_size().x < screen_size().y)
	{
		return screen_size().x * 0.2f;
	}
	else
	{
		return screen_size().x * 0.09f;
	}
}

Vec2 roll_button_pos()
{
	return V2(screen_size().x - mobile_button_size(), screen_size().y * 0.4f);
}

Vec2 interact_button_pos()
{
	return V2(screen_size().x - mobile_button_size() * 2.0f, screen_size().y * (0.4f + (0.4f - 0.25f)));
}
Vec2 attack_button_pos()
{
	return V2(screen_size().x - mobile_button_size() * 2.0f, screen_size().y * 0.25f);
}

// everything is in pixels in world space, 43 pixels is approx 1 meter measured from
// merchant sprite being 5'6"
const float pixels_per_meter = 43.0f;

#define IMG(img) img, full_region(img)

// full region in pixels
AABB full_region(sg_image img)
{
	return (AABB){
		.upper_left = V2(0.0f, 0.0f),
		.lower_right = img_size(img),
	};
}

AABB aabb(Vec2 upper_left, Vec2 lower_right) {
	return (AABB) {
		upper_left, lower_right
	};
}

AABB aabb_at(Vec2 upper_left, Vec2 size)
{
	return (AABB){
		.upper_left = upper_left,
		.lower_right = AddV2(upper_left, V2(size.x, -size.y)),
	};
}

AABB screen_aabb()
{
	return aabb_at(V2(0.0, screen_size().y), screen_size());
}

AABB aabb_at_yplusdown(Vec2 at, Vec2 size)
{
	return (AABB){
		.upper_left = at,
		.lower_right = AddV2(at, V2(size.x, size.y)),
	};
}

Quad quad_at(Vec2 at, Vec2 size)
{
	Quad to_return;

	to_return.points[0] = V2(0.0, 0.0);
	to_return.points[1] = V2(size.X, 0.0);
	to_return.points[2] = V2(size.X, -size.Y);
	to_return.points[3] = V2(0.0, -size.Y);

	for (int i = 0; i < 4; i++)
	{
		to_return.points[i] = AddV2(to_return.points[i], at);
	}
	return to_return;
}

// out must be of at least length 4
Quad quad_centered(Vec2 at, Vec2 size)
{
	Quad to_return = quad_at(at, size);
	for (int i = 0; i < 4; i++)
	{
		to_return.points[i] = AddV2(to_return.points[i], V2(-size.X * 0.5f, size.Y * 0.5f));
	}
	return to_return;
}

Quad quad_rotated_centered(Vec2 at, Vec2 size, float rotation)
{
	Quad to_return = quad_centered(at, size);
	for (int i = 0; i < 4; i++)
	{
		to_return.points[i] = AddV2(RotateV2(SubV2(to_return.points[i], at), rotation), at);
	}
	return to_return;
}



Quad quad_aabb(AABB aabb)
{
	Vec2 size_vec = SubV2(aabb.lower_right, aabb.upper_left); // negative in vertical direction

	assert(aabb_is_valid(aabb));
	return (Quad){
		.ul = aabb.upper_left,
		.ur = AddV2(aabb.upper_left, V2(size_vec.X, 0.0f)),
		.lr = AddV2(aabb.upper_left, size_vec),
		.ll = AddV2(aabb.upper_left, V2(0.0f, size_vec.Y)),
	};
}

Quad centered_quad(Vec2 at, Vec2 size)
{
	return quad_aabb(aabb_centered(at, size));
}

// both segment_a and segment_b must be arrays of length 2
bool segments_overlapping(float *a_segment, float *b_segment)
{
	assert(a_segment[1] >= a_segment[0]);
	assert(b_segment[1] >= b_segment[0]);
	float total_length = (a_segment[1] - a_segment[0]) + (b_segment[1] - b_segment[0]);
	float farthest_to_left = fminf(a_segment[0], b_segment[0]);
	float farthest_to_right = fmaxf(a_segment[1], b_segment[1]);
	if (farthest_to_right - farthest_to_left < total_length)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool overlapping(AABB a, AABB b)
{
	// x axis

	{
		float a_segment[2] =
			{a.upper_left.X, a.lower_right.X};
		float b_segment[2] =
			{b.upper_left.X, b.lower_right.X};
		if (segments_overlapping(a_segment, b_segment))
		{
		}
		else
		{
			return false;
		}
	}

	// y axis

	{
		float a_segment[2] =
			{a.lower_right.Y, a.upper_left.Y};
		float b_segment[2] =
			{b.lower_right.Y, b.upper_left.Y};
		if (segments_overlapping(a_segment, b_segment))
		{
		}
		else
		{
			return false;
		}
	}

	return true; // both segments overlapping
}

bool overlapping_circle(Circle a, Circle b)
{
	Vec2 disp = SubV2(b.center, a.center);
	float dist = LenV2(disp);
	return (dist < a.radius + b.radius);
}

bool has_point(AABB aabb, Vec2 point)
{
	return (aabb.upper_left.X < point.X && point.X < aabb.lower_right.X) &&
		   (aabb.upper_left.Y > point.Y && point.Y > aabb.lower_right.Y);
}

AABB screen_cam_aabb()
{
	return (AABB){.upper_left = V2(0.0, screen_size().Y), .lower_right = V2(screen_size().X, 0.0)};
}

#define FLOATS_PER_VERTEX (3 + 2)
float cur_batch_data[1024 * 10] = {0};
int cur_batch_data_index = 0;
// @TODO check last tint as well, do this when factor into drawing parameters
sg_image cur_batch_image = {0};
threedee_twodee_fs_params_t cur_batch_params = {0};
sg_pipeline cur_batch_pipeline = {0};
void flush_quad_batch()
{
	if (cur_batch_image.id == 0 || cur_batch_data_index == 0)
		return; // flush called when image changes, image starts out null!

	if (cur_batch_pipeline.id != 0)
	{
		sg_apply_pipeline(cur_batch_pipeline);
	}
	else
	{
		sg_apply_pipeline(state.twodee_pip);
	}

	state.bind.vertex_buffer_offsets[0] = sg_append_buffer(state.bind.vertex_buffers[0], &(sg_range){cur_batch_data, cur_batch_data_index * sizeof(*cur_batch_data)});
	state.bind.fs.images[SLOT_threedee_twodee_tex] = cur_batch_image;			// NOTE that this might get FUCKED if a custom pipeline is provided with more/less texture slots!!!
	state.bind.fs.samplers[SLOT_threedee_fs_twodee_smp] = state.sampler_linear; // NOTE that this might get FUCKED if a custom pipeline is provided with more/less sampler slots!!!
	sg_apply_bindings(&state.bind);
	cur_batch_params.tex_size = img_size(cur_batch_image);
	cur_batch_params.screen_size = screen_size();
#if defined(SOKOL_GLCORE33) || defined(SOKOL_GLES3)
	cur_batch_params.flip_and_swap_rgb = 0;
#else
	cur_batch_params.flip_and_swap_rgb = 1;
#endif
	sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_threedee_twodee_fs_params, &SG_RANGE(cur_batch_params));
	cur_batch_params.tex_size = V2(0, 0); // unsure if setting the tex_size to something nonzero fucks up the batching so I'm just resetting it back here
	assert(cur_batch_data_index % FLOATS_PER_VERTEX == 0);
	sg_draw(0, cur_batch_data_index / FLOATS_PER_VERTEX, 1);
	num_draw_calls += 1;
	num_vertices += cur_batch_data_index / FLOATS_PER_VERTEX;
	memset(cur_batch_data, 0, cur_batch_data_index * sizeof(*cur_batch_data));
	cur_batch_data_index = 0;
}

typedef enum
{
	LAYER_INVALID,
	LAYER_WORLD,
	LAYER_UI,
	LAYER_UI_FG,
	LAYER_ANGEL_SCREEN,
	LAYER_UI_TEXTINPUT,
	LAYER_SCREENSPACE_EFFECTS,

	LAYER_ULTRA_IMPORTANT_NOTIFICATIONS,
	LAYER_LAST
} Layer;

typedef BUFF(char, 200) StacktraceElem;
typedef BUFF(StacktraceElem, 16) StacktraceInfo;

#if 0 // #ifdef WINDOWS
#include <dbghelp.h>
#pragma comment(lib, "DbgHelp")
StacktraceInfo get_stacktrace()
{
	StacktraceInfo to_return = {0};
	void *stack[ARRLEN(to_return.data)] = {0};
	int captured = CaptureStackBackTrace(0, ARRLEN(to_return.data), stack, 0);

	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);

	for(int i = 0; i < captured; i++)
	{
		StacktraceElem new_elem = {0};

		SYMBOL_INFO *symbol = calloc(sizeof(SYMBOL_INFO) + ARRLEN(new_elem.data), 1);

		symbol->MaxNameLen = ARRLEN(new_elem.data);
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

	
		if(!SymFromAddr(process, (DWORD64) stack[i], 0, symbol))
		{
			DWORD error_code = GetLastError();
			Log("Could not read stack trace: %lu\n", error_code);
			assert(false);
		}

		size_t symbol_name_len = strlen(symbol->Name);
		assert(symbol_name_len < ARRLEN(new_elem.data));
		memcpy(new_elem.data, symbol->Name, symbol_name_len);
		new_elem.cur_index = (int)symbol_name_len;
		BUFF_APPEND(&to_return, new_elem);

		free(symbol);
	}
	return to_return;
}
#else
StacktraceInfo get_stacktrace()
{
	return (StacktraceInfo){0};
}
#endif

typedef struct DrawParams
{
	Quad quad;
	sg_image image;
	AABB image_region;
	Color tint;

	AABB clip_to; // if world space is in world space, if screen space is in screen space - Lao Tzu
	int sorting_key;
	float alpha_clip_threshold;

	bool do_clipping;
	Layer layer;

	sg_pipeline custom_pipeline;

	// for debugging purposes
	int line_number;
} DrawParams;

Vec2 into_clip_space(Vec2 screen_space_point)
{
	Vec2 zero_to_one = DivV2(screen_space_point, screen_size());
	Vec2 in_clip_space = SubV2(MulV2F(zero_to_one, 2.0), V2(1.0, 1.0));
	return in_clip_space;
}

Vec4 inverse_perspective_divide(Vec4 divided_point, float what_was_w)
{
	// return V4(v.x / v.w, v.y / v.w, v.z / v.w, v.w / v.w);

	// f(v).x = v.x / v.w;
	// output_x = input_x / input_w;

	return V4(divided_point.x * what_was_w, divided_point.y * what_was_w, divided_point.z * what_was_w, what_was_w);
}
Vec3 screenspace_point_to_camera_point(Vec2 screenspace, Mat4 view)
{
	/*
	gl_Position = perspective_divide(projection * view * world_space_point);
	inverse_perspective_divide(gl_Position) = projection * view * world_space_point;
	proj_inverse * inverse_perspective_divide(gl_Position) = view * world_space_point;
	view_inverse * proj_inverse * inverse_perspective_divide(gl_Position) = world_space_point;
	*/

	Vec2 clip_space = into_clip_space(screenspace);
	Vec4 output_position = V4(clip_space.x, clip_space.y, -1.0, 1.0);
	float what_was_w = MulM4V4(projection, V4(0, 0, -NEAR_PLANE_DISTANCE, 1)).w;
	Vec3 to_return = MulM4V4(MulM4(InvGeneralM4(view), InvGeneralM4(projection)), inverse_perspective_divide(output_position, what_was_w)).xyz;

	return to_return;
}

Vec3 ray_intersect_plane(Vec3 ray_point, Vec3 ray_vector, Vec3 plane_point, Vec3 plane_normal)
{
	float d = DotV3(plane_point, MulV3F(plane_normal, -1.0f));

	float denom = DotV3(plane_normal, ray_vector);
	if (fabsf(denom) <= 1e-4f)
	{
		// also could mean doesn't intersect plane
		return plane_point;
	}
	assert(fabsf(denom) > 1e-4f); // avoid divide by zero

	float t = -(DotV3(plane_normal, ray_point) + d) / DotV3(plane_normal, ray_vector);

	if (t <= 1e-4)
	{
		// means doesn't intersect the plane, I think...
		return plane_point;
	}

	assert(t > 1e-4);

	return AddV3(ray_point, MulV3F(ray_vector, t));
}

typedef BUFF(DrawParams, 1024 * 5) RenderingQueue;
RenderingQueue rendering_queues[LAYER_LAST] = {0};

// The image region is in pixel space of the image
void draw_quad_impl(DrawParams d, int line)
{
	d.line_number = line;
	Vec2 *points = d.quad.points;

	AABB cam_aabb = screen_cam_aabb();
	AABB points_bounding_box = {.upper_left = V2(INFINITY, -INFINITY), .lower_right = V2(-INFINITY, INFINITY)};

	for (int i = 0; i < 4; i++)
	{
		points_bounding_box.upper_left.X = fminf(points_bounding_box.upper_left.X, points[i].X);
		points_bounding_box.upper_left.Y = fmaxf(points_bounding_box.upper_left.Y, points[i].Y);

		points_bounding_box.lower_right.X = fmaxf(points_bounding_box.lower_right.X, points[i].X);
		points_bounding_box.lower_right.Y = fminf(points_bounding_box.lower_right.Y, points[i].Y);
	}
	if (!overlapping(cam_aabb, points_bounding_box))
	{
		return; // cull out of screen quads
	}

	assert(d.layer >= 0 && d.layer < ARRLEN(rendering_queues));
	BUFF_APPEND(&rendering_queues[(int)d.layer], d);
}

#define draw_quad(...) draw_quad_impl(__VA_ARGS__, __LINE__)

int rendering_compare(const void *a, const void *b)
{
	DrawParams *a_draw = (DrawParams *)a;
	DrawParams *b_draw = (DrawParams *)b;

	return (int)((a_draw->sorting_key - b_draw->sorting_key));
}

void swapVec2(Vec2 *p1, Vec2 *p2)
{
	Vec2 tmp = *p1;
	*p1 = *p2;
	*p2 = tmp;
}
void swapfloat(float *a, float *b)
{
	float tmp = *a;
	*a = *b;
	*b = tmp;
}

Vec2 tile_id_to_coord(sg_image tileset_image, Vec2 tile_size, uint16_t tile_id)
{
	int tiles_per_row = (int)(img_size(tileset_image).X / tile_size.X);
	int tile_index = tile_id - 1;
	int tile_image_row = tile_index / tiles_per_row;
	int tile_image_col = tile_index - tile_image_row * tiles_per_row;
	Vec2 tile_image_coord = V2((float)tile_image_col * tile_size.X, (float)tile_image_row * tile_size.Y);
	return tile_image_coord;
}

void colorquad(Quad q, Color col)
{
	bool queue = false;
	if (col.A < 1.0f)
	{
		queue = true;
	}
	// y coord sorting for colorquad puts it below text for dialog panel
	draw_quad((DrawParams){q, image_white_square, full_region(image_white_square), col, .layer = LAYER_UI});
}

Vec2 NozV2(Vec2 v)
{
	if (v.x == 0.0f && v.y == 0.0f)
	{
		return V2(0.0f, 0.0f);
	}
	else
	{
		return NormV2(v);
	}
}

Quad line_quad(Vec2 from, Vec2 to, float line_width)
{
	Vec2 normal = rotate_counter_clockwise(NozV2(SubV2(to, from)));

	return (Quad){
		.points = {
			AddV2(from, MulV2F(normal, line_width)),  // upper left
			AddV2(to, MulV2F(normal, line_width)),	  // upper right
			AddV2(to, MulV2F(normal, -line_width)),	  // lower right
			AddV2(from, MulV2F(normal, -line_width)), // lower left
		}};
}

// in world coordinates
void line(Vec2 from, Vec2 to, float line_width, Color color)
{
	colorquad(line_quad(from, to, line_width), color);
}

#ifdef DEVTOOLS
bool show_devtools = false;
#ifdef PROFILING
extern bool profiling;
#else
bool profiling;
#endif
#else
const bool show_devtools = false;
#endif

bool having_errors = false;

Color debug_color = {1, 0, 0, 1};

#define dbgcol(col) DeferLoop(debug_color = col, debug_color = RED)

void dbgsquare(Vec2 at)
{
#ifdef DEVTOOLS
	if (!show_devtools)
		return;
	colorquad(quad_centered(at, V2(10.0, 10.0)), debug_color);
#else
	(void)at;
#endif
}

void dbgbigsquare(Vec2 at)
{
#ifdef DEVTOOLS
	if (!show_devtools)
		return;
	colorquad(quad_centered(at, V2(20.0, 20.0)), debug_color);
#else
	(void)at;
#endif
}

void dbgline(Vec2 from, Vec2 to)
{
#ifdef DEVTOOLS
	if (!show_devtools)
		return;
	line(from, to, 1.0f, debug_color);
#else
	(void)from;
	(void)to;
#endif
}

void dbgvec(Vec2 from, Vec2 vec)
{
	Vec2 to = AddV2(from, vec);
	dbgline(from, to);
}

// in world space
void dbgrect(AABB rect)
{
#ifdef DEVTOOLS
	if (!show_devtools)
		return;
	if (!aabb_is_valid(rect))
	{
		dbgsquare(rect.upper_left);
	}
	else
	{
		const float line_width = 0.5;
		Color col = debug_color;
		Quad q = quad_aabb(rect);
		line(q.ul, q.ur, line_width, col);
		line(q.ur, q.lr, line_width, col);
		line(q.lr, q.ll, line_width, col);
		line(q.ll, q.ul, line_width, col);
	}
#else
	(void)rect;
#endif
}

Vec3 perspective_divide(Vec4 v)
{
	return V3(v.x / v.w, v.y / v.w, v.z / v.w);
}

Vec2 threedee_to_screenspace(Vec3 world)
{
	// View and projection matrices must be initialized before calling this.
	// We detect if this isn't true and early out with some arbitrary values,
	// but really ideally shouldn't be happening at all
	if (view.Elements[3][3] == 0.0)
	{
		Log("Early outting from projection, uninitialized view\n");
		return V2(world.x, world.y);
	}
	else
	{
		Vec4 view_space = MulM4V4(view, IsPoint(world));
		Vec4 clip_space_no_perspective_divide = MulM4V4(projection, view_space);

		// sometimes camera might be at 0,0,0, directly where you want to deproject.
		// In that case the projected value is undefined, because the perspective
		// divide produces nans.
		Vec3 clip_space;

		if (clip_space_no_perspective_divide.z < 0.0)
		{
			return V2(0.0, 0.0);
		}

		if (clip_space_no_perspective_divide.w != 0.0)
		{
			clip_space = perspective_divide(clip_space_no_perspective_divide);
		}
		else
		{
			clip_space = clip_space_no_perspective_divide.xyz;
		}

		// clip is from -1 to 1, need to map back to screen
		Vec2 mapped_correctly = V2((clip_space.x + 1.0f) / 2.0f, (clip_space.y + 1.0f) / 2.0f);

		return V2(mapped_correctly.x * screen_size().x, mapped_correctly.y * screen_size().y);
	}
}

void dbg3dline(Vec3 from, Vec3 to)
{
	// https://learnopengl.com/img/getting-started/coordinate_systems.png
	// from and to are already in world space. apply view and projection to get clip space.
	// Finally convert to screen space then draw

	Vec2 from_screenspace = threedee_to_screenspace(from);
	Vec2 to_screenspace = threedee_to_screenspace(to);
	dbgline(from_screenspace, to_screenspace);
}

void dbg3dline2d(Vec2 a, Vec2 b)
{
	Vec3 a_3 = V3(a.x, 0.0, a.y);
	Vec3 b_3 = V3(b.x, 0.0, b.y);

	dbg3dline(a_3, b_3);
}

void dbg3dline2dOffset(Vec2 a, Vec2 offset)
{
	Vec3 a_3 = V3(a.x, 0.0, a.y);
	Vec3 b_3 = V3(offset.x, 0.0, offset.y);

	dbg3dline(a_3, AddV3(a_3, b_3));
}

void colorquadplane(Quad q, Color col)
{
	Quad warped = {0};
	for (int i = 0; i < 4; i++)
	{
		q.points[i] = threedee_to_screenspace(plane_point(q.points[i]));
	}
	colorquad(warped, col);
}

void dbgsquare3d(Vec3 point)
{
	Vec2 in_screen = threedee_to_screenspace(point);
	dbgsquare(in_screen);
}

void dbgplanesquare(Vec2 at)
{
	if (!show_devtools)
		return;
	colorquadplane(quad_centered(at, V2(3.0, 3.0)), debug_color);
}

void dbgplaneline(Vec2 from, Vec2 to)
{
	if (!show_devtools)
		return;
	dbg3dline(plane_point(from), plane_point(to));
}

void dbgplanevec(Vec2 from, Vec2 vec)
{
	if (!show_devtools)
		return;
	Vec2 to = AddV2(from, vec);
	dbgplaneline(from, to);
}

void dbgplanerect(AABB aabb)
{
	if (!show_devtools)
		return;
	Quad q = quad_aabb(aabb);
	dbgplaneline(q.ul, q.ur);
	dbgplaneline(q.ur, q.lr);
	dbgplaneline(q.lr, q.ll);
	dbgplaneline(q.ll, q.ul);
}

typedef struct
{
	Mesh *mesh;
	Armature *armature;
	Transform t;

	float seed; // to make time unique in shaders, shaders can choose to add the seed
	float wobble_factor;
	Vec3 wobble_world_source;
	bool outline;
	bool no_dust;
	bool alpha_blend;
	bool dont_cast_shadows;
} DrawnThing;

int drawn_this_frame_length = 0;
DrawnThing drawn_this_frame[MAXIMUM_THREEDEE_THINGS] = {0};

void draw_thing(DrawnThing params)
{
	drawn_this_frame[drawn_this_frame_length] = params;
	drawn_this_frame_length += 1;
#ifdef DEVTOOLS
	assert(drawn_this_frame_length < MAXIMUM_THREEDEE_THINGS);
#else
	if (drawn_this_frame_length >= MAXIMUM_THREEDEE_THINGS)
	{
		Log("Drawing too many things!\n");
		drawn_this_frame_length = MAXIMUM_THREEDEE_THINGS - 1;
	}
#endif
}

typedef struct TextParams
{
	bool dry_run;
	String8 text;
	Vec2 pos;
	Color color;
	float scale;
	AABB clip_to;  // if in world space, in world space. In space of pos given
	Color *colors; // color per character, if not null must be array of same length as text
	bool do_clipping;
	LoadedFont *use_font; // if null, uses default font
	Layer layer;
} TextParams;

AABB update_bounds(AABB bounds, Quad quad) {
	for (int i = 0; i < 4; i++)
	{
		bounds.upper_left.X = fminf(bounds.upper_left.X, quad.points[i].X);
		bounds.upper_left.Y = fmaxf(bounds.upper_left.Y, quad.points[i].Y);
		bounds.lower_right.X = fmaxf(bounds.lower_right.X, quad.points[i].X);
		bounds.lower_right.Y = fminf(bounds.lower_right.Y, quad.points[i].Y);
	}
	return bounds;
}

// returns bounds. To measure text you can set dry run to true and get the bounds
AABB draw_text(TextParams t)
{
	AABB bounds = {0};
	LoadedFont font = default_font;
	if (t.use_font)
		font = *t.use_font;
	PROFILE_SCOPE("draw text")
	{
		size_t text_len = t.text.size; // CANNOT include the null terminator at the end! Check for this
		float y = 0.0;
		float x = 0.0;
		for (int i = 0; i < text_len; i++)
		{
			stbtt_aligned_quad q;
			float old_y = y;
			PROFILE_SCOPE("get baked quad")
			stbtt_GetBakedQuad(font.cdata, 512, 512, t.text.str[i] - 32, &x, &y, &q, 1);
			float difference = y - old_y;
			y = old_y + difference;

			Vec2 size = V2(q.x1 - q.x0, q.y1 - q.y0);
			if (t.text.str[i] == '\n')
			{
#ifdef DEVTOOLS
				y += font.font_size * 0.75f; // arbitrary, only debug t.text has newlines
				x = 0.0;
#else
				assert(false);
#endif
			}
			if (size.Y > 0.0 && size.X > 0.0)
			{ // spaces (and maybe other characters) produce quads of size 0
				Quad to_draw;
				PROFILE_SCOPE("Calculate to draw quad")
				{
					to_draw = (Quad){
						.points = {
							AddV2(V2(q.x0, -q.y0), V2(0.0f, 0.0f)),
							AddV2(V2(q.x0, -q.y0), V2(size.X, 0.0f)),
							AddV2(V2(q.x0, -q.y0), V2(size.X, -size.Y)),
							AddV2(V2(q.x0, -q.y0), V2(0.0f, -size.Y)),
						},
					};
				}

				PROFILE_SCOPE("Scale points")
				for (int i = 0; i < 4; i++)
				{
					to_draw.points[i] = MulV2F(to_draw.points[i], t.scale);
				}

				AABB font_atlas_region = (AABB){
					.upper_left = V2(q.s0, 1.0f - q.t1),
					.lower_right = V2(q.s1, 1.0f - q.t0),
				};
				font_atlas_region.upper_left.y += 1.0f / 512.0f;
				font_atlas_region.lower_right.y += 1.0f / 512.0f;
				PROFILE_SCOPE("Scaling font atlas region to img font size")
				{
					font_atlas_region.upper_left.X *= font.size.X;
					font_atlas_region.lower_right.X *= font.size.X;
					font_atlas_region.upper_left.Y *= font.size.Y;
					font_atlas_region.lower_right.Y *= font.size.Y;
				}

				PROFILE_SCOPE("bounds computation")
				bounds = update_bounds(bounds, to_draw);

				PROFILE_SCOPE("shifting points")
				for (int i = 0; i < 4; i++)
				{
					to_draw.points[i] = AddV2(to_draw.points[i], t.pos);
				}

				if (!t.dry_run)
				{
					PROFILE_SCOPE("Actually drawing")
					{
						Color col = t.color;
						if (t.colors)
						{
							col = t.colors[i];
						}

						Layer to_use = LAYER_UI_FG;
						if (t.layer != LAYER_INVALID)
						{
							to_use = t.layer;
						}
						draw_quad((DrawParams){to_draw, font.image, font_atlas_region, col, t.clip_to, .layer = to_use, .do_clipping = t.do_clipping});
					}
				}
			}
		}

		bounds.upper_left = AddV2(bounds.upper_left, t.pos);
		bounds.lower_right = AddV2(bounds.lower_right, t.pos);
	}

	return bounds;
}

float get_vertical_dist_between_lines(LoadedFont for_font, float text_scale)
{
	return for_font.font_line_advance * text_scale * 0.9f;
}

AABB draw_centered_text(TextParams t)
{
	if (t.scale <= 0.01f)
		return (AABB){0};
	t.dry_run = true;
	AABB text_aabb = draw_text(t);
	t.dry_run = false;
	Vec2 center_pos = t.pos;
	LoadedFont *to_use = &default_font;
	if (t.use_font)
		to_use = t.use_font;
	// t.pos = AddV2(center_pos, MulV2F(aabb_size(text_aabb), -0.5f));
	t.pos = V2(center_pos.x - aabb_size(text_aabb).x / 2.0f, center_pos.y - get_vertical_dist_between_lines(*to_use, t.scale) / 2.0f);
	return draw_text(t);
}

int sorting_key_at(Vec2 pos)
{
	return -(int)pos.y;
}

// gets aabbs overlapping the input aabb, including gs.entities
Overlapping get_overlapping(AABB aabb)
{
	Overlapping to_return = {0};

	// the gs.entities jessie
	PROFILE_SCOPE("checking the entities")
	ENTITIES_ITER(gs.entities)
	{
		if (!it->is_world && overlapping(aabb, entity_aabb(it)))
		{
			BUFF_APPEND(&to_return, it);
		}
	}


	return to_return;
}

typedef struct CollisionInfo
{
	bool happened;
	Vec2 normal;
	BUFF(Entity *, 8)
	with;
} CollisionInfo;

typedef struct MoveSlideParams
{
	Entity *from;
	Vec2 position;
	Vec2 movement_this_frame;

	// optional
	bool dont_collide_with_entities;
	CollisionInfo *col_info_out;
} MoveSlideParams;

Vec2 get_penetration_vector(AABB stable, AABB dynamic)
{
	// Assumes we already know that they are colliding.
	// It could be faster to use this info for collision detection as well,
	// but this would require an intrusive refactor, and it is not the common
	// case that things are colliding anyway, so it's actually not that much
	// duplicated work.
	Vec2 dynamic_centre = aabb_center(dynamic);
	Vec2 dynamic_half_dims = MulV2F(aabb_size(dynamic), 0.5f);

	stable.lower_right.x += dynamic_half_dims.x;
	stable.lower_right.y -= dynamic_half_dims.y;
	stable.upper_left.x -= dynamic_half_dims.x;
	stable.upper_left.y += dynamic_half_dims.y;

	float right_delta = stable.lower_right.x - dynamic_centre.x;
	float left_delta = stable.upper_left.x - dynamic_centre.x;
	float bottom_delta = stable.lower_right.y - dynamic_centre.y;
	float top_delta = stable.upper_left.y - dynamic_centre.y;

	float r = fabsf(right_delta);
	float l = fabsf(left_delta);
	float b = fabsf(bottom_delta);
	float t = fabsf(top_delta);

	if (r <= l && r <= b && r <= t)
		return V2(right_delta, 0.0);
	if (left_delta <= r && l <= b && l <= t)
		return V2(left_delta, 0.0);
	if (b <= r && b <= l && b <= t)
		return V2(0.0, bottom_delta);
	return V2(0.0, top_delta);
}

// returns new pos after moving and sliding against collidable things
Vec2 move_and_slide(MoveSlideParams p)
{
	float collision_radius = entity_radius(p.from);
	Vec2 new_pos = AddV2(p.position, p.movement_this_frame);

	assert(collision_radius > 0.0f);
	Circle at_new = {new_pos, collision_radius};
	typedef struct
	{
		CollisionShape shape;
		Entity *e; // required
	} CollisionObj;
	BUFF(CollisionObj, 256)
	to_check = {0};

	// add world boxes
	for (CollisionShape *cur = get_cur_room(&gs, &level_threedee)->collision_list; cur; cur = cur->next)
	{
		BUFF_APPEND(&to_check, ((CollisionObj){*cur, world(&gs)}));
	}

	// add entity boxes
	if (!p.dont_collide_with_entities)
	{
		ENTITIES_ITER(gs.entities)
		{
			if (it != p.from && !(it->is_npc && it->dead) && !it->is_world && it->current_roomid == p.from->current_roomid)
			{
				BUFF_APPEND(&to_check, ((CollisionObj){(CollisionShape){.circle.center = it->pos, .circle.radius = entity_radius(it)}, it}));
			}
		}
	}

	// here we do some janky C stuff to resolve collisions with the closest
	// box first, because doing so is a simple heuristic to avoid depenetrating and losing
	// sideways velocity. It's visual and I can't put diagrams in code so uh oh!

	typedef BUFF(CollisionObj, 32) OverlapBuff;
	OverlapBuff actually_overlapping = {0};

	BUFF_ITER(CollisionObj, &to_check)
	{
		c2Circle as_circle = shape_circle((CollisionShape){.circle = at_new});
		bool overlapping = false;
		if(it->shape.is_rect) {
			c2AABB into = shape_aabb(it->shape);
			overlapping = c2Collided(&as_circle, 0, C2_TYPE_CIRCLE, &into, 0, C2_TYPE_AABB);
		} else {
			c2Circle into = shape_circle(it->shape);
			overlapping = c2Collided(&as_circle, 0, C2_TYPE_CIRCLE, &into, 0, C2_TYPE_CIRCLE);
		}
		if(overlapping) BUFF_APPEND(&actually_overlapping, *it);
	}

	float smallest_distance = FLT_MAX;
	int smallest_circle_index = 0;
	int i = 0;
	BUFF_ITER(CollisionObj, &actually_overlapping)
	{
		float cur_dist;
		if(it->shape.is_rect) {
			cur_dist = LenV2(SubV2(at_new.center, aabb_center(it->shape.aabb)));
		} else {
			cur_dist = LenV2(SubV2(at_new.center, it->shape.circle.center));
		}
		if (cur_dist < smallest_distance)
		{
			smallest_distance = cur_dist;
			smallest_circle_index = i;
		}
		i++;
	}

	OverlapBuff overlapping_smallest_first = {0};
	if (actually_overlapping.cur_index > 0)
	{
		BUFF_APPEND(&overlapping_smallest_first, actually_overlapping.data[smallest_circle_index]);
	}
	BUFF_ITER_I(CollisionObj, &actually_overlapping, i)
	{
		if (i == smallest_circle_index)
		{
		}
		else
		{
			BUFF_APPEND(&overlapping_smallest_first, *it);
		}
	}

	// overlapping

	// overlapping_smallest_first = actually_overlapping;

	CollisionInfo info = {0};
	for (int col_iter_i = 0; col_iter_i < 1; col_iter_i++)
		BUFF_ITER(CollisionObj, &overlapping_smallest_first)
		{
			CollisionShape from = {.circle = at_new};
			c2Manifold manifold = {0};
			if(it->shape.is_rect) {
				c2CircletoAABBManifold(shape_circle(from), shape_aabb(it->shape), &manifold);
			} else {
				c2CircletoCircleManifold(shape_circle(from), shape_circle(it->shape), &manifold);
			}
			Vec2 resolution_vector = NozV2(MulV2F(c2_to_v2(manifold.n), -1.0f));
			for(int i = 0; i < manifold.count; i++) {
				at_new.center = AddV2(at_new.center, MulV2F(resolution_vector, manifold.depths[i]));
			}

			bool happened_with_this_one = true;

			if (happened_with_this_one)
			{
				bool already_in_happened = false;
				Entity *e = it->e;
				if (e)
				{
					BUFF_ITER(Entity *, &info.with)
					{
						if (e == *it)
						{
							already_in_happened = true;
						}
					}
					if (!already_in_happened)
					{
						if (!BUFF_HAS_SPACE(&info.with))
						{
							Log("WARNING not enough space in collision info out\n");
						}
						else
						{
							BUFF_APPEND(&info.with, e);
						}
					}
				}
			}
		}

	if (p.col_info_out)
		*p.col_info_out = info;

	Vec2 result_pos = at_new.center;
	return result_pos;
}

float character_width(LoadedFont for_font, int ascii_letter, float text_scale)
{
	int advanceWidth;
	stbtt_GetCodepointHMetrics(&for_font.font, ascii_letter, &advanceWidth, 0);
	return (float)advanceWidth * for_font.font_scale * text_scale;
}

// they're always joined by spaces anyways, so even if you add more delims
// spaces will be added between them inshallah.
String8List split_by_word(Arena *arena, String8 string)
{
	String8 word_delimeters[] = {S8Lit(" ")};
	return S8Split(arena, string, ARRLEN(word_delimeters), word_delimeters);
}

typedef struct PlacedWord
{
	struct PlacedWord *next;
	struct PlacedWord *prev;
	String8 text;
	Vec2 lower_left_corner;
	Vec2 size;
	int line_index;
} PlacedWord;

typedef struct
{
	PlacedWord *first;
	PlacedWord *last;
} PlacedWordList;

typedef enum
{
	JUST_LEFT,
	JUST_CENTER,
} TextJustification;

PlacedWordList place_wrapped_words(Arena *arena, String8List words, float text_scale, float maximum_width, LoadedFont for_font, TextJustification just)
{
	PlacedWordList to_return = {0};
	ArenaTemp scratch = GetScratch(&arena, 1);

	int current_line_index = 0;
	{
		Vec2 at_position = V2(0.0, 0.0);
		Vec2 cur = at_position;
		float space_size = character_width(for_font, (int)' ', text_scale);
		float current_vertical_offset = 0.0f; // goes negative
		for (String8Node *next_word = words.first; next_word; next_word = next_word->next)
		{
			if (next_word->string.size == 0)
			{
			}
			else
			{
				AABB word_bounds = draw_text((TextParams){true, next_word->string, V2(0.0, 0.0), .scale = text_scale, .use_font = &for_font});
				word_bounds.lower_right.x += space_size;
				float next_x_position = cur.x + aabb_size(word_bounds).x;
				if (next_x_position - at_position.x > maximum_width)
				{
					current_vertical_offset -= get_vertical_dist_between_lines(for_font, text_scale); // the 1.1 is just arbitrary padding because it looks too crowded otherwise
					cur = AddV2(at_position, V2(0.0f, current_vertical_offset));
					current_line_index += 1;
					next_x_position = cur.x + aabb_size(word_bounds).x;
				}

				PlacedWord *new_placed = PushArray(arena, PlacedWord, 1);
				new_placed->text = next_word->string;
				new_placed->lower_left_corner = cur;
				new_placed->size = aabb_size(word_bounds);
				new_placed->line_index = current_line_index;

				DblPushBack(to_return.first, to_return.last, new_placed);

				cur.x = next_x_position;
			}
		}
	}

	switch (just)
	{
	case JUST_LEFT:
		break;
	case JUST_CENTER:
	{
		if(to_return.first)
		for (int i = to_return.first->line_index; i <= to_return.last->line_index; i++)
		{
			PlacedWord *first_on_line = 0;
			PlacedWord *last_on_line = 0;

			for (PlacedWord *cur = to_return.first; cur; cur = cur->next)
			{
				if (cur->line_index == i)
				{
					if (first_on_line == 0)
					{
						first_on_line = cur;
					}
				}
				else if (cur->line_index > i)
				{
					last_on_line = cur->prev;
					assert(last_on_line->line_index == i);
					break;
				}
			}
			if (first_on_line && !last_on_line)
			{
				last_on_line = to_return.last;
			}

			float total_width = fabsf(first_on_line->lower_left_corner.x - (last_on_line->lower_left_corner.x + last_on_line->size.x));
			float midpoint = total_width / 2.0f;
			for (PlacedWord *cur = first_on_line; cur != last_on_line->next; cur = cur->next)
			{
				cur->lower_left_corner.x = (cur->lower_left_corner.x - midpoint) + maximum_width / 2.0f;
			}
		}
	}
	break;
	}

	ReleaseScratch(scratch);
	return to_return;
}

void translate_words_by(PlacedWordList words, Vec2 translation)
{
	for (PlacedWord *cur = words.first; cur; cur = cur->next)
	{
		cur->lower_left_corner = AddV2(cur->lower_left_corner, translation);
	}
}


typedef enum
{
	DELEM_NPC,
	DELEM_PLAYER,
	DELEM_ACTION_DESCRIPTION,
} DialogElementKind;

typedef struct
{
	u8 speech[MAX_SENTENCE_LENGTH];
	int speech_length;
	DialogElementKind kind;
	NpcKind who_said_it;
	bool was_last_said;
} DialogElement;

// trail is buffer of vec2s
Vec2 get_point_along_trail(BuffRef trail, float along)
{
	assert(trail.data_elem_size == sizeof(Vec2));
	assert(*trail.cur_index > 1);

	Vec2 *arr = (Vec2 *)trail.data;

	int cur = *trail.cur_index - 1;
	while (cur > 0)
	{
		Vec2 from = arr[cur];
		Vec2 to = arr[cur - 1];
		Vec2 cur_segment = SubV2(to, from);
		float len = LenV2(cur_segment);
		if (len < along)
		{
			along -= len;
		}
		else
		{
			return LerpV2(from, along / len, to);
		}
		cur -= 1;
	}
	return arr[*trail.cur_index - 1];
}
float get_total_trail_len(BuffRef trail)
{
	assert(trail.data_elem_size == sizeof(Vec2));

	if (*trail.cur_index <= 1)
	{
		return 0.0f;
	}
	else
	{
		float to_return = 0.0f;
		Vec2 *arr = (Vec2 *)trail.data;
		for (int i = 0; i < *trail.cur_index - 1; i++)
		{
			to_return += LenV2(SubV2(arr[i], arr[i + 1]));
		}
		return to_return;
	}
}

Vec2 mouse_pos = {0}; // in screen space

#define ROLL_KEY SAPP_KEYCODE_LEFT_SHIFT
double elapsed_time = 0.0;
double unwarped_elapsed_time = 0.0;
double last_frame_processing_time = 0.0;
double last_frame_gameplay_processing_time = 0.0;
uint64_t frame_index = 0; // for rendering tick stuff, gamestate tick is used for game logic tick stuff and is serialized/deserialized/saved
uint64_t last_frame_time;

typedef struct
{
	bool interact;
	bool mouse_down;
	bool mouse_up;
} PressedState;

PressedState pressed = {0};
bool mouse_down = false;
float learned_shift = 0.0;
float learned_space = 0.0;
float learned_e = 0.0;
#ifdef DEVTOOLS
bool mouse_frozen = false;
#endif

typedef struct
{
	AABB button_aabb;
	float text_scale;
	String8 text;
	String8 key;
	float dt;
	bool force_down;
	Layer layer;
	LoadedFont *font;
	sg_image *icon;
	bool icon_flipped;
	bool nobg;
	float icon_padding; // dist between icon png's top and bottom edges and the button's top and bottom edges
} ImbuttonArgs;

bool imbutton_key(ImbuttonArgs args)
{
	Layer layer = LAYER_UI;
	LoadedFont *font = &default_font;
	if (args.layer != LAYER_INVALID)
		layer = args.layer;
	if (args.font)
		font = args.font;
	get_state(
		state, struct { float pressed_amount; bool is_being_pressed; }, "%.*s", S8VArg(args.key));

	float raise = Lerp(0.0f, state->pressed_amount, 5.0f);
	args.button_aabb.upper_left.y += raise;
	args.button_aabb.lower_right.y += raise;

	bool to_return = false;
	float pressed_target = 0.5f;
	if (has_point(args.button_aabb, mouse_pos))
	{
		if (pressed.mouse_down)
		{
			state->is_being_pressed = true;
			pressed.mouse_down = false;
		}

		pressed_target = 1.0f; // when hovering button like pops out a bit

		if (pressed.mouse_up && state->is_being_pressed)
		{
			to_return = true; // when mouse released, and hovering over button, this is a button press - Lao Tzu
		}
	}
	if (pressed.mouse_up)
		state->is_being_pressed = false;

	if (state->is_being_pressed || args.force_down)
		pressed_target = 0.0f;

	state->pressed_amount = Lerp(state->pressed_amount, args.dt * 20.0f, pressed_target);

	float button_alpha = Lerp(0.5f, state->pressed_amount, 1.0f);

	if (aabb_is_valid(args.button_aabb))
	{
		if (!args.nobg)
			draw_quad((DrawParams){
				quad_aabb(args.button_aabb),
				IMG(image_white_square),
				blendalpha(WHITE, button_alpha),
				.layer = layer,
			});

		if (args.icon)
		{
			Vec2 button_size = aabb_size(args.button_aabb);
			float icon_vertical_size = button_size.y - 2.0f * args.icon_padding;
			Vec2 icon_size = V2(icon_vertical_size, icon_vertical_size);
			Vec2 center = aabb_center(args.button_aabb);
			AABB icon_aabb = aabb_centered(center, icon_size);
			/*
			if(args.icon_flipped) {
				swapVec2(&quad.ul, &quad.ur);
				swapVec2(&quad.ll, &quad.lr);
			}
			*/
			AABB region = full_region(*args.icon);
			if (args.icon_flipped)
			{
				swapVec2(&region.upper_left, &region.lower_right);
			}
			draw_quad((DrawParams){
				quad_aabb(icon_aabb),
				*args.icon,
				region,
				blendalpha(WHITE, button_alpha),
				.layer = layer,
			});
		}

		// don't use draw centered text here because it looks funny for some reason... I think it's because the vertical line advance of the font, used in draw_centered_text, is the wrong thing for a button like this
		TextParams t = (TextParams){false, args.text, aabb_center(args.button_aabb), BLACK, args.text_scale, .clip_to = args.button_aabb, .do_clipping = true, .layer = layer, .use_font = font};
		t.dry_run = true;
		AABB aabb = draw_text(t);
		if (aabb_is_valid(aabb))
		{
			t.dry_run = false;
			t.pos = SubV2(aabb_center(args.button_aabb), MulV2F(aabb_size(aabb), 0.5f));
			draw_text(t);
		}
	}

	return to_return;
}

#define imbutton(...) imbutton_key((ImbuttonArgs){__VA_ARGS__, .key = tprint("%d", __LINE__), .dt = unwarped_dt})

Quat rot_on_plane_to_quat(float rot)
{
	return QFromAxisAngle_RH(V3(0, 1, 0), AngleRad(-rot));
}

Transform entity_transform(Entity *e)
{
	// Models must face +X in blender. This is because, in the 2d game coordinate system,
	// a zero degree 2d rotation means you're facing +x, and this is how it is in the game logic.
	// The rotation is negative for some reason that I'm not quite sure about though, something about
	// the handedness of the 3d coordinate system not matching the handedness of the 2d coordinate system
	Quat entity_rot = rot_on_plane_to_quat(e->rotation);

	return (Transform){.offset = AddV3(plane_point(e->pos), V3(0, 0, 0)), .rotation = entity_rot, .scale = V3(1, 1, 1)};
	/*
	(void)entity_rot;
	return (Transform){.offset = AddV3(plane_point(e->pos), V3(0,0,0)), .rotation = Make_Q(0,0,0,1), .scale = V3(1, 1, 1)};
	*/
}

Shadow_State init_shadow_state()
{
	// To start off with, most of this initialisation code is taken from the
	//  sokol shadows sample, which can be found here.
	//  https://floooh.github.io/sokol-html5/shadows-sapp.html

	Shadow_State shadows = {0};

	shadows.pass_action = (sg_pass_action){
		.colors[0] = {
			.load_action = SG_LOADACTION_CLEAR,
			.clear_value = {1.0f, 1.0f, 1.0f, 1.0f}}};

	/*
		 As of right now, it looks like sokol_gfx does not support depth only
		 rendering passes, so we create the colour buffer always. It will likely
		 be pertinent to just dig into sokol and add the functionality we want later,
		 but as a first pass, we will just do as the romans do. I.e. have both a colour
		 and depth component. - Canada Day 2023.
		 TODO: with GLES3, depth only rendering passes are now supported
		 */
	sg_image_desc img_desc = {
		.render_target = true,
		.width = SHADOW_MAP_DIMENSION,
		.height = SHADOW_MAP_DIMENSION,
		.pixel_format = sapp_sgcontext().color_format,
		.sample_count = 1,
		.label = "shadow-map-color-image"};
	shadows.color_img = sg_make_image(&img_desc);
	img_desc.pixel_format = SG_PIXELFORMAT_DEPTH; // @TODO @URGENT replace depth with R8, I think depth isn't always safe on Webgl1 according to sg_gfx header. Also replace other instances of this in codebase
	img_desc.label = "shadow-map-depth-image";
	shadows.depth_img = sg_make_image(&img_desc);
	shadows.pass = sg_make_pass(&(sg_pass_desc){
		.color_attachments[0].image = shadows.color_img,
		.depth_stencil_attachment.image = shadows.depth_img,
		.label = "shadow-map-pass"});

	sg_pipeline_desc desc = (sg_pipeline_desc){
		.layout = {
			.attrs = {
				[ATTR_threedee_vs_pos_in].format = SG_VERTEXFORMAT_FLOAT3,
				[ATTR_threedee_vs_uv_in].format = SG_VERTEXFORMAT_FLOAT2,
			}},
		.shader = sg_make_shader(threedee_mesh_shadow_mapping_shader_desc(sg_query_backend())),
		// Cull front faces in the shadow map pass
		// .cull_mode = SG_CULLMODE_BACK,
		.sample_count = 1,
		.depth = {
			.pixel_format = SG_PIXELFORMAT_DEPTH,
			.compare = SG_COMPAREFUNC_LESS_EQUAL,
			.write_enabled = true,
		},
		.colors[0].pixel_format = sapp_sgcontext().color_format,
		.label = "shadow-map-pipeline"};

	shadows.pip = sg_make_pipeline(&desc);

	desc.label = "armature-shadow-map-pipeline";
	desc.shader = sg_make_shader(threedee_armature_shadow_mapping_shader_desc(sg_query_backend()));
	sg_vertex_attr_state skeleton_vertex_attrs[] = {
		[ATTR_threedee_vs_skeleton_pos_in].format = SG_VERTEXFORMAT_FLOAT3,
		[ATTR_threedee_vs_skeleton_uv_in].format = SG_VERTEXFORMAT_FLOAT2,
		[ATTR_threedee_vs_skeleton_indices_in].format = SG_VERTEXFORMAT_USHORT4N,
		[ATTR_threedee_vs_skeleton_weights_in].format = SG_VERTEXFORMAT_FLOAT4,
	};
	assert(ARRLEN(skeleton_vertex_attrs) < ARRLEN(desc.layout.attrs));
	memcpy(desc.layout.attrs, skeleton_vertex_attrs, sizeof(skeleton_vertex_attrs));
	shadows.armature_pip = sg_make_pipeline(&desc);
	return shadows;
}

float round_to_nearest(float input, float round_target)
{
	float result = 0.0f;
	if (round_target != 0.0f)
	{
		float div = roundf(input / round_target);
		result = div * round_target;
	}
	return result;
}

void debug_draw_img(sg_image img, int index)
{
	draw_quad((DrawParams){quad_at(V2(512.0f * index, 512.0), V2(512.0, 512.0)), IMG(img), WHITE, .layer = LAYER_UI});
}

void debug_draw_img_with_border(sg_image img, int index)
{
	float bs = 50.0;
	draw_quad((DrawParams){quad_at(V2(512.0f * index, 512.0), V2(512.0, 512.0)), img, (AABB){V2(-bs, -bs), AddV2(img_size(img), V2(bs, bs))}, WHITE, .layer = LAYER_UI});
}

void actually_draw_thing(DrawnThing *it, Mat4 light_space_matrix, bool for_outline)
{
	int num_vertices_to_draw = 0;
	if (it->mesh)
	{
		if (for_outline)
			sg_apply_pipeline(state.outline_mesh_pip);
		else if (it->alpha_blend)
			sg_apply_pipeline(state.threedee_alpha_blended_pip);
		else
			sg_apply_pipeline(state.threedee_pip);
		sg_bindings bindings = {0};
		bindings.fs.images[SLOT_threedee_tex] = it->mesh->image;
		if (for_outline)
		{
			bindings.fs.samplers[SLOT_threedee_fs_outline_smp] = state.sampler_linear;
		}
		else
		{
			bindings.fs.images[SLOT_threedee_shadow_map] = state.shadows.color_img;
			bindings.fs.samplers[SLOT_threedee_fs_shadow_smp] = state.sampler_linear_border;
			bindings.fs.samplers[SLOT_threedee_fs_smp] = state.sampler_linear;
		}
		bindings.vertex_buffers[0] = it->mesh->loaded_buffer;
		sg_apply_bindings(&bindings);

		Mat4 model = transform_to_matrix(it->t);
		threedee_vs_params_t vs_params = {
			.model = model,
			.view = view,
			.projection = projection,
			.directional_light_space_matrix = light_space_matrix,
			.time = (float)elapsed_time,
			.seed = it->seed,
			.wobble_factor = it->wobble_factor,
			.wobble_world_source = it->wobble_world_source,
		};
		sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_threedee_vs_params, &SG_RANGE(vs_params));

		num_vertices_to_draw = (int)it->mesh->num_vertices;
	}
	else if (it->armature)
	{
		if (for_outline)
			sg_apply_pipeline(state.outline_armature_pip);
		else
			sg_apply_pipeline(state.armature_pip);
		sg_bindings bindings = {0};
		bindings.vs.images[SLOT_threedee_bones_tex] = it->armature->bones_texture;
		bindings.vs.samplers[SLOT_threedee_vs_skeleton_smp] = state.sampler_nearest;
		bindings.fs.images[SLOT_threedee_tex] = it->armature->image;
		if (for_outline)
		{
			bindings.fs.samplers[SLOT_threedee_fs_outline_smp] = state.sampler_linear;
		}
		else
		{
			bindings.fs.images[SLOT_threedee_shadow_map] = state.shadows.color_img;
			bindings.fs.samplers[SLOT_threedee_fs_shadow_smp] = state.sampler_linear_border;
			bindings.fs.samplers[SLOT_threedee_fs_smp] = state.sampler_linear;
		}
		bindings.vertex_buffers[0] = it->armature->loaded_buffer;
		sg_apply_bindings(&bindings);

		Mat4 model = transform_to_matrix(it->t);
		threedee_skeleton_vs_params_t params = {
			.model = model,
			.view = view,
			.projection = projection,
			.directional_light_space_matrix = light_space_matrix,
			.bones_tex_size = V2((float)it->armature->bones_texture_width, (float)it->armature->bones_texture_height),
		};
		sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_threedee_skeleton_vs_params, &SG_RANGE(params));
		num_vertices_to_draw = (int)it->armature->vertices_length;
	}
	else
		assert(false);

	if (!for_outline)
	{
		threedee_fs_params_t fs_params = {0};
		fs_params.shadow_map_dimension = SHADOW_MAP_DIMENSION;
		if (it->no_dust)
		{
			fs_params.how_much_not_to_blend_ground_color = 0.0;
		}
		fs_params.alpha_blend_int = it->alpha_blend;
		sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_threedee_fs_params, &SG_RANGE(fs_params));
	}

	num_draw_calls += 1;
	assert(num_vertices_to_draw > 0);
	num_vertices += num_vertices_to_draw;
	sg_draw(0, num_vertices_to_draw, 1);
}

typedef struct
{
	Mat4 view;
	Mat4 projection;
} ShadowMats;

// I moved this out into its own separate function so that you could
// define helper functions to be used multiple times in it, and those functions
// would be near the actual 3d drawing in the file
// @Place(the actual 3d rendering)
void flush_all_drawn_things(ShadowMats shadow)
{
	// Draw all the 3D drawn things. Draw the shadows, then draw the things with the shadows.
	// Process armatures and upload their skeleton textures
	{

		// Animate armatures, and upload their bone textures. Also debug draw their skeleton
		{
			SLICE_ITER(DrawnThing, drawn_this_frame)
			{
				if (it->armature)
				{
					ArenaTemp scratch = GetScratch(0, 0);
					Armature *armature = it->armature;
					int bones_tex_size = 4 * armature->bones_texture_width * armature->bones_texture_height;
					u8 *bones_tex = ArenaPush(scratch.arena, bones_tex_size);

					for (u64 i = 0; i < armature->bones_length; i++)
					{
						Bone *cur = &armature->bones[i];

						// for debug drawing
						Vec3 from = MulM4V3(cur->matrix_local, V3(0, 0, 0));
						Vec3 x = MulM4V3(cur->matrix_local, V3(cur->length, 0, 0));
						Vec3 y = MulM4V3(cur->matrix_local, V3(0, cur->length, 0));
						Vec3 z = MulM4V3(cur->matrix_local, V3(0, 0, cur->length));

						Mat4 final = M4D(1.0f);
						final = MulM4(cur->inverse_model_space_pos, final);
						for (Bone *cur_in_hierarchy = cur; cur_in_hierarchy; cur_in_hierarchy = cur_in_hierarchy->parent)
						{
							int bone_index = (int)(cur_in_hierarchy - armature->bones);
							final = MulM4(transform_to_matrix(armature->anim_blended_poses[bone_index]), final);
						}

						from = MulM4V3(final, from);
						x = MulM4V3(final, x);
						y = MulM4V3(final, y);
						z = MulM4V3(final, z);

						Mat4 transform_matrix = transform_to_matrix(it->t);
						from = MulM4V3(transform_matrix, from);
						x = MulM4V3(transform_matrix, x);
						y = MulM4V3(transform_matrix, y);
						z = MulM4V3(transform_matrix, z);
						dbgcol(LIGHTBLUE)
							dbgsquare3d(y);
						dbgcol(RED)
							dbg3dline(from, x);
						dbgcol(GREEN)
							dbg3dline(from, y);
						dbgcol(BLUE)
							dbg3dline(from, z);

						for (int col = 0; col < 4; col++)
						{
							Vec4 to_upload = final.Columns[col];

							int bytes_per_pixel = 4;
							int bytes_per_column_of_mat = bytes_per_pixel * 4;
							int bytes_per_row = bytes_per_pixel * armature->bones_texture_width;
							for (int elem = 0; elem < 4; elem++)
							{
								float after_decoding = decode_normalized_float32(encode_normalized_float32(to_upload.Elements[elem]));
								assert(fabsf(after_decoding - to_upload.Elements[elem]) < 0.01f);
							}
							memcpy(&bones_tex[bytes_per_column_of_mat * col + bytes_per_row * i + bytes_per_pixel * 0], encode_normalized_float32(to_upload.Elements[0]).rgba, bytes_per_pixel);
							memcpy(&bones_tex[bytes_per_column_of_mat * col + bytes_per_row * i + bytes_per_pixel * 1], encode_normalized_float32(to_upload.Elements[1]).rgba, bytes_per_pixel);
							memcpy(&bones_tex[bytes_per_column_of_mat * col + bytes_per_row * i + bytes_per_pixel * 2], encode_normalized_float32(to_upload.Elements[2]).rgba, bytes_per_pixel);
							memcpy(&bones_tex[bytes_per_column_of_mat * col + bytes_per_row * i + bytes_per_pixel * 3], encode_normalized_float32(to_upload.Elements[3]).rgba, bytes_per_pixel);
						}
					}

					// sokol prohibits updating an image more than once per frame
					if (armature->last_updated_bones_frame != frame_index)
					{
						armature->last_updated_bones_frame = frame_index;
						sg_update_image(armature->bones_texture, &(sg_image_data){
																	 .subimage[0][0] = (sg_range){bones_tex, bones_tex_size},
																 });
					}

					ReleaseScratch(scratch);
				}
			}
		}

		// do the shadow pass
		Mat4 light_space_matrix;
		{
			light_space_matrix = MulM4(shadow.projection, shadow.view);
			// debug_draw_shadow_info(cam_pos, cam_facing, cam_right, light_space_matrix);

			sg_begin_pass(state.shadows.pass, &state.shadows.pass_action);

			// shadows for meshes
			sg_apply_pipeline(state.shadows.pip);
			SLICE_ITER(DrawnThing, drawn_this_frame)
			{
				assert(it->mesh || it->armature);
				if (it->dont_cast_shadows)
					continue;
				if (it->mesh)
				{
					sg_bindings bindings = {0};
					bindings.fs.images[SLOT_threedee_tex] = it->mesh->image;
					bindings.fs.samplers[SLOT_threedee_fs_shadow_mapping_smp] = state.sampler_linear;
					bindings.vertex_buffers[0] = it->mesh->loaded_buffer;
					sg_apply_bindings(&bindings);

					Mat4 model = transform_to_matrix(it->t);
					threedee_vs_params_t vs_params = {
						.model = model,
						.view = shadow.view,
						.projection = shadow.projection,
						.time = (float)elapsed_time,
					};
					sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_threedee_vs_params, &SG_RANGE(vs_params));
					num_draw_calls += 1;
					num_vertices += (int)it->mesh->num_vertices;
					sg_draw(0, (int)it->mesh->num_vertices, 1);
				}
			}

			// shadows for armatures
			sg_apply_pipeline(state.shadows.armature_pip);
			SLICE_ITER(DrawnThing, drawn_this_frame)
			{
				if (it->armature)
				{
					sg_bindings bindings = {0};
					bindings.vs.images[SLOT_threedee_bones_tex] = it->armature->bones_texture;
					bindings.vs.samplers[SLOT_threedee_vs_skeleton_smp] = state.sampler_nearest;
					bindings.fs.images[SLOT_threedee_tex] = it->armature->image;
					bindings.fs.samplers[SLOT_threedee_fs_shadow_mapping_smp] = state.sampler_linear;
					bindings.vertex_buffers[0] = it->armature->loaded_buffer;
					sg_apply_bindings(&bindings);

					Mat4 model = transform_to_matrix(it->t);
					threedee_skeleton_vs_params_t params = {
						.model = model,
						.view = shadow.view,
						.projection = shadow.projection,
						.directional_light_space_matrix = light_space_matrix,
						.bones_tex_size = V2((float)it->armature->bones_texture_width, (float)it->armature->bones_texture_height),
					};
					sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_threedee_skeleton_vs_params, &SG_RANGE(params));

					num_draw_calls += 1;
					num_vertices += (int)it->armature->vertices_length;
					sg_draw(0, (int)it->armature->vertices_length, 1);
				}
			}
			sg_end_pass();
		}

		// do the outline pass
		{
			sg_begin_pass(state.outline_pass, &(sg_pass_action){
												  .colors[0] = {
													  .load_action = SG_LOADACTION_CLEAR,
													  .clear_value = {0.0f, 0.0f, 0.0f, 0.0f},
												  }});

			SLICE_ITER(DrawnThing, drawn_this_frame)
			{
				if (it->outline)
				{
					actually_draw_thing(it, light_space_matrix, true);
				}
			}

			sg_end_pass();
		}

		// actually draw, IMPORTANT after this drawn_this_frame is zeroed out!
		{
			sg_begin_pass(state.threedee_pass, &state.threedee_msaa_pass_action);

			// draw meshes
			SLICE_ITER(DrawnThing, drawn_this_frame)
			{
				if (it->alpha_blend)
					continue;
				if (it->mesh)
					actually_draw_thing(it, light_space_matrix, false);
			}

			// draw armatures armature rendering
			SLICE_ITER(DrawnThing, drawn_this_frame)
			{
				if (it->armature)
				{
					assert(!it->alpha_blend); // too lazy to implement this right now
					actually_draw_thing(it, light_space_matrix, false);
				}
			}

			// draw transparent
			SLICE_ITER(DrawnThing, drawn_this_frame)
			{
				if (it->alpha_blend)
					actually_draw_thing(it, light_space_matrix, false);
			}

			// zero out everything
			SLICE_ITER(DrawnThing, drawn_this_frame)
			{
				*it = (DrawnThing){0};
			}
			sg_end_pass();
		}
		drawn_this_frame_length = 0;
	}
}

void animate_armature(Armature *it, float dt) {
	// progress the animation, then blend the two animations if necessary, and finally
	// output into anim_blended_poses
	Armature *cur = it;
	float seed = (float)((int64_t)cur % 1024); // offset into elapsed time to make all of their animations out of phase
	float along_current_animation = 0.0;
	if (cur->currently_playing_isnt_looping)
	{
		along_current_animation = (float)cur->cur_animation_time;
		cur->cur_animation_time += dt;
	}
	else
	{
		along_current_animation = (float)elapsed_time + seed;
	}

	if (cur->go_to_animation.size > 0)
	{
		if (S8Match(cur->go_to_animation, cur->currently_playing_animation, 0))
		{
		}
		else
		{
			memcpy(cur->current_poses, cur->anim_blended_poses, cur->bones_length * sizeof(*cur->current_poses));
			cur->currently_playing_animation = cur->go_to_animation;
			cur->animation_blend_t = 0.0f;
			cur->go_to_animation = (String8){0};
			if (cur->next_animation_isnt_looping)
			{
				cur->cur_animation_time = 0.0;
				cur->currently_playing_isnt_looping = true;
			}
			else
			{
				cur->currently_playing_isnt_looping = false;
			}
		}
		cur->next_animation_isnt_looping = false;
	}

	if (cur->animation_blend_t < 1.0f)
	{
		cur->animation_blend_t += dt / ANIMATION_BLEND_TIME;

		Animation *to_anim = get_anim_by_name(cur, cur->currently_playing_animation);
		assert(to_anim);

		for (u64 i = 0; i < cur->bones_length; i++)
		{
			Transform *output_transform = &cur->anim_blended_poses[i];
			Transform from_transform = cur->current_poses[i];
			Transform to_transform = get_animated_bone_transform(&to_anim->tracks[i], along_current_animation, cur->currently_playing_isnt_looping);

			*output_transform = lerp_transforms(from_transform, cur->animation_blend_t, to_transform);
		}
	}
	else
	{
		Animation *cur_anim = get_anim_by_name(cur, cur->currently_playing_animation);
		for (u64 i = 0; i < cur->bones_length; i++)
		{
			cur->anim_blended_poses[i] = get_animated_bone_transform(&cur_anim->tracks[i], along_current_animation, cur->currently_playing_isnt_looping);
		}
	}
}

typedef struct
{
	float text_scale;
	float width_in_pixels;
	float text_width_in_pixels;
	LoadedFont *font;
	int lines_per_page;
	int maximum_pages_from_ai;
} TextPlacementSettings;

TextPlacementSettings speech_bubble = {
	.text_scale = 1.0f,
	.width_in_pixels = 400.0f,
	.text_width_in_pixels = 400.0f * 0.8f,
	.font = &default_font,
	.lines_per_page = 2,
	.maximum_pages_from_ai = 2,
};

// Unsaid words are still there, so you gotta handle the animation homie
String8List words_on_current_page(Entity *it, TextPlacementSettings *settings)
{
	String8 last = S8Lit(""); // TODO
	PlacedWordList placed = place_wrapped_words(frame_arena, split_by_word(frame_arena, last), settings->text_scale, settings->text_width_in_pixels, *settings->font, JUST_LEFT);

	String8List on_current_page = {0};
	for (PlacedWord *cur = placed.first; cur; cur = cur->next)
	{
		if (cur->line_index / settings->lines_per_page == it->cur_page_index)
			S8ListPush(frame_arena, &on_current_page, cur->text);
	}

	return on_current_page;

	// return place_wrapped_words(frame_arena, on_current_page, text_scale, aabb_size(placing_text_in).x, default_font);
}

String8List words_on_current_page_without_unsaid(Entity *it, TextPlacementSettings *settings)
{
	String8List all_words = words_on_current_page(it, settings);
	int index = 0;
	String8List to_return = {0};
	for (String8Node *cur = all_words.first; cur; cur = cur->next)
	{
		if (index > it->words_said_on_page)
			break;
		S8ListPush(frame_arena, &to_return, cur->string);
		index += 1;
	}
	return to_return;
}

// the returned action is checked to ensure that it's allowed according to the current gamestate
// the error out is fed into the AI, so make it make sense.
Action bake_into_action(Arena *error_arena, String8 *error_out, GameState *gs, Entity *taking_the_action, Response *resp) {
	#define error(...) *error_out = FmtWithLint(error_arena, __VA_ARGS__)
	*error_out = S8Lit("");

	// TODO restrict the actions you're allowed to do here
	SituationAction *cur_action = 0;
	ARR_ITER(SituationAction, gamecode_actions) {
		if(S8Match(TCS8(it->name), TCS8(resp->action), 0)) {
			cur_action = it;
			break;
		}
	}
	if(!cur_action) {
		error("You output action '%.*s', don't know what that is\n", TCVArg(resp->action));
	}


	Action ret = {0};
	ret.kind = cur_action->gameplay_action;

	BUFF_ITER_I(ArgumentSpecification, &cur_action->args, arg_index) {
		TextChunk argtext = resp->arguments.data[arg_index];
		ActualActionArgument out = {0};
		switch(it->expected_type) {
			case ARGTYPE_invalid:
			error("At index %d expected no argument, but found one.", arg_index);
			break;

			case ARGTYPE_text:
			out.text = argtext;
			break;

			case ARGTYPE_character:
			{
				bool found = false;
				ENTITIES_ITER(gs->entities) {
					if(S8Match(npc_name(it), TCS8(argtext), 0)) {
						found = true;
						out.character = it;
					}
				}
				if(!found) {
					error("The character you're referring to at index %d, `%.*s`, doesn't exist in the game", arg_index, TextChunkVArg(argtext));
				}
			}
			break;
		}
		if(error_out->size == 0) {
			ret.args.data[arg_index] = out;
		}
	}
	if(error_out->size == 0)
		ret.args.cur_index = cur_action->args.cur_index;

	// validate that the action is allowed in the gameplay, and everything there looks good
	switch(ret.kind) {
		case ACT_invalid:
		assert(false);
		break;
		case ACT_none:
		break;
		case ACT_say_to:
		{
			// has to be in same room
			assert(act_by_enum(ACT_say_to)->args.data[0].expected_type == ARGTYPE_character);
			if(ret.args.data[0].character->current_roomid != taking_the_action->current_roomid) {
				error("You can't speak to character '%.*s', they're not in the same room as you", S8VArg(npc_name(ret.args.data[0].character)));
			}

			// arbitrary max word count to make it more succinct
			assert(act_by_enum(ACT_say_to)->args.data[1].expected_type == ARGTYPE_text);
			String8 speech = TextChunkString8(ret.args.data[1].text);
			int words_over_limit = (int)split_by_word(frame_arena, speech).node_count - MAX_WORD_COUNT;

			if (words_over_limit > 0)
			{
				error("Your speech '%.*s...' %d words over the maximum limit %d, you must be more succinct and remove at least that many words", S8VArg(S8Chop(speech, 100)), words_over_limit, MAX_WORD_COUNT);
			}
		}
		break;
	}

	#undef error
	return ret;
}

Vec3 point_on_plane_from_camera_point(Mat4 view, Vec2 screenspace_camera_point)
{
	Vec3 view_cam_pos = MulM4V4(InvGeneralM4(view), V4(0, 0, 0, 1)).xyz;
	Vec3 world_point = screenspace_point_to_camera_point(screenspace_camera_point, view);
	Vec3 point_ray = NormV3(SubV3(world_point, view_cam_pos));
	Vec3 marker = ray_intersect_plane(view_cam_pos, point_ray, V3(0, 0, 0), V3(0, 1, 0));
	return marker;
}

int mod(int a, int b)
{
	int r = a % b;
	return r < 0 ? r + b : r;
}

CharacterSituation *generate_situation(Arena *arena, GameState *gs, Entity *e) {
	(void) gs;
	(void) e;
	CharacterSituation *ret = PushArrayZero(arena, CharacterSituation, 1);
	ret->room_description = TextChunkLit("A lush forest, steeped in shade. Some mysterious gears are scattered across the floor");

	BUFF_ITER(Memory, &e->memories) {
		BUFF_APPEND(&ret->events, it->description_from_my_perspective);
	}
	ENTITIES_ITER(gs->entities){
		if(it->is_npc && it != e) {
			BUFF_APPEND(&ret->targets, ((SituationTarget) {
					.name = TextChunkLit("The Player"),
					.description = TextChunkLit("The Player. They just spawned in out of nowhere, and are causing a bit of a ruckus."),
					.kind = TARGET_person,
				})
			);
		}
	}
	ARR_ITER(SituationAction, gamecode_actions) BUFF_APPEND(&ret->actions, *it);
	BUFF_ITER(TextChunk, &e->error_notices) BUFF_APPEND(&ret->errors, *it);
	return ret;
}

void frame(void)
{
	static float speed_factor = 1.0f;
	// elapsed_time
	double unwarped_dt_double = 0.0;
	{
		unwarped_dt_double = stm_sec(stm_diff(stm_now(), last_frame_time));
		unwarped_dt_double = fmin(unwarped_dt_double, MINIMUM_TIMESTEP * 5.0); // clamp dt at maximum 5 frames, avoid super huge dt
		elapsed_time += unwarped_dt_double * speed_factor;
		unwarped_elapsed_time += unwarped_dt_double;
		last_frame_time = stm_now();
	}
	double dt_double = unwarped_dt_double * speed_factor;
	float unwarped_dt = (float)unwarped_dt_double;
	float dt = (float)dt_double;
	frame_index += 1;

#if 0
	{
		printf("Frametime: %.1f ms\n", dt*1000.0);
		sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
		sg_apply_pipeline(state.twodee_pipeline);

		//colorquad(false, quad_at(V2(0.0, 100.0), V2(100.0f, 100.0f)), RED);
		sg_image img = image_white_square;
		AABB region = full_region(img);
		//region.lower_right.X *= 0.5f;
		draw_quad((DrawParams) { false, quad_at(V2(0.0, 100.0), V2(100.0f, 100.0f)), img, region, WHITE });


		flush_quad_batch();
		sg_end_pass();
		sg_commit();
	}
	return;
#endif

	PROFILE_SCOPE("frame")
	{
		uint64_t time_start_frame = stm_now();

		gs.time += dt_double;

		text_input_fade = Lerp(text_input_fade, unwarped_dt * 8.0f, receiving_text_input ? 1.0f : 0.0f);

		Vec3 cam_target_pos = V3(0, 0, 0);
		if (gs.edit.enabled)
		{
			cam_target_pos.x = gs.edit.camera_panning.x;
			cam_target_pos.z = gs.edit.camera_panning.y;
		}
		else if(gs.unprocessed_first)
		{
			if(gs.unprocessed_first->action.kind == ACT_say_to) {
				cam_target_pos.x = gete(gs.unprocessed_first->author)->pos.x;
				cam_target_pos.z = gete(gs.unprocessed_first->author)->pos.y;
			}
		}
		else if (player(&gs))
		{
			cam_target_pos.x = player(&gs)->pos.x;
			cam_target_pos.z = player(&gs)->pos.y;
		}
		gs.cur_cam_pos = LerpV3(gs.cur_cam_pos, 9.0f * unwarped_dt, cam_target_pos);
		// dbgline(V2(0,0), V2(500, 500));
		const float vertical_to_horizontal_ratio = CAM_VERTICAL_TO_HORIZONTAL_RATIO;
		const float cam_distance = CAM_DISTANCE;
		Vec3 cam_offset_from_target;
		{
			float ratio = vertical_to_horizontal_ratio;
			float x = sqrtf((cam_distance * cam_distance) / (1 + (ratio * ratio)));
			float y = ratio * x;
			cam_offset_from_target = V3(x, y, 0.0);
		}
		cam_offset_from_target = MulM4V4(Rotate_RH(-PI32 / 3.0f + PI32, V3(0, 1, 0)), IsPoint(cam_offset_from_target)).xyz;
		if (get_cur_room(&gs, &level_threedee)->camera_offset_is_overridden)
		{
			cam_offset_from_target = get_cur_room(&gs, &level_threedee)->camera_offset;
		}
		Vec3 cam_pos = AddV3(gs.cur_cam_pos, cam_offset_from_target);

		Vec2 movement = {0};
		if (mobile_controls)
		{
			movement = SubV2(thumbstick_nub_pos, thumbstick_base_pos);
			if (LenV2(movement) > 0.0f)
			{
				movement = MulV2F(NormV2(movement), LenV2(movement) / (thumbstick_base_size() * 0.5f));
			}
		}
		else
		{
			movement = V2(
				(float)keydown[SAPP_KEYCODE_D] - (float)keydown[SAPP_KEYCODE_A],
				(float)keydown[SAPP_KEYCODE_W] - (float)keydown[SAPP_KEYCODE_S]);
		}
		if (LenV2(movement) > 1.0)
		{
			movement = NormV2(movement);
		}

		Vec3 light_dir;
		{
			float t = 0.3f;
			Vec3 sun_vector = V3(2.0f * t - 1.0f, sinf(t * PI32) * 0.8f + 0.2f, 0.8f); // where the sun is pointing from
			light_dir = NormV3(MulV3F(sun_vector, -1.0f));
		}

		// make movement relative to camera forward
		Vec3 facing = NormV3(SubV3(gs.cur_cam_pos, cam_pos));
		Vec3 right = Cross(facing, V3(0, 1, 0));
		Vec2 forward_2d = NormV2(V2(facing.x, facing.z));
		Vec2 right_2d = NormV2(V2(right.x, right.z));
		movement = AddV2(MulV2F(forward_2d, movement.y), MulV2F(right_2d, movement.x));

		if (flycam)
			movement = V2(0, 0);

		view = Translate(V3(0.0, 1.0, -5.0f));
		Mat4 normal_cam_view = LookAt_RH(cam_pos, gs.cur_cam_pos, V3(0, 1, 0));
		if (flycam)
		{
			Basis basis = flycam_basis();
			view = LookAt_RH(flycam_pos, AddV3(flycam_pos, basis.forward), V3(0, 1, 0));
			// view = flycam_matrix();
		}
		else
		{
			view = normal_cam_view;
		}

		projection = Perspective_RH_NO(FIELD_OF_VIEW, screen_size().x / screen_size().y, NEAR_PLANE_DISTANCE, FAR_PLANE_DISTANCE);

		// calculate light stuff
		Vec3 camera_bounds[] = {
			point_on_plane_from_camera_point(normal_cam_view, V2(0.0, 0.0)),
			point_on_plane_from_camera_point(normal_cam_view, V2(screen_size().x, 0.0)),
			point_on_plane_from_camera_point(normal_cam_view, V2(screen_size().x, screen_size().y)),
			point_on_plane_from_camera_point(normal_cam_view, V2(0.0, screen_size().y)),
		};
		for (int i = 0; i < ARRLEN(camera_bounds); i++)
			dbgcol(PINK)
				dbg3dline(camera_bounds[i], camera_bounds[(i + 1) % ARRLEN(camera_bounds)]);

		Vec3 center = V3(0, 0, 0);
		ARR_ITER(Vec3, camera_bounds)
		center = AddV3(center, *it);
		center = MulV3F(center, 1.0f / (float)ARRLEN(camera_bounds));
		Vec3 shadows_focused_on = center;
		float max_radius = 0.0f;
		ARR_ITER(Vec3, camera_bounds)
		{
			float l = LenV3(SubV3(*it, shadows_focused_on));
			if (l > max_radius)
			{
				max_radius = l;
			}
		}
		ShadowMats shadow = {
			.view = LookAt_RH(shadows_focused_on, AddV3(shadows_focused_on, light_dir), V3(0, 1, 0)),
			.projection = Orthographic_RH_NO(-max_radius, max_radius, -max_radius, max_radius, -100.0f, 400.0f),
			//.projection = Orthographic_RH_NO(svp.l, svp.r, svp.b, svp.t, svp.n, svp.f),
		};

		// @Place(draw 3d things)

		for (PlacedMesh *cur = get_cur_room(&gs, &level_threedee)->placed_mesh_list; cur; cur = cur->next)
		{
			float seed = (float)((int64_t)cur % 1024);

			DrawnThing call = (DrawnThing){.mesh = cur->draw_with, .t = cur->t};
			if (S8Match(cur->name, S8Lit("Ground"), 0))
				call.no_dust = true;

			call.no_dust = true;
			float helicopter_offset = (float)sin(elapsed_time * 0.5f) * 0.5f;
			if (S8Match(cur->name, S8Lit("HelicopterBlade"), 0))
			{
				call.t.offset.y += helicopter_offset;
				call.t.rotation = QFromAxisAngle_RH(V3(0, 1, 0), (float)elapsed_time * 15.0f);
			}
			if (S8Match(cur->name, S8Lit("BlurryBlade"), 0))
			{
				call.t.rotation = QFromAxisAngle_RH(V3(0, 1, 0), (float)elapsed_time * 15.0f);
				call.t.offset.y += helicopter_offset;
				call.alpha_blend = true;
				call.dont_cast_shadows = true;
			}
			if (S8Match(cur->name, S8Lit("HelicopterBody"), 0))
			{
				call.t.offset.y += helicopter_offset;
			}
			if (S8FindSubstring(cur->name, S8Lit("Bush"), 0, 0) == 0)
			{
				call.wobble_factor = 1.0f;
				call.seed = seed;
			}
			draw_thing(call);
		}

		// @Place(Draw entities)
		ENTITIES_ITER(gs.entities)
		{
			if (it->is_npc && it->current_roomid == get_cur_room(&gs, &level_threedee)->roomid)
			{
				assert(it->is_npc);
				Transform draw_with = entity_transform(it);
				if(!it->armature) {
					it->armature = duplicate_armature(gs.arena, &player_armature);
				}

				{
					Armature *to_use = it->armature;
					animate_armature(to_use, dt);

					// if9!

					if (it->killed)
					{
						to_use->go_to_animation = S8Lit("Die Backwards");
						to_use->next_animation_isnt_looping = true;
					}
					else if (LenV2(it->vel) > 0.5f)
					{
						to_use->go_to_animation = S8Lit("Running");
					}
					else
					{
						to_use->go_to_animation = S8Lit("Idle");
					}

					draw_thing((DrawnThing){.armature = to_use, .t = draw_with, .outline = player(&gs) && gete(player(&gs)->interacting_with) == it});

					if (gete(it->aiming_shotgun_at))
					{
						Transform shotgun_t = draw_with;
						shotgun_t.offset.y += 0.7f;
						shotgun_t.scale = V3(4, 4, 4);
						shotgun_t.rotation = rot_on_plane_to_quat(AngleOfV2(SubV2(gete(it->aiming_shotgun_at)->pos, it->pos)));
						draw_thing((DrawnThing){.mesh = &mesh_shotgun, .t = shotgun_t});
					}
				}
			}
		}

		flush_all_drawn_things(shadow);

		// draw the 3d render
		draw_quad((DrawParams){quad_at(V2(0.0, screen_size().y), screen_size()), IMG(state.threedee_pass_resolve_image), WHITE, .layer = LAYER_WORLD, .custom_pipeline = state.twodee_colorcorrect_pip});
		draw_quad((DrawParams){quad_at(V2(0.0, screen_size().y), screen_size()), IMG(state.outline_pass_resolve_image), WHITE, .custom_pipeline = state.twodee_outline_pip, .layer = LAYER_UI});

		// 2d drawing TODO move this to when the drawing is flushed.
		sg_begin_default_pass(&state.clear_depth_buffer_pass_action, sapp_width(), sapp_height());
		sg_apply_pipeline(state.twodee_pip);

		// @Place(high priority UI rendering)

		// @Place(text input drawing)
#ifdef DESKTOP
		draw_quad((DrawParams){quad_at(V2(0, screen_size().y), screen_size()), IMG(image_white_square), blendalpha(BLACK, text_input_fade * 0.3f), .layer = LAYER_UI_TEXTINPUT});
		Vec2 edge_of_text = MulV2F(screen_size(), 0.5f);
		float text_scale = 1.0f;
		if (text_input.text_length > 0)
		{
			AABB bounds = draw_centered_text((TextParams){false, TextChunkString8(text_input), MulV2F(screen_size(), 0.5f), blendalpha(WHITE, text_input_fade), text_scale, .use_font = &font_for_text_input, .layer = LAYER_UI_TEXTINPUT});
			edge_of_text = bounds.lower_right;
		}
		Vec2 cursor_center = V2(edge_of_text.x, screen_size().y / 2.0f);
		draw_quad((DrawParams){quad_centered(cursor_center, V2(3.0f, get_vertical_dist_between_lines(font_for_text_input, text_scale))), IMG(image_white_square), blendalpha(WHITE, text_input_fade * (sinf((float)elapsed_time * 8.0f) / 2.0f + 0.5f)), .layer = LAYER_UI_TEXTINPUT});
#endif

		Entity *cur_unread_entity = 0;
		uint64_t earliest_unread_time = gs.tick;
		ENTITIES_ITER(gs.entities)
		{
			if (it->is_npc && it->undismissed_action && it->undismissed_action_tick < earliest_unread_time)
			{
				earliest_unread_time = it->undismissed_action_tick;
				cur_unread_entity = it;
			}
		}

		// @Place(UI rendering that happens before gameplay processing so can consume events before the gameplay needs them)
		PROFILE_SCOPE("Random ui rendering")
		{
			float size = 100.0f;
			float margin = size;
			bool kill_self = imbutton(aabb_centered(V2(margin, margin), V2(size, size)), .icon = &image_retry, .icon_padding = size * 0.1f, .nobg = true);
			if(kill_self && player(&gs)) {
				player(&gs)->destroy = true;
			}
		}
		PROFILE_SCOPE("Editor Rendering")
		{
			if (keypressed[SAPP_KEYCODE_TAB])
			{
				gs.edit.enabled = !gs.edit.enabled;
			}
			if (gs.edit.enabled)
			{
				draw_text((TextParams){false, S8Lit("Editing"), V2(0, 0), WHITE, .scale = 1.0f, .use_font = &font_for_text_input});
				if (get_cur_room(&gs, &level_threedee)->roomid == gs.edit.player_spawn_roomid)
				{
					draw_thing((DrawnThing){&mesh_spawnring, .t = (Transform){.offset = plane_point(gs.edit.player_spawn_position), .rotation = Make_Q(0, 0, 0, 1), .scale = V3(1, 1, 1)}});
				}

				LoadedFont room_name_font = font_for_text_input;
				float max_room_name_width = 0.0f;
				float max_height = 0.0f;
				for (Room *cur = level_threedee.room_list_first; cur; cur = cur->next)
				{
					Vec2 bounds = aabb_size(draw_text((TextParams){true, cur->name, .use_font = &room_name_font, .scale = 1.0f}));
					max_room_name_width = max(max_room_name_width, bounds.x);
					max_height = max(max_height, bounds.y);
				}
				float padding = 10.0;
				float whole_height = max_height + padding;
				Vec2 left_right_buttons_size = V2(whole_height, whole_height);
				float whole_width = left_right_buttons_size.x * 2.0f + padding * 2.0f + max_room_name_width;

				Room *cur_room = get_cur_room(&gs, &level_threedee);
				bool left = imbutton(aabb_at(V2(screen_size().x / 2.0f - whole_width / 2.0f, screen_size().y - padding / 2.0f), left_right_buttons_size), .icon = &image_right_arrow, .icon_padding = whole_height * 0.1f, .nobg = true, .icon_flipped = true);
				bool right = imbutton(aabb_at(V2(screen_size().x / 2.0f + whole_width / 2.0f - left_right_buttons_size.x, screen_size().y - padding / 2.0f), left_right_buttons_size), .icon = &image_right_arrow, .icon_padding = whole_height * 0.1f, .nobg = true);
				// bool left = imbutton(aabb_at(mouse_pos, left_right_buttons_size), .icon = &image_left_arrow, .icon_padding = whole_height*0.1f, .nobg = true);
				draw_centered_text((TextParams){false, cur_room->name, V2(screen_size().x / 2.0f, screen_size().y - padding), WHITE, 1.0f, .use_font = &room_name_font});
				if (left)
					cur_room = cur_room->prev ? cur_room->prev : level_threedee.room_list_last;
				if (right)
					cur_room = cur_room->next ? cur_room->next : level_threedee.room_list_first;
				gs.edit.current_roomid = cur_room->roomid;

				Vec3 mouse_movement_on_plane = {0};
				{
					Vec2 to_pos = mouse_pos;
					Vec2 from_pos = AddV2(mouse_pos, mouse_movement);
					// dbgline(from_pos, to_pos);
					Vec3 to_plane = point_on_plane_from_camera_point(view, to_pos);
					Vec3 from_plane = point_on_plane_from_camera_point(view, from_pos);
					mouse_movement_on_plane = SubV3(to_plane, from_plane);
					// dbg3dline(from_plane, to_plane);
				}

				if (mouse_down)
				{
					gs.edit.camera_panning_target = AddV2(gs.edit.camera_panning_target, point_plane(mouse_movement_on_plane));
				}

				if (imbutton(grow_from_ml(stats(screen_aabb()).ml, V2(10.0f, 0.0f), V2(200.0f, 60.0f)), 1.0f, S8Lit("Set Player Spawn")))
				{
					gs.edit.placing_spawn = true;
				}

				if (gs.edit.placing_spawn)
				{
					gs.edit.player_spawn_position = point_plane(point_on_plane_from_camera_point(view, mouse_pos));
					gs.edit.player_spawn_roomid = get_cur_room(&gs, &level_threedee)->roomid;
					if (pressed.mouse_down)
					{
						gs.edit.placing_spawn = false;
					}
				}

				// characters sidebar
				{
					float screen_margin = 10.0f;
					float width = 400.0f;

					float character_panel_height = 150.0f;
					float total_height = 0.0f;
					BUFF_ITER(Npc, &gs.characters)
					{
						total_height += character_panel_height;
					}
					AABB sidebar = aabb_at(V2(screen_size().x - width - screen_margin, screen_size().y / 2.0f + total_height / 2.0f), V2(width, total_height));
					draw_centered_text((TextParams){false, S8Lit("Characters"), AddV2(sidebar.upper_left, V2(aabb_size(sidebar).x / 2.0f, 30.0f)), WHITE, 1.0f, .use_font = &font_for_text_input});
					float plus_size = 50.0f;
					if (imbutton(aabb_centered(AddV2(sidebar.lower_right, V2(-aabb_size(sidebar).x / 2.0f, -plus_size - screen_margin)), V2(plus_size, plus_size)), 1.0f, S8Lit(""), .icon = &image_add, .nobg = true))
					{
						Npc new = (Npc){.name = TextChunkLit("<Unnamed>"), .kind = get_next_kind(&gs)};
						BUFF_APPEND(&gs.characters, new);
					}

					Vec2 cur = sidebar.upper_left; // upper left corner of current
					BUFF_ITER_I(Npc, &gs.characters, i)
					{
						Quad panel = quad_at(cur, V2(width, character_panel_height));
						draw_quad((DrawParams){panel, IMG(image_white_square), ((Color){0.7f, 0.7f, 0.7f, 1.0f}), 1.0f, .layer = LAYER_UI});
						String8 name = TextChunkString8(it->name);
						bool rename = imbutton_key((ImbuttonArgs){aabb_at(cur, V2(width, 50.0f)), 1.0f, name, .key = tprint("%d %d", __LINE__, i), .dt = unwarped_dt}); // the hack here in the key is off the charts. Holy moly.
						get_state(state, TextInputResultKey, "%d %d", __LINE__, i);
						if (rename)
						{
							*state = begin_text_input(name);
						}
						String8 new = text_ready_for_me(*state);
						if (new.size > 0)
						{
							chunk_from_s8(&it->name, new);
						}

						Vec2 button_size = V2(75.0f, 75.0f);
						bool place = imbutton_key((ImbuttonArgs){aabb_at(AddV2(panel.lr, V2(-button_size.x, button_size.y)), button_size), 1.0f, S8Lit(""), .key = tprint("%d %d", __LINE__, i), .dt = unwarped_dt, .icon = &image_place, .nobg = true});
						bool edit = imbutton_key((ImbuttonArgs){aabb_at(AddV2(panel.lr, V2(-button_size.x * 2.0f, button_size.y)), button_size), 1.0f, S8Lit(""), .key = tprint("%d %d", __LINE__, i), .dt = unwarped_dt, .icon = &image_edit_brain, .nobg = true});
						if (place)
						{
							Entity *existing = npcs_entity(it->kind);
							if (!existing)
							{
								existing = new_entity(&gs);
								existing->npc_kind = it->kind;
								existing->is_npc = true;
							}
							existing->current_roomid = cur_room->roomid;
							gs.edit.placing_npc = it->kind;
						}
						if (edit)
						{
							gs.edit.editing_dialog_open = true;
							gs.edit.editing_npc = it->kind;
						}
						cur.y -= character_panel_height;
					}
					Entity *placing = npcs_entity(gs.edit.placing_npc);
					if (placing)
					{
						placing->pos = point_plane(point_on_plane_from_camera_point(view, mouse_pos));
						placing->current_roomid = cur_room->roomid;
						if (pressed.mouse_down)
						{
							gs.edit.placing_npc = NPC_nobody;
						}
					}

					Npc *editing = npc_data(&gs, gs.edit.editing_npc);
					get_state(editing_factor, float, "%d", __LINE__);
					bool is_editing = editing != &nobody_data && gs.edit.editing_dialog_open;
					*editing_factor = Lerp(*editing_factor, dt * 19.0f, is_editing ? 1.0f : 0.0f);
					{
						const float screen_margin = 100.0f;
						AABB editing_box = lerp_aabbs((AABB){.upper_left = aabb_center(screen_aabb()), .lower_right = aabb_center(screen_aabb())}, *editing_factor, grow_from_center(screen_aabb(), V2(-screen_margin, -screen_margin)));
						if (pressed.mouse_down && !has_point(editing_box, mouse_pos))
						{
							gs.edit.editing_dialog_open = false;
						}
						if (aabb_is_valid(editing_box))
						{
							draw_quad((DrawParams){quad_aabb(screen_aabb()), IMG(image_white_square), blendalpha(BLACK, 0.2f * *editing_factor), .layer = LAYER_UI});
							draw_quad((DrawParams){quad_aabb(editing_box), IMG(image_white_square), blendalpha(WHITE, 0.8f), .layer = LAYER_UI});
							float margin = 50.0f;
							AABB within = grow_from_center(editing_box, V2(-margin, -margin));
							float text_percent = 0.8f;
							if(aabb_is_valid(within)) {
								AABB desc = aabb_at(within.upper_left, V2(aabb_size(within).x*text_percent, aabb_size(within).y));
								String8 desc_str = TextChunkString8(editing->description);
								if(desc_str.size == 0) desc_str = S8Lit("<Empty>");
								PlacedWordList wrapped = place_wrapped_words(frame_arena, split_by_word(frame_arena, desc_str), 1.0f, aabb_size(desc).x, default_font, JUST_LEFT);
								float height = -wrapped.last->lower_left_corner.y + get_vertical_dist_between_lines(default_font, 1.0f);
								translate_words_by(wrapped, AddV2(desc.upper_left, V2(0, -get_vertical_dist_between_lines(default_font, 1.0f))));
								for(PlacedWord *cur = wrapped.first; cur; cur = cur->next) {
									draw_text((TextParams){false, cur->text, cur->lower_left_corner, blendalpha(BLACK, *editing_factor),1.0f});
								}

								AABB edit_button = aabb(stats(desc).ur, AddV2(stats(within).ur, V2(0, -height)));
								dbgrect(desc);
								bool edit = imbutton(grow_from_center(edit_button, V2(-(1.0f - *editing_factor) * aabb_size(edit_button).x, -(1.0f - *editing_factor) * aabb_size(edit_button).y)), 1.0f, S8Lit("Edit"));
								get_state(desc_result, TextInputResultKey, "%d", __LINE__);
								if (edit)
								{
									*desc_result = begin_text_input(desc_str);
								}
								String8 new = text_ready_for_me(*desc_result);
								if (new.size > 0)
								{
									chunk_from_s8(&editing->description, new);
								}
							}
						}
					}
				}

				gs.edit.camera_panning = LerpV2(gs.edit.camera_panning, unwarped_dt * 19.0f, gs.edit.camera_panning_target);
			}
		}

		PROFILE_SCOPE("Entity UI Rendering")
		{
			ENTITIES_ITER(gs.entities)
			{
				if (it->is_npc && it->npc_kind != NPC_player && it->current_roomid == get_cur_room(&gs, &level_threedee)->roomid)
				{
					if (it->undismissed_action)
					{
						assert(it->undismissed_action_tick <= gs.tick); // no future undismissed actions
					}

					// dialog bubble rendering
					const float text_scale = speech_bubble.text_scale;
					float dist = 0.0f;
					if (player(&gs))
						dist = LenV2(SubV2(it->pos, player(&gs)->pos));
					float bubble_factor = 1.0f - clamp01(dist / 6.0f);
					Vec3 bubble_pos = AddV3(plane_point(it->pos), V3(0, 1.7f, 0)); // 1.7 meters is about 5'8", average person height
					Vec2 head_pos = threedee_to_screenspace(bubble_pos);
					Vec2 screen_pos = head_pos;
					Vec2 size = V2(speech_bubble.width_in_pixels, speech_bubble.width_in_pixels);
					Vec2 bubble_center = AddV2(screen_pos, V2(-10.0f, 55.0f));
					float dialog_alpha = clamp01(bubble_factor * it->dialog_fade);
					bool unread = false;

					if (cur_unread_entity == it)
					{
						dialog_alpha = 1.0f;
						unread = true;
					}
					draw_quad((DrawParams){
						quad_centered(bubble_center, size),
						IMG(image_dialog_bubble),
						blendalpha(WHITE, dialog_alpha),
						.layer = LAYER_UI_FG,
					});
					// String8List words_to_say = words_on_current_page(it, &speech_bubble);
					String8List words_to_say = {0};
					if (unread)
					{
						draw_quad((DrawParams){
							quad_centered(AddV2(bubble_center, V2(size.x * 0.4f, -32.0f + (float)sin(unwarped_elapsed_time * 2.0) * 10.0f)), V2(32, 32)),
							IMG(image_unread_triangle),
							blendalpha(WHITE, 0.8f),
							.layer = LAYER_UI_FG,
						});

						if (pressed.interact)
						{
							if (it->words_said_on_page < words_to_say.node_count)
							{
								// still saying stuff
								it->words_said_on_page = (int)words_to_say.node_count;
							}
							else
							{
								it->cur_page_index += 1;
								if (words_on_current_page(it, &speech_bubble).node_count == 0)
								{
									// don't reset words_said_on_page because, even when the action is dismissed, the text for the last
									// page of dialog should still linger
									it->undismissed_action = false;
									it->cur_page_index -= 1;
								}
								else
								{
									it->characters_of_word_animated = 0.0f;
									it->words_said_on_page = 0;
								}
							}
							pressed.interact = false;
						}
					}
					it->loading_anim_in = Lerp(it->loading_anim_in, unwarped_dt * 5.0f, it->gen_request_id != 0 ? 1.0f : 0.0f);
					draw_quad((DrawParams){
						quad_rotated_centered(head_pos, V2(40, 40), (float)unwarped_elapsed_time * 2.0f),
						IMG(image_loading),
						blendalpha(WHITE, it->loading_anim_in),
						.layer = LAYER_UI_FG,
					});
					AABB placing_text_in = aabb_centered(AddV2(bubble_center, V2(0, 10.0f)), V2(speech_bubble.text_width_in_pixels, size.y * 0.15f));

					// String8List to_draw = words_on_current_page_without_unsaid(it, &speech_bubble);
					String8List to_draw = {0};
					if (to_draw.node_count != 0)
					{
						PlacedWordList placed = place_wrapped_words(frame_arena, to_draw, text_scale, aabb_size(placing_text_in).x, default_font, JUST_LEFT);

						// also called on npc response to see if it fits in the right amount of bubbles, if not tells AI how many words it has to trim its response by
						// translate_words_by(placed, V2(placing_text_in.upper_left.x, placing_text_in.lower_right.y));
						translate_words_by(placed, AddV2(placing_text_in.upper_left, V2(0, -get_vertical_dist_between_lines(default_font, text_scale))));
						for (PlacedWord *cur = placed.first; cur; cur = cur->next)
						{
							draw_text((TextParams){false, cur->text, cur->lower_left_corner, blendalpha(colhex(0xEEE6D2), dialog_alpha), text_scale});
						}
					}
				}
			}
		}


		// draw and manage cutscene 
		if(gs.unprocessed_first)
		{
			bool delete = false;
			do {
				delete = false;
				CutsceneEvent *cut = gs.unprocessed_first;
				if(!gete(cut->author)) {
					delete = true;
				} else {
					switch (cut->action.kind)
					{
						case ACT_invalid:
						case ACT_none:
							delete = true;
							break;
						case ACT_say_to:
						{
							Entity *e = gete(cut->author);
							Vec3 threedee_point_at_head = AddV3(plane_point(e->pos), V3(0, 1.7f, 0)); // 1.7 meters is about 5'8", average person height
							Vec2 bubble_center = AddV2(threedee_to_screenspace(threedee_point_at_head), V2(0, 50.0f));

							String8 whole_speech = TCS8(cut->action.args.data[1].text);
							String8 trimmed = S8Substring(whole_speech, 0, (u64)cut->said_letters_of_speech);
							
							PlacedWordList wrapped = place_wrapped_words(frame_arena, split_by_word(frame_arena, trimmed), 1.0f, screen_size().x*0.7f, default_font, JUST_CENTER);
							// for(PlacedWord *cur = wrapped.first; cur; cur = cur->next) {
							// 	width = max(width, cur->lower_left_corner.x + cur->size.x);
							// }
							// float height = -wrapped.last->lower_left_corner.y + get_vertical_dist_between_lines(default_font, 1.0f);
							translate_words_by(wrapped,  V2(screen_size().x*0.3f/2.0f, -get_vertical_dist_between_lines(default_font, 1.0f) + bubble_center.y));
							AABB bounds = (AABB){
								.upper_left = V2(INFINITY, -INFINITY),
								.lower_right = V2(-INFINITY, INFINITY),
							};
							for(PlacedWord *cur = wrapped.first; cur; cur = cur->next) {
								AABB word_bounds = draw_text((TextParams){false, cur->text, cur->lower_left_corner, blendalpha(WHITE, 1.0f),1.0f, .layer = LAYER_UI_FG});
								bounds = update_bounds(bounds, quad_aabb(word_bounds));
							}
							if(trimmed.size > 0) {
								AABB bg = grow_from_center(bounds, V2(25.0f, 25.0f));
								draw_quad((DrawParams){quad_aabb(bg), IMG(image_white_square), blendalpha(BLACK, 0.8f), .layer = LAYER_UI});
							}

							cut->said_letters_of_speech += unwarped_dt_double*CHARACTERS_PER_SEC;

							if(!mobile_controls) {
								draw_centered_text((TextParams){
									false,
									S8Lit("(Press E)"),
									V2(screen_size().x/2.0f, screen_size().y*0.20f),
									WHITE,
									1.0f,
									.layer = LAYER_UI_FG,
								});
							}
							if(pressed.interact) {
								if(cut->said_letters_of_speech < whole_speech.size) {
									cut->said_letters_of_speech = whole_speech.size + 1.0;
								} else {
									delete = true;
								}
								pressed.interact = false;
							}
						}
						break;
					}
				}
				if(delete) DblRemove(gs.unprocessed_first, gs.unprocessed_last, gs.unprocessed_first);
			} while(delete && gs.unprocessed_first); // so that events that shouldn't be there like none events or broken ones, the camera doesn't pan to them, delete them in sequence lik this, such that either the currnet unprocessed first should be animated/focusd on or it shouldn't be there anymore 
		}


		// gameplay processing loop, do multiple if lagging
		// these are static so that, on frames where no gameplay processing is necessary and just rendering, the rendering uses values from last frame
		// @Place(gameplay processing loops)
		static Entity *interacting_with = 0; // used by rendering to figure out who to draw dialog box on
		static bool player_in_combat = false;

		float speed_target = 1.0f;
		gs.stopped_time = gs.unprocessed_first != 0;
		if (gs.stopped_time)
			speed_target = 0.0f;
		// pausing the game
		speed_factor = Lerp(speed_factor, unwarped_dt * 10.0f, speed_target);
		if (fabsf(speed_factor - speed_target) <= 0.05f)
		{
			speed_factor = speed_target;
		}
		int num_timestep_loops = 0;
		// restore the pressed state after gameplay loop so pressed input events can be processed in the
		// rendering correctly as well
		PressedState before_gameplay_loops = pressed;
		bool keypressed_before_gameplay[SAPP_KEYCODE_MAX];
		memcpy(keypressed_before_gameplay, keypressed, sizeof(keypressed));
		PROFILE_SCOPE("gameplay processing")
		{
			uint64_t time_start_gameplay_processing = stm_now();
			unprocessed_gameplay_time += unwarped_dt;
			float timestep = fminf(unwarped_dt, (float)MINIMUM_TIMESTEP);
			while (unprocessed_gameplay_time >= timestep)
			{
				num_timestep_loops++;
				unprocessed_gameplay_time -= timestep;
				float unwarped_dt = timestep;
				float dt = unwarped_dt * speed_factor;

				gs.tick += 1;

				if (!gs.edit.enabled && player(&gs) == 0)
				{
					Entity *player = new_entity(&gs);
					player->is_npc = true;
					player->npc_kind = NPC_player;
					player->current_roomid = gs.edit.player_spawn_roomid;
					player->pos = gs.edit.player_spawn_position;
				}
				if(player(&gs)) gs.current_roomid = player(&gs)->current_roomid;

				// process gs.entities process entities
				PROFILE_SCOPE("entity processing")
				{
					ENTITIES_ITER(gs.entities)
					{
						assert(!(it->exists && it->generation == 0));

						if (LenV2(it->last_moved) > 0.0f && !it->killed)
							it->rotation = lerp_angle(it->rotation, dt * (it->quick_turning_timer > 0 ? 12.0f : 8.0f), AngleOfV2(it->last_moved));

						if (it->is_npc)
						{
							// @Place(entity processing)
							if (it->dialog_fade > 0.0f)
								it->dialog_fade -= dt / DIALOG_FADE_TIME;
							Entity *e = it;
								
							Entity *toface = 0;
							if (gete(it->aiming_shotgun_at))
							{
								toface = gete(it->aiming_shotgun_at);
							}
							else if (gete(it->looking_at))
							{
								toface = gete(it->looking_at);
							}
							else if(gs.unprocessed_first && gs.unprocessed_first->action.kind == ACT_say_to && gete(gs.unprocessed_first->author) == it)
							{
								toface = gs.unprocessed_first->action.args.data[0].character;
							}
							if (toface)
								it->target_rotation = AngleOfV2(SubV2(toface->pos, it->pos));

							if (it->npc_kind != NPC_player)
								it->rotation = lerp_angle(it->rotation, unwarped_dt * 8.0f, it->target_rotation);

							if (it->gen_request_id != 0 && !gs.stopped_time)
							{
								assert(it->gen_request_id > 0);

								GenRequestStatus status = gen_request_status(it->gen_request_id);
								switch (status)
								{
								case GEN_Deleted:
									it->gen_request_id = 0;
									break;
								case GEN_NotDoneYet:
									break;
								case GEN_Success:
								{
									having_errors = false;
									// done! we can get the string
									String8 content = gen_request_content(frame_arena, it->gen_request_id);
									ParseResult parsed = ParseWholeString(frame_arena, S8Lit("generated"), content);
									if(!having_errors && parsed.errors.node_count > 0) {
										Log("The server returned bogged data:\n");
										for(Message *cur = parsed.errors.first; cur; cur = cur->next) {
											Log("%.*s\n", S8VArg(cur->string));
										}
										having_errors = true;
									}
									ServerResponse response = {0};
									if(!having_errors) {
										Node *root = parsed.node->first_child;
										parse_response(frame_arena, &response, root);
										if(response.error.size > 0) {
											Log("Error from server: %.*s\n", S8VArg(response.error));
											having_errors = true;
										} 
									}
									if(NodeIsNil(response.ai_response)) {
										if(response.ai_error.size == 0) {
											Log("When the root node is nil, there should be an AI error to explain why. But this isn't the case. Either a logic or server error\n");
											having_errors = true;
										}
									}
									bool ai_error = false;
									if(!having_errors) {
										if(response.ai_error.size > 0) {
											Log("Got an AI error: '%.*s'", S8VArg(response.ai_error));
											ai_error = true;
											TextChunk err = {0};
											chunk_from_s8(&err, S8Chop(response.ai_error, MAX_SENTENCE_LENGTH));
											BUFF_QUEUE_APPEND(&e->error_notices, err);
										}
										String8 err = {0};
										FullResponse *resp = json_to_responses(frame_arena, response.ai_response, &err);
										BUFF_ITER(Response, resp) {
											Action baked = bake_into_action(frame_arena, &err, &gs, e, it);
											if(err.size > 0) {
												Log("Error while baking to action: '%.*s'", S8VArg(err));
												ai_error = true;
												TextChunk out_err = {0};
												chunk_from_s8(&out_err, S8Chop(err, MAX_SENTENCE_LENGTH));
												BUFF_QUEUE_APPEND(&e->error_notices, out_err);
												break;
											}
											perform_action(e, baked);
										}
									}

									done_with_request(it->gen_request_id);
									it->gen_request_id = 0;
									if(ai_error) {
										it->perceptions_dirty = true;
									} else if(!having_errors) {
										BUFF_CLEAR(&it->error_notices);
									}
								}
								break;
								case GEN_Failed:
									Log("Failed to generate dialog! Fuck!\n");
									having_errors = true;
									it->gen_request_id = 0;
									break;
								default:
									Log("Unknown generation request status: %d\n", status);
									it->gen_request_id = 0;
									break;
								}
							}
						}

						it->being_hovered = false;

						if (it->is_npc)
						{
							// character speech animation text input
							{
								ArenaTemp scratch = GetScratch(0, 0);

								// String8List to_say = words_on_current_page(it, &speech_bubble);
								// String8List to_say_without_unsaid = words_on_current_page_without_unsaid(it, &speech_bubble);
								String8List to_say = {0};
								String8List to_say_without_unsaid = {0};
								if (to_say.node_count > 0 && it->words_said_on_page < to_say.node_count)
								{
									if (cur_unread_entity == it)
									{
										it->characters_of_word_animated += CHARACTERS_PER_SEC * unwarped_dt;
										int characters_in_animating_word = (int)to_say_without_unsaid.last->string.size;
										if ((int)it->characters_of_word_animated + 1 > characters_in_animating_word)
										{
											it->words_said_on_page += 1;
											it->characters_of_word_animated = 0.0f;

											float dist = LenV2(SubV2(it->pos, player(&gs)->pos));
											float volume = Lerp(-0.6f, clamp01(dist / 70.0f), -1.0f);
											AudioSample *possible_grunts[] = {
												&sound_grunt_0,
												&sound_grunt_1,
												&sound_grunt_2,
												&sound_grunt_3,
											};
											play_audio(possible_grunts[rand() % ARRLEN(possible_grunts)], volume);
										}
									}
								}

								ReleaseScratch(scratch);
							}

							if (gete(it->joined))
							{
								int place_in_line = 1;
								Entity *e = it;
								ENTITIES_ITER(gs.entities)
								{
									if (it->is_npc && gete(it->joined) == gete(e->joined))
									{
										if (it == e)
											break;
										place_in_line += 1;
									}
								}

								Vec2 target = get_point_along_trail(BUFF_MAKEREF(&it->position_history), (float)place_in_line * 1.0f);

								Vec2 last_pos = it->pos;
								it->pos = LerpV2(it->pos, dt * 5.0f, target);
								if (LenV2(SubV2(it->pos, last_pos)) > 0.01f)
								{
									it->last_moved = NormV2(SubV2(it->pos, last_pos));
									it->vel = MulV2F(it->last_moved, 1.0f / dt);
								}
								else
								{
									it->vel = V2(0, 0);
								}
							}

							// A* code
							if (false)
							{
								Entity *targeting = player(&gs);

								/*
									 G cost: distance from the current node to the start node
									 H cost: distance from the current node to the target node

									 G     H
									 SUM
									 F cost: G + H
									 */
								Vec2 to = targeting->pos;

								PathCache *cached = get_path_cache(elapsed_time, it->cached_path);
								AStarPath path = {0};
								bool succeeded = false;
								if (cached)
								{
									path = cached->path;
									succeeded = true;
								}
								else
								{
									Vec2 from = it->pos;
									typedef struct AStarNode
									{
										bool exists;
										struct AStarNode *parent;
										bool in_closed_set;
										bool in_open_set;
										float f_score; // total of g score and h score
										float g_score; // distance from the node to the start node
										Vec2 pos;
									} AStarNode;

									BUFF(AStarNode, MAX_ASTAR_NODES)
									nodes = {0};
									struct
									{
										Vec2 key;
										AStarNode *value;
									} *node_cache = 0;
#define V2_HASH(v) (FloorV2(v))
									const float jump_size = TILE_SIZE / 2.0f;
									BUFF_APPEND(&nodes, ((AStarNode){.in_open_set = true, .pos = from}));
									Vec2 from_hash = V2_HASH(from);
									float got_there_tolerance = max_coord(entity_aabb_size(targeting)) * 1.5f;
									hmput(node_cache, from_hash, &nodes.data[0]);

									bool should_quit = false;
									AStarNode *last_node = 0;
									PROFILE_SCOPE("A* Pathfinding") // astar pathfinding a star
									while (!should_quit)
									{
										int openset_size = 0;
										BUFF_ITER(AStarNode, &nodes)
										if (it->in_open_set) openset_size += 1;
										if (openset_size == 0)
										{
											should_quit = true;
										}
										else
										{
											AStarNode *current = 0;
											PROFILE_SCOPE("Get lowest fscore astar node in open set")
											{
												float min_fscore = INFINITY;
												int min_fscore_index = -1;
												BUFF_ITER_I(AStarNode, &nodes, i)
												if (it->in_open_set)
												{
													if (it->f_score < min_fscore)
													{
														min_fscore = it->f_score;
														min_fscore_index = i;
													}
												}
												assert(min_fscore_index >= 0);
												current = &nodes.data[min_fscore_index];
												assert(current);
											}

											float length_to_goal = 0.0f;
											PROFILE_SCOPE("get length to goal")
											length_to_goal = LenV2(SubV2(to, current->pos));

											if (length_to_goal <= got_there_tolerance)
											{
												succeeded = true;
												should_quit = true;
												last_node = current;
											}
											else
											{
												current->in_open_set = false;
												Vec2 neighbor_positions[] = {
													V2(-jump_size, 0.0f),
													V2(jump_size, 0.0f),
													V2(0.0f, jump_size),
													V2(0.0f, -jump_size),

													V2(-jump_size, jump_size),
													V2(jump_size, jump_size),
													V2(jump_size, -jump_size),
													V2(-jump_size, -jump_size),
												};
												ARR_ITER(Vec2, neighbor_positions) *it = AddV2(*it, current->pos);

												Entity *e = it;

												PROFILE_SCOPE("Checking neighbor positions")
												ARR_ITER(Vec2, neighbor_positions)
												{
													Vec2 cur_pos = *it;

													dbgsquare(cur_pos);

													bool would_block_me = false;

													PROFILE_SCOPE("Checking for overlap")
													{
														Overlapping overlapping_at_want = get_overlapping(entity_aabb_at(e, cur_pos));
														BUFF_ITER(Entity *, &overlapping_at_want)
														if (*it != e) would_block_me = true;
													}

													if (would_block_me)
													{
													}
													else
													{
														AStarNode *existing = 0;
														Vec2 hash = V2_HASH(cur_pos);
														existing = hmget(node_cache, hash);

														if (false)
															PROFILE_SCOPE("look for existing A* node")
														BUFF_ITER(AStarNode, &nodes)
														{
															if (V2ApproxEq(it->pos, cur_pos))
															{
																existing = it;
																break;
															}
														}

														float tentative_gscore = current->g_score + jump_size;
														if (tentative_gscore < (existing ? existing->g_score : INFINITY))
														{
															if (!existing)
															{
																if (!BUFF_HAS_SPACE(&nodes))
																{
																	should_quit = true;
																	succeeded = false;
																}
																else
																{
																	BUFF_APPEND(&nodes, (AStarNode){0});
																	existing = &nodes.data[nodes.cur_index - 1];
																	existing->pos = cur_pos;
																	Vec2 pos_hash = V2_HASH(cur_pos);
																	hmput(node_cache, pos_hash, existing);
																}
															}

															if (existing)
																PROFILE_SCOPE("estimate heuristic")
																{
																	existing->parent = current;
																	existing->g_score = tentative_gscore;
																	float h_score = 0.0f;
																	{
																		// diagonal movement heuristic from some article
																		Vec2 curr_cell = *it;
																		Vec2 goal = to;
																		float D = jump_size;
																		float D2 = LenV2(V2(jump_size, jump_size));
																		float dx = fabsf(curr_cell.x - goal.x);
																		float dy = fabsf(curr_cell.y - goal.y);
																		float h = D * (dx + dy) + (D2 - 2 * D) * fminf(dx, dy);

																		h_score += h;
																		// approx distance with manhattan distance
																		// h_score += fabsf(existing->pos.x - to.x) + fabsf(existing->pos.y - to.y);
																	}
																	existing->f_score = tentative_gscore + h_score;
																	existing->in_open_set = true;
																}
														}
													}
												}
											}
										}
									}

									hmfree(node_cache);
									node_cache = 0;

									// reconstruct path
									if (succeeded)
									{
										assert(last_node);
										AStarNode *cur = last_node;
										while (cur)
										{
											BUFF_PUSH_FRONT(&path, cur->pos);
											cur = cur->parent;
										}
									}

									if (succeeded)
										it->cached_path = cache_path(elapsed_time, &path);
								}

								Vec2 next_point_on_path = {0};
								if (succeeded)
								{
									float nearest_dist = INFINITY;
									int nearest_index = -1;
									Entity *from = it;
										BUFF_ITER_I(Vec2, &path, i)
									{
										float dist = LenV2(SubV2(*it, from->pos));
										if (dist < nearest_dist)
										{
											nearest_dist = dist;
											nearest_index = i;
										}
									}
									assert(nearest_index >= 0);
									int target_index = (nearest_index + 1);

									if (target_index >= path.cur_index)
									{
										next_point_on_path = to;
									}
									else
									{
										next_point_on_path = path.data[target_index];
									}
								}

								BUFF_ITER_I(Vec2, &path, i)
								{

									if (i == 0)
									{
									}
									else
									{
										dbgcol(BLUE) dbgline(*it, path.data[i - 1]);
									}
								}
							}


							if (false) // used to be old man code
							{
								/*
									 draw_dialog_panel(it);
									 Entity *targeting = player;
									 it->shotgun_timer += dt;
									 Vec2 to_player = NormV2(SubV2(targeting->pos, it->pos));
									 if(it->shotgun_timer >= 1.0f)
									 {
									 it->shotgun_timer = 0.0f;
									 const float spread = (float)PI/4.0f;
								// shoot shotgun
								int num_bullets = 5;
								for(int i = 0; i < num_bullets; i++)
								{
								Vec2 dir = to_player;
								float theta = Lerp(-spread/2.0f, ((float)i / (float)(num_bullets - 1)), spread/2.0f);
								dir = RotateV2(dir, theta);
								Entity *new_bullet = new_entity();
								new_bullet->is_bullet = true;
								new_bullet->pos = AddV2(it->pos, MulV2F(dir, 20.0f));
								new_bullet->vel = MulV2F(dir, 15.0f);
								it->vel = AddV2(it->vel, MulV2F(dir, -3.0f));
								}
								}

								Vec2 target_vel = NormV2(AddV2(rotate_counter_clockwise(to_player), MulV2F(to_player, 0.5f)));
								target_vel = MulV2F(target_vel, 3.0f);
								it->vel = LerpV2(it->vel, 15.0f * dt, target_vel);
								it->pos = move_and_slide((MoveSlideParams){it, it->pos, MulV2F(it->vel, pixels_per_meter * dt)});
								*/
							}
							// @Place(NPC processing)
							else
							{
							}

							if (it->damage >= entity_max_damage(it))
							{
								it->destroy = true;
							}
						}
						else if (it->is_world)
						{
						}
						else
						{
							assert(false);
						}
					}
				}

				PROFILE_SCOPE("Destroy gs.entities, maybe send generation requests")
				{
					for (int i = 0; i < ARRLEN(gs.entities); i++)
					{
						Entity *it = &gs.entities[i];
						if (it->destroy)
						{
							int gen = it->generation;
							*it = (Entity){0};
							it->generation = gen;
						}
					}

					ENTITIES_ITER(gs.entities)
					if (it->is_npc)
					{
						bool doesnt_prompt_on_dirty_perceptions = false || it->current_roomid != get_cur_room(&gs, &level_threedee)->roomid || it->npc_kind == NPC_player;
						if (it->perceptions_dirty && doesnt_prompt_on_dirty_perceptions)
						{
							it->perceptions_dirty = false;
						}
						if (it->perceptions_dirty)
						{
							if (!gs.stopped_time)
							{
								it->perceptions_dirty = false; // needs to be in beginning because they might be redirtied by the new perception
								String8 prompt_str = {0};
								// prompt_str = generate_chatgpt_prompt(frame_arena, &gs, it, get_can_talk_to(it));
								Npc *npc = get_hardcoded_npc(frame_arena, TCS8(npc_data(&gs, it->npc_kind)->name), it->npc_kind);
								prompt_str = generate_chatgpt_prompt(frame_arena, npc, generate_situation(frame_arena, &gs, it));
								Log("Want to make request with prompt `%.*s`\n", S8VArg(prompt_str));

								bool mocking_the_ai_response = false;
#ifdef DEVTOOLS
#ifdef MOCK_AI_RESPONSE
								mocking_the_ai_response = true;
#endif												   // mock
#endif												   // devtools
								if (mocking_the_ai_response)
								{
									Log("TODO unimplemented\n");
								}
								else
								{
									it->gen_request_id = make_generation_request(prompt_str);
								}
#undef SAY
							}
						}
					}
				}

				// @Place(process player)
				if (player(&gs))
					PROFILE_SCOPE("process player")
					{
						String8 text_to_say = text_ready_for_me(player(&gs)->player_input_key);
						if(text_to_say.size > 0) {
							String8 no_whitespace = S8SkipWhitespace(S8ChopWhitespace(text_to_say));
							if (no_whitespace.size == 0)
							{
								// this just means cancel the dialog
							}
							else
							{
								Entity *targeting = gete(player(&gs)->talking_to);
								if(targeting)
								{
									String8 what_player_said = text_to_say;
									what_player_said = S8ListJoin(frame_arena, S8Split(frame_arena, what_player_said, 1, &S8Lit("\n")), &(StringJoin){0});
									Action act = {
										.kind = ACT_say_to,
									};
									act.args.data[0].character = targeting;
									chunk_from_s8(&act.args.data[1].text, what_player_said);
									act.args.cur_index = 2;
									perform_action(player(&gs), act);
								}
							}
						}
						// do dialog
						Entity *closest_interact_with = 0;
						{
							// find closest to talk to
							{
								AABB dialog_rect = aabb_centered(player(&gs)->pos, V2(DIALOG_INTERACT_SIZE, DIALOG_INTERACT_SIZE));
								Overlapping possible_dialogs = get_overlapping(dialog_rect);
								float closest_interact_with_dist = INFINITY;
								BUFF_ITER(Entity *, &possible_dialogs)
								{
									bool entity_talkable = true;
									if (entity_talkable)
										entity_talkable = entity_talkable && (*it)->is_npc;
									if (entity_talkable)
										entity_talkable = entity_talkable && (*it)->npc_kind != NPC_player;
									if (entity_talkable)
										entity_talkable = entity_talkable && !(*it)->killed;
#ifdef WEB
									if (entity_talkable)
										entity_talkable = entity_talkable && (*it)->gen_request_id == 0;
#endif

									bool entity_interactible = entity_talkable;

									if (entity_interactible)
									{
										float dist = LenV2(SubV2((*it)->pos, player(&gs)->pos));
										if (dist < closest_interact_with_dist)
										{
											closest_interact_with_dist = dist;
											closest_interact_with = (*it);
										}
									}
								}
							}
							interacting_with = closest_interact_with;

							player(&gs)->interacting_with = frome(interacting_with);
						}

						if (pressed.interact)
						{
							if (closest_interact_with)
							{
								if (closest_interact_with->is_npc)
								{
									// begin dialog with closest npc
									player(&gs)->talking_to = frome(closest_interact_with);
									player(&gs)->player_input_key = begin_text_input(S8Lit(""));
								}
								else
								{
									assert(false);
								}
							}
						}

						float speed = 0.0f;
						if (!player(&gs)->killed)
							speed = PLAYER_SPEED;
						// velocity processing for player player movement
						{
							player(&gs)->last_moved = NormV2(movement);
							Vec2 target_vel = MulV2F(movement, pixels_per_meter * speed);
							float player_speed = LenV2(player(&gs)->vel);
							float target_speed = LenV2(target_vel);
							bool quick_turn = (player_speed < target_speed / 2) || DotV2(player(&gs)->vel, target_vel) < -0.707f;
							player(&gs)->quick_turning_timer -= dt;
							if (quick_turn)
							{
								player(&gs)->quick_turning_timer = 0.125f;
							}
							if (quick_turn)
							{
								player(&gs)->vel = target_vel;
							}
							else
							{ // framerate-independent smoothly transition towards target (functions as friction when target is 0)
								player(&gs)->vel = SubV2(player(&gs)->vel, target_vel);
								player(&gs)->vel = MulV2F(player(&gs)->vel, powf(1e-8f, dt));
								player(&gs)->vel = AddV2(player(&gs)->vel, target_vel);
							}
							// printf("%f%s\n", LenV2(player(&gs)->vel), player(&gs)->quick_turning_timer > 0 ? " QUICK TURN" : "");

							player(&gs)->pos = move_and_slide((MoveSlideParams){player(&gs), player(&gs)->pos, MulV2F(player(&gs)->vel, dt)});

							bool should_append = false;

							// make it so no snap when new points added
							if (player(&gs)->position_history.cur_index > 0)
							{
								player(&gs)->position_history.data[player(&gs)->position_history.cur_index - 1] = player(&gs)->pos;
							}

							if (player(&gs)->position_history.cur_index > 2)
							{
								should_append = LenV2(SubV2(player(&gs)->position_history.data[player(&gs)->position_history.cur_index - 2], player(&gs)->pos)) > TILE_SIZE;
							}
							else
							{
								should_append = true;
							}
							if (should_append)
								BUFF_QUEUE_APPEND(&player(&gs)->position_history, player(&gs)->pos);
						}
						// health
						if (player(&gs)->damage >= 1.0)
						{
							reset_level();
						}
					}

				pressed = (PressedState){0};
				memset(keypressed, 0, sizeof(keypressed));
			} // while loop

			last_frame_gameplay_processing_time = stm_sec(stm_diff(stm_now(), time_start_gameplay_processing));
		}
		memcpy(keypressed, keypressed_before_gameplay, sizeof(keypressed));
		pressed = before_gameplay_loops;

#ifdef DEVTOOLS
		if (flycam)
		{
			Basis basis = flycam_basis();
			float speed = 2.0f;
			speed *= flycam_speed;
			flycam_pos = AddV3(flycam_pos, MulV3F(basis.forward, ((float)keydown[SAPP_KEYCODE_W] - (float)keydown[SAPP_KEYCODE_S]) * speed * dt));
			flycam_pos = AddV3(flycam_pos, MulV3F(basis.right, ((float)keydown[SAPP_KEYCODE_D] - (float)keydown[SAPP_KEYCODE_A]) * speed * dt));
			flycam_pos = AddV3(flycam_pos, MulV3F(basis.up, (((float)keydown[SAPP_KEYCODE_SPACE] + (float)keydown[SAPP_KEYCODE_LEFT_CONTROL]) - (float)keydown[SAPP_KEYCODE_LEFT_SHIFT]) * speed * dt));
		}
#endif

		// @Place(player rendering)
		if (0)
			PROFILE_SCOPE("render player") // draw character draw player render character
			{
				if (player(&gs)->position_history.cur_index > 0)
				{
					float trail_len = get_total_trail_len(BUFF_MAKEREF(&player(&gs)->position_history));
					if (trail_len > 0.0f) // fmodf returns nan
					{
						float along = fmodf((float)elapsed_time * 100.0f, 200.0f);
						Vec2 at = get_point_along_trail(BUFF_MAKEREF(&player(&gs)->position_history), along);
						dbgbigsquare(at);
						dbgbigsquare(get_point_along_trail(BUFF_MAKEREF(&player(&gs)->position_history), 50.0f));
					}
					BUFF_ITER_I(Vec2, &player(&gs)->position_history, i)
					{
						if (i == player(&gs)->position_history.cur_index - 1)
						{
						}
						else
						{
							dbgline(*it, player(&gs)->position_history.data[i + 1]);
						}
					}
				}

				// if somebody, show their dialog panel
				if (interacting_with)
				{
					// interaction keyboard hint
					if (!mobile_controls)
					{
						float size = 100.0f;
						Vec2 midpoint = MulV2F(AddV2(interacting_with->pos, player(&gs)->pos), 0.5f);
						draw_quad((DrawParams){quad_centered(AddV2(midpoint, V2(0.0, 5.0f + sinf((float)elapsed_time * 3.0f) * 5.0f)), V2(size, size)), IMG(image_e_icon), blendalpha(WHITE, clamp01(1.0f - learned_e)), .layer = LAYER_UI_FG});
					}

					// interaction circle
					draw_quad((DrawParams){quad_centered(interacting_with->pos, V2(TILE_SIZE, TILE_SIZE)), image_hovering_circle, full_region(image_hovering_circle), WHITE, .layer = LAYER_UI});
				}

				// hurt vignette
				if (player(&gs)->damage > 0.0)
				{
					draw_quad((DrawParams){
						(Quad){.ul = V2(0.0f, screen_size().Y), .ur = screen_size(), .lr = V2(screen_size().X, 0.0f)},
						image_hurt_vignette,
						full_region(image_hurt_vignette),
						(Color){1.0f, 1.0f, 1.0f, player(&gs)->damage},
						.layer = LAYER_SCREENSPACE_EFFECTS,
					});
				}
			}

		if (having_errors)
		{
			Vec2 text_center = V2(screen_size().x / 2.0f, screen_size().y * 0.8f);
			draw_quad((DrawParams){centered_quad(text_center, V2(screen_size().x * 0.8f, screen_size().y * 0.1f)), IMG(image_white_square), blendalpha(BLACK, 0.5f), .layer = LAYER_ULTRA_IMPORTANT_NOTIFICATIONS});
			draw_centered_text((TextParams){false, S8Lit("The AI server is having technical difficulties..."), text_center, WHITE, 1.0f, .layer = LAYER_ULTRA_IMPORTANT_NOTIFICATIONS});
		}

		// win screen
		{
			static float visible = 0.0f;
			float target = 0.0f;
			if (gs.won)
			{
				target = 1.0f;
			}
			visible = Lerp(visible, unwarped_dt * 9.0f, target);

			draw_quad((DrawParams){quad_at(V2(0, screen_size().y), screen_size()), IMG(image_white_square), blendalpha(BLACK, visible * 0.7f), .layer = LAYER_UI});
			float shake_speed = 9.0f;
			Vec2 win_offset = V2(sinf((float)unwarped_elapsed_time * shake_speed * 1.5f + 0.1f), sinf((float)unwarped_elapsed_time * shake_speed + 0.3f));
			win_offset = MulV2F(win_offset, 10.0f);
			draw_centered_text((TextParams){false, S8Lit("YOU WON"), AddV2(MulV2F(screen_size(), 0.5f), win_offset), WHITE, 9.0f * visible});

			if (imbutton(aabb_centered(V2(screen_size().x / 2.0f, screen_size().y * 0.25f), MulV2F(V2(170.0f, 60.0f), visible)), 1.5f * visible, S8Lit("Restart")))
			{
				reset_level();
			}
		}

		// killed screen
		if (player(&gs))
		{
			static float visible = 0.0f;
			float target = 0.0f;
			bool anybody_unread = false;
			ENTITIES_ITER(gs.entities)
			{
				if (it->undismissed_action)
					anybody_unread = true;
			}
			if (player(&gs)->killed && (!anybody_unread || gs.finished_reading_dying_dialog))
			{
				gs.finished_reading_dying_dialog = true;
				target = 1.0f;
			}
			visible = Lerp(visible, unwarped_dt * 4.0f, target);

			draw_quad((DrawParams){quad_at(V2(0, screen_size().y), screen_size()), IMG(image_white_square), blendalpha(BLACK, visible * 0.7f), .layer = LAYER_UI});
			float shake_speed = 9.0f;
			Vec2 win_offset = V2(sinf((float)unwarped_elapsed_time * shake_speed * 1.5f + 0.1f), sinf((float)unwarped_elapsed_time * shake_speed + 0.3f));
			win_offset = MulV2F(win_offset, 10.0f);
			draw_centered_text((TextParams){false, S8Lit("YOU FAILED TO SAVE DANIEL"), AddV2(MulV2F(screen_size(), 0.5f), win_offset), WHITE, 3.0f * visible}); // YOU DIED

			if (imbutton(aabb_centered(V2(screen_size().x / 2.0f, screen_size().y * 0.25f), MulV2F(V2(170.0f, 60.0f), visible)), 1.5f * visible, S8Lit("Continue")))
			{
				player(&gs)->killed = false;
				// transition_to_room(&gs, &level_threedee, S8Lit("StartingRoom"));
				reset_level();
			}
		}

#define HELPER_SIZE 250.0f

		// keyboard tutorial icons
		if (false)
			if (!mobile_controls)
			{
				float total_height = HELPER_SIZE * 2.0f;
				float vertical_spacing = HELPER_SIZE / 2.0f;
				total_height -= (total_height - (vertical_spacing + HELPER_SIZE));
				const float padding = 50.0f;
				float y = screen_size().y / 2.0f + total_height / 2.0f;
				float x = screen_size().x - padding - HELPER_SIZE;
				draw_quad((DrawParams){quad_at(V2(x, y), V2(HELPER_SIZE, HELPER_SIZE)), IMG(image_shift_icon), (Color){1.0f, 1.0f, 1.0f, fmaxf(0.0f, 1.0f - learned_shift)}, .layer = LAYER_UI_FG});
				y -= vertical_spacing;
				draw_quad((DrawParams){quad_at(V2(x, y), V2(HELPER_SIZE, HELPER_SIZE)), IMG(image_space_icon), (Color){1.0f, 1.0f, 1.0f, fmaxf(0.0f, 1.0f - learned_space)}, .layer = LAYER_UI_FG});
			}

		if (mobile_controls)
		{
			float thumbstick_nub_size = (img_size(image_mobile_thumbstick_nub).x / img_size(image_mobile_thumbstick_base).x) * thumbstick_base_size();
			draw_quad((DrawParams){quad_centered(thumbstick_base_pos, V2(thumbstick_base_size(), thumbstick_base_size())), IMG(image_mobile_thumbstick_base), WHITE, .layer = LAYER_UI_FG});
			draw_quad((DrawParams){quad_centered(thumbstick_nub_pos, V2(thumbstick_nub_size, thumbstick_nub_size)), IMG(image_mobile_thumbstick_nub), WHITE, .layer = LAYER_UI_FG});

			if (interacting_with)
			{
				draw_quad((DrawParams){quad_centered(interact_button_pos(), V2(mobile_button_size(), mobile_button_size())), IMG(image_mobile_button), WHITE, .layer = LAYER_UI_FG});
			}
			draw_quad((DrawParams){quad_centered(roll_button_pos(), V2(mobile_button_size(), mobile_button_size())), IMG(image_mobile_button), WHITE, .layer = LAYER_UI_FG});
			draw_quad((DrawParams){quad_centered(attack_button_pos(), V2(mobile_button_size(), mobile_button_size())), IMG(image_mobile_button), WHITE, .layer = LAYER_UI_FG});
		}

#ifdef DEVTOOLS
		if(RELOAD_ON_MOUSEUP)
		if(pressed.mouse_up) {
			String8 error = {0};
			Log("Saving gamestate...\n");
			String8 saved = save_to_string(frame_arena, frame_arena, &error, &gs);
			if(error.size > 0) {
				Log("Failed to save gamestate: %.*s\n", S8VArg(error));
			} else {
				WriteEntireFile((char*)nullterm(frame_arena, S8Lit("assets/main_game_level.bin")).str, saved);
			}
			// reload from the saved data to make sure that functionality works
			if(error.size == 0) {
				GameState *deserialized_gs = load_from_string(frame_arena, frame_arena, saved, &error);
				if(error.size == 0) {
					cleanup_gamestate(&gs);
					gs = *deserialized_gs;
				}  else {
					Log("Failed to load from saved gamestate: %.*s\n", S8VArg(error));
				}
			}
		}
		// statistics @Place(devtools drawing developer menu drawing)
		if (show_devtools)
			PROFILE_SCOPE("devtools drawing")
			{
				Vec2 depth_size = V2(200.0f, 200.0f);
				draw_quad((DrawParams){quad_at(V2(screen_size().x - depth_size.x, screen_size().y), depth_size), IMG(state.shadows.color_img), WHITE, .layer = LAYER_UI_FG});
				draw_quad((DrawParams){quad_at(V2(0.0, screen_size().y / 2.0f), MulV2F(screen_size(), 0.1f)), IMG(state.outline_pass_resolve_image), WHITE, .layer = LAYER_UI_FG});

				Vec3 view_cam_pos = MulM4V4(InvGeneralM4(view), V4(0, 0, 0, 1)).xyz;
				// if(view_cam_pos.y >= 4.900f) // causes nan if not true... not good...
				if (true)
				{
					Vec3 world_mouse = screenspace_point_to_camera_point(mouse_pos, view);
					Vec3 mouse_ray = NormV3(SubV3(world_mouse, view_cam_pos));
					Vec3 marker = ray_intersect_plane(view_cam_pos, mouse_ray, V3(0, 0, 0), V3(0, 1, 0));
					Vec2 mouse_on_floor = point_plane(marker);
					Overlapping mouse_over = get_overlapping(aabb_centered(mouse_on_floor, V2(1, 1)));
					BUFF_ITER(Entity *, &mouse_over)
					{
						dbgcol(PINK)
						{
							dbgplanerect(entity_aabb(*it));

							// debug draw memories of hovered
							Entity *to_view = *it;
							Vec2 start_at = V2(0, 300);
							Vec2 cur_pos = start_at;

							AABB bounds = draw_text((TextParams){false, S8Fmt(frame_arena, "--Memories for %.*s--", TextChunkVArg(npc_data(&gs, to_view->npc_kind)->name)), cur_pos, WHITE, 1.0});
							cur_pos.y -= aabb_size(bounds).y;

							if (keypressed[SAPP_KEYCODE_Q] && !receiving_text_input)
							{
								Log("\n\n==========------- Printing debugging information for %.*s -------==========\n", TextChunkVArg(npc_data(&gs, to_view->npc_kind)->name));
								Log("\nMemories-----------------------------\n");
								Log("\nPrompt-----------------------------\n");
								Log("UNIMPLEMENTED!")
								// String8 prompt = generate_chatgpt_prompt(frame_arena, &gs, to_view, get_can_talk_to(to_view));
								// printf("%.*s\n", S8VArg(prompt));
							}
						}
					}
				}

				Vec2 pos = V2(0.0, screen_size().Y);
				int num_entities = 0;
				ENTITIES_ITER(gs.entities)
				num_entities++;
				String8 stats =
					tprint("Frametime: %.1f ms\n"
						   "Processing: %.1f ms\n"
						   "Gameplay processing: %.1f ms\n"
						   "Entities: %d\n"
						   "Draw calls: %d\n"
						   "Drawn Vertices: %d\n"
						   "Profiling: %s\n"
						   "Number gameplay processing loops: %d\n"
						   "Flycam: %s\n"
						   "Player position: %f %f\n",
						   dt * 1000.0,
						   last_frame_processing_time * 1000.0,
						   last_frame_gameplay_processing_time * 1000.0,
						   num_entities,
						   num_draw_calls,
						   num_vertices,
						   profiling ? "yes" : "no",
						   num_timestep_loops,
						   flycam ? "yes" : "no",
						   v2varg((player(&gs) ? player(&gs)->pos : V2(0, 0))));
				AABB bounds = draw_text((TextParams){true, stats, pos, BLACK, 1.0f});
				pos.Y -= bounds.upper_left.Y - screen_size().Y;
				bounds = draw_text((TextParams){true, stats, pos, BLACK, 1.0f});

				// background panel
				colorquad(quad_aabb(bounds), (Color){1.0, 1.0, 1.0, 0.3f});
				draw_text((TextParams){false, stats, pos, BLACK, 1.0f});
				num_draw_calls = 0;
				num_vertices = 0;
			}
#endif // devtools

		// @Place(actually render 2d)
		PROFILE_SCOPE("flush rendering")
		{
			ARR_ITER_I(RenderingQueue, rendering_queues, i)
			{
				RenderingQueue *rendering_queue = it;
				qsort(&rendering_queue->data[0], rendering_queue->cur_index, sizeof(rendering_queue->data[0]), rendering_compare);

				BUFF_ITER(DrawParams, rendering_queue)
				{
					DrawParams d = *it;
					PROFILE_SCOPE("Draw quad")
					{
						Vec2 *points = d.quad.points;
						threedee_twodee_fs_params_t params = {
							.tint = d.tint,
							.time = (float)fmod(elapsed_time, 100),
						};
						params.alpha_clip_threshold = d.alpha_clip_threshold;
						if (d.do_clipping)
						{
							Vec2 aabb_clip_ul = into_clip_space(d.clip_to.upper_left);
							Vec2 aabb_clip_lr = into_clip_space(d.clip_to.lower_right);
							params.clip_ul = aabb_clip_ul;
							params.clip_lr = aabb_clip_lr;
						}
						else
						{
							params.clip_ul = V2(-1.0, 1.0);
							params.clip_lr = V2(1.0, -1.0);
						}
						// if the rendering call is different, and the batch must be flushed
						if (d.image.id != cur_batch_image.id || memcmp(&params, &cur_batch_params, sizeof(params)) != 0 || d.custom_pipeline.id != cur_batch_pipeline.id)
						{
							flush_quad_batch();
							cur_batch_image = d.image;
							cur_batch_params = params;
							cur_batch_pipeline = d.custom_pipeline;
						}

						float new_vertices[FLOATS_PER_VERTEX * 4] = {0};
						Vec2 region_size = SubV2(d.image_region.lower_right, d.image_region.upper_left);

						// the region size can be negative if the image is desired to be flipped
						// assert(region_size.X > 0.0);
						// assert(region_size.Y > 0.0);

						Vec2 tex_coords[4] =
							{
								// upper left vertex, upper right vertex, lower right vertex, lower left vertex

								/*
								AddV2(lower_left, V2(0.0,           region_size.y)),
								AddV2(lower_left, V2(region_size.x, region_size.y)),
								AddV2(lower_left, V2(region_size.x, 0.0          )),
								AddV2(lower_left, V2(0.0          , 0.0          )),
								*/

								// This flips the image

								AddV2(d.image_region.upper_left, V2(0.0, region_size.Y)),
								AddV2(d.image_region.upper_left, V2(region_size.X, region_size.Y)),
								AddV2(d.image_region.upper_left, V2(region_size.X, 0.0)),
								AddV2(d.image_region.upper_left, V2(0.0, 0.0)),

								/*
								AddV2(d.image_region.upper_left, V2(region_size.X, region_size.Y)),
								AddV2(d.image_region.upper_left, V2(0.0,           region_size.Y)),
								AddV2(d.image_region.upper_left, V2(0.0,                     0.0)),
								AddV2(d.image_region.upper_left, V2(region_size.X,           0.0)),
								*/
							};

						// convert to uv space
						sg_image_desc desc = sg_query_image_desc(d.image);
						for (int i = 0; i < 4; i++)
						{
							tex_coords[i] = DivV2(tex_coords[i], V2((float)desc.width, (float)desc.height));
						}
						for (int i = 0; i < 4; i++)
						{
							Vec2 in_clip_space = into_clip_space(points[i]);
							new_vertices[i * FLOATS_PER_VERTEX + 0] = in_clip_space.X;
							new_vertices[i * FLOATS_PER_VERTEX + 1] = in_clip_space.Y;
							// update Y_COORD_IN_BACK, Y_COORD_IN_FRONT when this changes
							/*
								 float unmapped = (clampf(d.y_coord_sorting, -1.0f, 2.0f));
								 float mapped = (unmapped + 1.0f)/3.0f;
								 new_vertices[i*FLOATS_PER_VERTEX + 2] = 1.0f - (float)clamp(mapped, 0.0, 1.0);
								 */
							new_vertices[i * FLOATS_PER_VERTEX + 2] = 0.0f;
							new_vertices[i * FLOATS_PER_VERTEX + 3] = tex_coords[i].X;
							new_vertices[i * FLOATS_PER_VERTEX + 4] = tex_coords[i].Y;
						}

						// two triangles drawn, six vertices
						size_t total_size = 6 * FLOATS_PER_VERTEX;

						// batched a little too close to the sun
						if (cur_batch_data_index + total_size >= ARRLEN(cur_batch_data))
						{
							flush_quad_batch();
							cur_batch_image = d.image;
							cur_batch_params = params;
						}

#define PUSH_VERTEX(vert)                                                                        \
	{                                                                                            \
		memcpy(&cur_batch_data[cur_batch_data_index], &vert, FLOATS_PER_VERTEX * sizeof(float)); \
		cur_batch_data_index += FLOATS_PER_VERTEX;                                               \
	}
						PUSH_VERTEX(new_vertices[0 * FLOATS_PER_VERTEX]);
						PUSH_VERTEX(new_vertices[1 * FLOATS_PER_VERTEX]);
						PUSH_VERTEX(new_vertices[2 * FLOATS_PER_VERTEX]);
						PUSH_VERTEX(new_vertices[0 * FLOATS_PER_VERTEX]);
						PUSH_VERTEX(new_vertices[2 * FLOATS_PER_VERTEX]);
						PUSH_VERTEX(new_vertices[3 * FLOATS_PER_VERTEX]);
#undef PUSH_VERTEX
					}
				}
				BUFF_CLEAR(rendering_queue);
			}

			// end of rendering
			flush_quad_batch();
			sg_end_pass();
			sg_commit();
		}

		last_frame_processing_time = stm_sec(stm_diff(stm_now(), time_start_frame));

		ArenaClear(frame_arena);

		memset(keypressed, 0, sizeof(keypressed));
		pressed = (PressedState){0};
		mouse_movement = V2(0, 0);
	}
}

void cleanup(void)
{
#ifdef DESKTOP
	for (ChatRequest *cur = requests_first; cur; cur = cur->next)
	{
		cur->should_close = true;
	}
#endif

	ArenaRelease(frame_arena);

	// Don't free the persistent arena because threads still access their ChatRequest should_close fieldon shutdown,
	// and ChatRequest is allocated on the persistent arena. We just shamelessly leak this memory. Cowabunga!
	// ArenaRelease(persistent_arena);
	sg_shutdown();
	cleanup_immediate_state();
	Log("Cleaning up\n");
}

void event(const sapp_event *e)
{
#ifdef DESKTOP
	// the desktop text backend, for debugging purposes
	if (receiving_text_input)
	{
		if (e->type == SAPP_EVENTTYPE_KEY_DOWN && e->key_code == SAPP_KEYCODE_BACKSPACE)
		{
			if (text_input.text_length > 0)
				text_input.text_length -= 1;
		}
		else
		{
			if (e->type == SAPP_EVENTTYPE_CHAR)
			{
				if (text_input.text_length < ARRLEN(text_input.text))
				{
					text_input.text[text_input.text_length] = (char)e->char_code;
					text_input.text_length += 1;
				}
			}
		}

		if (e->type == SAPP_EVENTTYPE_KEY_DOWN && e->key_code == SAPP_KEYCODE_ENTER)
		{
			end_text_input((char *)nullterm(frame_arena, TextChunkString8(text_input)).str);
		}
	}
#endif

	if (e->key_repeat)
		return;

	if (e->type == SAPP_EVENTTYPE_RESIZED)
	{
		create_screenspace_gfx_state();
	}
	if (e->type == SAPP_EVENTTYPE_TOUCHES_BEGAN)
	{
		if (!mobile_controls)
		{
			thumbstick_base_pos = V2(screen_size().x * 0.25f, screen_size().y * 0.25f);
			thumbstick_nub_pos = thumbstick_base_pos;
		}
		mobile_controls = true;
	}

	if (e->type == SAPP_EVENTTYPE_KEY_DOWN &&
		(e->key_code == SAPP_KEYCODE_F11 ||
		 e->key_code == SAPP_KEYCODE_ENTER && ((e->modifiers & SAPP_MODIFIER_ALT) || (e->modifiers & SAPP_MODIFIER_SHIFT))))
	{
#ifdef DESKTOP
		sapp_toggle_fullscreen();
#else
		EM_ASM({
			var elem = document.documentElement;
			if (document.fullscreenElement || document.webkitFullscreenElement || document.mozFullScreenElement || document.msFullscreenElement)
			{
				if (document.exitFullscreen)
					document.exitFullscreen();
				else if (document.webkitExitFullscreen)
					document.webkitExitFullscreen();
				else if (document.mozCancelFullScreen)
					document.mozCancelFullScreen();
				else if (document.msExitFullscreen)
					document.msExitFullscreen();
			}
			else
			{
				if (elem.requestFullscreen)
					elem.requestFullscreen();
				else if (elem.webkitRequestFullscreen)
					elem.webkitRequestFullscreen();
				else if (elem.mozRequestFullScreen)
					elem.mozRequestFullScreen();
				else if (elem.msRequestFullscreen)
					elem.msRequestFullscreen();
			}
		});
#endif
	}

#ifdef DEVTOOLS
	if (!receiving_text_input && e->type == SAPP_EVENTTYPE_KEY_DOWN && e->key_code == SAPP_KEYCODE_F)
	{
		flycam = !flycam;
		sapp_lock_mouse(flycam);
	}

	if (flycam)
	{
		if (e->type == SAPP_EVENTTYPE_MOUSE_MOVE && !mouse_frozen)
		{
			const float rotation_speed = 0.001f;
			flycam_horizontal_rotation -= e->mouse_dx * rotation_speed;
			flycam_vertical_rotation -= e->mouse_dy * rotation_speed;
			flycam_vertical_rotation = clampf(flycam_vertical_rotation, -PI32 / 2.0f + 0.01f, PI32 / 2.0f - 0.01f);
		}
		else if (e->type == SAPP_EVENTTYPE_MOUSE_SCROLL)
		{
			flycam_speed *= 1.0f + 0.1f * e->scroll_y;
		}
	}

#endif

	// mobile handling touch controls handling touch input
	if (mobile_controls)
	{
		if (e->type == SAPP_EVENTTYPE_TOUCHES_BEGAN)
		{
#define TOUCHPOINT_SCREEN(point) V2(point.pos_x, screen_size().y - point.pos_y)
			for (int i = 0; i < e->num_touches; i++)
			{
				sapp_touchpoint point = e->touches[i];
				Vec2 touchpoint_screen_pos = TOUCHPOINT_SCREEN(point);
				if (touchpoint_screen_pos.x < screen_size().x * 0.4f)
				{
					if (!movement_touch.active)
					{
						// if(LenV2(SubV2(touchpoint_screen_pos, thumbstick_base_pos)) > 1.25f * thumbstick_base_size())
						if (true)
						{
							thumbstick_base_pos = touchpoint_screen_pos;
						}
						movement_touch = activate(point.identifier);
						thumbstick_nub_pos = thumbstick_base_pos;
					}
				}
				if (LenV2(SubV2(touchpoint_screen_pos, roll_button_pos())) < mobile_button_size() * 0.5f)
				{
					roll_pressed_by = activate(point.identifier);
					mobile_roll_pressed = true;
				}
				if (LenV2(SubV2(touchpoint_screen_pos, interact_button_pos())) < mobile_button_size() * 0.5f)
				{
					interact_pressed_by = activate(point.identifier);
					mobile_interact_pressed = true;
					pressed.interact = true;
				}
				if (LenV2(SubV2(touchpoint_screen_pos, attack_button_pos())) < mobile_button_size() * 0.5f)
				{
					attack_pressed_by = activate(point.identifier);
					mobile_attack_pressed = true;
				}
			}
		}
		if (e->type == SAPP_EVENTTYPE_TOUCHES_MOVED)
		{
			for (int i = 0; i < e->num_touches; i++)
			{
				if (movement_touch.active)
				{
					if (e->touches[i].identifier == movement_touch.identifier)
					{
						thumbstick_nub_pos = TOUCHPOINT_SCREEN(e->touches[i]);
						Vec2 move_vec = SubV2(thumbstick_nub_pos, thumbstick_base_pos);
						float clampto_size = thumbstick_base_size() / 2.0f;
						if (LenV2(move_vec) > clampto_size)
						{
							thumbstick_nub_pos = AddV2(thumbstick_base_pos, MulV2F(NormV2(move_vec), clampto_size));
						}
					}
				}
			}
		}
		if (e->type == SAPP_EVENTTYPE_TOUCHES_ENDED)
		{
			for (int i = 0; i < e->num_touches; i++)
				if (e->touches[i].changed) // only some of the touch events are released
				{
					if (maybe_deactivate(&interact_pressed_by, e->touches[i].identifier))
					{
						mobile_interact_pressed = false;
					}
					if (maybe_deactivate(&roll_pressed_by, e->touches[i].identifier))
					{
						mobile_roll_pressed = false;
					}
					if (maybe_deactivate(&attack_pressed_by, e->touches[i].identifier))
					{
						mobile_attack_pressed = false;
					}
					if (maybe_deactivate(&movement_touch, e->touches[i].identifier))
					{
						thumbstick_nub_pos = thumbstick_base_pos;
					}
				}
		}
	}

	if (e->type == SAPP_EVENTTYPE_MOUSE_DOWN)
	{
		if (e->mouse_button == SAPP_MOUSEBUTTON_LEFT)
		{
			pressed.mouse_down = true;
			mouse_down = true;
		}
	}

	if (e->type == SAPP_EVENTTYPE_MOUSE_UP)
	{
		if (e->mouse_button == SAPP_MOUSEBUTTON_LEFT)
		{
			mouse_down = false;
			pressed.mouse_up = true;
		}
	}

	if (e->type == SAPP_EVENTTYPE_KEY_DOWN)
#ifdef DESKTOP
		if (!receiving_text_input)
#endif
		{
			mobile_controls = false;
			assert(e->key_code < sizeof(keydown) / sizeof(*keydown));
			keydown[e->key_code] = true;
			keypressed[e->key_code] = true;

			if (e->key_code == SAPP_KEYCODE_E)
			{
				pressed.interact = true;
			}

			if (e->key_code == SAPP_KEYCODE_LEFT_SHIFT)
			{
				learned_shift += 0.15f;
			}
			if (e->key_code == SAPP_KEYCODE_SPACE)
			{
				learned_space += 0.15f;
			}
			if (e->key_code == SAPP_KEYCODE_E)
			{
				learned_e += 0.15f;
			}
#ifdef DESKTOP // very nice for my run from cmdline workflow, escape to quit
			if (e->key_code == SAPP_KEYCODE_ESCAPE)
			{
				sapp_quit();
			}
#endif
#ifdef DEVTOOLS
			if (e->key_code == SAPP_KEYCODE_T)
			{
				mouse_frozen = !mouse_frozen;
			}
			if (e->key_code == SAPP_KEYCODE_9)
			{
				gs.won = true;
			}
			if (e->key_code == SAPP_KEYCODE_P)
			{
				profiling = !profiling;
				if (profiling)
				{
					init_profiling(PROFILING_SAVE_FILENAME);
					init_profiling_mythread(0);
				}
				else
				{
					end_profiling_mythread();
					end_profiling();
				}
			}
			if (e->key_code == SAPP_KEYCODE_7)
			{
				show_devtools = !show_devtools;
			}
#endif
		}
	if (e->type == SAPP_EVENTTYPE_KEY_UP)
	{
		keydown[e->key_code] = false;
	}
	if (e->type == SAPP_EVENTTYPE_MOUSE_MOVE)
	{
		mouse_movement.x += e->mouse_dx;
		mouse_movement.y -= e->mouse_dy;
		bool ignore_movement = false;
#ifdef DEVTOOLS
		if (mouse_frozen)
			ignore_movement = true;
#endif
		if (!ignore_movement)
			mouse_pos = V2(e->mouse_x, (float)sapp_height() - e->mouse_y);
	}
}

sapp_desc sokol_main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	return (sapp_desc){
		.init_cb = init,
		.frame_cb = frame,
		.cleanup_cb = cleanup,
		.event_cb = event,
		.sample_count = 1,
		.width = 800,
		.height = 600,
		.window_title = "Dante's Cowboy",
		.win32_console_attach = true,
		.win32_console_create = true,
		.icon.sokol_default = true,
		.logger.func = slog_func,
	};
}
