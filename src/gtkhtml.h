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

#ifndef _GTKHTML_H_
#define _GTKHTML_H_

#include <sys/types.h>
#include <gtk/gtkbindings.h>
#include <gtk/gtklayout.h>
#include <libgnome/gnome-paper.h>
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-master.h>

#include "gtkhtml-types.h"
#include "gtkhtml-enums.h"

/* FIXME we should remove html dep */
#include "htmltypes.h"

#define GTK_TYPE_HTML                  (gtk_html_get_type ())
#define GTK_HTML(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_HTML, GtkHTML))
#define GTK_HTML_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_HTML, GtkHTMLClass))
#define GTK_IS_HTML(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_HTML))
#define GTK_IS_HTML_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_HTML))

struct _GtkHTML {
	GtkLayout layout;

	GtkBindingSet        *editor_bindings;
	GtkWidget            *iframe_parent;
	HTMLObject           *frame;
	GtkHTMLEditorAPI     *editor_api;
	gpointer              editor_data;
	HTMLEngine           *engine;

	/* The URL of the link over which the pointer currently is.  NULL if
           the pointer is not over a link.  */
	gchar *pointer_url;

	/* The cursors we use within the widget.  */
	GdkCursor *hand_cursor;
	GdkCursor *arrow_cursor;
	GdkCursor *ibeam_cursor;

	gint selection_x1, selection_y1;

	guint in_selection : 1;
	guint button1_pressed : 1;

	guint debug : 1;
	guint allow_selection : 1;

	guint hadj_connection;
	guint vadj_connection;

	gboolean binding_handled;
	GtkHTMLPrivate *priv;
};

struct _GtkHTMLClass {
	GtkLayoutClass parent_class;
	
        void (* title_changed)   (GtkHTML *html, const gchar *new_title);
        void (* url_requested)   (GtkHTML *html, const gchar *url, GtkHTMLStream *handle);
        void (* load_done)       (GtkHTML *html);
        void (* link_clicked)    (GtkHTML *html, const gchar *url);
	void (* set_base)        (GtkHTML *html, const gchar *base_url);
	void (* set_base_target) (GtkHTML *html, const gchar *base_url);

	void (* on_url)		 (GtkHTML *html, const gchar *url);
	void (* redirect)        (GtkHTML *html, const gchar *url, int delay);
	void (* submit)          (GtkHTML *html, const gchar *method, const gchar *url, const gchar *encoding);
	gboolean (* object_requested)(GtkHTML *html, GtkHTMLEmbedded *);

	void (* current_paragraph_style_changed) (GtkHTML *html, GtkHTMLParagraphStyle new_style);
	void (* current_paragraph_alignment_changed) (GtkHTML *html, GtkHTMLParagraphAlignment new_alignment);
	void (* current_paragraph_indentation_changed) (GtkHTML *html, guint new_indentation);
	void (* insertion_font_style_changed) (GtkHTML *html, GtkHTMLFontStyle style);
	void (* insertion_color_changed) (GtkHTML *html, GdkColor *color);

        void (* size_changed)       (GtkHTML *html);
	void (* iframe_created)     (GtkHTML *html, GtkHTML *iframe);

	/* keybindings signals */
	void     (* scroll)               (GtkHTML *html, GtkOrientation orientation, GtkScrollType scroll_type,
					   gfloat position);
	void     (* cursor_move)          (GtkHTML *html, GtkDirectionType dir_type, GtkHTMLCursorSkipType skip);
	gboolean (* command)              (GtkHTML *html, GtkHTMLCommandType com_type);

	/* properties */
	GtkHTMLClassProperties *properties;
};

struct _GtkHTMLEditorAPI
{
	/* spell checking methods */
	gboolean  (* check_word)              (GtkHTML *html, const gchar *word, gpointer data);
	void      (* suggestion_request)      (GtkHTML *html, const gchar *word, gpointer data);
	void      (* add_to_session)          (GtkHTML *html, const gchar *word, gpointer data);
	void      (* add_to_personal)         (GtkHTML *html, const gchar *word, gpointer data);

	/* unhandled commands */
	gboolean  (* command)                 (GtkHTML *html, GtkHTMLCommandType com_type, gpointer data);

	GtkArg  * (* event)                   (GtkHTML *html, GtkHTMLEditorEventType event_type, GtkArg **args, gpointer data);

	/* input line */
	GtkWidget * (* create_input_line)     (GtkHTML *html, gpointer data);

	/* spell checking methods */
	void      (* set_language)            (GtkHTML *html, const gchar *language, gpointer data);
};



/* Creation.  */
GtkType    gtk_html_get_type        (void);
void       gtk_html_construct       (GtkWidget        *html);
GtkWidget *gtk_html_new             (void);
GtkWidget *gtk_html_new_from_string (const gchar      *str,
				     gint              len);
void       gtk_html_set_editor_api  (GtkHTML          *html,
				     GtkHTMLEditorAPI *api,
				     gpointer          data);

/* parent iframe setting */
void       gtk_html_set_iframe_parent       (GtkHTML *html,
					     GtkWidget *parent,
					     HTMLObject *frame);

/* Debugging.  */
void  gtk_html_enable_debug  (GtkHTML  *html,
			      gboolean  debug);

/* Behavior.  */
void  gtk_html_allow_selection            (GtkHTML  *html,
					   gboolean  allow);
void  gtk_html_select_word                (GtkHTML  *html);
void  gtk_html_select_line                (GtkHTML  *html);
void  gtk_html_select_paragraph           (GtkHTML  *html);
void  gtk_html_select_paragraph_extended  (GtkHTML  *html);
void  gtk_html_select_all                 (GtkHTML  *html);
int   gtk_html_request_paste              (GtkHTML  *html,
					   GdkAtom   selection,
					   gint      type,
					   gint32    time);
/* Loading.  */
GtkHTMLStream *gtk_html_begin             (GtkHTML             *html);
GtkHTMLStream *gtk_html_begin_content     (GtkHTML             *html,
					   gchar               *content_type);

void           gtk_html_write             (GtkHTML             *html,
					   GtkHTMLStream       *handle,
					   const gchar         *buffer,
					   size_t               size);
void           gtk_html_end               (GtkHTML             *html,
					   GtkHTMLStream       *handle,
					   GtkHTMLStreamStatus  status);
void           gtk_html_load_empty        (GtkHTML             *html);
void           gtk_html_load_from_string  (GtkHTML             *html,
					   const gchar         *str,
					   gint                 len);

/* Saving.  */
gboolean  gtk_html_save  (GtkHTML               *html,
			  GtkHTMLSaveReceiverFn  receiver,
			  gpointer               data);

gboolean  gtk_html_export  (GtkHTML               *html,
			    const char            *type,
			    GtkHTMLSaveReceiverFn  receiver,
			    gpointer               data);

/* Editable support.  */
void      gtk_html_set_editable  (GtkHTML       *html,
				  gboolean       editable);
gboolean  gtk_html_get_editable  (const GtkHTML *html);

/* Printing support.  */

void  gtk_html_print_with_header_footer  (GtkHTML              *html,
					  GnomePrintContext    *print_context,
					  gdouble               header_height,
					  gdouble               footer_height,
					  GtkHTMLPrintCallback  header_print,
					  GtkHTMLPrintCallback  footer_print,
					  gpointer              user_data);
void  gtk_html_print                     (GtkHTML              *html,
					  GnomePrintContext    *print_context);
void  gtk_html_print_set_master          (GtkHTML              *html,
					  GnomePrintMaster     *print_master);

/* Title.  */
const gchar *gtk_html_get_title  (GtkHTML *html);

/* Anchors.  */
gboolean  gtk_html_jump_to_anchor  (GtkHTML *html,
				    const gchar *anchor);


/* Editing functions.  */

GtkHTMLParagraphStyle	   gtk_html_get_paragraph_style          (GtkHTML                   *html);
void  			   gtk_html_set_paragraph_style          (GtkHTML                   *html,
								  GtkHTMLParagraphStyle      style);
void  			   gtk_html_set_indent                   (GtkHTML                   *html,
								  gint                       level);
void  			   gtk_html_modify_indent_by_delta       (GtkHTML                   *html,
								  gint                       delta);
void  			   gtk_html_set_font_style               (GtkHTML                   *html,
								  GtkHTMLFontStyle           and_mask,
								  GtkHTMLFontStyle           or_mask);
void  			   gtk_html_set_color                    (GtkHTML                   *html,
								  HTMLColor                 *color);
void                       gtk_html_toggle_font_style            (GtkHTML                   *html,
								  GtkHTMLFontStyle           style);
GtkHTMLParagraphAlignment  gtk_html_get_paragraph_alignment      (GtkHTML                   *html);
void  			   gtk_html_set_paragraph_alignment      (GtkHTML                   *html,
								  GtkHTMLParagraphAlignment  alignment);

void  gtk_html_cut    (GtkHTML *html);
void  gtk_html_copy   (GtkHTML *html);
void  gtk_html_paste  (GtkHTML *html);

void  gtk_html_undo  (GtkHTML *html);
void  gtk_html_redo  (GtkHTML *html);

void  gtk_html_insert_html (GtkHTML *html, const gchar *html_src);
void  gtk_html_append_html (GtkHTML *html, const gchar *html_src);

/* misc utils */

void      gtk_html_set_default_background_color  (GtkHTML     *html,
						  GdkColor    *c);
void      gtk_html_set_default_content_type      (GtkHTML     *html,
						  gchar       *content_type);
gpointer  gtk_html_get_object_by_id              (GtkHTML     *html,
						  const gchar *id);
gboolean  gtk_html_command                       (GtkHTML     *html,
						  const gchar *command_name);
gboolean  gtk_html_edit_make_cursor_visible      (GtkHTML     *html);

gboolean  gtk_html_build_with_gconf              (void);
void      gtk_html_set_magnification             (GtkHTML *html,
						  gdouble magnification);
void      gtk_html_zoom_in                       (GtkHTML *html);
void      gtk_html_zoom_out                      (GtkHTML *html);
void      gtk_html_zoom_reset                    (GtkHTML *html);

void      gtk_html_update_styles                 (GtkHTML *html);
void      gtk_html_set_allow_frameset            (GtkHTML *html, gboolean allow);
gboolean  gtk_html_get_allow_frameset            (GtkHTML *html);

void         gtk_html_set_base                      (GtkHTML *html, const char *url);
const char*  gtk_html_get_base                      (GtkHTML *html);

#endif /* _GTKHTML_H_ */
