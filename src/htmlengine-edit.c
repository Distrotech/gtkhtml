/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.

    Copyright (C) 1999, 2000 Helix Code, Inc.
    Copyright (C) 2001 Ximian, Inc.

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

    Authors: Ettore Perazzoli <ettore@helixcode.com>
             Radek Doulik     <rodo@ximian.com>
*/


#include <config.h>
#include <ctype.h>
#include <glib.h>

#include "gtkhtml.h"
#include "gtkhtml-properties.h"

#include "htmlobject.h"
#include "htmlclueflow.h"
#include "htmlcursor.h"
#include "htmllinktext.h"
#include "htmltable.h"
#include "htmltext.h"
#include "htmltextslave.h"
#include "htmlimage.h"
#include "htmlinterval.h"
#include "htmlselection.h"
#include "htmlundo.h"

#include "htmlengine-edit.h"
#include "htmlengine-edit-cut-and-paste.h"
#include "htmlengine-edit-cursor.h"
#include "htmlengine-edit-movement.h"
#include "htmlengine-edit-selection-updater.h"
#include "htmlengine-edit-table.h"
#include "htmlengine-edit-tablecell.h"


void
html_engine_undo (HTMLEngine *e)
{
	HTMLUndo *undo;

	g_return_if_fail (e != NULL);
	g_return_if_fail (HTML_IS_ENGINE (e));
	g_return_if_fail (e->undo != NULL);
	g_return_if_fail (e->editable);

	html_engine_unselect_all (e);

	undo = e->undo;
	html_undo_do_undo (undo, e);
}

void
html_engine_redo (HTMLEngine *e)
{
	HTMLUndo *undo;

	g_return_if_fail (e != NULL);
	g_return_if_fail (HTML_IS_ENGINE (e));
	g_return_if_fail (e->undo != NULL);

	html_engine_unselect_all (e);

	undo = e->undo;
	html_undo_do_redo (undo, e);
}


void
html_engine_set_mark (HTMLEngine *e)
{
	g_return_if_fail (e != NULL);
	g_return_if_fail (HTML_IS_ENGINE (e));
	g_return_if_fail (e->editable);

	if (e->mark != NULL)
		html_engine_unselect_all (e);

	e->mark = html_cursor_dup (e->cursor);

	html_engine_edit_selection_updater_reset (e->selection_updater);
	html_engine_edit_selection_updater_schedule (e->selection_updater);
}

void
html_engine_clipboard_push (HTMLEngine *e)
{
	e->clipboard_stack = g_list_prepend (e->clipboard_stack, GUINT_TO_POINTER (e->clipboard_len));
	e->clipboard_stack = g_list_prepend (e->clipboard_stack, e->clipboard);
	e->clipboard       = NULL;
}

void
html_engine_clipboard_pop (HTMLEngine *e)
{
	g_assert (e->clipboard_stack);

	e->clipboard       = HTML_OBJECT (e->clipboard_stack->data);
	e->clipboard_stack = g_list_remove (e->clipboard_stack, e->clipboard_stack->data);
	e->clipboard_len   = GPOINTER_TO_UINT (e->clipboard_stack->data);
	e->clipboard_stack = g_list_remove (e->clipboard_stack, e->clipboard_stack->data);
}

void
html_engine_selection_push (HTMLEngine *e)
{
	if (html_engine_is_selection_active (e)) {
		e->selection_stack
			= g_list_prepend (e->selection_stack, GINT_TO_POINTER (html_cursor_get_position (e->mark)));
		e->selection_stack
			= g_list_prepend (e->selection_stack, GINT_TO_POINTER (html_cursor_get_position (e->cursor)));
		e->selection_stack = g_list_prepend (e->selection_stack, GINT_TO_POINTER (TRUE));
	} else {
		e->selection_stack = g_list_prepend (e->selection_stack, GINT_TO_POINTER (FALSE));
	}
}

void
html_engine_selection_pop (HTMLEngine *e)
{
	gboolean selection;

	g_assert (e->selection_stack);

	selection = GPOINTER_TO_INT (e->selection_stack->data);
	e->selection_stack = g_list_remove (e->selection_stack, e->selection_stack->data);

	html_engine_disable_selection (e);

	if (selection) {
		gint cursor, mark;

		cursor = GPOINTER_TO_INT (e->selection_stack->data);
		e->selection_stack = g_list_remove (e->selection_stack, e->selection_stack->data);
		mark = GPOINTER_TO_INT (e->selection_stack->data);
		e->selection_stack = g_list_remove (e->selection_stack, e->selection_stack->data);

		html_cursor_jump_to_position (e->cursor, e, mark);
		html_engine_set_mark (e);
		html_cursor_jump_to_position (e->cursor, e, cursor);
	}
	html_engine_edit_selection_updater_update_now (e->selection_updater);
}

void
html_engine_cut_and_paste_begin (HTMLEngine *e, const gchar *undo_op_name, const gchar *redo_op_name)
{
	html_engine_hide_cursor (e);
	html_engine_selection_push (e);
	html_engine_clipboard_push (e);
	html_undo_level_begin (e->undo, undo_op_name, redo_op_name);
	html_engine_cut (e);
}

void
html_engine_cut_and_paste_end (HTMLEngine *e)
{
	if (e->clipboard) {
		html_engine_paste (e);
		html_engine_clipboard_clear (e);
	}
	html_undo_level_end (e->undo);
	html_engine_clipboard_pop (e);
	html_engine_selection_pop (e);
	html_engine_show_cursor (e);
}

void
html_engine_cut_and_paste (HTMLEngine *e, const gchar *undo_op_name, const gchar *redo_op_name,
			   HTMLObjectForallFunc iterator, gpointer data)
{
	html_engine_edit_selection_updater_update_now (e->selection_updater);
	html_engine_cut_and_paste_begin (e, undo_op_name, redo_op_name);
	if (e->clipboard)
		html_object_forall (e->clipboard, e, iterator, data);
	html_engine_cut_and_paste_end (e);
}

static void
spell_check_object (HTMLObject *o, HTMLEngine *e, gpointer data)
{
	if (HTML_OBJECT_TYPE (o) == HTML_TYPE_CLUEFLOW)
		html_clueflow_spell_check (HTML_CLUEFLOW (o), e, (HTMLInterval *) data);
}

void
html_engine_spell_check_range (HTMLEngine *e, HTMLCursor *begin, HTMLCursor *end)
{
	HTMLInterval *i;

	e->need_spell_check = FALSE;

	if (!e->widget->editor_api || !GTK_HTML_CLASS (GTK_OBJECT (e->widget)->klass)->properties->live_spell_check
	    || !begin->object->parent)
		return;

	begin = html_cursor_dup (begin);
	end   = html_cursor_dup (end);

	while (html_is_in_word (html_cursor_get_prev_char (begin)))
		if (html_cursor_backward (begin, e));
	while (html_is_in_word (html_cursor_get_current_char (end)))
		if (html_cursor_forward (end, e));

	i = html_interval_new_from_cursor (begin, end);
	if (begin->object->parent != end->object->parent)
		html_interval_forall (i, e, spell_check_object, i);
	else if (HTML_IS_CLUEFLOW (begin->object->parent))
		html_clueflow_spell_check (HTML_CLUEFLOW (begin->object->parent), e, i);
	html_interval_destroy (i);
	html_cursor_destroy (begin);
	html_cursor_destroy (end);
}

gboolean
html_is_in_word (gunichar uc)
{
	/* printf ("test %d %c => %d\n", uc, uc, g_unichar_isalnum (uc) || uc == '\''); */
	return g_unichar_isalpha (uc) || uc == '\'';
}

void
html_engine_select_word_editable (HTMLEngine *e)
{
	while (html_is_in_word (html_cursor_get_prev_char (e->cursor)))
		html_cursor_backward (e->cursor, e);
	html_engine_set_mark (e);
	while (html_is_in_word (html_cursor_get_current_char (e->cursor)))
		html_cursor_forward (e->cursor, e);
}

void
html_engine_select_line_editable (HTMLEngine *e)
{
	html_engine_beginning_of_line (e);
	html_engine_set_mark (e);
	html_engine_end_of_line (e);
}

void
html_engine_select_paragraph_editable (HTMLEngine *e)
{
	html_engine_beginning_of_paragraph (e);
	html_engine_set_mark (e);
	html_engine_end_of_paragraph (e);
}

void
html_engine_select_paragraph_extended (HTMLEngine *e)
{
	gboolean fw;

	html_engine_hide_cursor (e);
	html_engine_beginning_of_paragraph (e);
	fw = html_cursor_backward (e->cursor, e);
	html_engine_set_mark (e);
	if (fw)
		html_cursor_forward (e->cursor, e);
	html_engine_end_of_paragraph (e);
	html_cursor_forward (e->cursor, e);
	html_engine_show_cursor (e);

	html_engine_update_selection_if_necessary (e);
}

void
html_engine_select_all_editable (HTMLEngine *e)
{
	html_engine_beginning_of_document (e);
	html_engine_set_mark (e);
	html_engine_end_of_document (e);
}

struct SetData {
	HTMLType      object_type;
	const gchar  *key;
	const gchar  *value;
};

static void
set_data (HTMLObject *o, HTMLEngine *e, gpointer p)
{
	struct SetData *data = (struct SetData *) p;

	if (HTML_OBJECT_TYPE (o) == data->object_type) {
		/* printf ("set data %s --> %p\n", data->key, data->value); */
		html_object_set_data (o, data->key, data->value);
	}
}

void
html_engine_set_data_by_type (HTMLEngine *e, HTMLType object_type, const gchar *key, const gchar * value)
{
	struct SetData *data = g_new (struct SetData, 1);

	/* printf ("html_engine_set_data_by_type %s\n", key); */

	data->object_type = object_type;
	data->key         = key;
	data->value       = value;

	html_object_forall (e->clue, NULL, set_data, data);

	g_free (data);
}

void
html_engine_clipboard_clear (HTMLEngine *e)
{
	if (e->clipboard) {
		html_object_destroy (e->clipboard);
		e->clipboard = NULL;
	}
}

HTMLObject *
html_engine_new_text (HTMLEngine *e, const gchar *text, gint len)
{
	return e->insertion_url
		? html_link_text_new_with_len (text, len, e->insertion_font_style, e->insertion_color,
					       e->insertion_url, e->insertion_target)
		: html_text_new_with_len      (text, len, e->insertion_font_style, e->insertion_color);
}

HTMLObject *
html_engine_new_text_empty (HTMLEngine *e)
{
	return html_engine_new_text (e, "", 0);
}

gboolean
html_engine_cursor_on_bop (HTMLEngine *e)
{
	g_assert (e);
	g_assert (e->cursor);
	g_assert (e->cursor->object);

	return e->cursor->offset == 0 && html_object_prev_not_slave (e->cursor->object) == NULL;
}

guint
html_engine_get_indent (HTMLEngine *e)
{
	g_assert (e);
	g_assert (e->cursor);
	g_assert (e->cursor->object);

	return e->cursor->object && e->cursor->object->parent
		&& HTML_OBJECT_TYPE (e->cursor->object->parent) == HTML_TYPE_CLUEFLOW
		? HTML_CLUEFLOW (e->cursor->object->parent)->level : 0;
}

#define LINE_LEN 71

static inline guint
inc_line_offset (guint line_offset, gunichar uc)
{
	return uc == '\t'
		? line_offset + 8 - (line_offset % 8)
		: line_offset + 1;
}

static guint
try_break_this_line (HTMLEngine *e, guint line_offset, guint last_space)
{
	HTMLObject *flow;
	gunichar uc;

	flow = e->cursor->object->parent;

	while (html_cursor_forward (e->cursor, e) && e->cursor->object->parent == flow) {
		uc = html_cursor_get_current_char (e->cursor);
		line_offset = inc_line_offset (line_offset, uc);
		if (uc == ' ' || uc == '\t')
			last_space = line_offset;
		if (uc && line_offset >= LINE_LEN) {
			if (last_space) {
				html_cursor_backward_n (e->cursor, e, line_offset - last_space);
				uc = ' ';
			} else {
				/* go to end of word */
				while (html_cursor_forward (e->cursor, e)) {
					line_offset = inc_line_offset (line_offset, uc);
					uc = html_cursor_get_current_char (e->cursor);
					if (uc == ' ' || uc == '\t' || !uc)
						break;
				}
			}
			if (uc == ' ' || uc == '\t') {
				html_engine_insert_empty_paragraph (e);
				html_engine_delete_n (e, 1, TRUE);

				flow        = e->cursor->object->parent;
				last_space  = 0;
				line_offset = 0;
			}
		}
		if (!uc)
			return line_offset;
	}

	return line_offset;
}

static void
go_to_begin_of_pre_para (HTMLEngine *e)
{
	HTMLObject *prev;

	do {
		html_cursor_beginning_of_paragraph (e->cursor, e);
		prev = html_object_prev_leaf (e->cursor->object);
		if (prev && html_object_get_length (prev))
			html_cursor_backward (e->cursor, e);
		else
			break;
	} while (1);
}

void
html_engine_indent_pre_paragraph (HTMLEngine *e)
{
	guint position;
	guint line_offset;
	guint last_space;

	g_assert (e->cursor->object);
	if (HTML_OBJECT_TYPE (e->cursor->object->parent) != HTML_TYPE_CLUEFLOW
	    || html_clueflow_get_style (HTML_CLUEFLOW (e->cursor->object->parent)) != HTML_CLUEFLOW_STYLE_PRE)
		return;

	html_engine_disable_selection (e);
	position = e->cursor->position;

	html_undo_level_begin (e->undo, "Indent PRE paragraph", "Reverse paragraph indentation");
	html_engine_freeze (e);

	go_to_begin_of_pre_para (e);

	line_offset = 0;
	last_space  = 0;
	do {
		line_offset = try_break_this_line (e, line_offset, last_space);
		if (html_cursor_forward (e->cursor, e)
		    && e->cursor->offset == 0 && html_object_get_length (e->cursor->object)
		    && html_object_prev_not_slave (e->cursor->object) == NULL) {
			if (line_offset < LINE_LEN - 1) {
				gunichar prev;
				html_engine_delete_n (e, 1, FALSE);
				prev = html_cursor_get_prev_char (e->cursor);
				if (prev != ' ' && prev != '\t') {
					html_engine_insert_text (e, " ", 1);
					line_offset ++;
				} else if (position > e->cursor->position)
					position --;
				last_space = line_offset - 1;
			} else {
				line_offset = 0;
				last_space  = 0;
			}
		} else
			break;
	} while (1);

	html_cursor_jump_to_position (e->cursor, e, position);
	html_engine_thaw (e);
	html_undo_level_end (e->undo);
}

void
html_engine_indent_pre_line (HTMLEngine *e)
{
	guint position;
	guint line_offset;
	guint last_space;
	HTMLObject *flow;
	gunichar uc;

	g_assert (e->cursor->object);
	if (HTML_OBJECT_TYPE (e->cursor->object->parent) != HTML_TYPE_CLUEFLOW
	    || html_clueflow_get_style (HTML_CLUEFLOW (e->cursor->object->parent)) != HTML_CLUEFLOW_STYLE_PRE)
		return;

	html_engine_disable_selection (e);
	position = e->cursor->position;

	html_undo_level_begin (e->undo, "Indent PRE paragraph", "Reverse paragraph indentation");
	html_engine_freeze (e);

	last_space  = 0;
	line_offset = 0;

	html_cursor_beginning_of_paragraph (e->cursor, e);

	flow = e->cursor->object->parent;
	while (html_cursor_forward (e->cursor, e) && e->cursor->object->parent == flow) {
		uc = html_cursor_get_current_char (e->cursor);
		line_offset = inc_line_offset (line_offset, uc);

		if (uc == ' ' || uc == '\t') {
			last_space = line_offset;
		}
		
		if (line_offset >= LINE_LEN) {
			if (last_space) {
				html_cursor_backward_n (e->cursor, e, line_offset - last_space);

				html_cursor_forward (e->cursor, e);
				if ((uc = html_cursor_get_current_char (e->cursor))) {
					html_engine_insert_empty_paragraph (e);
					if (position >= e->cursor->position)
						position++;
				}
			}	
		}
		if (!uc)
			break;
	}

	html_cursor_jump_to_position (e->cursor, e, position);
	html_engine_thaw (e);
	html_undo_level_end (e->undo);
}

void
html_engine_fill_pre_line (HTMLEngine *e)
{
	guint position;
	guint line_offset;
	guint last_space;
	HTMLObject *flow;
	gunichar uc;

	g_assert (e->cursor->object);
	position = e->cursor->position;

	if (HTML_OBJECT_TYPE (e->cursor->object->parent) != HTML_TYPE_CLUEFLOW
	    || html_clueflow_get_style (HTML_CLUEFLOW (e->cursor->object->parent)) != HTML_CLUEFLOW_STYLE_PRE) {
		return;
	}

	last_space  = 0;
	line_offset = 0;

	html_cursor_beginning_of_paragraph (e->cursor, e);

	flow = e->cursor->object->parent;
	while (html_cursor_forward (e->cursor, e) && (e->cursor->position < position - 1)) {
		uc = html_cursor_get_current_char (e->cursor);
		line_offset = inc_line_offset (line_offset, uc);
		
		if (uc == ' ' || uc == '\t') {
			last_space = line_offset;
		}
		
		if (line_offset >= LINE_LEN) {
			if (last_space) {
				html_cursor_backward_n (e->cursor, e, line_offset - last_space);
				
				html_cursor_forward (e->cursor, e);
				if ((uc = html_cursor_get_current_char (e->cursor))) {
					html_engine_insert_empty_paragraph (e);
					if (position >= e->cursor->position)
						position++;

					line_offset = 0;
					last_space = 0;
				}
			}	
		}
		if (!uc)
			break;
	}
	html_cursor_jump_to_position (e->cursor, e, position);
}

void
html_engine_space_and_fill_line (HTMLEngine *e)
{

	g_assert (e->cursor->object);
	html_undo_level_begin (e->undo, "insert and fill", "reverse insert and fill");


	html_engine_disable_selection (e);
	html_engine_freeze (e);
	html_engine_insert_text (e, " ", 1);

	html_engine_fill_pre_line (e);

	html_engine_thaw (e);
	html_undo_level_end (e->undo);
}

void
html_engine_break_and_fill_line (HTMLEngine *e)
{
	html_undo_level_begin (e->undo, "break and fill", "reverse break and fill");

	html_engine_disable_selection (e);
	html_engine_freeze (e);

	html_engine_fill_pre_line (e);

	html_engine_insert_empty_paragraph (e);
	html_engine_thaw (e);
	html_undo_level_end (e->undo);
}

gboolean
html_engine_next_cell (HTMLEngine *e, gboolean create)
{
	HTMLTableCell *cell, *current_cell;

	cell = html_engine_get_table_cell (e);
	if (cell) {
		html_engine_hide_cursor (e);
		do {
			html_cursor_end_of_line (e->cursor, e);
			html_cursor_forward (e->cursor, e);
			current_cell = html_engine_get_table_cell (e);
		} while (current_cell == cell);
			
		if (create && HTML_IS_TABLE (e->cursor->object)) {
			html_cursor_backward (e->cursor, e);
			html_engine_insert_table_row (e, TRUE);
		}
		html_engine_show_cursor (e);

		return TRUE;
	}

	return FALSE;
}

gboolean
html_engine_prev_cell (HTMLEngine *e)
{
	HTMLTableCell *cell, *current_cell;

	cell = html_engine_get_table_cell (e);
	if (cell) {
		html_engine_hide_cursor (e);
		do {
			html_cursor_beginning_of_line (e->cursor, e);
			html_cursor_backward (e->cursor, e);
			current_cell = html_engine_get_table_cell (e);
		} while (current_cell == cell);

		html_engine_show_cursor (e);

		return TRUE;
	}

	return FALSE;
}
