#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

#define QUAD_WIDTH 32

#define RED 0
#define GREEN 1
#define BLUE 2
#define ALPHA 3

#define MASK_RED(COLOR)   ((COLOR & 0xff000000) >> 24)
#define MASK_GREEN(COLOR) ((COLOR & 0xff0000) >> 16)
#define MASK_BLUE(COLOR)  ((COLOR & 0xff00) >> 8)
#define MASK_ALPHA(COLOR) (COLOR & 0xff)

static guchar *
gen_tex_data (uint32_t color)
{
  guchar *tex_data, *p;
  uint8_t r = MASK_RED (color);
  uint8_t g = MASK_GREEN (color);
  uint8_t b = MASK_BLUE (color);
  uint8_t a = MASK_ALPHA (color);

  tex_data = g_malloc (QUAD_WIDTH * QUAD_WIDTH * 4);

  for (p = tex_data + QUAD_WIDTH * QUAD_WIDTH * 4; p > tex_data;)
    {
      *(--p) = a;
      *(--p) = b;
      *(--p) = g;
      *(--p) = r;
    }

  return tex_data;
}

static CoglTexture *
make_texture (uint32_t color,
	      CoglPixelFormat src_format,
	      CoglPixelFormat internal_format)
{
  CoglTexture2D *tex_2d;
  guchar *tex_data = gen_tex_data (color);

  tex_2d = cogl_texture_2d_new_from_data (ctx,
                                          QUAD_WIDTH,
                                          QUAD_WIDTH,
                                          src_format,
                                          internal_format,
                                          QUAD_WIDTH * 4,
                                          tex_data,
                                          NULL);

  g_free (tex_data);

  return COGL_TEXTURE (tex_2d);
}

static void
set_region (CoglTexture *tex,
	    uint32_t color,
	    CoglPixelFormat format)
{
  guchar *tex_data = gen_tex_data (color);

  cogl_texture_set_region (tex,
                           0, 0, /* src x, y */
                           0, 0, /* dst x, y */
                           QUAD_WIDTH, QUAD_WIDTH, /* dst width, height */
                           QUAD_WIDTH, QUAD_WIDTH, /* src width, height */
                           format,
                           0, /* auto compute row stride */
                           tex_data);
}

static void
check_texture (CoglPipeline *pipeline,
	       CoglHandle material,
	       int x,
	       int y,
	       CoglTexture *tex,
	       uint32_t expected_result)
{
  /* Legacy */
  cogl_push_framebuffer (fb);
  cogl_material_set_layer (material, 0, tex);
  cogl_set_source (material);
  cogl_rectangle (x * QUAD_WIDTH,
		  y * QUAD_WIDTH,
		  x * QUAD_WIDTH + QUAD_WIDTH,
		  y * QUAD_WIDTH + QUAD_WIDTH);
  test_utils_check_pixel (fb, x * QUAD_WIDTH + QUAD_WIDTH / 2, y * QUAD_WIDTH + QUAD_WIDTH / 2, expected_result);
  cogl_pop_framebuffer ();

  /* New API */
  cogl_pipeline_set_layer_texture (pipeline, 0, COGL_TEXTURE (tex));
  cogl_framebuffer_draw_rectangle (fb, pipeline,
				   x * QUAD_WIDTH,
				   y * QUAD_WIDTH,
				   x * QUAD_WIDTH + QUAD_WIDTH,
				   y * QUAD_WIDTH + QUAD_WIDTH);
  test_utils_check_pixel (fb, x * QUAD_WIDTH + QUAD_WIDTH / 2, y * QUAD_WIDTH + QUAD_WIDTH / 2, expected_result);
}

void
test_premult (void)
{
  CoglPipeline *pipeline;
  CoglHandle material;
  CoglTexture *tex;

  cogl_framebuffer_orthographic (fb, 0, 0,
				 cogl_framebuffer_get_width (fb),
				 cogl_framebuffer_get_height (fb),
				 -1,
				 100);

  cogl_framebuffer_clear4f (fb,
                            COGL_BUFFER_BIT_COLOR,
                            1.0f, 1.0f, 1.0f, 1.0f);

  /* Legacy */
  material = cogl_material_new ();
  cogl_material_set_blend (material,
                           "RGBA = ADD (SRC_COLOR, 0)", NULL);
  cogl_material_set_layer_combine (material, 0,
                                   "RGBA = REPLACE (TEXTURE)", NULL);

  /* New API */
  pipeline = cogl_pipeline_new (ctx);
  cogl_pipeline_set_blend (pipeline,
                           "RGBA = ADD (SRC_COLOR, 0)", NULL);
  cogl_pipeline_set_layer_combine (pipeline, 0,
                                   "RGBA = REPLACE (TEXTURE)", NULL);

  /* If the user explicitly specifies an unmultiplied internal format then
   * Cogl shouldn't automatically premultiply the given texture data... */
  if (cogl_test_verbose ())
    g_print ("make_texture (0xff00ff80, "
                            "src = RGBA_8888, internal = RGBA_8888)\n");
  tex = make_texture (0xff00ff80,
                      COGL_PIXEL_FORMAT_RGBA_8888, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888); /* internal format */
  check_texture (pipeline, material, 0, 0, /* position */
		 tex,
		 0xff00ff80); /* expected */

  /* If the user explicitly requests a premultiplied internal format and
   * gives unmultiplied src data then Cogl should always premultiply that
   * for us */
  if (cogl_test_verbose ())
    g_print ("make_texture (0xff00ff80, "
                            "src = RGBA_8888, internal = RGBA_8888_PRE)\n");
  tex = make_texture (0xff00ff80,
                      COGL_PIXEL_FORMAT_RGBA_8888, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE); /* internal format */
  check_texture (pipeline, material, 1, 0, /* position */
		 tex,
		 0x80008080); /* expected */

  /* If the user gives COGL_PIXEL_FORMAT_ANY for the internal format then
   * by default Cogl should premultiply the given texture data...
   * (In the future there will be additional Cogl API to control this
   *  behaviour) */
  if (cogl_test_verbose ())
    g_print ("make_texture (0xff00ff80, "
                            "src = RGBA_8888, internal = ANY)\n");
  tex = make_texture (0xff00ff80,
                      COGL_PIXEL_FORMAT_RGBA_8888, /* src format */
                      COGL_PIXEL_FORMAT_ANY); /* internal format */
  check_texture (pipeline, material, 2, 0, /* position */
		 tex,
		 0x80008080); /* expected */

  /* If the user requests a premultiplied internal texture format and supplies
   * premultiplied source data, Cogl should never modify that source data...
   */
  if (cogl_test_verbose ())
    g_print ("make_texture (0x80008080, "
                            "src = RGBA_8888_PRE, "
                            "internal = RGBA_8888_PRE)\n");
  tex = make_texture (0x80008080,
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE); /* internal format */
  check_texture (pipeline, material, 3, 0, /* position */
		 tex,
		 0x80008080); /* expected */

  /* If the user requests an unmultiplied internal texture format, but
   * supplies premultiplied source data, then Cogl should always
   * un-premultiply the source data... */
  if (cogl_test_verbose ())
    g_print ("make_texture (0x80008080, "
                            "src = RGBA_8888_PRE, internal = RGBA_8888)\n");
  tex = make_texture (0x80008080,
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888); /* internal format */
  check_texture (pipeline, material, 4, 0, /* position */
		 tex,
		 0xff00ff80); /* expected */

  /* If the user allows any internal texture format and provides premultipled
   * source data then by default Cogl shouldn't modify the source data...
   * (In the future there will be additional Cogl API to control this
   *  behaviour) */
  if (cogl_test_verbose ())
    g_print ("make_texture (0x80008080, "
                            "src = RGBA_8888_PRE, internal = ANY)\n");
  tex = make_texture (0x80008080,
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE, /* src format */
                      COGL_PIXEL_FORMAT_ANY); /* internal format */
  check_texture (pipeline, material, 5, 0, /* position */
		 tex,
		 0x80008080); /* expected */

  /*
   * Test cogl_texture_set_region() ....
   */

  if (cogl_test_verbose ())
    g_print ("make_texture (0xDEADBEEF, "
                            "src = RGBA_8888, internal = RGBA_8888)\n");
  tex = make_texture (0xDEADBEEF,
                      COGL_PIXEL_FORMAT_RGBA_8888, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888); /* internal format */
  if (cogl_test_verbose ())
    g_print ("set_region (0xff00ff80, RGBA_8888)\n");
  set_region (tex, 0xff00ff80, COGL_PIXEL_FORMAT_RGBA_8888);
  check_texture (pipeline, material, 6, 0, /* position */
		 tex,
		 0xff00ff80); /* expected */

  /* Updating a texture region for an unmultiplied texture using premultiplied
   * region data should result in Cogl unmultiplying the given region data...
   */
  if (cogl_test_verbose ())
    g_print ("make_texture (0xDEADBEEF, "
                            "src = RGBA_8888, internal = RGBA_8888)\n");
  tex = make_texture (0xDEADBEEF,
                      COGL_PIXEL_FORMAT_RGBA_8888, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888); /* internal format */
  if (cogl_test_verbose ())
    g_print ("set_region (0x80008080, RGBA_8888_PRE)\n");
  set_region (tex, 0x80008080, COGL_PIXEL_FORMAT_RGBA_8888_PRE);
  check_texture (pipeline, material, 7, 0, /* position */
		 tex,
		 0xff00ff80); /* expected */


  if (cogl_test_verbose ())
    g_print ("make_texture (0xDEADBEEF, "
                            "src = RGBA_8888_PRE, "
                            "internal = RGBA_8888_PRE)\n");
  tex = make_texture (0xDEADBEEF,
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE); /* internal format */
  if (cogl_test_verbose ())
    g_print ("set_region (0x80008080, RGBA_8888_PRE)\n");
  set_region (tex, 0x80008080, COGL_PIXEL_FORMAT_RGBA_8888_PRE);
  check_texture (pipeline, material, 8, 0, /* position */
		 tex,
		 0x80008080); /* expected */


  /* Updating a texture region for a premultiplied texture using unmultiplied
   * region data should result in Cogl premultiplying the given region data...
   */
  if (cogl_test_verbose ())
    g_print ("make_texture (0xDEADBEEF, "
                            "src = RGBA_8888_PRE, "
                            "internal = RGBA_8888_PRE)\n");
  tex = make_texture (0xDEADBEEF,
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE, /* src format */
                      COGL_PIXEL_FORMAT_RGBA_8888_PRE); /* internal format */
  if (cogl_test_verbose ())
    g_print ("set_region (0xff00ff80, RGBA_8888)\n");
  set_region (tex, 0xff00ff80, COGL_PIXEL_FORMAT_RGBA_8888);
  check_texture (pipeline, material, 9, 0, /* position */
		 tex,
		 0x80008080); /* expected */


  if (cogl_test_verbose ())
    g_print ("OK\n");
}

