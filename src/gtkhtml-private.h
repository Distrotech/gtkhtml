/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.

    Copyright 1999, 2000 Helix Code, Inc.
    
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef _GTKHTML_PRIVATE_H
#define _GTKHTML_PRIVATE_H

#include <libgnome/gnome-paper.h>
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-master.h>
#include <gtk/gtkwidget.h>
#include "gtkhtml-types.h"
#include "htmltypes.h"


struct _GtkHTMLPrivate {
	guint idle_handler_id;
	guint scroll_timeout_id;

	GtkHTMLParagraphStyle paragraph_style;
	guint paragraph_indentation;
	GtkHTMLParagraphAlignment paragraph_alignment;
	GtkHTMLFontStyle insertion_font_style;

	gboolean update_styles;

	gint last_selection_type;
	/* Used to hold the primary selection when
	** pasting within ourselves
	*/
	HTMLObject *primary;
	guint       primary_len;

	gchar *content_type;
	char  *base_url;

	GtkWidget *search_input_line;

	GnomePrintMaster *print_master;

#ifdef GTKHTML_HAVE_GCONF
	guint set_font_id;
	guint notify_id;
#endif
#ifdef GTK_HTML_USE_XIM
	GdkICAttr *ic_attr;
	GdkIC *ic;
#endif
};

void  gtk_html_private_calc_scrollbars  (GtkHTML                 *html,
					 gboolean                *changed_x,
					 gboolean                *changed_y);
void  gtk_html_editor_event_command     (GtkHTML                 *html,
					 GtkHTMLCommandType       com_type,
					 gboolean                 before);
void  gtk_html_editor_event             (GtkHTML                 *html,
					 GtkHTMLEditorEventType   event,
					 GtkArg                 **args);
void  gtk_html_api_set_language         (GtkHTML                 *html);

#endif /* _GTKHTML_PRIVATE_H */


