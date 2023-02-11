#define SOKOL_IMPL
#if defined(WIN32) || defined(_WIN32)
#define SOKOL_D3D11
#endif
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_glue.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "HandMadeMath.h"

#pragma warning(disable : 4996) // fopen is safe. I don't care about fopen_s

#include <math.h>

typedef struct AABB {
    HMM_Vec2 upper_left;
    HMM_Vec2 lower_right;
} AABB;

typedef struct TileInstance {
    uint16_t kind;
} TileInstance;

typedef struct TileInfo {
    uint16_t kind;
    int num_frames;
    AABB regions[32];
    sg_image *img;
} TileInfo;

#define LEVEL_TILES 60
#define TILE_SIZE 32 // in pixels
typedef struct Level {
    TileInstance tiles[LEVEL_TILES][LEVEL_TILES];
} Level;

HMM_Vec2 tilecoord_to_world(int x, int y) {
    return HMM_V2( (float)x * (float)TILE_SIZE * 1.0f, -(float)y * (float)TILE_SIZE * 1.0f );
}

sg_image load_image(const char *path) {
    sg_image to_return = {0};

    int png_width, png_height, num_channels;
    const int desired_channels = 4;
    stbi_uc* pixels = stbi_load(
        path,
        &png_width, &png_height,
        &num_channels, 0);
    assert(pixels);
    to_return = sg_make_image(&(sg_image_desc) {
        .width = png_width,
        .height = png_height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .data.subimage[0][0] = {
            .ptr = pixels,
            .size = (size_t)(png_width * png_height * 4),
        }
    });
    stbi_image_free(pixels);
    return to_return;
}

#include "quad-sapp.glsl.h"
#include "assets.gen.c"

TileInfo tiles[] = {
    {
        .kind = 53,
        .num_frames = 1,
        .regions[0] = { 0, 32, 32, 64 },
        .img = &image_animated_terrain,
    },
};
TileInfo mystery_tile = {
    .img = &image_mystery_tile,
    .num_frames = 1,
    .regions[0] = { 0, 0, 32, 32 },
};

sg_image image_font;
stbtt_bakedchar cdata[96]; // ASCII 32..126 is 95 glyphs

// so can be grep'd and removed
#define dbgprint(...) { printf("Debug | %s:%d | ", __FILE__, __LINE__); printf(__VA_ARGS__); }

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
} state;

void init(void) {
    stm_setup();
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });

    load_assets();

    // load font
    {
        FILE* fontFile = fopen("assets/orange kid.ttf", "rb");
        fseek(fontFile, 0, SEEK_END);
        size_t size = ftell(fontFile); /* how long is the file ? */
        fseek(fontFile, 0, SEEK_SET); /* reset */

        char *fontBuffer = malloc(size);

        fread(fontBuffer, size, 1, fontFile);
        fclose(fontFile);

        unsigned char font_bitmap[512*512] = {0};
        stbtt_BakeFontBitmap(fontBuffer, 0, 64.0, font_bitmap, 512, 512, 32, 96, cdata);

        unsigned char *font_bitmap_rgba = malloc(4 * 512 * 512); // stack would be too big if allocated on stack (stack overflow)
        for(int i = 0; i < 512 * 512; i++) {
            font_bitmap_rgba[i*4 + 0] = 255;
            font_bitmap_rgba[i*4 + 1] = 255;
            font_bitmap_rgba[i*4 + 2] = 255;
            font_bitmap_rgba[i*4 + 3] = font_bitmap[i];
        }

        image_font = sg_make_image( &(sg_image_desc){
            .width = 512,
            .height = 512,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .min_filter = SG_FILTER_NEAREST,
            .mag_filter = SG_FILTER_NEAREST,
            .data.subimage[0][0] = {
                .ptr = font_bitmap_rgba,
                .size = (size_t)(512 * 512 * 4),
            }
        });
        free(font_bitmap_rgba);
    }

    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .usage = SG_USAGE_STREAM,
        //.data = SG_RANGE(vertices),
        .size = 1024*500,
        .label = "quad-vertices"
    });

    /* an index buffer with 2 triangles */
    uint16_t indices[] = { 0, 1, 2,  0, 2, 3 };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices),
        .label = "quad-indices"
    });


    sg_shader shd = sg_make_shader(quad_program_shader_desc(sg_query_backend()));

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .attrs = {
                [ATTR_quad_vs_position].format = SG_VERTEXFORMAT_FLOAT2,
                [ATTR_quad_vs_texcoord0].format   = SG_VERTEXFORMAT_FLOAT2,
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

    state.pass_action = (sg_pass_action) {
        .colors[0] = { .action=SG_ACTION_CLEAR, .value={1.0f, 1.0f, 1.0f, 1.0f } }
    };
}

typedef HMM_Vec4 Color;


#define WHITE (Color){1.0f, 1.0f, 1.0f, 1.0f}
#define BLACK (Color){0.0f, 0.0f, 0.0f, 1.0f}

HMM_Vec2 screen_size() {
    return HMM_V2((float)sapp_width(), (float)sapp_height());
}

typedef struct Camera {
    HMM_Vec2 pos;
    float scale;
} Camera;


// everything is in pixels in world space, 43 pixels is approx 1 meter measured from 
// merchant sprite being 5'6"
const float pixels_per_meter = 43.0f;
Camera cam = {.scale = 2.0f };

HMM_Vec2 cam_offset() {
    return HMM_AddV2(cam.pos, HMM_MulV2F(screen_size(), 0.5f));
}

// screen coords are in pixels counting from bottom left as (0,0), Y+ is up
HMM_Vec2 world_to_screen(HMM_Vec2 world) {
    HMM_Vec2 to_return = world;
    to_return = HMM_MulV2F(to_return, cam.scale);
    to_return = HMM_AddV2(to_return, cam_offset());
    return to_return;
}

HMM_Vec2 screen_to_world(HMM_Vec2 screen) {
    HMM_Vec2 to_return = screen;
    to_return = HMM_SubV2(to_return, cam_offset());
    to_return = HMM_MulV2F(to_return, 1.0f/cam.scale);
    return to_return;
}

// out must be of at least length 4
void quad_points_corner_size(HMM_Vec2 *out, HMM_Vec2 at, HMM_Vec2 size) {
    out[0] = HMM_V2(0.0, 0.0);
    out[1] = HMM_V2(size.X, 0.0);
    out[2] = HMM_V2(size.X, -size.Y);
    out[3] = HMM_V2(0.0, -size.Y);

    for(int i = 0; i < 4; i++) {
        out[i] = HMM_AddV2(out[i], at);
    }
}

// out must be of at least length 4
void quad_points_centered_size(HMM_Vec2 *out, HMM_Vec2 at, HMM_Vec2 size) {
    quad_points_corner_size(out, at, size);
    for(int i = 0; i < 4; i++) {
        out[i] = HMM_AddV2(out[i], HMM_V2(-size.X*0.5f, size.Y*0.5f));
    }
}

// both segment_a and segment_b must be arrays of length 2
bool segments_overlapping(float *a_segment, float *b_segment) {
    assert(a_segment[1] >= a_segment[0]);
    assert(b_segment[1] >= b_segment[0]);
    float total_length = (a_segment[1] - a_segment[0]) + (b_segment[1] - b_segment[0]);
    float farthest_to_left = min(a_segment[0], b_segment[0]);
    float farthest_to_right = max(a_segment[1], b_segment[1]);
    if (farthest_to_right - farthest_to_left < total_length) {
        return true;
    } else {
        return false;
    }
}

bool overlapping(AABB a, AABB b) {
    // x axis
    {
        float a_segment[2] = { a.upper_left.X, a.lower_right.X };
        float b_segment[2] = { b.upper_left.X, b.lower_right.X };
        if(segments_overlapping(a_segment, b_segment)) {
        } else {
            return false;
        }
    }

    // y axis
    {
        float a_segment[2] = { a.lower_right.Y, a.upper_left.Y };
        float b_segment[2] = { b.lower_right.Y, b.upper_left.Y };
        if(segments_overlapping(a_segment, b_segment)) {
        } else {
            return false;
        }
    }

    return true; // both segments overlapping
}

// points must be of length 4, and be in the order: upper left, upper right, lower right, lower left
// the points are in pixels in screen space. The image region is in pixel space of the image
void draw_quad_all_parameters(HMM_Vec2 *points, sg_image image, AABB image_region, Color tint) {
    AABB cam_aabb = { .upper_left = HMM_V2(0.0, screen_size().Y), .lower_right = HMM_V2(screen_size().X, 0.0) };
    AABB points_bounding_box = { .upper_left = HMM_V2(INFINITY, -INFINITY), .lower_right = HMM_V2(-INFINITY, INFINITY) };

    for(int i = 0; i < 4; i++) {
        points_bounding_box.upper_left.X = min(points_bounding_box.upper_left.X, points[i].X);
        points_bounding_box.upper_left.Y = max(points_bounding_box.upper_left.Y, points[i].Y);

        points_bounding_box.lower_right.X = max(points_bounding_box.lower_right.X, points[i].X);
        points_bounding_box.lower_right.Y = min(points_bounding_box.lower_right.Y, points[i].Y);
    }
    if(!overlapping(cam_aabb, points_bounding_box)) {
        return; // cull out of screen quads
    }

    float new_vertices[ (2 + 2)*4 ];
    HMM_Vec2 region_size = HMM_SubV2(image_region.lower_right, image_region.upper_left);
    assert(region_size.X > 0.0);
    assert(region_size.Y > 0.0);
    HMM_Vec2 tex_coords[4] = {
        HMM_AddV2(image_region.upper_left, HMM_V2(0.0,                    0.0)),
        HMM_AddV2(image_region.upper_left, HMM_V2(region_size.X,           0.0)),
        HMM_AddV2(image_region.upper_left, HMM_V2(region_size.X, region_size.Y)),
        HMM_AddV2(image_region.upper_left, HMM_V2(0.0,           region_size.Y)),
    };
    // convert to uv space
    sg_image_info info = sg_query_image_info(image);
    for(int i = 0; i < 4; i++) {
        tex_coords[i] = HMM_DivV2(tex_coords[i], HMM_V2((float)info.width, (float)info.height));
    }
    for(int i = 0; i < 4; i++) {
        HMM_Vec2 zero_to_one = HMM_DivV2(points[i], screen_size());
        HMM_Vec2 in_clip_space = HMM_SubV2(HMM_MulV2F(zero_to_one, 2.0), HMM_V2(1.0, 1.0));
        new_vertices[i*4] = in_clip_space.X;
        new_vertices[i*4 + 1] = in_clip_space.Y;
        new_vertices[i*4 + 2] = tex_coords[i].X;
        new_vertices[i*4 + 3] = tex_coords[i].Y;
    }
    state.bind.vertex_buffer_offsets[0] = sg_append_buffer(state.bind.vertex_buffers[0], &SG_RANGE(new_vertices));
    quad_fs_params_t params = {0};
    params.tint[0] = tint.R;
    params.tint[1] = tint.G;
    params.tint[2] = tint.B;
    params.tint[3] = tint.A;
    params.upper_left[0] = image_region.upper_left.X;
    params.upper_left[1] = image_region.upper_left.Y;
    params.lower_right[0] = image_region.lower_right.X;
    params.lower_right[1] = image_region.lower_right.Y;

    state.bind.fs_images[SLOT_quad_tex] = image;
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_quad_fs_params, &SG_RANGE(params));
    sg_draw(0, 6, 1);
}

void draw_quad_world_all(HMM_Vec2 *points, sg_image image, AABB image_region, Color tint) {
    HMM_Vec2 into_screen[4] = {0};
    memcpy(into_screen, points, sizeof(into_screen));
    for(int i = 0; i < 4; i++) {
        into_screen[i] = world_to_screen(into_screen[i]);
    }
    draw_quad_all_parameters(into_screen, image, image_region, tint);
}

// in pixels
HMM_Vec2 img_size(sg_image img) {
    sg_image_info info = sg_query_image_info(img);
    return HMM_V2((float)info.width, (float)info.height);
}

// full region in pixels
AABB full_region(sg_image img) {
    return (AABB) {
        .upper_left = HMM_V2(0.0f, 0.0f),
        .lower_right = img_size(img),
    };
}

double time = 0.0;
uint64_t last_frame_time;
HMM_Vec2 mouse_pos = {0}; // in screen space
HMM_Vec2 character_pos = {0}; // world space point
bool keydown[SAPP_KEYCODE_MENU] = {0};
#ifdef DEVTOOLS
bool mouse_frozen = false;
#endif
void frame(void) {
    // time
    double dt_double = 0.0;
    {
        dt_double = stm_sec(stm_diff(stm_now(), last_frame_time));
        time += dt_double;
        last_frame_time = stm_now();
    }
    float dt = (float)dt_double;

    HMM_Vec2 movement = HMM_V2(
        (float)keydown[SAPP_KEYCODE_D] - (float)keydown[SAPP_KEYCODE_A],
        (float)keydown[SAPP_KEYCODE_W] - (float)keydown[SAPP_KEYCODE_S]
    );
    if(HMM_LenV2(movement) > 1.0) {
        movement = HMM_NormV2(movement);
    }

    character_pos = HMM_AddV2(character_pos, HMM_MulV2F(movement, dt * pixels_per_meter * 4.0f));
    cam.pos = HMM_LerpV2(cam.pos, dt*8.0f, HMM_MulV2F(character_pos, -1.0f * cam.scale));

    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(state.pip);

    // tilemap
#if 1
    Level * cur_level = &level_level0;
    for(int row = 0; row < LEVEL_TILES; row++) {
        for(int col = 0; col < LEVEL_TILES; col++)
        {
            TileInstance cur = cur_level->tiles[row][col];
            if(cur.kind != 0){
                HMM_Vec2 points[4] = {0};
                quad_points_corner_size(points, tilecoord_to_world(col, row), HMM_V2(TILE_SIZE, TILE_SIZE));
                TileInfo *info = NULL;
                for(int i = 0; i < sizeof(tiles)/sizeof(*tiles); i++) {
                    if(tiles[i].kind == cur.kind) {
                        info = &tiles[i];
                        break;
                    }
                }
                if(info == NULL) info = &mystery_tile;
                draw_quad_world_all(points, *info->img, info->regions[0], WHITE);
            }
        }
    }
#endif

    // debug draw font image
    {
        HMM_Vec2 points[4] = {0};
        quad_points_centered_size(points, HMM_V2(0.0, 0.0), HMM_V2(250.0, 250.0));
        draw_quad_world_all(points, image_font,full_region(image_font), BLACK);
    }
    
    // draw text
    const char *text = "Hello, can anybody hear me?";
    {
        float x = 0.0;
        float y = 0.0;
        for(int i = 0; i < strlen(text); i++) {
            stbtt_aligned_quad q;
            float old_y = y;
            stbtt_GetBakedQuad(cdata, 512, 512, text[i]-32, &x, &y, &q, 1);
            float difference = y - old_y;
            y = old_y + difference;

            HMM_Vec2 size = HMM_V2(q.x1 - q.x0, q.y1 - q.y0);
            HMM_Vec2 points[4] = {
                HMM_AddV2(HMM_V2(q.x0, -q.y0), HMM_V2(0.0f, 0.0f)),
                HMM_AddV2(HMM_V2(q.x0, -q.y0), HMM_V2(size.X, 0.0f)),
                HMM_AddV2(HMM_V2(q.x0, -q.y0), HMM_V2(size.X, -size.Y)),
                HMM_AddV2(HMM_V2(q.x0, -q.y0), HMM_V2(0.0f, -size.Y)),
            };
            
            AABB region = (AABB){
                .upper_left  = HMM_V2(q.s0, q.t0),
                .lower_right = HMM_V2(q.s1, q.t1),
            };
            region.upper_left.X *= img_size(image_font).X;
            region.lower_right.X *= img_size(image_font).X;
            region.upper_left.Y *= img_size(image_font).Y;
            region.lower_right.Y *= img_size(image_font).Y;

            draw_quad_world_all(points, image_font, region, BLACK);
        }
    }

    // merchant
    int index = (int)floor(time/0.3);
    float size = img_size(image_merchant).Y;
    HMM_Vec2 points[4] = {0};
    quad_points_centered_size(points, character_pos, HMM_V2(size, size));

    int cell_size = 110;
    assert((int)img_size(image_merchant).X % cell_size == 0);
    AABB region;
    region.upper_left = HMM_V2( (float)((index % ((int)img_size(image_merchant).X/cell_size)) * cell_size), 0.0);
    region.lower_right = HMM_V2(region.upper_left.X + (float)cell_size, (float)cell_size);

    draw_quad_world_all(points, image_merchant, region, WHITE);

    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    sg_shutdown();
}

void event(const sapp_event *e) {
    if(e->type == SAPP_EVENTTYPE_KEY_DOWN) {
        assert(e->key_code < sizeof(keydown)/sizeof(*keydown));
        keydown[e->key_code] = true;
        if(e->key_code == SAPP_KEYCODE_ESCAPE) {
            sapp_quit();
        }
#ifdef DEVTOOLS
        if(e->key_code == SAPP_KEYCODE_T) {
            mouse_frozen = !mouse_frozen;
        }
#endif
    }
    if(e->type == SAPP_EVENTTYPE_KEY_UP) {
        keydown[e->key_code] = false;
    }
    if(e->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
        bool ignore_movement = false;
#ifdef DEVTOOLS
        if(mouse_frozen) ignore_movement = true;
#endif
        if(!ignore_movement) mouse_pos = HMM_V2(e->mouse_x, (float)sapp_height() - e->mouse_y);
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = 800,
        .height = 600,
        //.gl_force_gles2 = true, not sure why this was here in example, look into
        .window_title = "RPGPT",

        .win32_console_attach = true,

        .icon.sokol_default = true,
    };
}
