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
 int num_frames;
 uint16_t frames[32];
} AnimatedTile;

typedef struct TileSet
{
 sg_image *img;
 AnimatedTile animated[128];
} TileSet;

typedef struct AnimatedSprite
{
 sg_image *img;
 double time_per_frame;
 int num_frames;
 Vec2 start;
 float horizontal_diff_btwn_frames;
 Vec2 region_size;
} AnimatedSprite;


#define LEVEL_TILES 60
#define TILE_SIZE 32 // in pixels
typedef struct Level
{
 TileInstance tiles[LEVEL_TILES][LEVEL_TILES];
 Vec2 spawnpoint;
} Level;

typedef struct TileCoord
{
 int x;
 int y;
} TileCoord;

// no alignment etc because lazy
typedef struct Arena
{
 char *data;
 size_t data_size;
 size_t cur;
} Arena;

Arena make(size_t max_size)
{
 return (Arena)
 {
  .data = calloc(1, max_size),
  .data_size = max_size,
  .cur = 0,
 };
}

void reset(Arena *a)
{
 memset(a->data, 0, a->data_size);
 a->cur = 0;
}

char *get(Arena *a, size_t of_size)
{
 assert(a->data != NULL);
 char *to_return = a->data + a->cur;
 a->cur += of_size;
 assert(a->cur < a->data_size);
 return to_return;
}

Arena scratch = {0};

char *tprint(const char *format, ...)
{
 va_list argptr;
 va_start(argptr, format);

 int size = vsnprintf(NULL, 0, format, argptr) + 1; // for null terminator

 char *to_return = get(&scratch, size);

 vsnprintf(to_return, size, format, argptr);

  va_end(argptr);

  return to_return;
}

Vec2 tilecoord_to_world(TileCoord t)
{
 return V2( (float)t.x * (float)TILE_SIZE * 1.0f, -(float)t.y * (float)TILE_SIZE * 1.0f );
}

TileCoord world_to_tilecoord(Vec2 w)
{
 // world = V2(tilecoord.x * tile_size, -tilecoord.y * tile_size)
 // world.x = tilecoord.x * tile_size
 // world.x / tile_size = tilecoord.x
 // world.y = -tilecoord.y * tile_size
 // - world.y / tile_size = tilecoord.y
 return (TileCoord){ (int)floorf(w.X / TILE_SIZE), (int)floorf(-w.Y / TILE_SIZE) };
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

Vec2 aabb_center(AABB aabb)
{
 return MulV2F(AddV2(aabb.upper_left, aabb.lower_right), 0.5f);
}

AABB centered_aabb(Vec2 at, Vec2 size)
{
 return (AABB){
  .upper_left  = AddV2(at, V2(-size.X/2.0f, size.Y/2.0f)),
   .lower_right = AddV2(at, V2( size.X/2.0f, -size.Y/2.0f)),
 };
}

TileInstance get_tile(Level *l, TileCoord t)
{
 bool out_of_bounds = false;
 out_of_bounds |= t.x < 0;
 out_of_bounds |= t.x >= LEVEL_TILES;
 out_of_bounds |= t.y < 0;
 out_of_bounds |= t.y >= LEVEL_TILES;
 //assert(!out_of_bounds);
 if(out_of_bounds) return (TileInstance){0};
 return l->tiles[t.y][t.x];
}

sg_image load_image(const char *path)
{
 sg_image to_return = {0};

 int png_width, png_height, num_channels;
 const int desired_channels = 4;
 stbi_uc* pixels = stbi_load(
   path,
   &png_width, &png_height,
   &num_channels, 0);
 assert(pixels);
 to_return = sg_make_image(&(sg_image_desc)
   {
   .width = png_width,
   .height = png_height,
   .pixel_format = SG_PIXELFORMAT_RGBA8,
   .min_filter = SG_FILTER_NEAREST,
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

#include "quad-sapp.glsl.h"
#include "assets.gen.c"

AnimatedSprite knight_idle =
{
 .img = &image_knight_idle,
 .time_per_frame = 0.3,
 .num_frames = 10,
 .start =
 {16.0f, 0.0f},
 .horizontal_diff_btwn_frames = 120.0,
 .region_size =
 {80.0f, 80.0f},
};

AnimatedSprite knight_running =
{
 .img = &image_knight_run,
 .time_per_frame = 0.06,
 .num_frames = 10,
 .start =
 {19.0f, 0.0f},
 .horizontal_diff_btwn_frames = 120.0,
 .region_size =
 {80.0f, 80.0f},
};

sg_image image_font =
{0};
const float font_size = 32.0;
stbtt_bakedchar cdata[96]; // ASCII 32..126 is 95 glyphs

// so can be grep'd and removed
#define dbgprint(...) { printf("Debug | %s:%d | ", __FILE__, __LINE__); printf(__VA_ARGS__); }

static struct
{
 sg_pass_action pass_action;
 sg_pipeline pip;
 sg_bindings bind;
} state;

Vec2 character_pos =
{0}; // world space point

void init(void)
{
 scratch = make(1024 * 10);
 stm_setup();
 sg_setup(& (sg_desc){
  .context = sapp_sgcontext()
 });

 load_assets();

 // player spawnpoint
 Vec2 spawnpoint_tilecoord = MulV2F(level_level0.spawnpoint, 1.0/TILE_SIZE);
 character_pos = tilecoord_to_world((TileCoord){(int)spawnpoint_tilecoord.X, (int)spawnpoint_tilecoord.Y});

 // load font
 {
  FILE* fontFile = fopen("assets/orange kid.ttf", "rb");
  fseek(fontFile, 0, SEEK_END);
  size_t size = ftell(fontFile); /* how long is the file ? */
  fseek(fontFile, 0, SEEK_SET); /* reset */

  char *fontBuffer = malloc(size);

  fread(fontBuffer, size, 1, fontFile);
  fclose(fontFile);

  unsigned char font_bitmap[512*512] =
  {0};
  stbtt_BakeFontBitmap(fontBuffer, 0, font_size, font_bitmap, 512, 512, 32, 96, cdata);

  unsigned char *font_bitmap_rgba = malloc(4 * 512 * 512); // stack would be too big if allocated on stack (stack overflow)
  for(int i = 0; i < 512 * 512; i++)
  {
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
    .data.subimage[0][0] =
    {
    .ptr = font_bitmap_rgba,
    .size = (size_t)(512 * 512 * 4),
    }
  } );
  free(font_bitmap_rgba);
 }

 state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc)
   {
    .usage = SG_USAGE_STREAM,
    //.data = SG_RANGE(vertices),
    .size = 1024*500,
    .label = "quad-vertices"
   });

 /* an index buffer with 2 triangles */
 uint16_t indices[] =
 { 0, 1, 2,  0, 2, 3 };
 state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
   .type = SG_BUFFERTYPE_INDEXBUFFER,
   .data = SG_RANGE(indices),
   .label = "quad-indices"
  });


 sg_shader shd = sg_make_shader(quad_program_shader_desc(sg_query_backend()));

 state.pip = sg_make_pipeline(&(sg_pipeline_desc)
   {
    .shader = shd,
    .index_type = SG_INDEXTYPE_UINT16,
    .layout = {
     .attrs =
     {
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

 state.pass_action = (sg_pass_action)
 {
  //.colors[0] = { .action=SG_ACTION_CLEAR, .value={12.5f/255.0f, 12.5f/255.0f, 12.5f/255.0f, 1.0f } }
  //.colors[0] = { .action=SG_ACTION_CLEAR, .value={255.5f/255.0f, 255.5f/255.0f, 255.5f/255.0f, 1.0f } }
  // 0x898989 is the color in tiled
  .colors[0] =
  { .action=SG_ACTION_CLEAR, .value={137.0f/255.0f, 137.0f/255.0f, 137.0f/255.0f, 1.0f } }
 };
}

typedef Vec4 Color;


#define WHITE (Color){1.0f, 1.0f, 1.0f, 1.0f}
#define BLACK (Color){0.0f, 0.0f, 0.0f, 1.0f}
#define RED   (Color){1.0f, 0.0f, 0.0f, 1.0f}


Vec2 screen_size()
{
 return V2((float)sapp_width(), (float)sapp_height());
}

typedef struct Camera
{
 Vec2 pos;
 float scale;
} Camera;


// everything is in pixels in world space, 43 pixels is approx 1 meter measured from 
// merchant sprite being 5'6"
const float pixels_per_meter = 43.0f;
Camera cam =
{.scale = 2.0f };

Vec2 cam_offset()
{
 return AddV2(cam.pos, MulV2F(screen_size(), 0.5f));
}

// in pixels
Vec2 img_size(sg_image img)
{
 sg_image_info info = sg_query_image_info(img);
 return V2((float)info.width, (float)info.height);
}

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
 to_return = MulV2F(to_return, 1.0f/cam.scale);
 return to_return;
}


Quad quad_at(Vec2 at, Vec2 size)
{
 Quad to_return;

 to_return.points[0] = V2(0.0, 0.0);
 to_return.points[1] = V2(size.X, 0.0);
 to_return.points[2] = V2(size.X, -size.Y);
 to_return.points[3] = V2(0.0, -size.Y);

 for(int i = 0; i < 4; i++)
 {
  to_return.points[i] = AddV2(to_return.points[i], at);
 }
 return to_return;
}

Quad tile_quad(TileCoord coord)
{
  return quad_at(tilecoord_to_world(coord), V2(TILE_SIZE, TILE_SIZE));
}

// out must be of at least length 4
Quad quad_centered(Vec2 at, Vec2 size)
{
 Quad to_return = quad_at(at, size);
 for(int i = 0; i < 4; i++)
 {
  to_return.points[i] = AddV2(to_return.points[i], V2(-size.X*0.5f, size.Y*0.5f));
 }
 return to_return;
}

Quad quad_aabb(AABB aabb)
{
 Vec2 size_vec = SubV2(aabb.lower_right, aabb.upper_left); // negative in vertical direction
 assert(size_vec.Y <= 0.0f);
 assert(size_vec.X >= 0.0f);
 return (Quad) {
  .ul = aabb.upper_left,
  .ur = AddV2(aabb.upper_left, V2(size_vec.X, 0.0f)),
  .lr = AddV2(aabb.upper_left, size_vec),
  .ll = AddV2(aabb.upper_left, V2(0.0f, size_vec.Y)),
 };
}


// both segment_a and segment_b must be arrays of length 2
bool segments_overlapping(float *a_segment, float *b_segment)
{
 assert(a_segment[1] >= a_segment[0]);
 assert(b_segment[1] >= b_segment[0]);
 float total_length = (a_segment[1] - a_segment[0]) + (b_segment[1] - b_segment[0]);
 float farthest_to_left = min(a_segment[0], b_segment[0]);
 float farthest_to_right = max(a_segment[1], b_segment[1]);
 if (farthest_to_right - farthest_to_left < total_length)
 {
  return true;
 } else
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
  if(segments_overlapping(a_segment, b_segment))
  {
  } else
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
  if(segments_overlapping(a_segment, b_segment))
  {
  } else
  {
   return false;
  }
 }

 return true; // both segments overlapping
}

// points must be of length 4, and be in the order: upper left, upper right, lower right, lower left
// the points are in pixels in screen space. The image region is in pixel space of the image
void draw_quad(bool world_space, Quad quad, sg_image image, AABB image_region, Color tint)
{
 Vec2 *points = quad.points;

 if(world_space)
 {
  for(int i = 0; i < 4; i++)
  {
   points[i] = world_to_screen(points[i]);
  }
 }
 AABB cam_aabb =
 { .upper_left = V2(0.0, screen_size().Y), .lower_right = V2(screen_size().X, 0.0) };
 AABB points_bounding_box =
 { .upper_left = V2(INFINITY, -INFINITY), .lower_right = V2(-INFINITY, INFINITY) };

 for(int i = 0; i < 4; i++)
 {
  points_bounding_box.upper_left.X = min(points_bounding_box.upper_left.X, points[i].X);
  points_bounding_box.upper_left.Y = max(points_bounding_box.upper_left.Y, points[i].Y);

  points_bounding_box.lower_right.X = max(points_bounding_box.lower_right.X, points[i].X);
  points_bounding_box.lower_right.Y = min(points_bounding_box.lower_right.Y, points[i].Y);
 }
 if(!overlapping(cam_aabb, points_bounding_box))
 {
  return; // cull out of screen quads
 }

 float new_vertices[ (2 + 2)*4 ];
 Vec2 region_size = SubV2(image_region.lower_right, image_region.upper_left);
 assert(region_size.X > 0.0);
 assert(region_size.Y > 0.0);
 Vec2 tex_coords[4] =
 {
  AddV2(image_region.upper_left, V2(0.0,                    0.0)),
  AddV2(image_region.upper_left, V2(region_size.X,           0.0)),
  AddV2(image_region.upper_left, V2(region_size.X, region_size.Y)),
  AddV2(image_region.upper_left, V2(0.0,           region_size.Y)),
 };
 // convert to uv space
 sg_image_info info = sg_query_image_info(image);
 for(int i = 0; i < 4; i++)
 {
  tex_coords[i] = DivV2(tex_coords[i], V2((float)info.width, (float)info.height));
 }
 for(int i = 0; i < 4; i++)
 {
  Vec2 zero_to_one = DivV2(points[i], screen_size());
  Vec2 in_clip_space = SubV2(MulV2F(zero_to_one, 2.0), V2(1.0, 1.0));
  new_vertices[i*4] = in_clip_space.X;
  new_vertices[i*4 + 1] = in_clip_space.Y;
  new_vertices[i*4 + 2] = tex_coords[i].X;
  new_vertices[i*4 + 3] = tex_coords[i].Y;
 }
 state.bind.vertex_buffer_offsets[0] = sg_append_buffer(state.bind.vertex_buffers[0], &SG_RANGE(new_vertices));
 quad_fs_params_t params =
 {0};
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

void swap(Vec2 *p1, Vec2 *p2)
{
 Vec2 tmp = *p1;
 *p1 = *p2;
 *p2 = tmp;
}

void draw_animated_sprite(AnimatedSprite *s, double time, bool flipped, Vec2 pos, Color tint)
{
 sg_image spritesheet_img = *s->img;
 int index = (int)floor(time/s->time_per_frame) % s->num_frames;

 Quad q = quad_centered(pos, s->region_size);

 if(flipped)
 {
  swap(&q.points[0], &q.points[1]);
  swap(&q.points[3], &q.points[2]);
 }

 AABB region;
 region.upper_left = AddV2(s->start, V2(index * s->horizontal_diff_btwn_frames, 0.0f));
 region.lower_right = V2(region.upper_left.X + (float)s->region_size.X, (float)s->region_size.Y);

 draw_quad(true, q, spritesheet_img, region, tint);
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

// returns bounds. To measure text you can set dry run to true and get the bounds
AABB draw_text(bool world_space, bool dry_run, const char *text, Vec2 pos, Color color)
{
 size_t text_len = strlen(text);
 AABB bounds =
 {0};
 float y = 0.0;
 float x = 0.0;
 for(int i = 0; i < text_len; i++)
 {
  stbtt_aligned_quad q;
  float old_y = y;
  stbtt_GetBakedQuad(cdata, 512, 512, text[i]-32, &x, &y, &q, 1);
  float difference = y - old_y;
  y = old_y + difference;

  Vec2 size = V2(q.x1 - q.x0, q.y1 - q.y0);
  if(text[i] == '\n')
  {
#ifdef DEVTOOLS
   y += font_size*0.75f; // arbitrary, only debug text has newlines
   x = 0.0;
#else
   assert(false);
#endif
  }
  if(size.Y > 0.0 && size.X > 0.0)
  { // spaces (and maybe other characters) produce quads of size 0
   Quad to_draw = {
   .points = {
     AddV2(V2(q.x0, -q.y0), V2(0.0f, 0.0f)),
     AddV2(V2(q.x0, -q.y0), V2(size.X, 0.0f)),
     AddV2(V2(q.x0, -q.y0), V2(size.X, -size.Y)),
     AddV2(V2(q.x0, -q.y0), V2(0.0f, -size.Y)),
    },
   };

   AABB font_atlas_region = (AABB)
   {
    .upper_left  = V2(q.s0, q.t0),
     .lower_right = V2(q.s1, q.t1),
   };
   font_atlas_region.upper_left.X *= img_size(image_font).X;
   font_atlas_region.lower_right.X *= img_size(image_font).X;
   font_atlas_region.upper_left.Y *= img_size(image_font).Y;
   font_atlas_region.lower_right.Y *= img_size(image_font).Y;

   for(int i = 0; i < 4; i++)
   {
    bounds.upper_left.X = min(bounds.upper_left.X, to_draw.points[i].X);
    bounds.upper_left.Y = max(bounds.upper_left.Y, to_draw.points[i].Y);
    bounds.lower_right.X = max(bounds.lower_right.X, to_draw.points[i].X);
    bounds.lower_right.Y = min(bounds.lower_right.Y, to_draw.points[i].Y);
   }

   for(int i = 0; i < 4; i++)
   {
    to_draw.points[i] = AddV2(to_draw.points[i], pos);
   }

   if(!dry_run)
   {
    draw_quad(world_space, to_draw, image_font, font_atlas_region, color);
   }
  }
 }

 bounds.upper_left = AddV2(bounds.upper_left, pos);
 bounds.lower_right = AddV2(bounds.lower_right, pos);
 return bounds;
}

void colorquad(bool world_space, Quad q, Color col)
{
 draw_quad(world_space, q, image_white_square, full_region(image_white_square), col);
}

void dbgsquare(Vec2 at)
{
 colorquad(true, quad_centered(at, V2(10.0, 10.0)), RED);
}

// in world coordinates
void line(Vec2 from, Vec2 to, float line_width, Color color)
{
 Vec2 normal = rotate_counter_clockwise(NormV2(SubV2(to, from)));
 Quad line_quad = {
  .points = {
  AddV2(from, MulV2F(normal, line_width)),  // upper left
  AddV2(to, MulV2F(normal, line_width)),    // upper right
  AddV2(to, MulV2F(normal, -line_width)),   // lower right
  AddV2(from, MulV2F(normal, -line_width)), // lower left
  }
 };
 colorquad(true, line_quad, color);
}

void dbgline(Vec2 from, Vec2 to)
{
 line(from, to, 2.0f, RED);
}

// in world space
void dbgrect(AABB rect)
{
 const float line_width = 0.5;
 const Color col = RED;
 Quad q = quad_aabb(rect);
 line(q.ul, q.ur, line_width, col);
 line(q.ur, q.lr, line_width, col);
 line(q.lr, q.ll, line_width, col);
 line(q.ll, q.ul, line_width, col);
}


double time = 0.0;
double last_frame_processing_time = 0.0;
uint64_t last_frame_time;
Vec2 mouse_pos =
{0}; // in screen space
bool character_facing_left = false;
bool keydown[SAPP_KEYCODE_MENU] =
{0};
#ifdef DEVTOOLS
bool mouse_frozen = false;
#endif
void frame(void)
{
 uint64_t time_start_frame = stm_now();
 // time
 double dt_double = 0.0;

 {
  dt_double = stm_sec(stm_diff(stm_now(), last_frame_time));
  dt_double = min(dt_double, 5.0 / 60.0); // clamp dt at maximum 5 frames, avoid super huge dt
  time += dt_double;
  last_frame_time = stm_now();
 }
 float dt = (float)dt_double;

 Vec2 movement = V2(
  (float)keydown[SAPP_KEYCODE_D] - (float)keydown[SAPP_KEYCODE_A],
  (float)keydown[SAPP_KEYCODE_W] - (float)keydown[SAPP_KEYCODE_S]
 );
 if(LenV2(movement) > 1.0)
 {
  movement = NormV2(movement);
 }
 sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
 sg_apply_pipeline(state.pip);

 // tilemap
#if 1
 Level * cur_level = &level_level0;
 for(int row = 0; row < LEVEL_TILES; row++)
 {
  for(int col = 0; col < LEVEL_TILES; col++)

  {
   TileCoord cur_coord = { col, row };
   TileInstance cur = get_tile(cur_level, cur_coord);
   TileSet tileset = tileset_ruins_animated;
   if(cur.kind != 0)
   {
    Vec2 tile_size = V2(TILE_SIZE, TILE_SIZE);

    sg_image tileset_image = *tileset.img;

    Vec2 tile_image_coord = tile_id_to_coord(tileset_image, tile_size, cur.kind);

    AnimatedTile *anim = NULL;
    for(int i = 0; i < sizeof(tileset.animated)/sizeof(*tileset.animated); i++)
    {
     if(tileset.animated[i].id_from == cur.kind-1)
     {
      anim = &tileset.animated[i];
     }
    }
    if(anim)
    {
     double time_per_frame = 0.1;
     int frame_index = (int)(time/time_per_frame) % anim->num_frames;
     tile_image_coord = tile_id_to_coord(tileset_image, tile_size, anim->frames[frame_index]+1);
    }

    AABB region;
    region.upper_left = tile_image_coord;
    region.lower_right = AddV2(region.upper_left, tile_size);

    draw_quad(true, tile_quad(cur_coord), tileset_image, region, WHITE);
   }
  }
 }
#endif



 Vec2 new_pos = AddV2(character_pos, MulV2F(movement, dt * pixels_per_meter * 4.0f));
 Vec2 character_aabb_size = { TILE_SIZE, TILE_SIZE };
 AABB at_new = centered_aabb(new_pos, character_aabb_size);
 Vec2 at_new_size_vector = SubV2(at_new.lower_right, at_new.upper_left);
 Vec2 points_to_check[] =
 {
  AddV2(at_new.upper_left, V2(0.0, 0.0)),
  AddV2(at_new.upper_left, V2(at_new_size_vector.X, 0.0)),
  AddV2(at_new.upper_left, V2(at_new_size_vector.X, at_new_size_vector.Y)),
  AddV2(at_new.upper_left, V2(0.0, at_new_size_vector.Y)),
 };
 //dbgsquare(character_pos);
 //dbgsquare(at_new.upper_left);
 //dbgsquare(at_new.lower_right);
 for(int i = 0; i < sizeof(points_to_check)/sizeof(*points_to_check); i++)
 {
  Vec2 *it = &points_to_check[i];
  TileCoord to_check = world_to_tilecoord(*it);

  uint16_t tile_id = get_tile(&level_level0, to_check).kind;
  if(tile_id == 53 || tile_id == 0 || tile_id == 367 || tile_id == 317 || tile_id == 313 || tile_id == 366)
  {
   dbgsquare(tilecoord_to_world(to_check));
   AABB to_depenetrate_from = tile_aabb(to_check);
   while(overlapping(to_depenetrate_from, at_new))
   { 
    //while(false)
    {
     //dbgsquare(to_depenetrate_from.upper_left);
     //dbgsquare(to_depenetrate_from.lower_right);
     const float move_dist = 0.05f;

     Vec2 to_player = NormV2(SubV2(aabb_center(at_new), aabb_center(to_depenetrate_from)));
     Vec2 compass_dirs[4] = {
      V2( 1.0, 0.0),
      V2(-1.0, 0.0),
      V2(0.0,  1.0),
      V2(0.0, -1.0),
     };
     int closest_index = -1;
     float closest_dot = -99999999.0f;
     for(int i = 0; i < 4; i++)
     {
      float dot = DotV2(compass_dirs[i], to_player);
      if(dot > closest_dot)
      {
       closest_index = i;
       closest_dot = dot;
      }
     }
     Vec2 move_dir = compass_dirs[closest_index];
     Vec2 move = MulV2F(move_dir, move_dist);
     at_new.upper_left = AddV2(at_new.upper_left,move);
     at_new.lower_right = AddV2(at_new.lower_right,move);
    }
   }
  }
 }
  character_pos = aabb_center(at_new);
  cam.pos = LerpV2(cam.pos, dt*8.0f, MulV2F(character_pos, -1.0f * cam.scale));

#ifdef DEVTOOLS
  dbgsquare(screen_to_world(mouse_pos));
  
  // tile coord
  {
   TileCoord hovering = world_to_tilecoord(screen_to_world(mouse_pos));
   Vec2 points[4] ={0};
   AABB q = tile_aabb(hovering);
   dbgrect(q);
   draw_text(false, false, tprint("%d", get_tile(&level_level0, hovering).kind), world_to_screen(tilecoord_to_world(hovering)), BLACK);
  }

  // line test
  {
   dbgline(character_pos, screen_to_world(mouse_pos));
  }

  // debug draw font image
  {
   draw_quad(true, quad_centered(V2(0.0, 0.0), V2(250.0, 250.0)), image_font,full_region(image_font), WHITE);
  }

  // statistics
  {
   Vec2 pos = V2(0.0, screen_size().Y);
   char *stats = tprint("Frametime: %.1f ms\nProcessing: %.1f ms", dt*1000.0, last_frame_processing_time*1000.0);
   AABB bounds = draw_text(false, true, stats, pos, BLACK);
   pos.Y -= bounds.upper_left.Y - screen_size().Y;
   bounds = draw_text(false, true, stats, pos, BLACK);
   // background panel
   colorquad(false, quad_aabb(bounds), (Color){1.0, 1.0, 1.0, 0.3f});
   draw_text(false, false, stats, pos, BLACK);
  }
  // text test render
#if 0
  const char *text = "great idea\nother idea";
  // measure text
  Vec2 pos = character_pos;

  {
   AABB bounds = draw_text(true, true, text, strlen(text), pos, WHITE);
   colorbox(true, bounds.upper_left, bounds.lower_right, (Color){1.0,0.0,0.0,0.5});
  }
  // draw text

  {
   draw_text(true, false, text, strlen(text), pos, WHITE);
  }
#endif

#endif // devtools

  if(fabsf(movement.X) > 0.01f) character_facing_left = movement.X < 0.0f;
  Vec2 character_sprite_pos = AddV2(character_pos, V2(0.0, 20.0f));
  if(LenV2(movement) > 0.01)
  {
   draw_animated_sprite(&knight_running, time, character_facing_left, character_sprite_pos, WHITE);
  } else
  {
   draw_animated_sprite(&knight_idle, time, character_facing_left, character_sprite_pos, WHITE);
  }


  sg_end_pass();
  sg_commit();

  last_frame_processing_time = stm_sec(stm_diff(stm_now(),time_start_frame));

  reset(&scratch);
}
void cleanup(void)
{
 sg_shutdown();
}

void event(const sapp_event *e)
{
 if(e->type == SAPP_EVENTTYPE_KEY_DOWN)
 {
  assert(e->key_code < sizeof(keydown)/sizeof(*keydown));
  keydown[e->key_code] = true;
  if(e->key_code == SAPP_KEYCODE_ESCAPE)
  {
   sapp_quit();
  }
#ifdef DEVTOOLS
  if(e->key_code == SAPP_KEYCODE_T)
  {
   mouse_frozen = !mouse_frozen;
  }
#endif
 }
 if(e->type == SAPP_EVENTTYPE_KEY_UP)
 {
  keydown[e->key_code] = false;
 }
 if(e->type == SAPP_EVENTTYPE_MOUSE_MOVE)
 {
  bool ignore_movement = false;
#ifdef DEVTOOLS
  if(mouse_frozen) ignore_movement = true;
#endif
  if(!ignore_movement) mouse_pos = V2(e->mouse_x, (float)sapp_height() - e->mouse_y);
 }
}

sapp_desc sokol_main(int argc, char* argv[])
{
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
