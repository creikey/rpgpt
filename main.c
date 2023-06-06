
#include "tuning.h"

#define SOKOL_IMPL

#if defined(WIN32) || defined(_WIN32)
#define DESKTOP
#define WINDOWS
#define SOKOL_D3D11
#endif

#if defined(__EMSCRIPTEN__)
#define WEB
#define SOKOL_GLES2
#endif

#ifdef WINDOWS
#include <Windows.h>
#include <processthreadsapi.h>
#include <dbghelp.h>
#endif

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x

#include "buff.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_audio.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include "HandmadeMath.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

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
} WebArena;

WebArena *web_arena_alloc()
{
	WebArena *to_return = malloc(sizeof(to_return));

	*to_return = (WebArena) {
		.data = calloc(1, ARENA_SIZE),
			.cap = ARENA_SIZE,
			.pos = 0,
	};

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
	(void)arena;
	(void)align;
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
#define STBSP_ADD_TO_FUNCTIONS no_ubsan
#define MD_FUNCTION no_ubsan
#include "md.h"
#include "md.c"
#pragma warning(pop)

MD_Arena *persistent_arena = 0; // watch out, arenas have limited size. 

#include <math.h>

#ifdef DEVTOOLS
#ifdef DESKTOP
#define PROFILING
#define PROFILING_IMPL
#endif
#endif
#include "profiling.h"




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

typedef struct TileInstance
{
	uint16_t kind;
} TileInstance;

typedef struct AnimatedTile
{
	uint16_t id_from;
	bool exists;
	int num_frames;
	uint16_t frames[32];
} AnimatedTile;

typedef struct TileSet
{
	sg_image *img;
	uint16_t first_gid;
	AnimatedTile animated[128];
} TileSet;

#include "makeprompt.h"

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
	Log("Testing chatgpt parsing...\n");

	MD_ArenaTemp scratch = MD_GetScratch(0, 0);

	Entity e = {0};
	e.npc_kind = NPC_TheBlacksmith;
	e.exists = true;
	Action a = {0};
	MD_String8 error;
	MD_String8 speech;

	speech = MD_S8Lit("Better have a good reason for bothering me.");
	MD_String8 thoughts = MD_S8Lit("Man I'm tired today\" Whatever.");
	error = parse_chatgpt_response(scratch.arena, &e, FmtWithLint(scratch.arena, " Within the player's party, while the player is talking to Meld, you hear: ACT_none \"%.*s\" [%.*s]", MD_S8VArg(speech), MD_S8VArg(thoughts)), &a);
	assert(error.size == 0);
	assert(a.kind == ACT_none);
	assert(MD_S8Match(speech, MD_S8(a.speech, a.speech_length), 0));
	assert(MD_S8Match(thoughts, MD_S8(a.internal_monologue, a.internal_monologue_length), 0));

	error = parse_chatgpt_response(scratch.arena, &e, MD_S8Lit("ACT_give_item(ITEM_Chalice) \"Here you go\""), &a);
	assert(error.size > 0);
	error = parse_chatgpt_response(scratch.arena, &e, MD_S8Lit("ACT_give_item(ITEM_Chalice) \""), &a);
	assert(error.size > 0);
	error = parse_chatgpt_response(scratch.arena, &e, MD_S8Lit("ACT_give_item(ITEM_Cha \""), &a);
	assert(error.size > 0);

	BUFF_APPEND(&e.held_items, ITEM_Chalice);

	error = parse_chatgpt_response(scratch.arena, &e, MD_S8Lit("ACT_give_item(ITEM_Chalice \""), &a);
	assert(error.size > 0);
	error = parse_chatgpt_response(scratch.arena, &e, MD_S8Lit("ACT_give_item(ITEM_Chalice) \"Here you go\" [Man I'm gonna miss that chalice]"), &a);
	assert(error.size == 0);
	assert(a.kind == ACT_give_item);
	assert(a.argument.item_to_give == ITEM_Chalice);

	MD_ReleaseScratch(scratch);
}
#endif

typedef struct Overlap
{
	bool is_tile; // in which case e will be null, naturally
	TileInstance t;
	Entity *e;
} Overlap;

typedef BUFF(Overlap, 16) Overlapping;

typedef struct Level
{
	TileInstance tiles[LAYERS][LEVEL_TILES][LEVEL_TILES];
	Entity initial_entities[MAX_ENTITIES]; // shouldn't be directly modified, only used to initialize gs.entities on loading of level
} Level;

typedef struct TileCoord
{
	int x; // column
	int y; // row
} TileCoord;

// no alignment etc because lazy
typedef struct Arena
{
	char *data;
	size_t data_size;
	size_t cur;
} Arena;

Entity *player = 0; // up here, used in text backend callback

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
bool keydown[SAPP_KEYCODE_MENU] = { 0 };

bool choosing_item_grid = false;

// set to true when should receive text input from the web input box
// or desktop text input
bool receiving_text_input = false;

// called from the web to see if should do the text input modal
bool is_receiving_text_input()
{
	return receiving_text_input;
}

#ifdef DESKTOP
char text_input_buffer[MAX_SENTENCE_LENGTH] = {0};
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
	memset(keydown, 0, ARRLEN(keydown));
	_sapp_emsc_register_eventhandlers();
}
#else
#error "No platform defined for text input!
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
#include <WinHttp.h>
#include <process.h>

typedef struct ChatRequest
{
	struct ChatRequest *next;
	struct ChatRequest *prev;
	int id;
	int status;
	char generated[MAX_SENTENCE_LENGTH];
	int generated_length;
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
	if(!succeeded)
	{
		Log("Couldn't do the web: %u\n", GetLastError());
		my_request->status = 2;
	}
	if(succeeded)
	{
		WinAssertWithErrorCode(WinHttpReceiveResponse(hRequest, 0));

		DWORD status_code;
		DWORD status_code_size = sizeof(status_code);
		WinAssertWithErrorCode(WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_code_size, WINHTTP_NO_HEADER_INDEX));
		Log("Status code: %u\n", status_code);

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
				Log("Got this from http, size %d: %s\n", dwDownloaded, out_buffer);
				MD_S8ListPush(my_request->arena, &received_data_list, MD_S8(out_buffer, dwDownloaded)); 
			}
		} while (dwSize > 0);
		MD_String8 received_data = MD_S8ListJoin(my_request->arena, received_data_list, &(MD_StringJoin){0});

		MD_String8 ai_response = MD_S8Substring(received_data, 1, received_data.size);
		if(ai_response.size > ARRLEN(my_request->generated))
		{
			Log("%lld too big for %lld\n", ai_response.size, ARRLEN(my_request->generated));
			my_request->status = 2;
			return;
		}
		memcpy(my_request->generated, ai_response.str, ai_response.size);
		my_request->generated_length = (int)ai_response.size;
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
#error "Only know how to do desktop http requests on windows"
#endif // WINDOWS
#endif // DESKTOP

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

AABB entity_sword_aabb(Entity *e, float width, float height)
{
	if (e->facing_left)
	{
		return (AABB) {
			.upper_left = AddV2(e->pos, V2(-width, height)),
				.lower_right = AddV2(e->pos, V2(0.0, -height)),
		};
	}
	else
	{
		return (AABB) {
			.upper_left = AddV2(e->pos, V2(0.0, height)),
				.lower_right = AddV2(e->pos, V2(width, -height)),
		};
	}
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
		return V2(TILE_SIZE*0.9f, TILE_SIZE*0.5f);
	}
	else if (e->is_npc)
	{
		if (npc_is_knight_sprite(e))
		{
			return V2(TILE_SIZE*0.5f, TILE_SIZE*0.5f);
		}
		else if (e->npc_kind == NPC_GodRock)
		{
			return V2(TILE_SIZE*0.5f, TILE_SIZE*0.5f);
		}
		else if (e->npc_kind == NPC_OldMan)
		{
			return V2(TILE_SIZE*0.5f, TILE_SIZE*0.5f);
		}
		else if (e->npc_kind == NPC_Death)
		{
			return V2(TILE_SIZE*1.10f, TILE_SIZE*1.10f);
		}
		else if (npc_is_skeleton(e))
		{
			return V2(TILE_SIZE*1.0f, TILE_SIZE*1.0f);
		}
		else if (e->npc_kind == NPC_TheGuard)
		{
			return V2(TILE_SIZE*0.5f, TILE_SIZE*0.5f);
		}
		else if (e->npc_kind == NPC_PeaceTotem)
		{
			return V2(TILE_SIZE, TILE_SIZE);
		}
		else
		{
			assert(false);
			return (Vec2) { 0 };
		}
	}
	else if (e->is_prop)
	{
		return V2(TILE_SIZE*0.5f, TILE_SIZE*0.5f);
	}
	else if (e->is_item)
	{
		return V2(TILE_SIZE*0.5f, TILE_SIZE*0.5f);
	}
	else
	{
		assert(false);
		return (Vec2) { 0 };
	}
}

bool is_tile_solid(TileInstance t)
{
	uint16_t tile_id = t.kind;
	uint16_t collideable[] = {
		57 , 58 , 59 ,
		121, 122, 123,
		185, 186, 187,
		249, 250, 251,
		313, 314, 315,
		377, 378, 379,
	};
	for (int i = 0; i < ARRLEN(collideable); i++)
	{
		if (tile_id == collideable[i] + 1) return true;
	}
	return false;
	//return tile_id == 53 || tile_id == 0 || tile_id == 367 || tile_id == 317 || tile_id == 313 || tile_id == 366 || tile_id == 368;
}

bool is_overlap_collision(Overlap o)
{
	if (o.is_tile)
	{
		return is_tile_solid(o.t);
	}
	else
	{
		assert(o.e);
		return !o.e->is_item;
	}
}


// tilecoord is integer tile position, not like tile coord
Vec2 tilecoord_to_world(TileCoord t)
{
	return V2((float)t.x * (float)TILE_SIZE * 1.0f, -(float)t.y * (float)TILE_SIZE * 1.0f);
}

// points from tiled editor have their own strange and alien coordinate system (local to the tilemap Y+ down)
Vec2 tilepoint_to_world(Vec2 tilepoint)
{
	Vec2 tilecoord = MulV2F(tilepoint, 1.0 / TILE_SIZE);
	return tilecoord_to_world((TileCoord) { (int)tilecoord.X, (int)tilecoord.Y });
}

TileCoord world_to_tilecoord(Vec2 w)
{
	// world = V2(tilecoord.x * tile_size, -tilecoord.y * tile_size)
	// world.x = tilecoord.x * tile_size
	// world.x / tile_size = tilecoord.x
	// world.y = -tilecoord.y * tile_size
	// - world.y / tile_size = tilecoord.y
	return (TileCoord) { (int)floorf(w.X / TILE_SIZE), (int)floorf(-w.Y / TILE_SIZE) };
}


AABB tile_aabb(TileCoord t)
{
	return (AABB)
	{
		.upper_left = tilecoord_to_world(t),
			.lower_right = AddV2(tilecoord_to_world(t), V2(TILE_SIZE, -TILE_SIZE)),
	};
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

AABB centered_aabb(Vec2 at, Vec2 size)
{
	return (AABB) {
		.upper_left = AddV2(at, V2(-size.X / 2.0f, size.Y / 2.0f)),
			.lower_right = AddV2(at, V2(size.X / 2.0f, -size.Y / 2.0f)),
	};
}


AABB entity_aabb_at(Entity *e, Vec2 at)
{
	return centered_aabb(at, entity_aabb_size(e));
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

TileInstance get_tile_layer(Level *l, int layer, TileCoord t)
{
	bool out_of_bounds = false;
	out_of_bounds |= t.x < 0;
	out_of_bounds |= t.x >= LEVEL_TILES;
	out_of_bounds |= t.y < 0;
	out_of_bounds |= t.y >= LEVEL_TILES;
	//assert(!out_of_bounds);
	if (out_of_bounds) return (TileInstance) { 0 };
	return l->tiles[layer][t.y][t.x];
}

TileInstance get_tile(Level *l, TileCoord t)
{
	return get_tile_layer(l, 0, t);
}

sg_image load_image(const char *path)
{
	sg_image to_return = { 0 };

	int png_width, png_height, num_channels;
	const int desired_channels = 4;
	stbi_uc* pixels = stbi_load(
			path,
			&png_width, &png_height,
			&num_channels, 0);
	assert(pixels);
	Log("Pah %s | Loading image with dimensions %d %d\n", path, png_width, png_height);
	to_return = sg_make_image(&(sg_image_desc)
			{
			.width = png_width,
			.height = png_height,
			.pixel_format = SG_PIXELFORMAT_RGBA8,
			.min_filter = SG_FILTER_NEAREST,
			.num_mipmaps = 0,
			.wrap_u = SG_WRAP_CLAMP_TO_EDGE,
			.wrap_v = SG_WRAP_CLAMP_TO_EDGE,
			.mag_filter = SG_FILTER_NEAREST,
			.data.subimage[0][0] =
			{
			.ptr = pixels,
			.size = (size_t)(png_width * png_height * 4),
			}
			});
	stbi_image_free(pixels);
	return to_return;
}

#include "assets.gen.c"
#include "quad-sapp.glsl.h"

AABB level_aabb = { .upper_left = { 0.0f, 0.0f }, .lower_right = { TILE_SIZE * LEVEL_TILES, -(TILE_SIZE * LEVEL_TILES) } };
GameState gs = { 0 };

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
	EntityRef to_return = {
		.index = (int)(e - gs.entities),
		.generation = e->generation,
	};
	assert(to_return.index >= 0);
	assert(to_return.index < ARRLEN(gs.entities));
	return to_return;
}

Entity *gete(EntityRef ref)
{
	if (ref.generation == 0) return 0;
	Entity *to_return = &gs.entities[ref.index];
	if (!to_return->exists || to_return->generation != ref.generation)
	{
		return 0;
	}
	else
	{
		return to_return;
	}
}

void push_memory(Entity *e, MD_String8 speech, MD_String8 monologue, ActionKind a_kind, ActionArgument a_argument, MemoryContext context, bool is_error)
{
	Memory new_memory = {.action_taken = a_kind};
	assert(speech.size <= ARRLEN(new_memory.speech));
	new_memory.tick_happened = gs.tick;
	new_memory.context = context;
	new_memory.is_error = is_error;
	new_memory.action_argument = a_argument;
	memcpy(new_memory.speech, speech.str, speech.size);
	new_memory.speech_length = (int)speech.size;
	memcpy(new_memory.internal_monologue, monologue.str, monologue.size);
	new_memory.internal_monologue_length = (int)monologue.size;
	if(!BUFF_HAS_SPACE(&e->memories))
	{
		BUFF_REMOVE_FRONT(&e->memories);
	}
	BUFF_APPEND(&e->memories, new_memory);

	if(!context.i_said_this)
	{
		// self speech doesn't dirty
		e->perceptions_dirty = true;
	}
}

void remember_error(Entity *to_modify, MD_String8 error_message)
{
	assert(!to_modify->is_character); // this is a game logic bug if a player action is invalid

	push_memory(to_modify, error_message, MD_S8(0, 0), ACT_none, (ActionArgument){0}, (MemoryContext){0}, true);
}

void remember_action(Entity *to_modify, Action a, MemoryContext context)
{
	push_memory(to_modify, MD_S8(a.speech, a.speech_length), MD_S8(a.internal_monologue, a.internal_monologue_length), a.kind, (ActionArgument){0}, context, false);
	if(context.i_said_this)
	{
		to_modify->words_said = 0;
		to_modify->word_anim_in = 0;
	}
}

// from must not be null, to can be null if the action isn't directed at anybody
// the action must have been validated to be valid if you're calling this
void cause_action_side_effects(Entity *from, Entity *to, Action a)
{
	assert(from);
	MD_ArenaTemp scratch = MD_GetScratch(0, 0);

	MD_String8 failure_reason = is_action_valid(scratch.arena, from, to, a);
	if(failure_reason.size > 0)
	{
		Log("Failed to process action, invalid action: `%.*s`\n", MD_S8VArg(failure_reason));
		assert(false);
	}

    if(a.kind == ACT_gives_peace_token)
    {
        assert(!from->has_given_peace_token);
        from->has_given_peace_token = true;
				to->peace_tokens += 1;
    }

	if(a.kind == ACT_give_item)
	{
		assert(a.argument.item_to_give != ITEM_none);
		assert(to);

		int item_to_remove = -1;
		Entity *e = from;
		BUFF_ITER_I(ItemKind, &e->held_items, i)
		{
			if (*it == a.argument.item_to_give)
			{
				item_to_remove = i;
				break;
			}
		}
		if (item_to_remove < 0)
		{
			Log("Can't find item %s to give from NPC %s to the player\n", items[a.argument.item_to_give].name, characters[e->npc_kind].name);
			assert(false);
		}
		else
		{
			BUFF_REMOVE_AT_INDEX(&e->held_items, item_to_remove);
			BUFF_APPEND(&to->held_items, a.argument.item_to_give);
		}

	}

	if(a.kind == ACT_fights_player)
	{
		from->standing = STANDING_FIGHTING;
	}
	if(a.kind == ACT_stops_fighting_player || a.kind == ACT_leaves_player)
	{
		from->standing = STANDING_INDIFFERENT;
	}
	if(a.kind == ACT_joins_player)
	{
		from->standing = STANDING_JOINED;
	}

	MD_ReleaseScratch(scratch);
}

// only called when the action is instantiated, correctly propagates the information
// of the action physically and through the party
// If the action is invalid, remembers the error if it's an NPC, and does nothing else
// Returns if the action was valid or not
bool perform_action(Entity *from, Action a)
{
	MD_ArenaTemp scratch = MD_GetScratch(0, 0);

	MemoryContext context = {0};
	context.author_npc_kind = from->npc_kind;

	Entity *action_target = 0;
	if(from == player && gete(from->talking_to))
	{
		action_target = gete(from->talking_to);
		assert(action_target->is_npc);
	}
	else if(gete(player->talking_to) == from)
		action_target = player;

	if(action_target)
	{
		context.was_directed_at_somebody = true;
		context.directed_at_kind = action_target->npc_kind;
	}

	MD_String8 is_valid = is_action_valid(scratch.arena, from, action_target, a);
	bool proceed_propagating = true;
	if(is_valid.size > 0)
	{
		remember_error(from, is_valid);
		proceed_propagating = false;
	}

	if(proceed_propagating)
	{
		cause_action_side_effects(from, action_target, a);
		// self memory
		if(!from->is_character)
		{
			MemoryContext my_context = context;
			my_context.i_said_this = true;
			remember_action(from, a, my_context); 
		}

		// memory of target
		if(action_target)
		{
			remember_action(action_target, a, context);
		}

		bool propagate_to_party = from->is_character || (from->is_npc && from->standing == STANDING_JOINED);

		if(action_target == player) propagate_to_party = true;


		if(context.eavesdropped_from_party) propagate_to_party = false;

		if(propagate_to_party)
		{
			ENTITIES_ITER(gs.entities)
			{
				if(it->is_npc && it->standing == STANDING_JOINED && it != from && it != action_target)
				{
					MemoryContext eavesdropped_context = context;
					eavesdropped_context.eavesdropped_from_party = true;
					remember_action(it, a, eavesdropped_context);
				}
			}
		}


		// npcs in party when they talk should have their speech heard by who the player is talking to
		if(from->is_npc && from->standing == STANDING_JOINED)
		{
			if(gete(player->talking_to) && gete(player->talking_to) != from)
			{
				assert(gete(player->talking_to));
				assert(gete(player->talking_to)->is_npc);
				MemoryContext from_party_context = context;
				from_party_context.directed_at_kind = gete(player->talking_to)->npc_kind;
				remember_action(gete(player->talking_to), a, from_party_context);
			}
		}

		// TODO Propagate physically
	}

	MD_ReleaseScratch(scratch);
	return proceed_propagating;
}

bool eq(EntityRef ref1, EntityRef ref2)
{
	return ref1.index == ref2.index && ref1.generation == ref2.generation;
}

Entity *new_entity()
{
	for (int i = 0; i < ARRLEN(gs.entities); i++)
	{
		if (!gs.entities[i].exists)
		{
			Entity *to_return = &gs.entities[i];
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

void update_player_from_entities()
{
	player = 0;
	ENTITIES_ITER(gs.entities)
	{
		if (it->is_character)
		{
			assert(player == 0);
			player = it;
		}
	}
	assert(player != 0);
	player->npc_kind = NPC_Player; // bad
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

		ToVisit new = {.depth = cur_tovisit->depth + 1};
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
	NpcKind to_return = NPC_Invalid;
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

	if(to_return == NPC_Invalid)
	{
		PushWithLint(arena, errors, "The %s `%.*s` could not be recognized in the game", enum_kind_name, MD_S8VArg(enum_str));
	}

	MD_ReleaseScratch(scratch);

	return to_return;
}

#define parse_enumstr(arena, enum_str, errors, string_array, enum_kind_name, prefix) parse_enumstr_impl(arena, enum_str, string_array, ARRLEN(string_array), errors, enum_kind_name, prefix)

void reset_level()
{
	// load level
	gs.won = false;
	Level *to_load = &level_level0;
	{
		assert(ARRLEN(to_load->initial_entities) == ARRLEN(gs.entities));
		memcpy(gs.entities, to_load->initial_entities, sizeof(Entity) * MAX_ENTITIES);
		gs.version = CURRENT_VERSION;
		ENTITIES_ITER(gs.entities)
		{
			if (it->generation == 0) it->generation = 1; // zero value generation means doesn't exist
		}
	}
	update_player_from_entities();

#ifdef DEVTOOLS
	if(false)
	{
		BUFF_APPEND(&player->held_items, ITEM_WhiteSquare);
		for (int i = 0; i < 20; i++)
			BUFF_APPEND(&player->held_items, ITEM_Boots);
	}

	ENTITIES_ITER(gs.entities)
	{
		if(false)
		if (it->npc_kind == NPC_TheBlacksmith)
		{
			Memory test_memory = {0};
			test_memory.context.author_npc_kind = NPC_TheBlacksmith;
			MD_String8 speech = MD_S8Lit("This is some very important testing dialog. Too important to count. Very very very very important. Super caliafradgalisticexpelaladosis");
			memcpy(test_memory.speech, speech.str, speech.size);
			test_memory.speech_length = (int)speech.size;
			RANGE_ITER(0, 15)
				BUFF_APPEND(&it->memories, test_memory);
			//RANGE_ITER(0, 20)
			//BUFF_APPEND(&it->remembered_perceptions, ((Perception) { .type = PlayerDialog, .player_dialog = SENTENCE_CONST("Testing dialog") }));

			//BUFF_APPEND(&it->held_items, ITEM_Chalice);
		}
	}
#endif


	// parse and enact the drama document
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
						MD_String8 thoughts = MD_ChildFromString(cur, MD_S8Lit("thoughts"), 0)->first_child->string;
						MD_String8 action = MD_ChildFromString(cur, MD_S8Lit("action"), 0)->first_child->string; 

						current_context.author_npc_kind = parse_enumstr(scratch.arena, enum_str, &drama_errors, NpcKind_names, "NpcKind", "NPC_");
						if(action.size > 0)
						{
							current_action.kind = parse_enumstr(scratch.arena, action, &drama_errors,ActionKind_names, "ActionKind", "ACT_");
						}

						if(dialog.size >= ARRLEN(current_action.speech))
						{
							PushWithLint(scratch.arena, &drama_errors, "Current action's speech is of size %d, bigger than allowed size %d", (int)dialog.size, (int)ARRLEN(current_action.speech));
						}
						if(thoughts.size >= ARRLEN(current_action.internal_monologue))
						{
							PushWithLint(scratch.arena, &drama_errors, "Current thought's speech is of size %d, bigger than allowed size %d", (int)thoughts.size, (int)ARRLEN(current_action.internal_monologue));
						}
						if(drama_errors.node_count == 0)
						{
							memcpy(current_action.speech, dialog.str, dialog.size);
							current_action.speech_length = (int)dialog.size;

							memcpy(current_action.internal_monologue, thoughts.str, thoughts.size);
							current_action.internal_monologue_length = (int)thoughts.size;
						}
					}

					if(drama_errors.node_count == 0)
					{
						for(MD_Node *cur_kind_node = can_hear; !MD_NodeIsNil(cur_kind_node); cur_kind_node = cur_kind_node->next)
						{
							NpcKind want = parse_enumstr(scratch.arena, cur_kind_node->string, &drama_errors, NpcKind_names, "NpcKind", "NPC_");
							if(drama_errors.node_count == 0)
							{
								bool found = false;
								ENTITIES_ITER(gs.entities)
								{
									if(it->is_npc && it->npc_kind == want)
									{
										MemoryContext this_context = current_context;
										if(it->npc_kind == current_context.author_npc_kind)
										{
											this_context.i_said_this = true;
										}
										remember_action(it, current_action, this_context);
										found = true;
										break;
									}
								}

								if(!found)
								{
									PushWithLint(scratch.arena, &drama_errors, "Couldn't find NPC of kind %s in the current map", characters[want].enum_name);
								}
							}
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


		ENTITIES_ITER(gs.entities)
		{
			it->perceptions_dirty = false; // nobody should say anything about jester memories
		}
	}
}


#ifdef WEB
	EMSCRIPTEN_KEEPALIVE
void dump_save_data()
{
	EM_ASM( {
			save_game_data = new Int8Array(Module.HEAP8.buffer, $0, $1);
			}, (char*)(&gs), sizeof(gs));
}
	EMSCRIPTEN_KEEPALIVE
void read_from_save_data(char *data, size_t length)
{
	GameState read_data = { 0 };
	memcpy((char*)(&read_data), data, length);
	if (read_data.version != CURRENT_VERSION)
	{
		Log("Bad gamestate, has version %d expected version %d\n", read_data.version, CURRENT_VERSION);
	}
	else
	{
		gs = read_data;
		update_player_from_entities();
	}
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
		what_player_said = MD_S8Substring(what_player_said, 0, ARRLEN(to_perform.speech));

		memcpy(to_perform.speech, what_player_said.str, what_player_said.size);
		to_perform.speech_length = (int)what_player_said.size;

		perform_action(player, to_perform);
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


sg_image image_font = { 0 };
Vec2 image_font_size = { 0 }; // this image's size is queried a lot, and img_size seems to be slow when profiled

float font_line_advance = 0.0f;
const float font_size = 32.0;
float font_scale;
unsigned char *fontBuffer = 0; // font file data. Can't be freed until program quits, because used to get character width
stbtt_bakedchar cdata[96]; // ASCII 32..126 is 95 glyphs
stbtt_fontinfo font;


static struct
{
	sg_pass_action pass_action;
	sg_pipeline pip;
	sg_bindings bind;
} state;


void audio_stream_callback(float *buffer, int num_frames, int num_channels)
{
	assert(num_channels == 1);
	const int num_samples = num_frames * num_channels;
	double time_to_play = (double)num_frames / (double)SAMPLE_RATE;
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


#define WHITE ((Color) { 1.0f, 1.0f, 1.0f, 1.0f })
#define GREY  ((Color) { 0.4f, 0.4f, 0.4f, 1.0f })
#define BLACK ((Color) { 0.0f, 0.0f, 0.0f, 1.0f })
#define RED   ((Color) { 1.0f, 0.0f, 0.0f, 1.0f })
#define PINK  ((Color) { 1.0f, 0.0f, 1.0f, 1.0f })
#define BLUE  ((Color) { 0.0f, 0.0f, 1.0f, 1.0f })
#define GREEN ((Color) { 0.0f, 1.0f, 0.0f, 1.0f })
#define BROWN (colhex(0x4d3d25))

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

void init(void)
{
#ifdef WEB
	EM_ASM( {
			set_server_url(UTF8ToString($0));
			}, SERVER_DOMAIN );
#endif

#ifdef DEVTOOLS
	do_metadesk_tests();
	do_parsing_tests();
#endif

	frame_arena = MD_ArenaAlloc();
	persistent_arena = MD_ArenaAlloc();

	Log("Size of entity struct: %zu\n", sizeof(Entity));
	Log("Size of %d gs.entities: %zu kb\n", (int)ARRLEN(gs.entities), sizeof(gs.entities) / 1024);
	sg_setup(&(sg_desc) {
			.context = sapp_sgcontext(),
			});
	stm_setup();
	saudio_setup(&(saudio_desc) {
			.stream_cb = audio_stream_callback,
			.logger.func = slog_func,
			});

	typedef BUFF(char, 1024) DialogNode;
	DialogNode cur_node = { 0 };

	load_assets();
	reset_level();

#ifdef WEB
	EM_ASM( {
			load_all();
			});
#endif

	// load font
	{
		FILE* fontFile = fopen("assets/Roboto-Regular.ttf", "rb");
		fseek(fontFile, 0, SEEK_END);
		size_t size = ftell(fontFile); /* how long is the file ? */
		fseek(fontFile, 0, SEEK_SET); /* reset */

		fontBuffer = calloc(size, 1);

		fread(fontBuffer, size, 1, fontFile);
		fclose(fontFile);

		unsigned char *font_bitmap = calloc(1, 512*512);
		stbtt_BakeFontBitmap(fontBuffer, 0, font_size, font_bitmap, 512, 512, 32, 96, cdata);

		unsigned char *font_bitmap_rgba = malloc(4 * 512 * 512); // stack would be too big if allocated on stack (stack overflow)
		for (int i = 0; i < 512 * 512; i++)
		{
			font_bitmap_rgba[i*4 + 0] = 255;
			font_bitmap_rgba[i*4 + 1] = 255;
			font_bitmap_rgba[i*4 + 2] = 255;
			font_bitmap_rgba[i*4 + 3] = font_bitmap[i];
		}

		image_font = sg_make_image(&(sg_image_desc) {
				.width = 512,
				.height = 512,
				.pixel_format = SG_PIXELFORMAT_RGBA8,
				.min_filter = SG_FILTER_LINEAR,
				.mag_filter = SG_FILTER_LINEAR,
				.data.subimage[0][0] =
				{
				.ptr = font_bitmap_rgba,
				.size = (size_t)(512 * 512 * 4),
				}
				});

		image_font_size = img_size(image_font);

		stbtt_InitFont(&font, fontBuffer, 0);
		int ascent = 0;
		int descent = 0;
		int lineGap = 0;
		font_scale = stbtt_ScaleForPixelHeight(&font, font_size);
		stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
		font_line_advance = (float)(ascent - descent + lineGap) * font_scale * 0.75f;

		free(font_bitmap_rgba);
	}

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

	const sg_shader_desc *desc = quad_program_shader_desc(sg_query_backend());
	assert(desc);
	sg_shader shd = sg_make_shader(desc);

	Color clearcol = colhex(0x98734c);
	state.pip = sg_make_pipeline(&(sg_pipeline_desc)
			{
			.shader = shd,
			.depth = {
			.compare = SG_COMPAREFUNC_LESS_EQUAL,
			.write_enabled = true
			},
			.layout = {
			.attrs =
			{
			[ATTR_quad_vs_position].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_quad_vs_texcoord0].format = SG_VERTEXFORMAT_FLOAT2,
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

	state.pass_action = (sg_pass_action)
	{
		//.colors[0] = { .action=SG_ACTION_CLEAR, .value={12.5f/255.0f, 12.5f/255.0f, 12.5f/255.0f, 1.0f } }
		//.colors[0] = { .action=SG_ACTION_CLEAR, .value={255.5f/255.0f, 255.5f/255.0f, 255.5f/255.0f, 1.0f } }
		// 0x898989 is the color in tiled
		.colors[0] =
		{ .action = SG_ACTION_CLEAR, .value = { clearcol.r, clearcol.g, clearcol.b, 1.0f } }
	};
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
Camera cam = { .scale = 2.0f };

Vec2 cam_offset()
{
	Vec2 to_return = AddV2(cam.pos, MulV2F(screen_size(), 0.5f));
	to_return = FloorV2(to_return); // avoid pixel glitching on tilemap atlas
	return to_return;
}


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

// screen coords are in pixels counting from bottom left as (0,0), Y+ is up
Vec2 world_to_screen(Vec2 world)
{
	Vec2 to_return = world;
	to_return = MulV2F(to_return, cam.scale);
	to_return = AddV2(to_return, cam_offset());
	return to_return;
}

Vec2 screen_to_world(Vec2 screen)
{
	Vec2 to_return = screen;
	to_return = SubV2(to_return, cam_offset());
	to_return = MulV2F(to_return, 1.0f / cam.scale);
	return to_return;
}

AABB aabb_screen_to_world(AABB screen)
{
	return (AABB) { .upper_left = screen_to_world(screen.upper_left), .lower_right = screen_to_world(screen.lower_right ), };
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

Quad tile_quad(TileCoord coord)
{
	Quad to_return = quad_at(tilecoord_to_world(coord), V2(TILE_SIZE, TILE_SIZE));

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
	return quad_aabb(centered_aabb(at, size));
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

AABB world_cam_aabb()
{
	AABB to_return = screen_cam_aabb();
	to_return.upper_left = screen_to_world(to_return.upper_left);
	to_return.lower_right = screen_to_world(to_return.lower_right);
	return to_return;
}

int num_draw_calls = 0;

#define FLOATS_PER_VERTEX (3 + 2)
float cur_batch_data[1024*10] = { 0 };
int cur_batch_data_index = 0;
// @TODO check last tint as well, do this when factor into drawing parameters
sg_image cur_batch_image = { 0 };
quad_fs_params_t cur_batch_params = { 0 };
void flush_quad_batch()
{
	if (cur_batch_image.id == 0 || cur_batch_data_index == 0) return; // flush called when image changes, image starts out null!
	state.bind.vertex_buffer_offsets[0] = sg_append_buffer(state.bind.vertex_buffers[0], &(sg_range) { cur_batch_data, cur_batch_data_index*sizeof(*cur_batch_data) });
	state.bind.fs_images[SLOT_quad_tex] = cur_batch_image;
	sg_apply_bindings(&state.bind);
	sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_quad_fs_params, &SG_RANGE(cur_batch_params));
	assert(cur_batch_data_index % FLOATS_PER_VERTEX == 0);
	sg_draw(0, cur_batch_data_index / FLOATS_PER_VERTEX, 1);
	num_draw_calls += 1;
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
			Log("Could not read stack trace: %d\n", error_code);
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
	bool world_space;
	Quad quad;
	sg_image image;
	AABB image_region;
	Color tint;

	AABB clip_to; // if world space is in world space, if screen space is in screen space - Lao Tzu
	int sorting_key;
	float alpha_clip_threshold;

	bool do_clipping;
	Layer layer;

	// for debugging purposes
	int line_number; 
} DrawParams;

Vec2 into_clip_space(Vec2 screen_space_point)
{
	Vec2 zero_to_one = DivV2(screen_space_point, screen_size());
	Vec2 in_clip_space = SubV2(MulV2F(zero_to_one, 2.0), V2(1.0, 1.0));
	return in_clip_space;
}


typedef BUFF(DrawParams, 1024*5) RenderingQueue;
RenderingQueue rendering_queues[LAYER_LAST] = { 0 };

// The image region is in pixel space of the image
void draw_quad_impl(DrawParams d, int line)
{
	d.line_number = line;
	Vec2 *points = d.quad.points;
	if (d.world_space)
	{
		for (int i = 0; i < 4; i++)
		{
			points[i] = world_to_screen(points[i]);
		}
	}

	if (d.do_clipping && d.world_space)
	{
		d.clip_to.upper_left = world_to_screen(d.clip_to.upper_left);
		d.clip_to.lower_right = world_to_screen(d.clip_to.lower_right);
	}

	// we've aplied the world space transform
	d.world_space = false;


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
		//dbgprint("Out of screen, cam aabb %f %f %f %f\n", cam_aabb.upper_left.X, cam_aabb.upper_left.Y, cam_aabb.lower_right.X, cam_aabb.lower_right.Y);
		//dbgprint("Points boundig box %f %f %f %f\n", points_bounding_box.upper_left.X, points_bounding_box.upper_left.Y, points_bounding_box.lower_right.X, points_bounding_box.lower_right.Y);
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

double anim_sprite_duration(AnimKind anim)
{
	AnimatedSprite *s = GET_TABLE_PTR(sprites, anim);
	return s->num_frames * s->time_per_frame;
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

void colorquad(bool world_space, Quad q, Color col)
{
	bool queue = false;
	if (col.A < 1.0f)
	{
		queue = true;
	}
	// y coord sorting for colorquad puts it below text for dialog panel
	draw_quad((DrawParams) { world_space, q, image_white_square, full_region(image_white_square), col, .layer = LAYER_UI });
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
bool in_screen_space = false;
void line(Vec2 from, Vec2 to, float line_width, Color color)
{
	colorquad(!in_screen_space, line_quad(from, to, line_width), color);
}

#ifdef DEVTOOLS
bool show_devtools = true;
#ifdef PROFILING
extern bool profiling;
#else
bool profiling;
#endif
#endif

Color debug_color = {1,0,0,1};

#define dbgcol(col) DeferLoop(debug_color = col, debug_color = RED)

void dbgsquare(Vec2 at)
{
#ifdef DEVTOOLS
	if (!show_devtools) return;
	colorquad(true, quad_centered(at, V2(3.0, 3.0)), debug_color);
#else
	(void)at;
#endif
}

void dbgbigsquare(Vec2 at)
{
#ifdef DEVTOOLS
	if (!show_devtools) return;
	colorquad(true, quad_centered(at, V2(20.0, 20.0)), BLUE);
#else
	(void)at;
#endif
}

void dbgline(Vec2 from, Vec2 to)
{
#ifdef DEVTOOLS
	if (!show_devtools) return;
	line(from, to, 0.5f, debug_color);
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

typedef struct TextParams
{
	bool world_space;
	bool dry_run;
	MD_String8 text;
	Vec2 pos;
	Color color;
	float scale;
	AABB clip_to; // if in world space, in world space. In space of pos given
	Color *colors; // color per character, if not null must be array of same length as text
	bool do_clipping;
} TextParams;

// returns bounds. To measure text you can set dry run to true and get the bounds
AABB draw_text(TextParams t)
{
	AABB bounds = { 0 };
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
				stbtt_GetBakedQuad(cdata, 512, 512, t.text.str[i]-32, &x, &y, &q, 1);
			float difference = y - old_y;
			y = old_y + difference;

			Vec2 size = V2(q.x1 - q.x0, q.y1 - q.y0);
			if (t.text.str[i] == '\n')
			{
#ifdef DEVTOOLS
				y += font_size*0.75f; // arbitrary, only debug t.text has newlines
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
					.upper_left = V2(q.s0, q.t0),
						.lower_right = V2(q.s1, q.t1),
				};
				PROFILE_SCOPE("Scaling font atlas region to img font size")
				{
					font_atlas_region.upper_left.X *= image_font_size.X;
					font_atlas_region.lower_right.X *= image_font_size.X;
					font_atlas_region.upper_left.Y *= image_font_size.Y;
					font_atlas_region.lower_right.Y *= image_font_size.Y;
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

						if (false) // drop shadow, don't really like it
							if (t.world_space)
							{
								Quad shadow_quad = to_draw;
								for (int i = 0; i < 4; i++)
								{
									shadow_quad.points[i] = AddV2(shadow_quad.points[i], V2(0.0, -1.0));
								}
								draw_quad((DrawParams) { t.world_space, shadow_quad, image_font, font_atlas_region, (Color) { 0.0f, 0.0f, 0.0f, 0.4f }, t.clip_to, .layer = LAYER_UI_FG, .do_clipping = t.do_clipping });
							}

						draw_quad((DrawParams) { t.world_space, to_draw, image_font, font_atlas_region, col, t.clip_to, .layer = LAYER_UI_FG, .do_clipping = t.do_clipping });
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

void draw_shadow_for(DrawParams d)
{
	Quad sheared_quad = d.quad;
	float height = d.quad.ur.y - d.quad.lr.y;
	Vec2 shear_addition = V2(-height*0.35f, -height*0.2f);
	sheared_quad.ul = AddV2(sheared_quad.ul, shear_addition);
	sheared_quad.ur = AddV2(sheared_quad.ur, shear_addition);
	d.quad = sheared_quad;
	d.tint = (Color) { 0, 0, 0, 0.2f };
	d.sorting_key -= 1;
	d.alpha_clip_threshold = 0.0f;
	dbgline(sheared_quad.ul, sheared_quad.ur);
	dbgline(sheared_quad.ur, sheared_quad.lr);
	dbgline(sheared_quad.lr, sheared_quad.ll);
	dbgline(sheared_quad.ll, sheared_quad.ul);
	draw_quad(d);
}

//void draw_animated_sprite(AnimatedSprite *s, double elapsed_time, bool flipped, Vec2 pos, Color tint)
void draw_animated_sprite(DrawnAnimatedSprite d)
{
	AnimatedSprite *s = GET_TABLE_PTR(sprites, d.anim);
	sg_image spritesheet_img = *GET_TABLE(anim_img_table, d.anim);

	d.pos = AddV2(d.pos, s->offset);
	int index = (int)floor(d.elapsed_time / s->time_per_frame) % s->num_frames;
	if (s->no_wrap)
	{
		index = (int)floor(d.elapsed_time / s->time_per_frame);
		if (index >= s->num_frames) index = s->num_frames - 1;
	}

	Quad q = quad_centered(d.pos, s->region_size);

	if (d.flipped)
	{
		swap(&q.points[0], &q.points[1]);
		swap(&q.points[3], &q.points[2]);
	}

	AABB region;
	region.upper_left = AddV2(s->start, V2(index * s->horizontal_diff_btwn_frames, 0.0f));
	float width = img_size(spritesheet_img).X;
	while (region.upper_left.X >= width)
	{
		region.upper_left.X -= width;
		region.upper_left.Y += s->region_size.Y;
	}
	region.lower_right = AddV2(region.upper_left, s->region_size);

	DrawParams drawn = (DrawParams) { true, q, spritesheet_img, region, d.tint, .sorting_key = sorting_key_at(d.pos), .layer = LAYER_WORLD, };
	if (!d.no_shadow) draw_shadow_for(drawn);
	draw_quad(drawn);
}

// gets aabbs overlapping the input aabb, including gs.entities and tiles
Overlapping get_overlapping(Level *l, AABB aabb)
{
	Overlapping to_return = { 0 };

	Quad q = quad_aabb(aabb);
	// the corners, jessie
	PROFILE_SCOPE("checking the corners")
		for (int i = 0; i < 4; i++)
		{
			TileCoord to_check = world_to_tilecoord(q.points[i]);
			TileInstance t = get_tile_layer(l, 2, to_check);
			if (is_tile_solid(t))
			{
				Overlap element = ((Overlap) { .is_tile = true, .t = t });
				//{ (&to_return)[(&to_return)->cur_index++] = element; assert((&to_return)->cur_index < ARRLEN((&to_return)->data)); }
				BUFF_APPEND(&to_return, element);
			}
		}

	// the gs.entities jessie
	PROFILE_SCOPE("checking the entities")
		ENTITIES_ITER(gs.entities)
		{
			if (!(it->is_character && it->is_rolling) && overlapping(aabb, entity_aabb(it)))
			{
				BUFF_APPEND(&to_return, (Overlap) { .e = it });
			}
		}

	return to_return;
}

typedef struct CollisionInfo
{
	bool happened;
	Vec2 normal;
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


// returns new pos after moving and sliding against collidable things
Vec2 move_and_slide(MoveSlideParams p)
{
	Vec2 collision_aabb_size = entity_aabb_size(p.from);
	Vec2 new_pos = AddV2(p.position, p.movement_this_frame);

	assert(collision_aabb_size.x > 0.0f);
	assert(collision_aabb_size.y > 0.0f);
	AABB at_new = centered_aabb(new_pos, collision_aabb_size);
	BUFF(AABB, 256) to_check = { 0 };



	// add tilemap boxes
	{
		Vec2 at_new_size_vector = SubV2(at_new.lower_right, at_new.upper_left);
		Vec2 points_to_check[] = {
			AddV2(at_new.upper_left, V2(0.0, 0.0)),
			AddV2(at_new.upper_left, V2(at_new_size_vector.X, 0.0)),
			AddV2(at_new.upper_left, V2(at_new_size_vector.X, at_new_size_vector.Y)),
			AddV2(at_new.upper_left, V2(0.0, at_new_size_vector.Y)),
		};
		for (int i = 0; i < ARRLEN(points_to_check); i++)
		{
			Vec2 *it = &points_to_check[i];
			TileCoord tilecoord_to_check = world_to_tilecoord(*it);

			if (is_tile_solid(get_tile_layer(&level_level0, 2, tilecoord_to_check)))
			{
				AABB t = tile_aabb(tilecoord_to_check);
				BUFF_APPEND(&to_check, t);
			}
		}
	}


	// add entity boxes
	if (!p.dont_collide_with_entities && !(p.from->is_character && p.from->is_rolling))
	{
		ENTITIES_ITER(gs.entities)
		{
			if (!(it->is_character && it->is_rolling) && it != p.from && !(it->is_npc && it->dead) && !it->is_item)
			{
				BUFF_APPEND(&to_check, centered_aabb(it->pos, entity_aabb_size(it)));
			}
		}
	}

	// here we do some janky C stuff to resolve collisions with the closest
	// box first, because doing so is a simple heuristic to avoid depenetrating and losing
	// sideways velocity. It's visual and I can't put diagrams in code so uh oh!

	typedef BUFF(AABB, 32) OverlapBuff;
	OverlapBuff actually_overlapping = { 0 };

	BUFF_ITER(AABB, &to_check)
	{
		if (overlapping(at_new, *it))
		{
			BUFF_APPEND(&actually_overlapping, *it);
		}
	}


	float smallest_distance = FLT_MAX;
	int smallest_aabb_index = 0;
	int i = 0;
	BUFF_ITER(AABB, &actually_overlapping)
	{
		float cur_dist = LenV2(SubV2(aabb_center(at_new), aabb_center(*it)));
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
	BUFF_ITER_I(AABB, &actually_overlapping, i)
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
	BUFF_ITER(AABB, &overlapping_smallest_first)
	{
		dbgcol(GREEN)
		{
			dbgrect(*it);
		}
	}


	//overlapping_smallest_first = actually_overlapping;

	BUFF_ITER(AABB, &actually_overlapping)
		dbgcol(WHITE)
		dbgrect(*it);

	BUFF_ITER(AABB, &overlapping_smallest_first)
		dbgcol(WHITE)
		dbgsquare(aabb_center(*it));

	CollisionInfo info = { 0 };
	for (int col_iter_i = 0; col_iter_i < 1; col_iter_i++)
		BUFF_ITER(AABB, &overlapping_smallest_first)
		{
			AABB to_depenetrate_from = *it;
			int iters_tried_to_push_apart = 0;
			while (overlapping(to_depenetrate_from, at_new) && iters_tried_to_push_apart < 500)
			{
				const float move_dist = 0.1f;

				info.happened = true;
				Vec2 from_point = aabb_center(to_depenetrate_from);
				Vec2 to_player = NormV2(SubV2(aabb_center(at_new), from_point));
				Vec2 compass_dirs[4] = {
					V2(1.0, 0.0),
					V2(-1.0, 0.0),
					V2(0.0,  1.0),
					V2(0.0, -1.0),
				};
				int closest_index = -1;
				float closest_dot = -99999999.0f;
				for (int i = 0; i < 4; i++)
				{
					float dot = DotV2(compass_dirs[i], to_player);
					if (dot > closest_dot)
					{
						closest_index = i;
						closest_dot = dot;
					}
				}
				assert(closest_index != -1);
				Vec2 move_dir = compass_dirs[closest_index];
				info.normal = move_dir;
				dbgvec(from_point, MulV2F(move_dir, 30.0f));
				Vec2 move = MulV2F(move_dir, move_dist);
				at_new.upper_left = AddV2(at_new.upper_left, move);
				at_new.lower_right = AddV2(at_new.lower_right, move);
				iters_tried_to_push_apart++;
			}
		}

	if (p.col_info_out) *p.col_info_out = info;

	Vec2 result_pos = aabb_center(at_new);
	dbgrect(centered_aabb(result_pos, collision_aabb_size));
	return result_pos;
}

float character_width(int ascii_letter, float text_scale)
{
	int advanceWidth;
	stbtt_GetCodepointHMetrics(&font, ascii_letter, &advanceWidth, 0);
	return (float)advanceWidth * font_scale * text_scale;
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
} PlacedWord;

typedef struct
{
	PlacedWord *first;
	PlacedWord *last;
} PlacedWordList;

PlacedWordList place_wrapped_words(MD_Arena *arena, MD_String8 text, float text_scale, float maximum_width)
{
	PlacedWordList to_return = {0};
	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);

	MD_String8List words = split_by_word(scratch.arena, text);
	Vec2 at_position = V2(0.0, 0.0);
	Vec2 cur = at_position;
	float space_size = character_width((int)' ', text_scale);
	float current_vertical_offset = 0.0f; // goes negative
	for(MD_String8Node *next_word = words.first; next_word; next_word = next_word->next)
	{
		if(next_word->string.size == 0)
		{
		}
		else
		{
			AABB word_bounds = draw_text((TextParams){false, true, next_word->string, V2(0.0, 0.0), .scale = text_scale});
			word_bounds.lower_right.x += space_size;
			float next_x_position = cur.x + aabb_size(word_bounds).x;
			if(next_x_position - at_position.x > maximum_width)
			{
				current_vertical_offset -= font_line_advance*text_scale*1.1f; // the 1.1 is just arbitrary padding because it looks too crowded otherwise
				cur = AddV2(at_position, V2(0.0f, current_vertical_offset));
				next_x_position = cur.x + aabb_size(word_bounds).x;
			}

			PlacedWord *new_placed = MD_PushArray(arena, PlacedWord, 1);
			new_placed->text = next_word->string;
			new_placed->lower_left_corner = cur;

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
	int last_speech_index = -1;
	BUFF_ITER_I(Memory, &npc->memories, i)
	{
		if(it->context.author_npc_kind == npc->npc_kind)
		{
			last_speech_index = i;
		}
	}
	
	if(last_speech_index == -1)
	{
		return (MD_String8){0};
	}
	else
	{
		return MD_S8(npc->memories.data[last_speech_index].speech, npc->memories.data[last_speech_index].speech_length);
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
	MD_u8 speech[MAX_SENTENCE_LENGTH];
	int speech_length;
	DialogElementKind kind;
	bool was_eavesdropped;
	NpcKind who_said_it;
	bool was_last_said;
} DialogElement;

MD_String8List last_said_without_unsaid_words(MD_Arena *arena, Entity *it)
{
	MD_ArenaTemp scratch = MD_GetScratch(&arena, 1);
	MD_String8List by_word = split_by_word(scratch.arena, last_said_sentence(it));
	MD_String8Node *cur = by_word.first;
	MD_String8List without_unsaid_words = {0};
	for(int i = 0; i < by_word.node_count; i++)
	{
		if(i >= it->words_said)
		{
			break;
		}
		else
		{
			assert(cur);
			MD_S8ListPush(arena, &without_unsaid_words, cur->string);
			cur = cur->next;
		}
	}
	MD_ReleaseScratch(scratch);
	return without_unsaid_words;
}

// Some perceptions can have multiple dialog elements.
// Like item give perceptions that have an action with both dialog
// and an argument. So worst case every perception has 2 dialog
// elements right now is why it's *2
typedef BUFF(DialogElement, REMEMBERED_MEMORIES*2) Dialog;
Dialog produce_dialog(Entity *talking_to, bool character_names)
{
	MD_ArenaTemp scratch = MD_GetScratch(0, 0);
	assert(talking_to->is_npc);
	Dialog to_return = { 0 };
	BUFF_ITER(Memory, &talking_to->memories)
	{
		if(!it->is_error && !it->context.dont_show_to_player)
		{
			if(it->speech_length > 0)
			{
				DialogElement new_element = { .who_said_it = it->context.author_npc_kind, .was_eavesdropped = it->context.eavesdropped_from_party };

				MD_String8 my_speech = MD_S8(it->speech, it->speech_length);
				if(last_said_sentence(talking_to).str == it->speech)
				{
					new_element.was_last_said = true;
					my_speech = MD_S8ListJoin(scratch.arena, last_said_without_unsaid_words(scratch.arena, talking_to), &(MD_StringJoin){.mid = MD_S8Lit(" ")});
				}

				MD_String8 dialog_speech = FmtWithLint(scratch.arena, "%s: %.*s", characters[it->context.author_npc_kind].name, MD_S8VArg(my_speech));

				memcpy(new_element.speech, dialog_speech.str, dialog_speech.size);
				new_element.speech_length = (int)dialog_speech.size;

				if(it->context.author_npc_kind == NPC_Player)
				{
					new_element.kind = DELEM_PLAYER;
				}
				else
				{
					new_element.kind = DELEM_NPC;
				}

				BUFF_APPEND(&to_return, new_element);
			}
		}
	}
	MD_ReleaseScratch(scratch);
	return to_return;
}

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

void draw_dialog_panel(Entity *talking_to, float alpha)
{
	MD_ArenaTemp scratch = MD_GetScratch(0, 0);

	float panel_width = 250.0f;
	float panel_height = 150.0f;
	float panel_vert_offset = 30.0f;


	AABB dialog_panel = (AABB) {
		.upper_left = AddV2(talking_to->pos, V2(-panel_width / 2.0f, panel_vert_offset + panel_height)),
			.lower_right = AddV2(talking_to->pos, V2(panel_width / 2.0f, panel_vert_offset)),
	};
	AABB constrict_to = world_cam_aabb();
	dialog_panel.upper_left.x = fmaxf(constrict_to.upper_left.x, dialog_panel.upper_left.x);
	dialog_panel.lower_right.y = fmaxf(constrict_to.lower_right.y, dialog_panel.lower_right.y);
	dialog_panel.upper_left.y = fminf(constrict_to.upper_left.y, dialog_panel.upper_left.y);
	dialog_panel.lower_right.x = fminf(constrict_to.lower_right.x, dialog_panel.lower_right.x);

	if (aabb_is_valid(dialog_panel))
	{
		Quad dialog_quad = quad_aabb(dialog_panel);
		float line_width = 2.0f;
		Quad panel_quad = dialog_quad;
		{
			float inset = line_width;
			panel_quad.ul = AddV2(panel_quad.ul, V2(inset,  -inset));
			panel_quad.ll = AddV2(panel_quad.ll, V2(inset,   inset));
			panel_quad.lr = AddV2(panel_quad.lr, V2(-inset,  inset));
			panel_quad.ur = AddV2(panel_quad.ur, V2(-inset, -inset));
		}
		colorquad(true, panel_quad, (Color) { 1.0f, 1.0f, 1.0f, 0.7f*alpha });
		Color line_color = (Color) { 0, 0, 0, alpha };
		line(AddV2(dialog_quad.ul, V2(-line_width, 0.0)), AddV2(dialog_quad.ur, V2(line_width, 0.0)), line_width, line_color);
		line(dialog_quad.ur, dialog_quad.lr, line_width, line_color);
		line(AddV2(dialog_quad.lr, V2(line_width, 0.0)), AddV2(dialog_quad.ll, V2(-line_width, 0.0)), line_width, line_color);
		line(dialog_quad.ll, dialog_quad.ul, line_width, line_color);

		float padding = 5.0f;
		dialog_panel.upper_left = AddV2(dialog_panel.upper_left, V2(padding, -padding));
		dialog_panel.lower_right = AddV2(dialog_panel.lower_right, V2(-padding, padding));

		if (aabb_is_valid(dialog_panel))
		{
			float new_line_height = dialog_panel.lower_right.Y;

			Dialog dialog = produce_dialog(talking_to, false);
			if (dialog.cur_index > 0)
			{
				for (int i = dialog.cur_index - 1; i >= 0; i--)
				{
					DialogElement *it = &dialog.data[i];
					{
						Color color;
						// decide color
						{
							if(it->was_eavesdropped)
							{
								color = colhex(0x9341a3);
							}
							else
							{
								if (it->kind == DELEM_PLAYER)
								{
									color = BLACK;
								}
								else if (it->kind == DELEM_NPC)
								{
									color = colhex(0x345e22);
								}
								else if (it->kind == DELEM_ACTION_DESCRIPTION)
								{
									color = colhex(0xb5910e);
								}
								else
								{
									assert(false);
								}
							}
						}

						color = blendalpha(color, alpha);
						const float text_scale = 0.5f;
						PlacedWordList wrapped = place_wrapped_words(scratch.arena, MD_S8(it->speech, it->speech_length), text_scale, dialog_panel.lower_right.x - dialog_panel.upper_left.x);
						float line_vertical_offset = -wrapped.last->lower_left_corner.y;
						translate_words_by(wrapped, V2(0.0, line_vertical_offset));
						translate_words_by(wrapped, V2(dialog_panel.upper_left.x, new_line_height));
						new_line_height += line_vertical_offset + font_line_advance * text_scale;

						AABB no_clip_curly_things = dialog_panel;
						no_clip_curly_things.lower_right.y -= padding;
						for(PlacedWord *cur = wrapped.first; cur; cur = cur->next)
						{
							draw_text((TextParams){true, false, cur->text, cur->lower_left_corner, color, text_scale, .clip_to = no_clip_curly_things, .do_clipping = true,});
						}
					}
				}
			}

			dbgrect(dialog_panel);
		}
	}

	MD_ReleaseScratch(scratch);
}


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
		draw_quad((DrawParams) { false, quad_aabb(button_aabb), IMG(image_white_square), blendalpha(WHITE, button_alpha), .layer = LAYER_UI, });
		draw_centered_text((TextParams) { false, false, text, aabb_center(button_aabb), BLACK, text_scale, .clip_to = button_aabb, .do_clipping = true });
	}

	hmput(imui_state, key, state);
	return to_return;
}

#define imbutton(...) imbutton_key(__VA_ARGS__, __LINE__, unwarped_dt, false)

void draw_item(bool world_space, ItemKind kind, AABB in_aabb, float alpha)
{
	Quad drawn = quad_aabb(in_aabb);
	if (kind == ITEM_Tripod)
	{
		draw_quad((DrawParams) { world_space, drawn, IMG(image_tripod), blendalpha(WHITE, alpha), .layer = LAYER_UI_FG });
	}
	else if (kind == ITEM_Boots)
	{
		draw_quad((DrawParams) { world_space, drawn, IMG(image_boots), blendalpha(WHITE, alpha), .layer = LAYER_UI_FG });
	}
	else if (kind == ITEM_Chalice)
	{
		draw_quad((DrawParams) { world_space, drawn, IMG(image_chalice), blendalpha(WHITE, alpha), .layer = LAYER_UI_FG });
	}
	else if (kind == ITEM_GoldCoin)
	{
		draw_quad((DrawParams) { world_space, drawn, IMG(image_gold_coin), blendalpha(WHITE, alpha), .layer = LAYER_UI_FG });
	}
	else if (kind == ITEM_WhiteSquare)
	{
		colorquad(world_space, drawn, blendalpha(WHITE, alpha));
	}
	else
	{
		assert(false);
	}
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
		sg_apply_pipeline(state.pip);

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

		// better for vertical aspect ratios
		if (screen_size().x < 0.7f*screen_size().y)
		{
			cam.scale = 2.3f;
		}
		else
		{
			cam.scale = 2.0f;
		}


		uint64_t time_start_frame = stm_now();

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

		sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
		sg_apply_pipeline(state.pip);

		Level *cur_level = &level_level0;

		// Draw Tilemap draw tilemap tilemap drawing
#if 1
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

		assert(player != NULL);

		// gameplay processing loop, do multiple if lagging
		// these are static so that, on frames where no gameplay processing is necessary and just rendering, the rendering uses values from last frame
		static Entity *interacting_with = 0; // used by rendering to figure out who to draw dialog box on
		static bool player_in_combat = false;
		const float dialog_interact_size = 2.5f * TILE_SIZE;

		float speed_target;
		// pausing the game
		if (player->in_conversation_mode || gs.won)
		{
			speed_target = 0.0f;
		}
		else
		{
			speed_target = 1.0f;
		}
		speed_factor = Lerp(speed_factor, unwarped_dt*10.0f, speed_target);
		if (fabsf(speed_factor - speed_target) <= 0.05f)
		{
			speed_factor = speed_target;
		}
		int num_timestep_loops = 0;
		// restore the pressed state after gameplay loop so pressed input events can be processed in the
		// rendering correctly as well
		PressedState before_gameplay_loops = pressed;
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

				// process gs.entities
				player_in_combat = false; // in combat set by various enemies when they fight the player
				PROFILE_SCOPE("entity processing")
				{
					if(player->knighted)
					{
						gs.won = true;
					}
					ENTITIES_ITER(gs.entities)
					{
						assert(!(it->exists && it->generation == 0));
						if (it->is_npc)
						{
							if (it->gen_request_id != 0)
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
#error "Don't know how to do this stuff on this platform."
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
										// done! we can get the string
										char sentence_cstr[MAX_SENTENCE_LENGTH] = { 0 };
#ifdef WEB
										EM_ASM( {
											let generation = get_generation_request_content($0);
											stringToUTF8(generation, $1, $2);
										}, it->gen_request_id, sentence_cstr, ARRLEN(sentence_cstr) - 1); // I think minus one for null terminator...
#endif

#ifdef DESKTOP
										memcpy(sentence_cstr, get_by_id(it->gen_request_id)->generated, get_by_id(it->gen_request_id)->generated_length);
#endif

										MD_String8 sentence_str = MD_S8CString(sentence_cstr);

										// parse out from the sentence NPC action and dialog
										Action out = {0};

										MD_ArenaTemp scratch = MD_GetScratch(0, 0);
										MD_String8 parse_response = parse_chatgpt_response(scratch.arena, it, sentence_str, &out);

										if (parse_response.size == 0)
										{
											perform_action(it, out);
										}
										else
										{
											remember_error(it, parse_response);
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
										// need somethin better here. Maybe each sentence has to know if it's player or NPC, that way I can remove the player's dialog
										Action to_perform = {0};
										MD_String8 speech_mdstring = MD_S8Lit("I'm not sure...");
										memcpy(to_perform.speech, speech_mdstring.str, speech_mdstring.size);
										to_perform.speech_length = (int)speech_mdstring.size;
										perform_action(it, to_perform);
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


						if (fabsf(it->vel.x) > 0.01f)
							it->facing_left = it->vel.x < 0.0f;

						if (it->dead)
						{
							it->dead_time += dt;
						}

						it->being_hovered = false;
						if (player->in_conversation_mode)
						{

							if (has_point(entity_aabb(it), screen_to_world(mouse_pos)))
							{
								it->being_hovered = true;
								if (pressed.mouse_down)
								{
									player->talking_to = frome(it);
									player->state = CHARACTER_TALKING;
								}
							}
						}

						if (it->is_npc)
						{
							// character speech animation text input
							if (true)
							{
								MD_ArenaTemp scratch = MD_GetScratch(0, 0);

								MD_String8List split = split_by_word(scratch.arena, last_said_sentence(it));
								if(it->words_said <= split.node_count)
								{
									it->word_anim_in += CHARACTERS_PER_SEC * unwarped_dt;
									int characters_in_animating_word = 0;
									MD_String8Node *cur = split.first;
									for(int i = 0; i < it->words_said + 1; i++)
									{
										if(cur)
										{
											if(i >= it->words_said - 1)
											{
												characters_in_animating_word = (int)cur->string.size;
												break;
											}
											cur = cur->next;
										}
									}
									if((int)it->word_anim_in + 1 > characters_in_animating_word)
									{
										it->words_said += 1;
										if(it->words_said < split.node_count)
										{
											it->word_anim_in = 0;
										}
										float dist = LenV2(SubV2(it->pos, player->pos));
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

								MD_ReleaseScratch(scratch);
							}


							if(it->standing == STANDING_JOINED)
							{
								int place_in_line = 1;
								Entity *e = it;
								ENTITIES_ITER(gs.entities)
								{
									if(it->is_npc && it->standing == STANDING_JOINED)
									{
										if(it == e) break;
										place_in_line += 1;
									}
								}

								Vec2 target = get_point_along_trail(BUFF_MAKEREF(&player->position_history), (float)place_in_line * TILE_SIZE);

								it->pos = LerpV2(it->pos, dt*5.0f, target);
							}


							// A* code
							if(false)
								if (it->standing == STANDING_FIGHTING || it->standing == STANDING_JOINED)
								{
									Entity *targeting = player;

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
										float got_there_tolerance = max_coord(entity_aabb_size(player))*1.5f;
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
																	Overlapping overlapping_at_want = get_overlapping(&level_level0, entity_aabb_at(e, cur_pos));
																	BUFF_ITER(Overlap, &overlapping_at_want) if (is_overlap_collision(*it) && !(it->e && it->e == e)) would_block_me = true;
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

							if (it->npc_kind == NPC_OldMan)
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
							else if (npc_is_skeleton(it))
							{
								if (it->dead)
								{
								}
								else
								{
								} // skelton combat and movement
							}
							else if (it->npc_kind == NPC_Death)
							{
							}
#if 0
							else if (it->npc_kind == DEATH)
							{
								draw_animated_sprite(&death_idle, elapsed_time, true, AddV2(it->pos, V2(0, 30.0f)), col);
							}
							else if (it->npc_kind == MERCHANT)
							{
								draw_animated_sprite(&merchant_idle, elapsed_time, true, AddV2(it->pos, V2(0, 30.0f)), col);
							}
#endif
							else if (it->npc_kind == NPC_GodRock)
							{
							}
							else if (it->npc_kind == NPC_Edeline)
							{
							}
							else if (it->npc_kind == NPC_TheGuard)
							{
								if (it->moved)
								{
									it->walking = true;
									Vec2 towards = SubV2(it->target_goto, it->pos);
									if (LenV2(towards) > 1.0f)
									{
										it->pos = LerpV2(it->pos, dt*5.0f, it->target_goto);
									}
								}
								else
								{
									it->walking = false;
								}
							}
							else if (it->npc_kind == NPC_TheKing)
							{
							}
							else if (it->npc_kind == NPC_Red)
							{
							}
							else if (it->npc_kind == NPC_Blue)
							{
							}
							else if (it->npc_kind == NPC_Davis)
							{
							}
							else if (it->npc_kind == NPC_TheBlacksmith)
							{
							}
							else if (it->npc_kind == NPC_Bill)
							{
							}
							else if (it->npc_kind == NPC_Jester)
							{
							}
							else if (it->npc_kind == NPC_PeaceTotem)
							{
							}
							else
							{
								assert(false);
							}

							if (it->damage >= entity_max_damage(it))
							{
								if (npc_is_skeleton(it))
								{
									it->dead = true;
								}
								else
								{
									it->destroy = true;
								}
							}
						}
						else if (it->is_item)
						{
							if (it->held_by_player)
							{
								Vec2 held_spot = V2(15.0f * (player->facing_left ? -1.0f : 1.0f), 7.0f);
								it->pos = AddV2(player->pos, held_spot);
							}
							else
							{
								it->vel = LerpV2(it->vel, dt*7.0f, V2(0.0f, 0.0f));
								CollisionInfo info = { 0 };
								it->pos = move_and_slide((MoveSlideParams) { it, it->pos, MulV2F(it->vel, pixels_per_meter * dt), .dont_collide_with_entities = true, .col_info_out = &info });
								if (info.happened) it->vel = ReflectV2(it->vel, info.normal);
							}

							//draw_quad((DrawParams){true, it->pos, IMG(image_white_square)
						}
						else if (it->is_character)
						{
						}
						else if (it->is_prop)
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
					ENTITIES_ITER(gs.entities)
					{
						if (it->destroy)
						{
							int gen = it->generation;
							*it = (Entity) { 0 };
							it->generation = gen;
						}
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
								it->perceptions_dirty = false; // needs to be in beginning because they might be redirtied by the new perception
								MD_String8 prompt_str = {0};
#ifdef DO_CHATGPT_PARSING
								prompt_str = generate_chatgpt_prompt(frame_arena, it);
#else
								generate_prompt(it, &prompt);
#endif
								Log("Sending request with prompt `%.*s`\n", MD_S8VArg(prompt_str));

#ifdef WEB
								// fire off generation request, save id
								MD_ArenaTemp scratch = MD_GetScratch(0, 0);
								MD_String8 terminated_completion_url = FmtWithLint(scratch.arena, "%s:%d/completion\0", SERVER_DOMAIN, SERVER_PORT);
								int req_id = EM_ASM_INT( {
										return make_generation_request(UTF8ToString($1, $2), UTF8ToString($0));
										}, terminated_completion_url.str, prompt_str.str, prompt_str.size);
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
								if(mocking_the_ai_response)
								{
									if(false)
									{
										const char *argument = 0;
										MD_String8List dialog_elems = {0};
										ActionKind act = ACT_none;

										it->times_talked_to++;
										if(it->memories.data[it->memories.cur_index-1].context.eavesdropped_from_party)
										{
											PushWithLint(scratch.arena, &dialog_elems, "Responding to eavesdropped: ");
										}
										if(it->npc_kind == NPC_TheBlacksmith && it->standing != STANDING_JOINED)
										{
											assert(it->times_talked_to == 1);
											act = ACT_joins_player;
											PushWithLint(scratch.arena, &dialog_elems, "Joining you...");
										}
										else
										{
											PushWithLint(scratch.arena, &dialog_elems, "%d times talked", it->times_talked_to);
										}


										MD_StringJoin join = {0};
										MD_String8 dialog = MD_S8ListJoin(scratch.arena, dialog_elems, &join);
										if (argument)
										{
											ai_response = FmtWithLint(scratch.arena, "ACT_%s(%s) \"%.*s\"", actions[act].name, argument, MD_S8VArg(dialog));
										}
										else
										{
											ai_response = FmtWithLint(scratch.arena, "ACT_%s \"%.*s\"", actions[act].name, MD_S8VArg(dialog));
										}
									}
									else
									{
										ai_response = MD_S8Lit(" Within the player's party, while the player is talking to Meld, you hear: ACT_none \"Better have a good reason for bothering me. fjdskfjdsakfjsdakf\"");
									}
								}
								else
								{
									MD_String8 post_request_body = FmtWithLint(scratch.arena, "|%.*s", MD_S8VArg(prompt_str));
									it->gen_request_id = make_generation_request(post_request_body);
								}

								// something to mock
								if(ai_response.size > 0)
								{
									Log("Mocking...\n");
									Action a = {0};
									MD_String8 error_message = MD_S8Lit("Something really bad happened bro. File " STRINGIZE(__FILE__) " Line " STRINGIZE(__LINE__));
									if(succeeded)
									{
										error_message = parse_chatgpt_response(scratch.arena, it, ai_response, &a);
									}

									if(mocking_the_ai_response)
									{
										assert(succeeded);
										assert(error_message.size == 0);
										perform_action(it, a);
									}
									else
									{
										if(succeeded)
										{
											if (error_message.size == 0)
											{
												perform_action(it, a);
											}
											else
											{
												Log("There was an error with the AI: %.*s", MD_S8VArg(error_message));
												remember_error(it, error_message);
											}
										}
									}
								}

								MD_ReleaseScratch(scratch);
#undef SAY
#endif
							}
							else
							{
								assert(false);
							}
						}
					}
				}

				PROFILE_SCOPE("process player")
				{
					// do dialog
					Entity *closest_interact_with = 0;
					{
						// find closest to talk to
						{
							AABB dialog_rect = centered_aabb(player->pos, V2(dialog_interact_size , dialog_interact_size));
							dbgrect(dialog_rect);
							Overlapping possible_dialogs = get_overlapping(cur_level, dialog_rect);
							float closest_interact_with_dist = INFINITY;
							BUFF_ITER(Overlap, &possible_dialogs)
							{
								bool entity_talkable = true;
								if (entity_talkable) entity_talkable = entity_talkable && !it->is_tile;
								if (entity_talkable) entity_talkable = entity_talkable && it->e->is_npc;
								//if(entity_talkable) entity_talkable = entity_talkable && !(it->e->npc_kind == NPC_Skeleton);
#ifdef WEB
								if (entity_talkable) entity_talkable = entity_talkable && it->e->gen_request_id == 0;
#endif

								if (entity_talkable)
								{
									float dist = LenV2(SubV2(it->e->pos, player->pos));
									if (dist < closest_interact_with_dist)
									{
										closest_interact_with_dist = dist;
										closest_interact_with = it->e;
									}
								}
							}
						}


						interacting_with = closest_interact_with;
						if (player->state == CHARACTER_TALKING)
						{
							interacting_with = gete(player->talking_to);
							assert(interacting_with);
						}


						// maybe get rid of talking to
						if (player->state == CHARACTER_TALKING)
						{
							if (gete(player->talking_to) == 0)
							{
								player->state = CHARACTER_IDLE;
							}
						}
						else
						{
							player->talking_to = (EntityRef) { 0 };
						}
					}

					if (interact)
					{
						if (player->state == CHARACTER_TALKING)
						{
							// don't add extra stuff to be done when changing state because in several
							// places it's assumed to end dialog I can just do player->state = CHARACTER_IDLE
							player->state = CHARACTER_IDLE;
						}
						else if (closest_interact_with)
						{
							if (closest_interact_with->is_npc)
							{
								if(closest_interact_with->npc_kind == NPC_PeaceTotem)
								{
									if(player->peace_tokens >= PEACE_TOKENS_NEEDED)
									{
										gs.won = true;
									}
									else
									{
										closest_interact_with->red_fade = 1.0f;
									}
								}
								else
								{
									// begin dialog with closest npc
									player->state = CHARACTER_TALKING;
									player->talking_to = frome(closest_interact_with);
								}
							}
							else
							{
								assert(false);
							}
						}
					}

					float speed = 0.0f;
					{
						Vec2 target_vel = { 0 };

						if (player->state == CHARACTER_WALKING)
						{
							speed = PLAYER_SPEED;
							if (player->is_rolling) speed = PLAYER_ROLL_SPEED;

							if (LenV2(movement) == 0.0)
							{
								player->state = CHARACTER_IDLE;
							}
							else
							{
							}
						}
						else if (player->state == CHARACTER_IDLE)
						{
							if (LenV2(movement) > 0.01) player->state = CHARACTER_WALKING;
						}
						else if (player->state == CHARACTER_TALKING)
						{
						}
						else
						{
							assert(false); // unknown character state? not defined how to process
						}
					} // not time stopped

					// velocity processing
					{
						Vec2 target_vel = MulV2F(movement, pixels_per_meter * speed);
						player->vel = LerpV2(player->vel, dt * 15.0f, target_vel);
						player->pos = move_and_slide((MoveSlideParams) { player, player->pos, MulV2F(player->vel, dt) });

						bool should_append = false;

						// make it so no snap when new points added
						if(player->position_history.cur_index > 0)
						{
							player->position_history.data[player->position_history.cur_index - 1] = player->pos;
						}

						if(player->position_history.cur_index > 2)
						{
							should_append = LenV2(SubV2(player->position_history.data[player->position_history.cur_index - 2], player->pos)) > TILE_SIZE;
						}
						else
						{
							should_append = true;
						}
						if(should_append) BUFF_QUEUE_APPEND(&player->position_history, player->pos);

					}
					// health
					if (player->damage >= 1.0)
					{
						reset_level();
					}
				}
				pressed = (PressedState) { 0 };
				interact = false;
			} // while loop

            last_frame_gameplay_processing_time = stm_sec(stm_diff(stm_now(), time_start_gameplay_processing));
		}
		pressed = before_gameplay_loops;


		PROFILE_SCOPE("render player") // draw character draw player render character
		{
			DrawnAnimatedSprite to_draw = { 0 };

			if(player->position_history.cur_index > 0)
			{
				float trail_len = get_total_trail_len(BUFF_MAKEREF(&player->position_history));
				if(trail_len > 0.0f) // fmodf returns nan
				{
					float along = fmodf((float)elapsed_time*100.0f, 200.0f);
					Vec2 at = get_point_along_trail(BUFF_MAKEREF(&player->position_history), along);
					dbgbigsquare(at);
					dbgbigsquare(get_point_along_trail(BUFF_MAKEREF(&player->position_history), 50.0f));
				}
				BUFF_ITER_I(Vec2, &player->position_history, i)
				{
					if(i == player->position_history.cur_index - 1) 
					{
					}
					else
					{
						dbgline(*it, player->position_history.data[i + 1]);
					}
				}
			}

			Vec2 character_sprite_pos = AddV2(player->pos, V2(0.0, 20.0f));
			// if somebody, show their dialog panel
			if (interacting_with)
			{
				// interaction keyboard hint
				if (!mobile_controls)
				{
					float size = 100.0f;
					Vec2 midpoint = MulV2F(AddV2(interacting_with->pos, player->pos), 0.5f);
					draw_quad((DrawParams) { true, quad_centered(AddV2(midpoint, V2(0.0, 5.0f + sinf((float)elapsed_time*3.0f)*5.0f)), V2(size, size)), IMG(image_e_icon), blendalpha(WHITE, clamp01(1.0f - learned_e)), .layer = LAYER_UI_FG });
				}

				// interaction circle
				draw_quad((DrawParams) { true, quad_centered(interacting_with->pos, V2(TILE_SIZE, TILE_SIZE)), image_hovering_circle, full_region(image_hovering_circle), WHITE, .layer = LAYER_UI });
			}

			if (player->state == CHARACTER_WALKING)
			{
				to_draw = (DrawnAnimatedSprite) { ANIM_knight_running, elapsed_time, player->facing_left, character_sprite_pos, WHITE };
			}
			else if (player->state == CHARACTER_IDLE)
			{
				to_draw = (DrawnAnimatedSprite) { ANIM_knight_idle, elapsed_time, player->facing_left, character_sprite_pos, WHITE };
			}
			else if (player->state == CHARACTER_TALKING)
			{
				to_draw = (DrawnAnimatedSprite) { ANIM_knight_idle, elapsed_time, player->facing_left, character_sprite_pos, WHITE };
			}
			else
			{
				assert(false); // unknown character state? not defined how to draw
			}

			// hurt vignette
			if (player->damage > 0.0)
			{
				draw_quad((DrawParams) { false, (Quad) { .ul = V2(0.0f, screen_size().Y), .ur = screen_size(), .lr = V2(screen_size().X, 0.0f) }, image_hurt_vignette, full_region(image_hurt_vignette), (Color) { 1.0f, 1.0f, 1.0f, player->damage }, .layer = LAYER_SCREENSPACE_EFFECTS, });
			}

			player->anim_change_timer += dt;
			if (player->anim_change_timer >= 0.05f)
			{
				player->anim_change_timer = 0.0f;
				player->cur_animation = to_draw.anim;
			}
			to_draw.anim = player->cur_animation;

			if (to_draw.anim)
			{
				draw_animated_sprite(to_draw);
			}
		}

		// render gs.entities render entities
		PROFILE_SCOPE("entity rendering")
			ENTITIES_ITER(gs.entities)
			{
				if (it->gen_request_id != 0)
				{
					draw_quad((DrawParams) { true, quad_centered(AddV2(it->pos, V2(0.0, 50.0)), V2(100.0, 100.0)), IMG(image_thinking), WHITE });
				}

				Color col = LerpV4(WHITE, it->damage, RED);
				if (it->is_npc)
				{
					// health bar
					{
						Vec2 health_bar_size = V2(TILE_SIZE, 0.1f * TILE_SIZE);
						float health_bar_progress = 1.0f - (it->damage / entity_max_damage(it));
						Vec2 health_bar_center = AddV2(it->pos, V2(0.0f, -entity_aabb_size(it).y));
						Vec2 bar_upper_left = AddV2(health_bar_center, MulV2F(health_bar_size, -0.5f));
						draw_quad((DrawParams) { true, quad_at(bar_upper_left, health_bar_size), IMG(image_white_square), BROWN });
						draw_quad((DrawParams) { true, quad_at(bar_upper_left, V2(health_bar_size.x * health_bar_progress, health_bar_size.y)), IMG(image_white_square), GREEN });
					}

					float dist = LenV2(SubV2(it->pos, player->pos));
					dist -= 10.0f; // radius around point where dialog is completely opaque
					float max_dist = dialog_interact_size / 2.0f;
					float alpha = 1.0f - (float)clamp(dist / max_dist, 0.0, 1.0);
					if (gete(player->talking_to) == it && player->state == CHARACTER_TALKING) alpha = 0.0f;
					if (it->being_hovered)
					{
						draw_quad((DrawParams) { true, quad_centered(it->pos, V2(TILE_SIZE, TILE_SIZE)), IMG(image_hovering_circle), WHITE });
						alpha = 1.0f;
					}


					it->dialog_panel_opacity = Lerp(it->dialog_panel_opacity, unwarped_dt*10.0f, alpha);
					draw_dialog_panel(it, it->dialog_panel_opacity);

					if (it->npc_kind == NPC_OldMan)
					{
						bool face_left = SubV2(player->pos, it->pos).x < 0.0f;
						draw_animated_sprite((DrawnAnimatedSprite) { ANIM_old_man_idle, elapsed_time, face_left, it->pos, col });
					}
					else if (npc_is_skeleton(it))
					{
						Color col = WHITE;
						if (it->dead)
						{
							draw_animated_sprite((DrawnAnimatedSprite) { ANIM_skeleton_die, it->dead_time, it->facing_left, it->pos, col });
						}
						else
						{
							if (it->swing_timer > 0.0)
							{
								// swinging sword
								draw_animated_sprite((DrawnAnimatedSprite) { ANIM_skeleton_swing_sword, it->swing_timer, it->facing_left, it->pos, col });
							}
							else
							{
								if (it->walking)
								{
									draw_animated_sprite((DrawnAnimatedSprite) { ANIM_skeleton_run, elapsed_time, it->facing_left, it->pos, col });
								}
								else
								{
									draw_animated_sprite((DrawnAnimatedSprite) { ANIM_skeleton_idle, elapsed_time, it->facing_left, it->pos, col });
								}
							}
						}
					}
					else if (it->npc_kind == NPC_Death)
					{
						draw_animated_sprite((DrawnAnimatedSprite) { ANIM_death_idle, elapsed_time, true, AddV2(it->pos, V2(0, 30.0f)), col });
					}
					else if (it->npc_kind == NPC_GodRock)
					{
						Vec2 prop_size = V2(46.0f, 40.0f);
						DrawParams d = (DrawParams) { true, quad_centered(AddV2(it->pos, V2(-0.0f, 0.0)), prop_size), image_props_atlas, aabb_at_yplusdown(V2(15.0f, 219.0f), prop_size), WHITE, .sorting_key = sorting_key_at(AddV2(it->pos, V2(0.0f, 20.0f))), .alpha_clip_threshold = 0.7f, .layer = LAYER_WORLD, };
						draw_shadow_for(d);
						draw_quad(d);
					}
					else if (npc_is_knight_sprite(it))
					{
						Color tint = WHITE;
						if (it->npc_kind == NPC_TheGuard)
						{
							tint = colhex(0xa84032);
						}
						else if (it->npc_kind == NPC_Edeline)
						{
							tint = colhex(0x8c34eb);
						}
						else if (it->npc_kind == NPC_TheKing)
						{
							tint = colhex(0xf0be1d);
						}
						else if (it->npc_kind == NPC_TheBlacksmith)
						{
							tint = colhex(0x5c5c5c);
						}
						else if (it->npc_kind == NPC_Red)
						{
							tint = colhex(0xf56f42);
						}
						else if (it->npc_kind == NPC_Blue)
						{
							tint = colhex(0x1153d6);
						}
						else if (it->npc_kind == NPC_Davis)
						{
							tint = colhex(0x8f8f8f);
						}
						else if (it->npc_kind == NPC_Bill)
						{
							tint = colhex(0x49d14b);
						}
						else if (it->npc_kind == NPC_Jester)
						{
							tint = colhex(0x49d14b);
						}
						else
						{
							assert(false);
						}
						draw_animated_sprite((DrawnAnimatedSprite) { ANIM_knight_idle, elapsed_time, true, AddV2(it->pos, V2(0, 30.0f)), tint });
					}
					else if(it->npc_kind == NPC_PeaceTotem)
					{
						DrawParams d = (DrawParams) { true, quad_centered(it->pos, V2(TILE_SIZE, TILE_SIZE)), IMG(image_peace_totem), WHITE, .layer = LAYER_WORLD, };
						draw_shadow_for(d);
						draw_quad(d);

						it->red_fade = Lerp(it->red_fade, dt*2.0f, 0.0f);

						float fade_requirements = Lerp(0.0f, 1.0f - clamp01(LenV2(SubV2(player->pos, it->pos))/(TILE_SIZE*4.0f)), 1.0f);

						MD_ArenaTemp scratch = MD_GetScratch(0, 0);
						draw_centered_text((TextParams){true, false, FmtWithLint(scratch.arena, "%d/%d", player->peace_tokens, PEACE_TOKENS_NEEDED), AddV2(it->pos, V2(0.0, 32.0)), blendalpha(blendcolors(WHITE, it->red_fade, RED), fade_requirements), (1.0f / cam.scale)*(1.0f + it->red_fade*0.5f)});
						MD_ReleaseScratch(scratch); 
					}
					else
					{
						assert(false);
					}
				}
				else if (it->is_item)
				{
					draw_item(true, it->item_kind, centered_aabb(it->pos, V2(15.0f, 15.0f)), 1.0f);
				}
				else if (it->is_character)
				{
				}
				else if (it->is_prop)
				{
					DrawParams d = { 0 };
					if (it->prop_kind == TREE0)
					{
						Vec2 prop_size = V2(74.0f, 122.0f);
						d = (DrawParams) { true, quad_centered(AddV2(it->pos, V2(-5.0f, 45.0)), prop_size), image_props_atlas, aabb_at_yplusdown(V2(2.0f, 4.0f), prop_size), WHITE, .sorting_key = sorting_key_at(AddV2(it->pos, V2(0.0f, 20.0f))), .alpha_clip_threshold = 0.7f };
					}
					else if (it->prop_kind == TREE1)
					{
						Vec2 prop_size = V2(94.0f, 120.0f);
						d = ((DrawParams) { true, quad_centered(AddV2(it->pos, V2(-4.0f, 55.0)), prop_size), image_props_atlas, aabb_at_yplusdown(V2(105.0f, 4.0f), prop_size), WHITE, .sorting_key = sorting_key_at(AddV2(it->pos, V2(0.0f, 20.0f))), .alpha_clip_threshold = 0.4f });
					}
					else if (it->prop_kind == TREE2)
					{
						Vec2 prop_size = V2(128.0f, 192.0f);
						d = ((DrawParams) { true, quad_centered(AddV2(it->pos, V2(-2.5f, 70.0)), prop_size), image_props_atlas, aabb_at_yplusdown(V2(385.0f, 479.0f), prop_size), WHITE, .sorting_key = sorting_key_at(AddV2(it->pos, V2(0.0f, 20.0f))), .alpha_clip_threshold = 0.4f });
					}
					else if (it->prop_kind == ROCK0)
					{
						Vec2 prop_size = V2(30.0f, 22.0f);
						d = (DrawParams) { true, quad_centered(AddV2(it->pos, V2(0.0f, 25.0)), prop_size), image_props_atlas, aabb_at_yplusdown(V2(66.0f, 235.0f), prop_size), WHITE, .sorting_key = sorting_key_at(AddV2(it->pos, V2(0.0f, 0.0f))), .alpha_clip_threshold = 0.7f };
					}
					else
					{
						assert(false);
					}
					draw_shadow_for(d);
					draw_quad(d);
				}
				else
				{
					assert(false);
				}
			}

		PROFILE_SCOPE("dialog menu") // big dialog panel draw big dialog panel
		{
			static float on_screen = 0.0f;
			Entity *talking_to = gete(player->talking_to);
			on_screen = Lerp(on_screen, unwarped_dt*9.0f, talking_to ? 1.0f : 0.0f);
			{
				float panel_width = screen_size().x * 0.4f * on_screen;
				AABB panel_aabb = (AABB) { .upper_left = V2(0.0f, screen_size().y), .lower_right = V2(panel_width, 0.0f) };
				float alpha = 1.0f;

				if (aabb_is_valid(panel_aabb))
				{
					if (!choosing_item_grid && pressed.mouse_down && !has_point(panel_aabb, mouse_pos))
					{
						player->state = CHARACTER_IDLE;
					}
					draw_quad((DrawParams) { false, quad_aabb(panel_aabb), IMG(image_white_square), blendalpha(BLACK, 0.7f) });

					// apply padding
					float padding = 0.1f * screen_size().y;
					panel_width -= padding * 2.0f;
					panel_aabb.upper_left = AddV2(panel_aabb.upper_left, V2(padding, -padding));
					panel_aabb.lower_right = AddV2(panel_aabb.lower_right, V2(-padding, padding));

					// draw button
					float space_btwn_buttons = 20.0f;
					float text_scale = 1.0f;
					const float num_buttons = 2.0f;
					Vec2 button_size = V2(
							(panel_width - (num_buttons - 1.0f)*space_btwn_buttons) / num_buttons,
							(panel_aabb.upper_left.y - panel_aabb.lower_right.y)*0.2f
							);
					float button_grid_width = button_size.x*num_buttons + space_btwn_buttons * (num_buttons - 1.0f);
					Vec2 cur_upper_left = V2((panel_aabb.upper_left.x + panel_aabb.lower_right.x) / 2.0f - button_grid_width / 2.0f, panel_aabb.lower_right.y + button_size.y);
					if(receiving_text_input && pressed.speak_shortcut)
					{
						end_text_input("");
						pressed.speak_shortcut = false;
					}
					if (imbutton_key(aabb_at(cur_upper_left, button_size), text_scale, MD_S8Lit("Speak"), __LINE__, unwarped_dt, receiving_text_input) || (talking_to && pressed.speak_shortcut))
					{
						begin_text_input();
					}


					// draw keyboard hint
					{
						Vec2 keyboard_helper_at = V2(cur_upper_left.x + button_size.x*0.5f, cur_upper_left.y - button_size.y*0.75f);
						draw_quad((DrawParams){false, centered_quad(keyboard_helper_at, V2(40.0f, 40.0f)), IMG(image_white_square), blendalpha(GREY, 0.4f)});
						draw_centered_text((TextParams){false, false, MD_S8Lit("S"), keyboard_helper_at, BLACK, 1.5f});
					}

					cur_upper_left.x += button_size.x + space_btwn_buttons;

					if(choosing_item_grid && pressed.give_shortcut)
					{
						pressed.give_shortcut = false;
						choosing_item_grid = false;
					}

					if (imbutton_key(aabb_at(cur_upper_left, button_size), text_scale, MD_S8Lit("Give Item"), __LINE__, unwarped_dt, choosing_item_grid) || (talking_to && pressed.give_shortcut))
					{
						choosing_item_grid = true;
					}


					// draw keyboard hint
					{
						Vec2 keyboard_helper_at = V2(cur_upper_left.x + button_size.x*0.5f, cur_upper_left.y - button_size.y*0.75f);
						draw_quad((DrawParams){false, centered_quad(keyboard_helper_at, V2(40.0f, 40.0f)), IMG(image_white_square), blendalpha(GREY, 0.4f)});
						draw_centered_text((TextParams){false, false, MD_S8Lit("G"), keyboard_helper_at, BLACK, 1.5f});
					}

					const float dialog_text_scale = 1.0f;
					float button_grid_height = button_size.y;
					AABB dialog_panel = panel_aabb;
					dialog_panel.lower_right.y += button_grid_height + 20.0f; // a little bit of padding because the buttons go up
					float new_line_height = dialog_panel.lower_right.y;

					// talking to dialog text
					if (talking_to && aabb_is_valid(dialog_panel))
					{
						MD_ArenaTemp scratch = MD_GetScratch(0, 0);
						Dialog dialog = produce_dialog(talking_to, true);
						{
							for (int i = dialog.cur_index - 1; i >= 0; i--)
							{
								DialogElement *it = &dialog.data[i];
								{
									Color color;
									if(it->was_eavesdropped)
									{
										color = colhex(0xcb40e6);
									}
									else
									{
										if (it->kind == DELEM_PLAYER)
										{
											color = WHITE;
										}
										else if (it->kind == DELEM_NPC)
										{
											color = colhex(0x34e05c);
										}
										else if (it->kind == DELEM_ACTION_DESCRIPTION)
										{
											color = colhex(0xebc334);
										}
										else
										{
											assert(false);
										}
									}
									color = blendalpha(color, alpha);

									const float text_scale = 1.0f;
									PlacedWordList wrapped = place_wrapped_words(scratch.arena, MD_S8(it->speech, it->speech_length), text_scale, dialog_panel.lower_right.x - dialog_panel.upper_left.x);
									float line_vertical_offset = -wrapped.last->lower_left_corner.y;
									translate_words_by(wrapped, V2(0.0, line_vertical_offset));
									translate_words_by(wrapped, V2(dialog_panel.upper_left.x, new_line_height));
									new_line_height += line_vertical_offset + font_line_advance * text_scale;


									for(PlacedWord *cur = wrapped.first; cur; cur = cur->next)
									{
										float this_text_scale = text_scale;
										if(it->was_last_said && cur->next == 0)
										{
											this_text_scale *= clamp01(talking_to->word_anim_in / (float)cur->text.size);
										}
										AABB clipping_aabb = dialog_panel;
										clipping_aabb.lower_right.y -= 50.0f;
										draw_text((TextParams){false, false, cur->text, cur->lower_left_corner, color, this_text_scale, .clip_to = clipping_aabb, .do_clipping = true,});
									}

									if(i != 0)
									{
										float separator_height = 40.0f; // how much vertical space the whole separation, including padding, takes
										float line_height = 1.0f;
										Vec2 line_from = AddV2(wrapped.first->lower_left_corner, V2(0, font_line_advance*text_scale + separator_height/2.0f));
										Vec2 line_to = AddV2(line_from, V2(aabb_size(dialog_panel).x, 0));
										draw_quad((DrawParams){false, line_quad(line_from, line_to, line_height), IMG(image_white_square), blendalpha(WHITE, 0.6f), .clip_to = dialog_panel, .do_clipping = true});

										new_line_height += separator_height;
									}

								}
							}
						}
						MD_ReleaseScratch(scratch);
					}
				}
			}
		}

		// item grid modal draw item grid choose item pick item give item
		{
			static float visible = 0.0f;
			static float hovered_state[ARRLEN(player->held_items.data)] = { 0 };
			float target = 0.0f;
			if (choosing_item_grid) target = 1.0f;
			visible = Lerp(visible, unwarped_dt*9.0f, target);

			if (player->state != CHARACTER_TALKING)
			{
				choosing_item_grid = false;
			}

			draw_quad((DrawParams) { false, quad_at(V2(0.0, screen_size().y), screen_size()), IMG(image_white_square), blendalpha(oflightness(0.2f), visible*0.4f), .layer = LAYER_UI });

			Vec2 grid_panel_size = LerpV2(V2(0.0f, 0.0f), visible, V2(screen_size().x*0.75f, screen_size().y * 0.75f));
			AABB grid_aabb = centered_aabb(MulV2F(screen_size(), 0.5f), grid_panel_size);
			if (choosing_item_grid && pressed.mouse_down && !has_point(grid_aabb, mouse_pos))
			{
				choosing_item_grid = false;
			}
			if (aabb_is_valid(grid_aabb))
			{
				draw_quad((DrawParams) { false, quad_aabb(grid_aabb), IMG(image_white_square), blendalpha(BLACK, visible * 0.7f), .layer = LAYER_UI });

				if (imbutton(centered_aabb(AddV2(grid_aabb.upper_left, V2(aabb_size(grid_aabb).x / 2.0f, -aabb_size(grid_aabb).y)), V2(100.f*visible, 50.0f*visible)), 1.0f, MD_S8Lit("Cancel")))
				{
					choosing_item_grid = false;
				}

				const float padding = 30.0f; // between border of panel and the items
				const float padding_btwn_items = 10.0f;

				const int horizontal_item_count = 10;
				const int vertical_item_count = 6;
				assert(ARRLEN(player->held_items.data) < horizontal_item_count * vertical_item_count);

				Vec2 space_for_items = SubV2(aabb_size(grid_aabb), V2(padding*2.0f, padding*2.0f));
				float item_icon_width = (space_for_items.x - (horizontal_item_count - 1)*padding_btwn_items) / horizontal_item_count;
				Vec2 item_icon_size = V2(item_icon_width, item_icon_width);

				Vec2 cursor = AddV2(grid_aabb.upper_left, V2(padding, -padding));
				int to_give = -1; // don't modify the item array while iterating
				BUFF_ITER_I(ItemKind, &player->held_items, i)
				{
					Vec2 real_size = LerpV2(item_icon_size, hovered_state[i], MulV2F(item_icon_size, 1.25f));
					Vec2 item_center = AddV2(cursor, MulV2F(V2(item_icon_size.x, -item_icon_size.y), 0.5f));
					AABB item_icon = centered_aabb(item_center, real_size);


					float target = 0.0f;
					if (aabb_is_valid(item_icon))
					{
						draw_quad((DrawParams) { false, quad_aabb(item_icon), IMG(image_white_square), blendalpha(WHITE, Lerp(0.0f, hovered_state[i], 0.4f)), .layer = LAYER_UI_FG });
						bool hovered = has_point(item_icon, mouse_pos);
						if (hovered)
						{
							target = 1.0f;
							if (pressed.mouse_down)
							{
								if (gete(player->talking_to))
								{
									to_give = i;
								}
							}
						}

						in_screen_space = true;
						dbgrect(item_icon);
						in_screen_space = false;
						draw_item(false, *it, item_icon, clamp01(visible*visible));
					}

					hovered_state[i] = Lerp(hovered_state[i], dt*12.0f, target);

					cursor.x += item_icon_size.x + padding_btwn_items;
					if ((i + 1) % horizontal_item_count == 0 && i != 0)
					{
						cursor.y -= item_icon_size.y + padding_btwn_items;
						cursor.x = grid_aabb.upper_left.x + padding;
					}
				}
				if (to_give > -1)
				{
					choosing_item_grid = false;

					Entity *to = gete(player->talking_to);
					assert(to);

					ItemKind given_item_kind = player->held_items.data[to_give];
					BUFF_REMOVE_AT_INDEX(&player->held_items, to_give);

					Action give_action = {.kind = ACT_give_item, .argument = { .item_to_give = given_item_kind }};
					perform_action(player, give_action);
				}

			}
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

			draw_quad((DrawParams) {false, quad_at(V2(0,screen_size().y), screen_size()), IMG(image_white_square), blendalpha(BLACK, visible*0.7f), .layer = LAYER_UI});
			float shake_speed = 9.0f;
			Vec2 win_offset = V2(sinf((float)unwarped_elapsed_time * shake_speed * 1.5f + 0.1f), sinf((float)unwarped_elapsed_time * shake_speed + 0.3f));
			win_offset = MulV2F(win_offset, 10.0f);
			draw_centered_text((TextParams){false, false, MD_S8Lit("YOU WON"), AddV2(MulV2F(screen_size(), 0.5f), win_offset), WHITE, 9.0f*visible});

			if(imbutton(centered_aabb(V2(screen_size().x/2.0f, screen_size().y*0.25f), MulV2F(V2(170.0f, 60.0f), visible)), 1.5f*visible, MD_S8Lit("Restart")))
			{
				reset_level();
			}
		}


		// ui
#define HELPER_SIZE 250.0f

		// how many peace tokens
		{
			MD_ArenaTemp scratch = MD_GetScratch(0, 0);
			const float to_screen_padding = 50.0f;
			const float btwn_elems = 10.0f;
			const float text_scale = 1.0f;
			const float peace_token_icon_size = 50.0f;

			TextParams t = {false, true, FmtWithLint(scratch.arena, "%d", player->peace_tokens), V2(0, 0), WHITE, text_scale};
			AABB text_bounds = draw_text(t);
			float total_elem_width = btwn_elems + peace_token_icon_size + aabb_size(text_bounds).x;
			float elem_height = peace_token_icon_size;
			AABB total_elem_box = aabb_at(V2(screen_size().x - to_screen_padding - total_elem_width, screen_size().y - to_screen_padding), V2(total_elem_width, elem_height));
			draw_quad((DrawParams){false, quad_at(total_elem_box.upper_left, V2(peace_token_icon_size, peace_token_icon_size)), IMG(image_peace_orb), WHITE, .layer = LAYER_UI});
			t.dry_run = false;
			t.pos = AddV2(total_elem_box.upper_left, V2(peace_token_icon_size + btwn_elems + aabb_size(text_bounds).x/2.0f, -peace_token_icon_size/2.0f));
			draw_text(t);
			MD_ReleaseScratch(scratch);
		}

		// keyboard tutorial icons
		if (!mobile_controls)
		{
			float total_height = HELPER_SIZE * 2.0f;
			float vertical_spacing = HELPER_SIZE / 2.0f;
			total_height -= (total_height - (vertical_spacing + HELPER_SIZE));
			const float padding = 50.0f;
			float y = screen_size().y / 2.0f + total_height / 2.0f;
			float x = screen_size().x - padding - HELPER_SIZE;
			draw_quad((DrawParams) { false, quad_at(V2(x, y), V2(HELPER_SIZE, HELPER_SIZE)), IMG(image_shift_icon), (Color) { 1.0f, 1.0f, 1.0f, fmaxf(0.0f, 1.0f-learned_shift) }, .layer = LAYER_UI_FG });
			y -= vertical_spacing;
			draw_quad((DrawParams) { false, quad_at(V2(x, y), V2(HELPER_SIZE, HELPER_SIZE)), IMG(image_space_icon), (Color) { 1.0f, 1.0f, 1.0f, fmaxf(0.0f, 1.0f-learned_space) }, .layer = LAYER_UI_FG });
		}


		if (mobile_controls)
		{
			float thumbstick_nub_size = (img_size(image_mobile_thumbstick_nub).x / img_size(image_mobile_thumbstick_base).x) * thumbstick_base_size();
			draw_quad((DrawParams) { false, quad_centered(thumbstick_base_pos, V2(thumbstick_base_size(), thumbstick_base_size())), IMG(image_mobile_thumbstick_base), WHITE, .layer = LAYER_UI_FG });
			draw_quad((DrawParams) { false, quad_centered(thumbstick_nub_pos, V2(thumbstick_nub_size, thumbstick_nub_size)), IMG(image_mobile_thumbstick_nub), WHITE, .layer = LAYER_UI_FG });

			if (interacting_with)
			{
				draw_quad((DrawParams) { false, quad_centered(interact_button_pos(), V2(mobile_button_size(), mobile_button_size())), IMG(image_mobile_button), WHITE, .layer = LAYER_UI_FG });
			}
			draw_quad((DrawParams) { false, quad_centered(roll_button_pos(), V2(mobile_button_size(), mobile_button_size())), IMG(image_mobile_button), WHITE, .layer = LAYER_UI_FG });
			draw_quad((DrawParams) { false, quad_centered(attack_button_pos(), V2(mobile_button_size(), mobile_button_size())), IMG(image_mobile_button), WHITE, .layer = LAYER_UI_FG });
		}

#ifdef DEVTOOLS

		dbgsquare(screen_to_world(mouse_pos));

		// tile coord
		if (show_devtools)
		{
			TileCoord hovering = world_to_tilecoord(screen_to_world(mouse_pos));
			Vec2 points[4] = { 0 };
			AABB q = tile_aabb(hovering);
			dbgrect(q);
			draw_text((TextParams) { false, false, tprint("%d", get_tile(&level_level0, hovering).kind), world_to_screen(tilecoord_to_world(hovering)), BLACK, 1.0f });
		}

		// debug draw font image
		{
			draw_quad((DrawParams) { true, quad_centered(V2(0.0, 0.0), V2(250.0, 250.0)), image_font, full_region(image_font), WHITE });
		}

		// statistics
		if (show_devtools)
			PROFILE_SCOPE("statistics")
			{
				Vec2 pos = V2(0.0, screen_size().Y);
				int num_entities = 0;
				ENTITIES_ITER(gs.entities) num_entities++;
				MD_String8 stats = tprint("Frametime: %.1f ms\nProcessing: %.1f ms\nGameplay processing: %.1f ms\nEntities: %d\nDraw calls: %d\nProfiling: %s\nNumber gameplay processing loops: %d\n", dt*1000.0, last_frame_processing_time*1000.0, last_frame_gameplay_processing_time*1000.0, num_entities, num_draw_calls, profiling ? "yes" : "no", num_timestep_loops);
				AABB bounds = draw_text((TextParams) { false, true, stats, pos, BLACK, 1.0f });
				pos.Y -= bounds.upper_left.Y - screen_size().Y;
				bounds = draw_text((TextParams) { false, true, stats, pos, BLACK, 1.0f });
				// background panel
				colorquad(false, quad_aabb(bounds), (Color) { 1.0, 1.0, 1.0, 0.3f });
				draw_text((TextParams) { false, false, stats, pos, BLACK, 1.0f });
				num_draw_calls = 0;
			}
#endif // devtools


		// update camera position
		{
			Vec2 target = MulV2F(player->pos, -1.0f * cam.scale);
			if (LenV2(SubV2(target, cam.pos)) <= 0.2)
			{
				cam.pos = target;
			}
			else
			{
				cam.pos = LerpV2(cam.pos, unwarped_dt*8.0f, target);
			}
		}

		PROFILE_SCOPE("flush rendering")
		{
			ARR_ITER_I(RenderingQueue, rendering_queues, i)
			{
				Layer layer = (Layer)i;
				RenderingQueue *rendering_queue = it;
				qsort(&rendering_queue->data[0], rendering_queue->cur_index, sizeof(rendering_queue->data[0]), rendering_compare);

				BUFF_ITER(DrawParams, rendering_queue)
				{
					DrawParams d = *it;
					PROFILE_SCOPE("Draw quad")
					{
						assert(!d.world_space); // world space already applied when queued for drawing
						Vec2 *points = d.quad.points;
						quad_fs_params_t params = { 0 };
						params.tint[0] = d.tint.R;
						params.tint[1] = d.tint.G;
						params.tint[2] = d.tint.B;
						params.tint[3] = d.tint.A;
						params.alpha_clip_threshold = d.alpha_clip_threshold;
						if (d.do_clipping)
						{
							Vec2 aabb_clip_ul = into_clip_space(d.clip_to.upper_left);
							Vec2 aabb_clip_lr = into_clip_space(d.clip_to.lower_right);
							params.clip_ul[0] = aabb_clip_ul.x;
							params.clip_ul[1] = aabb_clip_ul.y;
							params.clip_lr[0] = aabb_clip_lr.x;
							params.clip_lr[1] = aabb_clip_lr.y;
						}
						else
						{
							params.clip_ul[0] = -1.0;
							params.clip_ul[1] = 1.0;
							params.clip_lr[0] = 1.0;
							params.clip_lr[1] = -1.0;
						}
						// if the rendering call is different, and the batch must be flushed
						if (d.image.id != cur_batch_image.id || memcmp(&params, &cur_batch_params, sizeof(params)) != 0)
						{
							flush_quad_batch();
							cur_batch_image = d.image;
							cur_batch_params = params;
						}



						float new_vertices[ FLOATS_PER_VERTEX*4 ] = { 0 };
						Vec2 region_size = SubV2(d.image_region.lower_right, d.image_region.upper_left);
						assert(region_size.X > 0.0);
						assert(region_size.Y > 0.0);
						Vec2 tex_coords[4] =
						{
							AddV2(d.image_region.upper_left, V2(0.0,                     0.0)),
							AddV2(d.image_region.upper_left, V2(region_size.X,           0.0)),
							AddV2(d.image_region.upper_left, V2(region_size.X, region_size.Y)),
							AddV2(d.image_region.upper_left, V2(0.0,           region_size.Y)),
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

		pressed = (PressedState) { 0 };
	}
}

void cleanup(void)
{
	free(fontBuffer);
	MD_ArenaRelease(frame_arena);
	MD_ArenaRelease(persistent_arena);
	sg_shutdown();
	hmfree(imui_state);
	Log("Cleaning up\n");
}

void event(const sapp_event *e)
{
	if (e->key_repeat) return;
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
			end_text_input(text_input_buffer);
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
			if (e->key_code == SAPP_KEYCODE_M)
			{
				mobile_controls = true;
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
			.width = 800,
			.height = 600,
			//.gl_force_gles2 = true, not sure why this was here in example, look into
			.window_title = "RPGPT",
			.win32_console_attach = true,
			.win32_console_create = true,
			.icon.sokol_default = true,
	};
}
