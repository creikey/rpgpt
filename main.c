#define SOKOL_IMPL
#if defined(WIN32) || defined(_WIN32)
#define SOKOL_D3D11
#endif
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "quad-sapp.glsl.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
} state;



void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });

    // load the image
    state.bind.fs_images[SLOT_tex] = sg_alloc_image();
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
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .data.subimage[0][0] = {
                .ptr = pixels,
                .size = (size_t)(png_width * png_height * 4),
            }
        });
        stbi_image_free(pixels);
    }

    /* a vertex buffer */
    float vertices[] = {
        // positions    texcoords
        -0.5f,  0.5f,   0.0f, 0.0f,
         0.5f,  0.5f,   1.0f, 0.0f,
         0.5f, -0.5f,   1.0f, 1.0f,
        -0.5f, -0.5f,   0.0f, 1.0f,
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
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
    sg_shader shd = sg_make_shader(quad_shader_desc(sg_query_backend()));

    /* a pipeline state object */
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .attrs = {
                [ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT2,
                [ATTR_vs_texcoord0].format   = SG_VERTEXFORMAT_FLOAT2,
            }
        },
        .label = "quad-pipeline"
    });

    /* default pass action */
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .action=SG_ACTION_CLEAR, .value={0.0f, 0.0f, 0.0f, 1.0f } }
    };
}

void frame(void) {
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
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
