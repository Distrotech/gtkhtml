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

    Author: Ettore Perazzoli <ettore@helixcode.com>
*/


#include <glib.h>

#include "htmlobject.h"
#include "htmlclueflow.h"
#include "htmlcursor.h"
#include "htmltext.h"
#include "htmltextslave.h"

#include "htmlengine-edit.h"


void
html_engine_undo (HTMLEngine *e)
{
	HTMLUndo *undo;

	g_return_if_fail (e != NULL);
	g_return_if_fail (HTML_IS_ENGINE (e));
	g_return_if_fail (e->undo != NULL);
	g_return_if_fail (e->editable);

	g_warning ("*Undo!*");

	html_engine_freeze (e);

	undo = e->undo;
	html_undo_do_undo (undo, e);

	html_engine_thaw (e);
}

void
html_engine_redo (HTMLEngine *e)
{
	HTMLUndo *undo;

	g_return_if_fail (e != NULL);
	g_return_if_fail (HTML_IS_ENGINE (e));
	g_return_if_fail (e->undo != NULL);

	g_warning ("*Redo!*");

	html_engine_freeze (e);

	undo = e->undo;
	html_undo_do_redo (undo, e);

	html_engine_thaw (e);
}


void
html_engine_set_mark (HTMLEngine *e)
{
	g_return_if_fail (e != NULL);
	g_return_if_fail (HTML_IS_ENGINE (e));
	g_return_if_fail (e->editable);

	if (e->mark != NULL)
		html_engine_unselect_all (e, TRUE);

	e->mark = html_cursor_dup (e->cursor);

	html_engine_edit_selection_updater_reset (e->selection_updater);
	html_engine_edit_selection_updater_schedule (e->selection_updater);
}
