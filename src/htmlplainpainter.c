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

#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <gdk/gdkx.h>
#include <libart_lgpl/art_rect.h>

#include "htmlentity.h"
#include "htmlgdkpainter.h"
#include "htmlplainpainter.h"
#include "htmlcolor.h"
#include "htmlcolorset.h"
#include "htmlengine.h"

static HTMLGdkPainterClass *parent_class = NULL;

static void
draw_panel (HTMLPainter *painter,
	    GdkColor *bg,
	    gint x, gint y,
	    gint width, gint height,
	    GtkHTMLEtchStyle inset,
	    gint bordersize)
{
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

	if (color) {
		gdk_gc_set_foreground (gdk_painter->gc, color);
		gdk_draw_rectangle (gdk_painter->pixmap, gdk_painter->gc,
				    TRUE, x - gdk_painter->x1, y - gdk_painter->y1,
				    width, height);	
		
	}

	return;
}

static void
draw_pixmap (HTMLPainter *painter,
	     GdkPixbuf *pixbuf,
	     gint x, gint y,
	     gint scale_width, gint scale_height,
	     const GdkColor *color)
{
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

static HTMLFont *
alloc_fixed_font (HTMLPainter *painter, gchar *face, gdouble size, gboolean points, GtkHTMLFontStyle style)
{
	return HTML_PAINTER_CLASS (parent_class)->alloc_font (painter, 
							      face ? painter->font_manager.fixed.face : NULL,
							      painter->font_manager.fix_size, painter->font_manager.fix_points,
							      GTK_HTML_FONT_STYLE_DEFAULT); 
}


static void
draw_shade_line (HTMLPainter *painter,
		 gint x, gint y,
		 gint width)
{
}

static void
html_plain_painter_init (GObject *object)
{
}

static void
draw_text (HTMLPainter *painter,
	   gint x, gint y,
	   const gchar *text,
	   gint len)
{
	/* HTMLGdkPainter *gdk_painter;
	EFont *e_font;

	gdk_painter = HTML_GDK_PAINTER (painter);

	if (len == -1)
		len = g_utf8_strlen (text, -1);

	x -= gdk_painter->x1;
	y -= gdk_painter->y1;

	e_font = html_painter_get_font (painter, painter->font_face,
					painter->font_style);

	e_font_draw_utf8_text (gdk_painter->pixmap, e_font, 
			       e_style (painter->font_style), gdk_painter->gc,
			       x, y, text, 
			       g_utf8_offset_to_pointer (text, len) - text);
	*/
}

static void
draw_rect (HTMLPainter *painter,
	   gint x, gint y,
	   gint width, gint height)
{
}

static guint
get_page_width (HTMLPainter *painter, HTMLEngine *e)
{
	return MIN (72 * html_painter_get_space_width (painter, GTK_HTML_FONT_STYLE_SIZE_3, NULL),
		    html_engine_get_view_width (e)) + e->leftBorder + e->rightBorder;
}

static guint
get_page_height (HTMLPainter *painter, HTMLEngine *e)
{
	return html_engine_get_view_height (e) + e->topBorder + e->bottomBorder;
}

static void
html_plain_painter_class_init (GObjectClass *object_class)
{
	HTMLPainterClass *painter_class;

	painter_class = HTML_PAINTER_CLASS (object_class);
	parent_class = g_type_class_ref (HTML_TYPE_GDK_PAINTER);

	painter_class->alloc_font = alloc_fixed_font;
	painter_class->draw_rect = draw_rect;
	painter_class->draw_text = draw_text;
	painter_class->fill_rect = fill_rect;
	painter_class->draw_pixmap = draw_pixmap;
	painter_class->draw_shade_line = draw_shade_line;
	painter_class->draw_panel = draw_panel;
	painter_class->draw_background = draw_background;
	painter_class->get_page_width = get_page_width;
	painter_class->get_page_height = get_page_height;
}

GType
html_plain_painter_get_type (void)
{
	static GType html_plain_painter_type = 0;

	if (html_plain_painter_type == 0) {
		static const GTypeInfo html_plain_painter_info = {
			sizeof (HTMLPlainPainterClass),
			NULL,
			NULL,
			(GClassInitFunc) html_plain_painter_class_init,
			NULL,
			NULL,
			sizeof (HTMLPlainPainter),
			1,
			(GInstanceInitFunc) html_plain_painter_init,
		};
		html_plain_painter_type = g_type_register_static (HTML_TYPE_GDK_PAINTER, "HTMLPlainPainter",
								  &html_plain_painter_info, 0);
	}

	return html_plain_painter_type;
}

HTMLPainter *
html_plain_painter_new (GtkWidget *widget, gboolean double_buffer)
{
	HTMLPlainPainter *new;

	new = g_object_new (HTML_TYPE_PLAIN_PAINTER, NULL);
	HTML_GDK_PAINTER (new)->double_buffer = double_buffer;
	HTML_GDK_PAINTER (new)->pc = gtk_widget_get_pango_context (widget);
	g_object_ref (HTML_GDK_PAINTER (new)->pc);

	return HTML_PAINTER (new);
}
