/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This file is part of the GtkHTML library.

   Copyright (C) 1997 Martin Jones (mjones@kde.org)
   Copyright (C) 1997 Torben Weis (weis@kde.org)
   Copyright (C) 1999, 2000 Helix Code, Inc.
   
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

#include "htmlpainter.h"


/* Convenience macro to extract the HTMLPainterClass from a GTK+ object.  */
#define HP_CLASS(obj)					\
	HTML_PAINTER_CLASS (GTK_OBJECT (obj)->klass)

/* Our parent class.  */
static GtkObjectClass *parent_class = NULL;


/* GtkObject methods.  */

static void
finalize (GtkObject *object)
{
	HTMLPainter *painter;

	painter = HTML_PAINTER (object);
	html_font_manager_finalize (&painter->font_manager);

	/* FIXME ownership of the color set?  */

	(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}


#define DEFINE_UNIMPLEMENTED(method)						\
	static gint								\
	method##_unimplemented (GtkObject *obj)					\
	{									\
		g_warning ("Class `%s' does not implement `" #method "'\n",	\
			   gtk_type_name (GTK_OBJECT_TYPE (obj)));		\
		return 0;							\
	}

DEFINE_UNIMPLEMENTED (begin);
DEFINE_UNIMPLEMENTED (end);

DEFINE_UNIMPLEMENTED (alloc_font);
DEFINE_UNIMPLEMENTED (  ref_font);
DEFINE_UNIMPLEMENTED (unref_font);

DEFINE_UNIMPLEMENTED (alloc_color);
DEFINE_UNIMPLEMENTED (free_color);

DEFINE_UNIMPLEMENTED (calc_ascent);
DEFINE_UNIMPLEMENTED (calc_descent);
DEFINE_UNIMPLEMENTED (calc_text_width);

DEFINE_UNIMPLEMENTED (set_pen);
DEFINE_UNIMPLEMENTED (get_black);
DEFINE_UNIMPLEMENTED (draw_line);
DEFINE_UNIMPLEMENTED (draw_rect);
DEFINE_UNIMPLEMENTED (draw_text);
#ifdef GTKHTML_HAVE_PSPELL
DEFINE_UNIMPLEMENTED (draw_spell_error);
#endif
DEFINE_UNIMPLEMENTED (fill_rect);
DEFINE_UNIMPLEMENTED (draw_pixmap);
DEFINE_UNIMPLEMENTED (draw_ellipse);
DEFINE_UNIMPLEMENTED (clear);
DEFINE_UNIMPLEMENTED (set_background_color);
DEFINE_UNIMPLEMENTED (draw_shade_line);
DEFINE_UNIMPLEMENTED (draw_panel);

DEFINE_UNIMPLEMENTED (set_clip_rectangle);
DEFINE_UNIMPLEMENTED (draw_background);

DEFINE_UNIMPLEMENTED (get_pixel_size);

static void
init (GtkObject *object, HTMLPainterClass *real_klass)
{
	HTMLPainter *painter;

	painter = HTML_PAINTER (object);
	painter->color_set = html_colorset_new (NULL);

	html_font_manager_init (&painter->font_manager, real_klass->alloc_font, real_klass->ref_font, real_klass->unref_font);
	painter->font_style = GTK_HTML_FONT_STYLE_DEFAULT;
	painter->font_face = NULL;
}

static void
class_init (GtkObjectClass *object_class)
{
	HTMLPainterClass *class;

	class = HTML_PAINTER_CLASS (object_class);

	object_class->finalize = finalize;

	class->begin = (gpointer) begin_unimplemented;
	class->end = (gpointer) end_unimplemented;

	class->alloc_font = (gpointer) alloc_font_unimplemented;
	class->ref_font   = (gpointer)   ref_font_unimplemented;
	class->unref_font = (gpointer) unref_font_unimplemented;

	class->alloc_color = (gpointer) alloc_color_unimplemented;
	class->free_color = (gpointer) free_color_unimplemented;

	class->calc_ascent = (gpointer) calc_ascent_unimplemented;
	class->calc_descent = (gpointer) calc_descent_unimplemented;
	class->calc_text_width = (gpointer) calc_text_width_unimplemented;

	class->set_pen = (gpointer) set_pen_unimplemented;
	class->get_black = (gpointer) get_black_unimplemented;
	class->draw_line = (gpointer) draw_line_unimplemented;
	class->draw_rect = (gpointer) draw_rect_unimplemented;
	class->draw_text = (gpointer) draw_text_unimplemented;
#ifdef GTKHTML_HAVE_PSPELL
	class->draw_spell_error = (gpointer) draw_spell_error_unimplemented;
#endif
	class->fill_rect = (gpointer) fill_rect_unimplemented;
	class->draw_pixmap = (gpointer) draw_pixmap_unimplemented;
	class->draw_ellipse = (gpointer) draw_ellipse_unimplemented;
	class->clear = (gpointer) clear_unimplemented;
	class->set_background_color = (gpointer) set_background_color_unimplemented;
	class->draw_shade_line = (gpointer) draw_shade_line_unimplemented;
	class->draw_panel = (gpointer) draw_panel_unimplemented;

	class->set_clip_rectangle = (gpointer) set_clip_rectangle_unimplemented;
	class->draw_background = (gpointer) draw_background_unimplemented;

	class->get_pixel_size = (gpointer) get_pixel_size_unimplemented;

	parent_class = gtk_type_class (gtk_object_get_type ());
}


GtkType
html_painter_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info = {
			"HTMLPainter",
			sizeof (HTMLPainter),
			sizeof (HTMLPainterClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (GTK_TYPE_OBJECT, &info);
	}

	return type;
}

HTMLPainter *
html_painter_new (void)
{
	return gtk_type_new (html_painter_get_type ());
}


/* Functions to begin/end a painting process.  */

void
html_painter_begin (HTMLPainter *painter,
		    int x1, int y1, int x2, int y2)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));

	(* HP_CLASS (painter)->begin) (painter, x1, y1, x2, y2);
}

void
html_painter_end (HTMLPainter *painter)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));

	(* HP_CLASS (painter)->end) (painter);
}


/* Color control.  */
void
html_painter_alloc_color (HTMLPainter *painter,
			  GdkColor *color)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));
	g_return_if_fail (color != NULL);

	(* HP_CLASS (painter)->alloc_color) (painter, color);
}

void
html_painter_free_color (HTMLPainter *painter,
			 GdkColor *color)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));
	g_return_if_fail (color != NULL);

	(* HP_CLASS (painter)->free_color) (painter, color);
}


/* Font handling.  */

void
html_painter_set_font_style (HTMLPainter *painter,
			     GtkHTMLFontStyle font_style)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));
	g_return_if_fail (font_style != GTK_HTML_FONT_STYLE_DEFAULT);

	painter->font_style = font_style;
}

GtkHTMLFontStyle
html_painter_get_font_style (HTMLPainter *painter)
{
	g_return_val_if_fail (painter != NULL, GTK_HTML_FONT_STYLE_DEFAULT);
	g_return_val_if_fail (HTML_IS_PAINTER (painter), GTK_HTML_FONT_STYLE_DEFAULT);

	return painter->font_style;
}

void
html_painter_set_font_face (HTMLPainter *painter,
			    HTMLFontFace *face)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));

	if (!painter->font_face || !face || strcmp (painter->font_face, face)) {
		if (painter->font_face)
			g_free (painter->font_face);
		painter->font_face = g_strdup (face);
	}
}

gpointer
html_painter_get_font (HTMLPainter *painter, HTMLFontFace *face, GtkHTMLFontStyle style)
{
	return html_font_manager_get_font (&painter->font_manager, face, style);
}

guint
html_painter_calc_ascent (HTMLPainter *painter,
			  GtkHTMLFontStyle font_style,
			  HTMLFontFace *face)
{
	g_return_val_if_fail (painter != NULL, 0);
	g_return_val_if_fail (HTML_IS_PAINTER (painter), 0);
	g_return_val_if_fail (font_style != GTK_HTML_FONT_STYLE_DEFAULT, 0);

	return (* HP_CLASS (painter)->calc_ascent) (painter, font_style, face);
}

guint
html_painter_calc_descent (HTMLPainter *painter,
			   GtkHTMLFontStyle font_style,
			   HTMLFontFace *face)
{
	g_return_val_if_fail (painter != NULL, 0);
	g_return_val_if_fail (HTML_IS_PAINTER (painter), 0);
	g_return_val_if_fail (font_style != GTK_HTML_FONT_STYLE_DEFAULT, 0);

	return (* HP_CLASS (painter)->calc_descent) (painter, font_style, face);
}

guint
html_painter_calc_text_width (HTMLPainter *painter,
			      const gchar *text,
			      guint len,
			      GtkHTMLFontStyle font_style,
			      HTMLFontFace *face)
{
	g_return_val_if_fail (painter != NULL, 0);
	g_return_val_if_fail (HTML_IS_PAINTER (painter), 0);
	g_return_val_if_fail (text != NULL, 0);
	g_return_val_if_fail (font_style != GTK_HTML_FONT_STYLE_DEFAULT, 0);

	return (* HP_CLASS (painter)->calc_text_width) (painter, text, len, font_style, face);
}


/* The actual paint operations.  */

void
html_painter_set_pen (HTMLPainter *painter,
		      const GdkColor *color)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));
	g_return_if_fail (color != NULL);

	(* HP_CLASS (painter)->set_pen) (painter, color);
}

void
html_painter_draw_line (HTMLPainter *painter,
			gint x1, gint y1,
			gint x2, gint y2)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));

	(* HP_CLASS (painter)->draw_line) (painter, x1, y1, x2, y2);
}

void
html_painter_draw_rect (HTMLPainter *painter,
			gint x, gint y,
			gint width, gint height)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));

	(* HP_CLASS (painter)->draw_rect) (painter, x, y, width, height);
}

void
html_painter_draw_text (HTMLPainter *painter,
			gint x, gint y,
			const gchar *text, gint len)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));

	(* HP_CLASS (painter)->draw_text) (painter, x, y, text, len);
}

void
html_painter_fill_rect (HTMLPainter *painter,
			gint x, gint y,
			gint width, gint height)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));

	(* HP_CLASS (painter)->fill_rect) (painter, x, y, width, height);
}

void
html_painter_draw_pixmap (HTMLPainter    *painter,
			  GdkPixbuf *pixbuf,
			  gint x, gint y,
			  gint scale_width, gint scale_height,
			  const GdkColor *color)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));
	g_return_if_fail (pixbuf != NULL);

	(* HP_CLASS (painter)->draw_pixmap) (painter, pixbuf, x, y, scale_width, scale_height, color);
}

void
html_painter_draw_ellipse (HTMLPainter *painter,
			   gint x, gint y,
			   gint width, gint height)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));

	(* HP_CLASS (painter)->draw_ellipse) (painter, x, y, width, height);
}

void
html_painter_clear (HTMLPainter *painter)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));

	(* HP_CLASS (painter)->clear) (painter);
}

void
html_painter_set_background_color (HTMLPainter *painter,
				   const GdkColor *color)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));
	g_return_if_fail (color != NULL);

	(* HP_CLASS (painter)->set_background_color) (painter, color);
}

void
html_painter_draw_shade_line (HTMLPainter *painter,
			      gint x, gint y,
			      gint width)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));

	(* HP_CLASS (painter)->draw_shade_line) (painter, x, y, width);
}

void
html_painter_draw_panel (HTMLPainter *painter,
			 gint x, gint y,
			 gint width, gint height,
			 GtkHTMLEtchStyle inset,
			 gint bordersize)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));

	(* HP_CLASS (painter)->draw_panel) (painter, x, y, width, height, inset, bordersize);
}

/* Passing 0 for width/height means remove clip rectangle */
void
html_painter_set_clip_rectangle (HTMLPainter *painter,
				 gint x, gint y,
				 gint width, gint height)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));

	(* HP_CLASS (painter)->set_clip_rectangle) (painter, x, y, width, height);
}

/* Passing 0 for pix_width / pix_height makes it use the image width */
void
html_painter_draw_background (HTMLPainter *painter,
			      GdkColor *color,
			      GdkPixbuf *pixbuf,
			      gint x, gint y,
			      gint width, gint height,
			      gint tile_x, gint tile_y)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (HTML_IS_PAINTER (painter));

	(* HP_CLASS (painter)->draw_background) (painter, color, pixbuf, x, y, width, height, tile_x, tile_y);
}

guint
html_painter_get_pixel_size (HTMLPainter *painter)
{
	g_return_val_if_fail (painter != NULL, 0);
	g_return_val_if_fail (HTML_IS_PAINTER (painter), 0);
	
	return (* HP_CLASS (painter)->get_pixel_size) (painter);
}

#ifdef GTKHTML_HAVE_PSPELL
void
html_painter_draw_spell_error (HTMLPainter *painter,
			       gint x, gint y,
			       const gchar *text,
			       guint off, gint len)
{
	(* HP_CLASS (painter)->draw_spell_error) (painter, x, y, text, off, len);
}
#endif
