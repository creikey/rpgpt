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
#endif

#if defined(__EMSCRIPTEN__)
#define WEB
#define SOKOL_GLES2
#endif

#ifdef WINDOWS


#pragma warning(push, 3)
#include <Windows.h>
#include <processthreadsapi.h>
#include <dbghelp.h>
#include <stdint.h>

// https://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
// Tells nvidia to use dedicated graphics card if it can on laptops that also have integrated graphics
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
// Vice versa for AMD but I don't have the docs link on me at the moment
__declspec(dllexport) uint32_t AmdPowerXpressRequestHighPerformance = 0x00000001;

#endif

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x

#include "buff.h"
#include "sokol_app.h"
#pragma warning(push)
#pragma warning(disable : 4191) // unsafe function calling
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
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include "HandmadeMath.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#pragma warning(pop)

// web compatible metadesk
#ifdef WEB
#define __gnu_linux__
#define i386
#define MD_DEFAULT_ARENA 0

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
	assert(arena->pos < arena->cap);
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

#define MD_IMPL_Arena WebArena
#define MD_IMPL_ArenaAlloc web_arena_alloc
#define MD_IMPL_ArenaRelease web_arena_release
#define MD_IMPL_ArenaGetPos web_arena_get_pos
#define MD_IMPL_ArenaPush web_arena_push
#define MD_IMPL_ArenaPopTo web_arena_pop_to
#define MD_IMPL_ArenaSetAutoAlign web_arena_set_auto_align
#define MD_IMPL_ArenaMinPos 64 // no idea what this is honestly
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
#define MD_FUNCTION no_ubsan
#include "md.h"
#include "md.c"
#pragma warning(pop)

#include "ser.h"

#include <math.h>

#ifdef DEVTOOLS
#ifdef DESKTOP
#define PROFILING
#define PROFILING_IMPL
#endif
#endif
#include "profiling.h"

MD_String8 nullterm(MD_Arena *copy_onto, MD_String8 to_nullterm)
{
	MD_String8 to_return = {0};
	to_return.str = MD_PushArray(copy_onto, MD_u8, to_nullterm.size + 1);
	to_return.size = to_nullterm.size + 1;
	to_return.str[to_return.size - 1] = '\0';
	memcpy(to_return.str, to_nullterm.str, to_nullterm.size);
	return to_return;
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

#include "makeprompt.h"
typedef BUFF(Entity*, 16) Overlapping;

// no alignment etc because lazy
typedef struct Arena
{
	char *data;
	size_t data_size;
	size_t cur;
} Arena;

typedef struct AudioSample
{
	float *pcm_data; // allocated by loader, must be freed
	uint64_t pcm_data_length;
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
	unsigned int channels;
	unsigned int sampleRate;
	AudioSample to_return = { 0 };
	to_return.pcm_data = drwav_open_file_and_read_pcm_frames_f32(path, &channels, &sampleRate, &to_return.pcm_data_length, 0);
	assert(channels == 1);
	assert(sampleRate == SAMPLE_RATE);
	return to_return;
}

uint64_t cursor_pcm(AudioPlayer *p)
{
	return (uint64_t)(p->cursor_time * SAMPLE_RATE);
}
float float_rand(float min, float max)
{
	float scale = rand() / (float) RAND_MAX; /* [0, 1.0] */
	return min + scale * (max - min); /* [min, max] */
}
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

typedef struct {
	bool open;
	bool for_giving;
} ItemgridState;
ItemgridState item_grid_state = {0};




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
MD_u8 text_input_buffer[MAX_SENTENCE_LENGTH] = {0};
int text_input_buffer_length = 0;
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

void begin_text_input()
{
	receiving_text_input = true;
#ifdef DESKTOP
	text_input_buffer_length = 0;
#endif
}


Vec2 FloorV2(Vec2 v)
{
	return V2(floorf(v.x), floorf(v.y));
}

MD_Arena *frame_arena = 0;
MD_Arena *persistent_arena = 0; // watch out, arenas have limited size. 

#ifdef WINDOWS
// uses frame arena
LPCWSTR windows_string(MD_String8 s)
{
	int num_characters = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s.str, (int)s.size, 0, 0);
	wchar_t *to_return = MD_PushArray(frame_arena, wchar_t, num_characters + 1); // also allocate for null terminating character
	assert(MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s.str, (int)s.size, to_return, num_characters) == num_characters);
	to_return[num_characters] = '\0';
	return to_return;
}
#endif

#ifdef DESKTOP
#ifdef WINDOWS
#pragma warning(push, 3)
#include <WinHttp.h>
#include <process.h>
#pragma warning(pop)

typedef struct ChatRequest
{
	struct ChatRequest *next;
	struct ChatRequest *prev;
	bool should_close;
	int id;
	int status;
	TextChunk generated;
	uintptr_t thread_handle;
	MD_Arena *arena;
	MD_String8 post_req_body; // allocated on thread_arena
} ChatRequest;

ChatRequest *requests_first = 0;
ChatRequest *requests_last = 0;

int next_request_id = 1;
ChatRequest *requests_free_list = 0;

void generation_thread(void* my_request_voidptr)
{
	ChatRequest *my_request = (ChatRequest*)my_request_voidptr;

	bool succeeded = true;

#define WinAssertWithErrorCode(X) if( !( X ) ) { unsigned int error = GetLastError(); Log("Error %u in %s\n", error, #X); my_request->status = 2; return; }

	HINTERNET hSession = WinHttpOpen(L"PlayGPT winhttp backend", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	WinAssertWithErrorCode(hSession);

	LPCWSTR windows_server_name = windows_string(MD_S8Lit(SERVER_DOMAIN));
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
		my_request->status = 2;
	}
	if(succeeded)
	{
		WinAssertWithErrorCode(WinHttpReceiveResponse(hRequest, 0));

		DWORD status_code;
		DWORD status_code_size = sizeof(status_code);
		WinAssertWithErrorCode(WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_code_size, WINHTTP_NO_HEADER_INDEX));
		Log("Status code: %lu\n", status_code);

		DWORD dwSize = 0;
		MD_String8List received_data_list = {0};
		do
		{
			dwSize = 0;
			WinAssertWithErrorCode(WinHttpQueryDataAvailable(hRequest, &dwSize));

			if(dwSize == 0)
			{
				Log("Didn't get anything back.\n");
			}
			else
			{
				MD_u8* out_buffer = MD_PushArray(my_request->arena, MD_u8, dwSize + 1);
				DWORD dwDownloaded = 0;
				WinAssertWithErrorCode(WinHttpReadData(hRequest, (LPVOID)out_buffer, dwSize, &dwDownloaded));
				out_buffer[dwDownloaded - 1] = '\0';
				Log("Got this from http, size %lu: %s\n", dwDownloaded, out_buffer);
				MD_S8ListPush(my_request->arena, &received_data_list, MD_S8(out_buffer, dwDownloaded)); 
			}
		} while (dwSize > 0);
		MD_String8 received_data = MD_S8ListJoin(my_request->arena, received_data_list, &(MD_StringJoin){0});

		MD_String8 ai_response = MD_S8Substring(received_data, 1, received_data.size);
		if(ai_response.size > ARRLEN(my_request->generated.text))
		{
			Log("%lld too big for %lld\n", ai_response.size, ARRLEN(my_request->generated.text));
			my_request->status = 2;
			return;
		}
		chunk_from_s8(&my_request->generated, ai_response);
		my_request->status = 1;
	}
}

int make_generation_request(MD_String8 post_req_body)
{
	ChatRequest *to_return = 0;
	if(requests_free_list)
	{
		to_return = requests_free_list;
		requests_free_list = requests_free_list->next;
		//MD_StackPop(requests_free_list);
		*to_return = (ChatRequest){0};
	}
	else
	{
		to_return = MD_PushArrayZero(persistent_arena, ChatRequest, 1);
	}
	to_return->arena = MD_ArenaAlloc();
	to_return->id = next_request_id;
	next_request_id += 1;

	to_return->post_req_body.str = MD_PushArrayZero(to_return->arena, MD_u8, post_req_body.size);
	to_return->post_req_body.size = post_req_body.size;
	memcpy(to_return->post_req_body.str, post_req_body.str, post_req_body.size);

	to_return->thread_handle = _beginthread(generation_thread, 0, to_return);
	assert(to_return->thread_handle);

	MD_DblPushBack(requests_first, requests_last, to_return);

	return to_return->id;
}

// should never return null
// @TODO @IMPORTANT this doesn't work with save games because it assumes the id is always
// valid but saved IDs won't be valid on reboot
ChatRequest *get_by_id(int id)
{
	for(ChatRequest *cur = requests_first; cur; cur = cur->next)
	{
		if(cur->id == id)
		{
			return cur;
		}
	}
	assert(false);
	return 0;
}

void done_with_request(int id)
{
	ChatRequest *req = get_by_id(id);
	MD_ArenaRelease(req->arena);
	MD_DblRemove(requests_first, requests_last, req);
	MD_StackPush(requests_free_list, req);
}

#else
ISANERROR("Only know how to do desktop http requests on windows")
#endif // WINDOWS
#endif // DESKTOP

Memory *memories_free_list = 0;

TextChunkList *text_chunk_free_list = 0;

// s.size must be less than MAX_SENTENCE_LENGTH, or assert fails
void into_chunk(TextChunk *t, MD_String8 s)
{
	assert(s.size < MAX_SENTENCE_LENGTH);
	memcpy(t->text, s.str, s.size);
	t->text_length = (int)s.size;
}
TextChunkList *allocate_text_chunk(MD_Arena *arena)
{
	TextChunkList *to_return = 0;
	if(text_chunk_free_list)
	{
		to_return = text_chunk_free_list;
		MD_StackPop(text_chunk_free_list);
	}
	else
	{
		to_return = MD_PushArray(arena, TextChunkList, 1);
	}
	*to_return = (TextChunkList){0};
	return to_return;
}
void remove_text_chunk_from(TextChunkList **first, TextChunkList **last, TextChunkList *chunk)
{
	MD_DblRemove(*first, *last, chunk);
	MD_StackPush(text_chunk_free_list, chunk);
}
int text_chunk_list_count(TextChunkList *first)
{
	int ret = 0;
	for(TextChunkList *cur = first; cur != 0; cur = cur->next)
	{
		ret++;
	}
	return ret;
}
void append_to_errors(Entity *from, MD_String8 s)
{
	TextChunkList *error_chunk = allocate_text_chunk(persistent_arena);
	into_chunk(&error_chunk->text, s);
	while(text_chunk_list_count(from->errorlist_first) > REMEMBERED_ERRORS)
	{
		remove_text_chunk_from(&from->errorlist_first, &from->errorlist_last, from->errorlist_first);
	}
	MD_DblPushBack(from->errorlist_first, from->errorlist_last, error_chunk);
	from->perceptions_dirty = true;
}

MD_String8 tprint(char *format, ...)
{
	MD_String8 to_return = {0};
	va_list argptr;
	va_start(argptr, format);

	to_return = MD_S8FmtV(frame_arena, format, argptr);

	va_end(argptr);
	return to_return;
}

bool V2ApproxEq(Vec2 a, Vec2 b)
{
	return LenV2(SubV2(a, b)) <= 0.01f;
}

float max_coord(Vec2 v)
{
	return v.x > v.y ? v.x : v.y;
}

// aabb advice by iRadEntertainment
Vec2 entity_aabb_size(Entity *e)
{
	if (e->is_character)
	{
		return V2(1.0f*0.9f, 1.0f*0.5f);
	}
	else if (e->is_npc)
	{
		return V2(1,1);
	}
	else
	{
		assert(false);
		return (Vec2) { 0 };
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

typedef struct LoadedImage
{
	struct LoadedImage *next;
	MD_String8 name;
	sg_image image;
} LoadedImage;

LoadedImage *loaded_images = 0;

sg_image load_image(MD_String8 path)
{
	MD_ArenaTemp scratch = MD_GetScratch(0, 0);
	for(LoadedImage *cur = loaded_images; cur; cur = cur->next)
	{
		if(MD_S8Match(cur->name, path, 0))
		{
			return cur->image;
		}
	}

	LoadedImage *loaded = MD_PushArray(persistent_arena, LoadedImage, 1);
	loaded->name = MD_S8Copy(persistent_arena, path);
	MD_StackPush(loaded_images, loaded);

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
		stbi_uc *old_pixels = pixels;
		pixels = MD_ArenaPush(scratch.arena, png_width * png_height * 4 * sizeof(stbi_uc));
		for(MD_u64 pixel_i = 0; pixel_i < png_width * png_height; pixel_i++)
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

	assert(pixels);
	assert(desired_channels == num_channels);
	Log("Path %.*s | Loading image with dimensions %d %d\n", MD_S8VArg(path), png_width, png_height);
	to_return = sg_make_image(&(sg_image_desc)
			{
			.width = png_width,
			.height = png_height,
			.pixel_format = SG_PIXELFORMAT_RGBA8,
			.min_filter = SG_FILTER_LINEAR,
			.num_mipmaps = 1,
			.wrap_u = SG_WRAP_CLAMP_TO_EDGE,
			.wrap_v = SG_WRAP_CLAMP_TO_EDGE,
			.mag_filter = SG_FILTER_LINEAR,
			.data.subimage[0][0] =
			{
			.ptr = pixels,
			.size = (size_t)(png_width * png_height * num_channels),
			}
			});
	loaded->image = to_return;
	MD_ReleaseScratch(scratch);
	return to_return;
}

SER_MAKE_FOR_TYPE(uint64_t);
SER_MAKE_FOR_TYPE(bool);
SER_MAKE_FOR_TYPE(double);
SER_MAKE_FOR_TYPE(float);
SER_MAKE_FOR_TYPE(PropKind);
SER_MAKE_FOR_TYPE(NpcKind);
SER_MAKE_FOR_TYPE(CharacterState);
SER_MAKE_FOR_TYPE(Memory);
SER_MAKE_FOR_TYPE(Vec2);
SER_MAKE_FOR_TYPE(Vec3);
SER_MAKE_FOR_TYPE(EntityRef);
SER_MAKE_FOR_TYPE(NPCPlayerStanding);
SER_MAKE_FOR_TYPE(MD_u16);

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
	MD_u16  joint_indices[4];
	float joint_weights[4];
} ArmatureVertex;

SER_MAKE_FOR_TYPE(Vertex);

typedef struct Mesh
{
	struct Mesh *next;

	Vertex *vertices;
	MD_u64 num_vertices;

	sg_buffer loaded_buffer;
	sg_image image;
	MD_String8 name;
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
	float length;
} Bone;

typedef struct AnimationTrack
{
	PoseBone *poses;
	MD_u64 poses_length;
} AnimationTrack;

typedef struct Animation
{
	MD_String8 name;
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
	MD_String8 name;
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
Mesh load_mesh(MD_Arena *arena, MD_String8 binary_file, MD_String8 mesh_name)
{
	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);
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

	MD_String8 image_filename;
	ser_MD_String8(&ser, &image_filename, scratch.arena);
	to_return.image = load_image(MD_S8Fmt(scratch.arena, "assets/exported_3d/%.*s", MD_S8VArg(image_filename)));

	ser_MD_u64(&ser, &to_return.num_vertices);
	//Log("Mesh %.*s has %llu vertices and image filename '%.*s'\n", MD_S8VArg(mesh_name), to_return.num_vertices, MD_S8VArg(image_filename));

	to_return.vertices = MD_ArenaPush(arena, sizeof(*to_return.vertices) * to_return.num_vertices);
	for(MD_u64 i = 0; i < to_return.num_vertices; i++)
	{
		ser_Vertex(&ser, &to_return.vertices[i]);
	}

	assert(!ser.cur_error.failed);
	MD_ReleaseScratch(scratch);

	to_return.loaded_buffer = sg_make_buffer(&(sg_buffer_desc)
			{
			.usage = SG_USAGE_IMMUTABLE,
			.data = (sg_range){.ptr = to_return.vertices, .size = to_return.num_vertices * sizeof(Vertex)},
			.label = (const char*)nullterm(arena, MD_S8Fmt(arena, "%.*s-vertices", MD_S8VArg(mesh_name))).str,
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
Transform default_transform()
{
	return (Transform){.rotation = Make_Q(0,0,0,1)};
}

typedef struct
{
	MD_String8 name;

	Bone *bones;
	MD_u64 bones_length;

	Animation *animations;
	MD_u64 animations_length;

	// when set, blends to that animation next time this armature is processed for that
	MD_String8 go_to_animation;
	bool next_animation_isnt_looping;

	Transform *current_poses; // allocated on loading of the armature
	MD_String8 currently_playing_animation; // CANNOT be null.
	bool currently_playing_isnt_looping;
	float animation_blend_t; // [0,1] how much between current_animation and target_animation. Once >= 1, current = target and target = null.
	double cur_animation_time; // used for non looping animations to play once

	Transform *anim_blended_poses; // recalculated once per frame depending on above parameters, which at the same code location are calculated. Is `bones_length` long

	ArmatureVertex *vertices;
	MD_u64 vertices_length;
	sg_buffer loaded_buffer;

	sg_image bones_texture;
	sg_image image;
	int bones_texture_width;
	int bones_texture_height;
} Armature;

// armature_name is used for debugging purposes, it has to effect on things
Armature load_armature(MD_Arena *arena, MD_String8 binary_file, MD_String8 armature_name)
{
	assert(binary_file.str);
	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);
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

	MD_String8 image_filename;
	ser_MD_String8(&ser, &image_filename, scratch.arena);
	arena->align = 16; // SSE requires quaternions are 16 byte aligned
	to_return.image = load_image(MD_S8Fmt(scratch.arena, "assets/exported_3d/%.*s", MD_S8VArg(image_filename)));

	ser_MD_u64(&ser, &to_return.bones_length);
	Log("Armature %.*s has %llu bones\n", MD_S8VArg(armature_name), to_return.bones_length);
	to_return.bones = MD_PushArray(arena, Bone, to_return.bones_length);

	for(MD_u64 i = 0; i < to_return.bones_length; i++)
	{
		Bone *next_bone = &to_return.bones[i];

		BlenderMat model_space_pose;
		BlenderMat inverse_model_space_pose;
		MD_i32 parent_index;

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
				ser.cur_error = (SerError){.failed = true, .why = MD_S8Fmt(arena, "Parent index deserialized %d is out of range of the pose bones, which has a size of %llu", parent_index, to_return.bones_length)};
			}
			else
			{
				next_bone->parent = &to_return.bones[parent_index];
			}
		}
	}

	to_return.current_poses = MD_PushArray(arena, Transform, to_return.bones_length);
	to_return.anim_blended_poses = MD_PushArray(arena, Transform, to_return.bones_length);
	for(int i = 0; i < to_return.bones_length; i++)
	{
		to_return.anim_blended_poses[i] = (Transform){.scale = V3(1,1,1), .rotation = Make_Q(1,0,0,1)};
	}

	ser_MD_u64(&ser, &to_return.animations_length);
	Log("Armature %.*s has  %llu animations\n", MD_S8VArg(armature_name), to_return.animations_length);
	to_return.animations = MD_PushArray(arena, Animation, to_return.animations_length);

	for(MD_u64 i = 0; i < to_return.animations_length; i++)
	{
		Animation *new_anim = &to_return.animations[i];
		*new_anim = (Animation){0};

		ser_MD_String8(&ser, &new_anim->name, arena);

		new_anim->tracks = MD_PushArray(arena, AnimationTrack, to_return.bones_length);

		MD_u64 frames_in_anim;
		ser_MD_u64(&ser, &frames_in_anim);
		//Log("There are %llu animation frames in animation '%.*s'\n", frames_in_anim, MD_S8VArg(new_anim->name));

		for(MD_u64 i = 0; i < to_return.bones_length; i++)
		{
			new_anim->tracks[i].poses = MD_PushArray(arena, PoseBone, frames_in_anim);
			new_anim->tracks[i].poses_length = frames_in_anim;
		}

		for(MD_u64 anim_i = 0; anim_i < frames_in_anim; anim_i++)
		{
			float time_through;
			ser_float(&ser, &time_through);
			for(MD_u64 pose_bone_i = 0; pose_bone_i < to_return.bones_length; pose_bone_i++)
			{
				PoseBone *next_pose_bone = &new_anim->tracks[pose_bone_i].poses[anim_i];

				ser_Vec3(&ser, &next_pose_bone->parent_space_pose.offset);
				ser_Quat(&ser, &next_pose_bone->parent_space_pose.rotation);
				ser_Vec3(&ser, &next_pose_bone->parent_space_pose.scale);

				next_pose_bone->time = time_through;
			}
		}
	}

	ser_MD_u64(&ser, &to_return.vertices_length);
	to_return.vertices = MD_PushArray(arena, ArmatureVertex, to_return.vertices_length);
	for(MD_u64 i = 0; i < to_return.vertices_length; i++)
	{
		ser_Vec3(&ser, &to_return.vertices[i].position);
		ser_Vec2(&ser, &to_return.vertices[i].uv);
		MD_u16 joint_indices[4];
		float joint_weights[4];
		for(int ii = 0; ii < 4; ii++)
			ser_MD_u16(&ser, &joint_indices[ii]);
		for(int ii = 0; ii < 4; ii++)
			ser_float(&ser, &joint_weights[ii]);

		for(int ii = 0; ii < 4; ii++)
			to_return.vertices[i].joint_indices[ii] = joint_indices[ii];
		for(int ii = 0; ii < 4; ii++)
			to_return.vertices[i].joint_weights[ii] = joint_weights[ii];
	}
	Log("Armature %.*s has %llu vertices\n", MD_S8VArg(armature_name), to_return.vertices_length);

	assert(!ser.cur_error.failed);
	MD_ReleaseScratch(scratch);

	to_return.loaded_buffer = sg_make_buffer(&(sg_buffer_desc)
			{
			.usage = SG_USAGE_IMMUTABLE,
			.data = (sg_range){.ptr = to_return.vertices, .size = to_return.vertices_length * sizeof(ArmatureVertex)},
			.label = (const char*)nullterm(arena, MD_S8Fmt(arena, "%.*s-vertices", MD_S8VArg(armature_name))).str,
			});

	to_return.bones_texture_width = 16;
	to_return.bones_texture_height = (int)to_return.bones_length;

	Log("Amrature %.*s has bones texture size (%d, %d)\n", MD_S8VArg(armature_name), to_return.bones_texture_width, to_return.bones_texture_height);
	to_return.bones_texture = sg_make_image(&(sg_image_desc) {
		.width = to_return.bones_texture_width,
		.height = to_return.bones_texture_height,
		.pixel_format = SG_PIXELFORMAT_RGBA8,
		.min_filter = SG_FILTER_NEAREST,
		.mag_filter = SG_FILTER_NEAREST,

		// for webgl NPOT texures https://developer.mozilla.org/en-US/docs/Web/API/WebGL_API/Tutorial/Using_textures_in_WebGL
		.wrap_u = SG_WRAP_CLAMP_TO_EDGE,
		.wrap_v = SG_WRAP_CLAMP_TO_EDGE,
		.wrap_w = SG_WRAP_CLAMP_TO_EDGE,

		.usage = SG_USAGE_STREAM,
	});

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

typedef struct CollisionCube
{
	struct CollisionCube *next;
	AABB bounds;
} CollisionCube;

typedef struct
{
	Mesh *mesh_list;
	PlacedMesh *placed_mesh_list;
	CollisionCube *collision_list;
	PlacedEntity *placed_entity_list;
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

ThreeDeeLevel load_level(MD_Arena *arena, MD_String8 binary_file)
{
	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);
	SerState ser = {
		.data = binary_file.str,
		.max = binary_file.size,
		.arena = arena,
		.error_arena = scratch.arena,
		.serializing = false,
	};
	ThreeDeeLevel out = {0};

	// placed meshes
	{
		MD_u64 num_placed = 0;
		ser_MD_u64(&ser, &num_placed);
		arena->align = 16; // SSE requires quaternions are 16 byte aligned
		for(MD_u64 i = 0; i < num_placed; i++)
		{
			PlacedMesh *new_placed = MD_PushArray(arena, PlacedMesh, 1);
			//PlacedMesh *new_placed = calloc(sizeof(PlacedMesh), 1);

			ser_MD_String8(&ser, &new_placed->name, arena);

			BlenderTransform blender_transform = {0};
			ser_BlenderTransform(&ser, &blender_transform);

			new_placed->t = blender_to_game_transform(blender_transform);

			MD_StackPush(out.placed_mesh_list, new_placed);

			//Log("Placed mesh '%.*s' pos %f %f %f rotation %f %f %f %f scale %f %f %f\n", MD_S8VArg(placed_mesh_name), v3varg(new_placed->t.offset), qvarg(new_placed->t.rotation), v3varg(new_placed->t.scale));

			// load the mesh if we haven't already

			bool mesh_found = false;
			for(Mesh *cur = out.mesh_list; cur; cur = cur->next)
			{
				if(MD_S8Match(cur->name, new_placed->name, 0))
				{
					mesh_found = true;
					new_placed->draw_with = cur;
					assert(cur->name.size > 0);
					break;
				}
			}

			if(!mesh_found)
			{
				MD_String8 to_load_filepath = MD_S8Fmt(scratch.arena, "assets/exported_3d/%.*s.bin", MD_S8VArg(new_placed->name));
				//Log("Loading mesh '%.*s'...\n", MD_S8VArg(to_load_filepath));
				MD_String8 binary_mesh_file = MD_LoadEntireFile(scratch.arena, to_load_filepath);
				if(!binary_mesh_file.str)
				{
					ser.cur_error = (SerError){.failed = true, .why = MD_S8Fmt(ser.error_arena, "Couldn't load file '%.*s'", to_load_filepath)};
				}
				else
				{
					Mesh *new_mesh = MD_PushArray(arena, Mesh, 1);
					*new_mesh = load_mesh(arena, binary_mesh_file, new_placed->name);
					MD_StackPush(out.mesh_list, new_mesh);
					new_placed->draw_with = new_mesh;
				}
			}
		}
	}

	MD_u64 num_collision_cubes;
	ser_MD_u64(&ser, &num_collision_cubes);
	for(MD_u64 i = 0; i < num_collision_cubes; i++)
	{
		CollisionCube *new_cube = MD_PushArray(arena, CollisionCube, 1);
		Vec2 twodee_pos;
		Vec2 size;
		ser_Vec2(&ser, &twodee_pos);
		ser_Vec2(&ser, &size);
		new_cube->bounds = aabb_centered(twodee_pos, size);
		MD_StackPush(out.collision_list, new_cube);
	}

	// placed entities
	{
		MD_u64 num_placed = 0;
		ser_MD_u64(&ser, &num_placed);
		arena->align = 16; // SSE requires quaternions are 16 byte aligned
		for(MD_u64 i = 0; i < num_placed; i++)
		{
			PlacedEntity *new_placed = MD_PushArray(arena, PlacedEntity, 1);
			MD_String8 placed_entity_name = {0};
			ser_MD_String8(&ser, &placed_entity_name, scratch.arena);


			bool found = false;
			ARR_ITER_I(CharacterGen, characters, kind)
			{
				if(MD_S8Match(MD_S8CString(it->enum_name), placed_entity_name, 0))
				{
					found = true;
					new_placed->npc_kind = kind;
				}
			}
			BlenderTransform blender_transform = {0};
			ser_BlenderTransform(&ser, &blender_transform);
			if(found)
			{
				new_placed->t = blender_to_game_transform(blender_transform);
				MD_StackPush(out.placed_entity_list, new_placed);
			}
			else
			{
				ser.cur_error = (SerError){.failed = true, .why = MD_S8Fmt(arena, "Couldn't find placed npc kind '%.*s'...\n", MD_S8VArg(placed_entity_name))};
			}

			Log("Loaded placed entity '%.*s' at %f %f %f\n", MD_S8VArg(placed_entity_name), v3varg(new_placed->t.offset));
		}
	}


	assert(!ser.cur_error.failed);
	MD_ReleaseScratch(scratch);

	return out;
}

#include "assets.gen.c"
#include "threedee.glsl.h"

AABB level_aabb = { .upper_left = { 0.0f, 0.0f }, .lower_right = { TILE_SIZE * LEVEL_TILES, -(TILE_SIZE * LEVEL_TILES) } };
GameState gs = { 0 };
bool flycam = false;
Vec3 flycam_pos = {0};
float flycam_horizontal_rotation = 0.0;
float flycam_vertical_rotation = 0.0;
float flycam_speed = 1.0f;
Mat4 view = {0}; 
Mat4 projection = {0};

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
		.up = V3(0,1,0),
	};
	Mat4 rotate_horizontal = Rotate_RH(flycam_horizontal_rotation, V3(0,1,0));
	Mat4 rotate_vertical = Rotate_RH(flycam_vertical_rotation, V3(1,0,0));

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

# define MD_S8LitConst(s)        {(MD_u8 *)(s), sizeof(s)-1}

MD_String8 showing_secret_str = MD_S8LitConst("");
float showing_secret_alpha = 0.0f;

PathCache cached_paths[32] = { 0 };

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
			*it = (PathCache) { 0 };
			it->generation = gen + 1;

			it->path = *path;
			it->elapsed_time = elapsed_time;
			it->exists = true;
			return (PathCacheHandle) { .generation = it->generation, .index = i };
		}
	}
	return (PathCacheHandle) { 0 };
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
	if(e == 0)
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

void push_memory(GameState *gs, Entity *e, Memory new_memory)
{
	Memory *memory_allocated = 0;
	if(memories_free_list)
	{
		memory_allocated = memories_free_list;
		MD_StackPop(memories_free_list);
	}
	else
	{
		memory_allocated = MD_PushArray(persistent_arena, Memory, 1);
	}
	*memory_allocated = new_memory;
	
	int count = 0;
	for(Memory *cur = e->memories_first; cur; cur = cur->next) count += 1;
	while(count >= REMEMBERED_MEMORIES)
	{
		Memory *freed = e->memories_first;
		MD_DblRemove(e->memories_first, e->memories_last, freed);
		MD_StackPush(memories_free_list, freed);
		count -= 1;
	}
	if(gs->stopped_time)
		MD_StackPush(e->memories_added_while_time_stopped, memory_allocated);
	else
		MD_DblPushBack(e->memories_first, e->memories_last, memory_allocated);

	if(!new_memory.context.i_said_this)
	{
		// self speech doesn't dirty
		e->perceptions_dirty = true;
	}
}

CanTalkTo get_can_talk_to(Entity *e)
{
	CanTalkTo to_return = {0};
	ENTITIES_ITER(gs.entities)
	{
		if(it != e && (it->is_npc || it->is_character) && LenV2(SubV2(it->pos, e->pos)) < PROPAGATE_ACTIONS_RADIUS)
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
		if(it != from && (it->is_npc || it->is_character) && LenV2(SubV2(it->pos, from->pos)) < PROPAGATE_ACTIONS_RADIUS && it->npc_kind == targeted)
		{
			return it;
		}
	}
	return 0;
}

void remember_action(GameState *gs, Entity *to_modify, Action a, MemoryContext context)
{
	Memory new_memory = {0};
	new_memory.speech = a.speech;
	new_memory.action_taken = a.kind;
	new_memory.context = context;
	new_memory.action_argument = a.argument;

	push_memory(gs, to_modify, new_memory);

	if(context.i_said_this && (a.speech.text_length > 0 || a.kind != ACT_none))
	{
		to_modify->undismissed_action = true;
		to_modify->undismissed_action_tick = gs->tick;
		to_modify->characters_of_word_animated = 0.0f;
		to_modify->words_said_on_page = 0;
		to_modify->cur_page_index = 0;
	}
}


// returns reason why allocated on arena if invalid
// to might be null here, from can't be null
MD_String8 is_action_valid(MD_Arena *arena, Entity *from, Action a)
{
	assert(a.speech.text_length <= MAX_SENTENCE_LENGTH && a.speech.text_length >= 0);
	assert(a.kind >= 0 && a.kind < ARRLEN(actions));
	assert(from);

	MD_String8 error_message = (MD_String8){0};

	CanTalkTo talk = get_can_talk_to(from);
	if(error_message.size == 0 && a.talking_to_kind)
	{
		bool found = false;
		BUFF_ITER(Entity*, &talk)
		{
			if((*it)->npc_kind == a.talking_to_kind)
			{
				found = true;
				break;
			}
		}
		if(!found)
		{
			error_message = FmtWithLint(arena, "Character you're talking to, %s, isn't close enough to be talked to", characters[a.talking_to_kind].enum_name);
		}
	}

	if(error_message.size == 0 && a.kind == ACT_leave && gete(from->joined) == 0)
	{
		error_message = MD_S8Lit("You can't leave somebody unless you joined them.");
	}
	if(error_message.size == 0 && a.kind == ACT_join && gete(from->joined) != 0)
	{
		error_message = FmtWithLint(arena, "You can't join somebody, you're already in %s's party", characters[gete(from->joined)->npc_kind].name);
	}
	if(error_message.size == 0 && a.kind == ACT_fire_shotgun && gete(from->aiming_shotgun_at) == 0)
	{
		error_message = MD_S8Lit("You can't fire your shotgun without aiming it first");
	}
	if(error_message.size == 0 && a.kind == ACT_put_shotgun_away && gete(from->aiming_shotgun_at) == 0)
	{
		error_message = MD_S8Lit("You can't put your shotgun away without aiming it first");
	}

	bool target_is_character = a.kind == ACT_join || a.kind == ACT_aim_shotgun;

	if(error_message.size == 0 && target_is_character)
	{
		bool arg_valid = false;
		BUFF_ITER(Entity*, &talk)
		{
			if((*it)->npc_kind == a.argument.targeting) arg_valid  = true;
		}
		if(arg_valid == false)
		{
			error_message = FmtWithLint(arena, "Your action_argument for who the action `%s` be directed at, %s, is either invalid (you can't operate on nobody) or it's not an NPC that's near you right now.", actions[a.kind].name, characters[a.argument.targeting].name);
		}
	}

	if(error_message.size == 0)
	{
		AvailableActions available = {0};
		fill_available_actions(&gs, from, &available);
		bool found = false;
		MD_String8List action_strings_list = {0};
		BUFF_ITER(ActionKind, &available)
		{
			MD_S8ListPush(arena, &action_strings_list, MD_S8CString(actions[*it].name));
			if(*it == a.kind) found = true;
		}
		if(!found)
		{
			MD_String8 action_strings = MD_S8ListJoin(arena, action_strings_list, &(MD_StringJoin){.mid = MD_S8Lit(", ")});
			error_message = FmtWithLint(arena, "You cannot perform action %s right now, you can only perform these actions: [%.*s]", actions[a.kind].name, MD_S8VArg(action_strings));
		}
	}

	assert(error_message.size < MAX_SENTENCE_LENGTH); // is copied into text chunks

	return error_message;
}

// from must not be null
// the action must have been validated to be valid if you're calling this
void cause_action_side_effects(Entity *from, Action a)
{
	assert(from);
	MD_ArenaTemp scratch = MD_GetScratch(0, 0);

	MD_String8 failure_reason = is_action_valid(scratch.arena, from, a);
	if(failure_reason.size > 0)
	{
		Log("Failed to process action, invalid action: `%.*s`\n", MD_S8VArg(failure_reason));
		assert(false);
	}

	Entity *to = 0;
	if(a.talking_to_kind != NPC_nobody)
	{
		to = get_targeted(from, a.talking_to_kind);
		assert(to);
	}

	if(to)
	{
		from->looking_at = frome(to);
	}

	if(a.kind == ACT_join)
	{
		Entity *target = get_targeted(from, a.argument.targeting);
		assert(target); // error checked in is_action_valid
		from->joined = frome(target);
	}
	if(a.kind == ACT_leave)
	{
		from->joined = (EntityRef){0};
	}
	if(a.kind == ACT_aim_shotgun)
	{
		Entity *target = get_targeted(from, a.argument.targeting);
		assert(target); // error checked in is_action_valid
		from->aiming_shotgun_at = frome(target);
	}
	if(a.kind == ACT_fire_shotgun)
	{
		assert(gete(from->aiming_shotgun_at));
		gete(from->aiming_shotgun_at)->killed = true;
	}

	MD_ReleaseScratch(scratch);
}

typedef struct PropagatingAction
{
	struct PropagatingAction *next;

	Action a;
	MemoryContext context;

	Vec2 from;
	bool already_propagated_to[MAX_ENTITIES]; // tracks by index of entity
	float progress; // if greater than or equal to 1.0, is freed
} PropagatingAction;

PropagatingAction *propagating = 0;

PropagatingAction ignore_entity(Entity *to_ignore, PropagatingAction p)
{
	PropagatingAction to_return = p;
	to_return.already_propagated_to[frome(to_ignore).index] = true;
	return to_return;
}

void push_propagating(PropagatingAction to_push)
{
	to_push.context.heard_physically = true;
	bool found = false;
	for(PropagatingAction *cur = propagating; cur; cur = cur->next)
	{
		if(cur->progress >= 1.0f)
		{
			PropagatingAction *prev_next = cur->next;
			*cur = to_push;
			cur->next = prev_next;
			found = true;
			break;
		}
	}

	if(!found)
	{
		PropagatingAction *cur = MD_PushArray(persistent_arena, PropagatingAction, 1);
		*cur = to_push;
		MD_StackPush(propagating, cur);
	}
}

float propagating_radius(PropagatingAction *p)
{
	float t = powf(p->progress, 0.65f);
	return Lerp(0.0f, t, PROPAGATE_ACTIONS_RADIUS);
}

// only called when the action is instantiated, correctly propagates the information
// of the action physically and through the party
// If the action is invalid, remembers the error if it's an NPC, and does nothing else
// Returns if the action was valid or not
bool perform_action(GameState *gs, Entity *from, Action a)
{
	MD_ArenaTemp scratch = MD_GetScratch(0, 0);

	MemoryContext context = {0};
	context.author_npc_kind = from->npc_kind;

	if(a.speech.text_length > 0)
		from->dialog_fade = 2.5f;

	context.talking_to_kind = a.talking_to_kind;

	MD_String8 is_valid = is_action_valid(scratch.arena, from, a);
	bool proceed_propagating = true;
	if(is_valid.size > 0)
	{
		assert(!from->is_character);
		append_to_errors(from, is_valid);
		proceed_propagating = false;
	}



	Entity *targeted = 0; 
	if(proceed_propagating)
	{
		targeted = get_targeted(from, a.talking_to_kind);
		if(from->errorlist_first)
			MD_StackPush(text_chunk_free_list, from->errorlist_first);
		from->errorlist_first = 0;
		from->errorlist_last = 0;

		cause_action_side_effects(from, a);

		// self memory
		if(!from->is_character)
		{
			MemoryContext my_context = context;
			my_context.i_said_this = true;
			remember_action(gs, from, a, my_context); 
		}

		if(a.speech.text_length == 0 && a.kind == ACT_none)
		{
			proceed_propagating = false; // didn't say anything
		}
	}

	if(proceed_propagating)
	{
		// memory of target
		if(targeted)
		{
			remember_action(gs, targeted, a, context);
		}

		// propagate physically
		PropagatingAction to_propagate = {0};
		to_propagate.a = a;
		to_propagate.context = context;
		to_propagate.from = from->pos;
		to_propagate = ignore_entity(from, to_propagate);
		if(targeted)
		{
			to_propagate = ignore_entity(targeted, to_propagate);
		}
		push_propagating(to_propagate);
	}

	MD_ReleaseScratch(scratch);
	return proceed_propagating;
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
			*to_return = (Entity) { 0 };
			to_return->exists = true;
			to_return->generation = gen + 1;
			return to_return;
		}
	}
	assert(false);
	return NULL;
}

typedef struct ToVisit {
	struct ToVisit *next;
	struct ToVisit *prev;
	MD_Node *ptr;
	int depth;
} ToVisit ;

bool in_arr(ToVisit *arr, MD_Node *n)
{
	for(ToVisit *cur = arr; cur; cur = cur->next)
	{
		if(cur->ptr == n) return true;
	}
	return false;
}

void dump_nodes(MD_Node *node)
{
	MD_ArenaTemp scratch = MD_GetScratch(0, 0);
	ToVisit *horizon_first = 0;
	ToVisit *horizon_last = 0;

	ToVisit *visited = 0;

	ToVisit *first = MD_PushArrayZero(scratch.arena, ToVisit, 1);
	first->ptr = node;
	MD_DblPushBack(horizon_first, horizon_last, first);

	while(horizon_first)
	{
		ToVisit *cur_tovisit = horizon_first;
		MD_DblRemove(horizon_first, horizon_last, cur_tovisit);
		MD_StackPush(visited, cur_tovisit);
		char *tagstr = "   ";
		if(cur_tovisit->ptr->kind == MD_NodeKind_Tag) tagstr = "TAG";
		printf("%s", tagstr);

		for(int i = 0; i < cur_tovisit->depth; i++) printf(" |");

		printf(" `%.*s`\n", MD_S8VArg(cur_tovisit->ptr->string));

		for(MD_Node *cur = cur_tovisit->ptr->first_child; !MD_NodeIsNil(cur); cur = cur->next)
		{
			if(!in_arr(visited, cur))
			{
				ToVisit *new = MD_PushArrayZero(scratch.arena, ToVisit, 1);
				new->depth = cur_tovisit->depth + 1;
				new->ptr = cur;
				MD_DblPushFront(horizon_first, horizon_last, new);
			}
		}
		for(MD_Node *cur = cur_tovisit->ptr->first_tag; !MD_NodeIsNil(cur); cur = cur->next)
		{
			if(!in_arr(visited, cur))
			{
				ToVisit *new = MD_PushArrayZero(scratch.arena, ToVisit, 1);
				new->depth = cur_tovisit->depth + 1;
				new->ptr = cur;
				MD_DblPushFront(horizon_first, horizon_last, new);
			}
		}
	}

	MD_ReleaseScratch(scratch);
}

// allocates the error on the arena
MD_Node *expect_childnode(MD_Arena *arena, MD_Node *parent, MD_String8 string, MD_String8List *errors)
{
	MD_Node *to_return = MD_NilNode();
	if(errors->node_count == 0)
	{
		MD_Node *child_node = MD_ChildFromString(parent, string, 0);
		if(MD_NodeIsNil(child_node))
		{
			PushWithLint(arena, errors, "Couldn't find expected field %.*s", MD_S8VArg(string));
		}
		else
		{
			to_return = child_node;
		}
	}
	return to_return;
}

int parse_enumstr_impl(MD_Arena *arena, MD_String8 enum_str, char **enumstr_array, int enumstr_array_length, MD_String8List *errors, char *enum_kind_name, char *prefix)
{
	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);
	int to_return = -1;
	if(errors->node_count == 0)
	{
		MD_String8 enum_name_looking_for = enum_str;
		if(enum_name_looking_for.size == 0)
		{
			PushWithLint(arena, errors, "`%s` string must be of size greater than 0", enum_kind_name);
		}
		else
		{
			for(int i = 0; i < enumstr_array_length; i++)
			{
				if(MD_S8Match(FmtWithLint(scratch.arena, "%s%s", prefix, enumstr_array[i]), enum_name_looking_for, 0))
				{
					to_return = i;
					break;
				}
			}
		}
	}

	if(to_return == -1)
	{
		PushWithLint(arena, errors, "The %s `%.*s` could not be recognized in the game", enum_kind_name, MD_S8VArg(enum_str));
	}

	MD_ReleaseScratch(scratch);

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

void initialize_gamestate_from_threedee_level(GameState *gs, ThreeDeeLevel *level)
{
	memset(gs, 0, sizeof(GameState));

	bool found_player = false;
	for(PlacedEntity *cur = level->placed_entity_list; cur; cur = cur->next)
	{
		Entity *cur_entity = new_entity(gs);
		cur_entity->npc_kind = cur->npc_kind;
		cur_entity->pos = point_plane(cur->t.offset);
		if(cur_entity->npc_kind == NPC_Player)
		{
			found_player = true;
			cur_entity->is_character = true;
			gs->player = cur_entity;
		}
		else
		{
			cur_entity->is_npc = true;
		}
	}
	assert(found_player);

	gs->world_entity = new_entity(gs);
	gs->world_entity->is_world = true;


	ENTITIES_ITER(gs->entities)
	{
		it->rotation = PI32;
		it->target_rotation = it->rotation;
	}

	// @Place(parse and enact the drama document)
	if(1)
	{
		MD_String8List drama_errors = {0};

		MD_ArenaTemp scratch = MD_GetScratch(0, 0);
		MD_String8 filename = MD_S8Lit("assets/drama.mdesk");
		MD_String8 drama_document = MD_LoadEntireFile(scratch.arena, filename);
		assert(drama_document.size != 0);
		MD_ParseResult parse = MD_ParseWholeString(scratch.arena, filename, drama_document);
		if(parse.errors.first)
		{
			for(MD_Message *cur = parse.errors.first; cur; cur = cur->next)
			{
				MD_String8 to_print = MD_FormatMessage(scratch.arena, MD_CodeLocFromNode(cur->node), cur->kind, cur->string);
				PushWithLint(scratch.arena, &drama_errors, "Failed to parse: `%.*s`\n", MD_S8VArg(to_print));
			}
		}

		if(drama_errors.node_count == 0)
		{
			MD_Node *can_hear = MD_NilNode();
			for(MD_Node *cur = parse.node->first_child->first_child; !MD_NodeIsNil(cur) && drama_errors.node_count == 0; cur = cur->next)
			{
				MD_Node *cur_can_hear = MD_ChildFromString(cur, MD_S8Lit("can_hear"), 0);
				if(!MD_NodeIsNil(cur_can_hear))
				{
					if(MD_NodeIsNil(cur_can_hear->first_child))
					{
						PushWithLint(scratch.arena, &drama_errors, "`can_hear` must be followed by a valid array of NPC kinds who can hear the following conversation");
					}
					else
					{
						can_hear = cur_can_hear->first_child;
					}
				}
				else
				{
					if(MD_NodeIsNil(can_hear))
					{
						PushWithLint(scratch.arena, &drama_errors, "Expected a statement with `can_hear` before any speech that says who can hear the current speech");
					}

					Action current_action = {0};
					MemoryContext current_context = {0};
					current_context.dont_show_to_player = true;
					if(drama_errors.node_count == 0)
					{
						MD_String8 enum_str = expect_childnode(scratch.arena, cur, MD_S8Lit("enum"), &drama_errors)->first_child->string;
						MD_String8 dialog = expect_childnode(scratch.arena, cur, MD_S8Lit("dialog"), &drama_errors)->first_child->string;
						MD_String8 action_str = MD_ChildFromString(cur, MD_S8Lit("action"), 0)->first_child->string; 
						MD_String8 action_argument_str = MD_ChildFromString(cur, MD_S8Lit("action_argument"), 0)->first_child->string; 
						MD_String8 to_str = MD_ChildFromString(cur, MD_S8Lit("to"), 0)->first_child->string;

						if(to_str.size > 0)
						{
							NpcKind talking_to = parse_enumstr(scratch.arena, to_str, &drama_errors, NpcKind_enum_names, "NpcKind", "");
							if (talking_to == NPC_nobody)
							{
								PushWithLint(scratch.arena, &drama_errors, "The string provided for the 'to' field, intended to be who the NPC is directing their speech and action at, is invalid and is '%.*s'", MD_S8VArg(to_str));
							}
							else
							{
								current_context.talking_to_kind = talking_to;
								current_action.talking_to_kind = talking_to;
							}
						}

						current_context.author_npc_kind = parse_enumstr(scratch.arena, enum_str, &drama_errors, NpcKind_enum_names, "NpcKind", "");
						if(action_str.size > 0)
						{
							current_action.kind = parse_enumstr(scratch.arena, action_str, &drama_errors, ActionKind_names, "ActionKind", "ACT_");
						}
						if(action_argument_str.size > 0)
						{
							current_action.argument.targeting = parse_enumstr(scratch.arena, action_argument_str, &drama_errors, NpcKind_names, "NpcKind", "");
						}

						if(dialog.size >= ARRLEN(current_action.speech.text))
						{
							PushWithLint(scratch.arena, &drama_errors, "Current action_str's speech is of size %d, bigger than allowed size %d", (int)dialog.size, (int)ARRLEN(current_action.speech.text));
						}

						if(drama_errors.node_count == 0)
						{
							chunk_from_s8(&current_action.speech, dialog);
						}
					}

					if(drama_errors.node_count == 0)
					{
						for(MD_Node *cur_kind_node = can_hear; !MD_NodeIsNil(cur_kind_node); cur_kind_node = cur_kind_node->next)
						{
							NpcKind want = parse_enumstr(scratch.arena, cur_kind_node->string, &drama_errors, NpcKind_enum_names, "NpcKind", "");
							if(drama_errors.node_count == 0)
							{
								bool found = false;
								ENTITIES_ITER(gs->entities)
								{
									if(it->is_npc && it->npc_kind == want)
									{
										MemoryContext this_context = current_context;
										if(it->npc_kind == current_context.author_npc_kind)
										{
											this_context.i_said_this = true;
										}
										remember_action(gs, it, current_action, this_context);
										if(it->npc_kind != current_context.author_npc_kind && it->npc_kind != current_context.talking_to_kind)
										{
											// it's good for NPC health that they have examples of not saying anything in response to others speaking,
											// so that they do the same when it's unlikely for them to talk.
											Action no_speak = {0};
											MemoryContext no_speak_context = {.i_said_this = true, .author_npc_kind = it->npc_kind};
											remember_action(gs, it, no_speak, no_speak_context);
										}

										it->undismissed_action = false; // prevent the animating in sound effects of words said in drama document

										found = true;
										break;
									}
								}

								if(!found)
								{
									Log("Warning: NPC of kind %s isn't on the map, but has entries in the drama document\n", characters[want].enum_name);
								}
							}
							Log("Propagated to %d name '%s'...\n", want, characters[want].name);
						}
					}
				}
			}
		}

		if(drama_errors.node_count > 0)
		{
			for(MD_String8Node *cur = drama_errors.first; cur; cur = cur->next)
			{
				fprintf(stderr, "Error: %.*s\n", MD_S8VArg(cur->string));
			}
			assert(false);
		}


		ENTITIES_ITER(gs->entities)
		{
			it->perceptions_dirty = false; // nobody should say anything about jester memories
		}
	}
}

ThreeDeeLevel level_threedee = {0};

void reset_level()
{
	initialize_gamestate_from_threedee_level(&gs, &level_threedee);
}

enum
{
	V0,

	VMax,
} Version;


#define SER_BUFF(ser, BuffElemType, buff_ptr) {ser_int(ser, &((buff_ptr)->cur_index));\
	if((buff_ptr)->cur_index > ARRLEN((buff_ptr)->data))\
	{\
		ser->cur_error = (SerError){.failed = true, .why = MD_S8Fmt(ser->error_arena, "Current index %d is more than the buffer %s's maximum, %d", (buff_ptr)->cur_index, #buff_ptr, ARRLEN((buff_ptr)->data))};\
	}\
	BUFF_ITER(BuffElemType, buff_ptr)\
	{\
		ser_##BuffElemType(ser, it);\
	}\
}

void ser_TextChunk(SerState *ser, TextChunk *t)
{
	ser_int(ser, &t->text_length);
	if(t->text_length >= ARRLEN(t->text))
	{
		ser->cur_error = (SerError){.failed = true, .why = MD_S8Fmt(ser->error_arena, "In text chunk, length %d is too big to fit into %d", t->text_length, ARRLEN(t->text))};
	}
	ser_bytes(ser, (MD_u8*)t->text, t->text_length);
}

void ser_entity(SerState *ser, Entity *e)
{
	ser_bool(ser, &e->exists);
	ser_bool(ser, &e->destroy);
	ser_int(ser, &e->generation);

	ser_Vec2(ser, &e->pos);
	ser_Vec2(ser, &e->vel);
	ser_float(ser, &e->damage);


	ser_bool(ser, &e->is_world);

	ser_bool(ser, &e->is_npc);
	ser_bool(ser, &e->being_hovered);
	ser_bool(ser, &e->perceptions_dirty);

	if(ser->serializing)
	{
		TextChunkList *cur = e->errorlist_first;
		bool more_errors = cur != 0;
		ser_bool(ser, &more_errors);
		while(more_errors)
		{
			ser_TextChunk(ser, &cur->text);
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
			TextChunkList *new_chunk = MD_PushArray(ser->arena, TextChunkList, 1);
			ser_TextChunk(ser, &new_chunk->text);
			MD_DblPushBack(e->errorlist_first, e->errorlist_last, new_chunk);
			ser_bool(ser, &more_errors);
		}
	}

	if(ser->serializing)
	{
		Memory *cur = e->memories_first;
		bool more_memories = cur != 0;
		ser_bool(ser, &more_memories);
		while(more_memories)
		{
			ser_Memory(ser, cur);
			cur = cur->next;
			more_memories = cur != 0;
			ser_bool(ser, &more_memories);
		}
	}
	else
	{
		bool more_memories;
		ser_bool(ser, &more_memories);
		while(more_memories)
		{
			Memory *new_chunk = MD_PushArray(ser->arena, Memory, 1);
			ser_Memory(ser, new_chunk);
			MD_DblPushBack(e->memories_first, e->memories_last, new_chunk);
			ser_bool(ser, &more_memories);
		}
	}

	ser_float(ser, &e->dialog_panel_opacity);

	ser_bool(ser, &e->undismissed_action);
	ser_uint64_t(ser, &e->undismissed_action_tick);
	ser_float(ser, &e->characters_of_word_animated);
	ser_int(ser, &e->words_said_on_page);
	ser_int(ser, &e->cur_page_index);

	ser_NpcKind(ser, &e->npc_kind);
	ser_int(ser, &e->gen_request_id);
	ser_Vec2(ser, &e->target_goto);

	// character
	ser_bool(ser, &e->is_character);
	ser_Vec2(ser, &e->to_throw_direction);

	SER_BUFF(ser, Vec2, &e->position_history);
	ser_CharacterState(ser, &e->state);
	ser_EntityRef(ser, &e->talking_to);
}

void ser_GameState(SerState *ser, GameState *gs)
{
	if(ser->serializing) ser->version = VMax - 1;
	ser_int(ser, &ser->version);
	if(ser->version >= VMax)
	{
		ser->cur_error = (SerError){.failed = true, .why = MD_S8Fmt(ser->error_arena, "Version %d is beyond the current version, %d", ser->version, VMax - 1)};
	}

	ser_uint64_t(ser, &gs->tick);
	ser_bool(ser, &gs->won);
	int num_entities = MAX_ENTITIES;
	ser_int(ser, &num_entities);
	
	assert(num_entities <= MAX_ENTITIES);
	for(int i = 0; i < num_entities; i++)
	{
		ser_bool(ser, &gs->entities[i].exists);
		if(gs->entities[i].exists)
		{
			ser_entity(ser, &(gs->entities[i]));
		}
	}

	gs->player = 0;
	gs->world_entity = 0;


	if(!ser->cur_error.failed)
	{
		ARR_ITER(Entity, gs->entities)
		{
			if(it->is_character)
			{
				gs->player = it;
			}
			if(it->is_world)
			{
				gs->world_entity = it;
			}
		}

		if(gs->player == 0)
		{
			ser->cur_error = (SerError){.failed = true, .why = MD_S8Lit("No player entity found in deserialized entities")};
		}
		if(gs->world_entity == 0)
		{
			ser->cur_error = (SerError){.failed = true, .why = MD_S8Lit("No world entity found in deserialized entities")};
		}
	}
}

// error_out is allocated onto arena if it fails
MD_String8 save_to_string(MD_Arena *output_bytes_arena, MD_Arena *error_arena, MD_String8 *error_out, GameState *gs)
{
	MD_u8 *serialized_data = 0;
	MD_u64 serialized_length = 0;
	{
		SerState ser = {
			.version = VMax - 1,
			.serializing = true,
			.error_arena = error_arena,
		};
		ser_GameState(&ser, gs);

		if(ser.cur_error.failed)
		{
			*error_out = ser.cur_error.why;
		}
		else
		{
			ser.arena = 0; // serialization should never require allocation
			ser.max = ser.cur;
			ser.cur = 0;
			ser.version = VMax - 1;
			MD_ArenaTemp temp = MD_ArenaBeginTemp(output_bytes_arena);
			serialized_data = MD_ArenaPush(temp.arena, ser.max);
			ser.data = serialized_data;

			ser_GameState(&ser, gs);
			if(ser.cur_error.failed)
			{
				Log("Very weird that serialization fails a second time...\n");
				*error_out = MD_S8Fmt(error_arena, "VERY BAD Serialization failed after it already had no error: %.*s", ser.cur_error.why);
				MD_ArenaEndTemp(temp);
				serialized_data = 0;
			}
			else
			{
				serialized_length = ser.cur;
			}
		}
	}
	return MD_S8(serialized_data, serialized_length);
}


// error strings are allocated on error_arena, probably scratch for that. If serialization fails,
// nothing is allocated onto arena, the allocations are rewound
// If there was an error, the gamestate returned might be partially constructed and bad. Don't use it
GameState load_from_string(MD_Arena *arena, MD_Arena *error_arena, MD_String8 data, MD_String8 *error_out)
{
	MD_ArenaTemp temp = MD_ArenaBeginTemp(arena);

	SerState ser = {
		.serializing = false,
		.data = data.str,
		.max = data.size,
		.arena = temp.arena,
		.error_arena = error_arena,
	};
	GameState to_return = {0};
	ser_GameState(&ser, &to_return);
	if(ser.cur_error.failed)
	{
		MD_ArenaEndTemp(temp); // no allocations if it fails
		*error_out = ser.cur_error.why;
	}
	return to_return;
}

#ifdef WEB
EMSCRIPTEN_KEEPALIVE
void dump_save_data()
{
	MD_ArenaTemp scratch = MD_GetScratch(0, 0);

	MD_String8 error = {0};
	MD_String8 saved = save_to_string(scratch.arena, scratch.arena, &error, &gs);

	if(error.size > 0)
	{
		Log("Failed to save game: %.*s\n", MD_S8VArg(error));
	}
	else
	{
		EM_ASM( {
			save_game_data = new Int8Array(Module.HEAP8.buffer, $0, $1);
		}, (char*)(saved.str), saved.size);
	}

	MD_ReleaseScratch(scratch);
}

EMSCRIPTEN_KEEPALIVE
void read_from_save_data(char *data, size_t length)
{
	MD_ArenaTemp scratch = MD_GetScratch(0, 0);
	MD_String8 data_str = MD_S8((MD_u8*)data, length);

	MD_String8 error = {0};
	GameState new_gs = load_from_string(persistent_arena, scratch.arena, data_str, &error);

	if(error.size > 0)
	{
		Log("Failed to load from size %lu: %.*s\n", length, MD_S8VArg(error));
	}
	else
	{
		gs = new_gs;
	}

	MD_ReleaseScratch(scratch);
}
#endif

// a callback, when 'text backend' has finished making text. End dialog
void end_text_input(char *what_player_said_cstr)
{
	MD_ArenaTemp scratch = MD_GetScratch(0, 0);
	// avoid double ending text input
	if (!receiving_text_input)
	{
		return;
	}
	receiving_text_input = false;

	size_t player_said_len = strlen(what_player_said_cstr);
	int actual_len = 0;
	for (int i = 0; i < player_said_len; i++) if (what_player_said_cstr[i] != '\n') actual_len++;
	if (actual_len == 0)
	{
		// this just means cancel the dialog
	}
	else
	{
		MD_String8 what_player_said = MD_S8CString(what_player_said_cstr);
		what_player_said = MD_S8ListJoin(scratch.arena, MD_S8Split(scratch.arena, what_player_said, 1, &MD_S8Lit("\n")), &(MD_StringJoin){0});

		Action to_perform = {0};
		what_player_said = MD_S8Substring(what_player_said, 0, ARRLEN(to_perform.speech.text));

		chunk_from_s8(&to_perform.speech, what_player_said);

		if(gete(gs.player->talking_to))
		{
			assert(gete(gs.player->talking_to)->is_npc);
			to_perform.talking_to_kind = gete(gs.player->talking_to)->npc_kind;
		}

		perform_action(&gs, gs.player, to_perform);
	}
	MD_ReleaseScratch(scratch);
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


typedef struct {
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
	sg_pass_action clear_depth_buffer_pass_action;
	sg_pipeline twodee_pip;
	sg_bindings bind;

	sg_pipeline threedee_pip;
	sg_pipeline armature_pip;
	sg_bindings threedee_bind;

	sg_image outline_pass_image;
	sg_pass outline_pass;
	sg_pipeline outline_mesh_pip;
	sg_pipeline outline_armature_pip;

	sg_pass threedee_pass; // is a pass so I can do post processing in a shader
	sg_image threedee_pass_image;
	sg_image threedee_pass_depth_image;

	sg_pipeline twodee_outline_pip;
	sg_pipeline twodee_colorcorrect_pip;

	Shadow_State shadows;
} state;

// is a function, because also called when window resized to recreate the pass and the image.
// its target image must be the same size as the viewport. Is the reason. Cowabunga!
void create_screenspace_gfx_state()
{
	// this prevents common bug of whats passed to destroy func not being the resource
	// you queried to see if it exists
#define MAYBE_DESTROY(resource, destroy_func) if(resource.id != 0) destroy_func(resource);
	MAYBE_DESTROY(state.outline_pass, sg_destroy_pass);
	MAYBE_DESTROY(state.threedee_pass, sg_destroy_pass);

	MAYBE_DESTROY(state.outline_pass_image, sg_destroy_image);
	MAYBE_DESTROY(state.threedee_pass_image, sg_destroy_image);
	MAYBE_DESTROY(state.threedee_pass_depth_image, sg_destroy_image);
#undef MAYBE_DESTROY

	const sg_shader_desc *shd_desc = threedee_mesh_outline_shader_desc(sg_query_backend());
	assert(shd_desc);
	sg_shader shd = sg_make_shader(shd_desc);

	state.outline_mesh_pip = sg_make_pipeline(&(sg_pipeline_desc)
			{
			.shader = shd,
			.depth = {
			0
			},
			.sample_count = 1,
			.layout = {
			.attrs =
			{
			[ATTR_threedee_vs_pos_in].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_threedee_vs_uv_in].format = SG_VERTEXFORMAT_FLOAT2,
			}
			},
			.colors[0].blend = (sg_blend_state) { // allow transparency
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

	state.outline_armature_pip = sg_make_pipeline(&(sg_pipeline_desc)
			{
			.shader = shd,
			.depth = {
				.pixel_format = SG_PIXELFORMAT_NONE,
			},
			.sample_count = 1,
			.layout = {
			.attrs =
			{
			[ATTR_threedee_vs_skeleton_pos_in].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_threedee_vs_skeleton_uv_in].format = SG_VERTEXFORMAT_FLOAT2,
			[ATTR_threedee_vs_skeleton_indices_in].format = SG_VERTEXFORMAT_USHORT4N,
			[ATTR_threedee_vs_skeleton_weights_in].format = SG_VERTEXFORMAT_FLOAT4,
			}
			},
			.colors[0].blend = (sg_blend_state) { // allow transparency
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
		.pixel_format = SG_PIXELFORMAT_RGBA8,
		.min_filter = SG_FILTER_LINEAR,
		.mag_filter = SG_FILTER_LINEAR,
		.wrap_u = SG_WRAP_CLAMP_TO_BORDER,
		.wrap_v = SG_WRAP_CLAMP_TO_BORDER,
		.border_color = SG_BORDERCOLOR_OPAQUE_WHITE,
		.sample_count = 1,
		.label = "outline-pass-render-target",
	};
	state.outline_pass_image = sg_make_image(&desc);
	state.outline_pass = sg_make_pass(&(sg_pass_desc){
			.color_attachments[0].image = state.outline_pass_image,
			.depth_stencil_attachment = {
				0
			},
			.label = "outline-pass",
	});

	desc.sample_count = 0;
	
	desc.label = "threedee-pass-render-target";
	state.threedee_pass_image = sg_make_image(&desc);

	desc.label = "threedee-pass-depth-render-target";
	desc.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
	state.threedee_pass_depth_image = sg_make_image(&desc);

	state.threedee_pass = sg_make_pass(&(sg_pass_desc){
			.color_attachments[0].image = state.threedee_pass_image,
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
Animation *get_anim_by_name(Armature *armature, MD_String8 anim_name)
{
	MD_String8List anims = {0};
	for(MD_u64 i = 0; i < armature->animations_length; i++)
	{
		MD_S8ListPush(frame_arena, &anims, armature->animations[i].name);
		if(MD_S8Match(armature->animations[i].name, anim_name, 0))
		{
			return &armature->animations[i];
		}
	}

	if(anim_name.size > 0)
	{
		MD_String8 anims_str = MD_S8ListJoin(frame_arena, anims, &(MD_StringJoin){.mid = MD_S8Lit(", ")});
		Log("No animation found '%.*s', the animations: [%.*s]\n", MD_S8VArg(anim_name), MD_S8VArg(anims_str));
	}

	for(MD_u64 i = 0; i < armature->animations_length; i++)
	{
		if(MD_S8Match(armature->animations[i].name, MD_S8Lit("Idle"), 0))
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
	if(dont_loop)
	{
		time = fminf(time, total_anim_time);
	}
	else
	{
		time = fmodf(time, total_anim_time);
	}
	for(MD_u64 i = 0; i < track->poses_length - 1; i++)
	{
		if(track->poses[i].time <= time && time <= track->poses[i + 1].time)
		{
			PoseBone from = track->poses[i];
			PoseBone to = track->poses[i + 1];
			float gap_btwn_keyframes = to.time - from.time;
			float t = (time - from.time)/gap_btwn_keyframes;
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
	MD_u8 rgba[4];
} PixelData;

PixelData encode_normalized_float32(float to_encode)
{
	Vec4 to_return_vector = {0};

	// x is just -1.0f or 1.0f, encoded as a [0,1] normalized float. 
	if(to_encode < 0.0f) to_return_vector.x = -1.0f;
	else to_return_vector.x = 1.0f;
	to_return_vector.x = to_return_vector.x / 2.0f + 0.5f;

	float without_sign = fabsf(to_encode);
	to_return_vector.y = without_sign - floorf(without_sign);

	to_return_vector.z = fabsf(to_encode) - to_return_vector.y;
	assert(to_return_vector.z < 255.0f);
	to_return_vector.z /= 255.0f;

	// w is unused for now, but is 1.0f (and is the alpha channel in Vec4) so that it displays properly as a texture
	to_return_vector.w = 1.0f;


	PixelData to_return = {0};

	for(int i = 0; i < 4; i++)
	{
		assert(0.0f <= to_return_vector.Elements[i] && to_return_vector.Elements[i] <= 1.0f);
		to_return.rgba[i] = (MD_u8)(to_return_vector.Elements[i] * 255.0f);
	}

	return to_return;
}

float decode_normalized_float32(PixelData encoded)
{
	Vec4 v = {0};
	for(int i = 0; i < 4; i++)
	{
		v.Elements[i] = (float)encoded.rgba[i] / 255.0f;
	}

	float sign = 2.0f * v.x - 1.0f;

	float to_return = sign * (v.z*255.0f + v.y);

	return to_return;
}




void audio_stream_callback(float *buffer, int num_frames, int num_channels)
{
	assert(num_channels == 1);
	const int num_samples = num_frames * num_channels;
	double time_per_sample = 1.0 / (double)SAMPLE_RATE;
	for (int i = 0; i < num_samples; i++)
	{
		float output_frame = 0.0f;
		for (int audio_i = 0; audio_i < ARRLEN(playing_audio); audio_i++)
		{
			AudioPlayer *it = &playing_audio[audio_i];
			if (it->sample != 0)
			{
				if (cursor_pcm(it) >= it->sample->pcm_data_length)
				{
					it->sample = 0;
				}
				else
				{
					output_frame += it->sample->pcm_data[cursor_pcm(it)]*(float)(it->volume + 1.0);
					it->cursor_time += time_per_sample*(it->pitch + 1.0);
				}
			}
		}
		buffer[i] = output_frame;
	}
}


#define WHITE     ((Color) { 1.0f, 1.0f, 1.0f, 1.0f })
#define GREY      ((Color) { 0.4f, 0.4f, 0.4f, 1.0f })
#define BLACK ((Color) { 0.0f, 0.0f, 0.0f, 1.0f })
#define RED   ((Color) { 1.0f, 0.0f, 0.0f, 1.0f })
#define PINK  ((Color) { 1.0f, 0.0f, 1.0f, 1.0f })
#define BLUE  ((Color) { 0.0f, 0.0f, 1.0f, 1.0f })
#define LIGHTBLUE ((Color) { 0.2f, 0.2f, 0.8f, 1.0f })
#define GREEN ((Color) { 0.0f, 1.0f, 0.0f, 1.0f })
#define BROWN (colhex(0x4d3d25))
#define YELLOW (colhex(0xffdd00))

Color oflightness(float dark)
{
	return (Color) { dark, dark, dark, 1.0f };
}

Color colhex(uint32_t hex)
{
	int r = (hex & 0xff0000) >> 16;
	int g = (hex & 0x00ff00) >> 8;
	int b = (hex & 0x0000ff) >> 0;

	return (Color) { (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 1.0f };
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
	sg_image_info info = sg_query_image_info(img);
	return V2((float)info.width, (float)info.height);
}

#ifdef DEVTOOLS
void do_metadesk_tests()
{
	Log("Testing metadesk library...\n");
	MD_Arena *arena = MD_ArenaAlloc();
	MD_String8 s = MD_S8Lit("This is a testing|string");

	MD_String8List split_up = MD_S8Split(arena, s, 1, &MD_S8Lit("|"));

	assert(split_up.node_count == 2);
	assert(MD_S8Match(split_up.first->string, MD_S8Lit("This is a testing"), 0));
	assert(MD_S8Match(split_up.last->string, MD_S8Lit("string"), 0));

	MD_ArenaRelease(arena);

	Log("Testing passed!\n");
}
void do_parsing_tests()
{
	Log("(UNIMPLEMENTED) Testing chatgpt parsing...\n");

	MD_ArenaTemp scratch = MD_GetScratch(0, 0);
	MD_ReleaseScratch(scratch);
}

// these tests rely on the base level having been loaded
void do_serialization_tests()
{
	Log("Testing serialization...\n");

	MD_ArenaTemp scratch = MD_GetScratch(0, 0);
	
	GameState gs = {0};
	initialize_gamestate_from_threedee_level(&gs, &level_threedee);

	gs.player->pos = V2(50.0f, 0.0);

	MD_String8 error = {0};
	MD_String8 saved = save_to_string(scratch.arena, scratch.arena, &error, &gs);

	assert(error.size == 0);
	assert(saved.size > 0);
	assert(saved.str != 0);

	initialize_gamestate_from_threedee_level(&gs, &level_threedee);
	gs = load_from_string(persistent_arena, scratch.arena, saved, &error);
	assert(gs.player->pos.x == 50.0f);
	assert(error.size == 0);

	Log("Default save data size is %lld bytes\n", saved.size);

	MD_ReleaseScratch(scratch);
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

typedef struct {
	float font_size;
	float font_line_advance;
	float font_scale;
	MD_String8 font_buffer;
	stbtt_bakedchar cdata[96]; // ascii characters?
	stbtt_fontinfo font;

	sg_image image;
	Vec2 size; // this image's size is queried a lot, and img_size seems to be slow when profiled
} LoadedFont;

LoadedFont default_font;
LoadedFont font_for_text_input; // is bigger

LoadedFont load_font(MD_Arena *arena, MD_String8 font_filepath, float font_size)
{
	LoadedFont to_return = {0};

	to_return.font_buffer = MD_LoadEntireFile(arena, font_filepath);
	to_return.font_size = font_size;

	unsigned char *font_bitmap = MD_ArenaPush(arena, 512*512);

	const int font_bitmap_width = 512;

	stbtt_BakeFontBitmap(to_return.font_buffer.str, 0, to_return.font_size, font_bitmap, font_bitmap_width, font_bitmap_width, 32, 96, to_return.cdata);

	unsigned char *font_bitmap_rgba = MD_ArenaPush(frame_arena, 4 * font_bitmap_width * font_bitmap_width);

	// also flip the image, because I think opengl or something I'm too tired
	for(int row = 0; row < 512; row++)
	{
		for(int col = 0; col < 512; col++)
		{
			int i = row * 512 + col;
			int flipped_i = (512 - row) * 512 + col;
			font_bitmap_rgba[i*4 + 0] = 255;
			font_bitmap_rgba[i*4 + 1] = 255;
			font_bitmap_rgba[i*4 + 2] = 255;
			font_bitmap_rgba[i*4 + 3] = font_bitmap[flipped_i];
		}
	}

	to_return.image = sg_make_image(&(sg_image_desc) {
		.width = font_bitmap_width,
		.height = font_bitmap_width,
		.pixel_format = SG_PIXELFORMAT_RGBA8,
		.min_filter = SG_FILTER_LINEAR,
		.mag_filter = SG_FILTER_LINEAR,
		.data.subimage[0][0] =
		{
			.ptr = font_bitmap_rgba,
			.size = (size_t)(font_bitmap_width * font_bitmap_width * 4),
		}
	});

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

// armatureanimations are processed once every visual frame from this list
Armature *armatures[] = {
	&player_armature,
	&farmer_armature,
	&shifted_farmer_armature,
	&man_in_black_armature,
};

Mesh mesh_player = {0};
Mesh mesh_simple_worm = {0};
Mesh mesh_shotgun = {0};

void stbi_flip_into_correct_direction(bool do_it)
{
	if(do_it) stbi_set_flip_vertically_on_load(true);
}

void init(void)
{
	stbi_flip_into_correct_direction(true);

#ifdef WEB
	EM_ASM( {
			set_server_url(UTF8ToString($0));
			}, SERVER_DOMAIN );
#endif

	frame_arena = MD_ArenaAlloc();
#ifdef WEB
	next_arena_big  = true;
#endif
	persistent_arena = MD_ArenaAlloc();

#ifdef DEVTOOLS
	Log("Devtools is on!\n");
#else
	Log("Devtools is off!\n");
#endif



	Log("Size of entity struct: %zu\n", sizeof(Entity));
	Log("Size of %d gs.entities: %zu kb\n", (int)ARRLEN(gs.entities), sizeof(gs.entities) / 1024);
	sg_setup(&(sg_desc) {
			.context = sapp_sgcontext(),
			.buffer_pool_size = 512,
			});
	stm_setup();
	saudio_setup(&(saudio_desc) {
			.stream_cb = audio_stream_callback,
			.logger.func = slog_func,
			});

	load_assets();

	MD_String8 binary_file;

	binary_file = MD_LoadEntireFile(frame_arena, MD_S8Lit("assets/exported_3d/level.bin"));
	level_threedee = load_level(persistent_arena, binary_file);

	binary_file = MD_LoadEntireFile(frame_arena, MD_S8Lit("assets/exported_3d/ExportedWithAnims.bin"));
	mesh_player = load_mesh(persistent_arena, binary_file, MD_S8Lit("ExportedWithAnims.bin"));

	binary_file = MD_LoadEntireFile(frame_arena, MD_S8Lit("assets/exported_3d/ShotgunMesh.bin"));
	mesh_shotgun = load_mesh(persistent_arena, binary_file, MD_S8Lit("ShotgunMesh.bin"));

	binary_file = MD_LoadEntireFile(frame_arena, MD_S8Lit("assets/exported_3d/ArmatureExportedWithAnims.bin"));
	player_armature = load_armature(persistent_arena, binary_file, MD_S8Lit("ArmatureExportedWithAnims.bin"));

	man_in_black_armature = load_armature(persistent_arena, binary_file, MD_S8Lit("Man In Black"));
	man_in_black_armature.image = image_man_in_black;

	binary_file = MD_LoadEntireFile(frame_arena, MD_S8Lit("assets/exported_3d/Farmer.bin"));
	farmer_armature = load_armature(persistent_arena, binary_file, MD_S8Lit("Farmer.bin"));

	shifted_farmer_armature = load_armature(persistent_arena, binary_file, MD_S8Lit("Farmer.bin"));
	shifted_farmer_armature.image = image_shifted_farmer;


	MD_ArenaClear(frame_arena);

	reset_level();

#ifdef DEVTOOLS
	do_metadesk_tests();
	do_parsing_tests();
	do_serialization_tests();
	do_float_encoding_tests();
#endif

#ifdef WEB
	EM_ASM( {
			load_all();
			});
#endif

	default_font = load_font(persistent_arena, MD_S8Lit("assets/PalanquinDark-Regular.ttf"), 35.0f);
	font_for_text_input = load_font(persistent_arena, MD_S8Lit("assets/PalanquinDark-Regular.ttf"), 64.0f);
	

	state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc)
			{
			.usage = SG_USAGE_STREAM,
			//.data = SG_RANGE(vertices),
#ifdef DEVTOOLS
			.size = 1024*2500,
#else
			.size = 1024*700,
#endif
			.label = "quad-vertices"
			});

	create_screenspace_gfx_state();
	state.shadows = init_shadow_state();

	const sg_shader_desc *desc = threedee_twodee_shader_desc(sg_query_backend());
	assert(desc);
	sg_shader shd = sg_make_shader(desc);

	Color clearcol = colhex(0x98734c);
	state.twodee_pip = sg_make_pipeline(&(sg_pipeline_desc)
			{
			.shader = shd,
			.depth = {
			.compare = SG_COMPAREFUNC_LESS_EQUAL,
			.write_enabled = true
			},
			.layout = {
			.attrs =
			{
			[ATTR_threedee_vs_twodee_position].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_threedee_vs_twodee_texcoord0].format = SG_VERTEXFORMAT_FLOAT2,
			}
			},
			.colors[0].blend = (sg_blend_state) { // allow transparency
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

	state.twodee_outline_pip = sg_make_pipeline(&(sg_pipeline_desc)
			{
			.shader = sg_make_shader(threedee_twodee_outline_shader_desc(sg_query_backend())),
			.depth = {
			.compare = SG_COMPAREFUNC_LESS_EQUAL,
			.write_enabled = true
			},
			.layout = {
			.attrs =
			{
			[ATTR_threedee_vs_twodee_position].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_threedee_vs_twodee_texcoord0].format = SG_VERTEXFORMAT_FLOAT2,
			}
			},
			.colors[0].blend = (sg_blend_state) { // allow transparency
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

	state.twodee_colorcorrect_pip = sg_make_pipeline(&(sg_pipeline_desc)
			{
			.shader = sg_make_shader(threedee_twodee_colorcorrect_shader_desc(sg_query_backend())),
			.depth = {
			.compare = SG_COMPAREFUNC_LESS_EQUAL,
			.write_enabled = true
			},
			.layout = {
			.attrs =
			{
			[ATTR_threedee_vs_twodee_position].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_threedee_vs_twodee_texcoord0].format = SG_VERTEXFORMAT_FLOAT2,
			}
			},
			.colors[0].blend = (sg_blend_state) { // allow transparency
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

	state.threedee_pip = sg_make_pipeline(&(sg_pipeline_desc)
			{
			.shader = shd,
			.depth = {
			.compare = SG_COMPAREFUNC_LESS_EQUAL,
			.write_enabled = true
			},
			.layout = {
			.attrs =
			{
			[ATTR_threedee_vs_pos_in].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_threedee_vs_uv_in].format = SG_VERTEXFORMAT_FLOAT2,
			}
			},
			.colors[0].blend = (sg_blend_state) { // allow transparency
			.enabled = true,
			.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
			.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_rgb = SG_BLENDOP_ADD,
			.src_factor_alpha = SG_BLENDFACTOR_ONE,
			.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.op_alpha = SG_BLENDOP_ADD,
			},
			.label = "threedee",
			});

	desc = threedee_armature_shader_desc(sg_query_backend());
	assert(desc);
	shd = sg_make_shader(desc);

	state.armature_pip = sg_make_pipeline(&(sg_pipeline_desc)
			{
			.shader = shd,
			.depth = {
			.compare = SG_COMPAREFUNC_LESS_EQUAL,
			.write_enabled = true
			},
			.layout = {
			.attrs =
			{
			[ATTR_threedee_vs_skeleton_pos_in].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_threedee_vs_skeleton_uv_in].format = SG_VERTEXFORMAT_FLOAT2,
			[ATTR_threedee_vs_skeleton_indices_in].format = SG_VERTEXFORMAT_USHORT4N,
			[ATTR_threedee_vs_skeleton_weights_in].format = SG_VERTEXFORMAT_FLOAT4,
			}
			},
			.colors[0].blend = (sg_blend_state) { // allow transparency
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

	state.clear_depth_buffer_pass_action = (sg_pass_action)
	{
		.colors[0] = { .action = SG_ACTION_LOAD },
		.depth = { .action = SG_ACTION_CLEAR, .value = 1.0f },
	};
	state.clear_everything_pass_action = state.clear_depth_buffer_pass_action;
	state.clear_everything_pass_action.colors[0] = (sg_color_attachment_action){ .action = SG_ACTION_CLEAR, .value = { clearcol.r, clearcol.g, clearcol.b, 1.0f } };
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
Vec2 thumbstick_base_pos = { 0 };
Vec2 thumbstick_nub_pos = { 0 };
typedef struct TouchMemory
{
	// need this because uintptr_t = 0 *doesn't* mean no touching!
	bool active;
	uintptr_t identifier;
} TouchMemory;
TouchMemory activate(uintptr_t by)
{
	//Log("Activating %ld\n", by);
	return (TouchMemory) { .active = true, .identifier = by };
}
// returns if deactivated
bool maybe_deactivate(TouchMemory *memory, uintptr_t ended_identifier)
{
	if (memory->active)
	{
		if (memory->identifier == ended_identifier)
		{
			//Log("Deactivating %ld\n", memory->identifier);
			*memory = (TouchMemory) { 0 };
			return true;
		}
	}
	else
	{
		return false;
	}
	return false;
}
TouchMemory movement_touch = { 0 };
TouchMemory roll_pressed_by = { 0 };
TouchMemory attack_pressed_by = { 0 };
TouchMemory interact_pressed_by = { 0 };
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
	return V2(screen_size().x - mobile_button_size()*2.0f, screen_size().y * (0.4f + (0.4f - 0.25f)));
}
Vec2 attack_button_pos()
{
	return V2(screen_size().x - mobile_button_size()*2.0f, screen_size().y * 0.25f);
}

// everything is in pixels in world space, 43 pixels is approx 1 meter measured from 
// merchant sprite being 5'6"
const float pixels_per_meter = 43.0f;

#define IMG(img) img, full_region(img)

// full region in pixels
AABB full_region(sg_image img)
{
	return (AABB)
	{
		.upper_left = V2(0.0f, 0.0f),
			.lower_right = img_size(img),
	};
}

AABB aabb_at(Vec2 at, Vec2 size)
{
	return (AABB) {
		.upper_left = at,
			.lower_right = AddV2(at, V2(size.x, -size.y)),
	};
}

AABB aabb_at_yplusdown(Vec2 at, Vec2 size)
{
	return (AABB) {
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
		to_return.points[i] = AddV2(to_return.points[i], V2(-size.X*0.5f, size.Y*0.5f));
	}
	return to_return;
}

Quad quad_rotated_centered(Vec2 at, Vec2 size, float rotation)
{
	Quad to_return = quad_centered(at, size);
	for(int i = 0; i < 4; i++)
	{
		to_return.points[i] = AddV2(RotateV2(SubV2(to_return.points[i], at), rotation), at);
	}
	return to_return;
}

bool aabb_is_valid(AABB aabb)
{
	Vec2 size_vec = SubV2(aabb.lower_right, aabb.upper_left); // negative in vertical direction
	return size_vec.Y < 0.0f && size_vec.X > 0.0f;
}

// positive in both directions
Vec2 aabb_size(AABB aabb)
{
	assert(aabb_is_valid(aabb));
	Vec2 size_vec = SubV2(aabb.lower_right, aabb.upper_left); // negative in vertical direction
	size_vec.y *= -1.0;
	return size_vec;
}

Quad quad_aabb(AABB aabb)
{
	Vec2 size_vec = SubV2(aabb.lower_right, aabb.upper_left); // negative in vertical direction

	assert(aabb_is_valid(aabb));
	return (Quad) {
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
		{ a.upper_left.X, a.lower_right.X };
		float b_segment[2] =
		{ b.upper_left.X, b.lower_right.X };
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
		{ a.lower_right.Y, a.upper_left.Y };
		float b_segment[2] =
		{ b.lower_right.Y, b.upper_left.Y };
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

bool has_point(AABB aabb, Vec2 point)
{
	return
		(aabb.upper_left.X < point.X && point.X < aabb.lower_right.X) &&
		(aabb.upper_left.Y > point.Y && point.Y > aabb.lower_right.Y);
}

AABB screen_cam_aabb()
{
	return (AABB) { .upper_left = V2(0.0, screen_size().Y), .lower_right = V2(screen_size().X, 0.0) };
}

#define FLOATS_PER_VERTEX (3 + 2)
float cur_batch_data[1024*10] = { 0 };
int cur_batch_data_index = 0;
// @TODO check last tint as well, do this when factor into drawing parameters
sg_image cur_batch_image = { 0 };
threedee_twodee_fs_params_t cur_batch_params = { 0 };
sg_pipeline cur_batch_pipeline = { 0 };
void flush_quad_batch()
{
	if (cur_batch_image.id == 0 || cur_batch_data_index == 0) return; // flush called when image changes, image starts out null!

	if(cur_batch_pipeline.id != 0)
	{
		sg_apply_pipeline(cur_batch_pipeline);
	}
	else
	{
		sg_apply_pipeline(state.twodee_pip);
	}


	state.bind.vertex_buffer_offsets[0] = sg_append_buffer(state.bind.vertex_buffers[0], &(sg_range) { cur_batch_data, cur_batch_data_index*sizeof(*cur_batch_data) });
	state.bind.fs_images[SLOT_threedee_twodee_tex] = cur_batch_image;
	sg_apply_bindings(&state.bind);
	cur_batch_params.tex_size = img_size(cur_batch_image);
	sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_threedee_twodee_fs_params, &SG_RANGE(cur_batch_params));
	cur_batch_params.tex_size = V2(0,0); // unsure if setting the tex_size to something nonzero fucks up the batching so I'm just resetting it back here
	assert(cur_batch_data_index % FLOATS_PER_VERTEX == 0);
	sg_draw(0, cur_batch_data_index / FLOATS_PER_VERTEX, 1);
	num_draw_calls += 1;
	num_vertices += cur_batch_data_index / FLOATS_PER_VERTEX;
	memset(cur_batch_data, 0, cur_batch_data_index*sizeof(*cur_batch_data));
	cur_batch_data_index = 0;
}

typedef enum
{
	LAYER_TILEMAP,
	LAYER_WORLD,
	LAYER_UI,
	LAYER_UI_FG,
	LAYER_SCREENSPACE_EFFECTS,

	LAYER_LAST
} Layer;

typedef BUFF(char, 200) StacktraceElem;
typedef BUFF(StacktraceElem, 16) StacktraceInfo;

StacktraceInfo get_stacktrace()
{
#ifdef WINDOWS
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
#else
	return (StacktraceInfo){0};
#endif
}

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
	//return V4(v.x / v.w, v.y / v.w, v.z / v.w, v.w / v.w);

	// f(v).x = v.x / v.w;
	// output_x = input_x / input_w;


	return V4(divided_point.x * what_was_w, divided_point.y * what_was_w, divided_point.z * what_was_w, what_was_w);
}
Vec3 screenspace_point_to_camera_point(Vec2 screenspace)
{
	/*
	gl_Position = perspective_divide(projection * view * world_space_point);
	inverse_perspective_divide(gl_Position) = projection * view * world_space_point;
	proj_inverse * inverse_perspective_divide(gl_Position) = view * world_space_point;
	view_inverse * proj_inverse * inverse_perspective_divide(gl_Position) = world_space_point;
	*/

	Vec2 clip_space = into_clip_space(screenspace);
	Vec4 output_position = V4(clip_space.x, clip_space.y, -1.0, 1.0);
	float what_was_w = MulM4V4(projection, V4(0,0,-NEAR_PLANE_DISTANCE,1)).w;
	Vec3 to_return = MulM4V4(MulM4(InvGeneralM4(view), InvGeneralM4(projection)), inverse_perspective_divide(output_position, what_was_w)).xyz;

	return to_return;
}

Vec3 ray_intersect_plane(Vec3 ray_point, Vec3 ray_vector, Vec3 plane_point, Vec3 plane_normal)
{
	float d = DotV3(plane_point, MulV3F(plane_normal, -1.0f));

	float denom = DotV3(plane_normal, ray_vector);
	if(fabsf(denom) <= 1e-4f)
	{
		// also could mean doesn't intersect plane
		return plane_point;
	}
	assert(fabsf(denom) > 1e-4f); // avoid divide by zero

    float t = -(DotV3(plane_normal, ray_point) + d) / DotV3(plane_normal, ray_vector);

    if(t <= 1e-4)
    {
    	// means doesn't intersect the plane, I think...
    	return plane_point;
    }
    
    assert(t > 1e-4);

    return AddV3(ray_point, MulV3F(ray_vector, t));
}

typedef BUFF(DrawParams, 1024*5) RenderingQueue;
RenderingQueue rendering_queues[LAYER_LAST] = { 0 };

// The image region is in pixel space of the image
void draw_quad_impl(DrawParams d, int line)
{
	d.line_number = line;
	Vec2 *points = d.quad.points;

	AABB cam_aabb = screen_cam_aabb();
	AABB points_bounding_box = { .upper_left = V2(INFINITY, -INFINITY), .lower_right = V2(-INFINITY, INFINITY) };

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
	DrawParams *a_draw = (DrawParams*)a;
	DrawParams *b_draw = (DrawParams*)b;

	return (int)((a_draw->sorting_key - b_draw->sorting_key));
}

void swap(Vec2 *p1, Vec2 *p2)
{
	Vec2 tmp = *p1;
	*p1 = *p2;
	*p2 = tmp;
}

Vec2 tile_id_to_coord(sg_image tileset_image, Vec2 tile_size, uint16_t tile_id)
{
	int tiles_per_row = (int)(img_size(tileset_image).X / tile_size.X);
	int tile_index = tile_id - 1;
	int tile_image_row = tile_index / tiles_per_row;
	int tile_image_col = tile_index - tile_image_row*tiles_per_row;
	Vec2 tile_image_coord = V2((float)tile_image_col * tile_size.X, (float)tile_image_row*tile_size.Y);
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
	draw_quad((DrawParams) { q, image_white_square, full_region(image_white_square), col, .layer = LAYER_UI });
}


Vec2 NormV2_or_zero(Vec2 v)
{
	if(v.x == 0.0f && v.y == 0.0f)
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
	Vec2 normal = rotate_counter_clockwise(NormV2_or_zero(SubV2(to, from)));

	return (Quad){
		.points = {
			AddV2(from, MulV2F(normal, line_width)),  // upper left
			AddV2(to, MulV2F(normal, line_width)),    // upper right
			AddV2(to, MulV2F(normal, -line_width)),   // lower right
			AddV2(from, MulV2F(normal, -line_width)), // lower left
		}
	};
}

// in world coordinates
void line(Vec2 from, Vec2 to, float line_width, Color color)
{
	colorquad(line_quad(from, to, line_width), color);
}

#ifdef DEVTOOLS
bool show_devtools = true;
#ifdef PROFILING
extern bool profiling;
#else
bool profiling;
#endif
#else
const bool show_devtools = false;
#endif

bool having_errors = false;

Color debug_color = {1,0,0,1};

#define dbgcol(col) DeferLoop(debug_color = col, debug_color = RED)

void dbgsquare(Vec2 at)
{
#ifdef DEVTOOLS
	if (!show_devtools) return;
	colorquad(quad_centered(at, V2(10.0, 10.0)), debug_color);
#else
	(void)at;
#endif
}

void dbgbigsquare(Vec2 at)
{
#ifdef DEVTOOLS
	if (!show_devtools) return;
	colorquad(quad_centered(at, V2(20.0, 20.0)), BLUE);
#else
	(void)at;
#endif
}


void dbgline(Vec2 from, Vec2 to)
{
#ifdef DEVTOOLS
	if (!show_devtools) return;
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
	if (!show_devtools) return;
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
	if(view.Elements[3][3] == 0.0)
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

		if (clip_space_no_perspective_divide.z < 0.0) {
			return V2(0.0, 0.0);
		}

		if(clip_space_no_perspective_divide.w != 0.0)
		{
			clip_space = perspective_divide(clip_space_no_perspective_divide);
		}
		else
		{
			clip_space = clip_space_no_perspective_divide.xyz;
		}

		// clip is from -1 to 1, need to map back to screen
		Vec2 mapped_correctly = V2((clip_space.x + 1.0f)/2.0f, (clip_space.y + 1.0f)/2.0f);

		return V2(mapped_correctly.x * screen_size().x , mapped_correctly.y * screen_size().y);
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

void dbg3dline2d(Vec2 a, Vec2 b)  {
	Vec3 a_3 = V3(a.x, 0.0, a.y);
	Vec3 b_3 = V3(b.x, 0.0, b.y);

	dbg3dline(a_3, b_3);
}

void dbg3dline2dOffset(Vec2 a, Vec2 offset)  {
	Vec3 a_3 = V3(a.x, 0.0, a.y);
	Vec3 b_3 = V3(offset.x, 0.0, offset.y);

	dbg3dline(a_3, AddV3(a_3,b_3));
}


void colorquadplane(Quad q, Color col)
{
	Quad warped = {0};
	for(int i = 0; i < 4; i++)
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
	if(!show_devtools) return;
	colorquadplane(quad_centered(at, V2(3.0,3.0)), debug_color);
}

void dbgplaneline(Vec2 from, Vec2 to)
{
	if(!show_devtools) return;
	dbg3dline(plane_point(from), plane_point(to));
}

void dbgplanevec(Vec2 from, Vec2 vec)
{
	if(!show_devtools) return;
	Vec2 to = AddV2(from, vec);
	dbgplaneline(from, to);
}

void dbgplanerect(AABB aabb)
{
	if(!show_devtools) return;
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
	if(drawn_this_frame_length >= MAXIMUM_THREEDEE_THINGS)
	{
		Log("Drawing too many things!\n");
		drawn_this_frame_length = MAXIMUM_THREEDEE_THINGS - 1;
	}
#endif
}


typedef struct TextParams
{
	bool dry_run;
	MD_String8 text;
	Vec2 pos;
	Color color;
	float scale;
	AABB clip_to; // if in world space, in world space. In space of pos given
	Color *colors; // color per character, if not null must be array of same length as text
	bool do_clipping;
	LoadedFont *use_font; // if null, uses default font
} TextParams;

// returns bounds. To measure text you can set dry run to true and get the bounds
AABB draw_text(TextParams t)
{
	AABB bounds = { 0 };
	LoadedFont font = default_font;
	if(t.use_font) font = *t.use_font;
	PROFILE_SCOPE("draw text")
	{
		size_t text_len = t.text.size;
		float y = 0.0;
		float x = 0.0;
		for (int i = 0; i < text_len; i++)
		{
			stbtt_aligned_quad q;
			float old_y = y;
			PROFILE_SCOPE("get baked quad")
				stbtt_GetBakedQuad(font.cdata, 512, 512, t.text.str[i]-32, &x, &y, &q, 1);
			float difference = y - old_y;
			y = old_y + difference;

			Vec2 size = V2(q.x1 - q.x0, q.y1 - q.y0);
			if (t.text.str[i] == '\n')
			{
#ifdef DEVTOOLS
				y += font.font_size*0.75f; // arbitrary, only debug t.text has newlines
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

				AABB font_atlas_region = (AABB)
				{
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
					for (int i = 0; i < 4; i++)
					{
						bounds.upper_left.X = fminf(bounds.upper_left.X, to_draw.points[i].X);
						bounds.upper_left.Y = fmaxf(bounds.upper_left.Y, to_draw.points[i].Y);
						bounds.lower_right.X = fmaxf(bounds.lower_right.X, to_draw.points[i].X);
						bounds.lower_right.Y = fminf(bounds.lower_right.Y, to_draw.points[i].Y);
					}

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

						draw_quad((DrawParams) { to_draw, font.image, font_atlas_region, col, t.clip_to, .layer = LAYER_UI_FG, .do_clipping = t.do_clipping });
					}
				}
			}
		}

		bounds.upper_left = AddV2(bounds.upper_left, t.pos);
		bounds.lower_right = AddV2(bounds.lower_right, t.pos);
	}

	return bounds;
}

AABB draw_centered_text(TextParams t)
{
	if(t.scale <= 0.01f) return (AABB){0};
	t.dry_run = true;
	AABB text_aabb = draw_text(t);
	t.dry_run = false;
	Vec2 center_pos = t.pos;
	t.pos = AddV2(center_pos, MulV2F(aabb_size(text_aabb), -0.5f));
	return draw_text(t);
}

int sorting_key_at(Vec2 pos)
{
	return -(int)pos.y;
}

// gets aabbs overlapping the input aabb, including gs.entities
Overlapping get_overlapping(AABB aabb)
{
	Overlapping to_return = { 0 };

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
	BUFF(Entity*, 8) with;
}CollisionInfo;

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
	//Assumes we already know that they are colliding.
	//It could be faster to use this info for collision detection as well,
	//but this would require an intrusive refactor, and it is not the common
	//case that things are colliding anyway, so it's actually not that much
	//duplicated work.
	Vec2 dynamic_centre = aabb_center(dynamic);
	Vec2 dynamic_half_dims = MulV2F(aabb_size(dynamic), 0.5f);

	stable.lower_right.x += dynamic_half_dims.x;
	stable.lower_right.y -= dynamic_half_dims.y;
	stable.upper_left.x  -= dynamic_half_dims.x;
	stable.upper_left.y  += dynamic_half_dims.y;

	float right_delta  = stable.lower_right.x - dynamic_centre.x;
	float left_delta   = stable.upper_left.x  - dynamic_centre.x;
	float bottom_delta = stable.lower_right.y - dynamic_centre.y;
	float top_delta    = stable.upper_left.y  - dynamic_centre.y;

	float r = fabsf( right_delta);
	float l = fabsf(  left_delta);
	float b = fabsf(bottom_delta);
	float t = fabsf(   top_delta);

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
	Vec2 collision_aabb_size = entity_aabb_size(p.from);
	Vec2 new_pos = AddV2(p.position, p.movement_this_frame);

	assert(collision_aabb_size.x > 0.0f);
	assert(collision_aabb_size.y > 0.0f);
	AABB at_new = aabb_centered(new_pos, collision_aabb_size);
	typedef struct
	{
		AABB aabb;
		Entity *e; // required
	} CollisionObj;
	BUFF(CollisionObj, 256) to_check = { 0 };

	// add world boxes
	for(CollisionCube *cur = level_threedee.collision_list; cur; cur = cur->next)
	{
		BUFF_APPEND(&to_check, ((CollisionObj){cur->bounds, gs.world_entity}));
	}

	// add entity boxes
	if (!p.dont_collide_with_entities)
	{
		ENTITIES_ITER(gs.entities)
		{
			if (it != p.from && !(it->is_npc && it->dead) && !it->is_world)
			{
				BUFF_APPEND(&to_check, ((CollisionObj){aabb_centered(it->pos, entity_aabb_size(it)), it}));
			}
		}
	}

	// here we do some janky C stuff to resolve collisions with the closest
	// box first, because doing so is a simple heuristic to avoid depenetrating and losing
	// sideways velocity. It's visual and I can't put diagrams in code so uh oh!

	typedef BUFF(CollisionObj, 32) OverlapBuff;
	OverlapBuff actually_overlapping = { 0 };

	BUFF_ITER(CollisionObj, &to_check)
	{
		if (overlapping(at_new, it->aabb))
		{
			BUFF_APPEND(&actually_overlapping, *it);
		}
	}


	float smallest_distance = FLT_MAX;
	int smallest_aabb_index = 0;
	int i = 0;
	BUFF_ITER(CollisionObj, &actually_overlapping)
	{
		float cur_dist = LenV2(SubV2(aabb_center(at_new), aabb_center(it->aabb)));
		if (cur_dist < smallest_distance) {
			smallest_distance = cur_dist;
			smallest_aabb_index = i;
		}
		i++;
	}


	OverlapBuff overlapping_smallest_first = { 0 };
	if (actually_overlapping.cur_index > 0)
	{
		BUFF_APPEND(&overlapping_smallest_first, actually_overlapping.data[smallest_aabb_index]);
	}
	BUFF_ITER_I(CollisionObj, &actually_overlapping, i)
	{
		if (i == smallest_aabb_index)
		{
		}
		else
		{
			BUFF_APPEND(&overlapping_smallest_first, *it);
		}
	}

	// overlapping
	BUFF_ITER(CollisionObj, &overlapping_smallest_first)
	{
		dbgcol(GREEN)
		{
			dbgplanerect(it->aabb);
		}
	}


	//overlapping_smallest_first = actually_overlapping;

	BUFF_ITER(CollisionObj, &actually_overlapping)
		dbgcol(WHITE)
		dbgplanerect(it->aabb);

	BUFF_ITER(CollisionObj, &overlapping_smallest_first)
		dbgcol(WHITE)
		dbgplanesquare(aabb_center(it->aabb));

	CollisionInfo info = { 0 };
	for (int col_iter_i = 0; col_iter_i < 1; col_iter_i++)
		BUFF_ITER(CollisionObj, &overlapping_smallest_first)
		{
			AABB to_depenetrate_from = it->aabb;

			Vec2 resolution_vector = get_penetration_vector(to_depenetrate_from, at_new);
			at_new.upper_left  = AddV2(at_new.upper_left , resolution_vector);
			at_new.lower_right = AddV2(at_new.lower_right, resolution_vector);
			bool happened_with_this_one = true;

			if(happened_with_this_one)
			{
				bool already_in_happened = false;
				Entity *e = it->e;
				if(e)
				{
					BUFF_ITER(Entity *, &info.with)
					{
						if(e == *it)
						{
							already_in_happened = true;
						}
					}
					if(!already_in_happened)
					{
						if(!BUFF_HAS_SPACE(&info.with))
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

	if (p.col_info_out) *p.col_info_out = info;

	Vec2 result_pos = aabb_center(at_new);
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
MD_String8List split_by_word(MD_Arena *arena, MD_String8 string)
{
	MD_String8 word_delimeters[] = { MD_S8Lit(" ") };
	return MD_S8Split(arena, string, ARRLEN(word_delimeters), word_delimeters);
}

typedef struct PlacedWord
{
	struct PlacedWord *next;
	struct PlacedWord *prev;
	MD_String8 text;
	Vec2 lower_left_corner;
	int line_index;
} PlacedWord;

typedef struct
{
	PlacedWord *first;
	PlacedWord *last;
} PlacedWordList;

float get_vertical_dist_between_lines(LoadedFont for_font, float text_scale)
{
	return for_font.font_line_advance*text_scale*0.9f;
}

PlacedWordList place_wrapped_words(MD_Arena *arena, MD_String8List words, float text_scale, float maximum_width, LoadedFont for_font)
{
	PlacedWordList to_return = {0};
	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);

	Vec2 at_position = V2(0.0, 0.0);
	Vec2 cur = at_position;
	float space_size = character_width(for_font, (int)' ', text_scale);
	float current_vertical_offset = 0.0f; // goes negative
	int current_line_index = 0;
	for(MD_String8Node *next_word = words.first; next_word; next_word = next_word->next)
	{
		if(next_word->string.size == 0)
		{
		}
		else
		{
			AABB word_bounds = draw_text((TextParams){true, next_word->string, V2(0.0, 0.0), .scale = text_scale});
			word_bounds.lower_right.x += space_size;
			float next_x_position = cur.x + aabb_size(word_bounds).x;
			if(next_x_position - at_position.x > maximum_width)
			{
				current_vertical_offset -= get_vertical_dist_between_lines(for_font, text_scale);  // the 1.1 is just arbitrary padding because it looks too crowded otherwise
				cur = AddV2(at_position, V2(0.0f, current_vertical_offset));
				current_line_index += 1;
				next_x_position = cur.x + aabb_size(word_bounds).x;
			}

			PlacedWord *new_placed = MD_PushArray(arena, PlacedWord, 1);
			new_placed->text = next_word->string;
			new_placed->lower_left_corner = cur;
			new_placed->line_index = current_line_index;

			MD_DblPushBack(to_return.first, to_return.last, new_placed);

			cur.x = next_x_position;
		}
	}

	MD_ReleaseScratch(scratch);
	return to_return;
}

void translate_words_by(PlacedWordList words, Vec2 translation)
{
	for(PlacedWord *cur = words.first; cur; cur = cur->next)
	{
		cur->lower_left_corner = AddV2(cur->lower_left_corner, translation);
	}
}

MD_String8 last_said_sentence(Entity *npc)
{
	assert(npc->is_npc);

	MD_String8 to_return = (MD_String8){0};

	for(Memory *cur = npc->memories_last; cur; cur = cur->prev)
	{
		if(cur->context.author_npc_kind == npc->npc_kind)
		{
			to_return = TextChunkString8(cur->speech);
			break;
		}
	}

	return to_return;
}

typedef enum
{
	DELEM_NPC,
	DELEM_PLAYER,
	DELEM_ACTION_DESCRIPTION,
} DialogElementKind;

typedef struct
{
	MD_u8 speech[MAX_SENTENCE_LENGTH];
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

	Vec2 *arr = (Vec2*)trail.data;

	int cur = *trail.cur_index - 1;
	while(cur > 0)
	{
		Vec2 from = arr[cur];
		Vec2 to = arr[cur - 1];
		Vec2 cur_segment = SubV2(to, from);
		float len = LenV2(cur_segment);
		if(len < along)
		{
			along -= len;
		}
		else
		{
			return LerpV2(from, along/len, to);
		}
		cur -= 1;
	}
	return arr[*trail.cur_index - 1];
}
float get_total_trail_len(BuffRef trail)
{
	assert(trail.data_elem_size == sizeof(Vec2));

	if(*trail.cur_index <= 1)
	{
		return 0.0f;
	}
	else
	{
		float to_return = 0.0f;
		Vec2 *arr = (Vec2*)trail.data;
		for(int i = 0; i < *trail.cur_index - 1; i++)
		{
			to_return += LenV2(SubV2(arr[i], arr[i+1]));
		}
		return to_return;
	}
}

Vec2 mouse_pos = { 0 }; // in screen space

#define ROLL_KEY SAPP_KEYCODE_LEFT_SHIFT
double elapsed_time = 0.0;
double unwarped_elapsed_time = 0.0;
double last_frame_processing_time = 0.0;
double last_frame_gameplay_processing_time = 0.0;
uint64_t last_frame_time;

typedef struct
{
	bool interact;
	bool mouse_down;
	bool mouse_up;

	bool speak_shortcut;
	bool give_shortcut;
} PressedState;

PressedState pressed = { 0 };
bool mouse_down = false;
float learned_shift = 0.0;
float learned_space = 0.0;
float learned_e = 0.0;
#ifdef DEVTOOLS
bool mouse_frozen = false;
#endif

typedef struct
{
	float pressed_amount; // for buttons, 0.0 is completely unpressed (up), 1.0 is completely depressed (down)
	bool is_being_pressed;
} IMState;

struct { int key; IMState value; } *imui_state = 0;

bool imbutton_key(AABB button_aabb, float text_scale, MD_String8 text, int key, float dt, bool force_down)
{
	IMState state = hmget(imui_state, key);

	float raise = Lerp(0.0f, state.pressed_amount, 5.0f);
	button_aabb.upper_left.y += raise;
	button_aabb.lower_right.y += raise;

	bool to_return = false;
	float pressed_target = 0.5f;
	if (has_point(button_aabb, mouse_pos))
	{
		if (pressed.mouse_down)
		{
			state.is_being_pressed = true;
		}

		pressed_target = 1.0f; // when hovering button like pops out a bit

		if (pressed.mouse_up) to_return = true; // when mouse released, and hovering over button, this is a button press - Lao Tzu
	}
	if (pressed.mouse_up) state.is_being_pressed = false;

	if (state.is_being_pressed || force_down) pressed_target = 0.0f;

	state.pressed_amount = Lerp(state.pressed_amount, dt*20.0f, pressed_target);

	float button_alpha = Lerp(0.5f, state.pressed_amount, 1.0f);

	if (aabb_is_valid(button_aabb))
	{
		draw_quad((DrawParams) { quad_aabb(button_aabb), IMG(image_white_square), blendalpha(WHITE, button_alpha), .layer = LAYER_UI, });
		draw_centered_text((TextParams) { false, text, aabb_center(button_aabb), BLACK, text_scale, .clip_to = button_aabb, .do_clipping = true });
	}

	hmput(imui_state, key, state);
	return to_return;
}

#define imbutton(...) imbutton_key(__VA_ARGS__, __LINE__, unwarped_dt, false)

Quat rot_on_plane_to_quat(float rot)
{
	return QFromAxisAngle_RH(V3(0,1,0), AngleRad(-rot));
}

Transform entity_transform(Entity *e)
{
	// Models must face +X in blender. This is because, in the 2d game coordinate system,
	// a zero degree 2d rotation means you're facing +x, and this is how it is in the game logic.
	// The rotation is negative for some reason that I'm not quite sure about though, something about
	// the handedness of the 3d coordinate system not matching the handedness of the 2d coordinate system
	Quat entity_rot = rot_on_plane_to_quat(e->rotation);

	return (Transform){.offset = AddV3(plane_point(e->pos), V3(0,0,0)), .rotation = entity_rot, .scale = V3(1, 1, 1)};
	/*
	(void)entity_rot;
	return (Transform){.offset = AddV3(plane_point(e->pos), V3(0,0,0)), .rotation = Make_Q(0,0,0,1), .scale = V3(1, 1, 1)};
	*/
}


Shadow_State init_shadow_state() {
	//To start off with, most of this initialisation code is taken from the
	// sokol shadows sample, which can be found here. 
	// https://floooh.github.io/sokol-html5/shadows-sapp.html

	Shadow_State shadows = {0};

	shadows.pass_action = (sg_pass_action) {
		.colors[0] = {
			.action = SG_ACTION_CLEAR,
			.value = { 1.0f, 1.0f, 1.0f, 1.0f }
		}
	};

	/*
		 As of right now, it looks like sokol_gfx does not support depth only
		 rendering passes, so we create the colour buffer always. It will likely
		 be pertinent to just dig into sokol and add the functionality we want later,
		 but as a first pass, we will just do as the romans do. I.e. have both a colour
		 and depth component. - Canada Day 2023.
		 */
	sg_image_desc img_desc = {
		.render_target = true,
		.width = SHADOW_MAP_DIMENSION,
		.height = SHADOW_MAP_DIMENSION,
		.pixel_format = SG_PIXELFORMAT_RGBA8,
		.min_filter = SG_FILTER_LINEAR,
		.mag_filter = SG_FILTER_LINEAR,
		.wrap_u = SG_WRAP_CLAMP_TO_BORDER,
		.wrap_v = SG_WRAP_CLAMP_TO_BORDER,
		.border_color = SG_BORDERCOLOR_OPAQUE_WHITE,
		.sample_count = 1,
		.label = "shadow-map-color-image"
	};
	shadows.color_img = sg_make_image(&img_desc);
	img_desc.pixel_format = SG_PIXELFORMAT_DEPTH; // @TODO @URGENT replace depth with R8, I think depth isn't always safe on Webgl1 according to sg_gfx header. Also replace other instances of this in codebase
	img_desc.label = "shadow-map-depth-image";
	shadows.depth_img = sg_make_image(&img_desc);
	shadows.pass = sg_make_pass(&(sg_pass_desc){
			.color_attachments[0].image = shadows.color_img,
			.depth_stencil_attachment.image = shadows.depth_img,
			.label = "shadow-map-pass"
			});


	sg_pipeline_desc desc = (sg_pipeline_desc){
		.layout = {
			.attrs = {
				[ATTR_threedee_vs_pos_in].format = SG_VERTEXFORMAT_FLOAT3,
				[ATTR_threedee_vs_uv_in].format = SG_VERTEXFORMAT_FLOAT2,
			}
		},
			.shader = sg_make_shader(threedee_mesh_shadow_mapping_shader_desc(sg_query_backend())),
			// Cull front faces in the shadow map pass
			// .cull_mode = SG_CULLMODE_BACK,
			.sample_count = 1,
			.depth = {
				.pixel_format = SG_PIXELFORMAT_DEPTH,
				.compare = SG_COMPAREFUNC_LESS_EQUAL,
				.write_enabled = true,
			},
			.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
			.label = "shadow-map-pipeline"
	};

	shadows.pip = sg_make_pipeline(&desc);

	desc.label = "armature-shadow-map-pipeline";
	desc.shader = sg_make_shader(threedee_armature_shadow_mapping_shader_desc(sg_query_backend()));
	sg_vertex_attr_desc skeleton_vertex_attrs[] = {
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

typedef struct {
	float l; 
	float r; 
	float t; 
	float b; 
	float n;
	float f;
} Shadow_Volume_Params;

float round_to_nearest(float input, float round_target)
{
    float result = 0.0f;
    if(round_target != 0.0f)
    {
        float div = roundf(input / round_target);
        result = div * round_target;
    }
    return result;
}


typedef struct
{
	//For now we consider all vertices on the near plane to be equal to the camera position, and store that at vertices[0];
	Vec3 vertices[5];
} FrustumVertices;

FrustumVertices get_frustum_vertices(Vec3 cam_pos, Vec3 cam_forward, Vec3 cam_right) {
	FrustumVertices result = {0};

	float aspect_ratio = (float)sapp_width() / (float)sapp_height();


	const float cascade_distance = FAR_PLANE_DISTANCE;

	Vec2 far_plane_half_dims;
	far_plane_half_dims.y = cascade_distance * tanf(FIELD_OF_VIEW * 0.5f);
	far_plane_half_dims.x = far_plane_half_dims.y * aspect_ratio;

	Vec3 cam_up = Cross(cam_right, cam_forward); 

	Vec3 far_plane_centre = AddV3(cam_pos, MulV3F(cam_forward, cascade_distance));
	Vec3 far_plane_offset_to_right_side  = MulV3F(cam_right, far_plane_half_dims.x);
	Vec3 far_plane_offset_to_top_side    = MulV3F(cam_up   , far_plane_half_dims.y);
	Vec3 far_plane_offset_to_left_side   = MulV3F(far_plane_offset_to_right_side, -1.0); 
	Vec3 far_plane_offset_to_bot_side    = MulV3F(far_plane_offset_to_top_side  , -1.0);   

	result.vertices[0] = cam_pos;
	result.vertices[1] = AddV3(far_plane_centre, AddV3(far_plane_offset_to_bot_side, far_plane_offset_to_left_side ));
	result.vertices[2] = AddV3(far_plane_centre, AddV3(far_plane_offset_to_bot_side, far_plane_offset_to_right_side));
	result.vertices[3] = AddV3(far_plane_centre, AddV3(far_plane_offset_to_top_side, far_plane_offset_to_right_side));
	result.vertices[4] = AddV3(far_plane_centre, AddV3(far_plane_offset_to_top_side, far_plane_offset_to_left_side ));


	return result;
}

Shadow_Volume_Params calculate_shadow_volume_params(Vec3 light_dir, Vec3 cam_pos, Vec3 cam_forward, Vec3 cam_right) 
{
	Shadow_Volume_Params result = {0};


	//first, we calculate the scene bound
	//NOTE: Once we are moved to a pre-pass queue making type deal, this could be moved into that 
	//      loop.

	//For simplicity and speed, at the moment we consider only entity positions, not their extents when constructing the scene bounds.
	//To make up for this, we add an extra padding-skirt to the bounds.
	Mat4 light_space_matrix = LookAt_RH((Vec3){0}, light_dir, V3(0, 1, 0));

	Vec3 frustum_min = V3( INFINITY,  INFINITY,  INFINITY);
	Vec3 frustum_max = V3(-INFINITY, -INFINITY, -INFINITY);

	FrustumVertices frustum_vertices_worldspace = get_frustum_vertices(cam_pos, cam_forward, cam_right);
	const int num_frustum_vertices = sizeof(frustum_vertices_worldspace.vertices)/sizeof(frustum_vertices_worldspace.vertices[0]);

	for (int i = 0; i < num_frustum_vertices; ++i) {
		Vec3 p = frustum_vertices_worldspace.vertices[i];

		p = MulM4V3(light_space_matrix, p);

		frustum_min.x = fminf(frustum_min.x, p.x);
		frustum_max.x = fmaxf(frustum_max.x, p.x);

		frustum_min.y = fminf(frustum_min.y, p.y);
		frustum_max.y = fmaxf(frustum_max.y, p.y);
		
		frustum_min.z = fminf(frustum_min.z, p.z);
		frustum_max.z = fmaxf(frustum_max.z, p.z);	
	}



	result.l = frustum_min.x;
	result.r = frustum_max.x;

	result.b = frustum_min.y;
	result.t = frustum_max.y;

	float w = result.r - result.l;
	float h = result.t - result.b;
	float actual_size = fmaxf(w, h);

	{//Make sure it is square
		float diff = actual_size - h;
		if (diff > 0) {
			float half_diff = diff * 0.5f;
			result.t += half_diff;
			result.b -= half_diff;
		}
		diff = actual_size - w;
		if (diff > 0) {
			float half_diff = diff * 0.5f;
			result.r += half_diff;
			result.l -= half_diff;
		}
	}


	{//Snap the light position to shadow_map texel grid, to reduce shimmering when moving
		float texel_size = actual_size / (float)SHADOW_MAP_DIMENSION;
		result.l = round_to_nearest(result.l, texel_size);
		result.r = round_to_nearest(result.r, texel_size);
		result.b = round_to_nearest(result.b, texel_size);
		result.t = round_to_nearest(result.t, texel_size);
	}

	result.n = -100.0;
	result.f = 200.0;


	return result;
}


void debug_draw_img(sg_image img, int index) {
    draw_quad((DrawParams){quad_at(V2(512.0f*index, 512.0), V2(512.0, 512.0)), IMG(img), WHITE, .layer=LAYER_UI});
}

void debug_draw_img_with_border(sg_image img, int index) {
    float bs = 50.0;
    draw_quad((DrawParams){quad_at(V2(512.0f*index, 512.0), V2(512.0, 512.0)), img, (AABB){V2(-bs, -bs), AddV2(img_size(img), V2(bs, bs))}, WHITE, .layer=LAYER_UI});
}

void debug_draw_shadow_info(Vec3 frustum_tip, Vec3 cam_forward, Vec3 cam_right, Mat4 light_space_matrix) {
		debug_draw_img(state.shadows.color_img, 0);
		FrustumVertices fv = get_frustum_vertices(frustum_tip, cam_forward, cam_right);

		Vec2 projs[5];
		for (int i = 0; i < 5; ++i) {
			Vec3 v = fv.vertices[i];
			Vec4 p = V4(v.x, v.y, v.z, 1.0);
			Vec4 proj = MulM4V4(light_space_matrix, p);
			proj.x /= proj.w;
			proj.y /= proj.w;
			proj.z /= proj.w;

			proj.x *= 0.5f;
			proj.x += 0.5f;
			proj.y *= 0.5f;
			proj.y += 0.5f;
			proj.z *= 0.5f;
			proj.z += 0.5f;

			proj.x *= 512.0f;
			proj.y *= 512.0f;

			projs[i] = proj.XY;
			dbgsquare(proj.XY);
		}

		dbgline(projs[0], projs[1]);
		dbgline(projs[0], projs[2]);
		dbgline(projs[0], projs[3]);
		dbgline(projs[0], projs[4]);
		dbgline(projs[1], projs[2]);
		dbgline(projs[2], projs[3]);
		dbgline(projs[3], projs[4]);
		dbgline(projs[4], projs[1]);
}

void actually_draw_thing(DrawnThing *it, Mat4 light_space_matrix, bool for_outline)
{
	int num_vertices_to_draw = 0;
	if(it->mesh)
	{
		if(for_outline)
			sg_apply_pipeline(state.outline_mesh_pip);
		else
			sg_apply_pipeline(state.threedee_pip);
		sg_bindings bindings = {0};
		bindings.fs_images[SLOT_threedee_tex] = it->mesh->image;
		if(!for_outline)
			bindings.fs_images[SLOT_threedee_shadow_map] = state.shadows.color_img;
		bindings.vertex_buffers[0] = it->mesh->loaded_buffer;
		sg_apply_bindings(&bindings);

		Mat4 model = transform_to_matrix(it->t);
		threedee_vs_params_t vs_params = {
			.model = model,
			.view =	 view,
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
	else if(it->armature)
	{
		if(for_outline)
			sg_apply_pipeline(state.outline_armature_pip);
		else
			sg_apply_pipeline(state.armature_pip);
		sg_bindings bindings = {0};
		bindings.vs_images[SLOT_threedee_bones_tex] = it->armature->bones_texture;
		bindings.fs_images[SLOT_threedee_tex] = it->armature->image;
		if(!for_outline)
			bindings.fs_images[SLOT_threedee_shadow_map] = state.shadows.color_img;
		bindings.vertex_buffers[0] = it->armature->loaded_buffer;
		sg_apply_bindings(&bindings);

		Mat4 model = transform_to_matrix(it->t);
		threedee_skeleton_vs_params_t params = {
			.model = model,
			.view = view,
			.projection = projection,
			.directional_light_space_matrix = light_space_matrix,
			.bones_tex_size = V2((float)it->armature->bones_texture_width,(float)it->armature->bones_texture_height),
		};
		sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_threedee_skeleton_vs_params, &SG_RANGE(params));
		num_vertices_to_draw = (int)it->armature->vertices_length;
	}
	else
		assert(false);


	if(!for_outline)
	{
		threedee_fs_params_t fs_params = {0};
		fs_params.shadow_map_dimension = SHADOW_MAP_DIMENSION;
		if(it->no_dust)
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

// I moved this out into its own separate function so that you could
// define helper functions to be used multiple times in it, and those functions
// would be near the actual 3d drawing in the file
// @Place(the actual 3d rendering)
void flush_all_drawn_things(Vec3 light_dir, Vec3 cam_pos, Vec3 cam_facing, Vec3 cam_right)
{
	// Draw all the 3D drawn things. Draw the shadows, then draw the things with the shadows.
	// Process armatures and upload their skeleton textures
	{

		// Animate armatures, and upload their bone textures. Also debug draw their skeleton
		{
			SLICE_ITER(DrawnThing, drawn_this_frame)
			{
				if(it->armature)
				{
					MD_ArenaTemp scratch = MD_GetScratch(0, 0);
					Armature *armature = it->armature;
					int bones_tex_size = 4 * armature->bones_texture_width * armature->bones_texture_height;
					MD_u8 *bones_tex = MD_ArenaPush(scratch.arena, bones_tex_size);

					for(MD_u64 i = 0; i < armature->bones_length; i++)
					{
						Bone *cur = &armature->bones[i];

						// for debug drawing
						Vec3 from = MulM4V3(cur->matrix_local, V3(0,0,0));
						Vec3 x = MulM4V3(cur->matrix_local, V3(cur->length,0,0));
						Vec3 y = MulM4V3(cur->matrix_local, V3(0,cur->length,0));
						Vec3 z = MulM4V3(cur->matrix_local, V3(0,0,cur->length));

						Mat4 final = M4D(1.0f);
						final = MulM4(cur->inverse_model_space_pos, final);
						for(Bone *cur_in_hierarchy = cur; cur_in_hierarchy; cur_in_hierarchy = cur_in_hierarchy->parent)
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

						for(int col = 0; col < 4; col++)
						{
							Vec4 to_upload = final.Columns[col];

							int bytes_per_pixel = 4;
							int bytes_per_column_of_mat = bytes_per_pixel * 4;
							int bytes_per_row = bytes_per_pixel * armature->bones_texture_width;
							for(int elem = 0; elem < 4; elem++)
							{
								float after_decoding = decode_normalized_float32(encode_normalized_float32(to_upload.Elements[elem]));
								assert(fabsf(after_decoding - to_upload.Elements[elem]) < 0.01f);
							}
							memcpy(&bones_tex[bytes_per_column_of_mat*col + bytes_per_row*i + bytes_per_pixel*0], encode_normalized_float32(to_upload.Elements[0]).rgba, bytes_per_pixel);
							memcpy(&bones_tex[bytes_per_column_of_mat*col + bytes_per_row*i + bytes_per_pixel*1], encode_normalized_float32(to_upload.Elements[1]).rgba, bytes_per_pixel);
							memcpy(&bones_tex[bytes_per_column_of_mat*col + bytes_per_row*i + bytes_per_pixel*2], encode_normalized_float32(to_upload.Elements[2]).rgba, bytes_per_pixel);
							memcpy(&bones_tex[bytes_per_column_of_mat*col + bytes_per_row*i + bytes_per_pixel*3], encode_normalized_float32(to_upload.Elements[3]).rgba, bytes_per_pixel);
						}
					}

					sg_update_image(armature->bones_texture, &(sg_image_data){
							.subimage[0][0] = (sg_range){bones_tex, bones_tex_size},
							});

					MD_ReleaseScratch(scratch);
				}
			}
		}

		// do the shadow pass
		Mat4 light_space_matrix;
		{
			Shadow_Volume_Params svp = calculate_shadow_volume_params(light_dir, cam_pos, cam_facing, cam_right);

			Mat4 shadow_view       = LookAt_RH(V3(0, 0, 0), light_dir, V3(0, 1, 0));
			Mat4 shadow_projection = Orthographic_RH_NO(svp.l, svp.r, svp.b, svp.t, svp.n, svp.f);
			light_space_matrix = MulM4(shadow_projection, shadow_view);
			//debug_draw_shadow_info(cam_pos, cam_facing, cam_right, light_space_matrix);

			sg_begin_pass(state.shadows.pass, &state.shadows.pass_action);

			// shadows for meshes
			sg_apply_pipeline(state.shadows.pip);
			SLICE_ITER(DrawnThing, drawn_this_frame)
			{
				assert(it->mesh || it->armature);
				if(it->dont_cast_shadows) continue;
				if(it->mesh)
				{
					sg_bindings bindings = {0};
					bindings.fs_images[SLOT_threedee_tex] = it->mesh->image;
					bindings.vertex_buffers[0] = it->mesh->loaded_buffer;
					sg_apply_bindings(&bindings);

					Mat4 model = transform_to_matrix(it->t);
					threedee_vs_params_t vs_params = {
						.model = model,
						.view = shadow_view,
						.projection = shadow_projection,
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
				if(it->armature)
				{
					sg_bindings bindings = {0};
					bindings.vs_images[SLOT_threedee_bones_tex] = it->armature->bones_texture;
					bindings.fs_images[SLOT_threedee_tex] = it->armature->image;
					bindings.vertex_buffers[0] = it->armature->loaded_buffer;
					sg_apply_bindings(&bindings);

					Mat4 model = transform_to_matrix(it->t);
					threedee_skeleton_vs_params_t params = {
						.model = model,
						.view = shadow_view,
						.projection = shadow_projection,
						.directional_light_space_matrix = light_space_matrix,
						.bones_tex_size = V2((float)it->armature->bones_texture_width,(float)it->armature->bones_texture_height),
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
			sg_begin_pass(state.outline_pass, &(sg_pass_action) {
				.colors[0] = {
					.action = SG_ACTION_CLEAR,
					.value = { 0.0f, 0.0f, 0.0f, 0.0f },
				}
			});

			SLICE_ITER(DrawnThing, drawn_this_frame)
			{
				if(it->outline)
				{
					actually_draw_thing(it, light_space_matrix, true);
				}
			}

			sg_end_pass();
		}

		// actually draw, IMPORTANT after this drawn_this_frame is zeroed out!
		{
			sg_begin_pass(state.threedee_pass, &state.clear_everything_pass_action);

			// draw meshes
			SLICE_ITER(DrawnThing, drawn_this_frame)
			{
				if(it->alpha_blend) continue;
				if(it->mesh) actually_draw_thing(it, light_space_matrix, false);
			}

			// draw armatures armature rendering 
			SLICE_ITER(DrawnThing, drawn_this_frame)
			{
				if(it->armature)
				{
					assert(!it->alpha_blend); // too lazy to implement this right now
					actually_draw_thing(it, light_space_matrix, false);
				}
			}

			// draw transparent
			SLICE_ITER(DrawnThing, drawn_this_frame)
			{
				if(it->alpha_blend)
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

// Unsaid words are still there, so you gotta handle the animation homie
MD_String8List words_on_current_page(Entity *it)
{
	MD_String8 last = last_said_sentence(it);
	PlacedWordList placed = place_wrapped_words(frame_arena, split_by_word(frame_arena, last), BUBBLE_TEXT_SCALE, BUBBLE_TEXT_WIDTH_PIXELS, BUBBLE_FONT);

	MD_String8List on_current_page = {0};
	for(PlacedWord *cur = placed.first; cur; cur = cur->next)
	{
		if(cur->line_index / BUBBLE_LINES_PER_PAGE == it->cur_page_index)
			MD_S8ListPush(frame_arena, &on_current_page, cur->text);	
	}

	return on_current_page;

	//return place_wrapped_words(frame_arena, on_current_page, text_scale, aabb_size(placing_text_in).x, default_font);
}

MD_String8List words_on_current_page_without_unsaid(Entity *it)
{
	MD_String8List all_words = words_on_current_page(it);
	int index = 0;
	MD_String8List to_return = {0};
	for(MD_String8Node *cur = all_words.first; cur; cur = cur->next)
	{
		if(index > it->words_said_on_page)
			break;
		MD_S8ListPush(frame_arena, &to_return, cur->string);
		index += 1;
	}
	return to_return;
}

void frame(void)
{
	static float speed_factor = 1.0f;
	// elapsed_time
	double unwarped_dt_double = 0.0;
	{
		unwarped_dt_double = stm_sec(stm_diff(stm_now(), last_frame_time));
		unwarped_dt_double = fmin(unwarped_dt_double, MINIMUM_TIMESTEP * 5.0); // clamp dt at maximum 5 frames, avoid super huge dt
		elapsed_time += unwarped_dt_double*speed_factor;
		unwarped_elapsed_time += unwarped_dt_double;
		last_frame_time = stm_now();
	}
	double dt_double = unwarped_dt_double*speed_factor;
	float unwarped_dt = (float)unwarped_dt_double;
	float dt = (float)dt_double;

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

		text_input_fade = Lerp(text_input_fade, unwarped_dt * 8.0f, receiving_text_input ? 1.0f : 0.0f);

		Vec3 player_pos = V3(gs.player->pos.x, 0.0, gs.player->pos.y);
		//dbgline(V2(0,0), V2(500, 500));
		const float vertical_to_horizontal_ratio = CAM_VERTICAL_TO_HORIZONTAL_RATIO;
		const float cam_distance = CAM_DISTANCE;
		Vec3 away_from_player;
		{
			float ratio = vertical_to_horizontal_ratio;
			float x = sqrtf( (cam_distance * cam_distance) / (1 + (ratio*ratio)) );
			float y = ratio * x;
			away_from_player = V3(x, y, 0.0);
		}
		away_from_player = MulM4V4(Rotate_RH(-PI32/3.0f + PI32, V3(0,1,0)), IsPoint(away_from_player)).xyz;
		Vec3 cam_pos = AddV3(player_pos, away_from_player);

		Vec2 movement = { 0 };
		bool interact = false;
		if (mobile_controls)
		{
			movement = SubV2(thumbstick_nub_pos, thumbstick_base_pos);
			if (LenV2(movement) > 0.0f)
			{
				movement = MulV2F(NormV2(movement), LenV2(movement) / (thumbstick_base_size()*0.5f));
			}
			interact = pressed.interact;
		}
		else
		{
			movement = V2(
					(float)keydown[SAPP_KEYCODE_D] - (float)keydown[SAPP_KEYCODE_A],
					(float)keydown[SAPP_KEYCODE_W] - (float)keydown[SAPP_KEYCODE_S]
					);
			interact = pressed.interact;
		}
		if (LenV2(movement) > 1.0)
		{
			movement = NormV2(movement);
		}


		Vec3 light_dir;
		{
			float spin_factor = 0.0f;
			float t = (float)elapsed_time * spin_factor;

			float x = cosf(t);
			float z = sinf(t);

			light_dir = NormV3(V3(x, -0.5f, z));
		}



		// make movement relative to camera forward
		Vec3 facing = NormV3(SubV3(player_pos, cam_pos));
		Vec3 right = Cross(facing, V3(0,1,0));
		Vec2 forward_2d = NormV2(V2(facing.x, facing.z));
		Vec2 right_2d = NormV2(V2(right.x, right.z));
		movement = AddV2(MulV2F(forward_2d, movement.y), MulV2F(right_2d, movement.x));

		if(flycam)
			movement = V2(0,0);

		view = Translate(V3(0.0, 1.0, -5.0f));
		//view = LookAt_RH(V3(0,1,-5
		if(flycam)
		{
			Basis basis = flycam_basis();
			view = LookAt_RH(flycam_pos, AddV3(flycam_pos, basis.forward), V3(0, 1, 0));
			//view = flycam_matrix();
		}
		else
		{
			view = LookAt_RH(cam_pos, player_pos, V3(0, 1, 0));
		}

		projection = Perspective_RH_NO(FIELD_OF_VIEW, screen_size().x / screen_size().y, NEAR_PLANE_DISTANCE, FAR_PLANE_DISTANCE);


		// @Place(draw 3d things)

		for(PlacedMesh *cur = level_threedee.placed_mesh_list; cur; cur = cur->next)
		{
			float seed = (float)((int64_t)cur % 1024);

			DrawnThing call = (DrawnThing){.mesh = cur->draw_with, .t = cur->t};
			if(MD_S8Match(cur->name, MD_S8Lit("Ground"), 0))
				call.no_dust = true;

			call.no_dust = true;
			float helicopter_offset = (float)sin(elapsed_time*0.5f)*0.5f;
			if(MD_S8Match(cur->name, MD_S8Lit("HelicopterBlade"), 0))
			{
				call.t.offset.y += helicopter_offset;
				call.t.rotation = QFromAxisAngle_RH(V3(0,1,0), (float)elapsed_time * 15.0f);
			}
			if(MD_S8Match(cur->name, MD_S8Lit("BlurryBlade"), 0))
			{
				call.t.rotation = QFromAxisAngle_RH(V3(0,1,0), (float)elapsed_time * 15.0f);
				call.t.offset.y += helicopter_offset;
				call.alpha_blend = true;
				call.dont_cast_shadows = true;
			}
			if(MD_S8Match(cur->name, MD_S8Lit("HelicopterBody"), 0))
			{
				call.t.offset.y += helicopter_offset;
			}
			if(MD_S8FindSubstring(cur->name, MD_S8Lit("Bush"), 0, 0) == 0)
			{
				call.wobble_factor = 1.0f;
				call.seed = seed;
			}
			draw_thing(call);
		}

		ENTITIES_ITER(gs.entities)
		{
			if(it->is_npc || it->is_character)
			{
				Transform draw_with = entity_transform(it);

				if(it->npc_kind == NPC_Player)
				{
					draw_thing((DrawnThing){.armature = &player_armature, .t = draw_with});
				}
				else 
				{
					assert(it->is_npc);
					Armature *to_use = 0;

					if(it->npc_kind == NPC_Daniel)
						to_use = &farmer_armature;
					else if(it->npc_kind == NPC_Raphael)
						to_use = &man_in_black_armature;
					else
						assert(false);

					if(LenV2(it->vel) > 0.5f)
						to_use->go_to_animation = MD_S8Lit("Running");
					else
						to_use->go_to_animation = MD_S8Lit("Idle");

					draw_thing((DrawnThing){.armature = to_use, .t = draw_with, .outline = gete(gs.player->interacting_with) == it});

					if(gete(it->aiming_shotgun_at))
					{
						Transform shotgun_t = draw_with;
						shotgun_t.offset.y += 0.7f;
						shotgun_t.scale = V3(4,4,4);
						shotgun_t.rotation = rot_on_plane_to_quat(AngleOfV2(SubV2(gete(it->aiming_shotgun_at)->pos, it->pos)));
						draw_thing((DrawnThing){.mesh = &mesh_shotgun, .t = shotgun_t});
					}
				}
			}
		}

		// progress the animation, then blend the two animations if necessary, and finally
		// output into anim_blended_poses
		ARR_ITER(Armature*, armatures)
		{
			Armature *cur = *it;
			float seed = (float)((int64_t)cur % 1024); // offset into elapsed time to make all of their animations out of phase
			float along_current_animation = 0.0;
			if(cur->currently_playing_isnt_looping)
			{
				along_current_animation = (float)cur->cur_animation_time;
				cur->cur_animation_time += dt;
			}
			else
			{
				along_current_animation = (float)elapsed_time + seed;
			}

			if(cur->go_to_animation.size > 0)
			{
				if(MD_S8Match(cur->go_to_animation, cur->currently_playing_animation, 0))
				{
				}
				else
				{
					memcpy(cur->current_poses, cur->anim_blended_poses, cur->bones_length * sizeof(*cur->current_poses));
					cur->currently_playing_animation = cur->go_to_animation;
					cur->animation_blend_t = 0.0f;
					cur->go_to_animation = (MD_String8){0};
					if(cur->next_animation_isnt_looping)
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

			if(cur->animation_blend_t < 1.0f)
			{
				cur->animation_blend_t += dt / ANIMATION_BLEND_TIME;
				
				Animation *to_anim = get_anim_by_name(cur, cur->currently_playing_animation);
				assert(to_anim);

				for(MD_u64 i = 0; i < cur->bones_length; i++)
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
				for(MD_u64 i = 0; i < cur->bones_length; i++)
				{
					cur->anim_blended_poses[i] = get_animated_bone_transform(&cur_anim->tracks[i], along_current_animation, cur->currently_playing_isnt_looping);
				}
			}
		}

		flush_all_drawn_things(light_dir, cam_pos, facing, right);

		// draw the 3d render
		draw_quad((DrawParams){quad_at(V2(0.0, screen_size().y), screen_size()), IMG(state.threedee_pass_image), WHITE, .layer = LAYER_WORLD, .custom_pipeline = state.twodee_colorcorrect_pip });
		draw_quad((DrawParams){quad_at(V2(0.0, screen_size().y), screen_size()), IMG(state.outline_pass_image), WHITE, .layer = LAYER_UI_FG, .custom_pipeline = state.twodee_outline_pip, .layer = LAYER_UI});

		// 2d drawing TODO move this to when the drawing is flushed.
		sg_begin_default_pass(&state.clear_depth_buffer_pass_action, sapp_width(), sapp_height());
		sg_apply_pipeline(state.twodee_pip);

		// @Place(text input drawing)
		draw_quad((DrawParams){quad_at(V2(0,screen_size().y), screen_size()), IMG(image_white_square), blendalpha(BLACK, text_input_fade*0.3f), .layer = LAYER_UI_FG});
		Vec2 edge_of_text = MulV2F(screen_size(), 0.5f);
		if(text_input_buffer_length > 0)
		{
			AABB bounds = draw_centered_text((TextParams){false, MD_S8(text_input_buffer, text_input_buffer_length), MulV2F(screen_size(), 0.5f), blendalpha(WHITE, text_input_fade), 1.0f, .use_font = &font_for_text_input});
			edge_of_text = bounds.lower_right;
		}
		Vec2 cursor_center = V2(edge_of_text.x,screen_size().y/2.0f);
		draw_quad((DrawParams){quad_centered(cursor_center, V2(3.0f, 80.0f)), IMG(image_white_square), blendalpha(WHITE, text_input_fade * (sinf((float)elapsed_time*8.0f)/2.0f + 0.5f)), .layer = LAYER_UI_FG});

		// Draw Tilemap draw tilemap tilemap drawing
#if 0
		PROFILE_SCOPE("tilemap")
		{
			Vec2 starting_world = AddV2(world_cam_aabb().upper_left, V2(-TILE_SIZE, TILE_SIZE));
			Vec2 ending_world = AddV2(world_cam_aabb().lower_right, V2(TILE_SIZE, -TILE_SIZE));

			TileCoord starting_point = world_to_tilecoord(starting_world);
			TileCoord ending_point = world_to_tilecoord(ending_world);

			int starting_row = starting_point.y;
			int ending_row = ending_point.y;

			int starting_col = starting_point.x;
			int ending_col = ending_point.x;

			for (int layer = 0; layer < LAYERS; layer++)
			{
				for (int row = starting_row; row < ending_row; row++)
				{
					for (int col = starting_col; col < ending_col; col++)
					{
						TileCoord cur_coord = { col, row };
						TileInstance cur = get_tile_layer(cur_level, layer, cur_coord);

						int tileset_i = 0;
						uint16_t max_gid = 0;
						for (int i = 0; i < ARRLEN(tilesets); i++)
						{
							TileSet tileset = tilesets[i];
							if (cur.kind > tileset.first_gid && tileset.first_gid > max_gid)
							{
								tileset_i = i;
								max_gid = tileset.first_gid;
							}
						}

						TileSet tileset = tilesets[tileset_i];
						cur.kind -= tileset.first_gid - 1;

						if (cur.kind != 0)
						{
							Vec2 tile_size = V2(TILE_SIZE, TILE_SIZE);

							sg_image tileset_image = *tileset.img;

							Vec2 tile_image_coord = tile_id_to_coord(tileset_image, tile_size, cur.kind);

							AnimatedTile *anim = NULL;
							for (int i = 0; i < sizeof(tileset.animated) / sizeof(*tileset.animated); i++)
							{
								if (tileset.animated[i].exists && tileset.animated[i].id_from == cur.kind-1)
								{
									anim = &tileset.animated[i];
								}
							}
							if (anim)
							{
								double time_per_frame = 0.1;
								int frame_index = (int)(elapsed_time / time_per_frame) % anim->num_frames;
								tile_image_coord = tile_id_to_coord(tileset_image, tile_size, anim->frames[frame_index] + 1);
							}

							AABB region;
							region.upper_left = tile_image_coord;
							region.lower_right = AddV2(region.upper_left, tile_size);

							draw_quad((DrawParams) { true, tile_quad(cur_coord), tileset_image, region, WHITE, .layer = LAYER_TILEMAP });
						}
					}
				}
			}
		}
#endif

		Entity *cur_unread_entity = 0;
		uint64_t earliest_unread_time = gs.tick;
		ENTITIES_ITER(gs.entities)
		{
			if(it->is_npc && it->undismissed_action && it->undismissed_action_tick < earliest_unread_time)
			{
				earliest_unread_time = it->undismissed_action_tick;
				cur_unread_entity = it;
			}
		}


		// @Place(UI rendering that happens before gameplay processing so can consume events before the gameplay needs them)
		PROFILE_SCOPE("Entity UI Rendering")
		{


			ENTITIES_ITER(gs.entities)
			{
				if (it->is_npc)
				{
					if(it->undismissed_action)
					{
						assert(it->undismissed_action_tick <= gs.tick); // no future undismissed actions
					}

					const float text_scale = BUBBLE_TEXT_SCALE;
					float dist = LenV2(SubV2(it->pos, gs.player->pos));
					float bubble_factor = 1.0f - clamp01(dist / 6.0f);
					Vec3 bubble_pos = AddV3(plane_point(it->pos), V3(0, 1.7f, 0)); // 1.7 meters is about 5'8", average person height
					Vec2 head_pos = threedee_to_screenspace(bubble_pos);
					Vec2 screen_pos = head_pos;
					Vec2 size = V2(BUBBLE_WIDTH_PIXELS, BUBBLE_WIDTH_PIXELS);
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
					MD_String8List words_to_say = words_on_current_page(it);
					if (unread)
					{
						draw_quad((DrawParams){
								quad_centered(AddV2(bubble_center, V2(size.x * 0.4f, -32.0f + (float)sin(unwarped_elapsed_time * 2.0) * 10.0f)), V2(32, 32)),
								IMG(image_unread_triangle),
								blendalpha(WHITE, 0.8f),
								.layer = LAYER_UI_FG,
						});

						if (interact)
						{
							if(it->words_said_on_page < words_to_say.node_count)
							{
								// still saying stuff
								it->words_said_on_page = (int)words_to_say.node_count;
							}
							else
							{
								it->cur_page_index += 1;
								if(words_on_current_page(it).node_count == 0)
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
							interact = false;
						}
					}
					it->loading_anim_in = Lerp(it->loading_anim_in, unwarped_dt * 5.0f, it->gen_request_id != 0 ? 1.0f : 0.0f);
					draw_quad((DrawParams){
							quad_rotated_centered(head_pos, V2(40, 40), (float)unwarped_elapsed_time * 2.0f),
							IMG(image_loading),
							blendalpha(WHITE, it->loading_anim_in),
							.layer = LAYER_UI_FG,
					});
					AABB placing_text_in = aabb_centered(AddV2(bubble_center, V2(0, 10.0f)), V2(BUBBLE_TEXT_WIDTH_PIXELS, size.y * 0.15f));
					dbgrect(placing_text_in);

					MD_String8List to_draw = words_on_current_page_without_unsaid(it);
					if(to_draw.node_count != 0)
					{
						PlacedWordList placed = place_wrapped_words(frame_arena, to_draw, text_scale, aabb_size(placing_text_in).x, default_font);  
						
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

		assert(gs.player != NULL);

		// gameplay processing loop, do multiple if lagging
		// these are static so that, on frames where no gameplay processing is necessary and just rendering, the rendering uses values from last frame
		// @Place(gameplay processing loops)
		static Entity *interacting_with = 0; // used by rendering to figure out who to draw dialog box on
		static bool player_in_combat = false;




		float speed_target = 1.0f;
		gs.stopped_time = cur_unread_entity != 0;
		if(gs.stopped_time) speed_target = 0.0f;
		// pausing the game
		speed_factor = Lerp(speed_factor, unwarped_dt*10.0f, speed_target);
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
				float dt = unwarped_dt*speed_factor;

				gs.tick += 1;

				PROFILE_SCOPE("propagate actions")
				{
					for(PropagatingAction *cur = propagating; cur; cur = cur->next)
					{
						if(cur->progress < 1.0f)
						{
							cur->progress += dt;
							float effective_radius = propagating_radius(cur);
							ENTITIES_ITER(gs.entities)
							{
								if(it->is_npc && LenV2(SubV2(it->pos, cur->from)) < effective_radius)
								{
									if(!cur->already_propagated_to[frome(it).index])
									{
										cur->already_propagated_to[frome(it).index] = true;
										remember_action(&gs, it, cur->a, cur->context);
									}
								}
							}
						}
					}
				}

				// process gs.entities process entities
				player_in_combat = false; // in combat set by various enemies when they fight the player
				PROFILE_SCOPE("entity processing")
				{
					ENTITIES_ITER(gs.entities)
					{
						assert(!(it->exists && it->generation == 0));
						
						if (it->is_npc || it->is_character)
						{
							if(LenV2(it->last_moved) > 0.0f && !it->killed)
								it->rotation = lerp_angle(it->rotation, dt * 8.0f, AngleOfV2(it->last_moved));
						}

						if (it->is_npc)
						{

							// @Place(entity processing)
							
							if(it->dialog_fade > 0.0f)
								it->dialog_fade -= dt/DIALOG_FADE_TIME;
							
							Entity *toface = 0;
							if(gete(it->aiming_shotgun_at))
							{
								toface = gete(it->aiming_shotgun_at);
							}
							else if(gete(it->looking_at))
							{
								toface = gete(it->looking_at);
							}
							if(toface)
								it->target_rotation = AngleOfV2(SubV2(toface->pos, it->pos));

							it->rotation = lerp_angle(it->rotation, unwarped_dt*8.0f, it->target_rotation);

							if (it->gen_request_id != 0 && !gs.stopped_time)
							{
								assert(it->gen_request_id > 0);

#ifdef DESKTOP
								int status = get_by_id(it->gen_request_id)->status;
#else
#ifdef WEB
								int status = EM_ASM_INT( {
									return get_generation_request_status($0);
								}, it->gen_request_id);
#else
ISANERROR("Don't know how to do this stuff on this platform.")
#endif // WEB
#endif // DESKTOP
								if (status == 0)
								{
									// simply not done yet
								}
								else
								{
									if (status == 1)
									{
										having_errors = false;
										// done! we can get the string
										char sentence_cstr[MAX_SENTENCE_LENGTH] = { 0 };
#ifdef WEB
										EM_ASM( {
											let generation = get_generation_request_content($0);
											stringToUTF8(generation, $1, $2);
										}, it->gen_request_id, sentence_cstr, ARRLEN(sentence_cstr) - 1); // I think minus one for null terminator...
#endif

#ifdef DESKTOP
										memcpy(sentence_cstr, get_by_id(it->gen_request_id)->generated.text, get_by_id(it->gen_request_id)->generated.text_length);
#endif

										MD_String8 sentence_str = MD_S8CString(sentence_cstr);

										// parse out from the sentence NPC action and dialog
										Action out = {0};

										MD_ArenaTemp scratch = MD_GetScratch(0, 0);
										Log("Parsing `%.*s`...\n", MD_S8VArg(sentence_str));
										MD_String8 parse_response = parse_chatgpt_response(scratch.arena, it, sentence_str, &out);

										// check that it wraps in below two lines
										PlacedWordList placed = place_wrapped_words(frame_arena, split_by_word(frame_arena, TextChunkString8(out.speech)), BUBBLE_TEXT_SCALE, BUBBLE_TEXT_WIDTH_PIXELS, BUBBLE_FONT);
										int words_over_limit = 0;
										for(PlacedWord *cur = placed.first; cur; cur = cur->next)
										{
											if(cur->line_index >= BUBBLE_LINES_PER_PAGE*AI_MAX_BUBBLE_PAGES_IN_OUTPUT) // the max number of lines of text on a bubble
											{
												words_over_limit += 1;
											}
										}

										if(words_over_limit > 0)
										{
											// trim what the npc said so that the error message is never more than the text chunk, which without this would be super possible
											// if the speech the npc already made was too big
											int max_words_of_original_speech = MAX_SENTENCE_LENGTH / 2;
											MD_String8 original_speech = TextChunkString8(out.speech);
											MD_String8 trimmed_original_speech = original_speech.size < max_words_of_original_speech ? original_speech : FmtWithLint(frame_arena, "...%.*s", MD_S8VArg(MD_S8Substring(original_speech, original_speech.size - max_words_of_original_speech, original_speech.size)));
											MD_String8 new_err = FmtWithLint(frame_arena, "You said '%.*s' which is %d words over the maximum limit, you must be more succinct and remove at least that many words", MD_S8VArg(trimmed_original_speech), words_over_limit);
											append_to_errors(it, new_err);
										}
										else
										{
											if (parse_response.size == 0)
											{
												Log("Performing action %s!\n", actions[out.kind].name);
												perform_action(&gs, it, out);


											}
											else
											{
												Log("There was a parse error: `%.*s`\n", MD_S8VArg(parse_response));
												append_to_errors(it, parse_response);
											}
										}




										MD_ReleaseScratch(scratch);

#ifdef WEB
										EM_ASM( {
											done_with_generation_request($0);
										}, it->gen_request_id);
#endif
#ifdef DESKTOP
										done_with_request(it->gen_request_id);
#endif
									}
									else if (status == 2)
									{
										Log("Failed to generate dialog! Fuck!\n");
										having_errors = true;

										/*
										Action to_perform = {0};
										MD_String8 speech_mdstring = MD_S8Lit("I'm not sure...");
										memcpy(to_perform.speech, speech_mdstring.str, speech_mdstring.size);
										to_perform.speech_length = (int)speech_mdstring.size;
										perform_action(&gs, it, to_perform);
										*/
									}
									else if (status == -1)
									{
										Log("Generation request doesn't exist anymore, that's fine...\n");
									}
									else
									{
										Log("Unknown generation request status: %d\n", status);
									}
									it->gen_request_id = 0;
								}
							}
						}

						it->being_hovered = false;

						if (it->is_npc)
						{
							// character speech animation text input
							if (true)
							{
								MD_ArenaTemp scratch = MD_GetScratch(0, 0);

								MD_String8List to_say = words_on_current_page(it);
								MD_String8List to_say_without_unsaid = words_on_current_page_without_unsaid(it);
								if(to_say.node_count > 0 && it->words_said_on_page < to_say.node_count)
								{
									if(cur_unread_entity == it)
									{
										it->characters_of_word_animated += CHARACTERS_PER_SEC * unwarped_dt;
										int characters_in_animating_word = (int)to_say_without_unsaid.last->string.size; 
										if((int)it->characters_of_word_animated + 1 > characters_in_animating_word)
										{
											it->words_said_on_page += 1;
											it->characters_of_word_animated = 0.0f;

											float dist = LenV2(SubV2(it->pos, gs.player->pos));
											float volume = Lerp(-0.6f, clamp01(dist / 70.0f), -1.0f);
											AudioSample * possible_grunts[] = {
												&sound_grunt_0,
												&sound_grunt_1,
												&sound_grunt_2,
												&sound_grunt_3,
											};
											play_audio(possible_grunts[rand() % ARRLEN(possible_grunts)], volume);
										}
									}
								}

								MD_ReleaseScratch(scratch);
							}


							if(gete(it->joined))
							{
								int place_in_line = 1;
								Entity *e = it;
								ENTITIES_ITER(gs.entities)
								{
									if(it->is_npc && gete(it->joined) == gete(e->joined))
									{
										if(it == e) break;
										place_in_line += 1;
									}
								}

								Vec2 target = get_point_along_trail(BUFF_MAKEREF(&gs.player->position_history), (float)place_in_line * 1.0f);

								Vec2 last_pos = it->pos;
								it->pos = LerpV2(it->pos, dt*5.0f, target);
								if(LenV2(SubV2(it->pos, last_pos)) > 0.01f)
								{
									it->last_moved = NormV2(SubV2(it->pos, last_pos));
									it->vel = MulV2F(it->last_moved, 1.0f / dt);
								}
								else
								{
									it->vel = V2(0,0);
								}
							}


							// A* code
							if(false)
							{
								Entity *targeting = gs.player;

									/*
										 G cost: distance from the current node to the start node
										 H cost: distance from the current node to the target node

										 G     H
										 SUM
										 F cost: G + H
										 */
								Vec2 to = targeting->pos;

								PathCache *cached = get_path_cache(elapsed_time, it->cached_path);
								AStarPath path = { 0 };
								bool succeeded = false;
								if (cached)
								{
									path = cached->path;
									succeeded = true;
								}
								else
								{
									Vec2 from = it->pos;
									typedef struct AStarNode {
										bool exists;
										struct AStarNode * parent;
										bool in_closed_set;
										bool in_open_set;
											float f_score; // total of g score and h score
											float g_score; // distance from the node to the start node
											Vec2 pos;
										} AStarNode;

										BUFF(AStarNode, MAX_ASTAR_NODES) nodes = { 0 };
										struct { Vec2 key; AStarNode *value; } *node_cache = 0;
#define V2_HASH(v) (FloorV2(v))
										const float jump_size = TILE_SIZE / 2.0f;
										BUFF_APPEND(&nodes, ((AStarNode) { .in_open_set = true, .pos = from }));
										Vec2 from_hash = V2_HASH(from);
										float got_there_tolerance = max_coord(entity_aabb_size(gs.player))*1.5f;
										hmput(node_cache, from_hash, &nodes.data[0]);

										bool should_quit = false;
										AStarNode *last_node = 0;
										PROFILE_SCOPE("A* Pathfinding") // astar pathfinding a star
										while (!should_quit)
										{
											int openset_size = 0;
											BUFF_ITER(AStarNode, &nodes) if (it->in_open_set) openset_size += 1;
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
												PROFILE_SCOPE("get length to goal") length_to_goal = LenV2(SubV2(to, current->pos));

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
															BUFF_ITER(Entity*, &overlapping_at_want) if (*it != e) would_block_me = true;
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
																		BUFF_APPEND(&nodes, (AStarNode) { 0 });
																		existing = &nodes.data[nodes.cur_index-1];
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
																					//h_score += fabsf(existing->pos.x - to.x) + fabsf(existing->pos.y - to.y);
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

									Vec2 next_point_on_path = { 0 };
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
											dbgcol(BLUE) dbgline(*it, path.data[i-1]);
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
						else if (it->is_character)
						{
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
					for(int i = 0; i < ARRLEN(gs.entities); i++)
					{
						Entity *it = &gs.entities[i];
						if (it->destroy)
						{
							// add all memories to memory free list
							for(Memory *cur = it->memories_first; cur;)
							{
								Memory *prev = cur;
								cur = cur->next;
								MD_StackPush(memories_free_list, prev);
							}
							it->memories_first = 0;
							it->memories_last = 0;

							int gen = it->generation;
							*it = (Entity) { 0 };
							it->generation = gen;
						}
					}
					ENTITIES_ITER(gs.entities)
					{
						if (it->perceptions_dirty && !npc_does_dialog(it))
						{
							it->perceptions_dirty = false;
						}
						if (it->perceptions_dirty)
						{
							if(it->is_character)
							{
								it->perceptions_dirty = false;
							}
							else if(it->is_npc)
							{
								if (!gs.stopped_time)
								{
									it->perceptions_dirty = false; // needs to be in beginning because they might be redirtied by the new perception
									MD_String8 prompt_str = {0};
#ifdef DO_CHATGPT_PARSING
									prompt_str = generate_chatgpt_prompt(frame_arena, &gs, it, get_can_talk_to(it));
#else
									generate_prompt(it, &prompt);
#endif
									Log("Sending request with prompt `%.*s`\n", MD_S8VArg(prompt_str));

#ifdef WEB
									// fire off generation request, save id
									MD_ArenaTemp scratch = MD_GetScratch(0, 0);
									MD_String8 terminated_completion_url = nullterm(scratch.arena, FmtWithLint(scratch.arena, "%s://%s:%d/completion", IS_SERVER_SECURE ? "https" : "http", SERVER_DOMAIN, SERVER_PORT));
									int req_id = EM_ASM_INT({
										return make_generation_request(UTF8ToString($0, $1), UTF8ToString($2, $3));
									},
																					prompt_str.str, (int)prompt_str.size, terminated_completion_url.str, (int)terminated_completion_url.size);
									it->gen_request_id = req_id;
									MD_ReleaseScratch(scratch);
#endif

#ifdef DESKTOP
									// desktop http request, no more mocking
									MD_ArenaTemp scratch = MD_GetScratch(0, 0);

									MD_String8 ai_response = {0};
									bool mocking_the_ai_response = false;
#ifdef DEVTOOLS
#ifdef MOCK_AI_RESPONSE
									mocking_the_ai_response = true;
#endif
#endif
									bool succeeded = true; // couldn't get AI response if false
									if (mocking_the_ai_response)
									{
										if (it->memories_last->context.talking_to_kind == it->npc_kind)
										//if (it->memories_last->context.author_npc_kind != it->npc_kind)
										{
											const char *action = 0;
											if(gete(it->aiming_shotgun_at))
											{
												action = "fire_shotgun";
											} else {
												action = "aim_shotgun";
											}
											char *rigged_dialog[] = {
													"Repeated amounts of testing dialog overwhelmingly in support of the mulaney brothers",
											};
											char *next_dialog = rigged_dialog[it->times_talked_to % ARRLEN(rigged_dialog)];
											char *target = characters[it->memories_last->context.author_npc_kind].name;
											target = characters[NPC_Player].name;
											ai_response = FmtWithLint(frame_arena, "{\"target\": \"%s\", \"action\": \"%s\", \"action_argument\": \"The Player\", \"speech\": \"%s\"}", target, action, next_dialog);
#ifdef DESKTOP
											it->times_talked_to += 1;
#endif
										}
										else
										{
											ai_response = MD_S8Lit("{\"target\": \"nobody\", \"action\": \"none\", \"speech\": \"\"}");
										}
									}
									else
									{
										MD_String8 post_request_body = FmtWithLint(scratch.arena, "|%.*s", MD_S8VArg(prompt_str));
										it->gen_request_id = make_generation_request(post_request_body);
									}

									// something to mock
									if (ai_response.size > 0)
									{
										Log("Mocking...\n");
										Action a = {0};
										MD_String8 error_message = MD_S8Lit("Something really bad happened bro. File " STRINGIZE(__FILE__) " Line " STRINGIZE(__LINE__));
										if (succeeded)
										{
											error_message = parse_chatgpt_response(scratch.arena, it, ai_response, &a);
										}

										if (mocking_the_ai_response)
										{
										assert(succeeded);
										assert(error_message.size == 0);

										MD_String8 valid_str = is_action_valid(frame_arena, it, a);
										assert(valid_str.size == 0);
										perform_action(&gs, it, a);
									}
									else
									{
										if(succeeded)
										{
											if (error_message.size == 0)
											{
												perform_action(&gs, it, a);
											}
											else
											{
												Log("There was an error with the AI: %.*s", MD_S8VArg(error_message));
												append_to_errors(it, error_message);
											}
										}
									}
								}


								MD_ReleaseScratch(scratch);
#undef SAY
#endif		}
							}
							}
							else
							{
								assert(false);
							}
						}
					}
				}

				// @Place(process player)
				PROFILE_SCOPE("process player")
				{
					// do dialog
					Entity *closest_interact_with = 0;
					{
						// find closest to talk to
						{
							AABB dialog_rect = aabb_centered(gs.player->pos, V2(DIALOG_INTERACT_SIZE, DIALOG_INTERACT_SIZE));
							dbgrect(dialog_rect);
							Overlapping possible_dialogs = get_overlapping(dialog_rect);
							float closest_interact_with_dist = INFINITY;
							BUFF_ITER(Entity*, &possible_dialogs)
							{
								bool entity_talkable = true;
								if (entity_talkable) entity_talkable = entity_talkable && (*it)->is_npc;
								if (entity_talkable) entity_talkable = entity_talkable && !(*it)->is_character;
#ifdef WEB
								if (entity_talkable) entity_talkable = entity_talkable && (*it)->gen_request_id == 0;
#endif

								bool entity_interactible = entity_talkable;

								if (entity_interactible)
								{
									float dist = LenV2(SubV2((*it)->pos, gs.player->pos));
									if (dist < closest_interact_with_dist)
									{
										closest_interact_with_dist = dist;
										closest_interact_with = (*it);
									}
								}
							}
						}

						// maybe get rid of talking to
						if (gs.player->waiting_on_speech_with_somebody)
						{
							gs.player->state = CHARACTER_IDLE;
						}

						interacting_with = closest_interact_with;

						gs.player->interacting_with = frome(interacting_with);
					}

					if (interact)
					{
						if (closest_interact_with)
						{
							if (closest_interact_with->is_npc)
							{
								// begin dialog with closest npc
								gs.player->talking_to = frome(closest_interact_with);
								begin_text_input();
							}
							else
							{
								assert(false);
							}
						}
					}

					float speed = 0.0f;
					{
						
						if(gs.player->killed) gs.player->state = CHARACTER_KILLED;
						if(gs.player->state == CHARACTER_WALKING)
						{
							player_armature.go_to_animation = MD_S8Lit("Running");
						}
						else if(gs.player->state == CHARACTER_IDLE)
						{
							player_armature.go_to_animation = MD_S8Lit("Idle");
						}
						else if(gs.player->state == CHARACTER_KILLED)
						{
							player_armature.go_to_animation = MD_S8Lit("Die Backwards");
							player_armature.next_animation_isnt_looping = true;
						}
						else assert(false);

						if (gs.player->state == CHARACTER_WALKING)
						{
							speed = PLAYER_SPEED;
							if (LenV2(movement) == 0.0)
							{
								gs.player->state = CHARACTER_IDLE;
							}
							else
							{
							}
						}
						else if (gs.player->state == CHARACTER_IDLE)
						{
							if (LenV2(movement) > 0.01) gs.player->state = CHARACTER_WALKING;
						}
						else if (gs.player->state == CHARACTER_KILLED)
						{
						}
						else
						{
							assert(false); // unknown character state? not defined how to process
						}
					} // not time stopped

					// velocity processing
					{
						gs.player->last_moved = NormV2(movement);
						Vec2 target_vel = MulV2F(movement, pixels_per_meter * speed);
						gs.player->vel = LerpV2(gs.player->vel, dt * 15.0f, target_vel);
						gs.player->pos = move_and_slide((MoveSlideParams) { gs.player, gs.player->pos, MulV2F(gs.player->vel, dt) });

						bool should_append = false;

						// make it so no snap when new points added
						if(gs.player->position_history.cur_index > 0)
						{
							gs.player->position_history.data[gs.player->position_history.cur_index - 1] = gs.player->pos;
						}

						if(gs.player->position_history.cur_index > 2)
						{
							should_append = LenV2(SubV2(gs.player->position_history.data[gs.player->position_history.cur_index - 2], gs.player->pos)) > TILE_SIZE;
						}
						else
						{
							should_append = true;
						}
						if(should_append) BUFF_QUEUE_APPEND(&gs.player->position_history, gs.player->pos);

					}
					// health
					if (gs.player->damage >= 1.0)
					{
						reset_level();
					}
				}

				pressed = (PressedState) { 0 };
				memset(keypressed, 0, sizeof(keypressed));
				interact = false;
			} // while loop

            last_frame_gameplay_processing_time = stm_sec(stm_diff(stm_now(), time_start_gameplay_processing));
		}
		memcpy(keypressed, keypressed_before_gameplay, sizeof(keypressed));
		pressed = before_gameplay_loops;


#ifdef DEVTOOLS
		if(flycam)
		{
			Basis basis = flycam_basis();
			float speed = 2.0f;
			speed *= flycam_speed;
			flycam_pos = AddV3(flycam_pos, MulV3F(basis.forward, ((float)keydown[SAPP_KEYCODE_W] - (float)keydown[SAPP_KEYCODE_S])*speed*dt));
			flycam_pos = AddV3(flycam_pos, MulV3F(basis.right, ((float)keydown[SAPP_KEYCODE_D] - (float)keydown[SAPP_KEYCODE_A])*speed*dt));
			flycam_pos = AddV3(flycam_pos, MulV3F(basis.up, (((float)keydown[SAPP_KEYCODE_SPACE] + (float)keydown[SAPP_KEYCODE_LEFT_CONTROL]) - (float)keydown[SAPP_KEYCODE_LEFT_SHIFT])*speed*dt));
		}
#endif

		// @Place(player rendering)
		if(0)
		PROFILE_SCOPE("render player") // draw character draw player render character
		{
			if(gs.player->position_history.cur_index > 0)
			{
				float trail_len = get_total_trail_len(BUFF_MAKEREF(&gs.player->position_history));
				if(trail_len > 0.0f) // fmodf returns nan
				{
					float along = fmodf((float)elapsed_time*100.0f, 200.0f);
					Vec2 at = get_point_along_trail(BUFF_MAKEREF(&gs.player->position_history), along);
					dbgbigsquare(at);
					dbgbigsquare(get_point_along_trail(BUFF_MAKEREF(&gs.player->position_history), 50.0f));
				}
				BUFF_ITER_I(Vec2, &gs.player->position_history, i)
				{
					if(i == gs.player->position_history.cur_index - 1) 
					{
					}
					else
					{
						dbgline(*it, gs.player->position_history.data[i + 1]);
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
					Vec2 midpoint = MulV2F(AddV2(interacting_with->pos, gs.player->pos), 0.5f);
					draw_quad((DrawParams) { quad_centered(AddV2(midpoint, V2(0.0, 5.0f + sinf((float)elapsed_time*3.0f)*5.0f)), V2(size, size)), IMG(image_e_icon), blendalpha(WHITE, clamp01(1.0f - learned_e)), .layer = LAYER_UI_FG });
				}

				// interaction circle
				draw_quad((DrawParams) { quad_centered(interacting_with->pos, V2(TILE_SIZE, TILE_SIZE)), image_hovering_circle, full_region(image_hovering_circle), WHITE, .layer = LAYER_UI });
			}

			if (gs.player->state == CHARACTER_WALKING)
			{
			}
			else if (gs.player->state == CHARACTER_IDLE)
			{
			}
			else
			{
				assert(false); // unknown character state? not defined how to draw
			}

			// hurt vignette
			if (gs.player->damage > 0.0)
			{
				draw_quad((DrawParams) { (Quad) { .ul = V2(0.0f, screen_size().Y), .ur = screen_size(), .lr = V2(screen_size().X, 0.0f) }, image_hurt_vignette, full_region(image_hurt_vignette), (Color) { 1.0f, 1.0f, 1.0f, gs.player->damage }, .layer = LAYER_SCREENSPACE_EFFECTS, });
			}
		}

		// @Place(UI rendering)
		PROFILE_SCOPE("propagating")
		{
			for(PropagatingAction *cur = propagating; cur; cur = cur->next)
			{
				if(cur->progress < 1.0f)
				{
					float radius = propagating_radius(cur);
					Quad to_draw = quad_centered(cur->from, V2(radius, radius));
					draw_quad((DrawParams){ to_draw, IMG(image_hovering_circle), blendalpha(WHITE, 1.0f - cur->progress)});
				}
			}
		}

		if (having_errors)
		{
			Vec2 text_center = V2(screen_size().x / 2.0f, screen_size().y*0.8f);
			draw_quad((DrawParams){centered_quad(text_center, V2(screen_size().x*0.8f, screen_size().y*0.1f)), IMG(image_white_square), blendalpha(BLACK, 0.5f), .layer = LAYER_UI_FG});
			draw_centered_text((TextParams){false, MD_S8Lit("The AI server is having technical difficulties..."), text_center, WHITE, 1.0f });
		}

		// win screen
		{
			static float visible = 0.0f;
			float target = 0.0f;
			if(gs.won)
			{
				target = 1.0f;
			}
			visible = Lerp(visible, unwarped_dt*9.0f, target);

			draw_quad((DrawParams) {quad_at(V2(0,screen_size().y), screen_size()), IMG(image_white_square), blendalpha(BLACK, visible*0.7f), .layer = LAYER_UI});
			float shake_speed = 9.0f;
			Vec2 win_offset = V2(sinf((float)unwarped_elapsed_time * shake_speed * 1.5f + 0.1f), sinf((float)unwarped_elapsed_time * shake_speed + 0.3f));
			win_offset = MulV2F(win_offset, 10.0f);
			draw_centered_text((TextParams){false, MD_S8Lit("YOU WON"), AddV2(MulV2F(screen_size(), 0.5f), win_offset), WHITE, 9.0f*visible});

			if(imbutton(aabb_centered(V2(screen_size().x/2.0f, screen_size().y*0.25f), MulV2F(V2(170.0f, 60.0f), visible)), 1.5f*visible, MD_S8Lit("Restart")))
			{
				reset_level();
			}
		}

		// killed screen
		{
			static float visible = 0.0f;
			float target = 0.0f;
			if(gs.player->killed && !cur_unread_entity)
			{
				target = 1.0f;
			}
			visible = Lerp(visible, unwarped_dt*4.0f, target);

			draw_quad((DrawParams) {quad_at(V2(0,screen_size().y), screen_size()), IMG(image_white_square), blendalpha(BLACK, visible*0.7f), .layer = LAYER_UI});
			float shake_speed = 9.0f;
			Vec2 win_offset = V2(sinf((float)unwarped_elapsed_time * shake_speed * 1.5f + 0.1f), sinf((float)unwarped_elapsed_time * shake_speed + 0.3f));
			win_offset = MulV2F(win_offset, 10.0f);
			draw_centered_text((TextParams){false, MD_S8Lit("YOU WERE KILLED"), AddV2(MulV2F(screen_size(), 0.5f), win_offset), WHITE, 3.0f*visible});

			if(imbutton(aabb_centered(V2(screen_size().x/2.0f, screen_size().y*0.25f), MulV2F(V2(170.0f, 60.0f), visible)), 1.5f*visible, MD_S8Lit("Restart")))
			{
				reset_level();
			}
		}

#define HELPER_SIZE 250.0f



		// keyboard tutorial icons
		if(false)
		if (!mobile_controls)
		{
			float total_height = HELPER_SIZE * 2.0f;
			float vertical_spacing = HELPER_SIZE / 2.0f;
			total_height -= (total_height - (vertical_spacing + HELPER_SIZE));
			const float padding = 50.0f;
			float y = screen_size().y / 2.0f + total_height / 2.0f;
			float x = screen_size().x - padding - HELPER_SIZE;
			draw_quad((DrawParams) { quad_at(V2(x, y), V2(HELPER_SIZE, HELPER_SIZE)), IMG(image_shift_icon), (Color) { 1.0f, 1.0f, 1.0f, fmaxf(0.0f, 1.0f-learned_shift) }, .layer = LAYER_UI_FG });
			y -= vertical_spacing;
			draw_quad((DrawParams) { quad_at(V2(x, y), V2(HELPER_SIZE, HELPER_SIZE)), IMG(image_space_icon), (Color) { 1.0f, 1.0f, 1.0f, fmaxf(0.0f, 1.0f-learned_space) }, .layer = LAYER_UI_FG });
		}


		if (mobile_controls)
		{
			float thumbstick_nub_size = (img_size(image_mobile_thumbstick_nub).x / img_size(image_mobile_thumbstick_base).x) * thumbstick_base_size();
			draw_quad((DrawParams) { quad_centered(thumbstick_base_pos, V2(thumbstick_base_size(), thumbstick_base_size())), IMG(image_mobile_thumbstick_base), WHITE, .layer = LAYER_UI_FG });
			draw_quad((DrawParams) { quad_centered(thumbstick_nub_pos, V2(thumbstick_nub_size, thumbstick_nub_size)), IMG(image_mobile_thumbstick_nub), WHITE, .layer = LAYER_UI_FG });

			if (interacting_with)
			{
				draw_quad((DrawParams) { quad_centered(interact_button_pos(), V2(mobile_button_size(), mobile_button_size())), IMG(image_mobile_button), WHITE, .layer = LAYER_UI_FG });
			}
			draw_quad((DrawParams) { quad_centered(roll_button_pos(), V2(mobile_button_size(), mobile_button_size())), IMG(image_mobile_button), WHITE, .layer = LAYER_UI_FG });
			draw_quad((DrawParams) { quad_centered(attack_button_pos(), V2(mobile_button_size(), mobile_button_size())), IMG(image_mobile_button), WHITE, .layer = LAYER_UI_FG });
		}

#ifdef DEVTOOLS

		// statistics @Place(devtools drawing developer menu drawing)
		if (show_devtools)
			PROFILE_SCOPE("devtools drawing")
			{
				Vec2 depth_size = V2(200.0f, 200.0f);
				draw_quad((DrawParams){quad_at(V2(screen_size().x - depth_size.x, screen_size().y), depth_size), IMG(state.shadows.color_img), WHITE, .layer = LAYER_UI_FG});
				draw_quad((DrawParams){quad_at(V2(0.0, screen_size().y/2.0f), MulV2F(screen_size(), 0.1f)), IMG(state.outline_pass_image), WHITE, .layer = LAYER_UI_FG});

				Vec3 view_cam_pos = MulM4V4(InvGeneralM4(view), V4(0,0,0,1)).xyz;
				//if(view_cam_pos.y >= 4.900f) // causes nan if not true... not good...
				if(true)
				{
					Vec3 world_mouse = screenspace_point_to_camera_point(mouse_pos);
					Vec3 mouse_ray = NormV3(SubV3(world_mouse, view_cam_pos));
					Vec3 marker = ray_intersect_plane(view_cam_pos, mouse_ray, V3(0,0,0), V3(0,1,0));
					Vec2 mouse_on_floor = point_plane(marker);
					Overlapping mouse_over = get_overlapping(aabb_centered(mouse_on_floor, V2(1,1)));
					BUFF_ITER(Entity*, &mouse_over)
					{
						dbgcol(PINK)
						{
							dbgplanerect(entity_aabb(*it));

						// debug draw memories of hovered
							Entity *to_view = *it;
							Vec2 start_at = V2(0,300);
							Vec2 cur_pos = start_at;

							AABB bounds = draw_text((TextParams){false, MD_S8Fmt(frame_arena, "--Memories for %s--", characters[to_view->npc_kind].name), cur_pos, WHITE, 1.0});
							cur_pos.y -= aabb_size(bounds).y;

							for(Memory *cur = to_view->memories_first; cur; cur = cur->next)
								if(cur->speech.text_length > 0)
								{
									MD_String8 to_text = cur->context.talking_to_kind != NPC_nobody ? MD_S8Fmt(frame_arena, " to %s ", characters[cur->context.talking_to_kind].name) : MD_S8Lit("");
									MD_String8 text = MD_S8Fmt(frame_arena, "%s%s%.*s: %.*s", to_view->npc_kind == cur->context.author_npc_kind ? "(Me) " : "", characters[cur->context.author_npc_kind].name, MD_S8VArg(to_text), cur->speech.text_length, cur->speech);
									AABB bounds = draw_text((TextParams){false, text, cur_pos, WHITE, 1.0});
									cur_pos.y -= aabb_size(bounds).y;
								}

							if(keypressed[SAPP_KEYCODE_Q] && !receiving_text_input)
							{
								Log("-- Printing debug memories for %s --\n", characters[to_view->npc_kind].name);
								int mem_idx = 0;
								for(Memory *cur = to_view->memories_first; cur; cur = cur->next)
								{
									MD_String8 to_text = cur->context.talking_to_kind != NPC_nobody ? MD_S8Fmt(frame_arena, " to %s ", characters[cur->context.talking_to_kind].name) : MD_S8Lit("");
									MD_String8 text = MD_S8Fmt(frame_arena, "%s%s%.*s: %.*s", to_view->npc_kind == cur->context.author_npc_kind ? "(Me) " : "", characters[cur->context.author_npc_kind].name, MD_S8VArg(to_text), cur->speech.text_length, cur->speech);
									printf("Memory %d: %.*s\n", mem_idx, MD_S8VArg(text));
									mem_idx++;
								}
							}
						}
					}
				}

				Vec2 pos = V2(0.0, screen_size().Y);
				int num_entities = 0;
				ENTITIES_ITER(gs.entities) num_entities++;
				MD_String8 stats = tprint("Frametime: %.1f ms\nProcessing: %.1f ms\nGameplay processing: %.1f ms\nEntities: %d\nDraw calls: %d\nDrawn Vertices: %d\nProfiling: %s\nNumber gameplay processing loops: %d\nFlyecam: %s\nPlayer position: %f %f\n", dt*1000.0, last_frame_processing_time*1000.0, last_frame_gameplay_processing_time*1000.0, num_entities, num_draw_calls, num_vertices, profiling ? "yes" : "no", num_timestep_loops, flycam ? "yes" : "no", v2varg(gs.player->pos));
				AABB bounds = draw_text((TextParams) { true, stats, pos, BLACK, 1.0f });
				pos.Y -= bounds.upper_left.Y - screen_size().Y;
				bounds = draw_text((TextParams) { true, stats, pos, BLACK, 1.0f });

				// background panel
				colorquad(quad_aabb(bounds), (Color) { 1.0, 1.0, 1.0, 0.3f });
				draw_text((TextParams) { false, stats, pos, BLACK, 1.0f });
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



						float new_vertices[ FLOATS_PER_VERTEX*4 ] = { 0 };
						Vec2 region_size = SubV2(d.image_region.lower_right, d.image_region.upper_left);
						assert(region_size.X > 0.0);
						assert(region_size.Y > 0.0);
						//Vec2 lower_left = AddV2(d.image_region.upper_left, V2(0, region_size.y));
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

							AddV2(d.image_region.upper_left, V2(0.0,           region_size.Y)),
							AddV2(d.image_region.upper_left, V2(region_size.X, region_size.Y)),
							AddV2(d.image_region.upper_left, V2(region_size.X,           0.0)),
							AddV2(d.image_region.upper_left, V2(0.0,                     0.0)),


							/*
							AddV2(d.image_region.upper_left, V2(region_size.X, region_size.Y)),
							AddV2(d.image_region.upper_left, V2(0.0,           region_size.Y)),
							AddV2(d.image_region.upper_left, V2(0.0,                     0.0)),
							AddV2(d.image_region.upper_left, V2(region_size.X,           0.0)),
							*/
						};

						// convert to uv space
						sg_image_info info = sg_query_image_info(d.image);
						for (int i = 0; i < 4; i++)
						{
							tex_coords[i] = DivV2(tex_coords[i], V2((float)info.width, (float)info.height));
						}
						for (int i = 0; i < 4; i++)
						{
							Vec2 in_clip_space = into_clip_space(points[i]);
							new_vertices[i*FLOATS_PER_VERTEX + 0] = in_clip_space.X;
							new_vertices[i*FLOATS_PER_VERTEX + 1] = in_clip_space.Y;
							// update Y_COORD_IN_BACK, Y_COORD_IN_FRONT when this changes
							/*
								 float unmapped = (clampf(d.y_coord_sorting, -1.0f, 2.0f));
								 float mapped = (unmapped + 1.0f)/3.0f;
								 new_vertices[i*FLOATS_PER_VERTEX + 2] = 1.0f - (float)clamp(mapped, 0.0, 1.0);
								 */
							new_vertices[i*FLOATS_PER_VERTEX + 2] = 0.0f;
							new_vertices[i*FLOATS_PER_VERTEX + 3] = tex_coords[i].X;
							new_vertices[i*FLOATS_PER_VERTEX + 4] = tex_coords[i].Y;
						}

						// two triangles drawn, six vertices
						size_t total_size = 6*FLOATS_PER_VERTEX;

						// batched a little too close to the sun
						if (cur_batch_data_index + total_size >= ARRLEN(cur_batch_data))
						{
							flush_quad_batch();
							cur_batch_image = d.image;
							cur_batch_params = params;
						}

#define PUSH_VERTEX(vert) { memcpy(&cur_batch_data[cur_batch_data_index], &vert, FLOATS_PER_VERTEX*sizeof(float)); cur_batch_data_index += FLOATS_PER_VERTEX; }
						PUSH_VERTEX(new_vertices[0*FLOATS_PER_VERTEX]);
						PUSH_VERTEX(new_vertices[1*FLOATS_PER_VERTEX]);
						PUSH_VERTEX(new_vertices[2*FLOATS_PER_VERTEX]);
						PUSH_VERTEX(new_vertices[0*FLOATS_PER_VERTEX]);
						PUSH_VERTEX(new_vertices[2*FLOATS_PER_VERTEX]);
						PUSH_VERTEX(new_vertices[3*FLOATS_PER_VERTEX]);
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

		MD_ArenaClear(frame_arena);

		memset(keypressed, 0, sizeof(keypressed));
		pressed = (PressedState) { 0 };
	}
}

void cleanup(void)
{
#ifdef DESKTOP
	for(ChatRequest *cur = requests_first; cur; cur = cur->next)
	{
		cur->should_close = true;
	}
#endif

	MD_ArenaRelease(frame_arena);
	
	// Don't free the persistent arena because threads still access their ChatRequest should_close fieldon shutdown,
	// and ChatRequest is allocated on the persistent arena. We just shamelessly leak this memory. Cowabunga!
	//MD_ArenaRelease(persistent_arena);
	sg_shutdown();
	hmfree(imui_state);
	Log("Cleaning up\n");
}

void event(const sapp_event *e)
{
	if (e->key_repeat) return;

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

#ifdef DESKTOP
	// the desktop text backend, for debugging purposes
	if (receiving_text_input)
	{
		if (e->type == SAPP_EVENTTYPE_CHAR)
		{
			if (text_input_buffer_length < ARRLEN(text_input_buffer))
			{
				APPEND_TO_NAME(text_input_buffer, text_input_buffer_length, ARRLEN(text_input_buffer), (char)e->char_code);
			}
		}
		if (e->type == SAPP_EVENTTYPE_KEY_DOWN && e->key_code == SAPP_KEYCODE_ENTER)
		{
			// doesn't account for, if the text input buffer is completely full and doesn't have a null terminator.
			if(text_input_buffer_length >= ARRLEN(text_input_buffer))
			{
				text_input_buffer_length = ARRLEN(text_input_buffer) - 1;
			}
			text_input_buffer[text_input_buffer_length] = '\0';
			end_text_input((char*)text_input_buffer);
		}
	}
#endif

	if (e->type == SAPP_EVENTTYPE_KEY_DOWN && e->key_code == SAPP_KEYCODE_F11)
	{
		sapp_toggle_fullscreen();
	}

#ifdef DEVTOOLS
	if (!receiving_text_input && e->type == SAPP_EVENTTYPE_KEY_DOWN && e->key_code == SAPP_KEYCODE_F)
	{
		flycam = !flycam;
		sapp_lock_mouse(flycam);
	}

	if(flycam)
	{
    if (e->type == SAPP_EVENTTYPE_MOUSE_MOVE)
		{
			const float rotation_speed = 0.001f;
			flycam_horizontal_rotation -= e->mouse_dx * rotation_speed;
			flycam_vertical_rotation -= e->mouse_dy * rotation_speed;
			flycam_vertical_rotation = clampf(flycam_vertical_rotation, -PI32/2.0f + 0.01f, PI32/2.0f - 0.01f);
		}
		else if(e->type == SAPP_EVENTTYPE_MOUSE_SCROLL)
		{
			flycam_speed *= 1.0f + 0.1f*e->scroll_y;
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
				if (touchpoint_screen_pos.x < screen_size().x*0.4f)
				{
					if (!movement_touch.active)
					{
						//if(LenV2(SubV2(touchpoint_screen_pos, thumbstick_base_pos)) > 1.25f * thumbstick_base_size())
						if (true)
						{
							thumbstick_base_pos = touchpoint_screen_pos;
						}
						movement_touch = activate(point.identifier);
						thumbstick_nub_pos = thumbstick_base_pos;
					}
				}
				if (LenV2(SubV2(touchpoint_screen_pos, roll_button_pos())) < mobile_button_size()*0.5f)
				{
					roll_pressed_by = activate(point.identifier);
					mobile_roll_pressed = true;
				}
				if (LenV2(SubV2(touchpoint_screen_pos, interact_button_pos())) < mobile_button_size()*0.5f)
				{
					interact_pressed_by = activate(point.identifier);
					mobile_interact_pressed = true;
					pressed.interact = true;
				}
				if (LenV2(SubV2(touchpoint_screen_pos, attack_button_pos())) < mobile_button_size()*0.5f)
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

			if (e->key_code == SAPP_KEYCODE_S)
			{
				pressed.speak_shortcut = true;
			}
			if (e->key_code == SAPP_KEYCODE_G)
			{
				pressed.give_shortcut = true;
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
					init_profiling("rpgpt.spall");
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
		bool ignore_movement = false;
#ifdef DEVTOOLS
		if (mouse_frozen) ignore_movement = true;
#endif
		if (!ignore_movement) mouse_pos = V2(e->mouse_x, (float)sapp_height() - e->mouse_y);
	}
}

sapp_desc sokol_main(int argc, char* argv[])
{
	(void)argc; (void)argv;
	return (sapp_desc) {
		.init_cb = init,
			.frame_cb = frame,
			.cleanup_cb = cleanup,
			.event_cb = event,
			.sample_count = 4,
			.width = 800,
			.height = 600,
			//.gl_force_gles2 = true, not sure why this was here in example, look into
			.window_title = "RPGPT",
			.win32_console_attach = true,
			.win32_console_create = true,
			.icon.sokol_default = true,
	};
}
