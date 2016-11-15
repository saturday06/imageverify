#include <stdio.h>
#include <string.h>
#include <pango/pangoft2.h>
#include <raqm.h>
#include <freetype2/ft2build.h>
#include FT_BITMAP_H
#include "imageverify.h"

int image_verify()
{
  PangoContext* context = NULL;
  PangoLayout* layout = NULL;
  PangoFontDescription* font_desc = NULL;
  PangoFontMap* font_map = NULL;
  FT_Bitmap bmp = {0};
  int stride = 0;
  int width = 640;
  int height = 480;
  FT_Bitmap_New(&bmp);
  bmp.rows = height;
  bmp.width = width;
  font_map = pango_ft2_font_map_new();
  if (NULL == font_map) {
    printf("+ error: cannot create the pango font map.\n");
    exit(EXIT_FAILURE);
  }

  context = pango_font_map_create_context(font_map);
  if (NULL == context) {
    printf("+ error: cannot create pango font context.\n");
    exit(EXIT_FAILURE);
  }

  /* create layout object. */
  layout = pango_layout_new(context);
  if (NULL == layout) {
    printf("+ error: cannot create the pango layout.\n");
    exit(EXIT_FAILURE);
  }

  pango_layout_set_text(layout, "hello", -1);

  /* render */
  pango_ft2_render_layout(&bmp, layout, 0, 0);

	int argc = 4;
	const char* argv[] = {
			"prog",
			"/Users/saturday06/Downloads/f/google-fonts/ufl/ubuntu/Ubuntu-Regular.ttf",
			"Hello",
			"-ltr"
	};

    const char *fontfile;
    const char *text;
    const char *direction;
    int i;
    int ret = 1;

    raqm_t *rq = NULL;
    raqm_glyph_t *glyphs = NULL;
    size_t count;
    raqm_direction_t dir;

    FT_Library ft_library = NULL;
    FT_Face face = NULL;
    FT_Error ft_error;

    if (argc < 4)
    {
        printf ("Usage:\n FONT_FILE TEXT DIRECTION\n");
        return 1;
    }

    fontfile =  argv[1];
    text = argv[2];
    direction = argv[3];

    ft_error = FT_Init_FreeType (&ft_library);
    if (ft_error)
        goto final;
    ft_error = FT_New_Face (ft_library, fontfile, 0, &face);
    if (ft_error)
        goto final;
    ft_error = FT_Set_Char_Size (face, face->units_per_EM, 0, 0, 0);
    if (ft_error)
        goto final;

    dir = RAQM_DIRECTION_DEFAULT;
    if (strcmp (direction, "-rtl") == 0)
      dir = RAQM_DIRECTION_RTL;
    else if (strcmp (direction, "-ltr") == 0)
      dir = RAQM_DIRECTION_LTR;

    rq = raqm_create ();
    if (rq == NULL)
        goto final;

    if (!raqm_set_text_utf8 (rq, text, strlen (text)))
        goto final;

    if (!raqm_set_freetype_face (rq, face))
        goto final;

    if (!raqm_set_par_direction (rq, dir))
        goto final;

    if (!raqm_layout (rq))
        goto final;

    glyphs = raqm_get_glyphs (rq, &count);
    if (glyphs == NULL)
        goto final;


    for (i = 0; i < count; i++)
    {
        printf ("%d %d %d %d %d %d\n",
               glyphs[i].index,
               glyphs[i].x_offset,
               glyphs[i].y_offset,
               glyphs[i].x_advance,
               glyphs[i].y_advance,
               glyphs[i].cluster);
    }

    ret = 0;


final:
    raqm_destroy (rq);
    FT_Done_Face (face);
    FT_Done_FreeType (ft_library);

    return ret;
}
