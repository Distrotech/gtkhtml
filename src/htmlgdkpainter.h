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

#ifndef _HTMLGDKPAINTER_H
#define _HTMLGDKPAINTER_H

#include <pango/pango-context.h>
#include <pango/pango-layout.h>

#include <gtk/gtkwidget.h>

#include "htmlpainter.h"
#include "htmlfontmanager.h"

#define HTML_TYPE_GDK_PAINTER                 (html_gdk_painter_get_type ())
#define HTML_GDK_PAINTER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), HTML_TYPE_GDK_PAINTER, HTMLGdkPainter))
#define HTML_GDK_PAINTER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), HTML_TYPE_GDK_PAINTER, HTMLGdkPainterClass))
#define HTML_IS_GDK_PAINTER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HTML_TYPE_GDK_PAINTER))
#define HTML_IS_GDK_PAINTER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), HTML_TYPE_GDK_PAINTER))

struct _HTMLGdkPainter {
	HTMLPainter base;
	PangoContext *pc;
	GtkStyle *style;

	/* GdkWindow to draw on */
	GdkWindow *window;

	/* The current GC used.  */
	GdkGC *gc;

	/* For the double-buffering system.  */
	gboolean double_buffer;
	GdkPixmap *pixmap;
	gint x1, y1, x2, y2;
	GdkColor background;
	gboolean set_background;
	gboolean do_clear;
	gboolean alpha;

	/* Colors used for shading.  */
	GdkColor dark;
	GdkColor light;
	GdkColor black;
};

struct _HTMLGdkPainterClass {
	HTMLPainterClass base;
};

GType              html_gdk_painter_get_type                         (void);
HTMLPainter       *html_gdk_painter_new                              (GtkWidget             *widget,
								      gboolean               double_buffer);
void               html_gdk_painter_realize                          (HTMLGdkPainter        *painter,
								      GdkWindow             *window);
void               html_gdk_painter_unrealize                        (HTMLGdkPainter        *painter);
gboolean           html_gdk_painter_realized                         (HTMLGdkPainter        *painter);
HTMLTextPangoInfo *html_gdk_painter_text_itemize_and_prepare_glyphs  (HTMLGdkPainter        *painter,
								      PangoFontDescription  *desc,
								      const gchar           *text,
								      gint                   bytes,
								      GList                **glyphs,
								      gint                  *n_pi);

#endif /* _HTMLGDKPAINTER_H */
