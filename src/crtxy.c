/*
  crtxy.c

  CRT X-Y library (libcrtxy)
  http://libcrtxy.sf.net/

  Bill Kendrick <bill@newbreedsoftware.com>

  July 29, 2008 - August 2, 2008
*/

#include "crtxy.h"
#include <SDL_image.h>

SDL_Surface * XY_screen;
XY_fixed XY_canvasw, XY_canvash;
Uint32 XY_background_color;
Uint8 XY_background_r, XY_background_g, XY_background_b;
XY_bitmap * XY_background_bitmap;
XY_bool XY_background_bitmap_possible, XY_antialias;
XY_bool XY_background_bitmap_enabled;
SDL_Rect XY_background_dest;
Uint32 XY_want_fps, XY_start_time;
int XY_err_code;

/* FIXME: Always putting to XY_screen, so we can simplify further... */
void (*putpixel) (SDL_Surface *, int, int, Uint32, Uint8 alph);
Uint32 (*getpixel) (SDL_Surface * surface, int x, int y);
SDL_Surface * (*scale_surf) (SDL_Surface * orig, int new_w, int new_h);

int XY_trig[91] = {
  65536,  65526,  65496,  65446,  65376,
  65287,  65177,  65048,  64898,  64729,
  64540,  64332,  64104,  63856,  63589,
  63303,  62997,  62672,  62328,  61966,
  61584,  61183,  60764,  60326,  59870,
  59396,  58903,  58393,  57865,  57319,
  56756,  56175,  55578,  54963,  54332,
  53684,  53020,  52339,  51643,  50931,
  50203,  49461,  48703,  47930,  47143,
  46341,  45525,  44695,  43852,  42995,
  42126,  41243,  40348,  39441,  38521,
  37590,  36647,  35693,  34729,  33754,
  32768,  31772,  30767,  29753,  28729,
  27697,  26656,  25607,  24550,  23486,
  22415,  21336,  20252,  19161,  18064,
  16962,  15855,  14742,  13626,  12505,
  11380,  10252,   9121,   7987,   6850,
   5712,   4572,   3430,   2287,   1144,
      0
};

enum {
  XY_ERR_NONE,
  XY_ERR_OPTION_BAD,
  XY_ERR_OPTION_UNKNOWN,
  NUM_XY_ERRS
};

char * XY_errstr_txt[NUM_XY_ERRS] = {
  "",
  "Bad argument to option",
  "Unknown option"
};


/* Private functions and macros: */

void putpixel_16(SDL_Surface * surface, int x, int y, Uint32 pixel, Uint8 alph);
void putpixel_24(SDL_Surface * surface, int x, int y, Uint32 pixel, Uint8 alph);
void putpixel_32(SDL_Surface * surface, int x, int y, Uint32 pixel, Uint8 alph);

void putpixel_fakea_16(SDL_Surface * surface, int x, int y, Uint32 pixel, Uint8 alph);
void putpixel_fakea_24(SDL_Surface * surface, int x, int y, Uint32 pixel, Uint8 alph);
void putpixel_fakea_32(SDL_Surface * surface, int x, int y, Uint32 pixel, Uint8 alph);

void putpixel_reala_16(SDL_Surface * surface, int x, int y, Uint32 pixel, Uint8 alph);
void putpixel_reala_24(SDL_Surface * surface, int x, int y, Uint32 pixel, Uint8 alph);
void putpixel_reala_32(SDL_Surface * surface, int x, int y, Uint32 pixel, Uint8 alph);

Uint32 getpixel_16(SDL_Surface * surface, int x, int y);
Uint32 getpixel_24(SDL_Surface * surface, int x, int y);
Uint32 getpixel_32(SDL_Surface * surface, int x, int y);

#define XY_color_to_sdl_color(color) SDL_MapRGB(XY_screen->format, \
                                       ((color) >> 24) & 0xFF, \
                                       ((color) >> 16) & 0xFF, \
                                       ((color) >> 8) & 0xFF)

SDL_Surface * scale_surf_best(SDL_Surface * orig, int new_w, int new_h);
SDL_Surface * scale_surf_fast(SDL_Surface * orig, int new_w, int new_h);

void XY_draw_line_xiaolinwu(XY_fixed fsx1, XY_fixed fsy1,
                            XY_fixed fsx2, XY_fixed fsy2,
                            XY_color color);
void XY_draw_line_bresenham(XY_fixed fsx1, XY_fixed fsy1,
                            XY_fixed fsx2, XY_fixed fsy2,
                            XY_color);

/* Public functions: */

void XY_default_options(XY_options * opts)
{
  opts->displayw = 320;
  opts->displayh = 240;
  opts->displaybpp = 16;
  opts->fullscreen = XY_OPT_WINDOWED;
  opts->alpha = XY_OPT_ALPHA_BLEND;
  opts->antialias = XY_TRUE;
  opts->blur = XY_FALSE;
  opts->additive = XY_FALSE;
  opts->backgrounds = XY_TRUE;
  opts->scaling = XY_OPT_SCALE_FAST;
}

int XY_parse_options(int * argc, char * argv[], XY_options * opts)
{
  int i, j, eat, err_arg;
  int nextint;
  char * nextstr;

  XY_err_code = XY_ERR_NONE;
  err_arg = 0;

  for (i = 0; i < *argc && XY_err_code == XY_ERR_NONE; i++)
  {
    eat = 0;

    if (strcmp(argv[i], "--") == 0)
    {
      /* Double-dash says "end of options", so break out */
      i = *argc;
    }
    else
    {
      if (i < (*argc - 1))
      {
        nextint = atoi(argv[i + 1]);
        nextstr = argv[i + 1];
      }
      else
      {
        nextint = 0;
        nextstr = "";
      }

      if (strcmp(argv[i], "--crtxy-width") == 0)
      {
        if (nextint != 0)
        {
          opts->displayw = nextint;
          eat = 2;
        }
        else
        {
          XY_err_code = XY_ERR_OPTION_BAD;
          err_arg = i;
        }
      }
      else if (strcmp(argv[i], "--crtxy-height") == 0)
      {
        if (nextint != 0)
        {
          opts->displayh = nextint;
          eat = 2;
        }
        else
        {
          XY_err_code = XY_ERR_OPTION_BAD;
          err_arg = i;
        }
      }
      else if (strcmp(argv[i], "--crtxy-bpp") == 0)
      {
        if (nextint == 16 || nextint == 24 || nextint == 32)
        {
          opts->displaybpp = nextint;
          eat = 2;
        }
        else if (strcmp(nextstr, "any") == 0)
        {
          opts->displaybpp = 0;
          eat = 2;
        }
        else
        {
          XY_err_code = XY_ERR_OPTION_BAD;
          err_arg = i;
        }
      }
      else if (strcmp(argv[i], "--crtxy-fullscreen") == 0)
      {
        opts->fullscreen = XY_OPT_FULLSCREEN_REQUIRED;
        eat = 1;
      }
      else if (strcmp(argv[i], "--crtxy-fullscreen-or-window") == 0)
      {
        opts->fullscreen = XY_OPT_FULLSCREEN_REQUEST;
        eat = 1;
      }
      else if (strcmp(argv[i], "--crtxy-windowed") == 0)
      {
        opts->fullscreen = XY_OPT_WINDOWED;
        eat = 1;
      }
      else if (strcmp(argv[i], "--crtxy-alpha") == 0)
      {
        if (strcmp(nextstr, "on") == 0)
        {
          opts->alpha = XY_OPT_ALPHA_BLEND;
          eat = 2;
        }
        else if (strcmp(nextstr, "fake") == 0)
        {
          opts->alpha = XY_OPT_ALPHA_FAKE;
          eat = 2;
        }
        else if (strcmp(nextstr, "off") == 0)
        {
          opts->alpha = XY_OPT_ALPHA_OFF;
          eat = 2;
        }
        else
        {
          XY_err_code = XY_ERR_OPTION_BAD;
          err_arg = i;
        }
      }
      else if (strcmp(argv[i], "--crtxy-antialias") == 0)
      {
        if (strcmp(nextstr, "on") == 0)
        {
          opts->antialias = XY_TRUE;
          eat = 2;
        }
        else if (strcmp(nextstr, "off") == 0)
        {
          opts->antialias = XY_FALSE;
          eat = 2;
        }
        else
        {
          XY_err_code = XY_ERR_OPTION_BAD;
          err_arg = i;
        }
      }
      else if (strcmp(argv[i], "--crtxy-blur") == 0)
      {
        if (strcmp(nextstr, "on") == 0)
        {
          opts->blur = XY_TRUE;
          eat = 2;
        }
        else if (strcmp(nextstr, "off") == 0)
        {
          opts->blur = XY_FALSE;
          eat = 2;
        }
        else
        {
          XY_err_code = XY_ERR_OPTION_BAD;
          err_arg = i;
        }
      }
      else if (strcmp(argv[i], "--crtxy-additive") == 0)
      {
        if (strcmp(nextstr, "on") == 0)
        {
          opts->additive = XY_TRUE;
          eat = 2;
        }
        else if (strcmp(nextstr, "off") == 0)
        {
          opts->additive = XY_FALSE;
          eat = 2;
        }
        else
        {
          XY_err_code = XY_ERR_OPTION_BAD;
          err_arg = i;
        }
      }
      else if (strcmp(argv[i], "--crtxy-backgrounds") == 0)
      {
        if (strcmp(nextstr, "on") == 0)
        {
          opts->backgrounds = XY_TRUE;
          eat = 2;
        }
        else if (strcmp(nextstr, "off") == 0)
        {
          opts->backgrounds = XY_FALSE;
          eat = 2;
        }
        else
        {
          XY_err_code = XY_ERR_OPTION_BAD;
          err_arg = i;
        }
      }
      else if (strcmp(argv[i], "--crtxy-scaling") == 0)
      {
        if (strcmp(nextstr, "best") == 0)
        {
          opts->scaling = XY_OPT_SCALE_BEST;
          eat = 2;
        }
        else if (strcmp(nextstr, "fast") == 0)
        {
          opts->scaling = XY_OPT_SCALE_FAST;
          eat = 2;
        }
        else
        {
          XY_err_code = XY_ERR_OPTION_BAD;
          err_arg = i;
        }
      }
      else if (strstr(argv[i], "--crtxy") == argv[i])
      {
        XY_err_code = XY_ERR_OPTION_UNKNOWN;
        err_arg = i;
      }
    }

    if (eat > 0 && XY_err_code == XY_ERR_NONE)
    {
      for (j = i; j + eat < *argc; j++)
        argv[j] = argv[j + eat];

      *argc -= eat;
      i--;
    }
  }

  return(err_arg);
}

int XY_init(XY_options * opts, XY_fixed canvasw, XY_fixed canvash)
{
  int Bpp;

  if (SDL_Init(SDL_INIT_VIDEO) < 0)
  {
    /* FIXME: Set error */
    /* FIXME: Call SDL_QuitSubSystem(SDL_INIT_VIDEO) here? */
    return(-1);
  }

  XY_canvasw = canvasw;
  XY_canvash = canvash;

  /* FIXME: Add possibility of using SDL_ListModes to determine a suitable mode */
  /* FIXME: Options */
  if (opts->fullscreen == XY_OPT_WINDOWED)
  {
    XY_screen = SDL_SetVideoMode(opts->displayw, opts->displayh,
                                 opts->displaybpp, SDL_SWSURFACE);
  }
  else
  {
    XY_screen = SDL_SetVideoMode(opts->displayw, opts->displayh,
                                 opts->displaybpp,
                                 SDL_SWSURFACE | SDL_FULLSCREEN);

    if (XY_screen == NULL)
    {
      if (opts->fullscreen == XY_OPT_FULLSCREEN_REQUEST)
      {
        XY_screen = SDL_SetVideoMode(opts->displayw, opts->displayh,
                                     opts->displaybpp, SDL_SWSURFACE);
      }
    }
  }

  if (XY_screen == NULL)
  {
    /* FIXME: Set error */
    SDL_Quit();
    return(-1);
  }

  XY_background_color = SDL_MapRGB(XY_screen->format, 0x00, 0x00, 0x00);
  XY_background_r = 0;
  XY_background_g = 0;
  XY_background_b = 0;
  XY_background_bitmap = NULL;
  XY_background_bitmap_enabled = XY_FALSE;
  XY_background_dest.x = 0;
  XY_background_dest.y = 0;

  XY_background_bitmap_possible = opts->backgrounds;

  /* Determine which functions to use, based on display we actually got,
     and rendering options: */

  Bpp= XY_screen->format->BytesPerPixel;

  if (Bpp == 4)
    getpixel = getpixel_32;
  else if (Bpp == 3)
    getpixel = getpixel_24;
  else if (Bpp == 2)
    getpixel = getpixel_16;
  else
  {
    /* Unsupported depth */
    /* FIXME: Set error */
    return(-1);
  }

 
  XY_antialias = opts->antialias;

  if (opts->alpha == XY_OPT_ALPHA_FAKE)
  {
    /* Fake alpha blending (blend with background color) */
    if (Bpp == 4) putpixel = putpixel_fakea_32;
    else if (Bpp == 3) putpixel = putpixel_fakea_24;
    else if (Bpp == 2) putpixel = putpixel_fakea_16;
  }
  else if (opts->alpha == XY_OPT_ALPHA_BLEND)
  {
    /* Real alpha blending (blend with current pixel) */
    if (Bpp == 4) putpixel = putpixel_reala_32;
    else if (Bpp == 3) putpixel = putpixel_reala_24;
    else if (Bpp == 2) putpixel = putpixel_reala_16;
  }
  else
  {
    /* No alpha blending */
    if (Bpp == 4) putpixel = putpixel_32;
    else if (Bpp == 3) putpixel = putpixel_24;
    else if (Bpp == 2) putpixel = putpixel_16;
  }

  if (opts->scaling == XY_OPT_SCALE_BEST)
    scale_surf = scale_surf_best;
  else
    scale_surf = scale_surf_fast;

  return(0);
}

void XY_quit(void)
{
  if (XY_background_bitmap != NULL)
    XY_free_bitmap(XY_background_bitmap);

  /* FIXME: Use SDL_QuitSubSystem(SDL_INIT_VIDEO) instead? */
  SDL_Quit();
}

char * XY_errstr(void)
{
  return(XY_errstr_txt[XY_err_code]);
}

void XY_print_options(FILE * fi, XY_options opts)
{
  fprintf(fi, "Screen size: %d x %d pixels\n", opts.displayw, opts.displayh);
  fprintf(fi, "Screen depth: %d bpp\n", opts.displaybpp);
  fprintf(fi, "Fullscreen: %s\n",
    (opts.fullscreen == XY_OPT_WINDOWED ? "no" :
     (opts.fullscreen == XY_OPT_FULLSCREEN_REQUEST ? "requested" :
      "required")));
  fprintf(fi, "Alpha-blending: %s\n",
    (opts.alpha == XY_OPT_ALPHA_BLEND ? "real" :
     (opts.alpha == XY_OPT_ALPHA_FAKE ? "fake" :
      "off")));
  fprintf(fi, "Antialiasing: %s\n",
    (opts.antialias ? "on" : "off"));
  fprintf(fi, "Blurring: %s\n",
    (opts.blur ? "on" : "off"));
  fprintf(fi, "Additive effect: %s\n",
    (opts.additive ? "on" : "off"));
  fprintf(fi, "Background bitmaps: %s\n",
    (opts.backgrounds ? "on" : "off"));
  fprintf(fi, "Scaling: %s\n",
    (opts.scaling == XY_OPT_SCALE_BEST ? "best" :
     "fast"));
}

XY_bitmap * XY_load_bitmap(char * filename)
{
  SDL_Surface * surf, * dispsurf;
  XY_bitmap * xyb;

  xyb = (XY_bitmap *) malloc(sizeof(XY_bitmap));
  if (xyb == NULL)
    return(NULL); /* FIXME: Set error */

  surf = IMG_Load(filename);
  if (surf == NULL)
  {
    free(xyb);
    return(NULL); /* FIXME: Set error */
  }

  dispsurf = SDL_DisplayFormatAlpha(surf);
  SDL_FreeSurface(surf);
  if (dispsurf == NULL)
  {
    free(xyb);
    return(NULL); /* FIXME: Set error */
  }

  xyb->surf = dispsurf;

  return(xyb);
}

XY_bitmap * XY_load_bitmap_from_buffer(unsigned char * buffer, int size)
{
  SDL_RWops * rw;
  SDL_Surface * surf, * dispsurf;
  XY_bitmap * xyb;

  if (buffer == NULL || size == 0)
    return(NULL); /* FIXME: Set error */

  xyb = (XY_bitmap *) malloc(sizeof(XY_bitmap));
  if (xyb == NULL)
    return(NULL); /* FIXME: Set error */

  rw = SDL_RWFromMem((void *) buffer, size);

  surf = IMG_Load_RW(rw, 0);
  if (surf == NULL)
  {
    free(xyb);
    return(NULL); /* FIXME: Set error */
  }

  dispsurf = SDL_DisplayFormatAlpha(surf);
  SDL_FreeSurface(surf);
  if (dispsurf == NULL)
  {
    free(xyb);
    return(NULL); /* FIXME: Set error */
  }

  xyb->surf = dispsurf;

  return(xyb);
}

void XY_free_bitmap(XY_bitmap * bitmap)
{
  if (bitmap != NULL)
  {
    if (bitmap->surf != NULL)
      SDL_FreeSurface(bitmap->surf);

    free(bitmap);
  }
}

void XY_set_background(XY_color color, XY_bitmap * bitmap,
                       XY_fixed x, XY_fixed y, int posflags, int scaling)
{
  int w, h;
  int posx, posy;

  XY_background_r = (color >> 24) & 0xFF;
  XY_background_g = (color >> 16) & 0xFF;
  XY_background_b = (color >> 8) & 0xFF;
  XY_background_color = SDL_MapRGB(XY_screen->format,
                                   XY_background_r,
                                   XY_background_g,
                                   XY_background_b);

  /* FIXME: Adhere to bitmap option setting! */

  if (XY_background_bitmap != NULL)
    XY_free_bitmap(XY_background_bitmap);

  if (bitmap != NULL && bitmap->surf != NULL && XY_background_bitmap_possible)
  {
    w = bitmap->surf->w;
    h = bitmap->surf->h;

    XY_background_bitmap_enabled = XY_TRUE;
    XY_background_bitmap = malloc(sizeof(XY_bitmap));
    /* FIXME: check for errors */

    if (scaling == XY_SCALE_STRETCH)
    {
      w = XY_screen->w;
      h = XY_screen->h;
    }
    else if (scaling == XY_SCALE_KEEP_ASPECT_WIDE)
    {
      h = (((h << XY_FIXED_SHIFT) / w) * XY_screen->w) >> XY_FIXED_SHIFT;
      w = XY_screen->w;
    }
    else if (scaling == XY_SCALE_KEEP_ASPECT_TALL)
    {
      w = (((w << XY_FIXED_SHIFT) / h) * XY_screen->h) >> XY_FIXED_SHIFT;
      h = XY_screen->h;
    }


    if (w != bitmap->surf->w || h != bitmap->surf->h)
    {
      /* What we calculated is different from what we have; scale it! */
      SDL_Surface * scaled_surf = scale_surf(bitmap->surf, w, h);
      if (scaled_surf == NULL)
      {
        free(XY_background_bitmap);
        XY_background_bitmap = NULL;
        return; /* FIXME: Report errors */
      }
      XY_background_bitmap->surf = SDL_DisplayFormatAlpha(scaled_surf);
      SDL_FreeSurface(scaled_surf);
    }
    else
    {
      XY_background_bitmap->surf = SDL_DisplayFormatAlpha(bitmap->surf);
    }

    if (XY_background_bitmap->surf == NULL)
    {
      free(XY_background_bitmap);
      XY_background_bitmap = NULL;
      return; /* FIXME: Report errors */
    }

    /* Position */
    if (posflags & XY_POS_HCENTER)
      posx = (XY_screen->w - w) / 2;
    else if (posflags & XY_POS_RIGHT)
      posx = XY_screen->w - w;
    else
      posx = 0;

    if (posflags & XY_POS_VCENTER)
      posy = (XY_screen->h - h) / 2;
    else if (posflags & XY_POS_BOTTOM)
      posy = XY_screen->h - h;
    else
      posy = 0;

    /* Nudge */
    XY_background_dest.x = posx + x;
    XY_background_dest.y = posy + y;
  }
  else
  {
    /* No bitmap being used */
    XY_background_bitmap_enabled = XY_FALSE;
    XY_background_bitmap = NULL;
  }
}

SDL_Surface * scale_surf_best(SDL_Surface * orig, int new_w, int new_h)
{
  return(NULL);
}

SDL_Surface * scale_surf_fast(SDL_Surface * orig, int new_w, int new_h)
{
  SDL_Surface * s;
  int x, y;
  Uint32 tr, tg, tb, ta;
  Uint8 r, g, b, a;
  int tmp;
  XY_fixed src_x, src_y, xscale, yscale, f_orig_w, f_orig_h;
  void (*pp) (SDL_Surface *, int, int, Uint32, Uint8 alph);
  Uint32 (*gp) (SDL_Surface * surface, int x, int y);

  yscale = (orig->h << XY_FIXED_SHIFT) / new_h;
  xscale = (orig->w << XY_FIXED_SHIFT) / new_w;

  f_orig_w = orig->w << XY_FIXED_SHIFT;
  f_orig_h = orig->h << XY_FIXED_SHIFT;

  s = SDL_CreateRGBSurface(orig->flags,	/* SDL_SWSURFACE, */
			   new_w, new_h,
			   orig->format->BitsPerPixel,
			   orig->format->Rmask,
			   orig->format->Gmask,
			   orig->format->Bmask,
			   orig->format->Amask);
  /* FIXME: Check for errors */

  if (orig->format->BytesPerPixel == 2)
    gp = getpixel_16;
  else if (orig->format->BytesPerPixel == 3)
    gp = getpixel_24;
  else if (orig->format->BytesPerPixel == 4)
    gp = getpixel_32;

  if (s->format->BytesPerPixel == 2)
    pp = putpixel_16;
  else if (s->format->BytesPerPixel == 3)
    pp = putpixel_24;
  else if (s->format->BytesPerPixel == 4)
    pp = putpixel_32;

  for (y = 0; y < new_h; y++)
  {
    for (x = 0; x < new_w; x++)
    {
      tr = 0;
      tg = 0;
      tb = 0;
      ta = 0;

      tmp = 0;

      for (src_y = y * yscale; src_y < y * yscale + yscale &&
	   src_y < f_orig_h; src_y += XY_FIXED_ONE)
      {
	for (src_x = x * xscale; src_x < x * xscale + xscale &&
	     src_x < f_orig_w; src_x += XY_FIXED_ONE)
	{
	  SDL_GetRGBA(gp(orig, src_x >> XY_FIXED_SHIFT, src_y >> XY_FIXED_SHIFT),
		      orig->format, &r, &g, &b, &a);

	  tr = tr + r;
	  tb = tb + b;
	  tg = tg + g;
	  ta = ta + a;

	  tmp++;
	}
      }

      if (tmp != 0)
      {
	tr = tr / tmp;
	tb = tb / tmp;
	tg = tg / tmp;
	ta = ta / tmp;

	pp(s, x, y, SDL_MapRGB(s->format,
				(Uint8) tr,
				(Uint8) tg,
				(Uint8) tb),
	  (Uint8) ta);
      }
    }
  }

  return(s);
}

XY_color XY_setcolor(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
  return ((r << 24) | (g << 16) | (b << 8) | a);
}

void XY_getcolor(XY_color c, Uint8 * r, Uint8 * g, Uint8 * b, Uint8 * a)
{
  *r = (c >> 24) & 0xff;
  *r = (c >> 16) & 0xff;
  *r = (c >> 8) & 0xff;
  *a = c & 0xff;
}

void XY_enable_background(XY_bool enable)
{
  XY_background_bitmap_enabled = enable;
}

void XY_start_frame(int fps)
{
  /* FIXME: Dirty rects? */
  SDL_FillRect(XY_screen, NULL, XY_background_color);

  if (XY_background_bitmap != NULL && XY_background_bitmap_enabled == XY_TRUE)
    SDL_BlitSurface(XY_background_bitmap->surf, NULL,
                    XY_screen, &XY_background_dest);

  XY_want_fps = (fps == 0 ? 1 : fps);
  XY_start_time = SDL_GetTicks();
}

int XY_end_frame(XY_bool throttle)
{
  Uint32 end_time;

  /* FIXME: Dirty rects? */
  SDL_Flip(XY_screen);
  end_time = SDL_GetTicks();

  if (throttle)
  {
    if (end_time - XY_start_time < (1000 / XY_want_fps))
      SDL_Delay(XY_start_time + (1000 / XY_want_fps) - end_time);

    end_time = SDL_GetTicks();
  }

  return(end_time - XY_start_time);
}

void XY_draw_line(XY_fixed x1, XY_fixed y1, XY_fixed x2, XY_fixed y2,
                  XY_color color)
{
  int sx1, sy1, sx2, sy2;
  XY_fixed fsx1, fsy1, fsx2, fsy2;

  if ((color & 0xff) == 0)
    return; /* Fully transparent! */

  XY_canvas_to_screen(x1, y1, &sx1, &sy1);
  XY_canvas_to_screen(x2, y2, &sx2, &sy2);

  fsx1 = sx1 << XY_FIXED_SHIFT;
  fsy1 = sy1 << XY_FIXED_SHIFT;
  fsx2 = sx2 << XY_FIXED_SHIFT;
  fsy2 = sy2 << XY_FIXED_SHIFT;

  if (XY_antialias)
    XY_draw_line_xiaolinwu(fsx1, fsy1, fsx2, fsy2, color);
  else
    XY_draw_line_bresenham(fsx1, fsy1, fsx2, fsy2, color);
}

void XY_draw_line_xiaolinwu(XY_fixed fsx1, XY_fixed fsy1,
                            XY_fixed fsx2, XY_fixed fsy2,
                            XY_color color)
{
  Uint32 sdlcolor = XY_color_to_sdl_color(color);
  XY_fixed dx, dy, ftmp, gradient;
  XY_fixed xend, yend, xgap, ygap, xpxl1, ypxl1, xpxl2, ypxl2, interx, intery;
  XY_fixed brightness1, brightness2, x, y;
  Uint8 alph;

  alph = (color & 0xff);

  dx = fsx2 - fsx1;
  dy = fsy2 - fsy1;

  if (abs(dx) > abs(dy))
  {
    /* "Horizontal" lines */
    if (fsx2 < fsx1)
    {
      ftmp = fsx1;
      fsx1 = fsx2;
      fsx2 = ftmp;

      ftmp = fsy1;
      fsy1 = fsy2;
      fsy2 = ftmp;
    }

    gradient = XY_div(dy, dx);

    /* First endpoint */
    xend = XY_round(fsx1);

    yend = fsy1 + XY_mult(gradient, (xend - fsx1));
    xgap = XY_rfpart(fsx1 + XY_FIXED_HALF);
    xpxl1 = xend >> XY_FIXED_SHIFT;
    ypxl1 = XY_ipart(yend) >> XY_FIXED_SHIFT;
   
    brightness1 = (XY_mult(XY_rfpart(yend), xgap) * 255) >> XY_FIXED_SHIFT;
    brightness2 = (XY_mult(XY_fpart(yend), xgap) * 255) >> XY_FIXED_SHIFT;
  
    putpixel(XY_screen, xpxl1, ypxl1,
             sdlcolor, (brightness1 * alph) / 255);
    putpixel(XY_screen, xpxl1, ypxl1 + 1,
             sdlcolor, (brightness2 * alph) / 255);
  
    intery = yend + gradient;
  
    /* Second endpoint */
    xend = XY_round(fsx2);
  
    yend = fsy2 + XY_mult(gradient, (xend - fsx2));
    xgap = XY_fpart(fsx2 + XY_FIXED_HALF);
    xpxl2 = xend >> XY_FIXED_SHIFT;
    ypxl2 = XY_ipart(yend) >> XY_FIXED_SHIFT;
  
    brightness1 = (XY_mult(XY_rfpart(yend), xgap) * 255) >> XY_FIXED_SHIFT;
    brightness2 = (XY_mult(XY_fpart(yend), xgap) * 255) >> XY_FIXED_SHIFT;
  
    putpixel(XY_screen, xpxl2, ypxl2,
             sdlcolor, (brightness1 * alph) / 255);
    putpixel(XY_screen, xpxl2, ypxl2 + 1,
             sdlcolor, (brightness2 * alph) / 255);
  
    /* Main loop */
    for (x = xpxl1 + 1; x < xpxl2; x++)
    {
      brightness1 = (XY_rfpart(intery) * 255) >> XY_FIXED_SHIFT;
      brightness2 = (XY_fpart(intery) * 255) >> XY_FIXED_SHIFT;
  
      putpixel(XY_screen, x, XY_ipart(intery) >> XY_FIXED_SHIFT,
               sdlcolor, (brightness1 * alph) / 255);
      putpixel(XY_screen, x, (XY_ipart(intery) >> XY_FIXED_SHIFT) + 1,
               sdlcolor, (brightness2 * alph) / 255);
  
      intery += gradient;
    }
  }
  else
  {
    /* "Vertical" lines */
    if (fsy2 < fsy1)
    {
      ftmp = fsx1;
      fsx1 = fsx2;
      fsx2 = ftmp;

      ftmp = fsy1;
      fsy1 = fsy2;
      fsy2 = ftmp;
    }

    gradient = XY_div(dx, dy);

    /* First endpoint */
    yend = XY_round(fsy1);

    xend = fsx1 + XY_mult(gradient, (yend - fsy1));
    ygap = XY_rfpart(fsy1 + XY_FIXED_HALF);
    xpxl1 = XY_ipart(xend) >> XY_FIXED_SHIFT;
    ypxl1 = yend >> XY_FIXED_SHIFT;
   
    brightness1 = (XY_mult(XY_rfpart(xend), ygap) * 255) >> XY_FIXED_SHIFT;
    brightness2 = (XY_mult(XY_fpart(xend), ygap) * 255) >> XY_FIXED_SHIFT;
  
    putpixel(XY_screen, xpxl1, ypxl1,
             sdlcolor, (brightness1 * alph) / 255);
    putpixel(XY_screen, xpxl1 + 1, ypxl1,
             sdlcolor, (brightness2 * alph) / 255);
  
    interx = xend + gradient;
  
    /* Second endpoint */
    yend = XY_round(fsy2);
  
    xend = fsx2 + XY_mult(gradient, (yend - fsy2));
    ygap = XY_fpart(fsy2 + XY_FIXED_HALF);
    xpxl2 = XY_ipart(xend) >> XY_FIXED_SHIFT;
    ypxl2 = yend >> XY_FIXED_SHIFT;
  
    brightness1 = (XY_mult(XY_rfpart(xend), ygap) * 255) >> XY_FIXED_SHIFT;
    brightness2 = (XY_mult(XY_fpart(xend), ygap) * 255) >> XY_FIXED_SHIFT;
  
    putpixel(XY_screen, xpxl2, ypxl2,
             sdlcolor, (brightness1 * alph) / 255);
    putpixel(XY_screen, xpxl2 + 1, ypxl2,
             sdlcolor, (brightness2 * alph) / 255);
  
    /* Main loop */
    for (y = ypxl1 + 1; y < ypxl2; y++)
    {
      brightness1 = (XY_rfpart(interx) * 255) >> XY_FIXED_SHIFT;
      brightness2 = (XY_fpart(interx) * 255) >> XY_FIXED_SHIFT;
  
      putpixel(XY_screen, XY_ipart(interx) >> XY_FIXED_SHIFT, y,
               sdlcolor, (brightness1 * alph) / 255);
      putpixel(XY_screen, (XY_ipart(interx) >> XY_FIXED_SHIFT) + 1, y,
               sdlcolor, (brightness2 * alph) / 255);
  
      interx += gradient;
    }
  }
}

void XY_draw_line_bresenham(XY_fixed fsx1, XY_fixed fsy1,
                            XY_fixed fsx2, XY_fixed fsy2,
                            XY_color color)
{
  Uint32 sdlcolor = XY_color_to_sdl_color(color);
  Uint8 alph = (color & 0xff);
  XY_fixed dx, dy;
  XY_fixed m, b;
  int y;

  /* FIXME: clip!  if (XY_clip(&sx1, &sy1, &sx2, &sy2)) */
  {
    dx = fsx2 - fsx1;
    dy = fsy2 - fsy1;

    if (dx != 0)
    {
      m = XY_div(dy, dx);

      b = fsy1 - XY_mult(m, fsx1);

      if (fsx2 >= fsx1)
        dx = XY_FIXED_ONE;
      else
        dx = -XY_FIXED_ONE;

      while (fsx1 != fsx2)
      {
        fsy1 = XY_mult(m, fsx1) + b;
        fsy2 = XY_mult(m, (fsx1 + dx)) + b;

        if (fsy2 > fsy1)
        {
          for (y = fsy1 >> XY_FIXED_SHIFT; y <= fsy2 >> XY_FIXED_SHIFT; y++)
            putpixel(XY_screen, fsx1 >> XY_FIXED_SHIFT, y, sdlcolor, alph);
        }
        else
        {
          for (y = fsy2 >> XY_FIXED_SHIFT; y <= fsy1 >> XY_FIXED_SHIFT; y++)
            putpixel(XY_screen, fsx1 >> XY_FIXED_SHIFT, y, sdlcolor, alph);
        }

        fsx1 = fsx1 + dx;
      }
    }
    else
    {
      if (fsy2 > fsy1)
      {
        for (y = fsy1 >> XY_FIXED_SHIFT; y <= fsy2 >> XY_FIXED_SHIFT; y++)
          putpixel(XY_screen, fsx1 >> XY_FIXED_SHIFT, y, sdlcolor, alph);
      }
      else
      {
        for (y = fsy2 >> XY_FIXED_SHIFT; y <= fsy1 >> XY_FIXED_SHIFT; y++)
          putpixel(XY_screen, fsx1 >> XY_FIXED_SHIFT, y, sdlcolor, alph);
      }
    }
  }
}

void XY_draw_point(XY_fixed x, XY_fixed y, XY_color color)
{
  int sx, sy;
  Uint32 sdlcolor = XY_color_to_sdl_color(color);
  Uint8 alph = (color & 0xff);

  XY_canvas_to_screen(x, y, &sx, &sy);

  putpixel(XY_screen, sx, sy, sdlcolor, alph);
}

XY_fixed XY_cos(int degrees)
{
  while (degrees >= 360) degrees -= 360;
  while (degrees < 0) degrees += 360;

  if (degrees < 90)
    return(XY_trig[degrees]);

  else if (degrees <= 180)
    return(-XY_trig[180 - degrees]);

  else if (degrees <= 270)
    return(-XY_trig[degrees - 180]);

  else
    return(XY_trig[360 - degrees]);
}

XY_fixed XY_screenx_to_canvasx(int sx)
{
  return(sx * XY_canvasw) / XY_screen->w;
}

XY_fixed XY_screeny_to_canvasy(int sy)
{
  return(sy * XY_canvash) / XY_screen->h;
}

void XY_screen_to_canvas(int sx, int sy, XY_fixed * cx, XY_fixed * cy)
{
  *cx = XY_screenx_to_canvasx(sx);
  *cy = XY_screeny_to_canvasy(sy);
}

int XY_canvasx_to_screenx(XY_fixed cx)
{
  return(cx * XY_screen->w) / XY_canvasw;
}

int XY_canvasy_to_screeny(XY_fixed cy)
{
  return(cy * XY_screen->h) / XY_canvash;
}

void XY_canvas_to_screen(XY_fixed cx, XY_fixed cy, int * sx, int * sy)
{
  *sx = XY_canvasx_to_screenx(cx);
  *sy = XY_canvasx_to_screenx(cy);
}

int XY_get_screenw(void)
{
  return(XY_screen->w);
}

int XY_get_screenh(void)
{
  return(XY_screen->h);
}

/* FIXME: Handle alpha value if alpha option is enabled;
   For 'dumb' mode (XY_ALPHA_FAKE), always just blend with background color;
   For 'real' mode (XY_ALPHA_BLEND), blend with the current pixel;
   For 'off' mode (XY_ALPHA_NONE), just use the RBG value, ignore Alpha
     (unless it's 0!)
*/

void putpixel_16(SDL_Surface *surface, int x, int y, Uint32 pixel, Uint8 alph)
{
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * 2;

    if (x < 0 || y < 0 || x >= surface->w || y >= surface->h || alph == 0)
      return;

    *(Uint16 *)p = pixel;
}

void putpixel_24(SDL_Surface *surface, int x, int y, Uint32 pixel, Uint8 alph)
{
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * 3;

    if (x < 0 || y < 0 || x >= surface->w || y >= surface->h || alph == 0)
      return;

    if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
        p[0] = (pixel >> 16) & 0xff;
        p[1] = (pixel >> 8) & 0xff;
        p[2] = pixel & 0xff;
    } else {
        p[0] = pixel & 0xff;
        p[1] = (pixel >> 8) & 0xff;
        p[2] = (pixel >> 16) & 0xff;
    }
}

void putpixel_32(SDL_Surface *surface, int x, int y, Uint32 pixel, Uint8 alph)
{
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * 4;

    if (x < 0 || y < 0 || x >= surface->w || y >= surface->h || alph == 0)
      return;

    *(Uint32 *)p = pixel;
}

void putpixel_fakea_16(SDL_Surface *surface, int x, int y, Uint32 pixel, Uint8 alph)
{
  Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * 2;
  Uint8 antialph;
  Uint8 r1, g1, b1;
  Uint8 r, g, b;

  if (x < 0 || y < 0 || x >= surface->w || y >= surface->h)
    return;

  if (alph == 0)
    return;
  else if (alph < 255)
  {
    antialph = 255 - alph;

    SDL_GetRGB(pixel, surface->format, &r1, &g1, &b1);

    r = ((int) (r1 * alph) + (int) (XY_background_r * antialph)) / 256;
    g = ((int) (g1 * alph) + (int) (XY_background_g * antialph)) / 256;
    b = ((int) (b1 * alph) + (int) (XY_background_b * antialph)) / 256;

    pixel = SDL_MapRGB(surface->format, r, g, b);
  }

  *(Uint16 *)p = pixel;
}

void putpixel_fakea_24(SDL_Surface *surface, int x, int y, Uint32 pixel, Uint8 alph)
{
  Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * 3;
  Uint8 antialph;
  Uint8 r1, g1, b1;
  Uint8 r, g, b;

  if (x < 0 || y < 0 || x >= surface->w || y >= surface->h)
    return;

  if (alph == 0)
    return;
  else if (alph < 255)
  {
    antialph = 255 - alph;

    SDL_GetRGB(pixel, surface->format, &r1, &g1, &b1);

    r = ((int) (r1 * alph) + (int) (XY_background_r * antialph)) / 256;
    g = ((int) (g1 * alph) + (int) (XY_background_g * antialph)) / 256;
    b = ((int) (b1 * alph) + (int) (XY_background_b * antialph)) / 256;

    pixel = SDL_MapRGB(surface->format, r, g, b);
  }

  if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
    p[0] = (pixel >> 16) & 0xff;
    p[1] = (pixel >> 8) & 0xff;
    p[2] = pixel & 0xff;
  } else {
    p[0] = pixel & 0xff;
    p[1] = (pixel >> 8) & 0xff;
    p[2] = (pixel >> 16) & 0xff;
  }
}

void putpixel_fakea_32(SDL_Surface *surface, int x, int y, Uint32 pixel, Uint8 alph)
{
  Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * 4;
  Uint8 antialph;
  Uint8 r1, g1, b1;
  Uint8 r, g, b;

  if (x < 0 || y < 0 || x >= surface->w || y >= surface->h)
    return;

  if (alph == 0)
    return;
  else if (alph < 255)
  {
    antialph = 255 - alph;

    SDL_GetRGB(pixel, surface->format, &r1, &g1, &b1);

    r = ((int) (r1 * alph) + (int) (XY_background_r * antialph)) / 256;
    g = ((int) (g1 * alph) + (int) (XY_background_g * antialph)) / 256;
    b = ((int) (b1 * alph) + (int) (XY_background_b * antialph)) / 256;

    pixel = SDL_MapRGB(surface->format, r, g, b);
  }

  *(Uint32 *)p = pixel;
}

void putpixel_reala_16(SDL_Surface *surface, int x, int y, Uint32 pixel, Uint8 alph)
{
  Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * 2;
  Uint8 antialph;
  Uint8 r1, g1, b1, r2, g2, b2;
  Uint32 oldpixel;
  Uint8 r, g, b;

  if (x < 0 || y < 0 || x >= surface->w || y >= surface->h)
    return;

  if (alph == 0)
    return;
  else if (alph < 255)
  {
    antialph = 255 - alph;

    oldpixel = *(Uint16 *)p;

    SDL_GetRGB(pixel, surface->format, &r1, &g1, &b1);
    SDL_GetRGB(oldpixel, surface->format, &r2, &g2, &b2);

    r = ((int) (r1 * alph) + (int) (r2 * antialph)) / 256;
    g = ((int) (g1 * alph) + (int) (g2 * antialph)) / 256;
    b = ((int) (b1 * alph) + (int) (b2 * antialph)) / 256;

    pixel = SDL_MapRGB(surface->format, r, g, b);
  }

  *(Uint16 *)p = pixel;
}

void putpixel_reala_24(SDL_Surface *surface, int x, int y, Uint32 pixel, Uint8 alph)
{
  Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * 3;
  Uint8 antialph;
  Uint8 r1, g1, b1, r2, g2, b2;
  Uint32 oldpixel;
  Uint8 r, g, b;

  if (x < 0 || y < 0 || x >= surface->w || y >= surface->h)
    return;

  if (alph == 0)
    return;
  else if (alph < 255)
  {
    antialph = 255 - alph;

    if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
      oldpixel = (p[0] << 16) | (p[1] << 8) | p[2];
    } else {
      oldpixel = (p[2] << 16) | (p[1] << 8) | p[0];
    }

    SDL_GetRGB(pixel, surface->format, &r1, &g1, &b1);
    SDL_GetRGB(oldpixel, surface->format, &r2, &g2, &b2);

    r = ((int) (r1 * alph) + (int) (r2 * antialph)) / 256;
    g = ((int) (g1 * alph) + (int) (g2 * antialph)) / 256;
    b = ((int) (b1 * alph) + (int) (b2 * antialph)) / 256;

    pixel = SDL_MapRGB(surface->format, r, g, b);
  }

  if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
    p[0] = (pixel >> 16) & 0xff;
    p[1] = (pixel >> 8) & 0xff;
    p[2] = pixel & 0xff;
  } else {
    p[0] = pixel & 0xff;
    p[1] = (pixel >> 8) & 0xff;
    p[2] = (pixel >> 16) & 0xff;
  }
}

void putpixel_reala_32(SDL_Surface *surface, int x, int y, Uint32 pixel, Uint8 alph)
{
  Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * 4;
  Uint8 antialph;
  Uint8 r1, g1, b1, r2, g2, b2;
  Uint8 r, g, b;
  Uint32 oldpixel;

  if (x < 0 || y < 0 || x >= surface->w || y >= surface->h)
    return;

  if (alph == 0)
    return;
  else if (alph < 255)
  {
    antialph = 255 - alph;

    oldpixel = *(Uint32 *)p;

    SDL_GetRGB(pixel, surface->format, &r1, &g1, &b1);
    SDL_GetRGB(oldpixel, surface->format, &r2, &g2, &b2);

    r = ((int) (r1 * alph) + (int) (r2 * antialph)) / 256;
    g = ((int) (g1 * alph) + (int) (g2 * antialph)) / 256;
    b = ((int) (b1 * alph) + (int) (b2 * antialph)) / 256;

    pixel = SDL_MapRGB(surface->format, r, g, b);
  }

  *(Uint32 *)p = pixel;
}

/* Get a pixel: */
Uint32 getpixel_16(SDL_Surface * surface, int x, int y)
{
  Uint8 *p;

  /* get the X/Y values within the bounds of this surface */
  if ((unsigned) x > (unsigned) surface->w - 1u)
    x = (x < 0) ? 0 : surface->w - 1;
  if ((unsigned) y > (unsigned) surface->h - 1u)
    y = (y < 0) ? 0 : surface->h - 1;

  /* Set a pointer to the exact location in memory of the pixel
     in question: */

  p = (Uint8 *) (((Uint8 *) surface->pixels) +	/* Start at top of RAM */
		 (y * surface->pitch) +	/* Go down Y lines */
		 (x * 2));	/* Go in X pixels */


  /* Return the correctly-sized piece of data containing the
   * pixel's value (an 8-bit palette value, or a 16-, 24- or 32-bit
   * RGB value) */

  return (*(Uint16 *) p);
}

/* Get a pixel: */
Uint32 getpixel_24(SDL_Surface * surface, int x, int y)
{
  Uint8 *p;
  Uint32 pixel;

  /* get the X/Y values within the bounds of this surface */
  if ((unsigned) x > (unsigned) surface->w - 1u)
    x = (x < 0) ? 0 : surface->w - 1;
  if ((unsigned) y > (unsigned) surface->h - 1u)
    y = (y < 0) ? 0 : surface->h - 1;

  /* Set a pointer to the exact location in memory of the pixel
     in question: */

  p = (Uint8 *) (((Uint8 *) surface->pixels) +	/* Start at top of RAM */
		 (y * surface->pitch) +	/* Go down Y lines */
		 (x * 3));	/* Go in X pixels */


  /* Return the correctly-sized piece of data containing the
   * pixel's value (an 8-bit palette value, or a 16-, 24- or 32-bit
   * RGB value) */

  /* Depending on the byte-order, it could be stored RGB or BGR! */

  if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
    pixel = p[0] << 16 | p[1] << 8 | p[2];
  else
    pixel = p[0] | p[1] << 8 | p[2] << 16;

  return pixel;
}

/* Get a pixel: */
Uint32 getpixel_32(SDL_Surface * surface, int x, int y)
{
  Uint8 *p;

  /* get the X/Y values within the bounds of this surface */
  if ((unsigned) x > (unsigned) surface->w - 1u)
    x = (x < 0) ? 0 : surface->w - 1;
  if ((unsigned) y > (unsigned) surface->h - 1u)
    y = (y < 0) ? 0 : surface->h - 1;

  /* Set a pointer to the exact location in memory of the pixel
     in question: */

  p = (Uint8 *) (((Uint8 *) surface->pixels) +	/* Start at top of RAM */
		 (y * surface->pitch) +	/* Go down Y lines */
		 (x * 4));	/* Go in X pixels */


  /* Return the correctly-sized piece of data containing the
   * pixel's value (an 8-bit palette value, or a 16-, 24- or 32-bit
   * RGB value) */

  return *(Uint32 *) p;		// 32-bit display
}

/* FIXME: Implement Xiaolin Wu's antialiased line algorithm
   ( http://en.wikipedia.org/wiki/Xiaolin_Wu's_line_algorithm ) */

