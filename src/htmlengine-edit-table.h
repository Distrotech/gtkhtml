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
*/

#ifndef _HTMLENGINE_EDIT_TABLES_H
#define _HTMLENGINE_EDIT_TABLES_H

#include "htmltypes.h"
#include "htmlenums.h"

void  html_engine_insert_table_1_1        (HTMLEngine *e);
void  html_engine_insert_table_column     (HTMLEngine *e,
					   gboolean    after);
void  html_engine_delete_table_column     (HTMLEngine *e);
void  html_engine_insert_table_row        (HTMLEngine *e,
					   gboolean    after);
void  html_engine_delete_table_row        (HTMLEngine *e);
void  html_engine_table_set_border_width  (HTMLEngine *e,
					   gint        border_width,
					   gboolean    relative);
void  html_engine_table_set_bg_color      (HTMLEngine *e,
					   HTMLTable  *t,
					   GdkColor   *c);
			       
#endif

