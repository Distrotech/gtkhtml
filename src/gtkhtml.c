/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.

    Copyright 1999, 2000 Helix Code, Inc.

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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <string.h>
#include <gal/widgets/e-unicode.h>

#include <gnome.h>

#include "htmlengine-edit-clueflowstyle.h"
#include "htmlengine-edit-copy.h"
#include "htmlengine-edit-cut.h"
#include "htmlengine-edit-delete.h"
#include "htmlengine-edit-fontstyle.h"
#include "htmlengine-edit-insert.h"
#include "htmlengine-edit-movement.h"
#include "htmlengine-edit-cursor.h"
#include "htmlengine-edit-paste.h"
#include "htmlengine-edit-text.h"
#include "htmlengine-edit.h"
#include "htmlengine-print.h"
#include "htmlcolorset.h"

#include "gtkhtml-embedded.h"
#include "gtkhtml-im.h"
#include "gtkhtml-input.h"
#include "gtkhtml-keybinding.h"
#include "gtkhtml-search.h"
#include "gtkhtml-stream.h"
#include "gtkhtml-private.h"
#include <libgnome/gnome-util.h>


static GtkLayoutClass *parent_class = NULL;

#ifdef GTKHTML_HAVE_GCONF
GConfClient *gconf_client = NULL;
GConfError  *gconf_error  = NULL;
#endif

enum {
	TITLE_CHANGED,
	URL_REQUESTED,
	LOAD_DONE,
	LINK_CLICKED,
	SET_BASE,
	SET_BASE_TARGET,
	ON_URL,
	REDIRECT,
	SUBMIT,
	OBJECT_REQUESTED,
	CURRENT_PARAGRAPH_STYLE_CHANGED,
	CURRENT_PARAGRAPH_INDENTATION_CHANGED,
	CURRENT_PARAGRAPH_ALIGNMENT_CHANGED,
	INSERTION_FONT_STYLE_CHANGED,
	INSERTION_COLOR_CHANGED,
	SIZE_CHANGED,
	/* keybindings signals */
	SCROLL,
	CURSOR_MOVE,
	INSERT_PARAGRAPH,
	/* now only last signal */
	LAST_SIGNAL
};
static guint signals [LAST_SIGNAL] = { 0 };

/* keybindings signal hadlers */
static void scroll                 (GtkHTML *html, GtkOrientation orientation, GtkScrollType scroll_type, gfloat position);
static void cursor_move            (GtkHTML *html, GtkDirectionType dir_type, GtkHTMLCursorSkipType skip);
static void command                (GtkHTML *html, GtkHTMLCommandType com_type);
static gint mouse_change_pos       (GtkWidget *widget, gint x, gint y);
static void load_keybindings       (GtkHTMLClass *klass);
static void set_editor_keybindings (GtkHTML *html, gboolean editable);


/* Values for selection information.  FIXME: what about COMPOUND_STRING and
   TEXT?  */
enum _TargetInfo {  
	TARGET_STRING,
	TARGET_TEXT,
	TARGET_COMPOUND_TEXT
};
typedef enum _TargetInfo TargetInfo;

/* Interval for scrolling during selection.  */
#define SCROLL_TIMEOUT_INTERVAL 10


static GtkHTMLParagraphStyle
clueflow_style_to_paragraph_style (HTMLClueFlowStyle style)
{
	switch (style) {
	case HTML_CLUEFLOW_STYLE_NORMAL:
		return GTK_HTML_PARAGRAPH_STYLE_NORMAL;
	case HTML_CLUEFLOW_STYLE_H1:
		return GTK_HTML_PARAGRAPH_STYLE_H1;
	case HTML_CLUEFLOW_STYLE_H2:
		return GTK_HTML_PARAGRAPH_STYLE_H2;
	case HTML_CLUEFLOW_STYLE_H3:
		return GTK_HTML_PARAGRAPH_STYLE_H3;
	case HTML_CLUEFLOW_STYLE_H4:
		return GTK_HTML_PARAGRAPH_STYLE_H4;
	case HTML_CLUEFLOW_STYLE_H5:
		return GTK_HTML_PARAGRAPH_STYLE_H5;
	case HTML_CLUEFLOW_STYLE_H6:
		return GTK_HTML_PARAGRAPH_STYLE_H6;
	case HTML_CLUEFLOW_STYLE_ADDRESS:
		return GTK_HTML_PARAGRAPH_STYLE_ADDRESS;
	case HTML_CLUEFLOW_STYLE_PRE:
		return GTK_HTML_PARAGRAPH_STYLE_PRE;
	case HTML_CLUEFLOW_STYLE_ITEMDOTTED:
		return GTK_HTML_PARAGRAPH_STYLE_ITEMDOTTED;
	case HTML_CLUEFLOW_STYLE_ITEMROMAN:
		return GTK_HTML_PARAGRAPH_STYLE_ITEMROMAN;
	case HTML_CLUEFLOW_STYLE_ITEMDIGIT:
		return GTK_HTML_PARAGRAPH_STYLE_ITEMDIGIT;
	default:		/* This should not really happen, though.  */
		return GTK_HTML_PARAGRAPH_STYLE_NORMAL;
	}
}

static HTMLClueFlowStyle
paragraph_style_to_clueflow_style (GtkHTMLParagraphStyle style)
{
	switch (style) {
	case GTK_HTML_PARAGRAPH_STYLE_NORMAL:
		return HTML_CLUEFLOW_STYLE_NORMAL;
	case GTK_HTML_PARAGRAPH_STYLE_H1:
		return HTML_CLUEFLOW_STYLE_H1;
	case GTK_HTML_PARAGRAPH_STYLE_H2:
		return HTML_CLUEFLOW_STYLE_H2;
	case GTK_HTML_PARAGRAPH_STYLE_H3:
		return HTML_CLUEFLOW_STYLE_H3;
	case GTK_HTML_PARAGRAPH_STYLE_H4:
		return HTML_CLUEFLOW_STYLE_H4;
	case GTK_HTML_PARAGRAPH_STYLE_H5:
		return HTML_CLUEFLOW_STYLE_H5;
	case GTK_HTML_PARAGRAPH_STYLE_H6:
		return HTML_CLUEFLOW_STYLE_H6;
	case GTK_HTML_PARAGRAPH_STYLE_ADDRESS:
		return HTML_CLUEFLOW_STYLE_ADDRESS;
	case GTK_HTML_PARAGRAPH_STYLE_PRE:
		return HTML_CLUEFLOW_STYLE_PRE;
	case GTK_HTML_PARAGRAPH_STYLE_ITEMDOTTED:
		return HTML_CLUEFLOW_STYLE_ITEMDOTTED;
	case GTK_HTML_PARAGRAPH_STYLE_ITEMROMAN:
		return HTML_CLUEFLOW_STYLE_ITEMROMAN;
	case GTK_HTML_PARAGRAPH_STYLE_ITEMDIGIT:
		return HTML_CLUEFLOW_STYLE_ITEMDIGIT;
	default:		/* This should not really happen, though.  */
		return HTML_CLUEFLOW_STYLE_NORMAL;
	}
}

static HTMLHAlignType
paragraph_alignment_to_html (GtkHTMLParagraphAlignment alignment)
{
	switch (alignment) {
	case GTK_HTML_PARAGRAPH_ALIGNMENT_LEFT:
		return HTML_HALIGN_LEFT;
	case GTK_HTML_PARAGRAPH_ALIGNMENT_RIGHT:
		return HTML_HALIGN_RIGHT;
	case GTK_HTML_PARAGRAPH_ALIGNMENT_CENTER:
		return HTML_HALIGN_CENTER;
	default:
		return HTML_HALIGN_LEFT;
	}
}

static GtkHTMLParagraphAlignment
html_alignment_to_paragraph (HTMLHAlignType alignment)
{
	switch (alignment) {
	case HTML_HALIGN_LEFT:
		return GTK_HTML_PARAGRAPH_ALIGNMENT_LEFT;
	case HTML_HALIGN_CENTER:
		return GTK_HTML_PARAGRAPH_ALIGNMENT_CENTER;
	case HTML_HALIGN_RIGHT:
		return GTK_HTML_PARAGRAPH_ALIGNMENT_RIGHT;
	default:
		return GTK_HTML_PARAGRAPH_ALIGNMENT_LEFT;
	}
}

static void
update_styles (GtkHTML *html)
{
	GtkHTMLParagraphStyle paragraph_style;
	GtkHTMLParagraphAlignment alignment;
	HTMLClueFlowStyle clueflow_style;
	HTMLEngine *engine;
	guint indentation;

	if (! html_engine_get_editable (html->engine))
		return;

	engine = html->engine;

	clueflow_style = html_engine_get_current_clueflow_style (engine);
	paragraph_style = clueflow_style_to_paragraph_style (clueflow_style);

	if (paragraph_style != html->priv->paragraph_style) {
		html->priv->paragraph_style = paragraph_style;
		gtk_signal_emit (GTK_OBJECT (html), signals[CURRENT_PARAGRAPH_STYLE_CHANGED],
				 paragraph_style);
	}

	indentation = html_engine_get_current_clueflow_indentation (engine);
	if (indentation != html->priv->paragraph_indentation) {
		html->priv->paragraph_style = paragraph_style;
		gtk_signal_emit (GTK_OBJECT (html), signals[CURRENT_PARAGRAPH_STYLE_CHANGED], paragraph_style);
	}

	alignment = html_alignment_to_paragraph (html_engine_get_current_clueflow_alignment (engine));
 	if (alignment != html->priv->paragraph_alignment) {
		html->priv->paragraph_alignment = alignment;
		gtk_signal_emit (GTK_OBJECT (html), signals[CURRENT_PARAGRAPH_ALIGNMENT_CHANGED], alignment);
	}

	if (html_engine_update_insertion_font_style (engine))
		gtk_signal_emit (GTK_OBJECT (html), signals[INSERTION_FONT_STYLE_CHANGED], engine->insertion_font_style);

	if (html_engine_update_insertion_color (engine))
		gtk_signal_emit (GTK_OBJECT (html), signals[INSERTION_COLOR_CHANGED], engine->insertion_color);
	/* TODO add insertion_url_or_targed_changed signal */
	html_engine_update_insertion_url_and_target (engine);
}


/* GTK+ idle loop handler.  */

static gint
idle_handler (gpointer data)
{
	GtkHTML *html;
	HTMLEngine *engine;

	html = GTK_HTML (data);
	engine = html->engine;

	html_engine_make_cursor_visible (engine);

	gtk_adjustment_set_value (GTK_LAYOUT (html)->hadjustment, (gfloat) engine->x_offset);
	gtk_adjustment_set_value (GTK_LAYOUT (html)->vadjustment, (gfloat) engine->y_offset);

	gtk_html_private_calc_scrollbars (html);

	html_engine_flush_draw_queue (engine);

 	html->priv->idle_handler_id = 0;
	return FALSE;
}

static void
queue_draw (GtkHTML *html)
{
	if (html->priv->idle_handler_id == 0)
		html->priv->idle_handler_id = gtk_idle_add (idle_handler, html);
}


/* HTMLEngine callbacks.  */

static void
html_engine_title_changed_cb (HTMLEngine *engine, gpointer data)
{
	GtkHTML *gtk_html;

	gtk_html = GTK_HTML (data);
	gtk_signal_emit (GTK_OBJECT (gtk_html), signals[TITLE_CHANGED], engine->title->str);
}

static void
html_engine_set_base_cb (HTMLEngine *engine, const gchar *base, gpointer data)
{
	GtkHTML *gtk_html;

	gtk_html = GTK_HTML (data);
	gtk_signal_emit (GTK_OBJECT (gtk_html), signals[SET_BASE], base);
}

static void
html_engine_set_base_target_cb (HTMLEngine *engine, const gchar *base_target, gpointer data)
{
	GtkHTML *gtk_html;

	gtk_html = GTK_HTML (data);
	gtk_signal_emit (GTK_OBJECT (gtk_html), signals[SET_BASE_TARGET], base_target);
}

static void
html_engine_load_done_cb (HTMLEngine *engine, gpointer data)
{
	GtkHTML *gtk_html;

	gtk_html = GTK_HTML (data);
	gtk_signal_emit (GTK_OBJECT (gtk_html), signals[LOAD_DONE]);
}

static void
html_engine_url_requested_cb (HTMLEngine *engine,
			      const gchar *url,
			      GtkHTMLStream *handle,
			      gpointer data)
{
	GtkHTML *gtk_html;

	gtk_html = GTK_HTML (data);
	gtk_signal_emit (GTK_OBJECT (gtk_html), signals[URL_REQUESTED], url, handle);
}

static void
html_engine_draw_pending_cb (HTMLEngine *engine,
			     gpointer data)
{
	GtkHTML *html;

	html = GTK_HTML (data);
	queue_draw (html);
}

static void
html_engine_redirect_cb (HTMLEngine *engine,
			 const gchar *url,
			 int delay,
			 gpointer data)
{
	GtkHTML *gtk_html;

	gtk_html = GTK_HTML (data);

	gtk_signal_emit (GTK_OBJECT (gtk_html), signals[REDIRECT], url, delay);
}

static void
html_engine_submit_cb (HTMLEngine *engine,
		       const gchar *method,
		       const gchar *url,
		       const gchar *encoding,
		       gpointer data)
{
	GtkHTML *gtk_html;

	gtk_html = GTK_HTML (data);

	gtk_signal_emit (GTK_OBJECT (gtk_html), signals[SUBMIT], method, url, encoding);
}

static gboolean
html_engine_object_requested_cb (HTMLEngine *engine,
		       GtkHTMLEmbedded *eb,
		       gpointer data)
{
	GtkHTML *gtk_html;
	gboolean ret_val = FALSE;

	gtk_html = GTK_HTML (data);

	ret_val = FALSE;
	gtk_signal_emit (GTK_OBJECT (gtk_html), signals[OBJECT_REQUESTED], eb, &ret_val);
	return ret_val;
}


/* GtkAdjustment handling.  */

static void
scroll_update_mouse (GtkWidget *widget)
{
	gint x, y;

	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_get_pointer (GTK_LAYOUT (widget)->bin_window, &x, &y, NULL);
		mouse_change_pos (widget, x, y);
	}
}

static void
vertical_scroll_cb (GtkAdjustment *adjustment, gpointer data)
{
	GtkHTML *html = GTK_HTML (data);

	html->engine->y_offset = (gint)adjustment->value;
	scroll_update_mouse (GTK_WIDGET (data));
}

static void
horizontal_scroll_cb (GtkAdjustment *adjustment, gpointer data)
{
	GtkHTML *html = GTK_HTML (data);
		
	html->engine->x_offset = (gint)adjustment->value;
	scroll_update_mouse (GTK_WIDGET (data));
}

static void
connect_adjustments (GtkHTML *html,
		     GtkAdjustment *hadj,
		     GtkAdjustment *vadj)
{
	GtkLayout *layout;

	layout = GTK_LAYOUT (html);

	if (html->hadj_connection != 0)
		gtk_signal_disconnect (GTK_OBJECT(layout->hadjustment),
				       html->hadj_connection);

	if (html->vadj_connection != 0)
		gtk_signal_disconnect (GTK_OBJECT(layout->vadjustment),
				       html->vadj_connection);

	if (vadj != NULL)
		html->vadj_connection =
			gtk_signal_connect (GTK_OBJECT (vadj), "value_changed",
					    GTK_SIGNAL_FUNC (vertical_scroll_cb), (gpointer)html);
	else
		html->vadj_connection = 0;
	
	if (hadj != NULL)
		html->hadj_connection =
			gtk_signal_connect (GTK_OBJECT (hadj), "value_changed",
					    GTK_SIGNAL_FUNC (horizontal_scroll_cb), (gpointer)html);
	else
		html->hadj_connection = 0;
}


/* Scroll timeout handling.  */

static void
inc_adjustment (GtkAdjustment *adj, gint doc_width, gint alloc_width, gint inc)
{
	gfloat value;
	gint max;

	value = adj->value + (gfloat) inc;
	
	if (doc_width > alloc_width)
		max = doc_width - alloc_width;
	else
		max = 0;

	if (value > (gfloat) max)
		value = (gfloat) max;
	else if (value < 0)
		value = 0.0;

	gtk_adjustment_set_value (adj, value);
}

static gint
scroll_timeout_cb (gpointer data)
{
	GtkWidget *widget;
	GtkHTML *html;
	GtkLayout *layout;
	gint x_scroll, y_scroll;
	gint x, y;

	GDK_THREADS_ENTER ();

	widget = GTK_WIDGET (data);
	html = GTK_HTML (data);

	gdk_window_get_pointer (widget->window, &x, &y, NULL);

	if (x < 0) {
		x_scroll = x;
		x = 0;
	} else if (x >= widget->allocation.width) {
		x_scroll = x - widget->allocation.width + 1;
		x = widget->allocation.width;
	} else {
		x_scroll = 0;
	}
	x_scroll /= 2;

	if (y < 0) {
		y_scroll = y;
		y = 0;
	} else if (y >= widget->allocation.height) {
		y_scroll = y - widget->allocation.height + 1;
		y = widget->allocation.height;
	} else {
		y_scroll = 0;
	}
	y_scroll /= 2;

	if (html->in_selection && (x_scroll != 0 || y_scroll != 0)) {
		HTMLEngine *engine;

		engine = html->engine;
		html_engine_select_region (engine,
					   html->selection_x1, html->selection_y1,
					   x + engine->x_offset, y + engine->y_offset);
	}

	layout = GTK_LAYOUT (widget);

	inc_adjustment (layout->hadjustment, html_engine_get_doc_width (html->engine),
			widget->allocation.width, x_scroll);
	inc_adjustment (layout->vadjustment, html_engine_get_doc_height (html->engine),
			widget->allocation.height, y_scroll);

	GDK_THREADS_LEAVE ();

	return TRUE;
}

static void
setup_scroll_timeout (GtkHTML *html)
{
	if (html->priv->scroll_timeout_id != 0)
		return;

	html->priv->scroll_timeout_id = gtk_timeout_add (SCROLL_TIMEOUT_INTERVAL,
						   scroll_timeout_cb, html);

	GDK_THREADS_LEAVE();
	scroll_timeout_cb (html);
	GDK_THREADS_ENTER();
}

static void
remove_scroll_timeout (GtkHTML *html)
{
	if (html->priv->scroll_timeout_id == 0)
		return;

	gtk_timeout_remove (html->priv->scroll_timeout_id);
	html->priv->scroll_timeout_id = 0;
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	GtkHTML *html;

	html = GTK_HTML (object);

	g_free (html->pointer_url);
	gdk_cursor_destroy (html->hand_cursor);
	gdk_cursor_destroy (html->arrow_cursor);
	gdk_cursor_destroy (html->ibeam_cursor);

	connect_adjustments (html, NULL, NULL);

	if (html->priv->idle_handler_id != 0)
		gtk_idle_remove (html->priv->idle_handler_id);

	if (html->priv->scroll_timeout_id != 0)
		gtk_timeout_remove (html->priv->scroll_timeout_id);
	
	g_free (html->priv);
	html->priv = NULL;

	gtk_object_destroy (GTK_OBJECT (html->engine));
	gtk_html_input_line_destroy (html->input_line);

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* GtkWidget methods.  */
static void
style_set (GtkWidget *widget,
	   GtkStyle  *previous_style)
{
	HTMLEngine *engine = GTK_HTML (widget)->engine;
       
	html_colorset_set_style (engine->defaultSettings->color_set,
				 widget->style);
	html_colorset_set_unchanged (engine->settings->color_set,
				     engine->defaultSettings->color_set);
	html_engine_schedule_update (engine);

#ifdef GTKHTML_USE_XIM
	if (previous_style)
		gtk_html_im_style_set (widget);
#endif
}

static gint
key_press_event (GtkWidget *widget,
		 GdkEventKey *event)
{
	GtkHTML *html = GTK_HTML (widget);
	gboolean retval;
	gint position = html->engine->cursor->position;

	html->binding_handled = FALSE;
	if (html->editor_bindings && html_engine_get_editable (html->engine))
		gtk_binding_set_activate (html->editor_bindings, event->keyval, event->state, GTK_OBJECT (widget));
	gtk_bindings_activate (GTK_OBJECT (widget), event->keyval, event->state);
	retval = html->binding_handled;

	if (! retval
	    && html_engine_get_editable (html->engine)
	    && ! (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK))
	    && event->length > 0) {
		gchar *str;
		html_engine_delete_selection (html->engine, TRUE);
		str = e_utf8_from_gtk_event_key (widget, event->keyval, event->string);
		/* printf ("len: %d str: %s\n", unicode_strlen (str, -1), str); */
		if (str)
			html_engine_insert (html->engine, str, unicode_strlen (str, -1));
		g_free (str);
		retval = TRUE;
	}

	if (retval && html_engine_get_editable (html->engine))
		html_engine_reset_blinking_cursor (html->engine);

	if (retval) {
		queue_draw (html);
		if (position != html->engine->cursor->position)
			update_styles (html);
	}

	return retval;
}

static void
realize (GtkWidget *widget)
{
	GtkHTML *html;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_HTML (widget));

	html = GTK_HTML (widget);

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gdk_window_set_events (html->layout.bin_window,
			       (gdk_window_get_events (html->layout.bin_window)
				| GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK
				| GDK_ENTER_NOTIFY_MASK
				| GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
				| GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK));

	html_engine_realize (html->engine, html->layout.bin_window);

	gdk_window_set_cursor (widget->window, html->arrow_cursor);

	/* This sets the backing pixmap to None, so that scrolling does not
           erase the newly exposed area, thus making the thing smoother.  */
	gdk_window_set_back_pixmap (html->layout.bin_window, NULL, FALSE);

#ifdef GTKHTML_USE_XIM
	gtk_html_im_realize (html);
#endif /* GTKHTML_USE_XIM */
}

static void
unrealize (GtkWidget *widget)
{
	GtkHTML *html = GTK_HTML (widget);
	
	html_engine_unrealize (html->engine);

#ifdef GTKHTML_USE_XIM	
	gtk_html_im_unrealize (html);
#endif
	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static gint
expose (GtkWidget *widget, GdkEventExpose *event)
{
	html_engine_draw (GTK_HTML (widget)->engine,
			  event->area.x, event->area.y,
			  event->area.width, event->area.height);

	if (GTK_WIDGET_CLASS (parent_class)->expose_event)
		(* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);

	return TRUE;
}

static void
draw (GtkWidget *widget, GdkRectangle *area)
{
	GtkHTML *html = GTK_HTML (widget);
	HTMLPainter *painter = html->engine->painter;

	html_painter_clear (painter);

	html_engine_draw (GTK_HTML (widget)->engine,
			  area->x, area->y,
			  area->width, area->height);

	if (GTK_WIDGET_CLASS (parent_class)->draw)
		(* GTK_WIDGET_CLASS (parent_class)->draw) (widget, area);
}

static void
size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GtkHTML *html;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_HTML (widget));
	g_return_if_fail (allocation != NULL);
	
	html = GTK_HTML (widget);

	if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
		( *GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

	if (html->engine->width != allocation->width
	    || html->engine->height != allocation->height) {
		HTMLEngine *e = html->engine;

		e->width  = allocation->width;
		e->height = allocation->height;

		if (e->clue && e->painter
		    && html_object_calc_min_width (e->clue, e->painter) < e->width - e->leftBorder - e->rightBorder)
			html_engine_calc_size (html->engine);

		gtk_html_private_calc_scrollbars (html);
	}

#ifdef GTKHTML_USE_XIM
	gtk_html_im_size_allocate (html);
#endif
}

static void
on_object (GtkWidget *widget, HTMLObject *obj)
{
	GtkHTML *html = GTK_HTML (widget);
	const gchar *url;

	if (obj) {
		if ((url = (obj) ? html_object_get_url (obj) : NULL)) {
			if (html->pointer_url == NULL || strcmp (html->pointer_url, url) != 0) {
				g_free (html->pointer_url);
				html->pointer_url = g_strdup (url);
				gtk_signal_emit (GTK_OBJECT (html), signals[ON_URL], url);
			}

			if (html->engine->editable)
				gdk_window_set_cursor (widget->window, html->ibeam_cursor);
			else
				gdk_window_set_cursor (widget->window, html->hand_cursor);
		} else {
			if (html->pointer_url != NULL) {
				g_free (html->pointer_url);
				html->pointer_url = NULL;
				gtk_signal_emit (GTK_OBJECT (html), signals [ON_URL], NULL);
			}

			if (obj != NULL && html_object_is_text (obj) && html->allow_selection)
				gdk_window_set_cursor (widget->window, html->ibeam_cursor);
			else
				gdk_window_set_cursor (widget->window, html->arrow_cursor);
		}
	} else {
		if (html->pointer_url) {
			html->pointer_url = NULL;
			g_free (html->pointer_url);
		}
		gdk_window_set_cursor (widget->window, html->arrow_cursor);
	}
}

static gint
mouse_change_pos (GtkWidget *widget, gint x, gint y)
{
	GtkHTML *html;
	HTMLEngine *engine;
	HTMLObject *obj;
	HTMLType type;

	if (!GTK_WIDGET_REALIZED (widget))
		return FALSE;

	html = GTK_HTML (widget);
	engine = html->engine;

	obj = html_engine_get_object_at (engine,
					 x + engine->x_offset, y + engine->y_offset,
					 NULL, FALSE);

	if (html->button1_pressed && html->allow_selection) {
		gboolean need_scroll;

		if (obj) {
			type = HTML_OBJECT_TYPE (obj);

			/* FIXME this is broken */

			if (type == HTML_TYPE_BUTTON ||
			    type ==  HTML_TYPE_CHECKBOX ||
			    type ==  HTML_TYPE_EMBEDDED ||
			    type ==  HTML_TYPE_HIDDEN ||
			    type ==  HTML_TYPE_IMAGEINPUT ||
			    type ==  HTML_TYPE_RADIO ||
			    type ==  HTML_TYPE_SELECT ||
			    type ==  HTML_TYPE_TEXTAREA ||
			    type ==  HTML_TYPE_TEXTINPUT ) {
				return FALSE;
			}
		}

		html->in_selection = TRUE;

		need_scroll = FALSE;

		if (x < 0) {
			x = 0;
			need_scroll = TRUE;
		} else if (x >= widget->allocation.width) {
			x = widget->allocation.width - 1;
			need_scroll = TRUE;
		}

		if (y < 0) {
			y = 0;
			need_scroll = TRUE;
		} else if (y >= widget->allocation.height) {
			y = widget->allocation.height - 1;
			need_scroll = TRUE;
		}

		if (need_scroll)
			setup_scroll_timeout (html);
		else
			remove_scroll_timeout (html);

		/* This will put the mark at the position of the
                   previous click.  */
		if (engine->mark == NULL && engine->editable)
			html_engine_set_mark (engine);

		html_engine_select_region (engine,
					   html->selection_x1, html->selection_y1,
					   x + engine->x_offset, y + engine->y_offset);
	}

	on_object (widget, obj);

	return TRUE;
}

static gint
motion_notify_event (GtkWidget *widget,
		     GdkEventMotion *event)
{
	HTMLEngine *engine;
	gint x, y;

	g_return_val_if_fail (widget != NULL, 0);
	g_return_val_if_fail (GTK_IS_HTML (widget), 0);
	g_return_val_if_fail (event != NULL, 0);

	if (event->is_hint) {
		gdk_window_get_pointer (GTK_LAYOUT (widget)->bin_window, &x, &y, NULL);
	} else {
		x = event->x;
		y = event->y;
	}

	if (!mouse_change_pos (widget, x, y))
		return FALSE;

	engine = GTK_HTML (widget)->engine;
	if (GTK_HTML (widget)->button1_pressed && html_engine_get_editable (engine))
		html_engine_jump_at (engine,
				     event->x + engine->x_offset,
				     event->y + engine->y_offset);
	return TRUE;
}

static gint
button_press_event (GtkWidget *widget,
		    GdkEventButton *event)
{
	GtkHTML *html;
	HTMLEngine *engine;
	gint value;

	html = GTK_HTML (widget);
	engine = html->engine;

	gtk_widget_grab_focus (widget);

	if (event->type == GDK_BUTTON_PRESS) {
		GtkAdjustment *vadj;

		vadj   = GTK_LAYOUT ((html->iframe_parent) ? html->iframe_parent : widget)->vadjustment;
		
		switch (event->button) {
		case 4:
			/* Mouse wheel scroll up.  */
			value = vadj->value - vadj->step_increment * 3;
			
			if (value < vadj->lower)
				value = vadj->lower;
			
			gtk_adjustment_set_value (vadj, value);
			return TRUE;
			break;
		case 5:
			/* Mouse wheel scroll down.  */
			value = vadj->value + vadj->step_increment * 3;
			
			if (value > (vadj->upper - vadj->page_size))
				value = vadj->upper - vadj->page_size;
			
			gtk_adjustment_set_value (vadj, value);
			return TRUE;
			break;
		case 2:
			if (html_engine_get_editable (engine)) {
				gtk_html_request_paste (widget, event->time);
				return TRUE;
			}
			break;
		case 1:
			html->button1_pressed = TRUE;
			break;
		default:
			break;
		}
	}

	if (html_engine_get_editable (engine)) {
		html_engine_jump_at (engine,
				     event->x + engine->x_offset,
				     event->y + engine->y_offset);
		update_styles (html);
	}

	if (html->allow_selection && !(event->state & GDK_SHIFT_MASK)) {
		if (gdk_pointer_grab (GTK_LAYOUT (widget)->bin_window, FALSE,
				      (GDK_BUTTON_RELEASE_MASK
				       | GDK_BUTTON_MOTION_MASK
				       | GDK_POINTER_MOTION_HINT_MASK),
				      NULL, NULL, 0) == 0) {
			html->selection_x1 = event->x + engine->x_offset;
			html->selection_y1 = event->y + engine->y_offset;

			if (event->button == 1) {
				if (event->type == GDK_2BUTTON_PRESS && !event->state)
					gtk_html_select_word (html);
				else if (event->type == GDK_3BUTTON_PRESS && !event->state)
					gtk_html_select_line (html);
			}
		}
	}

	if (!(event->button == 1
	      && (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS)
	      && !event->state)) {
		if (!(event->state & GDK_SHIFT_MASK))
			html_engine_disable_selection (engine);
		else if (html->allow_selection)
			html_engine_select_region (engine,
						   html->selection_x1, html->selection_y1,
						   event->x + engine->x_offset, event->y + engine->y_offset);
	}

	return TRUE;
}

static gint
button_release_event (GtkWidget *widget,
		      GdkEventButton *event)
{
	GtkHTML *html;

	html = GTK_HTML (widget);

	gtk_grab_remove (widget);
	gdk_pointer_ungrab (0);

	if (event->button == 1) {
		html->button1_pressed = FALSE;
		if (html->pointer_url != NULL && ! html->in_selection)
			gtk_signal_emit (GTK_OBJECT (widget), signals[LINK_CLICKED], html->pointer_url);
	}

	if (html->in_selection) {
		gtk_selection_owner_set (widget, GDK_SELECTION_PRIMARY, event->time);
		html->in_selection = FALSE;
		update_styles (html);
	}

	remove_scroll_timeout (html);

	return TRUE;
}

static gint
focus_in_event (GtkWidget *widget,
		GdkEventFocus *event)
{
	GtkHTML *html = GTK_HTML (widget);

	if (!html->iframe_parent) {
		GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
		html_engine_set_focus (html->engine, TRUE);
	} else {
		GtkWidget *window = gtk_widget_get_ancestor (widget, gtk_window_get_type ());
		if (window)
			gtk_window_set_focus (GTK_WINDOW (window), html->iframe_parent);
	}

#ifdef GTKHTML_USE_XIM
	gtk_html_im_focus_in (html);
#endif

	return FALSE;
}

static gint
focus_out_event (GtkWidget *widget,
		 GdkEventFocus *event)
{
	GtkHTML *html = GTK_HTML (widget);
	if (!html->iframe_parent) {
		GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
		html_engine_set_focus (html->engine, FALSE);
	}

#ifdef GTKHTML_USE_XIM
	gtk_html_im_focus_out (html);
#endif

	return FALSE;
}

static gint
enter_notify_event (GtkWidget *widget, GdkEventCrossing *event)
{
	mouse_change_pos (widget, event->x, event->y);
	return TRUE;
}


/* X11 selection support.  */

static void
selection_get (GtkWidget        *widget, 
	       GtkSelectionData *selection_data_ptr,
	       guint             info,
	       guint             time)
{
	GtkHTML *html;
	gchar *selection_string;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_HTML (widget));
	
	html = GTK_HTML (widget);
	selection_string = html_engine_get_selection_string (html->engine);
	
	if (selection_string != NULL) {
		if (info == TARGET_STRING)
			{
				gtk_selection_data_set (selection_data_ptr,
							GDK_SELECTION_TYPE_STRING, 8,
							(const guchar *) selection_string, 
							strlen (selection_string));
			}
		else if ((info == TARGET_TEXT) || (info == TARGET_COMPOUND_TEXT))
			{
				guchar *text;
				GdkAtom encoding;
				gint format;
				gint new_length;
				
				gdk_string_to_compound_text (selection_string, 
							     &encoding, &format,
							     &text, &new_length);

				gtk_selection_data_set (selection_data_ptr,
							encoding, format,
							text, new_length);
				gdk_free_compound_text (text);
			}
		g_free (selection_string);
	}
}

/* receive a selection */
/* Signal handler called when the selections owner returns the data */
static void
selection_received (GtkWidget *widget,
		    GtkSelectionData *selection_data, 
		    guint time)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_HTML (widget));
	g_return_if_fail (selection_data != NULL);
	
	printf("got selection from system\n");
	
#if 0
	/* **** IMPORTANT **** Check to see if retrieval succeeded  */
	/* If we have more selection types we can ask for, try the next one,
	   until there are none left */
	if (selection_data->length < 0) {
		struct _zvtprivate *zp = _ZVT_PRIVATE(widget);
		
		/* now, try again with next selection type */
		if (gtk_html_request_paste(widget, zp->lastselectiontype+1, time)==0)
			g_print ("Selection retrieval failed\n");
		return;
	}
#endif
	
	/* we will get a selection type of atom(UTF-8) for utf text,
	   perhaps that needs to do something different if the terminal
	   isn't actually in utf8 mode? */
	
	/* Make sure we got the data in the expected form */
	if (selection_data->type != GDK_SELECTION_TYPE_STRING) {
		g_print ("Selection \"STRING\" was not returned as strings!\n");
		return;
	}
	
	if (selection_data->length) {
		printf ("selection text \"%.*s\"\n",
			selection_data->length, selection_data->data); 

		html_engine_delete_selection (GTK_HTML (widget)->engine, TRUE);
		html_engine_insert (GTK_HTML (widget)->engine, 
				    selection_data->data,
				    (guint) selection_data->length);
	}
}  

int
gtk_html_request_paste (GtkWidget *widget, gint32 time)
{
  GdkAtom string_atom;

  string_atom = gdk_atom_intern ("STRING", FALSE);

  if (string_atom == GDK_NONE) {
    g_warning("WARNING: Could not get string atom\n");
  }
  /* And request the "STRING" target for the primary selection */
    gtk_selection_convert (widget, GDK_SELECTION_PRIMARY, string_atom,
			   time);
  return 1;
}


static gint
selection_clear_event (GtkWidget *widget,
		       GdkEventSelection *event)
{
	GtkHTML *html;

	if (! gtk_selection_clear (widget, event))
		return FALSE;

	html = GTK_HTML (widget);

	html_engine_disable_selection (html->engine);
	html->in_selection = FALSE;

	return TRUE;
}


static void
set_adjustments (GtkLayout     *layout,
		 GtkAdjustment *hadj,
		 GtkAdjustment *vadj)
{
	GtkHTML *html = GTK_HTML(layout);

	connect_adjustments (html, hadj, vadj);
	
	if (parent_class->set_scroll_adjustments)
		(* parent_class->set_scroll_adjustments) (layout, hadj, vadj);
}


/* Initialization.  */

static gint
set_fonts_idle (GtkHTML *html)
{
	GtkHTMLClassProperties *prop = GTK_HTML_CLASS (GTK_OBJECT (html)->klass)->properties;

	if (html->engine) {
		html_font_manager_set_default (&html->engine->painter->font_manager,
					       prop->font_var_family, prop->font_fix_family,
					       prop->font_var_size,   prop->font_fix_size);

		/* tables don't resize correctly :( who knows the solution? */
		if (html->engine->clue) {
			html_object_reset (html->engine->clue);
			html_object_change_set_down (html->engine->clue, HTML_CHANGE_ALL);
			html_engine_calc_size (html->engine);
			html_engine_schedule_update (html->engine);
		}
	}
#ifdef GTKHTML_HAVE_GCONF
	html->priv->set_font_id = 0;
#endif

	return FALSE;
}

#ifdef GTKHTML_HAVE_GCONF

static void
set_fonts (GtkHTML *html)
{
#ifdef GTKHTML_HAVE_GCONF
	if (!html->priv->set_font_id)
		html->priv->set_font_id = gtk_idle_add ((GtkFunction) set_fonts_idle, html);
#else
	set_fonts_idle (html);
#endif
}

static void
client_notify_widget (GConfClient* client,
		      guint cnxn_id,
		      const gchar* key,
		      GConfValue* value,
		      gboolean is_default,
		      gpointer user_data)
{
	GtkHTML *html = (GtkHTML *) user_data;
	GtkHTMLClass *klass = GTK_HTML_CLASS (GTK_OBJECT (html)->klass);
	GtkHTMLClassProperties *prop = klass->properties;	
	gchar *tkey;

	/* printf ("notify widget\n"); */
	g_assert (client == gconf_client);
	g_assert (key);
	tkey = strrchr (key, '/');
	g_assert (tkey);

	if (!strcmp (tkey, "/font_variable_family")) {
		g_free (prop->font_var_family);
		prop->font_var_family = gconf_client_get_string (client, key, NULL);
		set_fonts (html);
	} else if (!strcmp (tkey, "/font_fixed_family")) {
		g_free (prop->font_fix_family);
		prop->font_fix_family = gconf_client_get_string (client, key, NULL);
		set_fonts (html);
	} else if (!strcmp (tkey, "/font_variable_size")) {
		prop->font_var_size = gconf_client_get_int (client, key, NULL);
		set_fonts (html);
	} else if (!strcmp (tkey, "/font_fixed_size")) {
		prop->font_fix_size = gconf_client_get_int (client, key, NULL);
		set_fonts (html);
	} else if (!strcmp (tkey, "/spell_error_color_red") {
		prop->spell_error_color.red = gconf_client_get_int (client, key, NULL);
	} else if (!strcmp (tkey, "/spell_error_color_green") {
		prop->spell_error_color.green = gconf_client_get_int (client, key, NULL);
	} else if (!strcmp (tkey, "/spell_error_color_blue") {
		prop->spell_error_color.blue = gconf_client_get_int (client, key, NULL);
		html_colorset_set_color (html->engine->defaultSettings->color_set,
					 &prop->spell_error_color, HTMLSpellErrorColor);
		html_colorset_set_color (html->engine->settings->color_set,
					 &prop->spell_error_color, HTMLSpellErrorColor);
		if (html_engine_get_editable (html->engine) && !strcmp (tkey, "/spell_error_color_blue"))
			gtk_widget_queue_draw (GTK_WIDGET (html));
	} else if (!strcmp (tkey, "/live_spell_check")) {
		prop->live_spell_check = gconf_client_get_bool (client, key, NULL);
	} else if (!strcmp (tkey, "/language")) {
		g_free (prop->language);
		prop->language = g_strdup (gconf_client_get_string (client, key, NULL);
	}
}

static void
client_notify_class (GConfClient* client,
		     guint cnxn_id,
		     const gchar* key,
		     GConfValue* value,
		     gboolean is_default,
		     gpointer user_data)
{
	GtkHTMLClass *klass = (GtkHTMLClass *) user_data;
	GtkHTMLClassProperties *prop = klass->properties;	
	gchar *tkey;

	g_assert (client == gconf_client);
	g_assert (key);
	tkey = strrchr (key, '/');
	g_assert (tkey);

	if (!strcmp (tkey, "/magic_links")) {
		prop->magic_links = gconf_client_get_bool (client, key, NULL);
	} else if (!strcmp (tkey, "/keybindings_theme")) {
		g_free (prop->keybindings_theme);
		prop->keybindings_theme = gconf_client_get_string (client, key, NULL);
		load_keybindings (klass);
	}
}

#endif

static void
init_properties (GtkHTMLClass *klass)
{
	klass->properties = gtk_html_class_properties_new ();
#ifdef GTKHTML_HAVE_GCONF
	if (gconf_is_initialized ()) {
		gconf_client = gconf_client_get_default ();
		if (!gconf_client)
			g_error ("cannot create gconf_client\n");
		gconf_client_add_dir (gconf_client, GTK_HTML_GCONF_DIR, GCONF_CLIENT_PRELOAD_ONELEVEL, &gconf_error);
		if (gconf_error)
			g_error ("gconf error: %s\n", gconf_error->str);
		gtk_html_class_properties_load (klass->properties, gconf_client);
	} else
		g_error ("gconf is not initialized, please call gconf_init before using GtkHTML library\n");
#else
		gtk_html_class_properties_load (klass->properties);
#endif
	load_keybindings (klass);
#ifdef GTKHTML_HAVE_GCONF
	gconf_client_notify_add (gconf_client, GTK_HTML_GCONF_DIR, client_notify_class, klass, NULL, &gconf_error);
	if (gconf_error)
		g_warning ("gconf error: %s\n", gconf_error->str);
#endif
}

static gint
focus (GtkContainer *c, GtkDirectionType direction)
{
	gint rv;

	GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (c), GTK_CAN_FOCUS);
	rv = (*GTK_CONTAINER_CLASS (parent_class)->focus) (c, direction);
	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (c), GTK_CAN_FOCUS);

	return rv;
}

static void
class_init (GtkHTMLClass *klass)
{
	GtkHTMLClass      *html_class;
	GtkWidgetClass    *widget_class;
	GtkObjectClass    *object_class;
	GtkLayoutClass    *layout_class;
	GtkContainerClass *container_class;
	
	html_class = (GtkHTMLClass *)klass;
	widget_class = (GtkWidgetClass *) klass;
	object_class = (GtkObjectClass *) klass;
	layout_class = (GtkLayoutClass *) klass;
	container_class = (GtkContainerClass *) klass;

	object_class->destroy = destroy;

	parent_class = gtk_type_class (GTK_TYPE_LAYOUT);

	signals [TITLE_CHANGED] = 
	  gtk_signal_new ("title_changed",
			  GTK_RUN_FIRST,
			  object_class->type,
			  GTK_SIGNAL_OFFSET (GtkHTMLClass, title_changed),
			  gtk_marshal_NONE__STRING,
			  GTK_TYPE_NONE, 1,
			  GTK_TYPE_STRING);
	signals [URL_REQUESTED] =
	  gtk_signal_new ("url_requested",
			  GTK_RUN_FIRST,
			  object_class->type,
			  GTK_SIGNAL_OFFSET (GtkHTMLClass, url_requested),
			  gtk_marshal_NONE__POINTER_POINTER,
			  GTK_TYPE_NONE, 2,
			  GTK_TYPE_STRING,
			  GTK_TYPE_POINTER);
	signals [LOAD_DONE] = 
	  gtk_signal_new ("load_done",
			  GTK_RUN_FIRST,
			  object_class->type,
			  GTK_SIGNAL_OFFSET (GtkHTMLClass, load_done),
			  gtk_marshal_NONE__NONE,
			  GTK_TYPE_NONE, 0);
	signals [LINK_CLICKED] =
	  gtk_signal_new ("link_clicked",
			  GTK_RUN_FIRST,
			  object_class->type,
			  GTK_SIGNAL_OFFSET (GtkHTMLClass, link_clicked),
			  gtk_marshal_NONE__STRING,
			  GTK_TYPE_NONE, 1,
			  GTK_TYPE_STRING);
	signals [SET_BASE] =
		gtk_signal_new ("set_base",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkHTMLClass, set_base),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);
	signals [SET_BASE_TARGET] =
		gtk_signal_new ("set_base_target",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkHTMLClass, set_base_target),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);
	
	signals [ON_URL] =
		gtk_signal_new ("on_url",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkHTMLClass, on_url),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);
	
	signals [REDIRECT] =
		gtk_signal_new ("redirect",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkHTMLClass, redirect),
				gtk_marshal_NONE__POINTER_INT,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_STRING,
				GTK_TYPE_INT);
	
	signals [SUBMIT] =
		gtk_signal_new ("submit",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkHTMLClass, submit),
				gtk_marshal_NONE__POINTER_POINTER_POINTER,
				GTK_TYPE_NONE, 3,
				GTK_TYPE_STRING,
				GTK_TYPE_STRING,
				GTK_TYPE_STRING);

	signals [OBJECT_REQUESTED] =
		gtk_signal_new ("object_requested",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkHTMLClass, object_requested),
				gtk_marshal_BOOL__POINTER,
				GTK_TYPE_BOOL, 1,
				GTK_TYPE_OBJECT);
	
	signals [CURRENT_PARAGRAPH_STYLE_CHANGED] =
		gtk_signal_new ("current_paragraph_style_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkHTMLClass, current_paragraph_style_changed),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_INT);

	signals [CURRENT_PARAGRAPH_INDENTATION_CHANGED] =
		gtk_signal_new ("current_paragraph_indentation_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkHTMLClass, current_paragraph_indentation_changed),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_INT);

	signals [CURRENT_PARAGRAPH_ALIGNMENT_CHANGED] =
		gtk_signal_new ("current_paragraph_alignment_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkHTMLClass, current_paragraph_alignment_changed),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_INT);

	signals [INSERTION_FONT_STYLE_CHANGED] =
		gtk_signal_new ("insertion_font_style_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkHTMLClass, insertion_font_style_changed),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_INT);
	
	signals [INSERTION_COLOR_CHANGED] =
		gtk_signal_new ("insertion_color_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkHTMLClass, insertion_color_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	
	signals [SIZE_CHANGED] = 
		gtk_signal_new ("size_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkHTMLClass, size_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	/* signals for keybindings */
	signals [SCROLL] =
		gtk_signal_new ("scroll",
				GTK_RUN_LAST | GTK_RUN_ACTION,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkHTMLClass, scroll),
				gtk_marshal_NONE__ENUM_FLOAT,
				GTK_TYPE_NONE, 3,
				GTK_TYPE_ORIENTATION,
				GTK_TYPE_SCROLL_TYPE, GTK_TYPE_FLOAT);

	signals [CURSOR_MOVE] =
		gtk_signal_new ("cursor_move",
				GTK_RUN_LAST | GTK_RUN_ACTION,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkHTMLClass, cursor_move),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_DIRECTION_TYPE, GTK_TYPE_HTML_CURSOR_SKIP);

	signals [INSERT_PARAGRAPH] =
		gtk_signal_new ("command",
				GTK_RUN_LAST | GTK_RUN_ACTION,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkHTMLClass, command),
				gtk_marshal_NONE__ENUM,
				GTK_TYPE_NONE, 1, GTK_TYPE_HTML_COMMAND);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	object_class->destroy = destroy;
	
	widget_class->realize = realize;
	widget_class->unrealize = unrealize;
	widget_class->style_set = style_set;
	widget_class->draw = draw;
	widget_class->key_press_event = key_press_event;
	widget_class->expose_event  = expose;
	widget_class->size_allocate = size_allocate;
	widget_class->motion_notify_event = motion_notify_event;
	widget_class->button_press_event = button_press_event;
	widget_class->button_release_event = button_release_event;
	widget_class->focus_in_event = focus_in_event;
	widget_class->focus_out_event = focus_out_event;
	widget_class->enter_notify_event = enter_notify_event;
	widget_class->selection_get = selection_get;
	widget_class->selection_received = selection_received;
	widget_class->selection_clear_event = selection_clear_event;

	container_class->focus = focus;

	layout_class->set_scroll_adjustments = set_adjustments;

	html_class->scroll            = scroll;
	html_class->cursor_move       = cursor_move;
	html_class->command           = command;

	init_properties (klass);
}

static void
init_properties_widget (GtkHTML *html)
{
	GtkHTMLClassProperties *prop;

	/* printf ("init_properties_widget\n"); */
	prop = GTK_HTML_CLASS (GTK_OBJECT (html)->klass)->properties;
	set_fonts_idle (html);
	html_colorset_set_color (html->engine->defaultSettings->color_set, &prop->spell_error_color, HTMLSpellErrorColor);

#ifdef GTKHTML_HAVE_GCONF
	gconf_client_notify_add (gconf_client, GTK_HTML_GCONF_DIR, client_notify_widget, html, NULL, &gconf_error);
	if (gconf_error)
		g_warning ("gconf error: %s\n", gconf_error->str);
#endif
}

static void
init (GtkHTML* html)
{
	static const GtkTargetEntry targets[] = {
		{ "STRING", 0, TARGET_STRING },
		{ "TEXT",   0, TARGET_TEXT }, 
		{ "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT },
	};
	static const gint n_targets = sizeof(targets) / sizeof(targets[0]);

	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (html), GTK_CAN_FOCUS);
	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (html), GTK_APP_PAINTABLE);

	html->editor_bindings = NULL;

	html->debug = FALSE;
	html->allow_selection = TRUE;

	html->pointer_url = NULL;
	html->hand_cursor = gdk_cursor_new (GDK_HAND2);
	html->arrow_cursor = gdk_cursor_new (GDK_LEFT_PTR);
	html->ibeam_cursor = gdk_cursor_new (GDK_XTERM);
	html->hadj_connection = 0;
	html->vadj_connection = 0;

	html->selection_x1 = 0;
	html->selection_y1 = 0;

	html->in_selection = FALSE;
	html->button1_pressed = FALSE;

	html->load_in_progress = TRUE;

	html->priv = g_new0 (GtkHTMLPrivate, 1);
	html->priv->idle_handler_id = 0;
	html->priv->scroll_timeout_id = 0;
	html->priv->paragraph_style = GTK_HTML_PARAGRAPH_STYLE_NORMAL;
	html->priv->paragraph_alignment = GTK_HTML_PARAGRAPH_ALIGNMENT_LEFT;
	html->priv->paragraph_indentation = 0;
	html->priv->insertion_font_style = GTK_HTML_FONT_STYLE_DEFAULT;

	gtk_selection_add_targets (GTK_WIDGET (html),
				   GDK_SELECTION_PRIMARY,
				   targets, n_targets);
}


guint
gtk_html_get_type (void)
{
	static guint html_type = 0;

	if (!html_type) {
		static const GtkTypeInfo html_info = {
			"GtkHTML",
			sizeof (GtkHTML),
			sizeof (GtkHTMLClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		html_type = gtk_type_unique (GTK_TYPE_LAYOUT, &html_info);
	}

	return html_type;
}

GtkWidget *
gtk_html_new (void)
{
	GtkWidget *html;

	html = gtk_type_new (gtk_html_get_type ());
	gtk_html_construct (html);
	return html;
}

void
gtk_html_construct (GtkWidget *htmlw)
{
	GtkHTML *html;

	g_return_if_fail (htmlw != NULL);
	g_return_if_fail (GTK_IS_HTML (htmlw));

	html = GTK_HTML (htmlw);

	html->engine        = html_engine_new (htmlw);
	html->input_line    = gtk_html_input_line_new (html);
	html->iframe_parent = NULL;
	
	gtk_signal_connect (GTK_OBJECT (html->engine), "title_changed",
			    GTK_SIGNAL_FUNC (html_engine_title_changed_cb), html);
	gtk_signal_connect (GTK_OBJECT (html->engine), "set_base",
			    GTK_SIGNAL_FUNC (html_engine_set_base_cb), html);
	gtk_signal_connect (GTK_OBJECT (html->engine), "set_base_target",
			    GTK_SIGNAL_FUNC (html_engine_set_base_target_cb), html);
	gtk_signal_connect (GTK_OBJECT (html->engine), "load_done",
			    GTK_SIGNAL_FUNC (html_engine_load_done_cb), html);
	gtk_signal_connect (GTK_OBJECT (html->engine), "url_requested",
			    GTK_SIGNAL_FUNC (html_engine_url_requested_cb), html);
	gtk_signal_connect (GTK_OBJECT (html->engine), "draw_pending",
			    GTK_SIGNAL_FUNC (html_engine_draw_pending_cb), html);
	gtk_signal_connect (GTK_OBJECT (html->engine), "redirect",
			    GTK_SIGNAL_FUNC (html_engine_redirect_cb), html);
	gtk_signal_connect (GTK_OBJECT (html->engine), "submit",
			    GTK_SIGNAL_FUNC (html_engine_submit_cb), html);
	gtk_signal_connect (GTK_OBJECT (html->engine), "object_requested",
			    GTK_SIGNAL_FUNC (html_engine_object_requested_cb), html);

	init_properties_widget (html);
}


void
gtk_html_enable_debug (GtkHTML *html,
		       gboolean debug)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html->debug = debug;
}


void
gtk_html_allow_selection (GtkHTML *html,
			  gboolean allow)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html->allow_selection = allow;
}



/* #define LOG_INPUT */
#ifdef LOG_INPUT
static FILE *log_file;
#endif

GtkHTMLStream *
gtk_html_begin (GtkHTML *html)
{
	GtkHTMLStream *handle;

#ifdef LOG_INPUT
	log_file = fopen ("gtkhtml.log", "w+");
#endif
	g_return_val_if_fail (! gtk_html_get_editable (html), NULL);

	handle = html_engine_begin (html->engine);
	if (handle == NULL)
		return NULL;

	html_engine_parse (html->engine);

	html->load_in_progress = TRUE;

	return handle;
}

void
gtk_html_write (GtkHTML *html,
		GtkHTMLStream *handle,
		const gchar *buffer,
		size_t size)
{
#ifdef LOG_INPUT
	gint i;
	for (i=0; i<size; i++)
		fprintf (log_file, "%c", buffer [i]);
#endif
	gtk_html_stream_write (handle, buffer, size);
}

void
gtk_html_end (GtkHTML *html,
	      GtkHTMLStream *handle,
	      GtkHTMLStreamStatus status)
{
#ifdef LOG_INPUT
	fclose (log_file);
#endif
	gtk_html_stream_close (handle, status);

	html->load_in_progress = FALSE;
}


const gchar *
gtk_html_get_title (GtkHTML *html)
{
	g_return_val_if_fail (html != NULL, NULL);
	g_return_val_if_fail (GTK_IS_HTML (html), NULL);

	if (html->engine->title == NULL)
		return NULL;

	return html->engine->title->str;
}


gboolean
gtk_html_jump_to_anchor (GtkHTML *html,
			 const gchar *anchor)
{
	g_return_val_if_fail (html != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);
	
	return html_engine_goto_anchor (html->engine, anchor);
}


gboolean
gtk_html_save (GtkHTML *html,
	       GtkHTMLSaveReceiverFn receiver,
	       gpointer data)
{
	g_return_val_if_fail (html != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);
	g_return_val_if_fail (receiver != NULL, FALSE);
	
	return html_engine_save (html->engine, receiver, data);
}

gboolean
gtk_html_export (GtkHTML *html,
		 const char *type,
		 GtkHTMLSaveReceiverFn receiver,
		 gpointer data)
{
	g_return_val_if_fail (html != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);
	g_return_val_if_fail (receiver != NULL, FALSE);
	
	if (strcmp (type, "text/html") == 0) {
		return html_engine_save (html->engine, receiver, data);
	} else if (strcmp (type, "text/plain") == 0) {
		return html_engine_save_plain (html->engine, receiver,
					       data);  
	} else {
		return FALSE;
	}
}


void
gtk_html_private_calc_scrollbars (GtkHTML *html)
{
	GtkLayout *layout;
	GtkAdjustment *vadj, *hadj;
	gint width, height;

	height = html_engine_get_doc_height (html->engine);
	width = html_engine_get_doc_width (html->engine);

	layout = GTK_LAYOUT (html);
	hadj = layout->hadjustment;
	vadj = layout->vadjustment;

	vadj->lower = 0;
	vadj->upper = height;
	vadj->page_size = html->engine->height;
	vadj->step_increment = 14; /* FIXME */
	vadj->page_increment = html->engine->height;

	hadj->lower = 0.0;
	hadj->upper = width;
	hadj->page_size = html->engine->width;
	hadj->step_increment = 14; /* FIXME */
	hadj->page_increment = html->engine->width;

	if ((width != layout->width) || (height != layout->height)) {
		gtk_signal_emit (GTK_OBJECT (html), signals[SIZE_CHANGED]);
		gtk_layout_set_size (layout, width, height);
	}
}


void
gtk_html_set_editable (GtkHTML *html,
		       gboolean editable)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_set_editable (html->engine, editable);
	set_editor_keybindings (html, editable);

	if (editable)
		update_styles (html);
}

gboolean
gtk_html_get_editable  (const GtkHTML *html)
{
	g_return_val_if_fail (html != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_HTML (html), FALSE);

	return html_engine_get_editable (html->engine);
}

void
gtk_html_load_empty (GtkHTML *html)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_load_empty (html->engine);
}


/* Printing.  */

void
gtk_html_print (GtkHTML *html,
		GnomePrintContext *print_context)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_print (html->engine, print_context);
}

void
gtk_html_print_with_header_footer (GtkHTML *html, GnomePrintContext *print_context,
				   gdouble header_height, gdouble footer_height,
				   GtkHTMLPrintCallback header_print, GtkHTMLPrintCallback footer_print)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_print_with_header_footer (html->engine, print_context,
					      header_height, footer_height, header_print, footer_print);
}


/* Editing.  */

void
gtk_html_set_paragraph_style (GtkHTML *html,
			      GtkHTMLParagraphStyle style)
{
	HTMLClueFlowStyle current_style;
	HTMLClueFlowStyle clueflow_style;

	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	/* FIXME precondition: check if it's a valid style.  */

	clueflow_style = paragraph_style_to_clueflow_style (style);

	current_style = html_engine_get_current_clueflow_style (html->engine);
	if (current_style == clueflow_style)
		return;

	if (! html_engine_set_clueflow_style (html->engine, clueflow_style, 0, 0,
					      HTML_ENGINE_SET_CLUEFLOW_STYLE, TRUE))
		return;

	html->priv->paragraph_style = style;

	gtk_signal_emit (GTK_OBJECT (html), signals[CURRENT_PARAGRAPH_STYLE_CHANGED],
			 style);
	queue_draw (html);
}

GtkHTMLParagraphStyle
gtk_html_get_paragraph_style (GtkHTML *html)
{
	return html_engine_get_current_clueflow_style (html->engine);
}

void
gtk_html_indent (GtkHTML *html,
		 gint delta)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_set_clueflow_style (html->engine, 0, 0, delta,
					HTML_ENGINE_SET_CLUEFLOW_INDENTATION, TRUE);

	update_styles (html);
}

void
gtk_html_set_font_style (GtkHTML *html,
			 GtkHTMLFontStyle and_mask,
			 GtkHTMLFontStyle or_mask)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	if (html_engine_set_font_style (html->engine, and_mask, or_mask))
		gtk_signal_emit (GTK_OBJECT (html), signals [INSERTION_FONT_STYLE_CHANGED],
				 html->engine->insertion_font_style);
}

void
gtk_html_toggle_font_style (GtkHTML *html,
			    GtkHTMLFontStyle style)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	if (html_engine_toggle_font_style (html->engine, style))
		gtk_signal_emit (GTK_OBJECT (html), signals [INSERTION_FONT_STYLE_CHANGED],
				 html->engine->insertion_font_style);
}

GtkHTMLParagraphAlignment
gtk_html_get_paragraph_alignment (GtkHTML *html)
{
	return paragraph_alignment_to_html (html_engine_get_current_clueflow_alignment (html->engine));
}

void
gtk_html_set_paragraph_alignment (GtkHTML *html,
			  GtkHTMLParagraphAlignment alignment)
{
	HTMLHAlignType align;

	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	align = paragraph_alignment_to_html (alignment);

	html_engine_set_clueflow_style (html->engine, 0, align, 0,
					HTML_ENGINE_SET_CLUEFLOW_ALIGNMENT, TRUE);
}


/* Clipboard operations.  */

void
gtk_html_cut (GtkHTML *html)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_cut (html->engine, TRUE);
}

void
gtk_html_copy (GtkHTML *html)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_copy (html->engine);
}

void
gtk_html_paste (GtkHTML *html)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_paste (html->engine, TRUE);
}


/* Undo/redo.  */

void
gtk_html_undo (GtkHTML *html)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_undo (html->engine);
}

void
gtk_html_redo (GtkHTML *html)
{
	g_return_if_fail (html != NULL);
	g_return_if_fail (GTK_IS_HTML (html));

	html_engine_redo (html->engine);
}

/* misc utils */

void
gtk_html_set_default_background_color (GtkHTML *html, GdkColor *c)
{
	html_colorset_set_color (html->engine->defaultSettings->color_set, c, HTMLBgColor);
}

/*******************************************

   keybindings

*/

static void
scroll (GtkHTML *html,
	GtkOrientation orientation,
	GtkScrollType  scroll_type,
	gfloat         position)
{
	GtkAdjustment *adj;
	gint line_height;
	gfloat delta;

	/* we dont want scroll in editable (move cursor instead) */
	if (html_engine_get_editable (html->engine))
		return;

	adj = (orientation == GTK_ORIENTATION_VERTICAL)
		? gtk_layout_get_vadjustment (GTK_LAYOUT (html)) : gtk_layout_get_hadjustment (GTK_LAYOUT (html));


	line_height = (html->engine && adj->page_increment > html->engine->painter->font_manager.var_size)
		? html->engine->painter->font_manager.var_size
		: 0;

	switch (scroll_type) {
	case GTK_SCROLL_STEP_FORWARD:
		delta = adj->step_increment;
		break;
	case GTK_SCROLL_STEP_BACKWARD:
		delta = -adj->step_increment;
		break;
	case GTK_SCROLL_PAGE_FORWARD:
		delta = adj->page_increment - line_height;
		break;
	case GTK_SCROLL_PAGE_BACKWARD:
		delta = -adj->page_increment + line_height;
		break;
	default:
		delta = 0.0;
		g_assert_not_reached ();
	}

	adj->value = CLAMP (adj->value + delta, adj->lower, adj->upper - adj->page_size);
	gtk_adjustment_value_changed (adj);

	html->binding_handled = TRUE;
}

static void
scroll_by_amount (GtkHTML *html,
		  gint amount)
{
	GtkLayout *layout;
	GtkAdjustment *adj;
	gfloat new_value;

	layout = GTK_LAYOUT (html);
	adj = layout->vadjustment;

	new_value = adj->value + (gfloat) amount;
	if (new_value < adj->lower)
		new_value = adj->lower;
	else if (new_value > adj->upper)
		new_value = adj->upper;

	gtk_adjustment_set_value (adj, new_value);
}

static void
cursor_move (GtkHTML *html, GtkDirectionType dir_type, GtkHTMLCursorSkipType skip)
{
	gint amount;

	if (!html_engine_get_editable (html->engine))
		return;

	if (html->engine->shift_selection)
		html_engine_disable_selection (html->engine);

	switch (skip) {
	case GTK_HTML_CURSOR_SKIP_ONE:
		switch (dir_type) {
		case GTK_DIR_LEFT:
			html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_LEFT, 1);
			break;
		case GTK_DIR_RIGHT:
			html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_RIGHT, 1);
			break;
		case GTK_DIR_UP:
			html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_UP, 1);
			break;
		case GTK_DIR_DOWN:
			html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_DOWN, 1);
			break;
		default:
			g_assert_not_reached ();
		}
		break;
	case GTK_HTML_CURSOR_SKIP_WORD:
		switch (dir_type) {
		case GTK_DIR_UP:
		case GTK_DIR_LEFT:
			html_engine_backward_word (html->engine);
			break;
		case GTK_DIR_DOWN:
		case GTK_DIR_RIGHT:
			html_engine_forward_word (html->engine);
			break;
		default:
			g_assert_not_reached ();
		}
		break;
	case GTK_HTML_CURSOR_SKIP_PAGE:
		switch (dir_type) {
		case GTK_DIR_UP:
		case GTK_DIR_LEFT:
			if ((amount = html_engine_scroll_up (html->engine, GTK_WIDGET (html)->allocation.height)) > 0)
				scroll_by_amount (html, - amount);
			break;
		case GTK_DIR_DOWN:
		case GTK_DIR_RIGHT:
			if ((amount = html_engine_scroll_down (html->engine, GTK_WIDGET (html)->allocation.height)) > 0)
				scroll_by_amount (html, amount);
			break;
		default:
			g_assert_not_reached ();
		}
		break;
	case GTK_HTML_CURSOR_SKIP_ALL:
		switch (dir_type) {
		case GTK_DIR_LEFT:
			html_engine_beginning_of_line (html->engine);
			break;
		case GTK_DIR_RIGHT:
			html_engine_end_of_line (html->engine);
			break;
		case GTK_DIR_UP:
			html_engine_beginning_of_document (html->engine);
			break;
		case GTK_DIR_DOWN:
			html_engine_end_of_document (html->engine);
			break;
		default:
			g_assert_not_reached ();
		}
		break;
	default:
		g_assert_not_reached ();
	}

	html->binding_handled = TRUE;
}

static void
move_selection (GtkHTML *html, GtkHTMLCommandType com_type)
{
	gint amount;

	if (!html_engine_get_editable (html->engine))
		return;

	html->engine->shift_selection = TRUE;
	if (!html->engine->mark)
		html_engine_set_mark (html->engine);
	switch (com_type) {
	case GTK_HTML_COMMAND_MODIFY_SELECTION_UP:
		html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_UP, 1);
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_DOWN:
		html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_DOWN, 1);
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_LEFT:
		html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_LEFT, 1);
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_RIGHT:
		html_engine_move_cursor (html->engine, HTML_ENGINE_CURSOR_RIGHT, 1);
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_BOL:
		html_engine_beginning_of_line (html->engine);
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_EOL:
		html_engine_end_of_line (html->engine);
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_BOD:
		html_engine_beginning_of_document (html->engine);
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_EOD:
		html_engine_end_of_document (html->engine);
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_PAGEUP:
		if ((amount = html_engine_scroll_up (html->engine, GTK_WIDGET (html)->allocation.height)) > 0)
			scroll_by_amount (html, - amount);
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_PAGEDOWN:
		if ((amount = html_engine_scroll_down (html->engine, GTK_WIDGET (html)->allocation.height)) > 0)
			scroll_by_amount (html, amount);
		break;
	default:
		g_assert_not_reached ();
	}

	html->binding_handled = TRUE;
}

static void
command (GtkHTML *html, GtkHTMLCommandType com_type)
{
	HTMLEngine *e = html->engine;

	/* printf ("command %d\n", com_type); */
	html->binding_handled = TRUE;

	/* non-editable + editable commands */
	switch (com_type) {
	case GTK_HTML_COMMAND_SEARCH_INCREMENTAL_FORWARD:
		gtk_html_isearch (html, TRUE);
		break;
	case GTK_HTML_COMMAND_SEARCH_INCREMENTAL_BACKWARD:
		gtk_html_isearch (html, FALSE);
		break;
	case GTK_HTML_COMMAND_FOCUS_FORWARD:
		html->binding_handled = gtk_container_focus (GTK_CONTAINER (html), GTK_DIR_TAB_FORWARD);
		break;
	case GTK_HTML_COMMAND_FOCUS_BACKWARD:
		html->binding_handled = gtk_container_focus (GTK_CONTAINER (html), GTK_DIR_TAB_BACKWARD);
		break;
	default:
		html->binding_handled = FALSE;
	}

	if (!html_engine_get_editable (e))
		return;

	/* editable commands only */
	switch (com_type) {
	case GTK_HTML_COMMAND_UNDO:
		html_engine_undo (e);
		break;
	case GTK_HTML_COMMAND_REDO:
		html_engine_redo (e);
		break;
	case GTK_HTML_COMMAND_COPY:
		if (e->active_selection)
			html_engine_copy (e);
		break;
	case GTK_HTML_COMMAND_CUT:
		html_engine_cut (e, TRUE);
		break;
	case GTK_HTML_COMMAND_CUT_LINE:
		html_engine_cut_line (e, TRUE);
		break;
	case GTK_HTML_COMMAND_PASTE:
		if (e->cut_buffer)
			html_engine_paste (e, TRUE);
		break;
	case GTK_HTML_COMMAND_INSERT_PARAGRAPH:
		html_engine_delete_selection (e, TRUE);
		html_engine_insert (e, "\n", 1);
		break;
	case GTK_HTML_COMMAND_DELETE:
		if (e->mark != NULL
		    && e->mark->position != e->cursor->position)
			html_engine_delete_selection (e, TRUE);
		else
			html_engine_delete (e, 1, TRUE, FALSE);
		break;
	case GTK_HTML_COMMAND_DELETE_BACK:
		if (e->mark != NULL
		    && e->mark->position != e->cursor->position)
			html_engine_delete_selection (e, TRUE);
		else
			html_engine_delete (e, 1, TRUE, TRUE);
		break;
	case GTK_HTML_COMMAND_SET_MARK:
		html_engine_set_mark (e);
		break;
	case GTK_HTML_COMMAND_DISABLE_SELECTION:
		html_engine_disable_selection (e);
		break;
	case GTK_HTML_COMMAND_TOGGLE_BOLD:
		gtk_html_toggle_font_style (html, GTK_HTML_FONT_STYLE_BOLD);
		break;
	case GTK_HTML_COMMAND_TOGGLE_ITALIC:
		gtk_html_toggle_font_style (html, GTK_HTML_FONT_STYLE_ITALIC);
		break;
	case GTK_HTML_COMMAND_TOGGLE_UNDERLINE:
		gtk_html_toggle_font_style (html, GTK_HTML_FONT_STYLE_UNDERLINE);
		break;
	case GTK_HTML_COMMAND_TOGGLE_STRIKEOUT:
		gtk_html_toggle_font_style (html, GTK_HTML_FONT_STYLE_STRIKEOUT);
		break;
	case GTK_HTML_COMMAND_SIZE_MINUS_2:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_SIZE_MASK, GTK_HTML_FONT_STYLE_SIZE_1);
		break;
	case GTK_HTML_COMMAND_SIZE_MINUS_1:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_SIZE_MASK, GTK_HTML_FONT_STYLE_SIZE_2);
		break;
	case GTK_HTML_COMMAND_SIZE_PLUS_0:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_SIZE_MASK, GTK_HTML_FONT_STYLE_SIZE_3);
		break;
	case GTK_HTML_COMMAND_SIZE_PLUS_1:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_SIZE_MASK, GTK_HTML_FONT_STYLE_SIZE_4);
		break;
	case GTK_HTML_COMMAND_SIZE_PLUS_2:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_SIZE_MASK, GTK_HTML_FONT_STYLE_SIZE_5);
		break;
	case GTK_HTML_COMMAND_SIZE_PLUS_3:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_SIZE_MASK, GTK_HTML_FONT_STYLE_SIZE_6);
		break;
	case GTK_HTML_COMMAND_SIZE_PLUS_4:
		gtk_html_set_font_style (html, ~GTK_HTML_FONT_STYLE_SIZE_MASK, GTK_HTML_FONT_STYLE_SIZE_7);
		break;
	case GTK_HTML_COMMAND_SIZE_INCREASE:
		html_engine_font_size_inc_dec (e, TRUE);
		break;
	case GTK_HTML_COMMAND_SIZE_DECREASE:
		html_engine_font_size_inc_dec (e, FALSE);
		break;
	case GTK_HTML_COMMAND_ALIGN_LEFT:
		gtk_html_set_paragraph_alignment (html, GTK_HTML_PARAGRAPH_ALIGNMENT_LEFT);
		break;
	case GTK_HTML_COMMAND_ALIGN_CENTER:
		gtk_html_set_paragraph_alignment (html, GTK_HTML_PARAGRAPH_ALIGNMENT_CENTER);
		break;
	case GTK_HTML_COMMAND_ALIGN_RIGHT:
		gtk_html_set_paragraph_alignment (html, GTK_HTML_PARAGRAPH_ALIGNMENT_RIGHT);
		break;
	case GTK_HTML_COMMAND_INDENT_INC:
		gtk_html_indent (html, +1);
		break;
	case GTK_HTML_COMMAND_INDENT_DEC:
		gtk_html_indent (html, -1);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_NORMAL:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_NORMAL);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_H1:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_H1);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_H2:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_H2);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_H3:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_H3);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_H4:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_H4);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_H5:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_H5);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_H6:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_H6);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_PRE:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_PRE);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_ADDRESS:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_ADDRESS);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_ITEMDOTTED:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_ITEMDOTTED);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_ITEMROMAN:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_ITEMROMAN);
		break;
	case GTK_HTML_COMMAND_PARAGRAPH_STYLE_ITEMDIGIT:
		gtk_html_set_paragraph_style (html, GTK_HTML_PARAGRAPH_STYLE_ITEMDIGIT);
		break;
	case GTK_HTML_COMMAND_MODIFY_SELECTION_UP:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_DOWN:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_LEFT:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_RIGHT:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_BOL:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_EOL:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_BOD:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_EOD:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_PAGEUP:
	case GTK_HTML_COMMAND_MODIFY_SELECTION_PAGEDOWN:
		move_selection (html, com_type);
		break;
	case GTK_HTML_COMMAND_CAPITALIZE_WORD:
		html_engine_capitalize_word (e);
		break;
	case GTK_HTML_COMMAND_UPCASE_WORD:
		html_engine_upcase_downcase_word (e, TRUE);
		break;
	case GTK_HTML_COMMAND_DOWNCASE_WORD:
		html_engine_upcase_downcase_word (e, FALSE);
		break;
	case GTK_HTML_COMMAND_SPELL_SUGGEST:
		if (html->spell_api && !html_engine_word_is_valid (e))
			(*html->spell_api->suggestion_request) (html, html_engine_get_word (e));
		break;
	case GTK_HTML_COMMAND_SPELL_PERSONAL_DICTIONARY_ADD:
	case GTK_HTML_COMMAND_SPELL_SESSION_DICTIONARY_ADD:
		if (html_engine_get_word (e) && html->spell_api) {
			if (com_type == GTK_HTML_COMMAND_SPELL_PERSONAL_DICTIONARY_ADD)
				(*html->spell_api->add_to_personal) (html, html_engine_get_word (e));
			else
				(*html->spell_api->add_to_session) (html, html_engine_get_word (e));
			html_engine_spell_check (e);
			gtk_widget_queue_draw (GTK_WIDGET (html));
		}
		break;
	default:
		html->binding_handled = FALSE;
	}
}

/*
  default keybindings:

  Viewer:

  Up/Down ............................. scroll one line up/down
  PageUp/PageDown ..................... scroll one page up/down
  Space/BackSpace ..................... scroll one page up/down

  Left/Right .......................... scroll one char left/right
  Shift Left/Right .................... scroll more left/right

  Editor:

  Up/Down ............................. move cursor one line up/down
  PageUp/PageDown ..................... move cursor one page up/down

  Return .............................. insert paragraph
  Delete .............................. delete one char
  BackSpace ........................... delete one char backwards

*/

static void
clean_bindings_set (GtkBindingSet *binding_set)
{
	GtkBindingEntry *cur;
	GList *mods, *vals, *cm, *cv;

	if (!binding_set)
		return;

	mods = NULL;
	vals = NULL;
	cur = binding_set->entries;
	while (cur) {
		mods = g_list_prepend (mods, GUINT_TO_POINTER (cur->modifiers));
		vals = g_list_prepend (vals, GUINT_TO_POINTER (cur->keyval));
		cur = cur->set_next;
	}
	cm = mods; cv = vals;
	while (cm) {
		gtk_binding_entry_remove (binding_set, GPOINTER_TO_UINT (cv->data), GPOINTER_TO_UINT (cm->data));
		cv = cv->next; cm = cm->next;
	}
	g_list_free (mods);
	g_list_free (vals);
}

static void
set_editor_keybindings (GtkHTML *html, gboolean editable)
{
	if (editable) {
		gchar *name;

		name = g_strconcat ("gtkhtml-bindings-",
				    GTK_HTML_CLASS (GTK_OBJECT (html)->klass)->properties->keybindings_theme, NULL);
		html->editor_bindings = gtk_binding_set_find (name);
		if (!html->editor_bindings)
			g_warning ("cannot find %s bindings", name);
		g_free (name);
	} else
		html->editor_bindings = NULL;
}

static void
load_bindings_from_file (gboolean from_share, gchar *name)
{
	gchar *rcfile;

	rcfile = g_strconcat ((from_share ? PREFIX "/share/gtkhtml/" : gnome_util_user_home ()),
			      (from_share ? "" : "/.gnome/"), name, NULL);
	if (g_file_exists (rcfile))
		gtk_rc_parse (rcfile);
	g_free (rcfile);
}

static void
load_keybindings (GtkHTMLClass *klass)
{
	GtkBindingSet *binding_set;

	/* FIXME add to gtk gtk_binding_set_clear & gtk_binding_set_remove_path */
	clean_bindings_set (gtk_binding_set_by_class (klass));
	clean_bindings_set (gtk_binding_set_find ("gtkhtml-bindings-emacs"));
	clean_bindings_set (gtk_binding_set_find ("gtkhtml-bindings-ms"));
	clean_bindings_set (gtk_binding_set_find ("gtkhtml-bindings-custom"));

	/* ensure enums are defined */
	gtk_html_cursor_skip_get_type ();
	gtk_html_command_get_type ();

	load_bindings_from_file (TRUE,  "keybindingsrc.emacs");
	load_bindings_from_file (TRUE,  "keybindingsrc.ms");
	load_bindings_from_file (FALSE, "gtkhtml-bindings-custom");

	binding_set = gtk_binding_set_by_class (klass);

	/* layout scrolling */
#define BSCROLL(m,key,orient,sc) \
	gtk_binding_entry_add_signal (binding_set, GDK_ ## key, m, \
				      "scroll", 3, \
				      GTK_TYPE_ORIENTATION, GTK_ORIENTATION_ ## orient, \
				      GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_ ## sc, \
				      GTK_TYPE_FLOAT, 0.0); \

	BSCROLL (0, Up, VERTICAL, STEP_BACKWARD);
	BSCROLL (0, KP_Up, VERTICAL, STEP_BACKWARD);
	BSCROLL (0, Down, VERTICAL, STEP_FORWARD);
	BSCROLL (0, KP_Down, VERTICAL, STEP_FORWARD);

	BSCROLL (0, Left, HORIZONTAL, STEP_BACKWARD);
	BSCROLL (0, KP_Left, HORIZONTAL, STEP_BACKWARD);
	BSCROLL (0, Right, HORIZONTAL, STEP_FORWARD);
	BSCROLL (0, KP_Right, HORIZONTAL, STEP_FORWARD);

	BSCROLL (0, Page_Up, VERTICAL, PAGE_BACKWARD);
	BSCROLL (0, KP_Page_Up, VERTICAL, PAGE_BACKWARD);
	BSCROLL (0, Page_Down, VERTICAL, PAGE_FORWARD);
	BSCROLL (0, KP_Page_Down, VERTICAL, PAGE_FORWARD);
	BSCROLL (0, BackSpace, VERTICAL, PAGE_BACKWARD);
	BSCROLL (0, space, VERTICAL, PAGE_FORWARD);

	BSCROLL (GDK_SHIFT_MASK, Left, HORIZONTAL, PAGE_BACKWARD);
	BSCROLL (GDK_SHIFT_MASK, KP_Left, HORIZONTAL, PAGE_BACKWARD);
	BSCROLL (GDK_SHIFT_MASK, Right, HORIZONTAL, PAGE_FORWARD);
	BSCROLL (GDK_SHIFT_MASK, KP_Right, HORIZONTAL, PAGE_FORWARD);

	/* editing */

#define BMOVE(m,key,dir,sk) \
	gtk_binding_entry_add_signal (binding_set, GDK_ ## key, m, \
				      "cursor_move", 2, \
				      GTK_TYPE_DIRECTION_TYPE, GTK_DIR_ ## dir, \
				      GTK_TYPE_HTML_CURSOR_SKIP, GTK_HTML_CURSOR_SKIP_ ## sk);

	BMOVE (0, Left,     LEFT,  ONE);
	BMOVE (0, KP_Left,  LEFT,  ONE);
	BMOVE (0, Right,    RIGHT, ONE);
	BMOVE (0, KP_Right, RIGHT, ONE);
	BMOVE (0, Up,       UP  ,  ONE);
	BMOVE (0, KP_Up,    UP  ,  ONE);
	BMOVE (0, Down,     DOWN,  ONE);
	BMOVE (0, KP_Down,  DOWN,  ONE);

	BMOVE (GDK_CONTROL_MASK, KP_Left,  LEFT,  WORD);
	BMOVE (GDK_CONTROL_MASK, Left,     LEFT,  WORD);
	BMOVE (GDK_MOD1_MASK,    Left,     LEFT,  WORD);
	BMOVE (GDK_MOD1_MASK,    b,        LEFT,  WORD);

	BMOVE (GDK_CONTROL_MASK, KP_Right, RIGHT, WORD);
	BMOVE (GDK_CONTROL_MASK, Right,    RIGHT, WORD);
	BMOVE (GDK_MOD1_MASK,    Right,    RIGHT, WORD);
	BMOVE (GDK_MOD1_MASK,    f,        RIGHT, WORD);

	BMOVE (0, Page_Up,       UP,   PAGE);
	BMOVE (0, KP_Page_Up,    UP,   PAGE);
	BMOVE (0, Page_Down,     DOWN, PAGE);
	BMOVE (0, KP_Page_Down,  DOWN, PAGE);

	BMOVE (GDK_CONTROL_MASK, a, LEFT,  ALL);
	BMOVE (GDK_CONTROL_MASK, e, RIGHT, ALL);

#define BCOM(m,key,com) \
	gtk_binding_entry_add_signal (binding_set, GDK_ ## key, m, \
				      "command", 1, \
				      GTK_TYPE_HTML_COMMAND, GTK_HTML_COMMAND_ ## com);

	BCOM (0, Return, INSERT_PARAGRAPH);
	BCOM (0, BackSpace, DELETE_BACK);
	BCOM (GDK_SHIFT_MASK, BackSpace, DELETE_BACK);
	BCOM (0, Delete, DELETE);
	BCOM (0, KP_Delete, DELETE);

	BCOM (0, Tab, FOCUS_FORWARD);
	BCOM (0, ISO_Left_Tab, FOCUS_FORWARD);
	BCOM (GDK_SHIFT_MASK, Tab, FOCUS_BACKWARD);
	BCOM (GDK_SHIFT_MASK, ISO_Left_Tab, FOCUS_BACKWARD);
}

void
gtk_html_set_iframe_parent (GtkHTML *html, GtkWidget *parent)
{
	g_assert (GTK_IS_HTML (parent));

	html->iframe_parent = parent;
}

void
gtk_html_select_word (GtkHTML *html)
{
	HTMLEngine *e;

	if (!html->allow_selection)
		return;

	e = html->engine;
	if (html_engine_get_editable (e))
		html_engine_select_word_editable (e);
	else
		html_engine_select_word (e);
}

void
gtk_html_select_line (GtkHTML *html)
{
	HTMLEngine *e;

	if (!html->allow_selection)
		return;

	e = html->engine;
	if (html_engine_get_editable (e))
		html_engine_select_line_editable (e);
	else
		html_engine_select_line (e);
}

void
gtk_html_set_spell_api (GtkHTML *html, GtkHTMLEditSpellAPI *api)
{
	html->spell_api = api;
}
