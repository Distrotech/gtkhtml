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
#include <atk/atkhyperlink.h>

#include "htmllinktext.h"

#include "html.h"
#include "hyperlink.h"

static void html_a11y_hyper_link_class_init    (HTMLA11YHyperLinkClass *klass);
static void html_a11y_hyper_link_init          (HTMLA11YHyperLink *a11y_hyper_link);

static void atk_action_interface_init (AtkHypertextIface *iface);

static void html_a11y_hyper_link_get_extents   (AtkComponent *component,
					  gint *x, gint *y, gint *width, gint *height, AtkCoordType coord_type);
static void html_a11y_hyper_link_get_size      (AtkComponent *component, gint *width, gint *height);

static gint html_a11y_hyper_link_get_n_hyper_links (AtkHypertext *hypertext);
static gint html_a11y_hyper_link_get_hyper_link_index (AtkHypertext *hypertext, gint char_index);

static AtkObjectClass *parent_class = NULL;

GType
html_a11y_hyper_link_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo tinfo = {
			sizeof (HTMLA11YHyperLinkClass),
			NULL,                                                      /* base init */
			NULL,                                                      /* base finalize */
			(GClassInitFunc) html_a11y_hyper_link_class_init,                /* class init */
			NULL,                                                      /* class finalize */
			NULL,                                                      /* class data */
			sizeof (HTMLA11YHyperLink),                                     /* instance size */
			0,                                                         /* nb preallocs */
			(GInstanceInitFunc) html_a11y_hyper_link_init,                   /* instance init */
			NULL                                                       /* value table */
		};

		static const GInterfaceInfo atk_action_info = {
			(GInterfaceInitFunc) atk_action_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = g_type_register_static (ATK_TYPE_HYPERLINK, "HTMLA11YHyperLink", &tinfo, 0);
		g_type_add_interface_static (type, ATK_TYPE_ACTION, &atk_action_info);
	}

	return type;
}

static void
atk_action_interface_init (AtkHypertextIface *iface)
{
	g_return_if_fail (iface != NULL);
}

static void
html_a11y_hyper_link_finalize (GObject *obj)
{
	HTMLA11YHyperLink *hl = HTML_A11Y_HYPER_LINK (obj);

	if (hl->a11y)
		g_object_remove_weak_pointer (G_OBJECT (hl->a11y),
					      (gpointer *) &hl->a11y);

	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
html_a11y_hyper_link_class_init (HTMLA11YHyperLinkClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize = html_a11y_hyper_link_finalize;
}

static void
html_a11y_hyper_link_init (HTMLA11YHyperLink *a11y_hyper_link)
{
}

AtkHyperlink * 
html_a11y_hyper_link_new (HTMLA11Y *a11y)
{
	HTMLA11YHyperLink *hl;

	g_return_val_if_fail (HTML_IS_HTML_A11Y (a11y), NULL);

	hl = HTML_A11Y_HYPER_LINK (g_object_new (G_TYPE_HTML_A11Y_HYPER_LINK, NULL));

	hl->a11y = a11y;
	g_object_add_weak_pointer (G_OBJECT (hl->a11y), (gpointer *) &hl->a11y);

	return ATK_HYPERLINK (hl);
}
