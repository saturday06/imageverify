#include <stdio.h>
#include <string.h>
#include <pango/pangoft2.h>
#include <raqm.h>
#include <math.h>
#include <ft2build.h>
#include FT_BITMAP_H
#include FT_GLYPH_H
#include "imageverify.h"
#include <string>
#include <fstream>
#include <iostream>
#include <fontconfig/fontconfig.h>
#include <string>
#include <vector>
#include <memory>
#include <libavutil/pixfmt.h>

#define SWS_FAST_BILINEAR     1
#define SWS_LANCZOS       0x200

extern "C" {
    void *sws_getContext(int srcW, int srcH, int srcFormat,
                                      int dstW, int dstH, int dstFormat,
                                      int flags, void *srcFilter,
                                      void *dstFilter, const double *param);
    int sws_scale(void *c, const uint8_t *const srcSlice[],
                  const int srcStride[], int srcSliceY, int srcSliceH,
                  uint8_t *const dst[], const int dstStride[]);

    void sws_freeContext(void *swsContext);
}

using std::vector;
using std::shared_ptr;

const int bwidth = 1300;
const int bheight = 300;

uint8_t buf[bwidth*bheight*4];

static FT_Int chooseBitmapStrike(FT_Face face, FT_F26Dot6 scaleY) {
    if (face == nullptr) {
        printf("chooseBitmapStrike aborted due to nullptr face.\n");
        return -1;
    }
    FT_Pos requestedPPEM = scaleY;  // FT_Bitmap_Size::y_ppem is in 26.6 format.
    FT_Int chosenStrikeIndex = -1;
    FT_Pos chosenPPEM = 0;
    for (FT_Int strikeIndex = 0; strikeIndex < face->num_fixed_sizes; ++strikeIndex) {
        FT_Pos strikePPEM = face->available_sizes[strikeIndex].y_ppem;
        if (chosenStrikeIndex == -1) {
            chosenPPEM = strikePPEM;
            chosenStrikeIndex = strikeIndex;
        } else if (strikePPEM == requestedPPEM) {
            // exact match - our search stops here
            return strikeIndex;
        } else if (chosenPPEM < requestedPPEM) {
            // attempt to increase chosenPPEM
            if (chosenPPEM < strikePPEM) {
                chosenPPEM = strikePPEM;
                chosenStrikeIndex = strikeIndex;
            }
        } else {
            // attempt to decrease chosenPPEM, but not below requestedPPEM
            if (requestedPPEM < strikePPEM && strikePPEM < chosenPPEM) {
                chosenPPEM = strikePPEM;
                chosenStrikeIndex = strikeIndex;
            }
        }
    }
    return chosenStrikeIndex;
}

FT_Face getDefaultFace(FT_Library ft, int fontSize) {
    shared_ptr<FcPattern> pattern(FcPatternCreate(), FcPatternDestroy);
    FcChar8 family[] = "Noto Color Emoji";
    if (!FcPatternAddString(pattern.get(), FC_FAMILY, family)) {
        return NULL;
    }
    if (!FcPatternAddDouble(pattern.get(), FC_SIZE, fontSize)) {
        return NULL;
    }
    
    FcDefaultSubstitute(pattern.get());
    
    FcResult matchResult;
    shared_ptr<FcPattern> font(FcFontMatch(NULL, pattern.get(), &matchResult), FcPatternDestroy);
    if (!font) {
        return NULL;
    }
    
    FcChar8 *faceFile;
    FcResult fileResult = FcPatternGetString(font.get(), FC_FILE, 0, &faceFile);
    if (fileResult != FcResultMatch) {
        return NULL;
    }
    
    int faceIndex = 0; // 取得失敗したら0
    FcPatternGetInteger(font.get(), FC_INDEX, 0, &faceIndex);
    
    FT_Face face;
    FT_Error faceError = FT_New_Face(ft, reinterpret_cast<char*>(faceFile), faceIndex, &face);
    if (faceError) {
        return NULL;
    }

    FcPatternPrint(font.get());
    return face;
}

int image_verify() {
    FT_Library ft;
    FT_Error ftError;
    ftError = FT_Init_FreeType(&ft);
    if (ftError) {
        // メモリ無い？
        return 1;
    }
    
    FcChar8 utf8[] = "日本語helloالسلام🍣👍🏿🍺👨‍👩‍👧‍👦👍🏻てすとてすと☺️😇";
    int fontSize = 32;

    int ucs4Len;
    int utf8MaxWidth;
    if (!FcUtf8Len(utf8, strlen(reinterpret_cast<char*>(&utf8)), &ucs4Len, &utf8MaxWidth)) {
        // 無効なUTF8
        return 1;
    }
    vector<FcChar32> ucs4(ucs4Len);
    {
        int from = 0;
        for (int to = 0; to < ucs4.size(); to++) {
            int adv = FcUtf8ToUcs4(&utf8[from], &ucs4[to], strlen(reinterpret_cast<char*>(utf8)) - from);
            if (adv <= 0) {
                // ?
                return 1;
            }
            from += adv;
        }
    }

    shared_ptr<raqm_t> raqm(raqm_create(), raqm_destroy);
    if (!raqm) {
        // no mem
        return 1;
    }
    
    if (!raqm_set_text(raqm.get(), &ucs4[0], ucs4.size())) {
        // no mem?
        return 1;
    }

    if (!raqm_set_par_direction(raqm.get(), RAQM_DIRECTION_LTR)) {
        // no mem?
        return 1;
    }
    
    FT_Face defaultFace = getDefaultFace(ft, fontSize);
    vector<double> faceFactors(ucs4.size());
    for (int i = 0; i < ucs4.size(); i++) {
        double faceFactor = 1;
        FT_Face face;
        if (FT_Get_Char_Index(defaultFace, ucs4[i]) != 0) {
            face = defaultFace;
        } else {
            shared_ptr<FcCharSet> charset(FcCharSetCreate(), FcCharSetDestroy);
            if (!charset) {
                // No memory
                return 1;
            }
            if (!FcCharSetAddChar(charset.get(), ucs4[i])) {
                // 謎
                return 1;
            }
            
            shared_ptr<FcPattern> pattern(FcPatternCreate(), FcPatternDestroy);
            if (!FcPatternAddCharSet(pattern.get(), FC_CHARSET, charset.get())) {
                // 謎
                return 1;
            }
            FcChar8 family[] = "Noto Sans";
            if (!FcPatternAddString(pattern.get(), FC_FAMILY, family)) {
                return 1;
            }
            if (!FcPatternAddDouble(pattern.get(), FC_SIZE, fontSize)) {
                return 1;
            }
            
            FcDefaultSubstitute(pattern.get());
            
            // FcPatternPrint(pattern.get());
            FcResult matchResult;
            shared_ptr<FcPattern> font(FcFontMatch(NULL, pattern.get(), &matchResult), FcPatternDestroy);
            if (!font) {
                // マッチしない
                return 1;
            }
            
            FcChar8 *faceFile;
            FcResult fileResult = FcPatternGetString(font.get(), FC_FILE, 0, &faceFile);
            if (fileResult != FcResultMatch) {
                // ファイルが無い
                return 1;
            }
            
            int faceIndex = 0; // 取得失敗したら0
            FcPatternGetInteger(font.get(), FC_INDEX, 0, &faceIndex);
            
            FT_Error faceError = FT_New_Face(ft, reinterpret_cast<char*>(faceFile), faceIndex, &face);
            if (faceError) {
                // ロードエラー
                continue;
            }
            
            // FcPatternPrint(font.get());
        }
        
        const FT_F26Dot6 y_ppem = static_cast<FT_F26Dot6>(fontSize * 64);
        if (FT_IS_SCALABLE(face)) {
            auto sizeError = FT_Set_Char_Size(face, y_ppem, y_ppem, 72, 72);
            if (sizeError) {
                printf("Size Error %d\n", sizeError);
                return 1;
            }
        } else if (FT_HAS_FIXED_SIZES(face)) {
            printf("strike\n");
            int fStrikeIndex = chooseBitmapStrike(face, fontSize);
            if (fStrikeIndex == -1) {
                printf("strike\n");
                return 1;
            }
            auto err = FT_Select_Size(face, fStrikeIndex);
            if (err != 0) {
                printf("FT_Select_Size\n");
                fStrikeIndex = -1;
                return 1;
            }
            printf("request=%d, %d, %d\n", fontSize, face->size->metrics.x_ppem, face->size->metrics.y_ppem);
            faceFactor = (double)fontSize / (double)face->size->metrics.y_ppem;
        } else {
            printf("error6\n");
            return 1;
        }
        if (!raqm_set_freetype_face_range(raqm.get(), face, i, 1)) {
            // no mem?
            return 1;
        }
        faceFactors[i] = faceFactor;
    }
    
    if (!raqm_layout(raqm.get())) {
        // no mem?
        return 1;
    }

    size_t numGlyphs = 0;
    raqm_glyph_t* glyphs = raqm_get_glyphs(raqm.get(), &numGlyphs);
    double xbase = 80;
    double ybase = 150;
    for (size_t glyphIndex = 0; glyphIndex < numGlyphs; ++glyphIndex) {
        auto glyph = glyphs[glyphIndex];
        double factor = faceFactors[glyphIndex];
        printf("g=%d i=%d x=%f y=%f %d %d %d %d\n", glyphIndex, glyph.index, xbase, ybase, glyph.x_offset, glyph.y_offset, glyph.x_advance, glyph.y_advance);
        // Even though x_offset is getting added, y_offset wants to be subtracted.
        // See https://github.com/googlei18n/fontview/issues/2 for pictures.
        double glyphX = xbase + glyph.x_offset / 64.0 * factor;
        double glyphY = ybase - glyph.y_offset / 64.0 * factor;
        int xadvance = glyph.x_advance * factor;
        int yadvance = glyph.y_advance * factor;

        auto er = FT_Load_Glyph(glyph.ftface, glyph.index, FT_LOAD_RENDER | FT_LOAD_COLOR);
        if (er) {
            printf("error1 %d\n", er);
        } else if (!glyph.ftface->glyph) {
            printf("error2\n");
        } else if (glyph.ftface->glyph->bitmap.width > 0 && glyph.ftface->glyph->bitmap.rows > 0) {
            double xPos = xbase;
            double yPos = ybase;
            xPos += glyph.ftface->glyph->bitmap_left * factor;
            yPos -= glyph.ftface->glyph->bitmap_top * factor;
            printf("scale=%f\n", faceFactors[glyphIndex]);
            if (glyph.ftface->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
                FT_Bitmap bitmap = glyph.ftface->glyph->bitmap;
                int width = bitmap.width;
                int height = bitmap.rows;
                int stride = bitmap.pitch;
                unsigned char* src = bitmap.buffer;
                if (factor != 1) {
                    width *= factor;
                    height *= factor;
                    stride = width * 4;
                    if (width == 0 || height == 0) {
                        continue;
                    }
                    src = reinterpret_cast<unsigned char*>(calloc(stride * height, 1)); // leak
                    shared_ptr<void> sws(sws_getContext(bitmap.width, bitmap.rows, AV_PIX_FMT_ABGR, width, height, AV_PIX_FMT_ABGR, SWS_FAST_BILINEAR, NULL, NULL, NULL), sws_freeContext);
                    uint8_t* srcBuf[4] = {bitmap.buffer};
                    uint8_t* dstBuf[4] = {reinterpret_cast<uint8_t*>(src)};
                    int srcStride[4] = {bitmap.pitch};
                    int dstStride[4] = {stride};
                    sws_scale(sws.get(), srcBuf, srcStride, 0, bitmap.rows, dstBuf, dstStride);
                }
                for (int x = 0; x < width; x++) {
                    for (int y = 0; y < height; y++) {
                        int xx = xPos + x;
                        int yy = yPos + y;
                        // buf[(int)xPos + x + ((int)yPos + y) * bitmap.pitch];
                        // printf("pix(%d, %d) = %d\n", xx, yy, bitmap.buffer[x + y * bitmap.pitch]);
                        int to = (xx + yy * bwidth) * 4;
                        int from = x * 4 + y * stride;
                        buf[to + 0] = src[from + 0];
                        buf[to + 1] = src[from + 1];
                        buf[to + 2] = src[from + 2];
                        buf[to + 3] = src[from + 3];
                    }
                }
            } else {
                FT_Bitmap bitmap;
                FT_Bitmap_Init(&bitmap);
                FT_Bitmap_Convert(glyph.ftface->glyph->library, &glyph.ftface->glyph->bitmap, &bitmap, 1);
                for (int y = 0; y < bitmap.rows; y++) {
                    for (int x = 0; x < bitmap.width; x++) {
                        int xx = xPos + x;
                        int yy = yPos + y;
                        // printf("pix(%d, %d) = %d\n", xx, yy, bitmap.buffer[x + y * bitmap.pitch]);
                        int to = (xx + yy * bwidth) * 4;
                        int from = x + y * bitmap.pitch;
                        buf[to + 0] = bitmap.buffer[from];
                        buf[to + 1] = bitmap.buffer[from];
                        buf[to + 2] = bitmap.buffer[from];
                        buf[to + 3] = 255;
                    }
                }
                FT_Bitmap_Done(glyph.ftface->glyph->library, &bitmap);
            }
        }
        xbase += xadvance / 64.0;
        ybase += yadvance / 64.0;
    }
    
    std::ofstream ppm("foo.ppm");
    ppm << "P3" << std::endl << bwidth << " " << bheight << std::endl << "255" << std::endl;
    for (int y = 0; y < bheight; y++) {
        for (int x = 0; x < bwidth; x++) {
            int off = (x + y * bwidth) * 4;
            ppm << (int)buf[off + 2] << " " << (int)buf[off + 1] << " " << (int)buf[off] << "  ";
        }
        ppm << std::endl;
    }
    printf("OK\n");
    return 0;
}

void DrawGlyph(FT_Face face, FT_UInt glyph,
               double xPos, double yPos) {
    int fontSize_ = 24;
    const double scale = 1.0;
    const FT_F26Dot6 size = static_cast<FT_F26Dot6>(fontSize_ * 64 * scale + 0.5);
    auto er = FT_Load_Glyph(face, glyph, FT_LOAD_RENDER /*| FT_LOAD_COLOR*/);
    if (er) {
        printf("errora %d\n", er);
        return;
    }
    if (!face->glyph) {
        printf("errorb\n");
        return;
    }

    const size_t width = face->glyph->bitmap.width;
    const size_t height = face->glyph->bitmap.rows;
    if (width == 0 || height == 0) {
        return;
    }
    
    FT_Bitmap ftBitmap;
    FT_Bitmap_Init(&ftBitmap);
    FT_Bitmap_Convert(face->glyph->library, &face->glyph->bitmap, &ftBitmap, 1);
    xPos += face->glyph->bitmap_left / scale;
    yPos -= face->glyph->bitmap_top / scale;
    for (int x = 0; x < ftBitmap.width; x++) {
        for (int y = 0; y < ftBitmap.rows; y++) {
            int xx = xPos + x;
            int yy = yPos + y;
            // buf[(int)xPos + x + ((int)yPos + y) * ftBitmap.pitch];
            printf("pix(%d, %d) = %d\n", xx, yy, ftBitmap.buffer[x + y * ftBitmap.pitch]);
            buf[xx + yy * bwidth] = ftBitmap.buffer[x + y * ftBitmap.pitch];
        }
    }
//    const int leftOffset = CompensateRounding(xPos, scale);
//    const int topOffset = CompensateRounding(yPos, scale);
    
//    wxBitmap bitmap;
//    if (bitmap.CreateScaled(ceil(width / scale) + leftOffset,
//                            ceil(height / scale) + topOffset,
//                            32, scale)) {
//        CopyAlpha(ftBitmap, leftOffset, topOffset, &bitmap);
//        dc.DrawBitmap(bitmap, xPos, yPos);
//    }
    
    FT_Bitmap_Done(face->glyph->library, &ftBitmap);
}

int image_verify2()
{
    /*
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
    bmp.pitch = width;
    bmp.buffer = (unsigned char*)malloc(bmp.rows * bmp.width);
    if (NULL == bmp.buffer) {
        printf("+ error: cannot allocate the buffer for the output bitmap.\n");
        exit(EXIT_FAILURE);
    }
    
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

  // create layout object.
  layout = pango_layout_new(context);
  if (NULL == layout) {
    printf("+ error: cannot create the pango layout.\n");
    exit(EXIT_FAILURE);
  }

    font_desc = pango_font_description_from_string("Station 35");
    pango_layout_set_font_description(layout, font_desc);
    pango_font_map_load_font(font_map, context, font_desc);
    pango_font_description_free(font_desc);
  pango_layout_set_text(layout, "hello", -1);

  // render
  pango_ft2_render_layout(&bmp, layout, 0, 0);
*/
	int argc = 4;
	const char* argv[] = {
			"prog",
			//"/Users/saturday06/Downloads/f/google-fonts/ufl/ubuntu/Ubuntu-Regular.ttf",
            "/Users/saturday06/Downloads/f/google-noto-fonts/NotoColorEmoji.ttf",
			"🍣🍺🍻",
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

    dir = RAQM_DIRECTION_LTR;


    rq = raqm_create ();
    if (rq == NULL)
        goto final;

    if (!raqm_set_text_utf8 (rq, text, strlen (text)))
        goto final;

    if (!raqm_set_freetype_face(rq, face))
        goto final;

    if (!raqm_set_par_direction(rq, dir))
        goto final;

    if (!raqm_layout(rq))
        goto final;

    glyphs = raqm_get_glyphs(rq, &count);
    if (glyphs == NULL)
        goto final;
    
    for (i = 0; i < count; i++)
    {
        // auto g = glyphs[i];
        printf ("index=%d x=%d y=%d xadv=%d yadv=%d clust=%d\n",
               glyphs[i].index,
               glyphs[i].x_offset,
               glyphs[i].y_offset,
               glyphs[i].x_advance,
               glyphs[i].y_advance,
               glyphs[i].cluster);
        //glyphs[i].ftface;
    }

    ret = 0;

final:
    raqm_destroy (rq);
    FT_Done_Face (face);
    FT_Done_FreeType (ft_library);

    return ret;
}

int image_verifyx() {
    int argc = 4;
    const char* argv[] = {
        "prog",
        //"/Users/saturday06/Downloads/f/google-fonts/ufl/ubuntu/Ubuntu-Regular.ttf",
        // "/Users/saturday06/Downloads/f/google-noto-fonts/NotoColorEmoji.ttf",
        "/Users/saturday06/Downloads/f/google-noto-fonts/NotoEmoji-Regular.ttf",
        "🍣🍻",
        "-ltr"
    };
    
    const char *fontfile;
    const char *direction;
    int i;
    int ret = 1;
    
    raqm_t *rq = NULL;
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
    std::string text_(argv[2]);
    std::string textLanguage_ = "ja";
    direction = argv[3];
    
    ft_error = FT_Init_FreeType (&ft_library);
    if (ft_error) {
        printf("error1\n");
        return 0;
    }
    
    ft_error = FT_New_Face (ft_library, fontfile, 0, &face);
    if (ft_error) {
        printf("error3\n");
        return 0;
    }
    
    //ft_error = FT_Set_Char_Size (face, face->units_per_EM, 0, 0, 0);
    //if (ft_error) {
    //    printf("error5 %d\n", ft_error);
    //    return 0;
    //}

    int fontSize_ = 24;
    const double scale = 1;
    auto fontFace_ = face;
    const FT_F26Dot6 size = static_cast<FT_F26Dot6>(fontSize_ * 64 * scale + 0.5);
    if (FT_IS_SCALABLE(face)) {
        ft_error = FT_Set_Char_Size(fontFace_, size, size, 72, 72);
        if (ft_error) {
            printf("error4 %d\n", ft_error);
            return 0;
        }
    } else if (FT_HAS_FIXED_SIZES(face)) {
        
    } else {
        printf("error6\n");
        return 1;
    }
    
    raqm_t* layout = raqm_create();
    if (!raqm_set_text_utf8(layout, &text_.front(), text_.size()) ||
        !raqm_set_par_direction(layout, RAQM_DIRECTION_DEFAULT) ||
        !raqm_set_language(layout, textLanguage_.c_str(), 0, text_.size()) ||
        !raqm_set_freetype_face(layout, fontFace_) ||
        // !raqm_set_line_width(layout, GetSize().x - 8) ||
        !raqm_layout(layout)) {
        raqm_destroy(layout);
        printf("error5\n");
        return 0;
    }
    
    const double ascender = fontSize_ *
    (static_cast<double>(fontFace_->ascender) /
     static_cast<double>(fontFace_->units_per_EM));
    size_t numGlyphs = 0;
    raqm_glyph_t* glyphs = raqm_get_glyphs(layout, &numGlyphs);
    const double border = 4 * scale;
    double x = border, y = border + ascender;
    for (size_t i = 0; i < numGlyphs; ++i) {
        // Even though x_offset is getting added, y_offset wants to be subtracted.
        // See https://github.com/googlei18n/fontview/issues/2 for pictures.
        double glyphX = x + glyphs[i].x_offset / (scale * 64.0);
        double glyphY = y - glyphs[i].y_offset / (scale * 64.0);
        DrawGlyph(glyphs[i].ftface, glyphs[i].index, glyphX, glyphY);
        x += glyphs[i].x_advance / (scale * 64.0);
        y -= glyphs[i].y_advance / (scale * 64.0);
    }
    
    raqm_destroy(layout);
    
    std::ofstream ppm("foo.ppm");
    ppm << "P2" << std::endl << bwidth << " " << bheight << std::endl << "256" << std::endl;
    for (int y = 0; y < bheight; y++) {
        for (int x = 0; x < bwidth; x++) {
            ppm << (int)buf[x + y * bwidth] << " ";
        }
        ppm << std::endl;
    }
    return 0;
}
