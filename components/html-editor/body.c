/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.

    Copyright (C) 2000 Helix Code, Inc.
    Authors:           Radek Doulik (rodo@helixcode.com)

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
#include <libgnome/gnome-i18n.h>
#include <string.h>
#include "gi-color-combo.h"
#include "htmlengine-edit.h"
#include "htmlengine-edit-clueflowstyle.h"
#include "htmlimage.h"
#include "htmlcolor.h"
#include "htmlcolorset.h"
#include "htmlsettings.h"
#include "body.h"
#include "properties.h"
#include "utils.h"

struct _GtkHTMLEditBodyProperties {
	GtkHTMLControlData *cd;

	GtkWidget *pixmap_entry;
	GtkWidget *option_template;
	GtkWidget *combo [3];
	GtkWidget *entry_title;

	GdkColor   color [HTMLColors];
	gboolean   color_changed [HTMLColors];
	gint       left_margin;

	gint template;
	GtkHTML   *sample;
};
typedef struct _GtkHTMLEditBodyProperties GtkHTMLEditBodyProperties;

#define TEMPLATES 9
typedef struct {
	gchar *name;
	gchar *bg_pixmap;
	GdkColor bg_color;
	GdkColor text_color;
	GdkColor link_color;
	gint left_margin;
} BodyTemplate;

static BodyTemplate body_templates [TEMPLATES] = {
	{
		N_("None"),
		NULL,
		{0, 0, 0, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
		10,
	},
	{
		N_("Perforated Paper"),
		ICONDIR "/paper.png",
		{0, 0xffff, 0xffff, 0xffff},
		{0, 0, 0, 0},
		{0, 0, 0x3380, 0x6680},
		30,
	},
	{
		N_("Blue Ink"),
		ICONDIR "/texture.png",
		{0, 0xffff, 0xffff, 0xffff},
		{0, 0x1fff, 0x1fff, 0x8fff},
		{0, 0, 0, 0xffff},
		10,
	},
	{
		N_("Paper"),
		ICONDIR "/rect.png",
		{0, 0xffff, 0xffff, 0xffff},
		{0, 0, 0, 0},
		{0, 0, 0, 0xffff},
		10,
  	},
	{
		N_("Ribbon"),
		ICONDIR "/ribbon.jpg",
		{0, 0xffff, 0xffff, 0xffff},
		{0, 0, 0, 0},
		{0, 0x9900, 0x3300, 0x6600},
		70,
  	},
	{
		N_("Midnight"),
		ICONDIR "/midnight-stars.jpg",
		{0, 0, 0, 0},
		{0, 0xffff, 0xffff, 0xffff},
		{0, 0xffff, 0x9900, 0},
		10,
  	},
	{
		N_("Confidential"),
		ICONDIR "/confidential-stamp.jpg",
		{0, 0xffff, 0xffff, 0xffff},
		{0, 0, 0, 0},
		{0, 0, 0, 0xffff},
		10,
  	},
	{
		N_("Draft"),
		ICONDIR "/draft-stamp.jpg",
		{0, 0xffff, 0xffff, 0xffff},
		{0, 0, 0, 0},
		{0, 0, 0, 0xffff},
		10,
  	},
	{
		N_("Graph paper"),
		ICONDIR "/draft-paper.png",
		{0, 0xffff, 0xffff, 0xffff},
		{0, 0, 0, 0x8000},
		{0, 0xe300, 0x2100, 0x2300},
		10,
  	},
};

static void
fill_sample (GtkHTMLEditBodyProperties *d)
{
	gchar *body, *body_tag, *bg_image, *lm;
	const gchar *fname;

	fname = gtk_entry_get_text (GTK_ENTRY (gnome_file_entry_gtk_entry
					       (GNOME_FILE_ENTRY (d->pixmap_entry))));
	bg_image = fname && *fname ? g_strdup_printf (" BACKGROUND=\"%s\"", fname) : g_strdup ("");
	lm = d->left_margin != 10 ? g_strdup_printf (" LEFTMARGIN=%d", d->left_margin) : g_strdup ("");
	body_tag = g_strdup_printf ("<BODY BGCOLOR=#%02x%02x%02x LINK=#%02x%02x%02x TEXT=#%02x%02x%02x%s%s>",
				    d->color [HTMLBgColor].red >> 8,
				    d->color [HTMLBgColor].green >> 8,
				    d->color [HTMLBgColor].blue >> 8,
				    d->color [HTMLLinkColor].red >> 8,
				    d->color [HTMLLinkColor].green >> 8,
				    d->color [HTMLLinkColor].blue >> 8,
				    d->color [HTMLTextColor].red >> 8,
				    d->color [HTMLTextColor].green >> 8,
				    d->color [HTMLTextColor].blue >> 8,
				    bg_image, lm);

	body  = g_strconcat (body_tag,
			     /* string marked for translations is sample text. you may want to translate it to common sample text in your language */
			     _("The quick brown <a href=\"mailto:fox\">fox</a> jumps over the lazy <a href=\"mailto:dog\">dog</a>."),
			     NULL);

	gtk_html_load_from_string (d->sample, body, -1);
	g_free (bg_image);
	g_free (lm);
	g_free (body_tag);
	g_free (body);
}

static void
color_changed (GtkWidget *w, GdkColor *color, gboolean custom, gboolean by_user, gboolean is_default,
	       GtkHTMLEditBodyProperties *data)
{
	gint idx;

	/* If the color was changed programatically there's not need to set things */
	if (!by_user)
		return;
		
	idx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "type"));
	data->color [idx] = color
		? *color
		: html_colorset_get_color (data->cd->html->engine->defaultSettings->color_set, idx)->color;
	data->color_changed [idx] = TRUE;
	gtk_html_edit_properties_dialog_change (data->cd->properties_dialog);
	fill_sample (data);
}

/* static gboolean
hide_preview (GtkHTMLEditBodyProperties *d)
{
	gnome_pixmap_entry_set_preview (GNOME_PIXMAP_ENTRY (d->pixmap_entry), FALSE);

	return FALSE;
} */

static void
entry_changed (GtkWidget *w, GtkHTMLEditBodyProperties *data)
{
	gtk_html_edit_properties_dialog_change (data->cd->properties_dialog);	
	fill_sample (data);
}

static void
changed_template (GtkWidget *w, GtkHTMLEditBodyProperties *d)
{
	d->template = g_list_index (GTK_MENU_SHELL (w)->children, gtk_menu_get_active (GTK_MENU (w)));

	gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (d->pixmap_entry))),
			    body_templates [d->template].bg_pixmap ? body_templates [d->template].bg_pixmap : "");

	if (d->template) {
		color_combo_set_color (COLOR_COMBO (d->combo [2]), &body_templates [d->template].bg_color);
		color_combo_set_color (COLOR_COMBO (d->combo [0]), &body_templates [d->template].text_color);
		color_combo_set_color (COLOR_COMBO (d->combo [1]), &body_templates [d->template].link_color);

		d->color [HTMLBgColor] = body_templates [d->template].bg_color;
		d->color_changed [HTMLBgColor] = TRUE;
		d->color [HTMLLinkColor] = body_templates [d->template].link_color;
		d->color_changed [HTMLLinkColor] = TRUE;
		d->color [HTMLTextColor] = body_templates [d->template].text_color;
		d->color_changed [HTMLTextColor] = TRUE;

		d->left_margin = body_templates [d->template].left_margin;
	} else {
		color_combo_set_color (COLOR_COMBO (d->combo [2]),
				       &html_colorset_get_color_allocated (d->cd->html->engine->painter,
									   HTMLBgColor)->color);
		color_combo_set_color (COLOR_COMBO (d->combo [0]),
				       &html_colorset_get_color_allocated (d->cd->html->engine->painter,
									   HTMLTextColor)->color);
		color_combo_set_color (COLOR_COMBO (d->combo [1]),
				       &html_colorset_get_color_allocated (d->cd->html->engine->painter,
									   HTMLLinkColor)->color);

		d->color [HTMLBgColor] = html_colorset_get_color (d->cd->html->engine->settings->color_set,
								  HTMLBgColor)->color;
		d->color_changed [HTMLBgColor] = FALSE;
		d->color [HTMLLinkColor] = html_colorset_get_color (d->cd->html->engine->settings->color_set,
								    HTMLLinkColor)->color;
		d->color_changed [HTMLLinkColor] = FALSE;
		d->color [HTMLTextColor] = html_colorset_get_color (d->cd->html->engine->settings->color_set,
								    HTMLTextColor)->color;
		d->color_changed [HTMLTextColor] = FALSE;

		d->left_margin = 10;
	}

	gtk_html_edit_properties_dialog_change (d->cd->properties_dialog);	
	fill_sample (d);
}

static void
fill_templates (GtkHTMLEditBodyProperties *d)
{
	GtkWidget *menu;
	gint i;

	menu = gtk_menu_new ();

	for (i = 0; i < TEMPLATES; i ++) {
		GtkWidget *item;

		item = gtk_menu_item_new_with_label (_(body_templates [i].name));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
		/* RM2 gdk_color_alloc (gdk_window_get_colormap (GTK_WIDGET (d->cd->html)->window), &body_templates [i].bg_color);
		   gdk_color_alloc (gdk_window_get_colormap (GTK_WIDGET (d->cd->html)->window), &body_templates [i].text_color);
		   gdk_color_alloc (gdk_window_get_colormap (GTK_WIDGET (d->cd->html)->window), &body_templates [i].link_color); */
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (d->option_template), menu);
}

GtkWidget *
body_properties (GtkHTMLControlData *cd, gpointer *set_data)
{
	GtkHTMLEditBodyProperties *data = g_new0 (GtkHTMLEditBodyProperties, 1);
	GtkWidget *hbox, *vbox, *combo, *table, *label, *t1;
	GSList *group;
	HTMLColor *color;
	gint i;

	*set_data = data;
	data->cd = cd;
	data->left_margin = 10;

	table = gtk_table_new (3, 2, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (table), 18);
	gtk_table_set_row_spacings (GTK_TABLE (table), 18);

	t1 = gtk_table_new (2, 2, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (t1), 12);
	gtk_table_set_row_spacings (GTK_TABLE (t1), 4);

	data->option_template = gtk_option_menu_new ();
	fill_templates (data);
	g_signal_connect (gtk_option_menu_get_menu (GTK_OPTION_MENU (data->option_template)),
			  "selection-done", G_CALLBACK (changed_template), data);
	editor_hig_attach_row (t1, _("T_emplate:"), data->option_template, 0);
	
	data->entry_title = gtk_entry_new ();
	if (gtk_html_get_title (data->cd->html)) {
		gtk_entry_set_text (GTK_ENTRY (data->entry_title), 
				    gtk_html_get_title (data->cd->html));
	}
	g_signal_connect (data->entry_title, "changed", G_CALLBACK (entry_changed), data);
	editor_hig_attach_row (t1, _("T_itle:"), data->entry_title, 1);	

	data->pixmap_entry = gnome_pixmap_entry_new ("background_image", _("Background Image"), FALSE);
	if (cd->html->engine->bgPixmapPtr) {
		HTMLImagePointer *ip = (HTMLImagePointer *) cd->html->engine->bgPixmapPtr;
		guint off = 0;
		 if (!strncmp (ip->url, "file:", 5))
			 off = 5;

		 gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (data->pixmap_entry))),
				     ip->url + off);
	}

	g_signal_connect (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (data->pixmap_entry)),
			  "changed", G_CALLBACK (entry_changed), data);

	gtk_table_attach (GTK_TABLE (table), editor_hig_vbox (_("Document"), t1), 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach (GTK_TABLE (table), editor_hig_vbox (_("Background Image"), data->pixmap_entry), 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), sample_frame (&data->sample), 0, 2, 2, 3);

	t1 = gtk_table_new (3, 2, FALSE);

	group = NULL;
	i = 0;
#define ADD_COLOR(x, ct, g) \
        data->color [ct] = html_colorset_get_color_allocated (cd->html->engine->painter, ct)->color; \
        color = html_colorset_get_color (cd->html->engine->defaultSettings->color_set, ct); \
        html_color_alloc (color, cd->html->engine->painter); \
	data->combo [i] = combo = color_combo_new (NULL, _("Automatic"), \
				 &color->color, \
				 color_group_fetch ("body_" g, cd)); \
        color_combo_set_color (COLOR_COMBO (combo), &data->color [ct]); \
        g_object_set_data (G_OBJECT (combo), "type", GINT_TO_POINTER (ct)); \
        g_signal_connect (combo, "color_changed", G_CALLBACK (color_changed), data); \
	hbox = gtk_hbox_new (FALSE, 3); \
        label = gtk_label_new_with_mnemonic (x); \
        gtk_misc_set_alignment (GTK_MISC (label), .0, .5); \
        gtk_table_attach (GTK_TABLE (t1), label, 0, 1, i, i + 1, GTK_FILL, GTK_FILL, 0, 0); \
        gtk_table_attach (GTK_TABLE (t1), combo, 1, 2, i, i + 1, GTK_FILL, GTK_FILL, 0, 0); \
        i ++

	ADD_COLOR (_("_Text:"), HTMLTextColor, "text");
	ADD_COLOR (_("_Link:"), HTMLLinkColor, "link");
	ADD_COLOR (_("_Background:"), HTMLBgColor, "bg");

	gtk_table_attach (GTK_TABLE (table), editor_hig_vbox (_("Colors"), t1), 1, 2, 0, 2, GTK_FILL, GTK_FILL, 0, 0);
	fill_sample (data);
	/* gtk_idle_add (hide_preview, data); */

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
	gtk_widget_show_all (vbox);

	return vbox;
}

gboolean
body_apply_cb (GtkHTMLControlData *cd, gpointer get_data)
{
	GtkHTMLEditBodyProperties *data = (GtkHTMLEditBodyProperties *) get_data;
	gboolean redraw = FALSE;
	const gchar *fname;

#define APPLY_COLOR(c) \
	if (data->color_changed [c]) { \
		html_colorset_set_color (cd->html->engine->settings->color_set, &data->color [c], c); \
                redraw = TRUE; \
        }

	cd->html->engine->leftBorder = data->left_margin;

	APPLY_COLOR (HTMLTextColor);
	APPLY_COLOR (HTMLLinkColor);
	APPLY_COLOR (HTMLBgColor);

	fname = gtk_entry_get_text (GTK_ENTRY (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (data->pixmap_entry))));
	if (fname && *fname) {
		HTMLEngine *e = data->cd->html->engine;
		gchar *file = g_strconcat ("file:", fname, NULL);

		if (e->bgPixmapPtr != NULL)
			html_image_factory_unregister (e->image_factory, e->bgPixmapPtr, NULL);
		e->bgPixmapPtr = html_image_factory_register (e->image_factory, NULL, file, TRUE);
		g_free (file);
		redraw = TRUE;
	}

	if (redraw)
		gtk_widget_queue_draw (GTK_WIDGET (cd->html));
	
	gtk_html_set_title (data->cd->html, gtk_entry_get_text (GTK_ENTRY (data->entry_title)));

	return TRUE;
}

void
body_close_cb (GtkHTMLControlData *cd, gpointer get_data)
{
	g_free (get_data);
}
