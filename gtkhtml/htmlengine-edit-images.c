/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.
 *
 *  Copyright (C) 2000 Helix Code, Inc.
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
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
*/

#include <config.h>
#include "htmlengine.h"
#include "htmlimage.h"
#include "htmlcluealigned.h"

#include "htmlengine-edit-cut-and-paste.h"
#include "htmlengine-edit-images.h"


void
html_engine_insert_image (HTMLEngine *e,
                          const gchar *file,
                          const gchar *url,
                          const gchar *target,
                          gint16 width,
                          gint16 height,
                          gboolean percent_width,
                          gboolean percent_height,
                          gint8 border,
                          HTMLColor *border_color,
                          HTMLHAlignType halign,
                          HTMLVAlignType valign,
                          gint8 hspace,
                          gint8 vspace,
                          gboolean reload)
{
	HTMLObject *image;

	g_return_if_fail (e != NULL);
	g_return_if_fail (HTML_IS_ENGINE (e));

	image = html_image_new (html_engine_get_image_factory (e),
				file,
				url,
				target,
				width, height,
				percent_width, percent_height,
				border,
				border_color,
				valign, reload);

	html_image_set_spacing (HTML_IMAGE (image), hspace, vspace);

	html_engine_paste_object (e, image, 1);
}
