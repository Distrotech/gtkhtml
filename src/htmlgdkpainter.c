/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This file is part of the GtkHTML library.

   Copyright (C) 2000 Helix Code, Inc.
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <string.h>
#include <stdlib.h>
#include <libart_lgpl/art_rect.h>
#include <gdk/gdkx.h>
#include <unicode.h>
#include <gal/widgets/e-font.h>

#include "htmlentity.h"
#include "htmlgdkpainter.h"


static HTMLPainterClass *parent_class = NULL;


/* GtkObject methods.  */

static void
finalize (GtkObject *object)
{
	HTMLGdkPainter *painter;

	painter = HTML_GDK_PAINTER (object);

	if (painter->gc != NULL)
		gdk_gc_destroy (painter->gc);

	if (painter->pixmap != NULL)
		gdk_pixmap_unref (painter->pixmap);

	(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static gint
get_size (gchar *font_name)
{
    gchar *s, *end;
    gint n;

    /* Search paramether */
    for (s=font_name, n=7; n; n--,s++)
	    s = strchr (s,'-');

    if (s && *s != 0) {
	    end = strchr (s, '-');
	    if (end) {
		    *end = 0;
		    n = atoi (s);
		    *end = '-';
		    return n;
	    } else
		    return 0;
    } else
	    return 0;
}

static gboolean
find_font (gchar *font_name, gdouble req_size, gint *font_size)
{
	gint n;
	gdouble size, smaller, bigger;
	gchar **list;
	gboolean rv = FALSE;

	/* list available sizes */
	smaller = bigger = size = .0;
	list = XListFonts (GDK_DISPLAY (), font_name, 0xffff, &n);
	/* look for right one */
	while (n) {
		n--;
		size = get_size (list [n]);
		if ((gdouble) size == req_size) {
			*font_size = size;
			rv = TRUE;
			break;
		} else if (size < req_size && size > smaller)
			smaller = size;
		else if (size > req_size && (size < bigger || bigger == .0))
			bigger = size;
	}
	XFreeFontNames (list);

	/* if not found use closest one */
	if (!rv && (bigger || smaller)) {
		if (bigger == .0)
			size = smaller;
		if (smaller == .0)
			size = bigger;
		if (bigger != .0 && smaller != .0)
			size = (bigger - req_size <= req_size - smaller) ? bigger : smaller;
		*font_size = size;
		rv = TRUE;
	}

	return rv;
}

static GdkFont *
get_font_with_enc (const gchar *family, const gchar *weight, const gchar *slant, gdouble size, gchar *enc)
{
	GdkFont *font;
	gchar *font_name;
	gint font_size;

	font      = NULL;
	font_name = g_strdup_printf ("-*-%s-%s-%s-normal-*-*-*-*-*-*-*-%s",
				     family, weight, slant, enc);
	if (find_font (font_name, size, &font_size)) {
		g_free (font_name);
		font_name = g_strdup_printf ("-*-%s-%s-%s-normal-*-%d-*-*-*-*-*-%s",
					     family, weight, slant, font_size, enc);
		font = gdk_font_load (font_name);
	}
	g_free (font_name);

	/* printf ("family: %s enc: %s ==> %p\n", family, enc, font); */

	return font;
}

static GdkFont *
get_font (const gchar *family, const gchar *weight, const gchar *slant, gdouble size)
{
	GdkFont *font;

	/* prefer unicode fonts */
	font = get_font_with_enc (family, weight, slant, size, "iso10646-1");
	return font ? font : get_font_with_enc (family, weight, slant, size, "*-*");
}

static GdkFont *
get_closest_font (const gchar *family, const gchar *weight, const gchar *slant, gdouble size)
{
	GdkFont *font;

	font = get_font (family, weight, slant, size);
	if (!font && *slant == 'i')
		font = get_font (family, weight, "o", size);

	return font;
}

static gpointer
alloc_font (gchar *face, gdouble size, GtkHTMLFontStyle style)
{
	return (face) ? get_closest_font (face, (style & GTK_HTML_FONT_STYLE_BOLD) ? "bold" : "medium",
					  (style & GTK_HTML_FONT_STYLE_ITALIC) ? "i" : "r", size) : gdk_font_load ("fixed");
}

static gpointer
alloc_e_font (gchar *face, gdouble size, GtkHTMLFontStyle style)
{
	GdkFont *font;

	font = alloc_font (face, size, style);
	return font ? e_font_from_gdk_font (font) : NULL;
}

static void
alloc_color (HTMLPainter *painter,
	     GdkColor *color)
{
	HTMLGdkPainter *gdk_painter;
	GdkColormap *colormap;

	gdk_painter = HTML_GDK_PAINTER (painter);
	g_return_if_fail (gdk_painter->window != NULL);

	colormap = gdk_window_get_colormap (gdk_painter->window);

	gdk_colormap_alloc_color (colormap, color, FALSE, TRUE);
}

static void
free_color (HTMLPainter *painter,
	    GdkColor *color)
{
	HTMLGdkPainter *gdk_painter;
	GdkColormap *colormap;

	gdk_painter = HTML_GDK_PAINTER (painter);

	g_return_if_fail (gdk_painter->window != NULL);
	g_return_if_fail (gdk_painter->gc != NULL);

	colormap = gdk_window_get_colormap (gdk_painter->window);
	gdk_colormap_free_colors (colormap, color, 1);
}


static void
begin (HTMLPainter *painter, int x1, int y1, int x2, int y2)
{
	HTMLGdkPainter *gdk_painter;
	GdkVisual *visual;

	gdk_painter = HTML_GDK_PAINTER (painter);
	visual = gdk_window_get_visual (gdk_painter->window);

	if (gdk_painter->double_buffer){
		const int width = x2 - x1 + 1;
		const int height = y2 - y1 + 1;

		g_assert (gdk_painter->pixmap == NULL);
		
		gdk_painter->pixmap = gdk_pixmap_new (gdk_painter->pixmap, width, height, visual->depth);
		gdk_painter->x1 = x1;
		gdk_painter->y1 = y1;
		gdk_painter->x2 = x2;
		gdk_painter->y2 = y2;

		if (gdk_painter->set_background){
			gdk_gc_set_background (gdk_painter->gc, &gdk_painter->background);
			gdk_painter->set_background = FALSE;
		}

		gdk_gc_set_foreground (gdk_painter->gc, &gdk_painter->background);
		gdk_draw_rectangle (gdk_painter->pixmap, gdk_painter->gc,
				    TRUE, 0, 0, width, height);
	} else {
		gdk_painter->pixmap = gdk_painter->window;
		gdk_painter->x1 = 0;
		gdk_painter->y1 = 0;
		gdk_painter->x2 = 0;
		gdk_painter->y2 = 0;
	}
}

static void
end (HTMLPainter *painter)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);
	
	if (! gdk_painter->double_buffer)
		return;

	gdk_draw_pixmap (gdk_painter->window, gdk_painter->gc,
			 gdk_painter->pixmap,
			 0, 0,
			 gdk_painter->x1, gdk_painter->y1,
			 gdk_painter->x2 - gdk_painter->x1,
			 gdk_painter->y2 - gdk_painter->y1);

	gdk_pixmap_unref (gdk_painter->pixmap);
	gdk_painter->pixmap = NULL;
}

static void
clear (HTMLPainter *painter)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);

	if (! gdk_painter->double_buffer){
		gdk_window_clear (gdk_painter->window);
	} else {
		if (gdk_painter->pixmap != NULL)
			gdk_window_clear (gdk_painter->pixmap);
		else
			gdk_painter->do_clear = TRUE;
	}
}


static void
set_clip_rectangle (HTMLPainter *painter,
		    gint x, gint y,
		    gint width, gint height)
{
	HTMLGdkPainter *gdk_painter;
	GdkRectangle rect;

	gdk_painter = HTML_GDK_PAINTER (painter);

	if (width == 0 || height == 0) {
		gdk_gc_set_clip_rectangle (gdk_painter->gc, NULL);
		return;
	}

	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height;
	
	gdk_gc_set_clip_rectangle (gdk_painter->gc, &rect);
}

static void
set_background_color (HTMLPainter *painter,
		      const GdkColor *color)
{
	g_warning ("HTMLGdkPainter::set_background_color() needs to be implemented.");
}

static void
set_pen (HTMLPainter *painter,
	 const GdkColor *color)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);

	/* GdkColor API not const-safe!  */
	gdk_gc_set_foreground (gdk_painter->gc, (GdkColor *) color);
}

static const GdkColor *
get_black (const HTMLPainter *painter)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);
	return &gdk_painter->black;
}


/* HTMLPainter drawing functions.  */

static void
draw_line (HTMLPainter *painter,
	   gint x1, gint y1,
	   gint x2, gint y2)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);

	x1 -= gdk_painter->x1;
	y1 -= gdk_painter->y1;
	x2 -= gdk_painter->x1;
	y2 -= gdk_painter->y1;

	gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc, x1, y1, x2, y2);
}

static void
draw_ellipse (HTMLPainter *painter,
	      gint x, gint y,
	      gint width, gint height)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);

	gdk_draw_arc (gdk_painter->pixmap, gdk_painter->gc, TRUE,
		      x - gdk_painter->x1, y - gdk_painter->y1,
		      width, height,
		      0, 360 * 64);
}

static void
draw_rect (HTMLPainter *painter,
	   gint x, gint y,
	   gint width, gint height)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);

	gdk_draw_rectangle (gdk_painter->pixmap, gdk_painter->gc, FALSE,
			    x - gdk_painter->x1, y - gdk_painter->y1,
			    width, height);
}

static void
draw_panel (HTMLPainter *painter,
	    gint x, gint y,
	    gint width, gint height,
	    GtkHTMLEtchStyle inset,
	    gint bordersize)
{
	HTMLGdkPainter *gdk_painter;
	GdkColor *col1 = NULL, *col2 = NULL;
	GdkColor *bg, dark, light;
	gint value, ldelta, ddelta;

#define DARK(c) 	dark. ## c  = MIN (MAX (((gint) bg-> ## c) + (gint) ddelta*(((gdouble) bg-> ## c)/0xffff), 0), 0xffff);
#define LIGHT(c)	light. ## c = MIN (MAX (((gint) bg-> ## c) + (gint) ldelta*(((gdouble) bg-> ## c)/0xffff), 0), 0xffff);
#define THOLD 0x2aaa

	bg = &((html_colorset_get_color (painter->color_set, HTMLBgColor))->color);

	value = MAX (bg->red, MAX (bg->green, bg->blue));
	if (value > 0x7fff) {
		ldelta = -THOLD;
		ddelta = -3*THOLD;
	} else {
		ldelta = 3*THOLD;
		ddelta = THOLD;
	}

	DARK(red);
	DARK(green);
	DARK(blue);
	LIGHT(red);
	LIGHT(green);
	LIGHT(blue);

	alloc_color (painter, &dark);
	alloc_color (painter, &light);

	gdk_painter = HTML_GDK_PAINTER (painter);

	switch (inset) {
	case GTK_HTML_ETCH_NONE:
		/* use the current pen color */
		col1 = NULL;
		col2 = NULL;
		break;
	case GTK_HTML_ETCH_OUT:
		col1 = &light;
		col2 = &dark;
		break;
	default:
	case GTK_HTML_ETCH_IN:
		col1 = &dark;
		col2 = &light;
		break;
	}
	
	x -= gdk_painter->x1;
	y -= gdk_painter->y1;
	
	while (bordersize > 0) {
		if (col2) {
			gdk_gc_set_foreground (gdk_painter->gc, col2);
		}

		gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc,
			       x + width - 1, y, x + width - 1, y + height - 1);
		gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc,
			       x + 1, y + height - 1, x + width - 1, y + height - 1);
		if (col1) {
			gdk_gc_set_foreground (gdk_painter->gc, col1);
		}

		gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc,
			       x, y, x + width - 2, y);
		gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc,
			       x, y, x, y + height - 1);
		bordersize--;
		x++;
		y++;
		width-=2;
		height-=2;
	}

	free_color (painter, &dark);
	free_color (painter, &light);
}

static void
draw_background (HTMLPainter *painter,
		 GdkColor *color,
		 GdkPixbuf *pixbuf,
		 gint x, gint y, 
		 gint width, gint height,
		 gint tile_x, gint tile_y)
{
	HTMLGdkPainter *gdk_painter;
	gint pw;
	gint ph;
	gint tile_width, tile_height;
	gint w, h;
	ArtIRect expose, paint, clip;

	gdk_painter = HTML_GDK_PAINTER (painter);

	expose.x0 = x;
	expose.y0 = y;
	expose.x1 = x + width;
	expose.y1 = y + height;

	clip.x0 = gdk_painter->x1;
	clip.x1 = gdk_painter->x2;
	clip.y0 = gdk_painter->y1;
	clip.y1 = gdk_painter->y2;
	clip.x0 = gdk_painter->x1;

	art_irect_intersect (&paint, &clip, &expose);
	if (art_irect_empty (&paint))
	    return;

	width = paint.x1 - paint.x0;
	height = paint.y1 - paint.y0;	
	
	tile_x += paint.x0 - x;
	tile_y += paint.y0 - y;
	
	x = paint.x0;
	y = paint.y0;

	if (!color && !pixbuf)
		return;

	if (color && !pixbuf) {
		gdk_gc_set_foreground (gdk_painter->gc, color);
		gdk_draw_rectangle (gdk_painter->pixmap, gdk_painter->gc,
				    TRUE, x - gdk_painter->x1, y - gdk_painter->y1,
				    width, height);	
		
	}

	if (!pixbuf)
		return;

	pw = gdk_pixbuf_get_width (pixbuf);
	ph = gdk_pixbuf_get_height (pixbuf);

	tile_width = (tile_x % pw) + width;
	tile_height = (tile_y % ph) + height;

	/* do tiling */
	if (tile_width > pw || tile_height > ph) {
		GdkPixmap *pixmap = NULL;
		gint cw, ch, cx, cy;
		gint dw, dh;
		GdkGC *gc;
		GdkBitmap *bitmap = NULL;
		
		dw = MIN (pw, tile_width);
		dh = MIN (ph, tile_height);

		gc = gdk_gc_new (gdk_painter->window);

  		if (color || !gdk_pixbuf_get_has_alpha (pixbuf)) {
			pixmap = gdk_pixmap_new (gdk_painter->window, dw, dh, -1);		
			
			if (color) {
				gdk_gc_set_foreground (gc, color);
				gdk_draw_rectangle (pixmap, gc,
						    TRUE, 0, 0,
						    dw, dh);
			}	

			gdk_pixbuf_render_to_drawable_alpha (pixbuf, pixmap,
						     0, 0,
						     0, 0, 
						     dw, dh,
						     GDK_PIXBUF_ALPHA_BILEVEL,
						     128,
						     GDK_RGB_DITHER_NORMAL,
						     x, y);
			
			gdk_gc_set_tile (gc, pixmap);
			gdk_gc_set_fill (gc, GDK_TILED);
			gdk_gc_set_ts_origin (gc, 
					      x - (tile_x % pw) - gdk_painter->x1,  
					      y - (tile_y % ph) - gdk_painter->y1);

			gdk_draw_rectangle (gdk_painter->pixmap, gc, TRUE,
					    x - gdk_painter->x1, y - gdk_painter->y1, 
					    width, height);
			
			gdk_pixmap_unref (pixmap);			
			gdk_gc_unref (gc);			
		} else {
			int incr_x = 0;
			int incr_y = 0;

			/* Right now we only support GDK_PIXBUF_ALPHA_BILEVEL, so we
			 * unconditionally create the clipping mask.
			 */
			bitmap = gdk_pixmap_new (NULL, dw, dh, 1);
			
			gdk_pixbuf_render_threshold_alpha (pixbuf, bitmap,
							   0, 0,
							   0, 0,
							   dw, dh,
							   128);
			
			gdk_gc_set_clip_mask (gc, bitmap);
			
			pixmap = gdk_pixmap_new (gdk_painter->window, dw, dh, -1);		
			gdk_pixbuf_render_to_drawable (pixbuf, pixmap, gc,
						       0, 0,
						       0, 0, 
						       dw, dh,
						       GDK_RGB_DITHER_NORMAL,
						       x, y);
			
			cy = y;
			ch = height;
			h = tile_y % ph;
			while (ch > 0) {
				incr_y = dh - h;

				cx = x;
				cw = width;
				w = tile_x % pw;
				while (cw > 0) {
					incr_x = dw - w;

					gdk_gc_set_clip_origin (gc, 
								cx - w - gdk_painter->x1,
								cy - h - gdk_painter->y1);
					
					
					gdk_draw_pixmap (gdk_painter->pixmap, gc, pixmap,
							 w, h, cx - gdk_painter->x1, cy - gdk_painter->y1,
							 (cw >= incr_x) ? incr_x : cw,
							 (ch >= incr_y) ? incr_y : ch);
					cw -= incr_x;
					cx += incr_x;
					w = 0;
				}
				ch -= incr_y;
				cy += incr_y;
				h = 0;
			}
			gdk_pixmap_unref (pixmap);			
			gdk_bitmap_unref (bitmap);
			gdk_gc_unref (gc);
		}
	} else {
		if (color && gdk_pixbuf_get_has_alpha (pixbuf)) {
			gdk_gc_set_foreground (gdk_painter->gc, color);
			gdk_draw_rectangle (gdk_painter->pixmap, gdk_painter->gc, TRUE,
					    x - gdk_painter->x1, y - gdk_painter->y1,
					    width, height);	
		}
		
		gdk_pixbuf_render_to_drawable_alpha (pixbuf, gdk_painter->pixmap,
						     tile_x % pw, tile_y % ph,
						     x - gdk_painter->x1, y - gdk_painter->y1, 
						     width, height,
						     GDK_PIXBUF_ALPHA_BILEVEL,
						     128,
						     GDK_RGB_DITHER_NORMAL,
						     x, y);
	}
}

static GdkPixbuf *
create_temporary_pixbuf (GdkPixbuf *src,
			 gint clip_width, gint clip_height)
{
	GdkPixbuf *pixbuf;
	gboolean has_alpha;
	guint bits_per_sample;

	has_alpha = gdk_pixbuf_get_has_alpha (src);
	bits_per_sample = gdk_pixbuf_get_bits_per_sample (src);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, has_alpha, bits_per_sample, clip_width, clip_height);

	return pixbuf;
}

static void
draw_pixmap (HTMLPainter *painter,
	     GdkPixbuf *pixbuf,
	     gint x, gint y,
	     gint scale_width, gint scale_height,
	     const GdkColor *color)
{
	ArtIRect clip, image, paint;
	HTMLGdkPainter *gdk_painter;
	GdkPixbuf *tmp_pixbuf;
	guint n_channels;
	gint orig_width;
	gint orig_height;
	gint paint_width;
	gint paint_height;

	gdk_painter = HTML_GDK_PAINTER (painter);

	orig_width = gdk_pixbuf_get_width (pixbuf);
	orig_height = gdk_pixbuf_get_height (pixbuf);

	if (scale_width < 0)
		scale_width = orig_width;
	if (scale_height < 0)
		scale_height = orig_height;

	image.x0 = x;
	image.y0 = y;
	image.x1 = x + scale_width;
	image.y1 = y + scale_height;

	clip.x0 = gdk_painter->x1;
	clip.x1 = gdk_painter->x2;
	clip.y0 = gdk_painter->y1;
	clip.y1 = gdk_painter->y2;

	art_irect_intersect (&paint, &clip, &image);
	if (art_irect_empty (&paint))
	    return;

	paint_width = paint.x1 - paint.x0;
	paint_height = paint.y1 - paint.y0;


	if (scale_width == orig_width && scale_height == orig_height && color == NULL && (!gdk_painter->alpha)) {
		gdk_pixbuf_render_to_drawable_alpha (pixbuf, gdk_painter->pixmap,
						     paint.x0 - image.x0,
						     paint.y0 - image.y0,
						     paint.x0 - clip.x0,
						     paint.y0 - clip.y0,
						     paint_width,
						     paint_height,
						     GDK_PIXBUF_ALPHA_BILEVEL,
						     128,
						     GDK_RGB_DITHER_NORMAL,
						     x, y);
		return;
	}

	if (gdk_pixbuf_get_has_alpha (pixbuf) && gdk_painter->alpha) {
	    tmp_pixbuf = gdk_pixbuf_get_from_drawable(NULL,
						      gdk_painter->pixmap,
						      gdk_window_get_colormap (gdk_painter->window),
						      paint.x0 - clip.x0, 
						      paint.y0 - clip.y0,
						      0, 0, paint_width, paint_height);
	} else {
		tmp_pixbuf = create_temporary_pixbuf (pixbuf,
						      paint_width,
						      paint_height);
	}

	if (tmp_pixbuf == NULL)
		return;

	gdk_pixbuf_composite (pixbuf, tmp_pixbuf,
			      0,
			      0,
			      paint_width, paint_height,
			      (double)-(paint.x0 - image.x0), 
			      (double)-(paint.y0 - image.y0),
			      (gdouble) scale_width/ (gdouble) orig_width,
			      (gdouble) scale_height/ (gdouble) orig_height,
			      GDK_INTERP_BILINEAR, 255);

	if (color != NULL) {
		guchar *p, *q;
		guint i, j;

		n_channels = gdk_pixbuf_get_n_channels (tmp_pixbuf);
		p = q = gdk_pixbuf_get_pixels (tmp_pixbuf);
		for (i = 0; i < paint_height; i++) {
			p = q;

			for (j = 0; j < paint_width; j++) {
				gint r, g, b, a;

				if (n_channels > 3)
					a = p[3];
				else
					a = 0xff;

				r = (a * p[0] + color->red) >> 9;
				g = (a * p[1] + color->green) >> 9;
				b = (a * p[2] + color->blue) >> 9;

				p[0] = r;
				p[1] = g;
				p[2] = b;

				if (n_channels > 3)
					p[3] = 0xff;

				p += n_channels;
			}

			q += gdk_pixbuf_get_rowstride (tmp_pixbuf);
		}
	}

	gdk_pixbuf_render_to_drawable_alpha (tmp_pixbuf, gdk_painter->pixmap,
					     0,
					     0,
					     paint.x0 - clip.x0,
					     paint.y0 - clip.y0,
					     paint_width,
					     paint_height,
					     GDK_PIXBUF_ALPHA_BILEVEL,
					     128,
					     GDK_RGB_DITHER_NORMAL,
					     x, y);
	gdk_pixbuf_unref (tmp_pixbuf);
}

static void
fill_rect (HTMLPainter *painter,
	   gint x, gint y,
	   gint width, gint height)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);

	gdk_draw_rectangle (gdk_painter->pixmap, gdk_painter->gc,
			    TRUE, x - gdk_painter->x1, y - gdk_painter->y1,
			    width, height);
}

/* static gint
utf8_gdk_text_width (GdkFont *font, const gchar *text, gint len)
{
	const gchar *p;
	gint width, bytes;

	if (!text || !font) return 0;

	width = 0;
	bytes = unicode_offset_to_index (text, len);
	for (p = text; p && *p && p - text < bytes; p = unicode_next_utf8 (p)) {
		unicode_char_t unival;
		guchar c;
		unicode_get_utf8 (p, &unival);
		if (unival < 0) unival = '_';
		if (unival > 255) unival = '_';
		if (unival == 0xa0) unival = ' ';
		c = unival;
		width += gdk_text_width (font, &c, 1);
	}

	return width;
}

static void
utf8_gdk_draw_text (GdkFont *font,
		    GdkDrawable *drawable,
		    GdkGC *gc,
		    gint x, gint y,
		    const gchar *text, gint len)
{
	const gchar *p;
	guchar *b;
	gint off, bytes;

	if (!text || !font) return;

	bytes = unicode_offset_to_index (text, len);
	b = alloca (bytes);
	off = 0;

	for (p = text; p && *p && p - text < bytes; p = unicode_next_utf8 (p)) {
		unicode_char_t unival;
		unicode_get_utf8 (p, &unival);
		if (unival < 0) unival = '_';
		if (unival > 255) unival = '_';
		if (unival == 0xa0) unival = ' ';
		b [off++] = unival;
	}

	gdk_draw_text (drawable, font, gc, x, y, b, len);
} */

static EFontStyle
e_style (GtkHTMLFontStyle style)
{
	EFontStyle rv = E_FONT_PLAIN;

	if (style & GTK_HTML_FONT_STYLE_BOLD)   rv |= E_FONT_BOLD;
	if (style & GTK_HTML_FONT_STYLE_ITALIC) rv |= E_FONT_ITALIC;

	return rv;
}

static void
draw_spell_error (HTMLPainter *painter,
		  gint x, gint y,
		  const gchar *text,
		  guint off, gint len)
{
	HTMLGdkPainter *gdk_painter;
	EFont *e_font;
	GdkGCValues values;
	guint x_off, width;
	gchar dash [2];

	gdk_painter = HTML_GDK_PAINTER (painter);

	x -= gdk_painter->x1;
	y -= gdk_painter->y1;

	e_font = html_painter_get_font  (painter, painter->font_face, painter->font_style);
	x_off  = e_font_utf8_text_width (e_font, e_style (painter->font_style), text,
					 unicode_offset_to_index (text, off)) + x;
	width  = e_font_utf8_text_width (e_font, e_style (painter->font_style), text + unicode_offset_to_index (text, off),
					 unicode_offset_to_index (text + unicode_offset_to_index (text, off), len)) + x_off;

	gdk_gc_get_values (gdk_painter->gc, &values);
	gdk_gc_set_fill (gdk_painter->gc, GDK_OPAQUE_STIPPLED);
	dash [0] = 2;
	dash [1] = 2;
	gdk_gc_set_line_attributes (gdk_painter->gc, 1, GDK_LINE_ON_OFF_DASH, values.cap_style, values.join_style);
	gdk_gc_set_dashes (gdk_painter->gc, 2, dash, 2);
	gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc,
		       x_off, y, 
		       width, y);
	gdk_gc_set_dashes (gdk_painter->gc, 0, dash, 2);
	gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc,
		       x_off, y + 1, 
		       width, y + 1);
	gdk_gc_set_line_attributes (gdk_painter->gc, values.line_width,
				    values.line_style, values.cap_style, values.join_style);
}

static void
draw_text (HTMLPainter *painter,
	   gint x, gint y,
	   const gchar *text,
	   gint len)
{
	HTMLGdkPainter *gdk_painter;
	EFont *e_font;

	gdk_painter = HTML_GDK_PAINTER (painter);

	if (len == -1)
		len = unicode_strlen (text, -1);

	x -= gdk_painter->x1;
	y -= gdk_painter->y1;

	e_font = html_painter_get_font (painter, painter->font_face, painter->font_style);
	e_font_draw_utf8_text (gdk_painter->pixmap, e_font, e_style (painter->font_style), gdk_painter->gc,
			       x, y, text, unicode_offset_to_index (text, len));

	if (painter->font_style & (GTK_HTML_FONT_STYLE_UNDERLINE
				   | GTK_HTML_FONT_STYLE_STRIKEOUT)) {
		guint width;

		width = e_font_utf8_text_width (e_font, e_style (painter->font_style),
						text, unicode_offset_to_index (text, len));

		if (painter->font_style & GTK_HTML_FONT_STYLE_UNDERLINE)
			gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc, 
				       x, y + 1, 
				       x + width, y + 1);

		if (painter->font_style & GTK_HTML_FONT_STYLE_STRIKEOUT)
			gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc, 
				       x, y - e_font_ascent (e_font) / 2, 
				       x + width, y - e_font_ascent (e_font) / 2);
	}
}

static void
draw_shade_line (HTMLPainter *painter,
		 gint x, gint y,
		 gint width)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (painter);
	
	x -= gdk_painter->x1;
	y -= gdk_painter->y1;
	
	gdk_gc_set_foreground (gdk_painter->gc, &gdk_painter->dark);
	gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc, x, y, x+width, y);
	gdk_gc_set_foreground (gdk_painter->gc, &gdk_painter->light);
	gdk_draw_line (gdk_painter->pixmap, gdk_painter->gc, x, y + 1, x + width, y + 1);
}

static guint
calc_ascent (HTMLPainter *painter, GtkHTMLFontStyle style, HTMLFontFace *face)
{
	HTMLGdkPainter *gdk_painter;
	EFont *e_font;

	gdk_painter = HTML_GDK_PAINTER (painter);

	e_font = html_painter_get_font (painter, face, style);
	if (e_font == NULL)
		return 0;

	return e_font_ascent (e_font);
}

static guint
calc_descent (HTMLPainter *painter, GtkHTMLFontStyle style, HTMLFontFace *face)
{
	HTMLGdkPainter *gdk_painter;
	EFont *e_font;

	gdk_painter = HTML_GDK_PAINTER (painter);

	e_font = html_painter_get_font (painter, face, style);
	if (e_font == NULL)
		return 0;

	return e_font_descent (e_font);
}

static guint
calc_text_width (HTMLPainter *painter,
		 const gchar *text,
		 guint len,
		 GtkHTMLFontStyle style,
		 HTMLFontFace *face)
{
	HTMLGdkPainter *gdk_painter;
	EFont *e_font;
	gint width;

	gdk_painter = HTML_GDK_PAINTER (painter);
	e_font = html_painter_get_font (painter, face, style);

	width = e_font_utf8_text_width (e_font, e_style (painter->font_style),
					text, unicode_offset_to_index (text, len));

	return width;
}

static guint
get_pixel_size (HTMLPainter *painter)
{
	return 1;
}


static void
init_color (GdkColor *color, gushort red, gushort green, gushort blue)
{
	color->pixel = 0;
	color->red = red;
	color->green = green;
	color->blue = blue;
}

static void
init (GtkObject *object)
{
	HTMLGdkPainter *gdk_painter;

	gdk_painter = HTML_GDK_PAINTER (object);

	gdk_painter->window = NULL;

	gdk_painter->alpha = TRUE;
	gdk_painter->gc = NULL;

	gdk_painter->double_buffer = TRUE;
	gdk_painter->pixmap = NULL;
	gdk_painter->x1 = gdk_painter->y1 = 0;
	gdk_painter->x2 = gdk_painter->y2 = 0;
	gdk_painter->set_background = FALSE;
	gdk_painter->do_clear = FALSE;

	init_color (& gdk_painter->background, 0xffff, 0xffff, 0xffff);
	init_color (& gdk_painter->dark, 0, 0, 0);
	init_color (& gdk_painter->light, 0, 0, 0);
}

static void
class_init (GtkObjectClass *object_class)
{
	HTMLPainterClass *painter_class;

	painter_class = HTML_PAINTER_CLASS (object_class);

	object_class->finalize = finalize;

	painter_class->begin = begin;
	painter_class->end = end;
	painter_class->alloc_font = alloc_e_font;
	painter_class->ref_font   = (HTMLFontManagerRefFont)   e_font_ref;
	painter_class->unref_font = (HTMLFontManagerUnrefFont) e_font_unref;
	painter_class->alloc_color = alloc_color;
	painter_class->free_color = free_color;
	painter_class->calc_ascent = calc_ascent;
	painter_class->calc_descent = calc_descent;
	painter_class->calc_text_width = calc_text_width;
	painter_class->set_pen = set_pen;
	painter_class->get_black = get_black;
	painter_class->draw_line = draw_line;
	painter_class->draw_rect = draw_rect;
	painter_class->draw_text = draw_text;
	painter_class->draw_spell_error = draw_spell_error;
	painter_class->fill_rect = fill_rect;
	painter_class->draw_pixmap = draw_pixmap;
	painter_class->draw_ellipse = draw_ellipse;
	painter_class->clear = clear;
	painter_class->set_background_color = set_background_color;
	painter_class->draw_shade_line = draw_shade_line;
	painter_class->draw_panel = draw_panel;
	painter_class->set_clip_rectangle = set_clip_rectangle;
	painter_class->draw_background = draw_background;
	painter_class->get_pixel_size = get_pixel_size;

	parent_class = gtk_type_class (html_painter_get_type ());
}

GtkType
html_gdk_painter_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info = {
			"HTMLGdkPainter",
			sizeof (HTMLGdkPainter),
			sizeof (HTMLGdkPainterClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (HTML_TYPE_PAINTER, &info);
	}

	return type;
}


HTMLPainter *
html_gdk_painter_new (gboolean double_buffer)
{
	HTMLGdkPainter *new;

	new = gtk_type_new (html_gdk_painter_get_type ());

	new->double_buffer = double_buffer;

	return HTML_PAINTER (new);
}

void
html_gdk_painter_realize (HTMLGdkPainter *gdk_painter,
			  GdkWindow *window)
{
	GdkColormap *colormap;

	g_return_if_fail (gdk_painter != NULL);
	g_return_if_fail (window != NULL);
	
	gdk_painter->gc = gdk_gc_new (window);
	gdk_painter->window = window;

	colormap = gdk_window_get_colormap (window);

	gdk_painter->light.red = 0xffff;
	gdk_painter->light.green = 0xffff;
	gdk_painter->light.blue = 0xffff;
	gdk_colormap_alloc_color (colormap, &gdk_painter->light, TRUE, TRUE);

	gdk_painter->dark.red = 0x7fff;
	gdk_painter->dark.green = 0x7fff;
	gdk_painter->dark.blue = 0x7fff;
	gdk_colormap_alloc_color (colormap, &gdk_painter->dark, TRUE, TRUE);

	gdk_painter->black.red = 0x0000;
	gdk_painter->black.green = 0x0000;
	gdk_painter->black.blue = 0x0000;
	gdk_colormap_alloc_color (colormap, &gdk_painter->black, TRUE, TRUE);
}

void
html_gdk_painter_unrealize (HTMLGdkPainter *painter)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_GDK_PAINTER (painter));
	
	gdk_gc_unref (painter->gc);
	painter->gc = NULL;

	painter->window = NULL;
}

gboolean
html_gdk_painter_realized (HTMLGdkPainter *painter)
{
	g_return_val_if_fail (painter != NULL, FALSE);
	g_return_val_if_fail (HTML_IS_GDK_PAINTER (painter), FALSE);

	if (painter->window == NULL)
		return FALSE;
	else
		return TRUE;
}
