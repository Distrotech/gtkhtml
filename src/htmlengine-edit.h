/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.
    Copyright (C) 1999, 2000 Helix Code, Inc.

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

#ifndef _HTMLENGINE_EDIT_H
#define _HTMLENGINE_EDIT_H

#include <glib.h>
#include <unicode.h>

#include "htmlengine.h"


/* Undo/redo.  */
void  html_engine_undo                 (HTMLEngine *e);
void  html_engine_redo                 (HTMLEngine *e);
void  html_engine_set_mark             (HTMLEngine *e);
void  html_engine_select_word_editable (HTMLEngine *e);
void  html_engine_select_line_editable (HTMLEngine *e);

void  html_engine_cut_buffer_push      (HTMLEngine *e);
void  html_engine_cut_buffer_pop       (HTMLEngine *e);

void  html_engine_selection_push       (HTMLEngine *e);
void  html_engine_selection_pop        (HTMLEngine *e);

void  html_engine_cut_and_paste_begin  (HTMLEngine *e,
					gchar *op_name);
void  html_engine_cut_and_paste_end    (HTMLEngine *e);
void  html_engine_cut_and_paste        (HTMLEngine *e,
					gchar *op_name,
					GFunc iterator,
					gpointer data);
#ifdef GTKHTML_HAVE_PSPELL
void  html_engine_spell_check_range    (HTMLEngine *e,
					HTMLCursor *begin,
					HTMLCursor *end);
#endif
gboolean html_is_in_word               (unicode_char_t uc);

#endif /* _HTMLENGINE_EDIT_H */
