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

/* FIXME: Should use BonoboUIHandler.  */

#include <gnome.h>
#include <bonobo.h>

#include "toolbar.h"
#include "utils.h"
#include "gtk-combo-box.h"
#include "htmlcolor.h"
#include "htmlengine-edit-fontstyle.h"


#define EDITOR_TOOLBAR_PATH "/HTMLEditor"


/* Paragraph style option menu.  */

static struct {
	GtkHTMLParagraphStyle style;
	const gchar *description;
} paragraph_style_items[] = {
	{ GTK_HTML_PARAGRAPH_STYLE_NORMAL, _("Normal") },
	{ GTK_HTML_PARAGRAPH_STYLE_H1, _("Header 1") },
	{ GTK_HTML_PARAGRAPH_STYLE_H2, _("Header 2") },
	{ GTK_HTML_PARAGRAPH_STYLE_H3, _("Header 3") },
	{ GTK_HTML_PARAGRAPH_STYLE_H4, _("Header 4") },
	{ GTK_HTML_PARAGRAPH_STYLE_H5, _("Header 5") },
	{ GTK_HTML_PARAGRAPH_STYLE_H6, _("Header 6") },
	{ GTK_HTML_PARAGRAPH_STYLE_ADDRESS, _("Address") },
	{ GTK_HTML_PARAGRAPH_STYLE_PRE, _("Pre") },
	{ GTK_HTML_PARAGRAPH_STYLE_ITEMDIGIT, _("List item (digit)") },
	{ GTK_HTML_PARAGRAPH_STYLE_ITEMDOTTED, _("List item (unnumbered)") },
	{ GTK_HTML_PARAGRAPH_STYLE_ITEMROMAN, _("List item (roman)") },
	{ GTK_HTML_PARAGRAPH_STYLE_NORMAL, NULL },
};

static void
paragraph_style_changed_cb (GtkHTML *html,
			    GtkHTMLParagraphStyle style,
			    gpointer data)
{
	GtkOptionMenu *option_menu;
	guint i;

	option_menu = GTK_OPTION_MENU (data);

	for (i = 0; paragraph_style_items[i].description != NULL; i++) {
		if (paragraph_style_items[i].style == style) {
			gtk_option_menu_set_history (option_menu, i);
			return;
		}
	}

	g_warning ("Editor component toolbar: unknown paragraph style %d", style);
}

static void
paragraph_style_menu_item_activated_cb (GtkWidget *widget,
					gpointer data)
{
	GtkHTMLParagraphStyle style;
	GtkHTML *html;

	html = GTK_HTML (data);
	style = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (widget), "paragraph_style_value"));

	/* g_warning ("Setting paragraph style to %d.", style); */

	gtk_html_set_paragraph_style (html, style);
}

static GtkWidget *
setup_paragraph_style_option_menu (GtkHTML *html)
{
	GtkWidget *option_menu;
	GtkWidget *menu;
	guint i;

	option_menu = gtk_option_menu_new ();
	menu = gtk_menu_new ();

	for (i = 0; paragraph_style_items[i].description != NULL; i++) {
		GtkWidget *menu_item;

		menu_item = gtk_menu_item_new_with_label (paragraph_style_items[i].description);
		gtk_widget_show (menu_item);

		gtk_menu_append (GTK_MENU (menu), menu_item);

		gtk_object_set_data (GTK_OBJECT (menu_item), "paragraph_style_value",
				     GINT_TO_POINTER (paragraph_style_items[i].style));
		gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
				    GTK_SIGNAL_FUNC (paragraph_style_menu_item_activated_cb),
				    html);
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);

	gtk_signal_connect (GTK_OBJECT (html), "current_paragraph_style_changed",
			    GTK_SIGNAL_FUNC (paragraph_style_changed_cb), option_menu);

	gtk_widget_show (option_menu);

	return option_menu;
}

static void
set_font_size (GtkWidget *w, GtkHTMLControlData *cd)
{
	GtkHTMLFontStyle style = GTK_HTML_FONT_STYLE_SIZE_1 + GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (w),
												    "size"));

	if (!cd->block_font_style_change)
		gtk_html_set_font_style (cd->html, GTK_HTML_FONT_STYLE_MAX & ~GTK_HTML_FONT_STYLE_SIZE_MASK, style);
}

static void
font_size_changed (GtkWidget *w, GtkHTMLParagraphStyle style, GtkHTMLControlData *cd)
{
	if (style == GTK_HTML_FONT_STYLE_DEFAULT)
		style = GTK_HTML_FONT_STYLE_SIZE_3;
	cd->block_font_style_change = TRUE;
	gtk_option_menu_set_history (GTK_OPTION_MENU (cd->font_size_menu),
				     (style & GTK_HTML_FONT_STYLE_SIZE_MASK) - GTK_HTML_FONT_STYLE_SIZE_1);
	cd->block_font_style_change = FALSE;
}

static GtkWidget *
setup_font_size_option_menu (GtkHTMLControlData *cd)
{
	GtkWidget *option_menu;
	GtkWidget *menu;
	guint i;
	gchar size [3];

	cd->font_size_menu = option_menu = gtk_option_menu_new ();

	menu = gtk_menu_new ();
	size [2] = 0;

	for (i = 0; i < GTK_HTML_FONT_STYLE_SIZE_MAX; i++) {
		GtkWidget *menu_item;

		size [0] = (i>1) ? '+' : '-';
		size [1] = '0' + ((i>1) ? i - 2 : 2 - i);

		menu_item = gtk_menu_item_new_with_label (size);
		gtk_widget_show (menu_item);

		gtk_menu_append (GTK_MENU (menu), menu_item);

		gtk_object_set_data (GTK_OBJECT (menu_item), "size",
				     GINT_TO_POINTER (i));
		gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
				    GTK_SIGNAL_FUNC (set_font_size), cd);
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), 2);

	gtk_signal_connect (GTK_OBJECT (cd->html), "insertion_font_style_changed",
			    GTK_SIGNAL_FUNC (font_size_changed), cd);

	gtk_widget_show (option_menu);

	return option_menu;
}

static void
set_color (GtkWidget *w, gushort r, gushort g, gushort b, gushort a, GtkHTMLControlData *cd)
{
	HTMLColor *color = html_color_new_from_rgb (r, g, b);

	html_engine_set_color (cd->html->engine, color);
	html_color_unref (color);
}

static void
color_button_clicked (GtkWidget *w, GtkHTMLControlData *cd)
{
	gdouble r, g, b, rn, gn, bn;
	gtk_combo_box_popup_hide (GTK_COMBO_BOX (cd->combo));

	gnome_color_picker_get_d (GNOME_COLOR_PICKER (cd->cpicker), &r, &g, &b, NULL);

	rn = ((gdouble) w->style->bg [GTK_STATE_NORMAL].red)  /0xffff;
	gn = ((gdouble) w->style->bg [GTK_STATE_NORMAL].green)/0xffff;
	bn = ((gdouble) w->style->bg [GTK_STATE_NORMAL].blue) /0xffff;

	if (r != rn || g != gn || b != bn) {
		gnome_color_picker_set_d (GNOME_COLOR_PICKER (cd->cpicker), rn, gn, bn, 1.0);
		set_color (cd->cpicker, rn*0xffff, gn*0xffff, bn*0xffff, 0xffff, cd);
	}
}

static void
default_clicked (GtkWidget *w, GtkHTMLControlData *cd)
{
	gtk_combo_box_popup_hide (GTK_COMBO_BOX (cd->combo));
	gnome_color_picker_set_d (GNOME_COLOR_PICKER (cd->cpicker), 0.0, 0.0, 0.0, 1.0);
	html_engine_set_color (cd->html->engine,
			       html_colorset_get_color (cd->html->engine->settings->color_set, HTMLTextColor));
	
}

static GtkWidget *
setup_color_combo (GtkHTMLControlData *cd)
{
       	GtkWidget *vbox, *table, *button;

	cd->cpicker = gnome_color_picker_new ();
	GTK_WIDGET_UNSET_FLAGS (cd->cpicker, GTK_CAN_FOCUS);
	vbox = gtk_vbox_new (FALSE, 2);
	button = gtk_button_new_with_label (_("Default"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked", default_clicked, cd);
	gtk_signal_connect (GTK_OBJECT (cd->cpicker), "color_set", GTK_SIGNAL_FUNC (set_color), cd);
	table = color_table_new (color_button_clicked, cd);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), button);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), table);
	cd->combo = gtk_combo_box_new (cd->cpicker, vbox);
	if (!gnome_preferences_get_toolbar_relief_btn ()) {
		gtk_combo_box_set_arrow_relief (GTK_COMBO_BOX (cd->combo), GTK_RELIEF_NONE);
		gtk_button_set_relief (GTK_BUTTON (cd->cpicker), GTK_RELIEF_NONE);
	}

	gtk_widget_show_all (cd->combo);
	return cd->combo;
}


/* Clipboard group.  */

static void
editor_toolbar_cut_cb (GtkWidget *widget, GtkHTMLControlData *cd)
{
	gtk_html_cut (GTK_HTML (cd->html));
}

static void
editor_toolbar_copy_cb (GtkWidget *widget, GtkHTMLControlData *cd)
{
	gtk_html_copy (GTK_HTML (cd->html));
}

static void
editor_toolbar_paste_cb (GtkWidget *widget, GtkHTMLControlData *cd)
{
	gtk_html_paste (GTK_HTML (cd->html));
}


/* Font style group.  */

static void
editor_toolbar_bold_cb (GtkWidget *widget, GtkHTMLControlData *cd)
{
	if (!cd->block_font_style_change) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
			gtk_html_set_font_style (GTK_HTML (cd->html),
						 GTK_HTML_FONT_STYLE_MAX,
						 GTK_HTML_FONT_STYLE_BOLD);
		else
			gtk_html_set_font_style (GTK_HTML (cd->html), ~GTK_HTML_FONT_STYLE_BOLD, 0);
	}
}

static void
editor_toolbar_italic_cb (GtkWidget *widget, GtkHTMLControlData *cd)
{
	if (!cd->block_font_style_change) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
			gtk_html_set_font_style (GTK_HTML (cd->html),
						 GTK_HTML_FONT_STYLE_MAX,
						 GTK_HTML_FONT_STYLE_ITALIC);
		else
			gtk_html_set_font_style (GTK_HTML (cd->html), ~GTK_HTML_FONT_STYLE_ITALIC, 0);
	}
}

static void
editor_toolbar_underline_cb (GtkWidget *widget, GtkHTMLControlData *cd)
{
	if (!cd->block_font_style_change) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
			gtk_html_set_font_style (GTK_HTML (cd->html),
						 GTK_HTML_FONT_STYLE_MAX,
						 GTK_HTML_FONT_STYLE_UNDERLINE);
		else
			gtk_html_set_font_style (GTK_HTML (cd->html), ~GTK_HTML_FONT_STYLE_UNDERLINE, 0);
	}
}

static void
editor_toolbar_strikeout_cb (GtkWidget *widget, GtkHTMLControlData *cd)
{
	if (!cd->block_font_style_change) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
			gtk_html_set_font_style (GTK_HTML (cd->html),
						 GTK_HTML_FONT_STYLE_MAX,
						 GTK_HTML_FONT_STYLE_STRIKEOUT);
		else
			gtk_html_set_font_style (GTK_HTML (cd->html), ~GTK_HTML_FONT_STYLE_STRIKEOUT, 0);
	}
}

static void
insertion_font_style_changed_cb (GtkHTML *widget, GtkHTMLFontStyle font_style, GtkHTMLControlData *cd)
{
	cd->block_font_style_change = TRUE;

	if (font_style & GTK_HTML_FONT_STYLE_BOLD)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cd->bold_button), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cd->bold_button), FALSE);

	if (font_style & GTK_HTML_FONT_STYLE_ITALIC)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cd->italic_button), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cd->italic_button), FALSE);

	if (font_style & GTK_HTML_FONT_STYLE_UNDERLINE)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cd->underline_button), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cd->underline_button), FALSE);

	if (font_style & GTK_HTML_FONT_STYLE_STRIKEOUT)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cd->strikeout_button), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cd->strikeout_button), FALSE);

	cd->block_font_style_change = FALSE;
}

static void
color_changed_cb (GtkHTML *widget,
		  HTMLColor *color,
		  GtkHTMLControlData *cd)
{
	gnome_color_picker_set_d (GNOME_COLOR_PICKER (cd->cpicker),
				  ((gdouble) color->color.red)   / 0xffff,
				  ((gdouble) color->color.green) / 0xffff,
				  ((gdouble) color->color.blue)  / 0xffff,
				  1.0);
}


/* Alignment group.  */

static void
editor_toolbar_left_align_cb (GtkWidget *widget,
			      GtkHTMLControlData *cd)
{
	/* If the button is not active at this point, it means that the user clicked on
           some other button in the radio group.  */
	if (! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		return;

	gtk_html_set_paragraph_alignment (GTK_HTML (cd->html),
					  GTK_HTML_PARAGRAPH_ALIGNMENT_LEFT);
}

static void
editor_toolbar_center_cb (GtkWidget *widget,
			  GtkHTMLControlData *cd)
{
	/* If the button is not active at this point, it means that the user clicked on
           some other button in the radio group.  */
	if (! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		return;

	gtk_html_set_paragraph_alignment (GTK_HTML (cd->html),
					  GTK_HTML_PARAGRAPH_ALIGNMENT_CENTER);
}

static void
editor_toolbar_right_align_cb (GtkWidget *widget,
			       GtkHTMLControlData *cd)
{
	/* If the button is not active at this point, it means that the user clicked on
           some other button in the radio group.  */
	if (! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		return;

	gtk_html_set_paragraph_alignment (GTK_HTML (cd->html),
					  GTK_HTML_PARAGRAPH_ALIGNMENT_RIGHT);
}

static void
safe_set_active (GtkWidget *widget,
		 gpointer data)
{
	GtkObject *object;
	GtkToggleButton *toggle_button;

	object = GTK_OBJECT (widget);
	toggle_button = GTK_TOGGLE_BUTTON (widget);

	gtk_signal_handler_block_by_data (object, data);
	gtk_toggle_button_set_active (toggle_button, TRUE);
	gtk_signal_handler_unblock_by_data (object, data);
}

static void
paragraph_alignment_changed_cb (GtkHTML *widget,
				GtkHTMLParagraphAlignment alignment,
				GtkHTMLControlData *cd)
{
	switch (alignment) {
	case GTK_HTML_PARAGRAPH_ALIGNMENT_LEFT:
		safe_set_active (cd->left_align_button, cd);
		break;
	case GTK_HTML_PARAGRAPH_ALIGNMENT_CENTER:
		safe_set_active (cd->center_button, cd);
		break;
	case GTK_HTML_PARAGRAPH_ALIGNMENT_RIGHT:
		safe_set_active (cd->right_align_button, cd);
		break;
	default:
		g_warning ("Unknown GtkHTMLParagraphAlignment %d.", alignment);
	}
}


/* Indentation group.  */

static void
editor_toolbar_indent_cb (GtkWidget *widget,
			  GtkHTMLControlData *cd)
{
	gtk_html_indent (GTK_HTML (cd->html), +1);
}

static void
editor_toolbar_unindent_cb (GtkWidget *widget,
			    GtkHTMLControlData *cd)
{
	gtk_html_indent (GTK_HTML (cd->html), -1);
}

/* undo/redo */

static void
editor_toolbar_undo_cb (GtkWidget *widget,
			GtkHTMLControlData *cd)
{
	gtk_html_undo (GTK_HTML (cd->html));
}

static void
editor_toolbar_redo_cb (GtkWidget *widget,
			GtkHTMLControlData *cd)
{
	gtk_html_redo (GTK_HTML (cd->html));
}



/* Editor toolbar.  */

static GnomeUIInfo editor_toolbar_alignment_group[] = {
	GNOMEUIINFO_ITEM_STOCK (N_("Left align"), N_("Left justify paragraphs"),
				editor_toolbar_left_align_cb, GNOME_STOCK_PIXMAP_ALIGN_LEFT),
	GNOMEUIINFO_ITEM_STOCK (N_("Center"), N_("Centers paragraphs"),
				editor_toolbar_center_cb, GNOME_STOCK_PIXMAP_ALIGN_CENTER),
	GNOMEUIINFO_ITEM_STOCK (N_("Right align"), N_("Right justify paragraphs"),
				editor_toolbar_right_align_cb, GNOME_STOCK_PIXMAP_ALIGN_RIGHT),
	GNOMEUIINFO_END
};

static GnomeUIInfo toolbar_info[] = {
	GNOMEUIINFO_ITEM_STOCK (N_("Cut"), N_("Cut the selected region to the clipboard"),
				editor_toolbar_cut_cb, GNOME_STOCK_PIXMAP_CUT),
	GNOMEUIINFO_ITEM_STOCK (N_("Copy"), N_("Copy the selected region to the clipboard"),
				editor_toolbar_copy_cb, GNOME_STOCK_PIXMAP_COPY),
	GNOMEUIINFO_ITEM_STOCK (N_("Paste"), N_("Paste contents of the clipboard"),
				editor_toolbar_paste_cb, GNOME_STOCK_PIXMAP_PASTE),
	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_STOCK (N_("Undo"), N_("Undo last editor operation"),
				editor_toolbar_undo_cb, GNOME_STOCK_PIXMAP_UNDO),
	GNOMEUIINFO_ITEM_STOCK (N_("Redo"), N_("Redo undone editor operation"),
				editor_toolbar_redo_cb, GNOME_STOCK_PIXMAP_REDO),

	GNOMEUIINFO_END
};

static GnomeUIInfo editor_toolbar_style_uiinfo[] = {

	{ GNOME_APP_UI_TOGGLEITEM, N_("Bold"), N_("Sets the bold font"),
	  editor_toolbar_bold_cb, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_TEXT_BOLD },
	{ GNOME_APP_UI_TOGGLEITEM, N_("Italic"), N_("Make the selection italic"),
	  editor_toolbar_italic_cb, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_TEXT_ITALIC },
	{ GNOME_APP_UI_TOGGLEITEM, N_("Underline"), N_("Make the selection underlined"),
	  editor_toolbar_underline_cb, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_TEXT_UNDERLINE },
	{ GNOME_APP_UI_TOGGLEITEM, N_("Strikeout"), N_("Make the selection striked out"),
	  editor_toolbar_strikeout_cb, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_TEXT_STRIKEOUT },

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_RADIOLIST (editor_toolbar_alignment_group),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_STOCK (N_("Unindent"), N_("Indent paragraphs less"),
				editor_toolbar_unindent_cb, GNOME_STOCK_PIXMAP_TEXT_UNINDENT),
	GNOMEUIINFO_ITEM_STOCK (N_("Indent"), N_("Indent paragraphs more"),
				editor_toolbar_indent_cb, GNOME_STOCK_PIXMAP_TEXT_INDENT),

	GNOMEUIINFO_END
};

/* static void
toolbar_destroy_cb (GtkObject *object,
		    GtkHTMLControlData *cd)
{
	if (cd->html)
		gtk_signal_disconnect (GTK_OBJECT (cd->html),
				       cd->font_style_changed_connection_id);
}

static void
html_destroy_cb (GtkObject *object,
		 GtkHTMLControlData *cd)
{
	cd->html = NULL;
} */

static GtkWidget *
create_style_toolbar (GtkHTMLControlData *cd)
{
	GtkWidget *frame, *hbox;

	hbox = gtk_hbox_new (FALSE, 0);
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);

	cd->toolbar_style = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);

	gtk_container_add (GTK_CONTAINER (frame), cd->toolbar_style);
	gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);

	gtk_toolbar_prepend_widget (GTK_TOOLBAR (cd->toolbar_style),
				    setup_paragraph_style_option_menu (cd->html),
				    NULL, NULL);
	gtk_toolbar_prepend_widget (GTK_TOOLBAR (cd->toolbar_style),
				    setup_font_size_option_menu (cd),
				    NULL, NULL);

	gnome_app_fill_toolbar_with_data (GTK_TOOLBAR (cd->toolbar_style), editor_toolbar_style_uiinfo, NULL, cd);
	gtk_toolbar_append_widget (GTK_TOOLBAR (cd->toolbar_style),
				   setup_color_combo (cd),
				   NULL, NULL);

	cd->font_style_changed_connection_id
		= gtk_signal_connect (GTK_OBJECT (cd->html), "insertion_font_style_changed",
				      GTK_SIGNAL_FUNC (insertion_font_style_changed_cb), cd);

	/* The following SUCKS!  */
	cd->bold_button = editor_toolbar_style_uiinfo[0].widget;
	cd->italic_button = editor_toolbar_style_uiinfo[1].widget;
	cd->underline_button = editor_toolbar_style_uiinfo[2].widget;
	cd->strikeout_button = editor_toolbar_style_uiinfo[3].widget;
	/* (FIXME TODO: Button for "fixed" style.)  */

	cd->left_align_button = editor_toolbar_alignment_group[0].widget;
	cd->center_button = editor_toolbar_alignment_group[1].widget;
	cd->right_align_button = editor_toolbar_alignment_group[2].widget;

	/* gtk_signal_connect (GTK_OBJECT (cd->html), "destroy",
	   GTK_SIGNAL_FUNC (html_destroy_cb), cd);

	   gtk_signal_connect (GTK_OBJECT (cd->toolbar_style), "destroy",
	   GTK_SIGNAL_FUNC (toolbar_destroy_cb), cd); */

	gtk_signal_connect (GTK_OBJECT (cd->html), "current_paragraph_alignment_changed",
			    GTK_SIGNAL_FUNC (paragraph_alignment_changed_cb), cd);

	gtk_signal_connect (GTK_OBJECT (cd->html), "insertion_color_changed",
			    GTK_SIGNAL_FUNC (color_changed_cb), cd);

	return hbox;
}


GtkWidget *
toolbar_style (GtkHTMLControlData *cd)
{
	g_return_val_if_fail (cd->html != NULL, NULL);
	g_return_val_if_fail (GTK_IS_HTML (cd->html), NULL);

	return create_style_toolbar (cd);
}

void
toolbar_setup (BonoboUIComponent  *uic,
	       GtkHTMLControlData *cd)
{
	BonoboUIHandlerToolbarItem *tree;
	BonoboUIHandler *uih = bonobo_ui_handler_new_from_component (uic);

	g_return_if_fail (uih != NULL);
	g_return_if_fail (BONOBO_IS_UI_HANDLER (uih));
	g_return_if_fail (cd->html != NULL);
	g_return_if_fail (GTK_IS_HTML (cd->html));

	tree = bonobo_ui_handler_toolbar_parse_uiinfo_list_with_data (toolbar_info, cd);
	bonobo_ui_handler_toolbar_add_list (uih, "/Toolbar", tree);
	bonobo_ui_handler_toolbar_free_list (tree);
}
