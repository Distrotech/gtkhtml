/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This file is part of the KDE libraries
    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)

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
#ifndef _HTMLENGINE_H_
#define _HTMLENGINE_H_

typedef struct _HTMLEngine HTMLEngine;
typedef struct _HTMLEngineClass HTMLEngineClass;

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "gtkhtml.h"
#include "htmltokenizer.h"
#include "htmlclue.h"
#include "htmlfont.h"
#include "htmllist.h"
#include "htmlcolor.h"
#include "htmlstack.h"
#include "htmlsettings.h"
#include "htmlpainter.h"
#include "stringtokenizer.h"

#define TYPE_HTML_ENGINE                 (html_engine_get_type ())
#define HTML_ENGINE(obj)                  (GTK_CHECK_CAST ((obj), TYPE_HTML_ENGINE, HTMLEngine))
#define HTML_ENGINE_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), TYPE_HTML_ENGINE, HTMLEngineClass))
#define IS_HTML_ENGINE(obj)              (GTK_CHECK_TYPE ((obj), TYPE_HTML_ENGINE))
#define IS_HTML_ENGINE_CLASS(klass)      (GTK_CHECK_CLASS_TYPE ((klass), TYPE_HTML_ENGINE))

#define LEFT_BORDER 10
#define RIGHT_BORDER 20
#define TOP_BORDER 10
#define BOTTOM_BORDER 10

typedef void (*HTMLParseFunc)(HTMLEngine *p, HTMLObject *clue, const gchar *str);

struct _HTMLEngine {
	GtkObject parent;

	gboolean parsing;
	HTMLTokenizer *ht;
	StringTokenizer *st;
	HTMLObject *clue;
	
	HTMLObject *flow;

	gint leftBorder;
	gint rightBorder;
	gint topBorder;
	gint bottomBorder;

	/* Size of current indenting */
	gint indent;

	/* For the widget */
	gint width;
	gint height;

	gboolean vspace_inserted;
	gboolean bodyParsed;

	HAlignType divAlign;

	/* Number of tokens parsed in the current time-slice */
	gint parseCount;
	gint granularity;

	/* Offsets */
	gint x_offset, y_offset;

	gboolean inTitle;

	gboolean bold;
	gboolean italic;
	gboolean underline;
 
	gint fontsize;
	
	HTMLFontStack *fs;
	HTMLColorStack *cs;

	gchar *url;
	gchar *target;

	HTMLParseFunc parseFuncArray[26]; /* FIXME move to `.c'.  */
	HTMLPainter *painter;
	HTMLStackElement *blockStack;
	HTMLSettings *settings;

	/* timer id to schedule paint events */
	guint updateTimer;

	/* timer id for parsing routine */
	guint timerId;

	/* Should the background be painted? */
	gboolean bDrawBackground;


	GString *title;

	gboolean writing;

	/* FIXME: Something else than gchar* is probably
	   needed if we want to support anything else than
	   file:/ urls */
	gchar *actualURL;
	gchar *baseURL;

	/* from <BASE TARGET="..."> */
	gchar *baseTarget;

	/* The background pixmap, an HTMLImagePointer */
        gpointer bgPixmapPtr;
  
	/* The background color */
	GdkColor bgColor;

	/* Stack of lists currently active */
	HTMLListStack *listStack;

	/* the widget, used for signal emission*/
	GtkHTML *widget;

        gpointer image_factory;
};

struct _HTMLEngineClass {
	GtkObjectClass parent_class;
	
	void (*title_changed) (HTMLEngine *engine);
};


HTMLEngine *html_engine_new (void);
GtkHTMLStreamHandle html_engine_begin (HTMLEngine *p, const char *url);
void        html_engine_schedule_update (HTMLEngine *p);
void        html_engine_draw_background (HTMLEngine *e, gint xval, gint yval, gint x, gint y, gint w, gint h);
gchar      *html_engine_parse_body (HTMLEngine *p, HTMLObject *clue, const gchar *end[], gboolean toplevel);
void        html_engine_parse_one_token (HTMLEngine *p, HTMLObject *clue, const gchar *str);
void        html_engine_parse (HTMLEngine *p);
HTMLFont *  html_engine_get_current_font (HTMLEngine *p);
void        html_engine_select_font (HTMLEngine *e);
void        html_engine_pop_font (HTMLEngine *e);
void        html_engine_block_end_font (HTMLEngine *e, HTMLObject *clue, HTMLStackElement *elem);
void        html_engine_push_block (HTMLEngine *e, gint id, gint level, HTMLBlockFunc exitFunc, gint miscData1, gint miscData2);
void        html_engine_pop_block (HTMLEngine *e, gint id, HTMLObject *clue);
void        html_engine_insert_text (HTMLEngine *e, gchar *str, HTMLFont *f);
void        html_engine_calc_size (HTMLEngine *p);
void        html_engine_new_flow (HTMLEngine *p, HTMLObject *clue);
void        html_engine_draw (HTMLEngine *e, gint x, gint y, gint width, gint height);
gboolean    html_engine_insert_vspace (HTMLEngine *e, HTMLObject *clue, gboolean vspace_inserted);
void        html_engine_block_end_color_font (HTMLEngine *e, HTMLObject *clue, HTMLStackElement *elem);
void        html_engine_pop_color (HTMLEngine *e);
gboolean    html_engine_set_named_color (HTMLEngine *p, GdkColor *c, const gchar *name);
void        html_painter_set_background_color (HTMLPainter *painter, GdkColor *color);
gint        html_engine_get_doc_height (HTMLEngine *p);
void        html_engine_stop_parser (HTMLEngine *e);
void        html_engine_block_end_list (HTMLEngine *e, HTMLObject *clue, HTMLStackElement *elem);
void        html_engine_free_block (HTMLEngine *e);
void        html_engine_calc_absolute_pos (HTMLEngine *e);
char       *html_engine_canonicalize_url (HTMLEngine *e, const char *in_url);

#endif /* _HTMLENGINE_H_ */
