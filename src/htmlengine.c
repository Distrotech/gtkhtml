/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.

    Copyright (C) 1997 Martin Jones (mjones@kde.org)
    Copyright (C) 1997 Torben Weis (weis@kde.org)
    Copyright (C) 1999 Anders Carlsson (andersca@gnu.org)
    Copyright (C) 1999, 2000, Helix Code, Inc.
    Copyright (C) 2001, 2002, 2003 Ximian Inc.

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

/* RULE: You should never create a new flow without inserting anything in it.
   If `e->flow' is not NULL, it must contain something.  */


#include <config.h>
#include "gtkhtml-compat.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkscrolledwindow.h>

#include "gtkhtml-embedded.h"
#include "gtkhtml-private.h"
#include "gtkhtml-properties.h"
#include "gtkhtml-stream.h"

#include "gtkhtmldebug.h"

#include "htmlengine.h"
#include "htmlengine-search.h"
#include "htmlengine-edit.h"
#include "htmlengine-edit-cursor.h"
#include "htmlengine-edit-movement.h"
#include "htmlengine-edit-cut-and-paste.h"
#include "htmlengine-edit-selection-updater.h"
#include "htmlengine-print.h"
#include "htmlcolor.h"
#include "htmlinterval.h"
#include "htmlobject.h"
#include "htmlsettings.h"
#include "htmltext.h"
#include "htmltokenizer.h"
#include "htmltype.h"
#include "htmlundo.h"
#include "htmldrawqueue.h"
#include "htmlgdkpainter.h"
#include "htmlplainpainter.h"
#include "htmlreplace.h"
#include "htmlentity.h"

#include "htmlanchor.h"
#include "htmlrule.h"
#include "htmlobject.h"
#include "htmlclueh.h"
#include "htmlcluev.h"
#include "htmlcluealigned.h"
#include "htmlimage.h"
#include "htmllist.h"
#include "htmltable.h"
#include "htmltablecell.h"
#include "htmltext.h"
#include "htmltextslave.h"
#include "htmlclueflow.h"
#include "htmlstack.h"
#include "htmlstringtokenizer.h"
#include "htmlselection.h"
#include "htmlform.h"
#include "htmlbutton.h"
#include "htmltextinput.h"
#include "htmlradio.h"
#include "htmlcheckbox.h"
#include "htmlhidden.h"
#include "htmlselect.h"
#include "htmltextarea.h"
#include "htmlimageinput.h"
#include "htmlstack.h"
#include "htmlsearch.h"
#include "htmlframeset.h"
#include "htmlframe.h"
#include "htmliframe.h"
#include "htmlshape.h"
#include "htmlmap.h"
#include "htmlmarshal.h"
#include "htmlstyle.h"

/* #define CHECK_CURSOR */
#ifdef CHECK_CURSOR
#include <libgnomeui/gnome-dialog-util.h>
#endif

static void      html_engine_class_init       (HTMLEngineClass     *klass);
static void      html_engine_init             (HTMLEngine          *engine);
static gboolean  html_engine_timer_event      (HTMLEngine          *e);
static gboolean  html_engine_update_event     (HTMLEngine          *e);
static void      html_engine_queue_redraw_all (HTMLEngine *e);
static char **   html_engine_stream_types     (GtkHTMLStream       *stream,
					       gpointer            data);
static void      html_engine_stream_write     (GtkHTMLStream       *stream,
					       const gchar         *buffer,
					       size_t               size,
					       gpointer             data);
static void      html_engine_stream_end       (GtkHTMLStream       *stream,
					       GtkHTMLStreamStatus  status,
					       gpointer             data);
static void      html_engine_set_object_data  (HTMLEngine          *e,
					       HTMLObject          *o);

static void      parse_one_token           (HTMLEngine *p,
					    HTMLObject *clue,
					    const gchar *str);
static void      element_parse_input       (HTMLEngine *e,
					    HTMLObject *clue,
					    const gchar *s);
static void      element_parse_iframe      (HTMLEngine *e,
					    HTMLObject *clue,
					    const gchar *s);
static void      update_embedded           (GtkWidget *widget,
					    gpointer );

static void      html_engine_map_table_clear (HTMLEngine *e);
static void      html_engine_id_table_clear (HTMLEngine *e);
static void      html_engine_add_map (HTMLEngine *e, const char *);
static void      clear_pending_expose (HTMLEngine *e);

static GtkLayoutClass *parent_class = NULL;

enum {
	SET_BASE_TARGET,
	SET_BASE,
	LOAD_DONE,
	TITLE_CHANGED,
	URL_REQUESTED,
	DRAW_PENDING,
	REDIRECT,
	SUBMIT,
	OBJECT_REQUESTED,
	LAST_SIGNAL
};
	
static guint signals [LAST_SIGNAL] = { 0 };

#define TIMER_INTERVAL 300
#define DT(x) ;
#define DF(x) ;
#define DE(x) x

#define ID_A "a"
#define ID_ADDRESS "address"
#define ID_B "b"
#define ID_BIG "big"
#define ID_BLOCKQUOTE "blockquote"
#define ID_BODY "body"
#define ID_CAPTION "caption"
#define ID_CENTER "center"
#define ID_CITE "cite"
#define ID_CODE "code"
#define ID_DIR "dir"
#define ID_DIV "div"
#define ID_DL "dl"
#define ID_DT "dt"
#define ID_DD "dd"
#define ID_LI "li"
#define ID_EM "em"
#define ID_FONT "font"
#define ID_FORM "form"
#define ID_MAP "map"
#define ID_HEADING "h"
#define ID_I "i"
#define ID_KBD "kbd"
#define ID_OL "ol"
#define ID_P "p"
#define ID_PRE "pre"
#define ID_SMALL "small"
#define ID_SPAN "span"
#define ID_STRONG "strong"
#define ID_U "u"
#define ID_UL "ul"
#define ID_TEXTAREA "textarea"
#define ID_TABLE "table"
#define ID_TD "td"
#define ID_TH "th"
#define ID_TR "tr"
#define ID_TT "tt"
#define ID_VAR "var"
#define ID_S "s"
#define ID_SUB "sub"
#define ID_SUP "sup"
#define ID_STRIKE "strike"
#define ID_HTML "html"
#define ID_DOCUMENT "Document"
#define ID_OPTION "option"
#define ID_SELECT "select"
#define ID_TEST "test"

#define ID_EQ(x,y) (x == g_quark_from_string (y))

typedef enum {
	DISPLAY_INLINE,
	DISPLAY_LIST_ITEM,
	DISPLAY_BLOCK,
	DISPLAY_MARKER,
	DISPLAY_NONE,
	DISPLAY_RUN_IN,
	DISPLAY_COMPACT,
	DISPLAY_INLINE_TABLE,
	DISPLAY_TABLE_ROW_GROUP,
	DISPLAY_TABLE_COLUMN,
	DISPLAY_TABLE_COLUMN_GROUP,
	DISPLAY_TABLE_HEADER_GROUP,
	DISPLAY_TABLE_FOOTER_GROUP,
	DISPLAY_TABLE_ROW,
	DISPLAY_TABLE_CELL,
        DISPLAY_TABLE_CAPTION,
	DISPLAY_TABLE
} HTMLDisplayType;


/*
 *  Font handling.
 */




/* Font styles */
typedef struct _HTMLElement HTMLElement;
typedef void (*BlockFunc)(HTMLEngine *e, HTMLObject *clue, HTMLElement *el);
struct _HTMLElement {
	GQuark          id;
	HTMLStyle      *style;
	HTMLDisplayType  display;

	GHashTable     *attributes;  /* the parsed attributes */

	gint level;
	gint miscData1;
	gint miscData2;
	BlockFunc exitFunc; 
};

static char *
parse_element_name (const char *str)
{
	const char *ep = str;

	ep = str;
	if (*ep == '/')
		ep++;

	while (*ep && *ep != ' ' && *ep != '>' && *ep != '/')
		ep++;
	
	if (ep - str == 0 || *str == '/' && ep - str == 1) {
		g_warning ("found token with no valid name");
		return NULL;
	}

	return g_strndup (str, ep - str);
}


static HTMLElement *
html_element_new (HTMLEngine *e, const char *str) {
	HTMLElement *element;
	char *name;

	name = parse_element_name (str);
	
	if (!name)
		return NULL;

	element = g_new0 (HTMLElement, 1);
	element->id = g_quark_from_string (name);
	g_free (name);

	element->attributes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	html_string_tokenizer_tokenize (e->st, str, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const gchar *token = html_string_tokenizer_next_token (e->st);
		gchar **attr;
		
		g_strsplit (token, "=", 2);
		
		if (attr[0]) {
			if (!g_hash_table_lookup (element->attributes, attr[0])) {
				DE (g_print ("attrs (%s, %s)", attr[0], attr[1]));
				g_hash_table_insert (element->attributes, attr[0], attr[1]);

				/* just free the array */
				g_free (attr);  
			} else {
				/* free the whole vector */
				g_strfreev (attr); 
			}
		}

	}
	
	return element;
}

#ifndef NO_ATTR_MACRO
#define html_element_get_attr(node, key, value) g_hash_table_lookup_extended (node->attributes, key, NULL, (gpointer *)value) 
#else 
gboolean
html_element_get_attr (HTMLElement *node, char *name, char **value)
{
	char *orig_key;

	g_return_if_fail (node->attributes != NULL);

	return g_hash_table_lookup_extended (node->attributes, name, &orig_key, value)
}
#endif

static void
html_element_parse_i18n (HTMLElement *node)
{
	char *value;
	/*
	  <!ENTITY % i18n
	  "lang        %LanguageCode; #IMPLIED  -- language code --
	  dir         (ltr|rtl)      #IMPLIED  -- direction for weak/neutral text --"
	  >
	*/
	
	if (html_element_get_attr (node, "dir", &value)) {
		printf ("dir = %s\n", value);
	}

	if (html_element_get_attr (node, "lang", &value)) {
		printf ("lang = %s\n", value);
	}
}

static void
html_element_parse_coreattrs (HTMLElement *node)
{
	char *value;
	
	/*
	  <!ENTITY % coreattrs
	  "id          ID             #IMPLIED  -- document-wide unique id --
	  class       CDATA          #IMPLIED  -- space-separated list of classes --
	  style       %StyleSheet;   #IMPLIED  -- associated style info --
	  title       %Text;         #IMPLIED  -- advisory title --"
	  >
	*/
	if (html_element_get_attr (node, "style", &value)) {
		node->style = html_style_add_attribute (node->style, value);
	}
}

static void
html_element_parse_events (HTMLElement *node)
{
	/* 
	   <!ENTITY % events
	   "onclick     %Script;       #IMPLIED  -- a pointer button was clicked --
	   ondblclick  %Script;       #IMPLIED  -- a pointer button was double clicked--
	   onmousedown %Script;       #IMPLIED  -- a pointer button was pressed down --
	   onmouseup   %Script;       #IMPLIED  -- a pointer button was released --
	   onmouseover %Script;       #IMPLIED  -- a pointer was moved onto --
	   onmousemove %Script;       #IMPLIED  -- a pointer was moved within --
	   onmouseout  %Script;       #IMPLIED  -- a pointer was moved away --
	   onkeypress  %Script;       #IMPLIED  -- a key was pressed and released --
	   onkeydown   %Script;       #IMPLIED  -- a key was pressed down --
	   onkeyup     %Script;       #IMPLIED  -- a key was released --"
	   >
	*/
}

static void
html_element_free (HTMLElement *element)
{
	g_hash_table_destroy (element->attributes);

	html_style_free (element->style);
	g_free (element);
}

static void
push_element (HTMLEngine *e, char *name, char *class, HTMLStyle *style)
{
	HTMLElement *element = g_new0 (HTMLElement, 1);

	element->id = g_quark_from_string (name);
	element->style = style;
	html_stack_push (e->span_stack, element);
}

static void
free_element (gpointer data)
{
	HTMLElement *span = data;

	html_style_free (span->style);
	g_free (span);
}

#define DI(x)

/* Color handling.  */
static gboolean
parse_color (const gchar *text,
	     GdkColor *color)
{
	gchar c [8];
	gint  len = strlen (text);

	if (gdk_color_parse (text, color))
		return TRUE;

	c [7] = 0;
	if (*text != '#') {
		c[0] = '#'; 
		strncpy (c + 1, text, 6);
		len++;
	} else {
		strncpy (c, text, 7);
	}
	
	if (len < 7)
		memset (c + len, '\0', 7-len);

	return gdk_color_parse (c, color);
}

static HTMLColor *
current_color (HTMLEngine *e) {
	HTMLElement *span;
	GList *item;

	for (item = e->span_stack->list; item; item = item->next) {
		span = item->data;
		
		if (ID_EQ (span->id,ID_TH) || ID_EQ (span->id, ID_TD))
			break;

		if (span->style && span->style->color)
			return span->style->color;
	}

	return html_colorset_get_color (e->settings->color_set, HTMLTextColor);
}

/* 
 * FIXME these are 100% wrong (bg color doesn't inheirit, but it is how the current table code works
 * and I don't want to regress yet
 */
static GdkColor *
current_row_bg_color (HTMLEngine *e)
{
	HTMLElement *span;
	GList *item;
		
	for (item = e->span_stack->list; item; item = item->next) {
		span = item->data;
		if (ID_EQ (span->id, ID_TR) && span->style)
			return span->style->bg_color;

		if (ID_EQ (span->id, ID_TABLE))
			break;
	}

	return NULL;
}

static char *
current_row_bg_image (HTMLEngine *e)
{
	HTMLElement *span;
	GList *item;
		
	for (item = e->span_stack->list; item; item = item->next) {
		span = item->data;
		if (ID_EQ (span->id, ID_TR) && span->style)
			return span->style->bg_image;

		if (ID_EQ (span->id, ID_TABLE))
			break;
	}

	return NULL;
}

static HTMLVAlignType
current_row_valign (HTMLEngine *e)
{
	HTMLElement *span;
	GList *item;
	HTMLVAlignType rv = HTML_VALIGN_MIDDLE;

	if (!html_stack_top (e->table_stack)) {
		DT (g_warning ("missing table");)
		return;
	}

	for (item = e->span_stack->list; item; item = item->next) {
		span = item->data;
		if (ID_EQ (span->id, ID_TR)) {
			DT(g_warning ("found row");)

			if (span->style)
				rv = span->style->text_valign;

			break;
		}

		if (ID_EQ (span->id,ID_TABLE)) {
			DT(g_warning ("found table before row");)
			//break;
		}
	}

	if (ID_EQ (span->id, ID_TR))
		DT(g_warning ("no row");)


	return rv;
}

static HTMLHAlignType
current_row_align (HTMLEngine *e)
{
	HTMLElement *span;
	GList *item;
	HTMLHAlignType rv = HTML_HALIGN_NONE;

	if (!html_stack_top (e->table_stack)) {
		DT (g_warning ("missing table");)
		return;
	}

	for (item = e->span_stack->list; item; item = item->next) {
		span = item->data;
		if (ID_EQ (span->id, ID_TR)) {
			DT(g_warning ("found row");)

			if (span->style)
				rv = span->style->text_align;

			break;
		}

		if (ID_EQ (span->id, ID_TABLE)) {
			DT(g_warning ("found table before row");)
			break;
		}
	}

	DT (
	if (ID_EQ (span->id, ID_TR))
		DT(g_warning ("no row");)
	);
	return rv;
}
/* end of these table hacks */
	
static HTMLFontFace *
current_font_face (HTMLEngine *e)
{


	HTMLElement *span;
	GList *item;
	
	for (item = e->span_stack->list; item; item = item->next) {
		span = item->data;
		if (span->style && span->style->face)
			return span->style->face;

	}

	return NULL;
}

static inline GtkHTMLFontStyle
current_font_style (HTMLEngine *e)
{
	HTMLElement *span;
	GList *item;
	GtkHTMLFontStyle style = GTK_HTML_FONT_STYLE_DEFAULT;
	
	for (item = g_list_last (e->span_stack->list); item; item = item->prev) {
		span = item->data;
		if (span->style)
			style = (style & ~span->style->mask) | (span->style->settings & span->style->mask);
	}
	return style;
}

static HTMLHAlignType
current_alignment (HTMLEngine *e)
{
	HTMLElement *span;
	GList *item;
	
	for (item = e->span_stack->list; item; item = item->next) {
		span = item->data;

		if (span->style && (span->style->text_align != HTML_HALIGN_NONE))
			return span->style->text_align;
		
		if (span->level > 2)
			break;
	}
	return HTML_HALIGN_NONE;
}
	
static GtkPolicyType 
parse_scroll (const char *token) 
{
	GtkPolicyType scroll;
	
	if (strncasecmp (token, "yes", 3) == 0) {
		scroll = GTK_POLICY_ALWAYS;
	} else if (strncasecmp (token, "no", 2) == 0) {
		scroll = GTK_POLICY_NEVER;
	} else /* auto */ {
		scroll = GTK_POLICY_AUTOMATIC;
	}
	return scroll;
}

static HTMLHAlignType
parse_halign (const char *token, HTMLHAlignType default_val)
{
	if (strcasecmp (token, "right") == 0)
		return HTML_HALIGN_RIGHT;
	else if (strcasecmp (token, "left") == 0)
		return HTML_HALIGN_LEFT;
	else if (strcasecmp (token, "center") == 0 || strcasecmp (token, "middle") == 0)
		return HTML_HALIGN_CENTER;
	else
		return default_val;
}
	

/* ClueFlow style handling.  */

static HTMLClueFlowStyle
current_clueflow_style (HTMLEngine *e)
{
	HTMLClueFlowStyle style;

	if (html_stack_is_empty (e->clueflow_style_stack))
		return HTML_CLUEFLOW_STYLE_NORMAL;

	style = (HTMLClueFlowStyle) GPOINTER_TO_INT (html_stack_top (e->clueflow_style_stack));
	return style;
}

static void
push_clueflow_style (HTMLEngine *e,
		     HTMLClueFlowStyle style)
{
	html_stack_push (e->clueflow_style_stack, GINT_TO_POINTER (style));
}

static void
pop_clueflow_style (HTMLEngine *e)
{
	html_stack_pop (e->clueflow_style_stack);
}



/* Utility functions.  */

static void new_flow (HTMLEngine *e, HTMLObject *clue, HTMLObject *first_object, HTMLClearType clear);
static void close_flow (HTMLEngine *e, HTMLObject *clue);
static void finish_flow (HTMLEngine *e, HTMLObject *clue);
static void pop_element (HTMLEngine *e, char *name);

static HTMLObject *
text_new (HTMLEngine *e, const gchar *text, GtkHTMLFontStyle style, HTMLColor *color)
{
	HTMLObject *o;

	o = html_text_new (text, style, color);
	html_engine_set_object_data (e, o);

	return o;
}

static HTMLObject *
flow_new (HTMLEngine *e, HTMLClueFlowStyle style, HTMLListType item_type, gint item_number, HTMLClearType clear)
{
	HTMLObject *o;
	GByteArray *levels;
	GList *l;

	levels = g_byte_array_new ();
	
	if (e->listStack && e->listStack->list) {
		l = e->listStack->list;
		while (l) {
			guint8 val = ((HTMLList *)l->data)->type;

			g_byte_array_prepend (levels, &val, 1);
			l = l->next;
		}
	}

	o = html_clueflow_new (style, levels, item_type, item_number, clear); 	
	html_engine_set_object_data (e, o);

	return o;
}

static HTMLObject *
create_empty_text (HTMLEngine *e)
{
	HTMLObject *o;

	o = text_new (e, "", current_font_style (e), current_color (e));
	html_text_set_font_face (HTML_TEXT (o), current_font_face (e));

	return o;
}

static void
insert_paragraph_break (HTMLEngine *e,
		     HTMLObject *clue)
{
	
	close_flow (e, clue);
	new_flow (e, clue, create_empty_text (e), HTML_CLEAR_NONE);
	close_flow (e, clue);
}

static void
add_line_break (HTMLEngine *e,
		HTMLObject *clue,
		HTMLClearType clear)
{
	if (!e->flow && !HTML_CLUE (clue)->head)
		new_flow (e, clue, create_empty_text (e), HTML_CLEAR_NONE);
	new_flow (e, clue, NULL, clear);
}

static void
block_end_anchor (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	g_free (e->url);
	e->url = NULL;

	g_free (e->target);
	e->target = NULL;

	e->eat_space = FALSE;
}

static void
finish_flow (HTMLEngine *e, HTMLObject *clue) {
	if (e->flow && HTML_CLUE (e->flow)->tail == NULL) {
		html_clue_remove (HTML_CLUE (clue), e->flow);
		html_object_destroy (e->flow);
		e->flow = NULL;
	}
	close_flow (e, clue);
}


static void
close_flow (HTMLEngine *e, HTMLObject *clue)
{
	HTMLObject *last;

	if (e->flow == NULL)
		return;

	last = HTML_CLUE (e->flow)->tail;
	if (last == NULL) {
		html_clue_append (HTML_CLUE (e->flow), create_empty_text (e));
	} else if (HTML_CLUE (e->flow)->tail != HTML_CLUE (e->flow)->head
		   && html_object_is_text (last)
		   && HTML_TEXT (last)->text_len == 1
		   && HTML_TEXT (last)->text [0] == ' ') {
		html_clue_remove (HTML_CLUE (e->flow), last);
		html_object_destroy (last);
	}

	e->flow = NULL;
}

static void
update_flow_align (HTMLEngine *e, HTMLObject *clue)
{
	if (e->flow != NULL) {
		if (HTML_CLUE (e->flow)->head != NULL)
			close_flow (e, clue);
		else
			HTML_CLUE (e->flow)->halign = current_alignment (e);
	}
}

static void
new_flow (HTMLEngine *e, HTMLObject *clue, HTMLObject *first_object, HTMLClearType clear)
{
	close_flow (e, clue);

	e->flow = flow_new (e, current_clueflow_style (e), HTML_LIST_TYPE_BLOCKQUOTE, 0, clear);

	HTML_CLUE (e->flow)->halign = current_alignment (e);

	if (first_object)
		html_clue_append (HTML_CLUE (e->flow), first_object);

	html_clue_append (HTML_CLUE (clue), e->flow);
}

static void
append_element (HTMLEngine *e,
		HTMLObject *clue,
		HTMLObject *obj)
{
	e->avoid_para = FALSE;

	if (e->flow == NULL)
		new_flow (e, clue, obj, HTML_CLEAR_NONE);
	else
		html_clue_append (HTML_CLUE (e->flow), obj);
}

static void
apply_attributes (HTMLText *text, HTMLEngine *e, GtkHTMLFontStyle style, HTMLColor *color, gint last_pos, gboolean link)
{
	PangoAttribute *attr;

	/* style */
	if (style & GTK_HTML_FONT_STYLE_BOLD) {
		attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
		attr->start_index = last_pos;
		attr->end_index = text->text_len;
		pango_attr_list_change (text->attr_list, attr);
	}

	if (style & GTK_HTML_FONT_STYLE_ITALIC) {
		attr = pango_attr_style_new (PANGO_STYLE_ITALIC);
		attr->start_index = last_pos;
		attr->end_index = text->text_len;
		pango_attr_list_change (text->attr_list, attr);
	}

	if (style & GTK_HTML_FONT_STYLE_UNDERLINE || link) {
		attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
		attr->start_index = last_pos;
		attr->end_index = text->text_len;
		pango_attr_list_change (text->attr_list, attr);
	}

	if (style & GTK_HTML_FONT_STYLE_STRIKEOUT) {
		attr = pango_attr_strikethrough_new (TRUE);
		attr->start_index = last_pos;
		attr->end_index = text->text_len;
		pango_attr_list_change (text->attr_list, attr);
	}

	/* color */
	if (link || color != html_colorset_get_color (e->settings->color_set, HTMLTextColor)) {
		attr = pango_attr_foreground_new (color->color.red, color->color.green, color->color.blue);
		attr->start_index = last_pos;
		attr->end_index = text->text_len;
		pango_attr_list_change (text->attr_list, attr);
	}

	/* size */
	/* face */
}

static void
insert_text (HTMLEngine *e,
	     HTMLObject *clue,
	     const gchar *text)
{
	GtkHTMLFontStyle font_style;
	HTMLObject *prev;
	HTMLColor *color;
	gchar *face;
	gboolean create_link;
	gint last_pos = 0;

	if (text [0] == ' ' && text [1] == 0) {
		if (e->eat_space)
			return;
		else
			e->eat_space = TRUE;
	} else
		e->eat_space = FALSE;
	

	if (e->url != NULL || e->target != NULL)
		create_link = TRUE;
	else
		create_link = FALSE;

	font_style = current_font_style (e);
	color = current_color (e);
	face = current_font_face (e);

	if ((create_link || e->flow == NULL || HTML_CLUE (e->flow)->head == NULL) && !e->inPre) {
		while (*text == ' ')
			text++;
		if (*text == 0)
			return;
	}

	if (e->flow == NULL)
		prev = NULL;
	else
		prev = HTML_CLUE (e->flow)->tail;

	if (!prev || !HTML_IS_TEXT (prev)) {
		prev = text_new (e, text, font_style, color);
		append_element (e, clue, prev);
	} else {
		last_pos = HTML_TEXT (prev)->text_len;
		html_text_append (HTML_TEXT (prev), text, -1);
	}

	if (prev && HTML_IS_TEXT (prev)) {
		apply_attributes (HTML_TEXT (prev), e, font_style, color, last_pos, create_link);
		if (create_link)
			html_text_add_link (HTML_TEXT (prev), e->url, e->target, last_pos, HTML_TEXT (prev)->text_len);
	}
}


/* Block stack.  */

static void
push_block_element (HTMLEngine *e,
		    char *name,
		    HTMLStyle *style,
		    gint level,
		    BlockFunc exitFunc,
		    gint miscData1,
		    gint miscData2)
{
	HTMLElement *element = g_new0 (HTMLElement, 1);
	
	pop_element (e, ID_P);

	element->id = g_quark_from_string (name);
	element->style = style;
	element->display = DISPLAY_BLOCK;
	element->level = level;
	element->exitFunc = exitFunc;
	element->miscData1 = miscData1;
	element->miscData2 = miscData2;

	html_stack_push (e->span_stack, element);
}

static void
push_block (HTMLEngine *e,
	    char *name,
	    gint level,
	    BlockFunc exitFunc,
	    gint miscData1,
	    gint miscData2)
{
	HTMLElement *elem;
	
	push_block_element (e, name, NULL, level, exitFunc, miscData1, miscData2);
}

static GList *
remove_element (HTMLEngine *e, GList *item)
{
	HTMLElement *elem = item->data;
	GList *next = item->next;

	if (elem->exitFunc)
		(*(elem->exitFunc))(e, e->clue, elem);

	e->span_stack->list = g_list_remove_link (e->span_stack->list, item);

	g_list_free (item);
	free_element (elem);
	
	return next;
}

static void
pop_cell (HTMLEngine *e, HTMLElement *elem)
{
	GList *l = e->span_stack->list;

	while (l) {
		l = remove_element (e, l);
	}
}

static void
pop_block (HTMLEngine *e, HTMLElement *elem)
{
	GList *l = e->span_stack->list;

	g_return_if_fail (elem->display == DISPLAY_BLOCK);
	
	while (l) {
		HTMLElement *cur = l->data;

		if (cur == elem) {
			l = remove_element (e, l);
			return;
		} else if (cur->display == DISPLAY_BLOCK || elem->level > 2) {
			l = remove_element (e, l);
		} else {
			l = l->next;
		}
	}
}

static void
pop_inline (HTMLEngine *e, HTMLElement *elem)
{
	GList *l = e->span_stack->list;

	while (l) {
		HTMLElement *cur = l->data;

		if (cur->level > 2)
			break;

		if (cur == elem) {
			l = remove_element (e, l);
			return;
		} else {
			l = l->next;
		}
	}
}
     
static void
pop_element (HTMLEngine *e, char *name)
{
	HTMLElement *elem = NULL;
	GList *l;
	gint maxLevel;
	GQuark id = g_quark_from_string (name);

	l = e->span_stack->list;
	maxLevel = 0;

	while (l) {
		elem = l->data;

		if (elem->id == id)
			break;

		maxLevel = MAX (maxLevel, elem->level);
		l = l->next;
	}

	if (l == NULL)
		return;

	switch (elem->display) {
	case DISPLAY_BLOCK:
		if (maxLevel > elem->level)
			return;
	
		pop_block (e, elem);
		break;
	case DISPLAY_INLINE:
		pop_inline (e, elem);
		break;
	case DISPLAY_TABLE_CELL:
		pop_cell (e, elem);
	default:
		pop_inline (e, elem);
		break;
	}
}

/* The following are callbacks that are called at the end of a block.  */
static void
block_end_div (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	close_flow (e, clue);
}

static void
block_end_p (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	if (e->avoid_para) {
		finish_flow (e, clue);
	} else {
		new_flow (e, clue, NULL, HTML_CLEAR_NONE);
		new_flow (e, clue, NULL, HTML_CLEAR_NONE);
		e->avoid_para = TRUE;
	}
}

static void
block_end_map (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	e->map = NULL;
}

static void
block_end_option (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	if ( e->inOption )
		html_select_set_text (e->formSelect, e->formText->str);
	
	e->inOption = FALSE;
}

static void
block_end_select (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	if ( e->inOption )
		html_select_set_text (e->formSelect, e->formText->str);
	
	e->inOption = FALSE;
	e->formSelect = NULL;
	e->eat_space = FALSE;
}

static void
block_end_textarea (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	if ( e->inTextArea )
		html_textarea_set_text (e->formTextArea, e->formText->str);
	
	e->inTextArea = FALSE;
	e->formTextArea = NULL;
	e->eat_space = FALSE;
}
static void
push_clue (HTMLEngine *e, HTMLObject *clue)
{
	//html_stack_push (e->body_stack, e->span_stack);
	html_stack_push (e->body_stack, e->clueflow_style_stack);
	html_stack_push (e->body_stack, e->listStack);
	html_stack_push (e->body_stack, e->clue);
	html_stack_push (e->body_stack, e->flow);

	//e->span_stack = html_stack_new (free_element);
	e->clueflow_style_stack = html_stack_new (NULL);
	e->listStack = html_stack_new ((HTMLStackFreeFunc)html_list_destroy);
	e->clue = clue;
	e->flow = NULL;

	html_stack_push (e->body_stack, GINT_TO_POINTER (e->avoid_para));
	
	e->avoid_para = TRUE;
}

static void
pop_clue (HTMLEngine *e)
{
	
#if 0
	if (HTML_CLUE (e->clue)->head == NULL)
		insert_paragraph_break (e, HTML_OBJECT (e->clue));
	close_flow (e, HTML_OBJECT (e->clue));
#else

	finish_flow (e, HTML_OBJECT (e->clue));
#endif

	e->avoid_para = GPOINTER_TO_INT (html_stack_pop (e->body_stack));
	
	html_stack_destroy (e->clueflow_style_stack);
	//html_stack_destroy (e->span_stack);

	e->flow = html_stack_pop (e->body_stack);
	e->clue = html_stack_pop (e->body_stack);
	e->listStack = html_stack_pop (e->body_stack);
	e->clueflow_style_stack = html_stack_pop (e->body_stack);
	//e->span_stack = html_stack_pop (e->body_stack);

}

static void
block_end_cell (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	pop_clue (e);
}


/* docment section parsers */
static void
block_end_title (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	/*
	 * only emit the title changed signal if we have a 
	 * valid title 
	 */
	if (e->inTitle && e->title) 
		g_signal_emit (e, signals [TITLE_CHANGED], 0);
	e->inTitle = FALSE;
}

static void
element_parse_title (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	e->inTitle = TRUE;
	e->title = g_string_new ("");
	
	push_block (e, "title", 0, block_end_title, 0, 0);
}

static gchar *
new_parse_body (HTMLEngine *e, const gchar *end[])
{
	HTMLObject *clue = NULL;
	gchar *str;
	gchar *rv = NULL;

	e->eat_space = FALSE;

	while (html_tokenizer_has_more_tokens (e->ht) && e->parsing) {
		str = html_tokenizer_next_token (e->ht);

		/* The token parser has pushed a body we want to use it. */
		clue = e->clue;
		//printf ("%p <-- clue\n", clue);

		if (*str == '\0')
			continue;

		if ( *str == ' ' && *(str+1) == '\0' ) {
			/* if in* is set this text belongs in a form element */
			if (e->inTextArea || e->inOption)
				e->formText = g_string_append (e->formText, " ");
			else if (e->inTitle)
				g_string_append (e->title, " ");
			else
				insert_text (e, clue, str);
		} else if (*str != TAG_ESCAPE) {
			if (e->inOption || e->inTextArea)
				g_string_append (e->formText, str);
			else if (e->inTitle) {
				g_string_append (e->title, str);
			} else {
				insert_text (e, clue, str);
			}
		} else {
			gint i  = 0;
			str++;

			while (end [i] != 0) {
				if (strncasecmp (str, end[i], strlen(end[i])) == 0) {
					rv = str;
				}
				i++;
			}
			
			/* The tag used for line break when we are in <pre>...</pre> */
			if (*str == '\n')
				add_line_break (e, clue, HTML_CLEAR_NONE);
			else
				parse_one_token (e, clue, str);
		}
	}
	
	if (!html_tokenizer_has_more_tokens (e->ht) && !e->writing)
		html_engine_stop_parser (e);

	return rv;
}

static gchar *
parse_body (HTMLEngine *e, HTMLObject *clue, const gchar *end[], gboolean toplevel, gboolean begin)
{
	gchar *str;
	gchar *rv = NULL;
	gboolean final = FALSE;

	if (begin && !toplevel) {
		push_clue (e, clue);
	}

	e->eat_space = FALSE;
	while (html_tokenizer_has_more_tokens (e->ht) && e->parsing) {
		str = html_tokenizer_next_token (e->ht);

		if (*str == '\0')
			continue;

		if ( *str == ' ' && *(str+1) == '\0' ) {
			/* if in* is set this text belongs in a form element */
			if (e->inTextArea || e->inOption)
				e->formText = g_string_append (e->formText, " ");
			else if (e->inTitle)
				g_string_append (e->title, " ");
			else
				insert_text (e, clue, str);
		} else if (*str != TAG_ESCAPE) {
			if (e->inOption || e->inTextArea)
				g_string_append (e->formText, str);
			else if (e->inTitle) {
				g_string_append (e->title, str);
		}
			else {
				insert_text (e, clue, str);
			}
		} else {
			gint i  = 0;
			str++;

			while (end [i] != 0) {
				if (strncasecmp (str, end[i], strlen(end[i])) == 0) {
					rv = str;
					final = TRUE;
					goto end_body;
				}
				i++;
			}
			
			/* The tag used for line break when we are in <pre>...</pre> */
			if (*str == '\n')
				add_line_break (e, clue, HTML_CLEAR_NONE);
			else
				parse_one_token (e, clue, str);
		}
	}

	if (!html_tokenizer_has_more_tokens (e->ht) && toplevel && !e->writing)
		html_engine_stop_parser (e);

 end_body:
	if (final) {
		if (e->flow && HTML_CLUE (e->flow)->tail == NULL) {
			html_clue_remove (HTML_CLUE (clue), e->flow);
			html_object_destroy (e->flow);
			e->flow = NULL;
		}
		pop_clue (e);
	}

	return rv;
}

static gchar *
discard_body (HTMLEngine *p, const gchar *end[])
{
	gchar *str = NULL;

	while (html_tokenizer_has_more_tokens (p->ht) && p->parsing) {
		str = html_tokenizer_next_token (p->ht);

		if (*str == '\0')
			continue;

		if ((*str == ' ' && *(str+1) == '\0')
		    || (*str != TAG_ESCAPE)) {
			/* do nothing */
		}
		else {
			gint i  = 0;
			str++;
			
			while (end [i] != 0) {
				if (strncasecmp (str, end[i], strlen(end[i])) == 0) {
					return str;
				}
				i++;
			}
		}
	}

	return 0;
}

static gboolean
is_leading_space (guchar *str)
{
	while (*str != '\0') {
		if (!(isspace (*str) || IS_UTF8_NBSP (str))) 
			return FALSE;
		
		str = g_utf8_next_char (str);
	}
	return TRUE;
}

static void
element_parse_param (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	GtkHTMLEmbedded *eb;
	char *name = NULL, *value = NULL;
	
	
	if (html_stack_is_empty (e->embeddedStack))
		return;
	
	eb = html_stack_top (e->embeddedStack);
	
	html_string_tokenizer_tokenize (e->st, str + 6, " >");
	while ( html_string_tokenizer_has_more_tokens (e->st) ) {
		const char *token = html_string_tokenizer_next_token (e->st);
		if ( strncasecmp( token, "name=", 5 ) == 0 ) {
			name = g_strdup (token+5);
		} else if ( strncasecmp( token, "value=", 6 ) == 0 ) {
			value = g_strdup (token+6);
		}
	}
	
	if (name!=NULL)
		gtk_html_embedded_set_parameter(eb, name, value);
	
	g_free(name);
	g_free(value);
}

static gchar * 
parse_object_params(HTMLEngine *p, HTMLObject *clue) 
{
	gchar *str;

	/* we peek at tokens looking for <param> elements and
	 * as soon as we find something that is not whitespace or a param
	 * element we bail and the caller deal with the rest
	 */
	while (html_tokenizer_has_more_tokens (p->ht) && p->parsing) {
		str = html_tokenizer_peek_token (p->ht);
		
		if (*str == '\0' || 
		    *str == '\n' ||
		    is_leading_space (str)) {
				str = html_tokenizer_next_token (p->ht);
				/* printf ("\"%s\": was the string\n", str); */
				continue;
		} else if (*str == TAG_ESCAPE) {
			str++;
			if (strncasecmp ("<param", str, 6) == 0) {
				/* go ahead and remove the token */
				html_tokenizer_next_token (p->ht);

				parse_one_token (p, clue, str);
				continue;
			}
		}
		return str;
	}
	
	return NULL;	
}

static void
parse_object (HTMLEngine *e, HTMLObject *clue, const gchar *attr)
{
	char *classid=NULL;
	char *name=NULL;
	char *type = NULL;
	char *str = NULL;
	char *data = NULL;
	int width=-1,height=-1;
	gint max_width = clue->max_width;
	static const gchar *end[] = { "</object", 0};
	GtkHTMLEmbedded *eb;
	HTMLEmbedded *el;
	gboolean object_found;
	
	/* this might have to do something different for form object
	   elements - check the spec MPZ */
	
	html_string_tokenizer_tokenize( e->st, attr + 6, " >" );
	while (html_string_tokenizer_has_more_tokens (e->st) ) {
		const char* token;
		
		token = html_string_tokenizer_next_token (e->st);
		if (strncasecmp (token, "classid=", 8) == 0) {
			classid = g_strdup (token + 8);
		} else if (strncasecmp (token, "name=", 5) == 0 ) {
			name = g_strdup (token + 5);
		} else if ( strncasecmp (token, "width=", 6) == 0) {
			width = atoi (token + 6);
		} else if (strncasecmp (token, "height=", 7) == 0) {
			height = atoi (token + 7);
		} else if (strncasecmp (token, "type=", 5) == 0) {
			type = g_strdup (token + 5);
		} else if (strncasecmp (token, "data=", 5) == 0) {
			data = g_strdup (token + 5);
		}
	}
	
	eb = (GtkHTMLEmbedded *) gtk_html_embedded_new (classid, name, type, data, width, height);
	html_stack_push (e->embeddedStack, eb);
	
	el = html_embedded_new_widget (GTK_WIDGET (e->widget), eb, e);
	
	/* evaluate params */
	parse_object_params (e, clue);

	/* create the object */
        object_found = FALSE;
	g_signal_emit (e, signals [OBJECT_REQUESTED], 0, eb, &object_found);
	
	/* show alt text on TRUE */ 
	if (object_found) {
		append_element (e, clue, HTML_OBJECT(el));
		/* automatically add this to a form if it is part of one */
		if (e->form)
			html_form_add_element (e->form, HTML_EMBEDDED (el));

		/* throw away the contents we can deal with the object */
		str = discard_body (e, end);
	} else {
		/* parse the body of the tag to display the alternative */
		str = parse_body (e, clue, end, FALSE, TRUE);
		close_flow (e, clue);
		html_object_destroy (HTML_OBJECT (el));
	}
	
	if ((!str || (strncasecmp (str, "</object", 8) == 0)) && 
	    (!html_stack_is_empty (e->embeddedStack))) {
		html_stack_pop (e->embeddedStack);
	}
	
	g_free (type);
	g_free (data);
	g_free (classid);
	g_free (name);
}

/* Frame parsers */
static void
element_parse_noframe (HTMLEngine *e, HTMLObject *clue, const gchar *str )
{
	static const char *end[] = {"</noframe", NULL};
	
	if (e->allow_frameset)
		discard_body (e, end);
}

static void
block_end_frameset (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	if (!html_stack_is_empty (e->frame_stack))
		html_stack_pop (e->frame_stack);
}

static void
element_parse_frameset (HTMLEngine *e, HTMLObject *clue, const gchar *attr)
{
	HTMLObject *set;
	char *rows = NULL;
	char *cols = NULL;

	if (e->allow_frameset)
		return

	html_string_tokenizer_tokenize (e->st, attr + 8, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const gchar *token = html_string_tokenizer_next_token (e->st);
	
		if (strncasecmp (token, "rows=", 5) == 0) {
			rows = g_strdup (token + 5);
		} else if (strncasecmp (token, "cols=", 5) == 0) {
			cols = g_strdup (token + 5);
		} else if (strncasecmp (token, "onload=", 7) == 0) {
			/* all frames in set loaded */
			/* unimplemented */
		} else if (strncasecmp (token, "onunload=", 9) == 0) {
			/* all frames unloaded */
			/* unimplemented */
		}		
		
	}
	
	/* clear the borders */
	e->bottomBorder = 0;
	e->topBorder = 0;
	e->leftBorder = 0;
	e->rightBorder = 0;
	
	set = html_frameset_new (e->widget, rows, cols);

	if (html_stack_is_empty (e->frame_stack)) {
		append_element (e, clue, set);
	} else {
		html_frameset_append (html_stack_top (e->frame_stack), set);
	}
		
	html_stack_push (e->frame_stack, set);
	       
	g_free (rows);
	g_free (cols);

	push_block (e, "frameset", 0, block_end_frameset, 0, 0);
}

static void
element_parse_iframe (HTMLEngine *e, HTMLObject *clue, const char *str) 
{	char *src = NULL;
	char *align = NULL;
	HTMLObject *iframe;
	static const gchar *end[] = { "</iframe", 0};
	gint width           = -1;
	gint height          = -1;
	gint border          = TRUE;
	GtkPolicyType scroll = GTK_POLICY_AUTOMATIC;
	gint margin_width    = -1;
	gint margin_height   = -1;

	html_string_tokenizer_tokenize (e->st, str + 6, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const gchar *token = html_string_tokenizer_next_token (e->st);

		if (strncasecmp (token, "src=", 4) == 0) {
			src = g_strdup (token + 4);
		} else if (strncasecmp (token, "width=", 6) == 0) {
			width = atoi (token + 6);
		} else if (strncasecmp (token, "height=", 7) == 0) {
			height = atoi (token + 7);
		} else if (strncasecmp (token, "align=", 6) == 0) {
			align = g_strdup (token + 6);
		} else if (strncasecmp (token, "longdesc=", 9) == 0) {
			/* TODO: Ignored */
		} else if (strncasecmp (token, "name=", 5) == 0) {
			/* TODO: Ignored */
		} else if (strncasecmp (token, "scrolling=", 10) == 0) {
			scroll = parse_scroll (token + 10);
		} else if (strncasecmp (token, "marginwidth=", 12) == 0) {
			margin_width = atoi (token + 12);
		} else if (strncasecmp (token, "marginheight=", 13) == 0) {
			margin_height = atoi (token + 13);
		} else if (strncasecmp (token, "frameborder=", 12) == 0) {
			border = atoi (token + 12);
		}

	}	
		
	/* FIXME fixup missing url */
	if (src) {
		iframe = html_iframe_new (GTK_WIDGET (e->widget),
					  src, width, height, border);
		if (margin_height >= 0)
			html_iframe_set_margin_height (HTML_IFRAME (iframe), margin_height);
		if (margin_width >= 0)
			html_iframe_set_margin_width (HTML_IFRAME (iframe), margin_width);
		if (scroll != GTK_POLICY_AUTOMATIC)
			html_iframe_set_scrolling (HTML_IFRAME (iframe), scroll);

		g_free (src);

		append_element (e, clue, iframe);
		discard_body (e, end);
	} else {
		parse_body (e, clue, end, FALSE, TRUE);
		close_flow (e, clue);
	}
	g_free (align);
}


static void
element_parse_area (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	HTMLShape *shape;
	char *type = NULL;
	char *href = NULL;
	char *coords = NULL;
	char *target = NULL;
	
	if (e->map == NULL)
		return;
	
	html_string_tokenizer_tokenize (e->st, str + 5, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {   
		gchar *token = html_string_tokenizer_next_token (e->st);
		
		if (strncasecmp (token, "shape=", 6) == 0) {
			type = g_strdup (token + 6);
		} else if (strncasecmp (token, "href=", 5) == 0) {
			href = g_strdup (token +5);
		} else if ( strncasecmp (token, "target=", 7) == 0) {
			target = g_strdup (token + 7);
		} else if ( strncasecmp (token, "coords=", 7) == 0) {
			coords = g_strdup (token + 7);
		}
	}
	
	if (type || coords) {
		
		shape = html_shape_new (type, coords, href, target);
		if (shape != NULL) {
			html_map_add_shape (e->map, shape);
		}
	}
	
	g_free (type);
	g_free (href);
	g_free (coords);
	g_free (target);
}

static void
element_parse_a (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	HTMLShape *shape;
	gchar *url = NULL;
	gchar *id = NULL;
	HTMLStyle *style = NULL;
	char *style_attr = NULL;
	char *type = NULL;
	char *coords = NULL;
	char *target = NULL;
	
	const gchar *p;
	
	pop_element (e, ID_A);
	
	html_string_tokenizer_tokenize( e->st, str + 2, " >" );
	
	while ((p = html_string_tokenizer_next_token (e->st)) != 0) {
		if (strncasecmp (p, "href=", 5) == 0) {
			url = g_strdup (p + 5);
			/* FIXME visited? */
		} else if (strncasecmp (p, "id=", 3) == 0) {
			/*
			 * FIXME this doesn't handle the 
			 * case where id and name are both set
			 * properly but it will do for now
			 */
			if (id == NULL)
				id = g_strdup (p + 3);
		} else if (strncasecmp (p, "name=", 5) == 0) {
			if (id == NULL)
				id = g_strdup (p + 5);
		} else if (strncasecmp (p, "shape=", 6) == 0) {
			type = g_strdup (p + 6);
		} else if ( strncasecmp (p, "coords=", 7) == 0) {
			coords = g_strdup (p + 7);
		} else if (strncasecmp (p, "style=", 6) == 0) {
			style_attr = g_strdup (p + 6);
		} else if (strncasecmp (p, "target=", 7) == 0) {
			target = g_strdup (p + 7);
		}
	}
	
	if (type || coords) {
		shape = html_shape_new (type, coords, url, target);
		if (shape != NULL) {
			html_map_add_shape (e->map, shape);
		}
	}
	
	if (id != NULL) {
		if (e->flow == 0)
			html_clue_append (HTML_CLUE (clue),
					  html_anchor_new (id));
		else
			html_clue_append (HTML_CLUE (e->flow),
					  html_anchor_new (id));
		g_free (id);
	}
	
	if (url != NULL) {
		g_free (e->url);
		e->url = url;
	}
	if (e->url || e->target) {
		style = html_style_add_color (style, html_colorset_get_color (e->settings->color_set, HTMLLinkColor));
		style = html_style_set_decoration (style, GTK_HTML_FONT_STYLE_UNDERLINE);
	}
	if (style_attr) {
		style = html_style_add_attribute (style, style_attr);
		g_free (style_attr);
	}
	push_element (e, ID_A, NULL, style);
	((HTMLElement *)html_stack_top (e->span_stack))->exitFunc = block_end_anchor;
}


/* block parsing */
static void
block_end_clueflow_style (HTMLEngine *e,
			  HTMLObject *clue,
			  HTMLElement *elem)
{
	finish_flow (e, clue);
	pop_clueflow_style (e);
}

static void
element_parse_address (HTMLEngine *e, HTMLObject *clue, const char *str)
{
	HTMLStyle *style = NULL;
	
	style = html_style_set_decoration (style, GTK_HTML_FONT_STYLE_ITALIC);
	push_block_element (e, ID_ADDRESS, style, 1, block_end_clueflow_style, 0, 0);
	
	push_clueflow_style (e, HTML_CLUEFLOW_STYLE_ADDRESS);
	close_flow (e, clue);

	e->avoid_para = TRUE;
}

static void
block_end_pre (HTMLEngine *e, HTMLObject *_clue, HTMLElement *elem)
{
	block_end_clueflow_style (e, _clue, elem);

	e->inPre--;
}

static void
element_parse_pre (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	push_block (e, ID_PRE, 1, block_end_pre, 0, 0);

	push_clueflow_style (e, HTML_CLUEFLOW_STYLE_PRE);
	close_flow (e, clue);

	e->inPre++;
	e->avoid_para = TRUE;
}

static void
element_parse_center (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	HTMLStyle *style = NULL;
	
	style = html_style_add_text_align (style, HTML_HALIGN_CENTER);
	push_block_element (e, ID_CENTER, style, 1, block_end_div, 0, FALSE);
	
	update_flow_align (e, clue);
}

static void
block_end_blockquote (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	finish_flow (e, clue);

	html_list_destroy (html_stack_pop (e->listStack));

	e->avoid_para = TRUE;
}

static void
element_parse_blockquote (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	gboolean type = HTML_LIST_TYPE_BLOCKQUOTE;
	
	html_string_tokenizer_tokenize (e->st, str + 11, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const char *token = html_string_tokenizer_next_token (e->st);
		if (strncasecmp (token, "type=", 5) == 0) {
			if (strncasecmp (token + 5, "cite", 5) == 0) {
				type = HTML_LIST_TYPE_BLOCKQUOTE_CITE;
			}
		}	 
	}
	
	html_stack_push (e->listStack, html_list_new (type));
	push_block (e, ID_BLOCKQUOTE, 1, block_end_blockquote, FALSE, FALSE);
	e->avoid_para = TRUE;
	close_flow (e, clue);
}

static void
element_parse_div (HTMLEngine *e, HTMLObject *clue, const char *str)
{
	HTMLStyle *style = NULL;
	
	html_string_tokenizer_tokenize (e->st, str + 4, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const char* token = html_string_tokenizer_next_token (e->st);
		if (strncasecmp (token, "align=", 6 ) == 0) {
			style = html_style_add_text_align (style, parse_halign (token + 6, HTML_HALIGN_NONE));
		}
	}
	
	push_block_element (e, ID_DIV, style, 1, block_end_div, 0, 0);
	update_flow_align (e, clue);
}

static void
element_parse_p (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	if (*str != '/') {
		HTMLStyle *style = NULL;
		gchar *class = NULL;
		gchar *token;

		html_string_tokenizer_tokenize (e->st, (gchar *)(str + 2), " >");
		while (html_string_tokenizer_has_more_tokens (e->st)) {
			token = html_string_tokenizer_next_token (e->st);
			if (strncasecmp (token, "align=", 6) == 0) {
				style = html_style_add_text_align (style, parse_halign (token + 6, HTML_HALIGN_NONE));
			} else if (strncasecmp (token, "class=", 6) == 0) {
				class = g_strdup (token + 6);
			}
		}
		
		push_block_element (e, ID_P, style, 1, block_end_p, 0, 0);
		if (!e->avoid_para) {	
			new_flow (e, clue, NULL, HTML_CLEAR_NONE);
			new_flow (e, clue, NULL, HTML_CLEAR_NONE);
		} else {
			if (e->flow)
				HTML_CLUE (e->flow)->halign = current_alignment (e);
			else 
				new_flow (e, clue, NULL, HTML_CLEAR_NONE);

		}
		g_free (class);

		e->avoid_para = TRUE;
	} else {
		pop_element (e, ID_P);
		if (!e->avoid_para) {
			new_flow (e, clue, NULL, HTML_CLEAR_NONE);
			new_flow (e, clue, NULL, HTML_CLEAR_NONE);
			e->avoid_para = TRUE;
		}		
	}
}

static void
element_parse_br (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	HTMLClearType clear;
	
	clear = HTML_CLEAR_NONE;

	/* 
	 * FIXME this parses the clear attributes on close tags
	 * as well I'm not sure if we should do that or not, someone
	 * should check the mozilla behavior
	 */
	html_string_tokenizer_tokenize (e->st, str + 3, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		gchar *token = html_string_tokenizer_next_token (e->st);
		
		if (strncasecmp (token, "clear=", 6) == 0) {
			gtk_html_debug_log (e->widget, "%s\n", token);
			if (strncasecmp (token + 6, "left", 4) == 0)
				clear = HTML_CLEAR_LEFT;
			else if (strncasecmp (token + 6, "right", 5) == 0)
				clear = HTML_CLEAR_RIGHT;
			else if (strncasecmp (token + 6, "all", 3) == 0)
				clear = HTML_CLEAR_ALL;
		}
	}
	
	add_line_break (e, clue, clear);
}


static void
element_parse_body (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	GdkColor color;

	html_string_tokenizer_tokenize (e->st, str + 5, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		gchar *token;
		
		token = html_string_tokenizer_next_token (e->st);
		gtk_html_debug_log (e->widget, "token is: %s\n", token);
		
		if (strncasecmp (token, "bgcolor=", 8) == 0) {
			gtk_html_debug_log (e->widget, "setting color\n");
			if (parse_color (token + 8, &color)) {
				gtk_html_debug_log (e->widget, "bgcolor is set\n");
				html_colorset_set_color (e->settings->color_set, &color, HTMLBgColor);
			} else {
				gtk_html_debug_log (e->widget, "Color `%s' could not be parsed\n", token);
			}
		} else if (strncasecmp (token, "background=", 11) == 0
			   && token [12]
			   && ! e->defaultSettings->forceDefault) {
			gchar *bgurl;
			
			bgurl = g_strdup (token + 11);
			if (e->bgPixmapPtr != NULL)
				html_image_factory_unregister(e->image_factory, e->bgPixmapPtr, NULL);
			e->bgPixmapPtr = html_image_factory_register(e->image_factory, NULL, bgurl, FALSE);
			g_free (bgurl);
		} else if ( strncasecmp( token, "text=", 5 ) == 0
			    && !e->defaultSettings->forceDefault ) {
			if (parse_color (token + 5, &color)) {
				html_colorset_set_color (e->settings->color_set, &color, HTMLTextColor);
				push_element (e, ID_BODY, NULL, 
					      html_style_add_color (NULL, html_colorset_get_color (e->settings->color_set, HTMLTextColor)));
			}
		} else if ( strncasecmp( token, "link=", 5 ) == 0
			    && !e->defaultSettings->forceDefault ) {
			parse_color (token + 5, &color);
			html_colorset_set_color (e->settings->color_set, &color, HTMLLinkColor);
		} else if ( strncasecmp( token, "vlink=", 6 ) == 0
			    && !e->defaultSettings->forceDefault ) {
			parse_color (token + 6, &color);
			html_colorset_set_color (e->settings->color_set, &color, HTMLVLinkColor);
		} else if ( strncasecmp( token, "alink=", 6 ) == 0
			    && !e->defaultSettings->forceDefault ) {
			parse_color (token + 6, &color);
			html_colorset_set_color (e->settings->color_set, &color, HTMLALinkColor);
		} else if ( strncasecmp( token, "leftmargin=", 11 ) == 0) {
			e->leftBorder = atoi (token + 11);
		} else if ( strncasecmp( token, "rightmargin=", 12 ) == 0) {
			e->rightBorder = atoi (token + 12);
		} else if ( strncasecmp( token, "topmargin=", 10 ) == 0) {
			e->topBorder = atoi (token + 10);
		} else if ( strncasecmp( token, "bottommargin=", 13 ) == 0) {
			e->bottomBorder = atoi (token + 13);
		} else if ( strncasecmp( token, "marginwidth=", 12 ) == 0) {
			e->leftBorder = e->rightBorder = atoi (token + 12);
		} else if ( strncasecmp( token, "marginheight=", 13 ) == 0) {
			e->topBorder = e->bottomBorder = atoi (token + 13);
		}
	}
	
	gtk_html_debug_log (e->widget, "parsed <body>\n");
}

static void
element_parse_base (HTMLEngine *e, HTMLObject *clue, const char *str)
{
	html_string_tokenizer_tokenize( e->st, str + 5, " >" );
	while ( html_string_tokenizer_has_more_tokens (e->st) ) {
		const char* token = html_string_tokenizer_next_token(e->st);
		if ( strncasecmp( token, "target=", 7 ) == 0 ) {
			g_signal_emit (e, signals [SET_BASE_TARGET], 0, token + 7);
		} else if ( strncasecmp( token, "href=", 5 ) == 0 ) {
			g_signal_emit (e, signals [SET_BASE], 0, token + 5);
		}
	}
}


static void
element_parse_data (HTMLEngine *e, HTMLObject *clue, const char *str)
{
	gchar *key = NULL;
	gchar *class_name = NULL;
	
	html_string_tokenizer_tokenize (e->st, str + 5, " >" );
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const gchar *token = html_string_tokenizer_next_token (e->st);
		if (strncasecmp (token, "class=", 6 ) == 0) {
			g_free (class_name);
			class_name = g_strdup (token + 6);
		} else if (strncasecmp (token, "key=", 4 ) == 0) {
			g_free (key);
			key = g_strdup (token + 4);
		} else if (class_name && key && strncasecmp (token, "value=", 6) == 0) {
			if (class_name) {
				html_engine_set_class_data (e, class_name, key, token + 6);
				if (!strcmp (class_name, "ClueFlow") && e->flow)
					html_engine_set_object_data (e, e->flow);
			}
		} else if (strncasecmp (token, "clear=", 6) == 0)
			if (class_name)
				html_engine_clear_class_data (e, class_name, token + 6);
		/* TODO clear flow data */
	}
	g_free (class_name);
	g_free (key);
}


static void
form_begin (HTMLEngine *e, HTMLObject *clue, gchar *action, gchar *method, gboolean close_paragraph)
{
	e->form = html_form_new (e, action, method);
	e->formList = g_list_append (e->formList, e->form);

	if (! e->avoid_para && close_paragraph) {
		if (e->flow && HTML_CLUE (e->flow)->head)
			close_flow (e, clue);
		e->avoid_para = FALSE;
	}
}

static void
block_end_form (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	e->form = NULL;

	if (!e->avoid_para && elem && elem->miscData1) {
		close_flow (e, clue);
	}
}

static void
element_parse_input (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	enum InputType { CheckBox, Hidden, Radio, Reset, Submit, Text, Image,
			 Button, Password, Undefined };
	HTMLObject *element = NULL;
	const char *p;
	enum InputType type = Text;
	gchar *name = NULL;
	gchar *value = NULL;
	gchar *imgSrc = NULL;
	gboolean checked = FALSE;
	int size = 20;
	int maxLen = -1;
	int imgHSpace = 0;
	int imgVSpace = 0;
	gboolean fix_form = FALSE;
	
	if (e->form == NULL) {
		fix_form = TRUE;
		form_begin (e, clue, NULL, "GET", FALSE);
	}		

	html_string_tokenizer_tokenize (e->st, str + 6, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const gchar *token = html_string_tokenizer_next_token (e->st);

		if ( strncasecmp( token, "type=", 5 ) == 0 ) {
			p = token + 5;
			if ( strncasecmp( p, "checkbox", 8 ) == 0 )
				type = CheckBox;
			else if ( strncasecmp( p, "password", 8 ) == 0 )
				type = Password;
			else if ( strncasecmp( p, "hidden", 6 ) == 0 )
				type = Hidden;
			else if ( strncasecmp( p, "radio", 5 ) == 0 )
				type = Radio;
			else if ( strncasecmp( p, "reset", 5 ) == 0 )
				type = Reset;
			else if ( strncasecmp( p, "submit", 5 ) == 0 )
				type = Submit;
			else if ( strncasecmp( p, "button", 6 ) == 0 )
				type = Button;
			else if ( strncasecmp( p, "text", 5 ) == 0 )
				type = Text;
			else if ( strncasecmp( p, "image", 5 ) == 0 )
				type = Image;
		}
		else if ( strncasecmp( token, "name=", 5 ) == 0 ) {
			name = g_strdup(token + 5);
		}
		else if ( strncasecmp( token, "value=", 6 ) == 0 ) {
			value = g_strdup(token + 6);
		}
		else if ( strncasecmp( token, "size=", 5 ) == 0 ) {
			size = atoi( token + 5 );
		}
		else if ( strncasecmp( token, "maxlength=", 10 ) == 0 ) {
			maxLen = atoi( token + 10 );
		}
		else if ( strncasecmp( token, "checked", 7 ) == 0 ) {
			checked = TRUE;
		}
		else if ( strncasecmp( token, "src=", 4 ) == 0 ) {
			imgSrc = g_strdup (token + 4);
		}
		else if ( strncasecmp( token, "onClick=", 8 ) == 0 ) {
			/* TODO: Implement Javascript */
		}
		else if ( strncasecmp( token, "hspace=", 7 ) == 0 ) {
			imgHSpace = atoi (token + 7);
		}
		else if ( strncasecmp( token, "vspace=", 7 ) == 0 ) {
			imgVSpace = atoi (token + 7);
		}
	}
	switch ( type ) {
	case CheckBox:
		element = html_checkbox_new(GTK_WIDGET(e->widget), name, value, checked);
		break;
	case Hidden:
		{
		HTMLObject *hidden = html_hidden_new(name, value);

		html_form_add_hidden (e->form, HTML_HIDDEN (hidden));

		break;
		}
	case Radio:
		element = html_radio_new(GTK_WIDGET(e->widget), name, value, checked, e->form);
		break;
	case Reset:
		element = html_button_new(GTK_WIDGET(e->widget), name, value, BUTTON_RESET);
		break;
	case Submit:
		element = html_button_new(GTK_WIDGET(e->widget), name, value, BUTTON_SUBMIT);
		break;
	case Button:
		element = html_button_new(GTK_WIDGET(e->widget), name, value, BUTTON_NORMAL);
		break;
	case Text:
	case Password:
		element = html_text_input_new(GTK_WIDGET(e->widget), name, value, size, maxLen, (type == Password));
		break;
	case Image:
		/* FIXME fixup missing url */
		if (imgSrc) {
			element = html_imageinput_new (e->image_factory, name, imgSrc);
			html_image_set_spacing (HTML_IMAGE (HTML_IMAGEINPUT (element)->image), imgHSpace, imgVSpace);
		}
		break;
	case Undefined:
		g_warning ("Unknown <input type>\n");
		break;
	}
	if (element) {
		append_element (e, clue, element);
		html_form_add_element (e->form, HTML_EMBEDDED (element));
	}

	if (name)
		g_free (name);
	if (value)
		g_free (value);
	if (imgSrc)
		g_free (imgSrc);
	
	if (fix_form)
		block_end_form (e, clue, NULL);
}

static void
element_parse_form (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	gchar *action = NULL;
	gchar *method = "GET";
	gchar *target = NULL;
	
	html_string_tokenizer_tokenize (e->st, str + 5, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const gchar *token = html_string_tokenizer_next_token (e->st);
		
		if ( strncasecmp( token, "action=", 7 ) == 0 ) {
			action = g_strdup (token + 7);
		} else if ( strncasecmp( token, "method=", 7 ) == 0 ) {
			if ( strncasecmp( token + 7, "post", 4 ) == 0 )
				method = "POST";
		} else if ( strncasecmp( token, "target=", 7 ) == 0 ) {
			target = g_strdup(token + 7);
		}
	}
	
	form_begin (e, clue, action, method, TRUE);
	g_free(action);
	g_free(target);
	
	push_block (e, ID_FORM, 1, block_end_form, TRUE, 0);
}

static void
element_parse_frame (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	char *src = NULL;
	HTMLObject *frame = NULL;
	gint margin_height = -1;
	gint margin_width = -1;
	GtkPolicyType scroll = GTK_POLICY_AUTOMATIC;
	
	if (!e->allow_frameset)
			return;
	
	src = NULL;
	html_string_tokenizer_tokenize (e->st, str + 5, " >");
	
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const gchar *token = html_string_tokenizer_next_token (e->st);
		
		if (strncasecmp (token, "src=", 4) == 0) {
			src = g_strdup (token + 4);
		} else if (strncasecmp (token, "noresize", 8) == 0) {
		} else if (strncasecmp (token, "frameborder=", 12) == 0) {
		} else if (strncasecmp (token, "border=", 7) == 0) {
			/*
			 * Netscape and Mozilla recognize this to turn of all the between
			 * frame decoration.
			 */
		} else if (strncasecmp(token, "marginwidth=", 12) == 0) {
			margin_width = atoi (token + 12);
		} else if (strncasecmp(token, "marginheight=", 13) == 0) {
			margin_height = atoi (token + 13);
		} else if (strncasecmp(token, "scrolling=", 10) == 0) {
			scroll = parse_scroll (token + 10);
		}
	}
	
	frame = html_frame_new (GTK_WIDGET (e->widget), src, -1 , -1, FALSE);
	if (!html_frameset_append (html_stack_top (e->frame_stack), frame))
		html_object_destroy (frame);
	
	if (margin_height > 0)
		html_frame_set_margin_height (HTML_FRAME (frame), margin_height);
	if (margin_width > 0)
		html_frame_set_margin_width (HTML_FRAME (frame), margin_width);
	if (scroll != GTK_POLICY_AUTOMATIC)
		html_frame_set_scrolling (HTML_FRAME (frame), scroll);
	
	g_free (src);
}


static void
element_parse_hr (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	gint size = 2;
	gint length = clue->max_width;
	gint percent = 100;
	HTMLHAlignType align = HTML_HALIGN_CENTER;
	gboolean shade = TRUE;
	
	html_string_tokenizer_tokenize (e->st, str + 3, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		gchar *token = html_string_tokenizer_next_token (e->st);
		if (strncasecmp (token, "align=", 6) == 0) {
			align = parse_halign (token + 6, align);
		}
		else if (strncasecmp (token, "size=", 5) == 0) {
			size = atoi (token + 5);
		}
		else if (strncasecmp (token, "width=", 6) == 0) {
			if (strchr (token + 6, '%'))
				percent = atoi (token + 6);
			else if (isdigit (*(token + 6))) {
				length = atoi (token + 6);
				percent = 0;
			}
		}
		else if (strncasecmp (token, "noshade", 7) == 0) {
			shade = FALSE;
		}
	}
	
	pop_element (e, ID_P);
	append_element (e, clue, html_rule_new (length, percent, size, shade, align));
	close_flow (e, clue);
}


static void
block_end_heading (HTMLEngine *e,
		   HTMLObject *clue,
		   HTMLElement *elem)
{
	block_end_clueflow_style (e, clue, elem);

	e->avoid_para = TRUE;
}

static void
element_end_heading (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	pop_element (e, "h1");
	pop_element (e, "h2");
	pop_element (e, "h3");
	pop_element (e, "h4");
	pop_element (e, "h5");
	pop_element (e, "h6");
}

static void
element_parse_heading (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	HTMLClueFlowStyle fstyle;
	HTMLStyle *style = NULL;
	
	element_end_heading (e, clue, str);

	fstyle = HTML_CLUEFLOW_STYLE_H1 + (str[1] - '1');
	style = html_style_set_decoration (style, GTK_HTML_FONT_STYLE_BOLD);
	switch (fstyle) {
	case HTML_CLUEFLOW_STYLE_H6:
		html_style_set_font_size (style, GTK_HTML_FONT_STYLE_SIZE_1);
		break;
	case HTML_CLUEFLOW_STYLE_H5:
		html_style_set_font_size (style, GTK_HTML_FONT_STYLE_SIZE_2);
		break;
	case HTML_CLUEFLOW_STYLE_H4:
		html_style_set_font_size (style, GTK_HTML_FONT_STYLE_SIZE_3);
		break;
	case HTML_CLUEFLOW_STYLE_H3:
		html_style_set_font_size (style, GTK_HTML_FONT_STYLE_SIZE_4);
		break;
	case HTML_CLUEFLOW_STYLE_H2:
		html_style_set_font_size (style, GTK_HTML_FONT_STYLE_SIZE_5);
		break;
	case HTML_CLUEFLOW_STYLE_H1:
		html_style_set_font_size (style, GTK_HTML_FONT_STYLE_SIZE_6);
		break;
	default:
		break;
	}
	
	html_string_tokenizer_tokenize (e->st, str + 3, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		gchar *token;
		
		token = html_string_tokenizer_next_token (e->st);
		if (strncasecmp (token, "align=", 6) == 0) {
			style = html_style_add_text_align (style, parse_halign (token + 6, HTML_HALIGN_NONE));
			//align = parse_halign (token + 6, align);
		} else if (strncasecmp (token, "style=", 6) == 0) {
			style = html_style_add_attribute (style, token + 6);
		}
	}
	
	/* FIXME this is temporary until the paring can be moved.*/
	{
		char *name = parse_element_name (str);
		push_block_element (e, name, style, 1, block_end_heading, 0, 0);
		g_free (name);
	}
	push_clueflow_style (e, fstyle);	
	close_flow (e, clue);

	e->avoid_para = TRUE;
}

static void
element_parse_img (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	HTMLObject *image = 0;
	HTMLHAlignType align = HTML_HALIGN_NONE;
	HTMLVAlignType valign = HTML_VALIGN_NONE;
	HTMLColor *color = NULL;
	gchar *token = 0; 
	gchar *tmpurl = NULL;
	gchar *mapname = NULL;
	gchar *id = NULL;
	gchar *alt = NULL;
	gint width = -1;
	gint height = -1;
	gint border = 0;
	gint hspace = 0;
	gint vspace = 0;
	gboolean percent_width  = FALSE;
	gboolean percent_height = FALSE;
	gboolean ismap = FALSE;

	color        = current_color (e);
	if (e->url != NULL || e->target != NULL)
		border = 2;

	if (e->url != NULL || e->target != NULL)
		border = 2;
	
	html_string_tokenizer_tokenize (e->st, str + 4, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		token = html_string_tokenizer_next_token (e->st);
		if (strncasecmp (token, "src=", 4) == 0) {
			tmpurl = g_strdup (token + 4);
		} else if (strncasecmp (token, "width=", 6) == 0) {
			if (isdigit (*(token + 6)))
				width = atoi (token + 6);
			percent_width = strchr (token + 6, '%') ? TRUE : FALSE;
		} else if (strncasecmp (token, "height=", 7) == 0) {
			if (isdigit (*(token + 7)))
				height = atoi (token + 7);
			percent_height = strchr (token + 7, '%') ? TRUE : FALSE;
		} else if (strncasecmp (token, "border=", 7) == 0) {
			border = atoi (token + 7);
		} else if (strncasecmp (token, "hspace=", 7) == 0) {
			hspace = atoi (token + 7);
		} else if (strncasecmp (token, "vspace=", 7) == 0) {
				vspace = atoi (token + 7);
		} else if (strncasecmp (token, "align=", 6) == 0) {
			if (strcasecmp (token + 6, "left") == 0)
				align = HTML_HALIGN_LEFT;
			else if (strcasecmp (token + 6, "right") == 0)
				align = HTML_HALIGN_RIGHT;
			else if (strcasecmp (token + 6, "top") == 0)
				valign = HTML_VALIGN_TOP;
			else if (strcasecmp (token + 6, "middle") == 0)
				valign = HTML_VALIGN_MIDDLE;
			else if (strcasecmp (token + 6, "bottom") ==0)
				valign = HTML_VALIGN_BOTTOM;
		} else if (strncasecmp (token, "id=", 3) == 0) {
			id = token + 3;
		} else if (strncasecmp (token, "alt=", 4) == 0) {
			alt = g_strdup (token + 4);
		} else if (strncasecmp (token, "usemap=", 7) == 0) {
			mapname = g_strdup (token + 7);
		} else if (strncasecmp (token, "ismap", 5) == 0) {
			ismap = TRUE;
		}
	}
	
	/* FIXME fixup missing url */
	if (!tmpurl)
		return;

	if (align != HTML_HALIGN_NONE)
		valign = HTML_VALIGN_BOTTOM;
	else if (valign == HTML_VALIGN_NONE)
		valign = HTML_VALIGN_BOTTOM;
	
	image = html_image_new (e->image_factory, tmpurl,
				e->url, e->target,
				width, height,
				percent_width, percent_height, border, color, valign, FALSE);
	
	if (id) 
		html_engine_add_object_with_id (e, id, (HTMLObject *) image);
	
	
	if (hspace < 0)
		hspace = 0;
	if (vspace < 0)
		vspace = 0;
	
	html_image_set_spacing (HTML_IMAGE (image), hspace, vspace);
	
	if (alt) {
		html_image_set_alt (HTML_IMAGE (image), alt);
		g_free (alt);
	}
	
	html_image_set_map (HTML_IMAGE (image), mapname, ismap);
	
	g_free (tmpurl);
	g_free (mapname);
	
	if (align == HTML_HALIGN_NONE) {
		append_element (e, clue, image);
	} else {
		/* We need to put the image in a HTMLClueAligned.  */
		/* Man, this is *so* gross.  */
		HTMLClueAligned *aligned = HTML_CLUEALIGNED (html_cluealigned_new (NULL, 0, 0, clue->max_width, 100));
		HTML_CLUE (aligned)->halign = align;
		html_clue_append (HTML_CLUE (aligned), HTML_OBJECT (image));
		append_element (e, clue, HTML_OBJECT (aligned));
	}
}

static void
element_parse_i (HTMLEngine *e, HTMLObject *_clue, const gchar *str)
{
	
	HTMLStyle *style = NULL;
	gchar *token = 0; 
	gchar *id;
	
	style = html_style_set_decoration (style, GTK_HTML_FONT_STYLE_ITALIC);
	
	html_string_tokenizer_tokenize (e->st, str + 1, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		token = html_string_tokenizer_next_token (e->st);
		if (strncasecmp (token, "style=", 6) == 0) {
			style = html_style_add_attribute (style, token + 6);
		} else if (strncasecmp (token, "id=", 3) == 0) {
			id = token + 3;
		}
	}
	push_element (e, ID_I, NULL, style);
}


static void
element_parse_meta (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	int refresh = 0;
	int refresh_delay = 0;
	gchar *refresh_url = NULL;
	
	html_string_tokenizer_tokenize( e->st, str + 5, " >" );
	while ( html_string_tokenizer_has_more_tokens (e->st) ) {

		const gchar* token = html_string_tokenizer_next_token(e->st);
		if ( strncasecmp( token, "http-equiv=", 11 ) == 0 ) {
			if ( strncasecmp( token + 11, "refresh", 7 ) == 0 )
				refresh = 1;
		} else if ( strncasecmp( token, "content=", 8 ) == 0 ) {
			if (refresh) {
				const gchar *content;
				content = token + 8;
				
				/* The time in seconds until the refresh */
				refresh_delay = atoi(content);
				
				html_string_tokenizer_tokenize(e->st, content, ",;> ");
				while ( html_string_tokenizer_has_more_tokens (e->st) ) {
					const gchar* token = html_string_tokenizer_next_token(e->st);
					if ( strncasecmp( token, "url=", 4 ) == 0 )
						refresh_url = g_strdup (token + 4);
				}
				
				g_signal_emit (e, signals [REDIRECT], 0, refresh_url, refresh_delay);
				if(refresh_url)
					g_free(refresh_url);
			}
		}
	}
}

static void
element_parse_map (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	pop_element (e, ID_MAP);
	
	html_string_tokenizer_tokenize (e->st, str + 3, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const char* token = html_string_tokenizer_next_token (e->st);
		if (strncasecmp (token, "name=", 5) == 0) {
			const char *name = token + 5;
			
			html_engine_add_map (e, name);
		}
	}
	/* FIXME map nesting */
	push_block (e, ID_MAP, 0, block_end_map, FALSE, FALSE);
}


/* list parsers */
static HTMLListType
get_list_type (gchar c)
{
	switch (c) {
	case 'i':
		return HTML_LIST_TYPE_ORDERED_LOWER_ROMAN;
	case 'I':
		return HTML_LIST_TYPE_ORDERED_UPPER_ROMAN;
	case 'a':
		return HTML_LIST_TYPE_ORDERED_LOWER_ALPHA;
	case 'A':
		return HTML_LIST_TYPE_ORDERED_UPPER_ALPHA;
	case '1':
	default:
		return HTML_LIST_TYPE_ORDERED_ARABIC;		
	}
}

static void
block_end_item (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	finish_flow (e, clue);
}

static void
element_parse_li (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	HTMLListType listType;
	gint listLevel;
	gint itemNumber;
	
	listType = HTML_LIST_TYPE_UNORDERED;
	listLevel = 1;
	itemNumber = 1;
	
	pop_element (e, ID_LI);
	
	if (!html_stack_is_empty (e->listStack)) {
		HTMLList *top;
		
		top = html_stack_top (e->listStack);
		
		listType = top->type;
		itemNumber = top->itemNumber;
	}
	
	html_string_tokenizer_tokenize (e->st, str + 3, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const gchar *token = html_string_tokenizer_next_token (e->st);
		
		if (!strncasecmp (token, "value=", 6))
			itemNumber = atoi (token + 6);
		else if (!strncasecmp (token, "type=", 5))
			listType = get_list_type (token [5]);
	}
	
	if (!html_stack_is_empty (e->listStack)) {
		HTMLList *list;
		
		list = html_stack_top (e->listStack);
		list->itemNumber = itemNumber + 1;
	}
	
	e->flow = flow_new (e, HTML_CLUEFLOW_STYLE_LIST_ITEM, listType, itemNumber, HTML_CLEAR_NONE);
	html_clueflow_set_item_color (HTML_CLUEFLOW (e->flow), current_color (e));
	
	html_clue_append (HTML_CLUE (clue), e->flow);
	e->avoid_para = TRUE;
	push_block (e, ID_LI, 1, block_end_item, FALSE, FALSE);
}


static void
block_end_list (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	html_list_destroy (html_stack_pop (e->listStack));

	finish_flow (e, clue);

	e->avoid_para = FALSE;
}

static void
element_parse_ol (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	HTMLList *list;
	HTMLListType listType = HTML_LIST_TYPE_ORDERED_ARABIC;
	
	pop_element (e, ID_LI);
	finish_flow (e, clue);

	/* FIXME */
	push_block (e, ID_OL, 1, block_end_list, FALSE, FALSE);
	
	html_string_tokenizer_tokenize( e->st, str + 3, " >" );
	
	while ( html_string_tokenizer_has_more_tokens (e->st) ) {
		const char* token;
		
		token = html_string_tokenizer_next_token (e->st);
		if ( strncasecmp( token, "type=", 5 ) == 0 )
			listType = get_list_type (token [5]);
	}
	
	e->flow = NULL;
	html_stack_push (e->listStack, html_list_new (listType));
}

static void
element_parse_ul (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	pop_element (e, ID_LI);
	finish_flow (e, clue);
	
	push_block (e, ID_UL, 1, block_end_list, FALSE, FALSE);
	
	html_string_tokenizer_tokenize (e->st, str + 3, " >");
	while (html_string_tokenizer_has_more_tokens (e->st))
		html_string_tokenizer_next_token (e->st);
	
	e->flow = NULL;
	
	html_stack_push (e->listStack, html_list_new (HTML_LIST_TYPE_UNORDERED));
	
	e->avoid_para = TRUE;
}

static void
block_end_glossary (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	html_list_destroy (html_stack_pop (e->listStack));
	block_end_item (e, clue, elem);
}

static void
element_parse_dd (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	pop_element (e, ID_DT);
	pop_element (e, ID_DD);
	
	close_flow (e, clue);
	
	push_block (e, ID_DD, 1, block_end_glossary, FALSE, FALSE);
	html_stack_push (e->listStack, html_list_new (HTML_LIST_TYPE_GLOSSARY_DD));
}

static void
element_parse_dt (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	pop_element (e, ID_DT);
	pop_element (e, ID_DD);
	
	close_flow (e, clue);
	
	/* FIXME this should set the item flag */
	push_block (e, ID_DT, 1, block_end_item, FALSE, FALSE);		
}

static void
element_parse_dl (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	close_flow (e, clue);
	
	push_block (e, ID_DL, 1, block_end_list, FALSE, FALSE);
	html_stack_push (e->listStack, html_list_new (HTML_LIST_TYPE_GLOSSARY_DL));
}

static void
element_parse_dir (HTMLEngine *e, HTMLObject *clue, const char *str)
{
	pop_element (e, ID_LI);
	finish_flow (e, clue);
	
	push_block (e, ID_DIR, 1, block_end_list, FALSE, FALSE);
	html_stack_push (e->listStack, html_list_new (HTML_LIST_TYPE_DIR));
	
	/* FIXME shouldn't it create a new flow? */
}

static void
element_parse_option (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	gchar *value = NULL;
	gboolean selected = FALSE;
	
	if ( !e->formSelect )
		return;
	
	html_string_tokenizer_tokenize( e->st, str + 3, " >" );
	
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const char* token;
		
		token = html_string_tokenizer_next_token (e->st);
		
		if (strncasecmp (token, "value=", 6 ) == 0) {
			value = g_strdup (token + 6);
		} else if (strncasecmp (token, "selected", 8) == 0) {
			selected = TRUE;
		}
	}
	
	if ( e->inOption )
		html_select_set_text (e->formSelect, e->formText->str);
	
	html_select_add_option (e->formSelect, value, selected );
	g_free (value);
	
	e->inOption = TRUE;
	g_string_assign (e->formText, "");
	
	push_block (e, ID_OPTION, 0, block_end_option, FALSE, FALSE);
}

static void
element_parse_select (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	gchar *name = NULL;
	gint size = 0;
	gboolean multi = FALSE;
	
	if (!e->form)
		return;
	
	html_string_tokenizer_tokenize (e->st, str + 7, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const gchar *token = html_string_tokenizer_next_token (e->st);
		
		if ( strncasecmp( token, "name=", 5 ) == 0 ) {
			name = g_strdup(token + 5);
		} else if ( strncasecmp( token, "size=", 5 ) == 0 ) {
			size = atoi (token + 5);
		} else if ( strncasecmp( token, "multiple", 8 ) == 0 ) {
			multi = TRUE;
		}
	}
	
	e->formSelect = HTML_SELECT (html_select_new (GTK_WIDGET(e->widget), name, size, multi));
	html_form_add_element (e->form, HTML_EMBEDDED ( e->formSelect ));
	
	append_element (e, clue, HTML_OBJECT (e->formSelect));
	
	g_free(name);
	push_block (e, ID_SELECT, 0, block_end_select, FALSE, FALSE);
}

/* table parsing logic */
static void
block_end_table (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem) 
{
	HTMLTable *table;
	HTMLHAlignType table_align = elem->miscData1;
	HTMLHAlignType clue_align = elem->miscData2;

	table = html_stack_top (e->table_stack);
	html_stack_pop (e->table_stack);

        if (table) { 
		if (table->col == 0 && table->row == 0) {
			DT(printf ("deleting empty table %p\n", table);)
			html_object_destroy (HTML_OBJECT (table));
			return;
		}
		  
		if (table_align != HTML_HALIGN_LEFT && table_align != HTML_HALIGN_RIGHT) {

		        finish_flow (e, clue);

			DT(printf ("unaligned table(%p)\n", table);)
			append_element (e, clue, HTML_OBJECT (table));

			if (table_align == HTML_HALIGN_NONE && e->flow)
				/* use the alignment we saved from when the clue was created */
				HTML_CLUE (e->flow)->halign = clue_align;
			else {
				/* for centering we don't need to create a cluealigned */
				HTML_CLUE (e->flow)->halign = table_align;
			}

			close_flow (e, clue);
		} else {
			HTMLClueAligned *aligned = HTML_CLUEALIGNED (html_cluealigned_new (NULL, 0, 0, clue->max_width, 100));
			HTML_CLUE (aligned)->halign = table_align;

			DT(printf ("ALIGNED table(%p)\n", table);)

			html_clue_append (HTML_CLUE (aligned), HTML_OBJECT (table));
			append_element (e, clue, HTML_OBJECT (aligned));
		}
	}
}

static void
close_current_table (HTMLEngine *e)
{
	HTMLElement *span;
	GList *item;
	
	for (item = e->span_stack->list; item; item = item->next) {
		span = item->data;
		
		DT(printf ("%d:", span->id);)
		if (ID_EQ (span->id, ID_TABLE))
			break;

		if (ID_EQ (span->id, ID_TH) || ID_EQ (span->id, ID_TD)) {
			DT(printf ("found cell\n");)
			return;
		}
	}

	DT(printf ("pop_table\n");)
	pop_element (e, ID_TABLE);
}

static void
element_parse_table (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	HTMLTable *table;
	HTMLStyle *style = NULL;
	HTMLHAlignType align = HTML_HALIGN_NONE;
	
	GdkColor tableColor;
	gboolean have_tableColor = FALSE;
	
	gint width = 0;
	gint percent = 0;
	gint padding = 1;
	gint spacing = 2;
	gint border = 0;
	
	/* see test16.html test0023.html and test0024.html */
	/* pop_element (e, ID_A); */
	close_current_table (e);
	
	html_string_tokenizer_tokenize (e->st, str + 5, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const gchar *token = html_string_tokenizer_next_token (e->st);
		if (strncasecmp (token, "cellpadding=", 12) == 0) {
			padding = atoi (token + 12);
		}
		else if (strncasecmp (token, "cellspacing=", 12) == 0) {
			spacing = atoi (token + 12);
		}
		else if (strncasecmp (token, "border", 6) == 0) {
			if (*(token + 6) == '=')
				border = atoi (token + 7);
			else
				border = 1;
		}
		else if (strncasecmp (token, "width=", 6) == 0) {
			if (strchr (token + 6, '%')) {
				percent = atoi (token + 6);
			} else if (strchr (token + 6, '*')) {
				/* Ignore */
			} else if (isdigit (*(token + 6))) {
				width = atoi (token + 6);
			}
		}
		else if (strncasecmp (token, "align=", 6) == 0) {
			align = parse_halign (token + 6, align);
		}
		else if (strncasecmp (token, "bgcolor=", 8) == 0
			 && !e->defaultSettings->forceDefault) {
			if (parse_color (token + 8, &tableColor)) {
				have_tableColor = TRUE;
			}
		}
		else if (strncasecmp (token, "background=", 11) == 0
			 && token [12]
			 && !e->defaultSettings->forceDefault) {
			style = html_style_add_background_image (style, (char *)token + 11);
		}
	}
	
	table = HTML_TABLE (html_table_new (width, percent, padding, spacing, border));
	
	if (have_tableColor)
		table->bgColor = gdk_color_copy (&tableColor);
	
	if (style && style->bg_image) {
		table->bgPixmap = html_image_factory_register (e->image_factory, NULL, style->bg_image, FALSE);
	}
	
	html_stack_push (e->table_stack, table);
	push_block_element (e, ID_TABLE, style, 6, block_end_table, align, current_alignment (e));
	
	e->avoid_para = FALSE;
}
     
static void
block_end_row (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	HTMLTable *table = html_stack_top (e->table_stack);
	
	if (table) {
		html_table_end_row (table);
	}
}

static void
block_ensure_row (HTMLEngine *e) 
{
	HTMLElement *span;
	HTMLTable *table;
	GList *item;

	table = html_stack_top (e->table_stack);
	if (!table)
		return;
	
	for (item = e->span_stack->list; item; item = item->next) {
		span = item->data;

		DT(printf ("%d:", span->id);)
		if (ID_EQ (span->id, ID_TR)) {
			DT(printf ("no ensure row\n");)
			return;
		}

		if (ID_EQ (span->id, ID_TABLE))
			break;
		    
	}
	
	html_table_start_row (table);
	push_block_element (e, ID_TR, NULL, 3, block_end_row, 0, 0);
}

static void
element_parse_tr (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	HTMLTable *table = html_stack_top (e->table_stack);
	gboolean have_rowColor;
	gboolean have_rowPixmap;
	GdkColor rowColor;
	HTMLStyle *style = NULL;
	
	if (!table)
		return;
	
	pop_element (e, ID_CAPTION);
	pop_element (e, ID_TR);
	
	have_rowColor = FALSE;
	have_rowPixmap = FALSE;
	
	html_string_tokenizer_tokenize (e->st, str + 2, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const gchar *token = html_string_tokenizer_next_token (e->st);
		if (strncasecmp (token, "valign=", 7) == 0) {
			if (strncasecmp (token + 7, "top", 3) == 0)
				style = html_style_add_text_valign (style, HTML_VALIGN_TOP);
			else if (strncasecmp (token + 7, "bottom", 6) == 0)
				style = html_style_add_text_valign (style, HTML_VALIGN_BOTTOM);
			else
				style = html_style_add_text_valign (style, HTML_VALIGN_MIDDLE);
		} else if (strncasecmp (token, "align=", 6) == 0) {
			style = html_style_add_text_align (style, parse_halign (token + 6, HTML_HALIGN_NONE));
		} else if (strncasecmp (token, "bgcolor=", 8) == 0) {
			if (parse_color (token + 8, &rowColor))
				style = html_style_add_background_color (style, &rowColor);
		} else if (strncasecmp (token, "background=", 11) == 0) {
			if (strlen (token + 11) != 0)
				style = html_style_add_background_image (style, token + 11);
		}
	}
	
	html_table_start_row (table);
	push_block_element (e, ID_TR, style, 3, block_end_row, 0, 0);
}

static void
element_parse_caption (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	HTMLTable *table = html_stack_top (e->table_stack);
	HTMLStyle *style = NULL;
	HTMLClueV *caption;
	HTMLVAlignType capAlign = HTML_VALIGN_MIDDLE;

	/* CAPTIONS are all wrong they don't even get render */
	/* Make sure this is a valid position for a caption */
	if (!table)
			return;
	
	pop_element (e, ID_TR);
	pop_element (e, ID_CAPTION);
	
	html_string_tokenizer_tokenize( e->st, str + 7, " >" );
	while ( html_string_tokenizer_has_more_tokens (e->st) ) {
		const char* token = html_string_tokenizer_next_token(e->st);
		if ( strncasecmp( token, "align=", 6 ) == 0) {
			if ( strncasecmp( token+6, "top", 3 ) == 0)
				capAlign = HTML_VALIGN_TOP;
		}
	}
	
	caption = HTML_CLUEV (html_cluev_new (0, 0, 100));
	
	e->flow = 0;
	
	style = html_style_add_text_align (style, HTML_HALIGN_CENTER);
	
	push_clue (e, HTML_OBJECT (caption));
	push_block_element (e, ID_CAPTION, style, 3, block_end_cell, 0, 0);
	
	table->caption = caption;
	//FIXME caption alignment should be based on the flow.... or something.... 
	table->capAlign = capAlign;		
}

static void
element_parse_td (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	HTMLStyle *style = NULL;
	HTMLTable *table = html_stack_top (e->table_stack);
	gint rowSpan = 1;
	gint colSpan = 1;
	gint cellwidth = clue->max_width;
	gint cellheight = -1;
	gboolean cellwidth_percent = FALSE;
	gboolean cellheight_percent = FALSE;
	gboolean no_wrap = FALSE;
	gboolean fixedWidth = FALSE;
	gboolean fixedHeight = FALSE;
	HTMLTableCell *cell = NULL;
	char *image_url = NULL;
	HTMLVAlignType valign;
	HTMLHAlignType halign;
	gboolean have_bgColor = FALSE;
	GdkColor bgColor;
	
	if (!table)
		return;
	
	pop_element (e, ID_TH);
	pop_element (e, ID_TD);
	pop_element (e, ID_CAPTION);
	
	valign = current_row_valign (e); 
	halign = current_row_align (e);
	
	style = html_style_unset_decoration (style, 0xffff);
	style = html_style_set_font_size (style, GTK_HTML_FONT_STYLE_SIZE_3);
	
	html_string_tokenizer_tokenize (e->st, str + 2, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const gchar *token = html_string_tokenizer_next_token (e->st);
		
		if (strncasecmp (token, "rowspan=", 8) == 0) {
			rowSpan = atoi (token + 8);
			if (rowSpan < 1)
				rowSpan = 1;
		}
		else if (strncasecmp (token, "colspan=", 8) == 0) {
			colSpan = atoi (token + 8);
			if (colSpan < 1)
				colSpan = 1;
		}
		else if (strncasecmp (token, "valign=", 7) == 0) {
			if (strncasecmp (token + 7, "top", 3) == 0)
				valign = HTML_VALIGN_TOP;
			else if (strncasecmp (token + 7, "bottom", 6) == 0)
				valign = HTML_VALIGN_BOTTOM;
			else 
				valign = HTML_VALIGN_MIDDLE;
		}
		else if (strncasecmp (token, "align=", 6) == 0) {
			style = html_style_add_text_align (style, parse_halign (token + 6, HTML_HALIGN_NONE));
		}
		else if (strncasecmp (token, "height=", 7) == 0) {
			if (strchr (token + 7, '%')) {
				/* gtk_html_debug_log (e->widget, "percent!\n");
				   cellheight = atoi (token + 7);
				   cellheight_percent = TRUE;
				   fixedHeight = TRUE; */
			}
			else if (strchr (token + 7, '*')) {
				/* ignore */
			}
			else if (isdigit (*(token + 7))) {
				cellheight = atoi (token + 7);
				cellheight_percent = FALSE;
				fixedHeight = TRUE;
			}
		}
		else if (strncasecmp (token, "width=", 6) == 0) {
			if (strchr (token + 6, '%')) {
				gtk_html_debug_log (e->widget, "percent!\n");
				cellwidth = atoi (token + 6);
				cellwidth_percent = TRUE;
				fixedWidth = TRUE;
			}
			else if (strchr (token + 6, '*')) {
				/* ignore */
			}
			else if (isdigit (*(token + 6))) {
				cellwidth = atoi (token + 6);
				cellwidth_percent = FALSE;
				fixedWidth = TRUE;
			}
		}
		else if (strncasecmp (token, "bgcolor=", 8) == 0
			 && !e->defaultSettings->forceDefault) {
			
			if (!strncasecmp (token + 8, "f3f2f1", 6))
				DT (g_warning ("magic table");)
					
					if (parse_color (token + 8, &bgColor))
						have_bgColor = TRUE;
		}
		else if (strncasecmp (token, "nowrap", 6) == 0) {
			no_wrap = TRUE;
		}
		else if (strncasecmp (token, "background=", 11) == 0
			 && token [12]
			 && !e->defaultSettings->forceDefault) {
			
			style = html_style_add_background_image (style, (char *)token + 11); 
		}
	}
	
	cell = HTML_TABLE_CELL (html_table_cell_new (rowSpan, colSpan, table->padding));
	cell->no_wrap = no_wrap;
	
	html_object_set_bg_color (HTML_OBJECT (cell), have_bgColor ? &bgColor : current_row_bg_color (e));
	
	if (style && style->bg_image) {
		image_url = style->bg_image;
	} else {
		image_url = current_row_bg_image (e);
	}
	
	if (image_url) {
		HTMLImagePointer *ip;
		
		ip = html_image_factory_register(e->image_factory, NULL, image_url, FALSE);
		html_table_cell_set_bg_pixmap (cell, ip);
	}
	
	HTML_CLUE (cell)->valign = valign;
	HTML_CLUE (cell)->halign = halign;
	
	if (fixedWidth)
		html_table_cell_set_fixed_width (cell, cellwidth, cellwidth_percent);
	if (fixedHeight)
		html_table_cell_set_fixed_height (cell, cellheight, cellheight_percent);
	
	block_ensure_row (e);
	html_table_add_cell (table, cell);
	push_clue (e, HTML_OBJECT (cell));
	push_block_element (e, ID_TD, style, 3, block_end_cell, 0, 0);
	//((HTMLElement *)html_stack_top (e->span_stack))->display = DISPLAY_TABLE_CELL;
}

static void
element_parse_th (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	HTMLStyle *style = NULL;
	HTMLTable *table = html_stack_top (e->table_stack);
	gint rowSpan = 1;
	gint colSpan = 1;
	gint cellwidth =  clue->max_width;
	gint cellheight = -1;
	gboolean cellwidth_percent = FALSE;
	gboolean cellheight_percent = FALSE;
	gboolean no_wrap = FALSE;
	gboolean fixedWidth = FALSE;
	gboolean fixedHeight = FALSE;
	HTMLVAlignType valign;
	HTMLHAlignType halign;
	HTMLTableCell *cell = NULL;
	char *image_url = NULL;
	
	gboolean have_bgColor = FALSE;
	GdkColor bgColor;
	
	if (!table)
		return;
		
	pop_element (e, ID_TH);
	pop_element (e, ID_TD);
	pop_element (e, ID_CAPTION);
	
	valign = current_row_valign (e);
	halign = current_row_align (e);
	
	style = html_style_unset_decoration (style, 0xffff);
	style = html_style_set_decoration (style, GTK_HTML_FONT_STYLE_BOLD);
	style = html_style_add_text_align (style, HTML_HALIGN_CENTER);
	style = html_style_set_font_size (style, GTK_HTML_FONT_STYLE_SIZE_3);
	
	html_string_tokenizer_tokenize (e->st, str + 2, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const gchar *token = html_string_tokenizer_next_token (e->st);
		
		if (strncasecmp (token, "rowspan=", 8) == 0) {
			rowSpan = atoi (token + 8);
			if (rowSpan < 1)
					rowSpan = 1;
		}
		else if (strncasecmp (token, "colspan=", 8) == 0) {
			colSpan = atoi (token + 8);
			if (colSpan < 1)
				colSpan = 1;
		}
		else if (strncasecmp (token, "valign=", 7) == 0) {
			if (strncasecmp (token + 7, "top", 3) == 0)
				valign = HTML_VALIGN_TOP;
			else if (strncasecmp (token + 7, "bottom", 6) == 0)
				valign = HTML_VALIGN_BOTTOM;
			else 
				valign = HTML_VALIGN_MIDDLE;
		}
		else if (strncasecmp (token, "align=", 6) == 0) {
			style = html_style_add_text_align (style, parse_halign (token + 6, HTML_HALIGN_NONE));
		}
		else if (strncasecmp (token, "height=", 7) == 0) {
			if (strchr (token + 7, '%')) {
				/* gtk_html_debug_log (e->widget, "percent!\n");
				   cellheight = atoi (token + 7);
				   cellheight_percent = TRUE;
				   fixedHeight = TRUE; */
			}
			else if (strchr (token + 7, '*')) {
				/* ignore */
			}
			else if (isdigit (*(token + 7))) {
				cellheight = atoi (token + 7);
				cellheight_percent = FALSE;
				fixedHeight = TRUE;
				}
		}
		else if (strncasecmp (token, "width=", 6) == 0) {
			if (strchr (token + 6, '%')) {
				gtk_html_debug_log (e->widget, "percent!\n");
				cellwidth = atoi (token + 6);
				cellwidth_percent = TRUE;
				fixedWidth = TRUE;
			}
			else if (strchr (token + 6, '*')) {
				/* ignore */
			}
			else if (isdigit (*(token + 6))) {
				cellwidth = atoi (token + 6);
				cellwidth_percent = FALSE;
				fixedWidth = TRUE;
			}
		}
		else if (strncasecmp (token, "bgcolor=", 8) == 0
			 && !e->defaultSettings->forceDefault) {
			
			if (parse_color (token + 8, &bgColor))
				have_bgColor = TRUE;
		}
		else if (strncasecmp (token, "nowrap", 6) == 0) {
			no_wrap = TRUE;
		}
		else if (strncasecmp (token, "background=", 11) == 0
			 && token [12]
			 && !e->defaultSettings->forceDefault) {
			
			style = html_style_add_background_image (style, token + 11); 
		}
	}
	
	cell = HTML_TABLE_CELL (html_table_cell_new (rowSpan, colSpan, table->padding));
	cell->no_wrap = no_wrap;
	cell->heading = TRUE;
	
	html_object_set_bg_color (HTML_OBJECT (cell), have_bgColor ? &bgColor : current_row_bg_color (e));
	
	
	if (style && style->bg_image) {
		image_url = style->bg_image;
	} else {
		image_url = current_row_bg_image (e);
	}
	
	if (image_url) {
		HTMLImagePointer *ip;
		
		ip = html_image_factory_register(e->image_factory, NULL, image_url, FALSE);
		html_table_cell_set_bg_pixmap (cell, ip);
	}
		
	HTML_CLUE (cell)->valign = valign;
	HTML_CLUE (cell)->halign = halign;
	
	if (fixedWidth)
		html_table_cell_set_fixed_width (cell, cellwidth, cellwidth_percent);
	if (fixedHeight)
		html_table_cell_set_fixed_height (cell, cellheight, cellheight_percent);
	
	block_ensure_row (e);
	html_table_add_cell (table, cell);
	push_clue (e, HTML_OBJECT (cell));
	push_block_element (e, ID_TH, style, 3, block_end_cell, 0, 0);
	//((HTMLElement *)html_stack_top (e->span_stack))->display = DISPLAY_TABLE_CELL;
}

static void
element_parse_textarea (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	gchar *name = NULL;
	gint rows = 5, cols = 40;
	
	if (!e->form)
		return;
	
	html_string_tokenizer_tokenize (e->st, str + 9, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const gchar *token = html_string_tokenizer_next_token (e->st);
		
		if (strncasecmp (token, "name=", 5) == 0) {
				name = g_strdup(token + 5);
		} else if (strncasecmp (token, "rows=", 5) == 0) {
			rows = atoi (token + 5);
		} else if (strncasecmp (token, "cols=", 5) == 0) {
			cols = atoi (token + 5);
		}
	}
	
	e->formTextArea = HTML_TEXTAREA (html_textarea_new (GTK_WIDGET(e->widget), name, rows, cols));
	html_form_add_element (e->form, HTML_EMBEDDED ( e->formTextArea ));
	
	append_element (e, clue, HTML_OBJECT (e->formTextArea));
	
	g_string_assign (e->formText, "");
	e->inTextArea = TRUE;
	
	g_free(name);
	push_block (e, ID_TEXTAREA, 3, block_end_textarea, 0, 0);
}

/* inline elements */
static void
element_parse_big (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	push_element (e, ID_BIG, NULL, html_style_set_font_size (NULL, GTK_HTML_FONT_STYLE_SIZE_4));
}

static void
element_parse_cite (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	push_element (e, ID_CITE, NULL, 
		      html_style_set_decoration (NULL, GTK_HTML_FONT_STYLE_ITALIC | GTK_HTML_FONT_STYLE_BOLD));
}

static void
element_parse_code (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	push_element (e, ID_CODE, NULL, html_style_set_decoration (NULL, GTK_HTML_FONT_STYLE_FIXED));
}

static void
element_parse_em (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	push_element (e, ID_EM, NULL, html_style_set_decoration (NULL, GTK_HTML_FONT_STYLE_ITALIC));
}

static void
element_parse_kbd (HTMLEngine *e, HTMLObject *_clue, const gchar *str)
{
	push_element (e, ID_KBD, NULL, html_style_set_decoration (NULL, GTK_HTML_FONT_STYLE_FIXED));
}

static void
element_parse_small (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	push_element (e, ID_SMALL, NULL, html_style_set_font_size (NULL, GTK_HTML_FONT_STYLE_SIZE_2));
}

static void
element_parse_strong (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	push_element (e, ID_STRONG, NULL, html_style_set_decoration (NULL, GTK_HTML_FONT_STYLE_BOLD));
}

static void
element_parse_sub (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	push_element (e, ID_SUB, NULL, html_style_set_decoration (NULL, GTK_HTML_FONT_STYLE_SUBSCRIPT));
}

static void
element_parse_sup (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	push_element (e, ID_SUP, NULL, html_style_set_decoration (NULL, GTK_HTML_FONT_STYLE_SUPERSCRIPT));
}

static void
element_parse_strike (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	push_element (e, ID_STRIKE, NULL, html_style_set_decoration (NULL, GTK_HTML_FONT_STYLE_STRIKEOUT));
}

static void
element_parse_s (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	push_element (e, ID_S, NULL, html_style_set_decoration (NULL, GTK_HTML_FONT_STYLE_STRIKEOUT));
}

static void
element_parse_tt (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	push_element (e, ID_TT, NULL, html_style_set_decoration (NULL, GTK_HTML_FONT_STYLE_FIXED));
}

static void
element_parse_u (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	push_element (e, ID_U, NULL, html_style_set_decoration (NULL, GTK_HTML_FONT_STYLE_UNDERLINE));
}

static void
element_parse_var (HTMLEngine *e, HTMLObject * _clue, const char *str )
{
	push_element (e, ID_VAR, NULL, html_style_set_decoration (NULL, GTK_HTML_FONT_STYLE_FIXED));
}

static void
element_parse_b (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	HTMLStyle *style = NULL;
	style = html_style_set_decoration (style, GTK_HTML_FONT_STYLE_BOLD);
	
	html_string_tokenizer_tokenize (e->st, str + 1, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		gchar *token = html_string_tokenizer_next_token (e->st);
		
		if (strncasecmp (token, "style=", 6) == 0) {
			html_style_add_attribute (style, token + 6);
		}
	}
	push_element (e, ID_B, NULL, style);
}

static void
element_parse_span (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	HTMLStyle *style = NULL;
	char *class = NULL;
	
	html_string_tokenizer_tokenize (e->st, str + 4, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		gchar *token = html_string_tokenizer_next_token (e->st);
		
		if (strncasecmp (token, "style=", 6 ) == 0 ) {
			style = html_style_add_attribute (style, token + 6);
		} else if (strncasecmp (token, "class=", 6 ) == 0) {
			class = g_strdup (token + 6);
		}
	}
	
	push_element (e, ID_SPAN, class, style);
	g_free (class);
}

static void
element_parse_font (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	GdkColor *color;
	HTMLColor *html_color = NULL;
	const HTMLFontFace *face = NULL;
	HTMLStyle *style = NULL;
	
	/* The GdkColor API is not const safe!  */
	color = gdk_color_copy ((GdkColor *) current_color (e));
	
	html_string_tokenizer_tokenize (e->st, str + 5, " >");
	while (html_string_tokenizer_has_more_tokens (e->st)) {
		const gchar *token = html_string_tokenizer_next_token (e->st);
		if (strncasecmp (token, "size=", 5) == 0) {
			gint size = atoi (token + 5);
			
			/* FIXME implement basefont */
			if (*(token + 5) == '+' || *(token + 5) == '-')
				size += GTK_HTML_FONT_STYLE_SIZE_3;
			
			CLAMP (size, GTK_HTML_FONT_STYLE_SIZE_1, GTK_HTML_FONT_STYLE_SIZE_MAX);
			
				style = html_style_set_font_size (style, size);
		} else if (strncasecmp (token, "face=", 5) == 0) {
			style = html_style_add_font_face (style, token + 5);
		} else if (strncasecmp (token, "color=", 6) == 0) {
			if (parse_color (token + 6, color)) {
				html_color = html_color_new_from_gdk_color (color);
				style = html_style_add_color (style, html_color);
				html_color_unref (html_color);
			}
		}
	}
	
	push_element (e, ID_FONT, NULL, style);
}

#ifdef TESTING		
static void
block_end_test (HTMLEngine *e, HTMLObject *clue, HTMLElement *elem)
{
	pop_clue (e); 
}

static void
element_parse_test (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	if (strncmp (str, "test", 4) == 0) {
		HTMLTableCell *cell;
		GdkColor color;

		close_flow (e, clue);

		cell = html_table_cell_new (1, 1, 20);
		if (parse_color ("red", &color)) {
			html_object_set_bg_color (HTML_OBJECT (cell), &color);
		}

		append_element (e, clue, HTML_OBJECT (cell));
		push_clue (e, HTML_OBJECT (cell));
		push_block (e, ID_TEST, 3, block_end_test, 0, 0);
	}
}
#endif /* TESTING */		



/* Parsing dispatch table.  */
typedef void (*HTMLParseFunc)(HTMLEngine *p, HTMLObject *clue, const gchar *str);
typedef struct _HTMLDispatchEntry {
	char *name;
	HTMLParseFunc func;
} HTMLDispatchEntry;

HTMLDispatchEntry basic_table[] = {
	{ID_A,                element_parse_a},
	{"area",              element_parse_area},
	{ID_ADDRESS,          element_parse_address},
	{ID_B,                element_parse_b},
	{"base",              element_parse_base},
	{ID_BIG,              element_parse_big},
	{ID_BLOCKQUOTE,       element_parse_blockquote},
	{ID_BODY,             element_parse_body},
	{ID_CAPTION,          element_parse_caption},
	{ID_CENTER,           element_parse_center},
	{ID_CITE,             element_parse_cite},
	{ID_CODE,             element_parse_code},
	{ID_DIR,              element_parse_dir},
	{ID_DIV,              element_parse_div},
	{"data",              element_parse_data},
	{ID_DL,               element_parse_dl},
	{ID_DT,               element_parse_dt},
	{ID_DD,               element_parse_dd},
	{ID_LI,               element_parse_li},
	{ID_EM,               element_parse_em},
	{ID_FONT,             element_parse_font},
	{ID_FORM,             element_parse_form},
	{"frameset",          element_parse_frameset},
	{"frame",             element_parse_frame},
	{ID_MAP,              element_parse_map},
	{"meta",              element_parse_meta},
	{"noframe",           element_parse_noframe},
	{ID_I,                element_parse_i},
	{"img",               element_parse_img},
	{"input",             element_parse_input},
	{"iframe",            element_parse_iframe},
	{ID_KBD,              element_parse_kbd},
	{ID_OL,               element_parse_ol},
	{ID_OPTION,           element_parse_option},	
	{"object",            parse_object},
	{"param",             element_parse_param},
	{ID_PRE,              element_parse_pre},
	{ID_SMALL,            element_parse_small},
	{ID_SPAN,             element_parse_span},
	{ID_STRONG,           element_parse_strong},
	{ID_SELECT,           element_parse_select},
	{ID_S,                element_parse_s},
	{ID_SUB,              element_parse_sup},
	{ID_SUP,              element_parse_sup},
	{ID_STRIKE,           element_parse_strike},
	{ID_U,                element_parse_u},
	{ID_UL,               element_parse_ul},
	{ID_TEXTAREA,         element_parse_textarea},
	{ID_TABLE,            element_parse_table},
	{ID_TD,               element_parse_td},
	{ID_TH,               element_parse_th},
	{ID_TR,               element_parse_tr},
	{ID_TT,               element_parse_tt},
	{"title",             element_parse_title},
	{ID_VAR,              element_parse_var},
#ifdef TESTING
	{ID_TEST,             element_parse_test},
#endif
	/* 
	 * the following elements have special behaviors for the close tags
	 * so we dispatch on the close element as well
	 */
	{"hr",                element_parse_hr}, 
	{"h1",                element_parse_heading},
	{"h2",                element_parse_heading},
	{"h3",                element_parse_heading},
	{"h4",                element_parse_heading},
	{"h5",                element_parse_heading},
	{"h6",                element_parse_heading},
	/* a /h1 after an h2 will close the h1 so we special case */
	{"/h1",               element_end_heading},
	{"/h2",               element_end_heading},
	{"/h3",               element_end_heading},
	{"/h4",               element_end_heading},
	{"/h5",               element_end_heading},
	{"/h6",               element_end_heading},
	/* p and br check the close marker themselves */
	{"p",                 element_parse_p},
	{"/p",                element_parse_p},
	{"br",                element_parse_br},
	{"/br",               element_parse_br},
	{NULL,                NULL}
};

static GHashTable *
dispatch_table_new (HTMLDispatchEntry *entry)
{
	GHashTable *table = g_hash_table_new (g_str_hash, g_str_equal);
	gint i = 0;

	while (entry[i].name) {
		g_hash_table_insert (table, entry[i].name, &entry[i]);
		i++;
	}
	
	return table;
}

static void
parse_one_token (HTMLEngine *e, HTMLObject *clue, const gchar *str)
{
	static GHashTable *basic = NULL;
	char *name, *ep;
	HTMLDispatchEntry *entry;

	if (basic == NULL)
		basic = dispatch_table_new (basic_table);

	if (*str == '<') {
		str++;
	} else {
		/* bad element */
		g_warning ("found token with no open");
		return;
	}
	
	name = parse_element_name (str);

	if (!name)
		return;

	entry = g_hash_table_lookup (basic, name);

	if (entry) {
		/* found a custom handler use it */
		DT (printf ("found handler for <%s>\n", name);)
		(*entry->func)(e, clue, str);
	} else if (*name == '/') {
		/* generic close element */
		DT (printf ("generic close handler for <%s>\n", name);)
		pop_element (e, name + 1);
	} else {
		/* unknown open element do nothing for now */
		printf ("generic open handler for <%s>\n", name);
	}

	g_free (name);
}


guint
html_engine_get_type (void)
{
	static GType html_engine_type = 0;

	if (html_engine_type == 0) {
		static const GTypeInfo html_engine_info = {
			sizeof (HTMLEngineClass),
			NULL,
			NULL,
			(GClassInitFunc) html_engine_class_init,
			NULL,
			NULL,
			sizeof (HTMLEngine),
			1,
			(GInstanceInitFunc) html_engine_init,
		};
		html_engine_type = g_type_register_static (G_TYPE_OBJECT, "HTMLEngine", &html_engine_info, 0);
	}

	return html_engine_type;
}

static void
clear_selection (HTMLEngine *e)
{
	if (e->selection) {
		html_interval_destroy (e->selection);
		e->selection = NULL;
	}
}

static void
html_engine_finalize (GObject *object)
{
	HTMLEngine *engine;
	GList *p;

	engine = HTML_ENGINE (object);

        /* it is critical to destroy timers immediately so that
	 * if widgets contained in the object tree manage to iterate the 
	 * mainloop we don't reenter in an inconsistant state.
	 */
	if (engine->timerId != 0) {
		gtk_idle_remove (engine->timerId);
		engine->timerId = 0;
	}
	if (engine->updateTimer != 0) {
		gtk_idle_remove (engine->updateTimer);
		engine->updateTimer = 0;
	}
	if (engine->thaw_idle_id != 0) {
		gtk_idle_remove (engine->thaw_idle_id);
		engine->thaw_idle_id = 0;
	}
	if (engine->blinking_timer_id != 0) {
		gtk_timeout_remove (engine->blinking_timer_id);
		engine->blinking_timer_id = 0;
	}
	if (engine->redraw_idle_id != 0) {
		gtk_timeout_remove (engine->redraw_idle_id);
		engine->redraw_idle_id = 0;
	}

	/* remove all the timers associated with image pointers also */
	if (engine->image_factory) {
		html_image_factory_stop_animations (engine->image_factory);
	}

	/* timers live in the selection updater too. */
	if (engine->selection_updater) {
		html_engine_edit_selection_updater_destroy (engine->selection_updater);
		engine->selection_updater = NULL;
	}
	
	if (engine->undo) {
		html_undo_destroy (engine->undo);
		engine->undo = NULL;
	}
	html_engine_clipboard_clear (engine);

	if (engine->invert_gc != NULL) {
		g_object_unref (engine->invert_gc);
		engine->invert_gc = NULL;
	}

	if (engine->cursor) {
		html_cursor_destroy (engine->cursor);
		engine->cursor = NULL;
	}
	if (engine->mark) {
		html_cursor_destroy (engine->mark);
		engine->mark = NULL;
	}

	if (engine->ht) {
		html_tokenizer_destroy (engine->ht);
		engine->ht = NULL;
	}

	if (engine->st) {
		html_string_tokenizer_destroy (engine->st);
		engine->st = NULL;
	}

	if (engine->settings) {
		html_settings_destroy (engine->settings);
		engine->settings = NULL;
	}

	if (engine->defaultSettings) {
		html_settings_destroy (engine->defaultSettings);
		engine->defaultSettings = NULL;
	}

	if (engine->insertion_color) {
		html_color_unref (engine->insertion_color);
		engine->insertion_color = NULL;
	}

	if (engine->clue != NULL) {
		HTMLObject *clue = engine->clue;
		
		/* extra safety in reentrant situations
		 * remove the clue before we destroy it
		 */
		engine->clue = NULL;
		html_object_destroy (clue);
	}

	if (engine->bgPixmapPtr) {
		html_image_factory_unregister (engine->image_factory, engine->bgPixmapPtr, NULL);
		engine->bgPixmapPtr = NULL;
	}

	if (engine->image_factory) {
		html_image_factory_free (engine->image_factory);
		engine->image_factory = NULL;
	}

	if (engine->painter) {
		g_object_unref (G_OBJECT (engine->painter));
		engine->painter = NULL;
	}

	if (engine->body_stack) {
		while (!html_stack_is_empty (engine->body_stack))
			pop_clue (engine);

		html_stack_destroy (engine->body_stack);
		engine->body_stack = NULL;
	}

	if (engine->span_stack) {
		html_stack_destroy (engine->span_stack);
		engine->span_stack = NULL;
	}

	if (engine->clueflow_style_stack) {
		html_stack_destroy (engine->clueflow_style_stack);
		engine->clueflow_style_stack = NULL;
	}

	if (engine->frame_stack) {
		html_stack_destroy (engine->frame_stack);
		engine->frame_stack = NULL;
	}
	
	if (engine->table_stack) {
		html_stack_destroy (engine->table_stack);
		engine->table_stack = NULL;
	}

	if (engine->listStack) {
		html_stack_destroy (engine->listStack);
		engine->listStack = NULL;
	}

	if (engine->embeddedStack) {
		html_stack_destroy (engine->embeddedStack);
		engine->embeddedStack = NULL;
	}

	if (engine->tempStrings) {
		for (p = engine->tempStrings; p != NULL; p = p->next)
			g_free (p->data);
		g_list_free (engine->tempStrings);
		engine->tempStrings = NULL;
	}

	if (engine->draw_queue) {
		html_draw_queue_destroy (engine->draw_queue);
		engine->draw_queue = NULL;
	}

	if (engine->search_info) {
		html_search_destroy (engine->search_info);
		engine->search_info = NULL;
	}

	clear_selection (engine);
	html_engine_map_table_clear (engine);
	html_engine_id_table_clear (engine);
	html_engine_clear_all_class_data (engine);

	if (engine->insertion_url) {
		g_free (engine->insertion_url);
		engine->insertion_url = NULL;
	}

	if (engine->insertion_target) {
		g_free (engine->insertion_target);
		engine->insertion_target = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
html_engine_set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec)
{
	HTMLEngine *engine = HTML_ENGINE (object);

	if (id == 1) {
		GtkHTMLClassProperties *prop;

		engine->widget          = GTK_HTML (g_value_get_object (value));
		engine->painter         = html_gdk_painter_new (GTK_WIDGET (engine->widget), TRUE);
		engine->settings        = html_settings_new (GTK_WIDGET (engine->widget));
		engine->defaultSettings = html_settings_new (GTK_WIDGET (engine->widget));
		html_colorset_add_slave (engine->settings->color_set, engine->painter->color_set);

		engine->insertion_color = html_colorset_get_color (engine->settings->color_set, HTMLTextColor);
		html_color_ref (engine->insertion_color);

		prop = GTK_HTML_CLASS (GTK_WIDGET_GET_CLASS (engine->widget))->properties;
	}
}

static void
html_engine_class_init (HTMLEngineClass *klass)
{
	GObjectClass *object_class;
	GParamSpec *pspec;

	object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (G_TYPE_OBJECT);

	signals [SET_BASE] =
		g_signal_new ("set_base",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (HTMLEngineClass, set_base),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);

	signals [SET_BASE_TARGET] =
		g_signal_new ("set_base_target",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (HTMLEngineClass, set_base_target),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);

	signals [LOAD_DONE] = 
		g_signal_new ("load_done",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (HTMLEngineClass, load_done),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals [TITLE_CHANGED] = 
		g_signal_new ("title_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (HTMLEngineClass, title_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals [URL_REQUESTED] =
		g_signal_new ("url_requested",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (HTMLEngineClass, url_requested),
			      NULL, NULL,
			      html_g_cclosure_marshal_VOID__STRING_POINTER,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_POINTER);

	signals [DRAW_PENDING] =
		g_signal_new ("draw_pending",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (HTMLEngineClass, draw_pending),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals [REDIRECT] =
		g_signal_new ("redirect",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (HTMLEngineClass, redirect),
			      NULL, NULL,
			      html_g_cclosure_marshal_VOID__POINTER_INT,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_INT);

	signals [SUBMIT] =
		g_signal_new ("submit",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (HTMLEngineClass, submit),
			      NULL, NULL,
			      html_g_cclosure_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE, 3,
			      G_TYPE_STRING,
			      G_TYPE_STRING,
			      G_TYPE_STRING);

	signals [OBJECT_REQUESTED] =
		g_signal_new ("object_requested",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (HTMLEngineClass, object_requested),
			      NULL, NULL,
			      html_g_cclosure_marshal_BOOL__OBJECT,
			      G_TYPE_BOOLEAN, 1,
			      G_TYPE_OBJECT);

	object_class->finalize = html_engine_finalize;
	object_class->set_property = html_engine_set_property;

	pspec = g_param_spec_object ("html", NULL, NULL, GTK_TYPE_HTML, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property (object_class, 1, pspec);

	html_engine_init_magic_links ();

	/* Initialize the HTML objects.  */
	html_types_init ();
}

static void
html_engine_init (HTMLEngine *engine)
{
	engine->clue = NULL;

	/* STUFF might be missing here!   */
	engine->freeze_count = 0;
	engine->thaw_idle_id = 0;
	engine->pending_expose = NULL;

	engine->window = NULL;
	engine->invert_gc = NULL;

	/* settings, colors and painter init */

	engine->newPage = FALSE;
	engine->allow_frameset = FALSE;

	engine->editable = FALSE;
	engine->caret_mode = FALSE;
	engine->clipboard = NULL;
	engine->clipboard_stack = NULL;
	engine->selection_stack  = NULL;

	engine->ht = html_tokenizer_new ();
	engine->st = html_string_tokenizer_new ();
	engine->image_factory = html_image_factory_new(engine);

	engine->undo = html_undo_new ();

	engine->body_stack = html_stack_new (NULL);
	engine->span_stack = html_stack_new (free_element);
	engine->clueflow_style_stack = html_stack_new (NULL);
	engine->frame_stack = html_stack_new (NULL);
	engine->table_stack = html_stack_new (NULL);

	engine->listStack = html_stack_new ((HTMLStackFreeFunc) html_list_destroy);
	/* FIXME rodo engine->embeddedStack = html_stack_new ((HTMLStackFreeFunc) gtk_object_unref); */
	engine->embeddedStack = html_stack_new (NULL);

	engine->url = NULL;
	engine->target = NULL;

	engine->leftBorder = LEFT_BORDER;
	engine->rightBorder = RIGHT_BORDER;
	engine->topBorder = TOP_BORDER;
	engine->bottomBorder = BOTTOM_BORDER;

	engine->inPre = FALSE;
	engine->inTitle = FALSE;

	engine->tempStrings = NULL;

	engine->draw_queue = html_draw_queue_new (engine);

	engine->map = NULL;
	engine->formList = NULL;

	engine->avoid_para = FALSE;

	engine->have_focus = FALSE;

	engine->cursor = html_cursor_new ();
	engine->mark = NULL;
	engine->cursor_hide_count = 1;

	engine->timerId = 0;
	engine->updateTimer = 0;

	engine->blinking_timer_id = 0;
	engine->blinking_status = FALSE;

	engine->insertion_font_style = GTK_HTML_FONT_STYLE_DEFAULT;
	engine->insertion_url = NULL;
	engine->insertion_target = NULL;
	engine->selection = NULL;
	engine->shift_selection = FALSE;
	engine->selection_mode = FALSE;
	engine->block_selection = 0;
	engine->cursor_position_stack = NULL;

	engine->selection_updater = html_engine_edit_selection_updater_new (engine);

	engine->search_info = NULL;
	engine->need_spell_check = FALSE;

	html_engine_print_set_min_split_index (engine, .75);

	engine->block = FALSE;
	engine->save_data = FALSE;
	engine->saved_step_count = -1;

	engine->map_table = NULL;

	engine->expose = FALSE;
	engine->need_update = FALSE;

	engine->language = NULL;
}

HTMLEngine *
html_engine_new (GtkWidget *w)
{
	HTMLEngine *engine;

	engine = g_object_new (HTML_TYPE_ENGINE, "html", w, NULL);

	return engine;
}

void
html_engine_realize (HTMLEngine *e,
		     GdkWindow *window)
{
	GdkGCValues gc_values;

	g_return_if_fail (e != NULL);
	g_return_if_fail (window != NULL);
	g_return_if_fail (e->window == NULL);

	e->window = window;

	html_gdk_painter_realize (HTML_GDK_PAINTER (e->painter), window);

	gc_values.function = GDK_INVERT;
	e->invert_gc = gdk_gc_new_with_values (e->window, &gc_values, GDK_GC_FUNCTION);

	if (e->need_update)
		html_engine_schedule_update (e);
}

void
html_engine_unrealize (HTMLEngine *e)
{
	html_gdk_painter_unrealize (HTML_GDK_PAINTER (e->painter));

	e->window = NULL;
}


/* This function makes sure @engine can be edited properly.  In order
   to be editable, the beginning of the document must have the
   following structure:
   
     HTMLClueV (cluev)
       HTMLClueFlow (head)
 	 HTMLObject (child) */
void
html_engine_ensure_editable (HTMLEngine *engine)
{
	HTMLObject *cluev;
	HTMLObject *head;
	HTMLObject *child;
	g_return_if_fail (engine != NULL);
	g_return_if_fail (HTML_IS_ENGINE (engine));

	cluev = engine->clue;
	if (cluev == NULL)
		engine->clue = cluev = html_cluev_new (0, 0, 100);

	head = HTML_CLUE (cluev)->head;
	if (head == NULL || HTML_OBJECT_TYPE (head) != HTML_TYPE_CLUEFLOW) {
		HTMLObject *clueflow;

		clueflow = flow_new (engine, HTML_CLUEFLOW_STYLE_NORMAL, HTML_LIST_TYPE_BLOCKQUOTE, 0, HTML_CLEAR_NONE);
		html_clue_prepend (HTML_CLUE (cluev), clueflow);

		head = clueflow;
	}

	child = HTML_CLUE (head)->head;
	if (child == NULL) {
		HTMLObject *text;

		text = text_new (engine, "", engine->insertion_font_style, engine->insertion_color);
		html_text_set_font_face (HTML_TEXT (text), current_font_face (engine));
		html_clue_prepend (HTML_CLUE (head), text);
	}
}


void
html_engine_draw_background (HTMLEngine *e,
			     gint x, gint y, gint w, gint h)
{
	HTMLImagePointer *bgpixmap;
	GdkPixbuf *pixbuf = NULL;

	/* return if no background pixmap is set */
	bgpixmap = e->bgPixmapPtr;
	if (bgpixmap && bgpixmap->animation) {
		pixbuf = gdk_pixbuf_animation_get_static_image (bgpixmap->animation);
	}

	html_painter_draw_background (e->painter, 
				      &html_colorset_get_color_allocated (e->painter, HTMLBgColor)->color,
				      pixbuf, x, y, w, h, x, y);
}

void
html_engine_stop_parser (HTMLEngine *e)
{
	if (!e->parsing)
		return;

	if (e->timerId != 0) {
		gtk_idle_remove (e->timerId);
		e->timerId = 0;
		while (html_engine_timer_event (e))
			;
	}
	
	e->parsing = FALSE;

	pop_element (e, ID_DOCUMENT);

	html_stack_clear (e->span_stack);
	html_stack_clear (e->clueflow_style_stack);
	html_stack_clear (e->frame_stack);
	html_stack_clear (e->table_stack);

	html_stack_clear (e->listStack);
}

/* used for cleaning up the id hash table */
static gboolean
id_table_free_func (gpointer key, gpointer val, gpointer data)
{
	g_free (key);
	return TRUE;
}

static void 
html_engine_id_table_clear (HTMLEngine *e)
{
	if (e->id_table) {
		g_hash_table_foreach_remove (e->id_table, id_table_free_func, NULL);
		g_hash_table_destroy (e->id_table);
		e->id_table = NULL;
	}
}

static gboolean
class_data_free_func (gpointer key, gpointer val, gpointer data)
{
	g_free (key);
	g_free (val);

	return TRUE;
}

static gboolean
class_data_table_free_func (gpointer key, gpointer val, gpointer data)
{
	GHashTable *t;

	t = (GHashTable *) val;
	g_hash_table_foreach_remove (t, class_data_free_func, NULL);
	g_hash_table_destroy (t);

	g_free (key);

	return TRUE;
}

static void 
html_engine_class_data_clear (HTMLEngine *e)
{
	if (e->class_data) {
		g_hash_table_foreach_remove (e->class_data, class_data_table_free_func, NULL);
		g_hash_table_destroy (e->class_data);
		e->class_data = NULL;
	}
}

/* #define LOG_INPUT */

GtkHTMLStream *
html_engine_begin (HTMLEngine *e, char *content_type)
{
	GtkHTMLStream *new_stream;

	html_engine_clear_all_class_data (e);
	html_tokenizer_begin (e->ht, content_type);
	
	html_engine_stop_parser (e);
	e->writing = TRUE;
	e->begin = TRUE;
	html_engine_set_focus_object (e, NULL, 0);

	html_engine_id_table_clear (e);
	html_engine_class_data_clear (e);
	html_engine_map_table_clear (e);
	html_image_factory_stop_animations (e->image_factory);

	new_stream = gtk_html_stream_new (GTK_HTML (e->widget),
					  html_engine_stream_types,
					  html_engine_stream_write,
					  html_engine_stream_end,
					  e);
#ifdef LOG_INPUT
	new_stream = gtk_html_stream_log_new (GTK_HTML (e->widget), new_stream);
#endif
	e->opened_streams = 1;
	
	e->newPage = TRUE;
	clear_selection (e);

	html_engine_thaw_idle_flush (e);

	g_slist_free (e->cursor_position_stack);
	e->cursor_position_stack = NULL;

	push_block_element (e, ID_DOCUMENT, NULL, 10, NULL, 0, 0);

	return new_stream;
}

char *engine_content_types[]= {"text/html", NULL};

static char **
html_engine_stream_types (GtkHTMLStream *handle,
			  gpointer data)
{
	return engine_content_types;
}

static void
html_engine_stream_write (GtkHTMLStream *handle,
			  const gchar *buffer,
			  size_t size,
			  gpointer data)
{
	HTMLEngine *e;

	e = HTML_ENGINE (data);

	if (buffer == NULL)
		return;

	html_tokenizer_write (e->ht, buffer, size == -1 ? strlen (buffer) : size);

	if (e->parsing && e->timerId == 0) {
		e->timerId = gtk_idle_add ((GtkFunction) html_engine_timer_event, e);
	}
}

static void
update_embedded (GtkWidget *widget, gpointer data)
{
	HTMLObject *obj;

	/* FIXME: this is a hack to update all the embedded widgets when
	 * they get moved off screen it isn't gracefull, but it should be a effective
	 * it also duplicates the draw_obj function in the drawqueue function very closely
	 * the common code in these functions should be merged and removed, but until then
	 * enjoy having your objects out of the way :)
	 */
	
	obj = HTML_OBJECT (g_object_get_data (G_OBJECT (widget), "embeddedelement"));
	if (obj && html_object_is_embedded (obj)) {
		HTMLEmbedded *emb = HTML_EMBEDDED (obj);
		
		if (emb->widget) {
			gint x, y;

			html_object_engine_translation (obj, NULL, &x, &y);
			
			x += obj->x;
			y += obj->y - obj->ascent;

			if (!emb->widget->parent) {
				gtk_layout_put (GTK_LAYOUT (emb->parent), emb->widget, x, y);
			} else {
				gtk_layout_move (GTK_LAYOUT(emb->parent), emb->widget, x, y);
			}
		}
	}
}

static gboolean
html_engine_update_event (HTMLEngine *e)
{
	DI (printf ("html_engine_update_event idle %p\n", e);)

	e->updateTimer = 0;

	if (html_engine_get_editable (e))
		html_engine_hide_cursor (e);
	html_engine_calc_size (e, FALSE);

	if (GTK_LAYOUT (e->widget)->vadjustment == NULL
	    || ! html_gdk_painter_realized (HTML_GDK_PAINTER (e->painter))) {
		e->need_update = TRUE;
		return FALSE;
	}

	e->need_update = FALSE;
	DI (printf ("continue %p\n", e);)

	/* Adjust the scrollbars */
	gtk_html_private_calc_scrollbars (e->widget, NULL, NULL);

	/* Scroll page to the top on first display */
	if (e->newPage) {
		gtk_adjustment_set_value (GTK_LAYOUT (e->widget)->vadjustment, 0);
		e->newPage = FALSE;
		if (! e->parsing && e->editable)
			html_cursor_home (e->cursor, e);
	}

	/* Is y_offset too big? */
	if (html_engine_get_doc_height (e) - e->y_offset < e->height) {
		e->y_offset = html_engine_get_doc_height (e) - e->height;
		if (e->y_offset < 0)
			e->y_offset = 0;
	}
		
	/* Is x_offset too big? */
	if (html_engine_get_doc_width (e) - e->x_offset < e->width) {
		e->x_offset = html_engine_get_doc_width (e) - e->width;
		if (e->x_offset < 0)
			e->x_offset = 0;
	}
	
	gtk_adjustment_set_value (GTK_LAYOUT (e->widget)->vadjustment, e->y_offset);
	gtk_adjustment_set_value (GTK_LAYOUT (e->widget)->hadjustment, e->x_offset);

	html_image_factory_deactivate_animations (e->image_factory);
	gtk_container_forall (GTK_CONTAINER (e->widget), update_embedded, e->widget);
	html_engine_queue_redraw_all (e);

	if (html_engine_get_editable (e))
		html_engine_show_cursor (e);
	
	return FALSE;
}


void
html_engine_schedule_update (HTMLEngine *e)
{
	/* printf ("html_engine_schedule_update (may block)\n"); */
	if (e->block && e->opened_streams)
		return;
	/* printf ("html_engine_schedule_update\n"); */
	if (e->updateTimer == 0)
		e->updateTimer = gtk_idle_add ((GtkFunction) html_engine_update_event, e);
}


gboolean
html_engine_goto_anchor (HTMLEngine *e,
			 const gchar *anchor)
{
	GtkAdjustment *vadj;
	HTMLAnchor *a;
	gint x, y;

	g_return_val_if_fail (anchor != NULL, FALSE);

	if (!e->clue)
		return FALSE;

	x = y = 0;
	a = html_object_find_anchor (e->clue, anchor, &x, &y);

	if (a == NULL) {
		/* g_warning ("Anchor: \"%s\" not found", anchor); */
		return FALSE;
	}

	vadj = GTK_LAYOUT (e->widget)->vadjustment;

	if (y < vadj->upper - vadj->page_size)
		gtk_adjustment_set_value (vadj, y);
	else
		gtk_adjustment_set_value (vadj, vadj->upper - vadj->page_size);

	return TRUE;
}

#if 0
struct {
	HTMLEngine *e;
	HTMLObject *o;
} respon

static void 
find_engine (HTMLObject *o, HTMLEngine *e, HTMLEngine **parent_engine)
{
	
}

gchar *
html_engine_get_object_base (HTMLEngine *e, HTMLObject *o) 
{
	HTMLEngine *parent_engine = NULL;
	
	g_return_if_fail (e != NULL);
	g_return_if_fail (o != NULL);
	
	html_object_forall (o, e, find_engine, &parent_engine);

	
}
#endif 

static gboolean
html_engine_timer_event (HTMLEngine *e)
{
	static const gchar *end[] = { NULL };
	gint lastHeight;
	gboolean retval = TRUE;

	DI (printf ("html_engine_timer_event idle %p\n", e);)

	/* Has more tokens? */
	if (!html_tokenizer_has_more_tokens (e->ht) && e->writing) {
		retval = FALSE;
		goto out;
	}

	/* Getting height */
	lastHeight = html_engine_get_doc_height (e);

	e->parseCount = e->granularity;

	/* Parsing body height */
	new_parse_body (e, end);

	e->begin = FALSE;
	html_engine_schedule_update (e);

	if (!e->parsing)
		retval = FALSE;

 out:
	if (!retval) {
		if(e->updateTimer != 0) {
			gtk_idle_remove (e->updateTimer);
			html_engine_update_event (e);
		}
		
		e->timerId = 0;
	}

	return retval;
}

/* This makes sure that the last HTMLClueFlow is non-empty.  */
static void
fix_last_clueflow (HTMLEngine *engine)
{
	HTMLClue *clue;
	HTMLClue *last_clueflow;

	clue = HTML_CLUE (engine->clue);
	if (clue == NULL)
		return;

	last_clueflow = HTML_CLUE (clue->tail);
	if (last_clueflow == NULL)
		return;

	if (last_clueflow->tail != NULL)
		return;

	html_clue_remove (HTML_CLUE (clue), HTML_OBJECT (last_clueflow));
	engine->flow = NULL;
}

static void
html_engine_stream_end (GtkHTMLStream *stream,
			GtkHTMLStreamStatus status,
			gpointer data)
{
	HTMLEngine *e;

	e = HTML_ENGINE (data);

	e->writing = FALSE;

	html_tokenizer_end (e->ht);

	if (e->timerId != 0) {
		gtk_idle_remove (e->timerId);
		e->timerId = 0;
	}

	while (html_engine_timer_event (e))
		;

	if (e->opened_streams)
		e->opened_streams --;
	/* printf ("ENGINE(%p) opened streams: %d\n", e, e->opened_streams); */
	if (e->block && e->opened_streams == 0)
		html_engine_schedule_update (e);

	fix_last_clueflow (e);
	html_engine_class_data_clear (e);
	
	if (e->editable) {
		html_engine_ensure_editable (e);
		html_cursor_home (e->cursor, e);
		e->newPage = FALSE;
	}

	g_signal_emit (e, signals [LOAD_DONE], 0);
}

static void
html_engine_draw_real (HTMLEngine *e, gint x, gint y, gint width, gint height, gboolean expose)
{
	gint x1, x2, y1, y2;
	
	if (e->block && e->opened_streams)
		return;

	/* printf ("html_engine_draw_real\n"); */

	/* This case happens when the widget has not been shown yet.  */
	if (width == 0 || height == 0)
		return;

	/* don't draw in case we are longer than available space and scrollbar is going to be shown */
	if (e->clue && e->clue->ascent + e->clue->descent > e->height - (html_engine_get_top_border (e) + html_engine_get_bottom_border (e))) {
		if (GTK_WIDGET (e->widget)->parent) {
			if (GTK_IS_SCROLLED_WINDOW (GTK_WIDGET (e->widget)->parent)) {
				if (GTK_SCROLLED_WINDOW (GTK_WIDGET (e->widget)->parent)->vscrollbar
				    && !GTK_WIDGET_VISIBLE (GTK_SCROLLED_WINDOW (GTK_WIDGET (e->widget)->parent)->vscrollbar)
				    && GTK_SCROLLED_WINDOW (GTK_WIDGET (e->widget)->parent)->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
					return;
			} 
		}
	}

	/* don't draw in case we are shorter than available space and scrollbar is going to be hidden */
	if (e->clue && e->clue->ascent + e->clue->descent <= e->height - (html_engine_get_top_border (e) + html_engine_get_bottom_border (e))) {
		if (GTK_WIDGET (e->widget)->parent) {
			if (GTK_IS_SCROLLED_WINDOW (GTK_WIDGET (e->widget)->parent)) {
				if (GTK_SCROLLED_WINDOW (GTK_WIDGET (e->widget)->parent)->vscrollbar
				    && GTK_WIDGET_VISIBLE (GTK_SCROLLED_WINDOW (GTK_WIDGET (e->widget)->parent)->vscrollbar)
				    && GTK_SCROLLED_WINDOW (GTK_WIDGET (e->widget)->parent)->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
					return;
			} 
		}
	}

	/* printf ("html_engine_draw_real THRU\n"); */

	/* printf ("html_engine_draw_real %d x %d, %d\n",
	   e->width, e->height, e->clue ? e->clue->ascent + e->clue->descent : 0); */

	e->expose = expose;
	
	x1 = x;
	x2 = x + width;
	y1 = y;
	y2 = y + height;
	
	if (!html_engine_intersection (e, &x1, &y1, &x2, &y2))
		return;
	
	html_painter_begin (e->painter, x1, y1, x2, y2);
	
	html_engine_draw_background (e, x1, y1, x2 - x1, y2 - y1);
	
	if (e->clue) {
		e->clue->x = html_engine_get_left_border (e);
		e->clue->y = html_engine_get_top_border (e) + e->clue->ascent;
		html_object_draw (e->clue, e->painter, x1, y1, x2 - x1, y2 - y1, 0, 0);
	}
	html_painter_end (e->painter);
	
	if (e->editable)
		html_engine_draw_cursor_in_area (e, x1, y1, x2 - x1, y2 - y1);

	e->expose = FALSE;
}

void
html_engine_expose (HTMLEngine *e, GdkEventExpose *event)
{
	if (html_engine_frozen (e))
		html_engine_add_expose (e, event->area.x, event->area.y, event->area.width, event->area.height, TRUE);
	else
		html_engine_draw_real (e, event->area.x, event->area.y, event->area.width, event->area.height, TRUE);
}

void
html_engine_draw (HTMLEngine *e, gint x, gint y, gint width, gint height)
{
	if (html_engine_frozen (e))
		html_engine_add_expose (e, x, y, width, height, FALSE);
	else
		html_engine_draw_real (e, x, y, width, height, FALSE);
}

static gint
redraw_idle (HTMLEngine *e)
{
       e->redraw_idle_id = 0;
       e->need_redraw = FALSE;
       html_engine_queue_redraw_all (e);

       return FALSE;
}

void
html_engine_schedule_redraw (HTMLEngine *e)
{
	/* printf ("html_engine_schedule_redraw\n"); */

	if (e->block_redraw)
		e->need_redraw = TRUE;
	else if (e->redraw_idle_id == 0) {
		clear_pending_expose (e);
		html_draw_queue_clear (e->draw_queue);
		e->redraw_idle_id = gtk_idle_add ((GtkFunction) redraw_idle, e);
	}
}

void
html_engine_block_redraw (HTMLEngine *e)
{
	e->block_redraw ++;
	if (e->redraw_idle_id) {
		gtk_idle_remove (e->redraw_idle_id);
		e->redraw_idle_id = 0;
		e->need_redraw = TRUE;
	}
}


void
html_engine_unblock_redraw (HTMLEngine *e)
{
	g_assert (e->block_redraw > 0);

	e->block_redraw --;
	if (!e->block_redraw && e->need_redraw) {
		if (e->redraw_idle_id) {
			gtk_idle_remove (e->redraw_idle_id);
			e->redraw_idle_id = 0;
		}
		redraw_idle (e);
	}
}


gint
html_engine_get_doc_width (HTMLEngine *e)
{
	return (e->clue ? e->clue->width : 0) + html_engine_get_left_border (e) + html_engine_get_right_border (e);
}

gint
html_engine_get_doc_height (HTMLEngine *e)
{
	gint height;

	if (e->clue) {
		height = e->clue->ascent;
		height += e->clue->descent;
		height += html_engine_get_top_border (e);
		height += html_engine_get_bottom_border (e);

		return height;
	}
	
	return 0;
}

gint
html_engine_calc_min_width (HTMLEngine *e)
{
	return html_object_calc_min_width (e->clue, e->painter)
		+ html_painter_get_pixel_size (e->painter) * (html_engine_get_left_border (e) + html_engine_get_right_border (e));
}

gint
html_engine_get_max_width (HTMLEngine *e)
{
	gint max_width;

	if (e->widget->iframe_parent)
		max_width = e->widget->frame->max_width
			- (html_engine_get_left_border (e) + html_engine_get_right_border (e)) * html_painter_get_pixel_size (e->painter);
	else
		max_width = html_painter_get_page_width (e->painter, e)
			- (html_engine_get_left_border (e) + html_engine_get_right_border (e)) * html_painter_get_pixel_size (e->painter);

	return MAX (0, max_width);
}

gint
html_engine_get_max_height (HTMLEngine *e)
{
	gint max_height;

	if (e->widget->iframe_parent)
		max_height = HTML_FRAME (e->widget->frame)->height
			- (html_engine_get_top_border (e) + html_engine_get_bottom_border (e)) * html_painter_get_pixel_size (e->painter);
	else
		max_height = html_painter_get_page_height (e->painter, e)
			- (html_engine_get_top_border (e) + html_engine_get_bottom_border (e)) * html_painter_get_pixel_size (e->painter);

	return MAX (0, max_height);
}

gboolean
html_engine_calc_size (HTMLEngine *e, GList **changed_objs)
{
	gint max_width; /* , max_height; */
	gboolean redraw_whole;

	if (e->clue == 0)
		return FALSE;

	html_object_reset (e->clue);

	max_width = MIN (html_engine_get_max_width (e),
			 html_painter_get_pixel_size (e->painter)
			 * (MAX_WIDGET_WIDTH - html_engine_get_left_border (e) - html_engine_get_right_border (e)));
	/* max_height = MIN (html_engine_get_max_height (e),
			 html_painter_get_pixel_size (e->painter)
			 * (MAX_WIDGET_WIDTH - e->topBorder - e->bottomBorder)); */

	redraw_whole = max_width != e->clue->max_width;
	html_object_set_max_width (e->clue, e->painter, max_width);
	/* html_object_set_max_height (e->clue, e->painter, max_height); */
	/* printf ("calc size %d\n", e->clue->max_width); */
	if (changed_objs)
		*changed_objs = NULL;
	html_object_calc_size (e->clue, e->painter, redraw_whole ? NULL : changed_objs);

	e->clue->x = html_engine_get_left_border (e);
	e->clue->y = e->clue->ascent + html_engine_get_top_border (e);

	return redraw_whole;
}

static void
destroy_form (gpointer data, gpointer user_data)
{
	html_form_destroy (HTML_FORM(data));
}

void
html_engine_parse (HTMLEngine *e)
{
	html_engine_stop_parser (e);

	/* reset search & replace */
	if (e->search_info) {
		html_search_destroy (e->search_info);
		e->search_info = NULL;
	}
	if (e->replace_info) {
		html_replace_destroy (e->replace_info);
		e->replace_info = NULL;
	}

	if (e->clue != NULL)
		html_object_destroy (e->clue);

	g_list_foreach (e->formList, destroy_form, NULL);

	g_list_free (e->formList);

	e->map = NULL;
	e->formList = NULL;
	e->form = NULL;
	e->formSelect = NULL;
	e->formTextArea = NULL;
	e->inOption = FALSE;
	e->inTextArea = FALSE;
	e->formText = g_string_new ("");

	e->flow = NULL;

	/* reset to default border size */
	e->leftBorder   = LEFT_BORDER;
	e->rightBorder  = RIGHT_BORDER;
	e->topBorder    = TOP_BORDER;
	e->bottomBorder = BOTTOM_BORDER;

	/* reset settings to default ones */
	html_colorset_set_by (e->settings->color_set, e->defaultSettings->color_set);

	e->clue = html_cluev_new (html_engine_get_left_border (e), html_engine_get_top_border (e), 100);
	HTML_CLUE (e->clue)->valign = HTML_VALIGN_TOP;
	HTML_CLUE (e->clue)->halign = HTML_HALIGN_LEFT;

	e->cursor->object = e->clue;

	/* Free the background pixmap */
	if (e->bgPixmapPtr) {
		html_image_factory_unregister(e->image_factory, e->bgPixmapPtr, NULL);
		e->bgPixmapPtr = NULL;
	}

	e->parsing = TRUE;
	e->avoid_para = FALSE;

	e->timerId = gtk_idle_add ((GtkFunction) html_engine_timer_event, e);
}


HTMLObject *
html_engine_get_object_at (HTMLEngine *e,
			   gint x, gint y,
			   guint *offset_return,
			   gboolean for_cursor)
{
	HTMLObject *clue;
	HTMLObject *obj;

	clue = HTML_OBJECT (e->clue);
	if (clue == NULL)
		return NULL;

	if (for_cursor) {
		gint width, height;

		width = clue->width;
		height = clue->ascent + clue->descent;

		if (width == 0 || height == 0)
			return NULL;

		if (x < html_engine_get_left_border (e))
			x = html_engine_get_left_border (e);
		else if (x >= html_engine_get_left_border (e) + width)
			x = html_engine_get_left_border (e) + width - 1;

		if (y < html_engine_get_top_border (e)) {
			x = html_engine_get_left_border (e);
			y = html_engine_get_top_border (e);
		} else if (y >= html_engine_get_top_border (e) + height) {
			x = html_engine_get_left_border (e) + width - 1;
			y = html_engine_get_top_border (e) + height - 1;
		}
	}

	obj = html_object_check_point (clue, e->painter, x, y, offset_return, for_cursor);

	return obj;
}

HTMLPoint *
html_engine_get_point_at (HTMLEngine *e,
			  gint x, gint y,
			  gboolean for_cursor)
{
	HTMLObject *o;
	guint off;

	o = html_engine_get_object_at (e, x, y, &off, for_cursor);

	return o ? html_point_new (o, off) : NULL;
}

const gchar *
html_engine_get_link_at (HTMLEngine *e, gint x, gint y)
{
	HTMLObject *obj;
	gint offset;

	if (e->clue == NULL)
		return NULL;

	obj = html_engine_get_object_at (e, x, y, &offset, FALSE);

	if (obj != NULL)
		return html_object_get_url (obj, offset);

	return NULL;
}


/**
 * html_engine_set_editable:
 * @e: An HTMLEngine object
 * @editable: A flag specifying whether the object must be editable
 * or not
 * 
 * Make @e editable or not, according to the value of @editable.
 **/
void
html_engine_set_editable (HTMLEngine *e,
			  gboolean editable)
{
	g_return_if_fail (e != NULL);
	g_return_if_fail (HTML_IS_ENGINE (e));

	if ((e->editable && editable) || (! e->editable && ! editable))
		return;

	if (editable)
		html_engine_spell_check (e);
	html_engine_disable_selection (e);

	html_engine_queue_redraw_all (e);

	e->editable = editable;

	if (editable) {
		html_engine_ensure_editable (e);
		html_cursor_home (e->cursor, e);
		e->newPage = FALSE;
		if (e->have_focus)
			html_engine_setup_blinking_cursor (e);
	} else {
		if (e->have_focus)
			if (e->caret_mode)
				html_engine_setup_blinking_cursor (e);
			else 
				html_engine_stop_blinking_cursor (e);
	}
}

gboolean
html_engine_get_editable (HTMLEngine *e)
{
	g_return_val_if_fail (e != NULL, FALSE);
	g_return_val_if_fail (HTML_IS_ENGINE (e), FALSE);

	if (e->editable && ! e->parsing && e->timerId == 0)
		return TRUE;
	else
		return FALSE;
}

static void
set_focus (HTMLObject *o, HTMLEngine *e, gpointer data)
{
	if (HTML_IS_IFRAME (o) || HTML_IS_FRAME (o)) {
		HTMLEngine *cur_e = GTK_HTML (HTML_IS_FRAME (o) ? HTML_FRAME (o)->html : HTML_IFRAME (o)->html)->engine;
		html_painter_set_focus (cur_e->painter, GPOINTER_TO_INT (data));
	}
}

void
html_engine_set_focus (HTMLEngine *engine,
		       gboolean have_focus)
{
	g_return_if_fail (engine != NULL);
	g_return_if_fail (HTML_IS_ENGINE (engine));

	if (engine->editable || engine->caret_mode) {
		if (! engine->have_focus && have_focus)
			html_engine_setup_blinking_cursor (engine);
		else if (engine->have_focus && ! have_focus)
			html_engine_stop_blinking_cursor (engine);
	}

	engine->have_focus = have_focus;

	html_painter_set_focus (engine->painter, engine->have_focus);
	if (engine->clue)
		html_object_forall (engine->clue, engine, set_focus, GINT_TO_POINTER (have_focus));
	html_engine_redraw_selection (engine);
}


/*
  FIXME: It might be nice if we didn't allow the tokenizer to be
  changed once tokenizing has begin.
*/
void
html_engine_set_tokenizer (HTMLEngine *engine,
			   HTMLTokenizer *tok)
{
	g_return_if_fail (engine && HTML_IS_ENGINE (engine));
	g_return_if_fail (tok && HTML_IS_TOKENIZER (tok));

	g_object_ref (G_OBJECT (tok));
	g_object_unref (G_OBJECT (engine->ht));
	engine->ht = tok;
}


gboolean
html_engine_make_cursor_visible (HTMLEngine *e)
{
	gint x1, y1, x2, y2, xo, yo;

	g_return_val_if_fail (e != NULL, FALSE);

	if (! e->editable && !e->caret_mode)
		return FALSE;

	if (e->cursor->object == NULL)
		return FALSE;

	html_object_get_cursor (e->cursor->object, e->painter, e->cursor->offset, &x1, &y1, &x2, &y2);

	xo = e->x_offset;
	yo = e->y_offset;

	if (x1 < e->x_offset)
		e->x_offset = x1 - html_engine_get_left_border (e);
	if (x1 > e->x_offset + e->width - html_engine_get_right_border (e))
		e->x_offset = x1 - e->width + html_engine_get_right_border (e);

	if (y1 < e->y_offset)
		e->y_offset = y1 - html_engine_get_top_border (e);
	if (y2 >= e->y_offset + e->height - html_engine_get_bottom_border (e))
		e->y_offset = y2 - e->height + html_engine_get_bottom_border (e) + 1;

	return xo != e->x_offset || yo != e->y_offset;
}


/* Draw queue handling.  */

void
html_engine_flush_draw_queue (HTMLEngine *e)
{
	g_return_if_fail (e != NULL);
	g_return_if_fail (HTML_IS_ENGINE (e));

	if (!html_engine_frozen (e)) {
		html_draw_queue_flush (e->draw_queue);
	}
}

void
html_engine_queue_draw (HTMLEngine *e, HTMLObject *o)
{
	g_return_if_fail (e != NULL);
	g_return_if_fail (HTML_IS_ENGINE (e));
	g_return_if_fail (o != NULL);

	html_draw_queue_add (e->draw_queue, o);
	/* printf ("html_draw_queue_add %p\n", o); */
}

void
html_engine_queue_clear (HTMLEngine *e,
			 gint x, gint y,
			 guint width, guint height)
{
	g_return_if_fail (e != NULL);

	/* if (e->freeze_count == 0) */
	html_draw_queue_add_clear (e->draw_queue, x, y, width, height,
				   &html_colorset_get_color_allocated (e->painter, HTMLBgColor)->color);
}


void
html_engine_form_submitted (HTMLEngine *e,
			    const gchar *method,
			    const gchar *action,
			    const gchar *encoding)
{
	g_signal_emit (e, signals [SUBMIT], 0, method, action, encoding);
}


/* Retrieving the selection as a string.  */

gchar *
html_engine_get_selection_string (HTMLEngine *engine)
{
	GString *buffer;
	gchar *string;

	g_return_val_if_fail (engine != NULL, NULL);
	g_return_val_if_fail (HTML_IS_ENGINE (engine), NULL);

	if (engine->clue == NULL)
		return NULL;

	buffer = g_string_new (NULL);
	html_object_append_selection_string (engine->clue, buffer);

	string = buffer->str;
	g_string_free (buffer, FALSE);

	return string;
}


/* Cursor normalization.  */

void
html_engine_normalize_cursor (HTMLEngine *engine)
{
	g_return_if_fail (engine != NULL);
	g_return_if_fail (HTML_IS_ENGINE (engine));

	html_cursor_normalize (engine->cursor);
	html_engine_edit_selection_updater_update_now (engine->selection_updater);
}


/* Freeze/thaw.  */

gboolean
html_engine_frozen (HTMLEngine *engine)
{
	g_return_val_if_fail (engine != NULL, FALSE);
	g_return_val_if_fail (HTML_IS_ENGINE (engine), FALSE);

	return engine->freeze_count > 0;
}

void
html_engine_freeze (HTMLEngine *engine)
{
	g_return_if_fail (engine != NULL);
	g_return_if_fail (HTML_IS_ENGINE (engine));

	if (engine->freeze_count == 0) {
		gtk_html_im_reset (engine->widget);
		html_engine_flush_draw_queue (engine);
		if ((HTML_IS_GDK_PAINTER (engine->painter) || HTML_IS_PLAIN_PAINTER (engine->painter)) && HTML_GDK_PAINTER (engine->painter)->window)
		gdk_window_process_updates (HTML_GDK_PAINTER (engine->painter)->window, FALSE);
	}


	html_engine_flush_draw_queue (engine);
	DF (printf ("html_engine_freeze %d\n", engine->freeze_count); fflush (stdout));

	html_engine_hide_cursor (engine);
	engine->freeze_count++;
}

static void
html_engine_get_viewport (HTMLEngine *e, GdkRectangle *viewport)
{
	viewport->x = e->x_offset;
	viewport->y = e->y_offset;
	viewport->width = e->width;
	viewport->height = e->height;
}

gboolean
html_engine_intersection (HTMLEngine *e, gint *x1, gint *y1, gint *x2, gint *y2)
{
	HTMLEngine *top = html_engine_get_top_html_engine (e);
	GdkRectangle draw;
	GdkRectangle clip;
	GdkRectangle paint;

	html_engine_get_viewport (e, &clip);

	draw.x = *x1;
	draw.y = *y1;
	draw.width = *x2 - *x1;
	draw.height = *y2 - *y1;

	if (!gdk_rectangle_intersect (&clip, &draw, &paint))
		return FALSE;

	if (e != top) {
		GdkRectangle top_clip;
		gint abs_x = 0, abs_y = 0;
				
		html_object_calc_abs_position (e->clue->parent, &abs_x, &abs_y);
		abs_y -= e->clue->parent->ascent;

		html_engine_get_viewport (top, &top_clip);
		top_clip.x -= abs_x;
		top_clip.y -= abs_y;

		if (!gdk_rectangle_intersect (&paint, &top_clip, &paint))
			return FALSE;
	}


	*x1 = paint.x;
	*x2 = paint.x + paint.width;
	*y1 = paint.y;
	*y2 = paint.y + paint.height;

	return TRUE;
}

static void
get_changed_objects (HTMLEngine *e, GdkRegion *region, GList *changed_objs)
{
	GList *cur;

	/* printf ("draw_changed_objects BEGIN\n"); */

	for (cur = changed_objs; cur; cur = cur->next) {
		if (cur->data) {
			HTMLObject *o;

			o = HTML_OBJECT (cur->data);
			html_engine_queue_draw (e, o);
		} else {
			cur = cur->next;
			if (e->window) {
				HTMLObjectClearRectangle *cr = (HTMLObjectClearRectangle *)cur->data;
				HTMLObject *o;
				GdkRectangle paint;
				gint tx, ty;
					
				o = cr->object;
				
				html_object_engine_translation (cr->object, e, &tx, &ty);
				
				paint.x = o->x + cr->x + tx;
				paint.y = o->y - o->ascent + cr->y + ty;
				paint.width = cr->width;
				paint.height = cr->height;
				
				gdk_region_union_with_rect (region, &paint);
			}
			g_free (cur->data);
		}
	}
	/* printf ("draw_changed_objects END\n"); */
}


struct HTMLEngineExpose {
	GdkRectangle area;
	gboolean expose;
};

static void
get_pending_expose (HTMLEngine *e, GdkRegion *region)
{
	GSList *l, *next;

	g_assert (!html_engine_frozen (e));
	/* printf ("do_pending_expose\n"); */

	for (l = e->pending_expose; l; l = next) {
		struct HTMLEngineExpose *r;

		next = l->next;
		r = (struct HTMLEngineExpose *) l->data;

		gdk_region_union_with_rect (region, &r->area);
		g_free (r);
	}
}

static void
free_expose_data (gpointer data, gpointer user_data)
{
	g_free (data);
}

static void
clear_pending_expose (HTMLEngine *e)
{
	g_slist_foreach (e->pending_expose, free_expose_data, NULL);
	g_slist_free (e->pending_expose);
	e->pending_expose = NULL;
}

#ifdef CHECK_CURSOR
static void
check_cursor (HTMLEngine *e)
{
	HTMLCursor *cursor;
	gboolean need_spell_check;

	cursor = html_cursor_dup (e->cursor);
	
	need_spell_check = e->need_spell_check;
	e->need_spell_check = FALSE;

	while (html_cursor_backward (cursor, e))
		;

	if (cursor->position != 0) {
		g_warning ("check cursor failed (%d)\n", cursor->position);
		gnome_ok_dialog ("Eeek, BAD cursor position!\n"
				 "\n"
				 "If you know how to get editor to this state,\n"
				 "please mail to gtkhtml-maintainers@ximian.com\n"
				 "detailed description\n"
				 "\n"
				 "Thank you");
		e->cursor->position -= cursor->position;
	}

	e->need_spell_check = need_spell_check;
	html_cursor_destroy (cursor);
}
#endif

static gint
thaw_idle (gpointer data)
{
	HTMLEngine *e = HTML_ENGINE (data);
	GList *changed_objs;
	gboolean redraw_whole;
	gint w, h;

	DF (printf ("thaw_idle %d\n", e->freeze_count); fflush (stdout));

#ifdef CHECK_CURSOR
	check_cursor (e);
#endif

	e->thaw_idle_id = 0;
	if (e->freeze_count != 1) {
		/* we have been frozen again meanwhile */
		DF (printf ("frozen again meanwhile\n"); fflush (stdout);)
		html_engine_show_cursor (e);
		e->freeze_count--;
		return FALSE;
	}

	w = html_engine_get_doc_width (e) - html_engine_get_right_border (e);
	h = html_engine_get_doc_height (e) - html_engine_get_bottom_border (e);

	redraw_whole = html_engine_calc_size (e, &changed_objs);

	gtk_html_private_calc_scrollbars (e->widget, NULL, NULL);
	gtk_html_edit_make_cursor_visible (e->widget);

	e->freeze_count--;

	if (redraw_whole) {
		html_engine_queue_redraw_all (e);
	} else {
		gint nw, nh;
		GdkRegion *region = gdk_region_new ();
		GdkRectangle paint;
		
		get_pending_expose (e, region);
		get_changed_objects (e, region, changed_objs);

		nw = html_engine_get_doc_width (e) - html_engine_get_right_border (e);
		nh = html_engine_get_doc_height (e) - html_engine_get_bottom_border (e);

		if (nh < h && nh - e->y_offset < e->height) {
			paint.x = e->x_offset;
			paint.y = nh;
			paint.width = e->width;
			paint.height = e->height + e->y_offset - nh;

			gdk_region_union_with_rect (region, &paint);
		}
		if (nw < w && nw - e->x_offset < e->width) {
			paint.x = nw;
			paint.y = e->y_offset;
			paint.width = e->width + e->x_offset - nw;

			gdk_region_union_with_rect (region, &paint);
		}
		g_list_free (changed_objs);
		gdk_window_invalidate_region (HTML_GDK_PAINTER (e->painter)->window, region, FALSE);
		gdk_region_destroy (region);
		html_engine_flush_draw_queue (e);
	}
	g_slist_free (e->pending_expose);
	e->pending_expose = NULL;

	html_engine_show_cursor (e);

	return FALSE;
}

void
html_engine_thaw (HTMLEngine *engine)
{
	g_return_if_fail (engine != NULL);
	g_return_if_fail (HTML_IS_ENGINE (engine));
	g_return_if_fail (engine->freeze_count > 0);

	if (engine->freeze_count == 1) {
		if (engine->thaw_idle_id == 0) {
			DF (printf ("queueing thaw_idle %d\n", engine->freeze_count);)
			engine->thaw_idle_id = gtk_idle_add (thaw_idle, engine);
		}
	} else {
		engine->freeze_count--;
		html_engine_show_cursor (engine);
	}

	DF (printf ("html_engine_thaw %d\n", engine->freeze_count);)
}

void
html_engine_thaw_idle_flush (HTMLEngine *e)
{
	DF (printf ("html_engine_thaw_idle_flush\n");fflush (stdout);)

	if (e->thaw_idle_id) {
		g_source_remove (e->thaw_idle_id);
		thaw_idle (e);
	}
}


/**
 * html_engine_load_empty:
 * @engine: An HTMLEngine object
 * 
 * Load an empty document into the engine.
 **/
void
html_engine_load_empty (HTMLEngine *engine)
{
	g_return_if_fail (engine != NULL);
	g_return_if_fail (HTML_IS_ENGINE (engine));

	/* FIXME: "slightly" hackish.  */
	html_engine_stop_parser (engine);
	html_engine_parse (engine);
	html_engine_stop_parser (engine);

	html_engine_ensure_editable (engine);
}

void
html_engine_replace (HTMLEngine *e, const gchar *text, const gchar *rep_text,
		     gboolean case_sensitive, gboolean forward, gboolean regular,
		     void (*ask)(HTMLEngine *, gpointer), gpointer ask_data)
{
	if (e->replace_info)
		html_replace_destroy (e->replace_info);
	e->replace_info = html_replace_new (rep_text, ask, ask_data);

	if (html_engine_search (e, text, case_sensitive, forward, regular))
		ask (e, ask_data);
}

static void
replace (HTMLEngine *e)
{
	HTMLObject *first = HTML_OBJECT (e->search_info->found->data);

	html_engine_edit_selection_updater_update_now (e->selection_updater);

	if (e->replace_info->text && *e->replace_info->text) {
		HTMLObject *new_text;

		new_text = text_new (e, e->replace_info->text,
				     HTML_TEXT (first)->font_style,
				     HTML_TEXT (first)->color);
		html_text_set_font_face (HTML_TEXT (new_text), HTML_TEXT (first)->face);
		html_engine_paste_object (e, new_text, html_object_get_length (HTML_OBJECT (new_text)));
	} else {
		html_engine_delete (e);
	}

	/* update search info to point just behind replaced text */
	g_list_free (e->search_info->found);
	e->search_info->found = g_list_append (NULL, e->cursor->object);
	e->search_info->start_pos = e->search_info->stop_pos = e->cursor->offset - 1;
	e->search_info->found_len = 0;
	html_search_pop  (e->search_info);
	html_search_push (e->search_info, e->cursor->object->parent);
}

void
html_engine_replace_do (HTMLEngine *e, HTMLReplaceQueryAnswer answer)
{
	g_assert (e->replace_info);

	switch (answer) {
	case RQA_ReplaceAll:
		html_undo_level_begin (e->undo, "Replace all", "Revert replace all");
		replace (e);
		while (html_engine_search_next (e))
			replace (e);
		html_undo_level_end (e->undo);
	case RQA_Cancel:
		html_replace_destroy (e->replace_info);
		e->replace_info = NULL;
		html_engine_disable_selection (e);
		break;

	case RQA_Replace:
		html_undo_level_begin (e->undo, "Replace", "Revert replace");
		replace (e);
		html_undo_level_end (e->undo);
	case RQA_Next:
		if (html_engine_search_next (e))
			e->replace_info->ask (e, e->replace_info->ask_data);
		else
			html_engine_disable_selection (e);
		break;
	}
}

/* spell checking */

static void
check_paragraph (HTMLObject *o, HTMLEngine *unused, HTMLEngine *e)
{
	if (HTML_OBJECT_TYPE (o) == HTML_TYPE_CLUEFLOW)
		html_clueflow_spell_check (HTML_CLUEFLOW (o), e, NULL);
}

void
html_engine_spell_check (HTMLEngine *e)
{
	g_assert (HTML_IS_ENGINE (e));
	g_assert (e->clue);

	e->need_spell_check = FALSE;

	if (e->widget->editor_api && e->widget->editor_api->check_word)
		html_object_forall (e->clue, NULL, (HTMLObjectForallFunc) check_paragraph, e);
}

static void
clear_spell_check (HTMLObject *o, HTMLEngine *unused, HTMLEngine *e)
{
	if (html_object_is_text (o))
		html_text_spell_errors_clear (HTML_TEXT (o));
}

void
html_engine_clear_spell_check (HTMLEngine *e)
{
	g_assert (HTML_IS_ENGINE (e));
	g_assert (e->clue);

	e->need_spell_check = FALSE;

	html_object_forall (e->clue, NULL, (HTMLObjectForallFunc) clear_spell_check, e);
	html_engine_draw (e, e->x_offset, e->y_offset, e->width, e->height);
}

gchar *
html_engine_get_spell_word (HTMLEngine *e)
{
	GString *text;
	HTMLCursor *cursor;
	gchar *word;
	gint pos;
	gunichar uc;
	gboolean cited, cited_tmp, cited2;

	cited = FALSE;
	if (!html_selection_spell_word (html_cursor_get_current_char (e->cursor), &cited) && !cited
	    && !html_selection_spell_word (html_cursor_get_prev_char (e->cursor), &cited) && !cited)
		return NULL;

	cursor = html_cursor_dup (e->cursor);
	pos    = cursor->position;
	text   = g_string_new (NULL);

	/* move to the beginning of word */
	cited = cited_tmp = FALSE;
	while (html_selection_spell_word (html_cursor_get_prev_char (cursor), &cited_tmp) || cited_tmp) {
		html_cursor_backward (cursor, e);
		if (cited_tmp)
			cited_tmp = TRUE;
		cited_tmp = FALSE;
	}

	/* move to the end of word */
	cited2 = FALSE;
	while (html_selection_spell_word (uc = html_cursor_get_current_char (cursor), &cited2) || (!cited && cited2)) {
		gchar out [7];
		gint size;

		size = g_unichar_to_utf8 (uc, out);
		g_assert (size < 7);
		out [size] = 0;
		text = g_string_append (text, out);
		html_cursor_forward (cursor, e);
		cited2 = FALSE;
	}

	word = text->str;
	g_string_free (text, FALSE);
	html_cursor_destroy (cursor);

	return word;
}

gboolean
html_engine_spell_word_is_valid (HTMLEngine *e)
{
	HTMLObject *obj;
	HTMLText   *text;
	GList *cur;
	gboolean valid = TRUE;
	gint offset;
	gchar prev, curr;
	gboolean cited;

	cited = FALSE;
	prev = html_cursor_get_prev_char (e->cursor);
	curr = html_cursor_get_current_char (e->cursor);

	/* if we are not in word always return TRUE so we care only about invalid words */
	if (!html_selection_spell_word (prev, &cited) && !cited && !html_selection_spell_word (curr, &cited) && !cited)
		return TRUE;

	if (html_selection_spell_word (curr, &cited)) {
		gboolean end;

		end    = (e->cursor->offset == html_object_get_length (e->cursor->object));
		obj    = (end) ? html_object_next_not_slave (e->cursor->object) : e->cursor->object;
		offset = (end) ? 0 : e->cursor->offset;
	} else {
		obj    = (e->cursor->offset) ? e->cursor->object : html_object_prev_not_slave (e->cursor->object);
		offset = (e->cursor->offset) ? e->cursor->offset - 1 : html_object_get_length (obj) - 1;
	}

	g_assert (html_object_is_text (obj));
	text = HTML_TEXT (obj);

	/* now we have text, so let search for spell_error area in it */
	cur = text->spell_errors;
	while (cur) {
		SpellError *se = (SpellError *) cur->data;
		if (se->off <= offset && offset <= se->off + se->len) {
			valid = FALSE;
			break;
		}
		if (offset < se->off)
			break;
		cur = cur->next;
	}

	/* printf ("is_valid: %d\n", valid); */

	return valid;
}

void
html_engine_replace_spell_word_with (HTMLEngine *e, const gchar *word)
{
	HTMLObject *replace = NULL;
	HTMLText   *orig;

	html_engine_select_spell_word_editable (e);

	orig = HTML_TEXT (e->mark->object);
	switch (HTML_OBJECT_TYPE (e->mark->object)) {
	case HTML_TYPE_TEXT:
		replace = text_new (e, word, orig->font_style, orig->color);
		break;
		/* FIXME-link case HTML_TYPE_LINKTEXT:
		replace = html_link_text_new (word, orig->font_style, orig->color,
					      HTML_LINK_TEXT (orig)->url,
					      HTML_LINK_TEXT (orig)->target);
					      break; */
	default:
		g_assert_not_reached ();
	}
	html_text_set_font_face (HTML_TEXT (replace), HTML_TEXT (orig)->face);
	html_engine_edit_selection_updater_update_now (e->selection_updater);
	html_engine_paste_object (e, replace, html_object_get_length (replace));
}

HTMLCursor *
html_engine_get_cursor (HTMLEngine *e)
{
	HTMLCursor *cursor;

	cursor = html_cursor_new ();
	cursor->object = html_engine_get_object_at (e, e->widget->selection_x1, e->widget->selection_y1,
						    &cursor->offset, FALSE);
	return cursor;
}

void
html_engine_set_painter (HTMLEngine *e, HTMLPainter *painter)
{
	g_return_if_fail (painter != NULL);
	g_return_if_fail (e != NULL);

	g_object_ref (G_OBJECT (painter));
	g_object_unref (G_OBJECT (e->painter));
	e->painter = painter;
	
	html_object_set_painter (e->clue, painter);
	html_object_change_set_down (e->clue, HTML_CHANGE_ALL);
	html_object_reset (e->clue);
	html_engine_calc_size (e, FALSE);
}

gint
html_engine_get_view_width (HTMLEngine *e)
{
	return MAX (0, (e->widget->iframe_parent
		? html_engine_get_view_width (GTK_HTML (e->widget->iframe_parent)->engine)
		: GTK_WIDGET (e->widget)->allocation.width) - (html_engine_get_left_border (e) + html_engine_get_right_border (e)));
}

gint
html_engine_get_view_height (HTMLEngine *e)
{
	return MAX (0, (e->widget->iframe_parent
		? html_engine_get_view_height (GTK_HTML (e->widget->iframe_parent)->engine)
		: GTK_WIDGET (e->widget)->allocation.height) - (html_engine_get_top_border (e) + html_engine_get_bottom_border (e)));
}

/* beginnings of ID support */
void
html_engine_add_object_with_id (HTMLEngine *e, const gchar *id, HTMLObject *obj)
{
	gpointer old_key;
	gpointer old_val;

	if (e->id_table == NULL)
		e->id_table = g_hash_table_new (g_str_hash, g_str_equal);

	if (!g_hash_table_lookup_extended (e->id_table, id, &old_key, &old_val))
		old_key = NULL;

	g_hash_table_insert (e->id_table, old_key ? old_key : g_strdup (id), obj);
}

HTMLObject *
html_engine_get_object_by_id (HTMLEngine *e, const gchar *id)
{
	if (e->id_table == NULL)
		return NULL;

	return (HTMLObject *) g_hash_table_lookup (e->id_table, id);
}

GHashTable *
html_engine_get_class_table (HTMLEngine *e, const gchar *class_name)
{
	return (class_name && e->class_data) ? g_hash_table_lookup (e->class_data, class_name) : NULL;
}

static inline GHashTable *
get_class_table_sure (HTMLEngine *e, const gchar *class_name)
{
	GHashTable *t;

	if (e->class_data == NULL)
		e->class_data = g_hash_table_new (g_str_hash, g_str_equal);

	t = html_engine_get_class_table (e, class_name);
	if (!t) {
		t = g_hash_table_new (g_str_hash, g_str_equal);
		g_hash_table_insert (e->class_data, g_strdup (class_name), t);
	}

	return t;
}

void
html_engine_set_class_data (HTMLEngine *e, const gchar *class_name, const gchar *key, const gchar *value)
{
	GHashTable *t;
	gpointer old_key;
	gpointer old_val;

	/* printf ("set (%s) %s to %s (%p)\n", class_name, key, value, e->class_data); */
	g_return_if_fail (class_name);

	t = get_class_table_sure (e, class_name);

	if (!g_hash_table_lookup_extended (t, key, &old_key, &old_val))
		old_key = NULL;
	else {
		if (strcmp (old_val, value))
			g_free (old_val);
		else
			return;
	}
	g_hash_table_insert (t, old_key ? old_key : g_strdup (key), g_strdup (value));
}

void
html_engine_clear_class_data (HTMLEngine *e, const gchar *class_name, const gchar *key)
{
	GHashTable *t;
	gpointer old_key;
	gpointer old_val;

	t = html_engine_get_class_table (e, class_name);

	/* printf ("clear (%s) %s\n", class_name, key); */
	if (t && g_hash_table_lookup_extended (t, key, &old_key, &old_val)) {
		g_hash_table_remove (t, old_key);
		g_free (old_key);
		g_free (old_val);
	}
}

static gboolean
remove_class_data (gpointer key, gpointer val, gpointer data)
{
	/* printf ("remove: %s, %s\n", key, val); */
	g_free (key);
	g_free (val);

	return TRUE;
}

static gboolean
remove_all_class_data (gpointer key, gpointer val, gpointer data)
{
	g_hash_table_foreach_remove ((GHashTable *) val, remove_class_data, NULL);
	/* printf ("remove table: %s\n", key); */
	g_hash_table_destroy ((GHashTable *) val);
	g_free (key);

	return TRUE;
}

void
html_engine_clear_all_class_data (HTMLEngine *e)
{
	if (e->class_data) {
		g_hash_table_foreach_remove (e->class_data, remove_all_class_data, NULL);
		g_hash_table_destroy (e->class_data);
		e->class_data = NULL;
	}
}

const gchar *
html_engine_get_class_data (HTMLEngine *e, const gchar *class_name, const gchar *key)
{
	GHashTable *t = html_engine_get_class_table (e, class_name);
	return t ? g_hash_table_lookup (t, key) : NULL;
}

static void
set_object_data (gpointer key, gpointer value, gpointer data)
{
	/* printf ("set %s\n", (const gchar *) key); */
	html_object_set_data (HTML_OBJECT (data), g_strdup ((const gchar *) key), g_strdup ((const gchar *) value));
}

static void
html_engine_set_object_data (HTMLEngine *e, HTMLObject *o)
{
	GHashTable *t;

	t = html_engine_get_class_table (e, html_type_name (HTML_OBJECT_TYPE (o)));
	if (t)
		g_hash_table_foreach (t, set_object_data, o);
}


HTMLEngine *
html_engine_get_top_html_engine (HTMLEngine *e)
{
	while (e->widget->iframe_parent)
		e = GTK_HTML (e->widget->iframe_parent)->engine;

	return e;
}

void
html_engine_add_expose  (HTMLEngine *e, gint x, gint y, gint width, gint height, gboolean expose)
{
	struct HTMLEngineExpose *r;

	/* printf ("html_engine_add_expose\n"); */

	g_assert (HTML_IS_ENGINE (e));

	r = g_new (struct HTMLEngineExpose, 1);

	r->area.x = x;
	r->area.y = y;
	r->area.width = width;
	r->area.height = height;
	r->expose = expose;

	e->pending_expose = g_slist_prepend (e->pending_expose, r);
}

static void
html_engine_queue_redraw_all (HTMLEngine *e)
{
	clear_pending_expose (e);
	html_draw_queue_clear (e->draw_queue);
	
	if (GTK_WIDGET_REALIZED (e->widget)) {
		gtk_widget_queue_draw (GTK_WIDGET (e->widget));
	}
}

void
html_engine_redraw_selection (HTMLEngine *e)
{
	if (e->selection) {
		html_interval_unselect (e->selection, e);
		html_interval_select (e->selection, e);
		html_engine_flush_draw_queue (e);
	}
}

void
html_engine_set_language (HTMLEngine *e, const gchar *language)
{
	g_free (e->language);
	e->language = g_strdup (language);

	gtk_html_api_set_language (GTK_HTML (e->widget));
}

const gchar *
html_engine_get_language (HTMLEngine *e)
{
	gchar *language;

	language = e->language;
	if (!language)
		language = GTK_HTML_CLASS (GTK_WIDGET_GET_CLASS (e->widget))->properties->language;
	if (!language)
		language = "";

	return language;
}

static void
draw_link_text (HTMLText *text, HTMLEngine *e, gint offset)
{
	HTMLTextSlave *start, *end;

	if (html_text_get_link_slaves_at_offset (text, offset, &start, &end)) {
		while (start) {
			html_engine_queue_draw (e, HTML_OBJECT (start));
			if (start == end)
				break;
			start = HTML_TEXT_SLAVE (HTML_OBJECT (start)->next);
		}
	}
}

HTMLObject *
html_engine_get_focus_object (HTMLEngine *e, gint *offset)
{
	HTMLObject *o = e->focus_object;

	while (html_object_is_frame (o)) {
		o = html_object_get_engine (o, e)->focus_object;
	}

	if (o && offset)
		*offset = html_object_get_engine (o, e)->focus_object_offset;

	return o;
}

static HTMLObject *
move_focus_object (HTMLObject *o, gint *offset, HTMLEngine *e, GtkDirectionType dir)
{
	if (HTML_IS_TEXT (o) && ((dir == GTK_DIR_TAB_FORWARD && html_text_next_link_offset (HTML_TEXT (o), offset))
				 || (dir == GTK_DIR_TAB_BACKWARD && html_text_prev_link_offset (HTML_TEXT (o), offset))))
		return o;

	o = dir == GTK_DIR_TAB_FORWARD
		? html_object_next_cursor_object (o, e, offset)
		: html_object_prev_cursor_object (o, e, offset);

	if (HTML_IS_TEXT (o)) {
		if (dir == GTK_DIR_TAB_FORWARD)
			html_text_first_link_offset (HTML_TEXT (o), offset);
		else
			html_text_last_link_offset (HTML_TEXT (o), offset);
	}

	return o;
}

gboolean
html_engine_focus (HTMLEngine *e, GtkDirectionType dir)
{
	if (e->clue && (dir == GTK_DIR_TAB_FORWARD || dir == GTK_DIR_TAB_BACKWARD)) {
		HTMLObject *cur;
		HTMLObject *focus_object;
		gint offset;

		focus_object = html_engine_get_focus_object (e, &offset);
		if (focus_object && html_object_is_embedded (focus_object)
		    && HTML_EMBEDDED (focus_object)->widget
		    && gtk_widget_child_focus (HTML_EMBEDDED (focus_object)->widget, dir))
			return TRUE;

		if (focus_object)
			cur = move_focus_object (focus_object, &offset, e, dir);
		else
			cur = dir == GTK_DIR_TAB_FORWARD
				? html_object_get_head_leaf (e->clue)
				: html_object_get_tail_leaf (e->clue);

		while (cur) {
			/* printf ("try child %p\n", cur); */
			if ((HTML_IS_TEXT (cur) && html_object_get_complete_url (cur, offset))
			    || (HTML_IS_IMAGE (cur) && HTML_IMAGE (cur)->url && *HTML_IMAGE (cur)->url)) {
				html_engine_set_focus_object (e, cur, offset);

				return TRUE;
			} else if (html_object_is_embedded (cur) && !html_object_is_frame (cur)
				   && HTML_EMBEDDED (cur)->widget) {
				if (!GTK_WIDGET_DRAWABLE (HTML_EMBEDDED (cur)->widget)) {
					gint x, y;

					html_object_calc_abs_position (cur, &x, &y);
					gtk_layout_put (GTK_LAYOUT (HTML_EMBEDDED (cur)->parent),
							HTML_EMBEDDED (cur)->widget, x, y);
				}

				if (gtk_widget_child_focus (HTML_EMBEDDED (cur)->widget, dir)) {
					html_engine_set_focus_object (e, cur, offset);
					return TRUE;
				}
			}
			cur = move_focus_object (cur, &offset, e, dir);
		}
		/* printf ("no focus\n"); */
		html_engine_set_focus_object (e, NULL, 0);
	}

	return FALSE;
}

static void
draw_focus_object (HTMLEngine *e, HTMLObject *o, gint offset)
{
	e = html_object_engine (o, e);
	if (HTML_IS_TEXT (o) && html_object_get_url (o, offset))
		draw_link_text (HTML_TEXT (o), e, offset);
	else if (HTML_IS_IMAGE (o))
		html_engine_queue_draw (e, o);
}

void
html_engine_draw_focus_object (HTMLEngine *e)
{
	gint offset;

	draw_focus_object (e, html_engine_get_focus_object (e, &offset), offset);
}

static void
reset_focus_object_forall (HTMLObject *o, HTMLEngine *e)
{
	if (e->focus_object) {
		/* printf ("reset focus object\n"); */
		if (!html_object_is_frame (e->focus_object)) {
			e->focus_object->draw_focused = FALSE;
			draw_focus_object (e, e->focus_object, e->focus_object_offset);
		}
		e->focus_object = NULL;
		html_engine_flush_draw_queue (e);
	}

	if (o)
		o->draw_focused = FALSE;
}

static void
reset_focus_object (HTMLEngine *e)
{
	HTMLEngine *e_top;

	e_top = html_engine_get_top_html_engine (e);

	if (e_top && e_top->clue) {
		reset_focus_object_forall (NULL, e_top);
		html_object_forall (e_top->clue, e_top, (HTMLObjectForallFunc) reset_focus_object_forall, NULL);
	}
}

static void
set_frame_parents_focus_object (HTMLEngine *e)
{
	while (e->widget->iframe_parent) {
		HTMLEngine *e_parent;

		/* printf ("set frame parent focus object\n"); */
		e_parent = GTK_HTML (e->widget->iframe_parent)->engine;
		e_parent->focus_object = e->clue->parent;
		e = e_parent;
	}
}

void
html_engine_set_focus_object (HTMLEngine *e, HTMLObject *o, gint offset)
{
	/* printf ("set focus object to: %p\n", o); */

	reset_focus_object (e);

	if (o) {
		e = html_object_engine (o, e);
		e->focus_object = o;
		e->focus_object_offset = offset;

		if (!html_object_is_frame (e->focus_object)) {
			o->draw_focused = TRUE;
			if (HTML_IS_TEXT (o))
				HTML_TEXT (o)->focused_link_offset = offset;
			draw_focus_object (e, o, offset);
			html_engine_flush_draw_queue (e);
		}
		set_frame_parents_focus_object (e);
	}
}

gboolean
html_engine_is_saved (HTMLEngine *e)
{
	return e->saved_step_count != -1 && e->saved_step_count == html_undo_get_step_count (e->undo);
}

void
html_engine_saved (HTMLEngine *e)
{
	e->saved_step_count = html_undo_get_step_count (e->undo);
}

static void
html_engine_add_map (HTMLEngine *e, const char *name)
{
	gpointer old_key = NULL, old_val;
 
	if (!e->map_table) {
		e->map_table = g_hash_table_new (g_str_hash, g_str_equal);
	}

	/* only add a new map if the name is unique */
	if (!g_hash_table_lookup_extended (e->map_table, name, &old_key, &old_val)) {
		e->map = html_map_new (name);
		
		/* printf ("added map %s", name); */

		g_hash_table_insert (e->map_table, e->map->name, e->map);
	}
}

static gboolean
map_table_free_func (gpointer key, gpointer val, gpointer data)
{
	html_map_destroy (HTML_MAP (val));
	return TRUE;
}

static void
html_engine_map_table_clear (HTMLEngine *e)
{
	if (e->map_table) {
		g_hash_table_foreach_remove (e->map_table, map_table_free_func, NULL);
		g_hash_table_destroy (e->map_table);
		e->map_table = NULL;
	}
}

HTMLMap *
html_engine_get_map (HTMLEngine *e, const gchar *name)
{
	return e->map_table ? HTML_MAP (g_hash_table_lookup (e->map_table, name)) : NULL;
}

struct HTMLEngineCheckSelectionType
{
	HTMLType req_type;
	gboolean has_type;
};

static void
check_type_in_selection (HTMLObject *o, HTMLEngine *e, struct HTMLEngineCheckSelectionType *tmp)
{
	if (HTML_OBJECT_TYPE (o) == tmp->req_type)
		tmp->has_type = TRUE;
}

gboolean
html_engine_selection_contains_object_type (HTMLEngine *e, HTMLType obj_type)
{
	struct HTMLEngineCheckSelectionType tmp;

	tmp.has_type = FALSE;
	html_engine_edit_selection_updater_update_now (e->selection_updater);
	if (e->selection)
		html_interval_forall (e->selection, e, (HTMLObjectForallFunc) check_type_in_selection, &tmp);

	return tmp.has_type;
}

static void
check_link_in_selection (HTMLObject *o, HTMLEngine *e, gboolean *has_link)
{
	if ((HTML_IS_TEXT (o) && HTML_TEXT (o)->links) ||
	    (HTML_IS_IMAGE (o) && (HTML_IMAGE (o)->url || HTML_IMAGE (o)->target)))
		*has_link = TRUE;
}

gboolean
html_engine_selection_contains_link (HTMLEngine *e)
{
	gboolean has_link;

	has_link = FALSE;
	html_engine_edit_selection_updater_update_now (e->selection_updater);
	if (e->selection)
		html_interval_forall (e->selection, e, (HTMLObjectForallFunc) check_link_in_selection, &has_link);

	return has_link;
}

gint
html_engine_get_left_border (HTMLEngine *e)
{
	return HTML_IS_PLAIN_PAINTER (e->painter) ? LEFT_BORDER : e->leftBorder;
}

gint
html_engine_get_right_border (HTMLEngine *e)
{
	return HTML_IS_PLAIN_PAINTER (e->painter) ? RIGHT_BORDER : e->rightBorder;
}

gint
html_engine_get_top_border (HTMLEngine *e)
{
	return HTML_IS_PLAIN_PAINTER (e->painter) ? TOP_BORDER : e->topBorder;
}

gint
html_engine_get_bottom_border (HTMLEngine *e)
{
	return HTML_IS_PLAIN_PAINTER (e->painter) ? BOTTOM_BORDER : e->bottomBorder;
}
