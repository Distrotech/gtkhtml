/*  This file is part of the GtkHTML library.
 *
 *  Copyright 2002 Ximian, Inc.
 *
 *  Author: Radek Doulik
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <atk/atkcomponent.h>
#include <atk/atktext.h>

#include "gtkhtml.h"
#include "htmlengine.h"
#include "htmlinterval.h"
#include "htmllinktext.h"
#include "htmltext.h"
#include "htmltextslave.h"

#include "object.h"
#include "html.h"
#include "text.h"

static void html_a11y_text_class_init    (HTMLA11YTextClass *klass);
static void html_a11y_text_init          (HTMLA11YText *a11y_text);
static void atk_component_interface_init (AtkComponentIface *iface);
static void atk_text_interface_init      (AtkTextIface *iface);

static void html_a11y_text_get_extents   (AtkComponent *component,
					  gint *x, gint *y, gint *width, gint *height, AtkCoordType coord_type);
static void html_a11y_text_get_size      (AtkComponent *component, gint *width, gint *height);
static gchar * html_a11y_text_get_text (AtkText *text, gint start_offset, gint end_offset);
static gchar * html_a11y_text_get_text_after_offset (AtkText *text, gint offset, AtkTextBoundary boundary_type,
						     gint *start_offset, gint *end_offset);
static gchar * html_a11y_text_get_text_at_offset (AtkText *text, gint offset, AtkTextBoundary boundary_type,
						  gint *start_offset, gint *end_offset);
static gunichar html_a11y_text_get_character_at_offset (AtkText *text, gint offset);
static gchar * html_a11y_text_get_text_before_offset (AtkText *text, gint offset, AtkTextBoundary boundary_type,
						      gint *start_offset, gint *end_offset);
static gint html_a11y_text_get_character_count (AtkText *text);

static AtkObjectClass *parent_class = NULL;

GType
html_a11y_text_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo tinfo = {
			sizeof (HTMLA11YTextClass),
			NULL,                                                      /* base init */
			NULL,                                                      /* base finalize */
			(GClassInitFunc) html_a11y_text_class_init,                /* class init */
			NULL,                                                      /* class finalize */
			NULL,                                                      /* class data */
			sizeof (HTMLA11YText),                                     /* instance size */
			0,                                                         /* nb preallocs */
			(GInstanceInitFunc) html_a11y_text_init,                   /* instance init */
			NULL                                                       /* value table */
		};

		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) atk_component_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		static const GInterfaceInfo atk_text_info = {
			(GInterfaceInitFunc) atk_text_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = g_type_register_static (G_TYPE_HTML_A11Y, "HTMLA11YText", &tinfo, 0);
		g_type_add_interface_static (type, ATK_TYPE_COMPONENT, &atk_component_info);
		g_type_add_interface_static (type, ATK_TYPE_TEXT, &atk_text_info);
	}

	return type;
}

static void 
atk_component_interface_init (AtkComponentIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->get_extents = html_a11y_text_get_extents;
	iface->get_size = html_a11y_text_get_size;
}

static void
atk_text_interface_init (AtkTextIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->get_text = html_a11y_text_get_text;
	iface->get_text_after_offset = html_a11y_text_get_text_after_offset;
	iface->get_text_before_offset = html_a11y_text_get_text_before_offset;
	iface->get_text_at_offset = html_a11y_text_get_text_at_offset;
	iface->get_character_at_offset = html_a11y_text_get_character_at_offset;
	iface->get_character_count = html_a11y_text_get_character_count;
}

static void
html_a11y_text_finalize (GObject *obj)
{
}

static void
html_a11y_text_initialize (AtkObject *obj, gpointer data)
{
	GtkTextBuffer *buffer;
	HTMLText *to;
	HTMLA11YText *ato;
	gchar *text;

	/* printf ("html_a11y_text_initialize\n"); */

	if (ATK_OBJECT_CLASS (parent_class)->initialize)
		ATK_OBJECT_CLASS (parent_class)->initialize (obj, data);

	to = HTML_TEXT (data);
	ato = HTML_A11Y_TEXT (obj);

	buffer = gtk_text_buffer_new (NULL);
	ato->util = gail_text_util_new ();
	gtk_text_buffer_set_text (buffer, to->text, -1);
	gail_text_util_buffer_setup (ato->util, buffer);
	g_object_unref (buffer);
}

static void
html_a11y_text_class_init (HTMLA11YTextClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	atk_class->initialize = html_a11y_text_initialize;
	gobject_class->finalize = html_a11y_text_finalize;
}

static void
html_a11y_text_init (HTMLA11YText *a11y_text)
{
}

AtkObject* 
html_a11y_text_new (HTMLObject *html_obj)
{
	GObject *object;
	AtkObject *accessible;

	g_return_val_if_fail (HTML_IS_TEXT (html_obj) || HTML_IS_LINK_TEXT (html_obj), NULL);

	object = g_object_new (G_TYPE_HTML_A11Y_TEXT, NULL);

	accessible = ATK_OBJECT (object);
	atk_object_initialize (accessible, html_obj);

	accessible->role = ATK_ROLE_TEXT;

	/* printf ("created new html accessible text object\n"); */

	return accessible;
}

/*
 * AtkComponent interface
 */

static void
get_size (HTMLObject *obj, gint *width, gint *height)
{
	HTMLObject *last;

	if (obj) {
		gint ax, ay;

		html_object_calc_abs_position (obj, &ax, &ay);
		last = obj;
		while (last->next && HTML_IS_TEXT_SLAVE (last->next))
			last = last->next;
		if (HTML_IS_TEXT_SLAVE (last)) {
			gint lx, ly;
			html_object_calc_abs_position (last, &lx, &ly);

			*width = lx + last->width - ax;
			*height = ly + last->descent - ay;
		}
	}
}

static void
html_a11y_text_get_extents (AtkComponent *component, gint *x, gint *y, gint *width, gint *height, AtkCoordType coord_type)
{
	HTMLObject *obj = HTML_A11Y_HTML (component);

	html_a11y_get_extents (component, x, y, width, height, coord_type);
	get_size (obj, width, height);
}

static void
html_a11y_text_get_size (AtkComponent *component, gint *width, gint *height)
{
	HTMLObject *obj = HTML_A11Y_HTML (component);

	html_a11y_get_size (component, width, height);
	get_size (obj, width, height);
}

/*
 * AtkText interface
 */

static gchar *
html_a11y_text_get_text (AtkText *text, gint start_offset, gint end_offset)
{
	HTMLText *to = HTML_TEXT (HTML_A11Y_HTML (text));
	gchar *str;

	/* printf ("%d - %d\n", start_offset, end_offset); */
	if (end_offset == -1)
		end_offset = to->text_len;

	g_return_val_if_fail (start_offset <= end_offset, NULL);
	g_return_val_if_fail (start_offset >= 0, NULL);
	g_return_val_if_fail (start_offset <= to->text_len, NULL);
	g_return_val_if_fail (end_offset <= to->text_len, NULL);

	str = html_text_get_text (to, start_offset);

	return g_strndup (str, g_utf8_offset_to_pointer (str, end_offset - start_offset) - str);
}

static gchar *
html_a11y_text_get_text_after_offset (AtkText *text, gint offset, AtkTextBoundary boundary_type,
				      gint *start_offset, gint *end_offset)
{
	return gail_text_util_get_text (HTML_A11Y_TEXT (text)->util, NULL, GAIL_AFTER_OFFSET, boundary_type, offset, 
					start_offset, end_offset);
}

static gchar *
html_a11y_text_get_text_at_offset (AtkText *text, gint offset, AtkTextBoundary boundary_type,
				   gint *start_offset, gint *end_offset)
{
	return gail_text_util_get_text (HTML_A11Y_TEXT (text)->util, NULL, GAIL_AT_OFFSET, boundary_type, offset, 
					start_offset, end_offset);
}

static gunichar
html_a11y_text_get_character_at_offset (AtkText *text, gint offset)
{
	HTMLText *to = HTML_TEXT (HTML_A11Y_HTML (text));

	g_return_val_if_fail (offset <= to->text_len, 0);

	return html_text_get_char (to, offset);
}

static gchar *
html_a11y_text_get_text_before_offset (AtkText *text, gint offset, AtkTextBoundary boundary_type,
				       gint *start_offset, gint *end_offset)
{
	return gail_text_util_get_text (HTML_A11Y_TEXT (text)->util, NULL, GAIL_BEFORE_OFFSET, boundary_type, offset, 
					start_offset, end_offset);
}

static gint
html_a11y_text_get_character_count (AtkText *text)
{
	return HTML_TEXT (HTML_A11Y_HTML (text))->text_len;
}

static gint
html_a11y_text_get_n_selections (AtkText *text)
{
	return HTML_A11Y_HTML (text)->selected ? 1 : 0;
}

static gchar *
html_a11y_text_get_selection (AtkText *text, gint selection_num, gint *start_offset, gint *end_offset)
{
	HTMLText *to = HTML_TEXT (HTML_A11Y_HTML (text));

	if (!HTML_OBJECT (to)->selected || selection_num > 0)
		return NULL;

	*start_offset = to->select_start;
	*end_offset = to->select_start + to->select_length;

	return html_a11y_text_get_text (text, *start_offset, *end_offset);
}

static gboolean
html_a11y_text_add_selection (AtkText *text, gint start_offset, gint end_offset)
{
	GtkHTML *html = GTK_HTML_A11Y_GTKHTML (html_a11y_get_gtkhtml_parent (HTML_A11Y (text)));
	HTMLObject *obj = HTML_A11Y_HTML (text);
	HTMLInterval *i;

	if (html_engine_is_selection_active (html->engine))
		return FALSE;

	i = html_interval_new (obj, obj, start_offset, end_offset);
	html_engine_select_interval (html->engine, i);

	return TRUE;
}

static gboolean
html_a11y_text_remove_selection (AtkText *text, gint selection_num)
{
	GtkHTML *html = GTK_HTML_A11Y_GTKHTML (html_a11y_get_gtkhtml_parent (HTML_A11Y (text)));
	HTMLObject *obj = HTML_A11Y_HTML (text);
	HTMLInterval *i;

	if (!obj->selected || selection_num)
		return FALSE;

	html_engine_unselect_all (html->engine);

	return TRUE;
}

static gboolean
html_a11y_text_set_selection (AtkText *text, gint selection_num, gint start_offset, gint end_offset)
{
	if (selection_num)
		return FALSE;

	return html_a11y_text_add_selection (text, start_offset, end_offset);
}

/*
  AtkAttributeSet* (* get_run_attributes)         (AtkText	    *text,
						   gint	  	    offset,
						   gint             *start_offset,
						   gint	 	    *end_offset);
  AtkAttributeSet* (* get_default_attributes)     (AtkText	    *text);
  void           (* get_character_extents)        (AtkText          *text,
                                                   gint             offset,
                                                   gint             *x,
                                                   gint             *y,
                                                   gint             *width,
                                                   gint             *height,
                                                   AtkCoordType	    coords);
  gint           (* get_character_count)          (AtkText          *text);
  gint           (* get_offset_at_point)          (AtkText          *text,
                                                   gint             x,
                                                   gint             y,
                                                   AtkCoordType	    coords);
  gboolean       (* set_caret_offset)             (AtkText          *text,
                                                   gint             offset);

*/
