/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.

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

    Author: Ettore Perazzoli <ettore@helixcode.com>
*/

#include <config.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gnome.h>
#include <bonobo.h>

#include "htmlengine-edit-cut-and-paste.h"

#include "menubar.h"
#include "gtkhtml.h"
#include "body.h"
#include "control-data.h"
#include "properties.h"
#include "image.h"
#include "text.h"
#include "paragraph.h"
#include "spell.h"
#include "table.h"
#include "template.h"

static void smiley_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname);
static void font_style_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname);
static void command_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname);

static void
paste_quotation_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	gtk_html_paste (cd->html, TRUE);
}

static void
search_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	search (cd, FALSE);
}

static void
search_regex_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	search (cd, TRUE);
}

static void
search_next_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	search_next (cd);
}

static void
replace_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	replace (cd);
}

static void
insert_image_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	if (cd->properties_dialog)
		gtk_html_edit_properties_dialog_close (cd->properties_dialog);
	cd->properties_dialog = gtk_html_edit_properties_dialog_new (cd, TRUE, _("Insert"), ICONDIR "/insert-image-24.png");

	gtk_html_edit_properties_dialog_add_entry (cd->properties_dialog,
						   GTK_HTML_EDIT_PROPERTY_IMAGE, _("Image"),
						   image_insertion,
						   image_insert_cb,
						   image_close_cb);
	gtk_html_edit_properties_dialog_show (cd->properties_dialog);
}

static void
insert_link_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	if (cd->properties_dialog)
		gtk_html_edit_properties_dialog_close (cd->properties_dialog);

	cd->properties_dialog = gtk_html_edit_properties_dialog_new (cd, TRUE, _("Insert"), ICONDIR "/insert-link-24.png");

	gtk_html_edit_properties_dialog_add_entry (cd->properties_dialog,
						   GTK_HTML_EDIT_PROPERTY_LINK, _("Link"),
						   link_insert,
						   link_insert_cb,
						   link_close_cb);

	gtk_html_edit_properties_dialog_show (cd->properties_dialog);
	gtk_html_edit_properties_dialog_set_page (cd->properties_dialog, GTK_HTML_EDIT_PROPERTY_LINK);
}

static void
insert_rule_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	if (cd->properties_dialog)
		gtk_html_edit_properties_dialog_close (cd->properties_dialog);

	cd->properties_dialog = gtk_html_edit_properties_dialog_new (cd, TRUE, _("Insert"), ICONDIR "/insert-rule-24.png");

	gtk_html_edit_properties_dialog_add_entry (cd->properties_dialog,
						   GTK_HTML_EDIT_PROPERTY_RULE, _("Rule"),
						   rule_insert,
						   rule_insert_cb,
						   rule_close_cb);

	gtk_html_edit_properties_dialog_show (cd->properties_dialog);
}

void
insert_table (GtkHTMLControlData *cd)
{
	if (cd->properties_dialog)
		gtk_html_edit_properties_dialog_close (cd->properties_dialog);

	cd->properties_dialog = gtk_html_edit_properties_dialog_new (cd, TRUE, _("Insert"), ICONDIR "/insert-table-24.png");

	gtk_html_edit_properties_dialog_add_entry (cd->properties_dialog,
						   GTK_HTML_EDIT_PROPERTY_TABLE, _("Table"),
						   table_insert,
						   table_insert_cb,
						   table_close_cb);

	gtk_html_edit_properties_dialog_show (cd->properties_dialog);
}

static void
insert_table_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	insert_table (cd);
}

static void
insert_template_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	if (cd->properties_dialog)
		gtk_html_edit_properties_dialog_close (cd->properties_dialog);

	cd->properties_dialog = gtk_html_edit_properties_dialog_new (cd, TRUE, _("Insert"), ICONDIR "/insert-object-24.png");

	gtk_html_edit_properties_dialog_add_entry (cd->properties_dialog,
						   GTK_HTML_EDIT_PROPERTY_TABLE, _("Template"),
						   template_insert,
						   template_insert_cb,
						   template_close_cb);

	gtk_html_edit_properties_dialog_show (cd->properties_dialog);
}

static void
file_dialog_destroy (GtkWidget *w, GtkHTMLControlData *cd)
{
	cd->file_dialog = NULL;
}

static void
file_dialog_ok (GtkWidget *w, GtkHTMLControlData *cd)
{
	const gchar *filename;
	GIOChannel *io = NULL;
	GError *error = NULL;
	gchar *data = NULL;
	gsize len = 0;
	const char *charset;
	
	filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (cd->file_dialog));
	io = g_io_channel_new_file (filename, "r", &error);

	if (error || !io)
		goto end;
		
	/* try reading as utf-8 */
	g_io_channel_read_to_end (io, &data, &len, &error);

	/* If we get a charset error try reading as the locale charset */
	if (error && g_error_matches (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE)
	    && !g_get_charset (&charset)) {

		g_error_free (error);
		error = NULL;

		/* 
		 * reopen the io channel since we can't set the 
		 * charset once we've begun reading.
		 */
		g_io_channel_unref (io);
		io = g_io_channel_new_file (filename, "r", &error);
		
		if (error || !io)
			goto end;
		
		g_io_channel_set_encoding (io, charset, NULL);
		g_io_channel_read_to_end (io, &data, &len, &error);
	}
	
	if (error)
		goto end;
	
	if (cd->file_html) {
		GtkHTML *tmp;
		GtkHTMLStream *stream;
		
		tmp = GTK_HTML (gtk_html_new ());
		stream = gtk_html_begin_content (tmp, "text/html; charset=utf-8");
		gtk_html_write (tmp, stream, data, len);
		gtk_html_end (tmp, stream, error ? GTK_HTML_STREAM_OK : GTK_HTML_STREAM_ERROR);
		gtk_html_insert_gtk_html (cd->html, tmp);
	} else {
		html_engine_paste_text (cd->html->engine, data, g_utf8_strlen (data, -1));
	}
	g_free (data);

 end:
	if (io)
		g_io_channel_unref (io);

	if (error) {
		GtkWidget *toplevel;
		
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (cd->html));
		
		if (GTK_WIDGET_TOPLEVEL (toplevel)) {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
							 GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_CLOSE,
							 _("Error loading file '%s': %s"),
							 filename, error->message);
		
			/* Destroy the dialog when the user responds to it (e.g. clicks a button) */
			g_signal_connect_swapped (GTK_OBJECT (dialog), "response",
						  G_CALLBACK (gtk_widget_destroy),
						  GTK_OBJECT (dialog));

			gtk_widget_show (dialog);
		} else {
			g_warning ("Error loading file '%s': %s", filename, error->message);
		}
		g_error_free (error);
	}

	gtk_widget_destroy (cd->file_dialog);
}

static void
insert_file_dialog (GtkHTMLControlData *cd, gboolean html)
{
	cd->file_html = html;
	if (cd->file_dialog != NULL) {
		gdk_window_show (GTK_WIDGET (cd->file_dialog)->window);
		return;
	}

	cd->file_dialog = gtk_file_selection_new (html ? _("Insert: HTML File") : _("Insert: Text File"));
	gtk_file_selection_set_filename (GTK_FILE_SELECTION (cd->file_dialog), "~/");

	g_signal_connect_object (GTK_FILE_SELECTION (cd->file_dialog)->cancel_button,
				 "clicked", G_CALLBACK (gtk_widget_destroy), GTK_OBJECT (cd->file_dialog),
				 G_CONNECT_SWAPPED);

	g_signal_connect (GTK_FILE_SELECTION (cd->file_dialog)->ok_button, "clicked", G_CALLBACK (file_dialog_ok), cd);

	g_signal_connect (cd->file_dialog, "destroy", G_CALLBACK (file_dialog_destroy), cd);

	gtk_widget_show (cd->file_dialog);
}

static void
insert_text_file_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	insert_file_dialog (cd, FALSE);
}

static void
insert_html_file_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	insert_file_dialog (cd, TRUE);
}

static void 
spell_check_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	spell_check_dialog (cd, TRUE);
}

static void
format_page_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	if (cd->properties_dialog)
		gtk_html_edit_properties_dialog_close (cd->properties_dialog);

	cd->properties_dialog = gtk_html_edit_properties_dialog_new (cd, FALSE, _("Properties"), ICONDIR "/properties-16.png");

	gtk_html_edit_properties_dialog_add_entry (cd->properties_dialog,
						   GTK_HTML_EDIT_PROPERTY_BODY, _("Page"),
						   body_properties,
						   body_apply_cb,
						   body_close_cb);

	gtk_html_edit_properties_dialog_show (cd->properties_dialog);
	gtk_html_edit_properties_dialog_set_page (cd->properties_dialog, GTK_HTML_EDIT_PROPERTY_BODY);
}

static void
format_text_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	if (cd->properties_dialog)
		gtk_html_edit_properties_dialog_close (cd->properties_dialog);

	cd->properties_dialog = gtk_html_edit_properties_dialog_new (cd, FALSE, _("Properties"), ICONDIR "/properties-16.png");

	gtk_html_edit_properties_dialog_add_entry (cd->properties_dialog,
						   GTK_HTML_EDIT_PROPERTY_BODY, _("Text"),
						   text_properties,
						   text_apply_cb,
						   text_close_cb);

	gtk_html_edit_properties_dialog_show (cd->properties_dialog);
	gtk_html_edit_properties_dialog_set_page (cd->properties_dialog, GTK_HTML_EDIT_PROPERTY_TEXT);
}

static void
format_paragraph_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	if (cd->properties_dialog)
		gtk_html_edit_properties_dialog_close (cd->properties_dialog);

	cd->properties_dialog = gtk_html_edit_properties_dialog_new (cd, FALSE, _("Properties"), ICONDIR "/properties-16.png");

	gtk_html_edit_properties_dialog_add_entry (cd->properties_dialog,
						   GTK_HTML_EDIT_PROPERTY_BODY, _("Paragraph"),
						   paragraph_properties,
						   paragraph_apply_cb,
						   paragraph_close_cb);

	gtk_html_edit_properties_dialog_show (cd->properties_dialog);
	gtk_html_edit_properties_dialog_set_page (cd->properties_dialog, GTK_HTML_EDIT_PROPERTY_PARAGRAPH);
}

static BonoboUIVerb editor_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("EditUndo", command_cb),
	BONOBO_UI_UNSAFE_VERB ("EditRedo", command_cb),
	BONOBO_UI_UNSAFE_VERB ("EditCut", command_cb),
	BONOBO_UI_UNSAFE_VERB ("EditCopy", command_cb),
	BONOBO_UI_UNSAFE_VERB ("EditPaste", command_cb),
	BONOBO_UI_UNSAFE_VERB ("EditPasteQuotation", paste_quotation_cb),
	BONOBO_UI_UNSAFE_VERB ("EditFind", search_cb),
	BONOBO_UI_UNSAFE_VERB ("EditFindRegex", search_regex_cb),
	BONOBO_UI_UNSAFE_VERB ("EditFindAgain", search_next_cb),
	BONOBO_UI_UNSAFE_VERB ("EditReplace", replace_cb),
	BONOBO_UI_UNSAFE_VERB ("EditSelectAll", command_cb),
	BONOBO_UI_UNSAFE_VERB ("EditSpellCheck", spell_check_cb),

	BONOBO_UI_UNSAFE_VERB ("InsertImage", insert_image_cb),
	BONOBO_UI_UNSAFE_VERB ("InsertLink",  insert_link_cb),
	BONOBO_UI_UNSAFE_VERB ("InsertRule",  insert_rule_cb),
	BONOBO_UI_UNSAFE_VERB ("InsertTable", insert_table_cb),
	BONOBO_UI_UNSAFE_VERB ("InsertTemplate", insert_template_cb),

	BONOBO_UI_UNSAFE_VERB ("InsertTextFile", insert_text_file_cb),
	BONOBO_UI_UNSAFE_VERB ("InsertHTMLFile", insert_html_file_cb),

	BONOBO_UI_UNSAFE_VERB ("InsertSmiley1", smiley_cb),
	BONOBO_UI_UNSAFE_VERB ("InsertSmiley2", smiley_cb),
	BONOBO_UI_UNSAFE_VERB ("InsertSmiley3", smiley_cb),
	BONOBO_UI_UNSAFE_VERB ("InsertSmiley4", smiley_cb),
	BONOBO_UI_UNSAFE_VERB ("InsertSmiley5", smiley_cb),
	BONOBO_UI_UNSAFE_VERB ("InsertSmiley6", smiley_cb),
	BONOBO_UI_UNSAFE_VERB ("InsertSmiley8", smiley_cb),
	BONOBO_UI_UNSAFE_VERB ("InsertSmiley9", smiley_cb),
	BONOBO_UI_UNSAFE_VERB ("InsertSmiley10", smiley_cb),
	BONOBO_UI_UNSAFE_VERB ("InsertSmiley11", smiley_cb),

	BONOBO_UI_UNSAFE_VERB ("IndentMore", command_cb),
	BONOBO_UI_UNSAFE_VERB ("IndentLess", command_cb),
	BONOBO_UI_UNSAFE_VERB ("WrapLines", command_cb),

	BONOBO_UI_UNSAFE_VERB ("FormatText", format_text_cb),
	BONOBO_UI_UNSAFE_VERB ("FormatParagraph", format_paragraph_cb),
	BONOBO_UI_UNSAFE_VERB ("FormatPage", format_page_cb),

	BONOBO_UI_UNSAFE_VERB ("HeadingNormal", command_cb),
	BONOBO_UI_UNSAFE_VERB ("HeadingPreformat", command_cb),
	BONOBO_UI_UNSAFE_VERB ("HeadingH1", command_cb),
	BONOBO_UI_UNSAFE_VERB ("HeadingH2", command_cb),
	BONOBO_UI_UNSAFE_VERB ("HeadingH3", command_cb),
	BONOBO_UI_UNSAFE_VERB ("HeadingH4", command_cb),
	BONOBO_UI_UNSAFE_VERB ("HeadingH5", command_cb),
	BONOBO_UI_UNSAFE_VERB ("HeadingH6", command_cb),
	BONOBO_UI_UNSAFE_VERB ("HeadingH1", command_cb),
	BONOBO_UI_UNSAFE_VERB ("HeadingAddress", command_cb),
	BONOBO_UI_UNSAFE_VERB ("HeadingBulletedList", command_cb),
	BONOBO_UI_UNSAFE_VERB ("HeadingRomanList", command_cb),
	BONOBO_UI_UNSAFE_VERB ("HeadingNumberedList", command_cb),
	BONOBO_UI_UNSAFE_VERB ("HeadingAlphabeticalList", command_cb),

	BONOBO_UI_UNSAFE_VERB ("FontSizeNegTwo", command_cb),
	BONOBO_UI_UNSAFE_VERB ("FontSizeNegOne", command_cb),
	BONOBO_UI_UNSAFE_VERB ("FontSizeZero", command_cb),
	BONOBO_UI_UNSAFE_VERB ("FontSizeOne", command_cb),
	BONOBO_UI_UNSAFE_VERB ("FontSizeTwo", command_cb),
	BONOBO_UI_UNSAFE_VERB ("FontSizeThree", command_cb),
	BONOBO_UI_UNSAFE_VERB ("FontSizeFour", command_cb),
	BONOBO_UI_UNSAFE_VERB ("FormatBold", command_cb),
	BONOBO_UI_UNSAFE_VERB ("FormatItalic", command_cb),
	BONOBO_UI_UNSAFE_VERB ("FormatUnderline", command_cb),
	BONOBO_UI_UNSAFE_VERB ("FormatStrikeout", command_cb),

	BONOBO_UI_UNSAFE_VERB ("FormatFixed", font_style_cb),
	BONOBO_UI_UNSAFE_VERB ("FormatSubscript", font_style_cb),
	BONOBO_UI_UNSAFE_VERB ("FormatSuperscript", font_style_cb),

	BONOBO_UI_UNSAFE_VERB ("AlignLeft", command_cb),
	BONOBO_UI_UNSAFE_VERB ("AlignCenter", command_cb),
	BONOBO_UI_UNSAFE_VERB ("AlignRight", command_cb),

	BONOBO_UI_VERB_END
};

static struct {
	const gchar *command;
	const gchar *verb;
} command_assoc[] = {
	{"size-minus-two", "FontSizeNegTwo"},
	{"size-minus-one", "FontSizeNegOne"},
	{"size-plus-0", "FontSizeZero"},
	{"size-plus-1", "FontSizeOne"},
	{"size-plus-2", "FontSizeTwo"},
	{"size-plus-3", "FontSizeThree"},
	{"size-plus-4", "FontSizeFour"},
	{"bold-toggle",    "FormatBold"},
	{"italic-toggle", "FormatItalic"},
	{"underline-toggle", "FormatUnderline"},
	{"strikeout-toggle", "FormatStrikeout"},
	{"indent-more", "IndentMore"},
	{"indent-less", "IndentLess"},
	{"indent-paragraph", "WrapLines"},
	{"align-left", "AlignLeft"},
	{"align-right", "AlignRight"},
	{"align-center", "AlignCenter"},
	{"select-all", "EditSelectAll"},
	{"undo", "EditUndo"},
	{"redo", "EditRedo"},
	{"cut", "EditCut"},
	{"copy", "EditCopy"},
	{"paste", "EditPaste"},
	{"style-normal", "HeadingNormal"},
	{"style-header1", "HeadingH1"},
	{"style-header2", "HeadingH2"},
	{"style-header3", "HeadingH3"},
	{"style-header4", "HeadingH4"},
	{"style-header5", "HeadingH5"},
	{"style-header6", "HeadingH6"},
	{"style-address", "HeadingAddress"},
	{"style-pre", "HeadingPreformat"},
	{"style-itemdot", "HeadingBulletedList"},
	{"style-itemroman", "HeadingRomanList"},
	{"style-itemdigit", "HeadingNumberedList"},
	{"style-itemalpha", "HeadingAlphabeticalList"},
	{0, NULL}
};

static struct {
	GtkHTMLFontStyle style;
	const gchar *verb;
} font_style_assoc[] = {	
	{GTK_HTML_FONT_STYLE_FIXED, "FormatFixed"},
	{GTK_HTML_FONT_STYLE_SUBSCRIPT, "FormatSubscript"},
	{GTK_HTML_FONT_STYLE_SUBSCRIPT, "FormatSuperscript"},
	{0, NULL}
};

static void
font_style_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
        int i;
                                                                              
        /* g_warning ("wowee %s :: %s", path, state); */
        for (i = 0; font_style_assoc[i].verb != NULL; i++) {
                if (!strcmp (cname, font_style_assoc[i].verb)) {
                        if (font_style_assoc[i].style > GTK_HTML_FONT_STYLE_MAX)
                                gtk_html_set_font_style (cd->html, ~0,
                                                         font_style_assoc[i].style);
                        else
                                gtk_html_set_font_style (cd->html,
                                                         GTK_HTML_FONT_STYLE_MAX
                                                         & ~GTK_HTML_FONT_STYLE_SIZE_MASK,
                                                         font_style_assoc[i].style);
                }
        }
}

static void 
command_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	int i;
	
	for (i = 0; command_assoc[i].verb != NULL; i++) {
		if (!strcmp (cname, command_assoc[i].verb)) {
			gtk_html_command (cd->html, command_assoc[i].command); 
			return;
		}
	}
}

void
menubar_update_format (GtkHTMLControlData *cd)
{
	CORBA_Environment ev;
	BonoboUIComponent *uic;
	gchar *sensitive;

	uic = bonobo_control_get_ui_component (cd->control);

	if ((uic != CORBA_OBJECT_NIL) && (bonobo_ui_component_get_container (uic) != CORBA_OBJECT_NIL)) {

		sensitive = (cd->format_html ? "1" : "0");

		CORBA_exception_init (&ev);
		bonobo_ui_component_freeze (uic, &ev);

		bonobo_ui_component_set_prop (uic, "/commands/InsertImage",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/InsertLink",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/InsertRule",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/InsertTable",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/InsertTemplate",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/InsertTemplate",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/InsertSmiley1",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/InsertSmiley2",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/InsertSmiley3",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/InsertSmiley4",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/InsertSmiley5",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/InsertSmiley6",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/InsertSmiley8",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/InsertSmiley9",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/InsertSmiley10",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/InsertSmiley11",
					      "sensitive", sensitive, &ev);

		bonobo_ui_component_set_prop (uic, "/commands/FormatBold",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/FormatItalic",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/FormatUnderline",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/FormatStrikeout",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/FormatPlain",
					      "sensitive", sensitive, &ev);
	
		bonobo_ui_component_set_prop (uic, "/commands/AlignLeft",
					      "sensitive", sensitive, &ev);		
		bonobo_ui_component_set_prop (uic, "/commands/AlignRight",
					      "sensitive", sensitive, &ev);	
		bonobo_ui_component_set_prop (uic, "/commands/AlignCenter",
					      "sensitive", sensitive, &ev);	

		bonobo_ui_component_set_prop (uic, "/commands/HeadingH1",
					      "sensitive", sensitive, &ev);	
		bonobo_ui_component_set_prop (uic, "/commands/HeadingH2",
					      "sensitive", sensitive, &ev);	
		bonobo_ui_component_set_prop (uic, "/commands/HeadingH3",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/HeadingH4",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/HeadingH5",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/HeadingH6",
					      "sensitive", sensitive, &ev);
		bonobo_ui_component_set_prop (uic, "/commands/HeadingAddress",
					      "sensitive", sensitive, &ev);

		bonobo_ui_component_thaw (uic, &ev);	

		CORBA_exception_free (&ev);
	}
}

void
menubar_set_languages (GtkHTMLControlData *cd, const gchar *lstr)
{
	GString *str;
	gboolean enabled;
	gint i;

	if (!cd->languages)
		return;

	str = g_string_new (NULL);
	cd->block_language_changes = TRUE;
	for (i = 0; i < cd->languages->_length; i ++) {
		enabled = strstr (lstr, cd->languages->_buffer [i].abbreviation) != NULL;
		g_string_printf (str, "/commands/SpellLanguage%d", i + 1);
		bonobo_ui_component_set_prop (cd->uic, str->str, "state", enabled ? "1" : "0", NULL);
	}
	cd->block_language_changes = FALSE;
}

#define SMILEYS 11
static gchar *smiley [SMILEYS] = {
	":D",
	":O",
	":)",
	";)",
	"=)",
	":(",
	":-)",
	":-|",
	":-/",
	":-P",
	":~("
};

static void
smiley_cb (BonoboUIComponent *uic, GtkHTMLControlData *cd, const char *cname)
{
	gint i;

	g_return_if_fail (cname);

	i = atoi (cname + 12) - 1;

	if (i >=0 && i < SMILEYS) {
		gchar *s;
		s = g_strdup_printf ("<IMG ALT=\"%s\" SRC=\"file://" ICONDIR "/smiley-%d.png\">", smiley [i], i + 1);
		gtk_html_insert_html (cd->html, s);
		g_free (s);
	}
}

static void
menubar_paragraph_style_changed_cb (GtkHTML *html, GtkHTMLParagraphStyle style, GtkHTMLControlData *cd)
{
	bonobo_ui_component_set_prop (bonobo_control_get_ui_component (cd->control), "/commands/WrapLines",
				      "sensitive", style == GTK_HTML_PARAGRAPH_STYLE_PRE ? "1" : "0", NULL);
}

void
menubar_setup (BonoboUIComponent  *uic,
	       GtkHTMLControlData *cd)
{
	gchar *domain;

	g_return_if_fail (cd->html != NULL);
	g_return_if_fail (GTK_IS_HTML (cd->html));
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (uic));

	/* printf ("xml: %s/%s\n", GTKHTML_DATADIR, "GNOME_GtkHTML_Editor.xml"); */

	/*
	  FIXME

	  we should pass domain to bonobo (once it provides such functionality in its API)
	  now we could "steal" domain from other threads until it's restored back
	  also hopefully no one else is doing this hack so we end with the right domain :(
	*/

	domain = g_strdup (textdomain (NULL));
	textdomain (GNOME_EXPLICIT_TRANSLATION_DOMAIN);
	bonobo_ui_component_add_verb_list_with_data (uic, editor_verbs, cd);
	bonobo_ui_util_set_ui (uic, GTKHTML_DATADIR, "GNOME_GtkHTML_Editor.xml", "GNOME_GtkHTML_Editor", NULL);

	spell_create_language_menu (cd);
	menubar_update_format (cd);

	textdomain (domain);
	g_free (domain);

	menubar_paragraph_style_changed_cb (cd->html, gtk_html_get_paragraph_style (cd->html), cd);
	g_signal_connect (cd->html, "current_paragraph_style_changed", G_CALLBACK (menubar_paragraph_style_changed_cb), cd);
}
