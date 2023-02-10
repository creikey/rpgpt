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
#include "HandMadeMath.h"

#include <math.h>

sg_image load_image(const char *path) {
    sg_image to_return = {0};

    int png_width, png_height, num_channels;
    const int desired_channels = 4;
    stbi_uc* pixels = stbi_load(
        path,
        &png_width, &png_height,
        &num_channels, 0);
    assert(pixels);
    to_return = sg_make_image(&(sg_image_desc){
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

    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .usage = SG_USAGE_STREAM,
        //.data = SG_RANGE(vertices),
        .size = 1024,
        .label = "quad-vertices"
    });

    /* an index buffer with 2 triangles */
    uint16_t indices[] = { 0, 1, 2,  0, 2, 3 };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices),
        .label = "quad-indices"
    });


    /* a shader (use separate shader sources here */
    sg_shader shd = sg_make_shader(quad_program_shader_desc(sg_query_backend()));

    /* a pipeline state object */
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .attrs = {
                [ATTR_quad_vs_position].format = SG_VERTEXFORMAT_FLOAT2,
                [ATTR_quad_vs_texcoord0].format   = SG_VERTEXFORMAT_FLOAT2,
            }
        },
        .label = "quad-pipeline"
    });

    /* default pass action */
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .action=SG_ACTION_CLEAR, .value={0.0f, 0.0f, 0.0f, 1.0f } }
    };
}
typedef struct AABB {
    HMM_Vec2 upper_left;
    HMM_Vec2 lower_right;
} AABB;

typedef HMM_Vec4 Color;

Color col(float r, float g, float b, float a) {
    return HMM_V4(r, g, b, a);
}

HMM_Vec2 screen_size() {
    return HMM_V2((float)sapp_width(), (float)sapp_height());
}


// points must be of length 4, and be in the order: upper left, upper right, lower right, lower left
// the points are in pixels in screen space
void draw_quad_all_parameters(HMM_Vec2 *points, sg_image image, AABB image_region, Color tint) {
    float new_vertices[ (2 + 2)*4 ];
    HMM_Vec2 region_size = HMM_SubV2(image_region.lower_right, image_region.upper_left);
    assert(region_size.X > 0.0);
    assert(region_size.Y > 0.0);
    HMM_Vec2 tex_coords[4] = {
        HMM_AddV2(image_region.upper_left, HMM_V2(0.0,                     0.0)),
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

double time = 0.0;
uint64_t last_frame_time;
HMM_Vec2 mouse_pos; // in screen space
#ifdef DEVTOOLS
bool mouse_frozen = false;
#endif
void frame(void) {
    // time
    {
        double dt = stm_sec(stm_diff(stm_now(), last_frame_time));
        time += dt;
        last_frame_time = stm_now();
    }

    /*HMM_Vec2 points[] = {
        HMM_V2(-0.5f, 0.5f),
        HMM_V2(0.5f, 0.5f),
        HMM_V2(0.5f, -0.5f),
        HMM_V2(-0.5f, -0.5f),
    };*/
    float size = 200.0;
    HMM_Vec2 points[] = {
        HMM_V2(0.0, 0.0),
        HMM_V2(size, 0.0),
        HMM_V2(size, -size),
        HMM_V2(0.0, -size),
    };
    for(int i = 0; i < 4; i ++) {
        points[i] = HMM_AddV2(points[i], mouse_pos);
    }


    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(state.pip);

    int index = (int)floor(time/0.3);

    sg_image_info info = sg_query_image_info(image_merchant);

    int cell_size = 110;
    assert(info.width % cell_size == 0);
    AABB region;
    region.upper_left = HMM_V2( (float)((index % (info.width/cell_size)) * cell_size), 0.0);
    region.lower_right = HMM_V2(region.upper_left.X + (float)cell_size, (float)cell_size);

    draw_quad_all_parameters(points, image_merchant, region, col(1.0, 1.0, 1.0, 1.0));

    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    sg_shutdown();
}

void event(const sapp_event *e) {
    if(e->type == SAPP_EVENTTYPE_KEY_DOWN) {
        if(e->key_code == SAPP_KEYCODE_ESCAPE) {
            sapp_quit();
        }
#ifdef DEVTOOLS
        if(e->key_code == SAPP_KEYCODE_T) {
            mouse_frozen = !mouse_frozen;
        }
#endif
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
        .gl_force_gles2 = true,
        .window_title = "RPGPT",

        .win32_console_attach = true,

        .icon.sokol_default = true,
    };
}
