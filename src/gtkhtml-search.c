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

#include "config.h"
#include "gtkhtml-search.h"

static void
search (GtkWidget *but, GtkHTMLSearchDialog *d)
{
	printf ("search\n");

	html_engine_search (d->html->engine, gtk_entry_get_text (GTK_ENTRY (d->entry)),
			    GTK_TOGGLE_BUTTON (d->case_sensitive)->active,
			    GTK_TOGGLE_BUTTON (d->backward)->active == 0, d->regular);
}

static void
entry_changed (GtkWidget *entry, GtkHTMLSearchDialog *d)
{
	printf ("entry changed\n");
}

static void
entry_activate (GtkWidget *entry, GtkHTMLSearchDialog *d)
{
	printf ("entry activate\n");
	gnome_dialog_close (d->dialog);
	search (NULL, d);
}

GtkHTMLSearchDialog *
gtk_html_search_dialog_new (GtkHTML *html, gboolean regular)
{
	GtkHTMLSearchDialog *dialog = g_new (GtkHTMLSearchDialog, 1);
	GtkWidget *hbox;

	dialog->dialog         = GNOME_DIALOG (gnome_dialog_new ((regular) ? _("Regex search") :  _("Search"), _("Search"),
								 GNOME_STOCK_BUTTON_CANCEL, NULL));
	dialog->entry          = gtk_entry_new_with_max_length (20);
	dialog->backward       = gtk_check_button_new_with_label (_("search backward"));
	dialog->case_sensitive = gtk_check_button_new_with_label (_("case sensitive"));
	dialog->html           = html;
	dialog->regular        = regular;

	hbox = gtk_hbox_new (FALSE, 0);

	gtk_box_pack_start_defaults (GTK_BOX (hbox), dialog->backward);
	gtk_box_pack_start_defaults (GTK_BOX (hbox), dialog->case_sensitive);

	gtk_box_pack_start_defaults (GTK_BOX (dialog->dialog->vbox), dialog->entry);
	gtk_box_pack_start_defaults (GTK_BOX (dialog->dialog->vbox), hbox);
	gtk_widget_show (dialog->entry);
	gtk_widget_show_all (hbox);

	gnome_dialog_button_connect (dialog->dialog, 0, search, dialog);
	gnome_dialog_close_hides (dialog->dialog, TRUE);
	gnome_dialog_set_close (dialog->dialog, TRUE);

	gnome_dialog_set_default (dialog->dialog, 0);
	gtk_widget_grab_focus (dialog->entry);

	gtk_signal_connect (GTK_OBJECT (dialog->entry), "changed",
			    entry_changed, dialog);
	gtk_signal_connect (GTK_OBJECT (dialog->entry), "activate",
			    entry_activate, dialog);

	return dialog;
}

void
gtk_html_search_dialog_destroy (GtkHTMLSearchDialog *d)
{
	g_free (d);
}

void
gtk_html_search_dialog_run (GtkHTMLSearchDialog *d)
{
	gint action;

	action = gnome_dialog_run (d->dialog);
}
