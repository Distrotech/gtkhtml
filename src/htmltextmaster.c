/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This file is part of the GtkHTML library.

   Copyright (C) 1997 Martin Jones (mjones@kde.org)
   Copyright (C) 1997 Torben Weis (weis@kde.org)
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

#include "htmlobject.h"
#include "htmlclue.h"
#include "htmlclueflow.h"
#include "htmltextmaster.h"
#include "htmltextslave.h"


HTMLTextMasterClass html_text_master_class;
static HTMLTextClass *parent_class = NULL;


/* HTMLObject methods.  */

static void
copy (HTMLObject *self,
      HTMLObject *dest)
{
	(* HTML_OBJECT_CLASS (parent_class)->copy) (self, dest);

	HTML_TEXT_MASTER (dest)->select_start = HTML_TEXT_MASTER (self)->select_start;
	HTML_TEXT_MASTER (dest)->select_length = HTML_TEXT_MASTER (self)->select_length;
}

static void
draw (HTMLObject *o,
      HTMLPainter *p, 
      gint x, gint y,
      gint width, gint height,
      gint tx, gint ty)
{
	/* Don't paint yourself.  */
}

static gint
calc_min_width (HTMLObject *self,
		HTMLPainter *painter)
{
	HTMLTextMaster *master;
	GtkHTMLFontStyle font_style;
	HTMLText *text;
	const gchar *p;
	gint min_width, run_width;
	guint space_width;

	master = HTML_TEXT_MASTER (self);
	text = HTML_TEXT (self);

	font_style = html_text_get_font_style (text);
	/* Check to see if we want any word wrapping, ugly but it works. */
	if ((self->parent != NULL && 
	     (HTML_OBJECT_TYPE (self->parent) == HTML_TYPE_CLUEFLOW)) &&
	    HTML_CLUEFLOW(self->parent)->style == HTML_CLUEFLOW_STYLE_NOWRAP) {

			return html_painter_calc_text_width (painter, text->text, text->text_len, font_style);
	}

	min_width = 0;
	run_width = 0;
	p = text->text;

	space_width = html_painter_calc_text_width (painter, " ", 1, font_style);

	while (1) {
		if (*p != ' ' && *p != 0) {
			run_width += html_painter_calc_text_width (painter, p, 1, font_style);
		} else {
			run_width += space_width;
			if (run_width > min_width)
				min_width = run_width;
			run_width = 0;
			if (*p == 0)
				break;
		}
		p++;
	}

	return min_width;
}

static gint
calc_preferred_width (HTMLObject *self,
		      HTMLPainter *painter)
{
	HTMLText *text;
	GtkHTMLFontStyle font_style;
	gint width;

	text = HTML_TEXT (self);
	font_style = html_text_get_font_style (text);

	width = html_painter_calc_text_width (painter,
					      text->text, text->text_len,
					      font_style);

	return width;
}

static void
calc_size (HTMLObject *self,
	   HTMLPainter *painter)
{
	self->width = 0;
	self->ascent = 0;
	self->descent = 0;
}

static HTMLFitType
fit_line (HTMLObject *o,
	  HTMLPainter *painter,
	  gboolean startOfLine,
	  gboolean firstRun,
	  gint widthLeft) 
{
	HTMLTextMaster *textmaster; 
	HTMLObject *next_obj;
	HTMLObject *text_slave;

	textmaster = HTML_TEXT_MASTER (o);

	if (o->flags & HTML_OBJECT_FLAG_NEWLINE)
		return HTML_FIT_COMPLETE;
	
	/* Remove existing slaves */
	next_obj = o->next;
	while (next_obj != NULL
	       && (HTML_OBJECT_TYPE (next_obj) == HTML_TYPE_TEXTSLAVE)) {
		o->next = next_obj->next;
		html_clue_remove (HTML_CLUE (next_obj->parent), next_obj);
		html_object_destroy (next_obj);
		next_obj = o->next;
	}
	
	/* Turn all text over to our slaves */
	text_slave = html_text_slave_new (textmaster, 0, HTML_TEXT (textmaster)->text_len);
	html_clue_append_after (HTML_CLUE (o->parent), text_slave, o);

	return HTML_FIT_COMPLETE;
}

static HTMLObject *
check_point (HTMLObject *self,
	     HTMLPainter *painter,
	     gint x, gint y,
	     guint *offset_return,
	     gboolean for_cursor)
{
	HTMLObject *p;

	/* This scans all the HTMLTextSlaves that represent the various lines
           in which the text is split.  */
	for (p = self->next; p != NULL && HTML_OBJECT_TYPE (p) == HTML_TYPE_TEXTSLAVE; p = p->next) {
		if (y < p->y + p->descent) {
			/* If the cursor is on this line, there is a newline
                           after this, and we want cursor-line behavior, then
                           the position we want is the last on this line.  */
			if (for_cursor
			    && (p->next == NULL
				|| (p->next->flags & HTML_OBJECT_FLAG_NEWLINE)
				|| HTML_OBJECT_TYPE (p->next) == HTML_TYPE_TEXTSLAVE)
			    && x >= p->x + p->width) {
				if (offset_return != NULL) {
					*offset_return = (HTML_TEXT_SLAVE (p)->posStart
							  + HTML_TEXT_SLAVE (p)->posLen);
					if (p->next == NULL
					    || (p->next->flags & HTML_OBJECT_FLAG_NEWLINE))
						(*offset_return)++;
				}
				return self;
			}

			/* Otherwise, we have to do the check the normal way.  */
			if (x >= p->x && x < p->x + p->width) {
				if (offset_return != NULL) {
					*offset_return = html_text_slave_get_offset_for_pointer
						(HTML_TEXT_SLAVE (p), painter, x, y);
					*offset_return += HTML_TEXT_SLAVE (p)->posStart;
				}
				return self;
			}
		}
	}

	return NULL;
}

static gboolean
select_range (HTMLObject *self,
	      HTMLEngine *engine,
	      guint offset,
	      gint length,
	      gboolean queue_draw)
{
	HTMLTextMaster *master;
	HTMLObject *p;
	gboolean changed;

	master = HTML_TEXT_MASTER (self);

	if (length < 0)
		length = HTML_TEXT (self)->text_len;

	if (offset != master->select_start || length != master->select_length)
		changed = TRUE;
	else
		changed = FALSE;

	if (queue_draw) {
		for (p = self->next;
		     p != NULL && HTML_OBJECT_TYPE (p) == HTML_TYPE_TEXTSLAVE;
		     p = p->next) {
			HTMLTextSlave *slave;
			gboolean was_selected, is_selected;
			guint max;

			slave = HTML_TEXT_SLAVE (p);

			max = slave->posStart + slave->posLen;

			if (master->select_start + master->select_length > slave->posStart
			    && master->select_start < max)
				was_selected = TRUE;
			else
				was_selected = FALSE;

			if (offset + length > slave->posStart &&
			    offset < max)
				is_selected = TRUE;
			else
				is_selected = FALSE;

			if (was_selected && is_selected) {
				gint diff1, diff2;

				diff1 = offset - slave->posStart;
				diff2 = master->select_start - slave->posStart;

				if (diff1 != diff2) {
					html_engine_queue_draw (engine, p);
				} else {
					diff1 = offset + length - slave->posStart;
					diff2 = (master->select_start + master->select_length
						 - slave->posStart);

					if (diff1 != diff2)
						html_engine_queue_draw (engine, p);
				}
			} else {
				if ((! was_selected && is_selected) || (was_selected && ! is_selected))
					html_engine_queue_draw (engine, p);
			}
		}
	}

	master->select_start = offset;
	master->select_length = length;

	if (length == 0)
		self->selected = FALSE;
	else
		self->selected = TRUE;

	return changed;
}

static HTMLObject *
get_selection (HTMLObject *self)
{
	HTMLObject *new;
	gchar *text;

	if (! self->selected)
		return NULL;

	text = g_strndup (HTML_TEXT (self)->text + HTML_TEXT_MASTER (self)->select_start,
			  HTML_TEXT_MASTER (self)->select_length);

	new = html_text_master_new (text, HTML_TEXT (self)->font_style, & (HTML_TEXT (self)->color));

	return new;
}

static void
get_cursor (HTMLObject *self,
	    HTMLPainter *painter,
	    guint offset,
	    gint *x1, gint *y1,
	    gint *x2, gint *y2)
{
	HTMLObject *slave;
	guint ascent, descent;

	html_object_get_cursor_base (self, painter, offset, x2, y2);

	slave = self->next;
	if (slave == NULL || HTML_OBJECT_TYPE (slave) != HTML_TYPE_TEXTSLAVE) {
		ascent = 0;
		descent = 0;
	} else {
		ascent = slave->ascent;
		descent = slave->descent;
	}

	*x1 = *x2;
	*y1 = *y2 - ascent;
	*y2 += descent - 1;
}

static void
get_cursor_base (HTMLObject *self,
		 HTMLPainter *painter,
		 guint offset,
		 gint *x, gint *y)
{
	HTMLObject *obj;

	for (obj = self->next; obj != NULL; obj = obj->next) {
		HTMLTextSlave *slave;

		if (HTML_OBJECT_TYPE (obj) != HTML_TYPE_TEXTSLAVE)
			break;

		slave = HTML_TEXT_SLAVE (obj);

		if (offset < slave->posStart + slave->posLen
		    || obj->next == NULL
		    || HTML_OBJECT_TYPE (obj->next) != HTML_TYPE_TEXTSLAVE) {
			html_object_calc_abs_position (obj, x, y);
			if (offset != slave->posStart) {
				HTMLText *text;
				GtkHTMLFontStyle font_style;

				text = HTML_TEXT (self);

				font_style = html_text_get_font_style (text);
				*x += html_painter_calc_text_width (painter,
								    text->text + slave->posStart,
								    offset - slave->posStart,
								    font_style);
			}

			return;
		}
	}
}


/* HTMLText methods.  */

static void
queue_draw (HTMLText *text,
	    HTMLEngine *engine,
	    guint offset,
	    guint len)
{
	HTMLObject *obj;

	for (obj = HTML_OBJECT (text)->next; obj != NULL; obj = obj->next) {
		HTMLTextSlave *slave;

		if (HTML_OBJECT_TYPE (obj) != HTML_TYPE_TEXTSLAVE)
			continue;

		slave = HTML_TEXT_SLAVE (obj);

		if (offset < slave->posStart + slave->posLen
		    && (len == 0 || offset + len >= slave->posStart)) {
			html_engine_queue_draw (engine, obj);
			if (len != 0 && slave->posStart + slave->posLen > offset + len)
				break;
		}
	}
}

static HTMLText *
split (HTMLText *self,
       guint offset)
{
	HTMLTextMaster *master;
	HTMLObject *new;
	gchar *s;

	master = HTML_TEXT_MASTER (self);

	if (offset >= HTML_TEXT (self)->text_len || offset == 0)
		return NULL;

	s = g_strdup (self->text + offset);
	new = html_text_master_new (s, self->font_style, &self->color);

	self->text = g_realloc (self->text, offset + 1);
	self->text[offset] = '\0';
	HTML_TEXT (self)->text_len = offset;

	if (master->select_length != 0) {
		if (offset <= master->select_start) {
			HTML_TEXT_MASTER (new)->select_start = master->select_start - offset;
			HTML_TEXT_MASTER (new)->select_length = master->select_length;
			new->selected = TRUE;

			HTML_TEXT_MASTER (self)->select_start = 0;
			HTML_TEXT_MASTER (self)->select_length = 0;
			HTML_OBJECT (self)->selected = FALSE;
		} else if (offset <= master->select_start + master->select_length) {
			HTML_TEXT_MASTER (new)->select_start = 0;
			HTML_TEXT_MASTER (new)->select_length
				= master->select_start + master->select_length - offset;
			new->selected = HTML_TEXT_MASTER (new)->select_length > 0;

			HTML_TEXT_MASTER (self)->select_length = offset - master->select_start;
			HTML_OBJECT (self)->selected = TRUE;

		} else {
			/* If the offset is past the selection, the new element
                           gets no selection and the selection in this element
                           is unchanged.  */
		}
	}

	return HTML_TEXT (new);
}

static void
merge (HTMLText *self,
       HTMLText **list)
{
	HTMLText **p;
	guint total_length;
	guint select_start, select_length;
	gchar *new_text;
	gchar *sp;

	total_length = self->text_len;

	select_length = HTML_TEXT_MASTER (self)->select_length;
	select_start = 0;

	for (p = list; *p != NULL; p++) {
		g_return_if_fail (HTML_OBJECT_TYPE (*p) == HTML_OBJECT_TYPE (self));

		/* XXX For this to make sense, selection must always be
                   contiguous.  */

		if (HTML_TEXT_MASTER (*p)->select_length > 0) {
			if (select_length == 0)
				select_start = total_length + HTML_TEXT_MASTER (*p)->select_start;
			select_length += HTML_TEXT_MASTER (*p)->select_length;
		}

		if (! HTML_OBJECT (self)->selected && HTML_OBJECT (*p)->selected)
			HTML_TEXT_MASTER (self)->select_start = (total_length
								 + HTML_TEXT_MASTER (*p)->select_start);

		total_length += HTML_TEXT (*p)->text_len;
	}

	new_text = g_malloc (total_length + 1);
	sp = new_text;

	if (self->text_len > 0) {
		memcpy (sp, self->text, self->text_len);
		sp += self->text_len;
	}

	for (p = list; *p != NULL; p++) {
		if ((*p)->text_len > 0) {
			memcpy (sp, (*p)->text, (*p)->text_len);
			sp += (*p)->text_len;
		}
	}

	*sp = 0;

	g_free (self->text);
	self->text = new_text;
	self->text_len = total_length;

	HTML_TEXT_MASTER (self)->select_start = select_start;
	HTML_TEXT_MASTER (self)->select_length = select_length;
}

static guint
insert_text (HTMLText *text,
	     HTMLEngine *engine,
	     guint offset,
	     const gchar *p,
	     guint len)
{
	HTMLTextMaster *master;
	gboolean chars_inserted;

	master = HTML_TEXT_MASTER (text);
	chars_inserted = HTML_TEXT_CLASS (parent_class)->insert_text (text, engine, offset, p, len);

	if (chars_inserted > 0
	    && offset >= master->select_start
	    && offset < master->select_start + master->select_length)
		master->select_length += chars_inserted;

	return chars_inserted;
}

static guint
remove_text (HTMLText *text,
	     HTMLEngine *engine,
	     guint offset,
	     guint len)
{
	HTMLTextMaster *master;
	gboolean chars_removed;

	master = HTML_TEXT_MASTER (text);
	chars_removed = HTML_TEXT_CLASS (parent_class)->remove_text (text, engine, offset, len);

	if (chars_removed > 0
	    && offset >= master->select_start
	    && offset < master->select_start + master->select_length) {
		if (chars_removed > master->select_length)
			master->select_length = 0;
		else
			master->select_length -= chars_removed;
	}

	return chars_removed;
}


void
html_text_master_type_init (void)
{
	html_text_master_class_init (&html_text_master_class, HTML_TYPE_TEXTMASTER, sizeof (HTMLTextMaster));
}

void
html_text_master_class_init (HTMLTextMasterClass *klass,
			     HTMLType type,
			     guint object_size)
{
	HTMLObjectClass *object_class;
	HTMLTextClass *text_class;

	object_class = HTML_OBJECT_CLASS (klass);
	text_class = HTML_TEXT_CLASS (klass);

	html_text_class_init (text_class, type, object_size);

	/* HTMLObject methods.  */

	object_class->copy = copy;
	object_class->draw = draw;
	object_class->fit_line = fit_line;
	object_class->calc_size = calc_size;
	object_class->calc_min_width = calc_min_width;
	object_class->calc_preferred_width = calc_preferred_width;
	object_class->get_cursor = get_cursor;
	object_class->get_cursor_base = get_cursor_base;
	object_class->check_point = check_point;
	object_class->select_range = select_range;
	object_class->get_selection = get_selection;

	/* HTMLText methods.  */

	text_class->queue_draw = queue_draw;
	text_class->split = split;
	text_class->merge = merge;
	text_class->insert_text = insert_text;
	text_class->remove_text = remove_text;

	parent_class = & html_text_class;
}

void
html_text_master_init (HTMLTextMaster *master,
		       HTMLTextMasterClass *klass,
		       gchar *text,
		       GtkHTMLFontStyle font_style,
		       const GdkColor *color)
{
	HTMLText* html_text;
	HTMLObject *object;

	html_text = HTML_TEXT (master);
	object = HTML_OBJECT (master);

	html_text_init (html_text, HTML_TEXT_CLASS (klass), text, font_style, color);

	master->select_start = 0;
	master->select_length = 0;
}

HTMLObject *
html_text_master_new (gchar *text,
		      GtkHTMLFontStyle font_style,
		      const GdkColor *color)
{
	HTMLTextMaster *master;

	master = g_new (HTMLTextMaster, 1);
	html_text_master_init (master, &html_text_master_class, text, font_style, color);

	return HTML_OBJECT (master);
}


HTMLObject *
html_text_master_get_slave (HTMLTextMaster *master, guint offset)
{
	HTMLObject *p;

	for (p = HTML_OBJECT (master)->next; p != NULL; p = p->next) {
		HTMLTextSlave *slave;

		slave = HTML_TEXT_SLAVE (p);
		if (slave->posStart <= offset
		    && slave->posStart + slave->posLen > offset)
			break;
	}

	return p;
}
