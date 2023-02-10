#define SOKOL_IMPL
#if defined(WIN32) || defined(_WIN32)
#define SOKOL_D3D11
#endif
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_glue.h"
#include "quad-sapp.glsl.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "HandMadeMath.h"

#include <math.h>

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

    // load the image
    state.bind.fs_images[SLOT_quad_tex] = sg_alloc_image();
    int png_width, png_height, num_channels;
    const int desired_channels = 4;
    stbi_uc* pixels = stbi_load(
        "assets/merchant.png",
        &png_width, &png_height,
        &num_channels, 0);
    if (pixels) {
        sg_init_image(state.bind.fs_images[0], &(sg_image_desc){
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
    }

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

double time = 0.0;
uint64_t last_frame_time;
void frame(void) {
    // time
    {
        double dt = stm_sec(stm_diff(stm_now(), last_frame_time));
        time += dt;
        last_frame_time = stm_now();
    }

    float new_vertices[] = {
        // positions    texcoords
        -0.5f,  0.5f,   0.0f, 0.0f,
         0.5f,  0.5f,   1.0f, 0.0f,
         0.5f, -0.5f,   1.0f, 1.0f,
        -0.5f, -0.5f,   0.0f, 1.0f,
    };
    int offset = sg_append_buffer(state.bind.vertex_buffers[0], &SG_RANGE(new_vertices));
    state.bind.vertex_buffer_offsets[0] = offset;
    

    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    int index = (int)floor(time/0.3);

    sg_image img = state.bind.fs_images[0];
    sg_image_info info = sg_query_image_info(img);

    quad_fs_params_t params = {
        .tint = {1.0, 1.0, 1.0, 1.0},
        .upper_left = {0.0, 0.0},
        .lower_right = {1.0, 1.0},
    };
    int cell_size = 110;
    assert(info.width % cell_size == 0);
    int upper_left = (index % (info.width/cell_size)) * cell_size;
    params.upper_left[0] = (float)upper_left;
    params.lower_right[0] = params.upper_left[0] + (float)cell_size;
    params.lower_right[1] = (float)cell_size;

    // transform from pixels to uv space
    params.upper_left[0] /= (float)info.width;
    params.lower_right[0] /= (float)info.width;
    params.upper_left[1] /= (float)info.height;
    params.lower_right[1] /= (float)info.height;

    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_quad_fs_params, &SG_RANGE(params));
    sg_draw(0, 6, 1);
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
