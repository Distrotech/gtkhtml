/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#define MEMDEBUG

#include <gnome.h>
#include <gtk/gtk.h>

#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-master.h>
#include <libgnomeprint/gnome-print-master-preview.h>

#include "config.h"
#include "gtkhtml.h"
#include "htmlurl.h"
#include "htmlengine.h"
#include "gtkhtml-embedded.h"
#include "gtkhtml-properties.h"

#include "gtkhtmldebug.h"

#undef PACKAGE
#undef VERSION

#include <WWWApp.h>
#include <WWWLib.h>
#include <WWWStream.h>
#include <WWWInit.h>
#undef PACKAGE
#undef VERSION

#include "config.h"

#include <glibwww/glibwww.h>

typedef struct {
  FILE *fil;
  GtkHTMLStream *handle;
} FileInProgress;

typedef struct {
	gchar *url;
	gchar *title;
	GtkWidget *widget;
} go_item; 

#define MAX_GO_ENTRIES 20

static void exit_cb (GtkWidget *widget, gpointer data);
static void print_preview_cb (GtkWidget *widget, gpointer data);
static void test_cb (GtkWidget *widget, gpointer data);
static void bug_cb (GtkWidget *widget, gpointer data);
static void slow_cb (GtkWidget *widget, gpointer data);
static void stop_cb (GtkWidget *widget, gpointer data);
static void dump_cb (GtkWidget *widget, gpointer data);
static void dump_simple_cb (GtkWidget *widget, gpointer data);
static void forward_cb (GtkWidget *widget, gpointer data);
static void back_cb (GtkWidget *widget, gpointer data);
static void home_cb (GtkWidget *widget, gpointer data);
static void reload_cb (GtkWidget *widget, gpointer data);
static void redraw_cb (GtkWidget *widget, gpointer data);
static void resize_cb (GtkWidget *widget, gpointer data);
static void title_changed_cb (GtkHTML *html, const gchar *title, gpointer data);
static void url_requested (GtkHTML *html, const char *url, GtkHTMLStream *handle, gpointer data);
static void entry_goto_url(GtkWidget *widget, gpointer data);
static void goto_url(const char *url, int back_or_forward);
static void on_set_base (GtkHTML *html, const gchar *url, gpointer data);

static int netin_stream_put_character (HTStream * me, char c);
static int netin_stream_put_string (HTStream * me, const char * s);;
static int netin_stream_write (HTStream * me, const char * s, int l);
static int netin_stream_flush (HTStream * me);
static int netin_stream_free (HTStream * me);
static int netin_stream_abort (HTStream * me, HTList * e);
static HTStream *netin_stream_new (GtkHTMLStream *handle, HTRequest *request);
static int redirectFilter(HTRequest *request, HTResponse *response, void *param, int status);
static gchar *parse_href (const gchar *s);

static GtkHTML *html;
static GtkHTMLStream *html_stream_handle = NULL;
static GtkWidget *animator, *entry;
static GtkWidget *popup_menu, *popup_menu_back, *popup_menu_forward, *popup_menu_home;
static GtkWidget *toolbar_back, *toolbar_forward;
static HTMLURL *baseURL;

static GList *go_list;
static int go_position;

static gboolean use_redirect_filter;

static gboolean slow_loading = FALSE, exit_when_done = FALSE;

static gint redirect_timerId = 0;
static gchar *redirect_url = NULL;

static GnomeUIInfo file_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("Print pre_view"), N_("Print preview"),
	  print_preview_cb },
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_EXIT_ITEM (exit_cb, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo test_menu[] = {
	{ GNOME_APP_UI_ITEM, "Test 1", "Run test 1",
	  test_cb, GINT_TO_POINTER (1), NULL, 0, 0, 0, 0},
	{ GNOME_APP_UI_ITEM, "Test 2", "Run test 2",
	  test_cb, GINT_TO_POINTER (2), NULL, 0, 0, 0, 0},
	{ GNOME_APP_UI_ITEM, "Test 3", "Run test 3",
	  test_cb, GINT_TO_POINTER (3), NULL, 0, 0, 0, 0},
	{ GNOME_APP_UI_ITEM, "Test 4", "Run test 4",
	  test_cb, GINT_TO_POINTER (4), NULL, 0, 0, 0, 0},
	{ GNOME_APP_UI_ITEM, "Test 5", "Run test 5",
	  test_cb, GINT_TO_POINTER (5), NULL, 0, 0, 0, 0},
	{ GNOME_APP_UI_ITEM, "Test 6", "Run test 6",
	  test_cb, GINT_TO_POINTER (6), NULL, 0, 0, 0, 0},
	{ GNOME_APP_UI_ITEM, "Test 7", "Run test 7 (FreshMeat)",
	  test_cb, GINT_TO_POINTER (7), NULL, 0, 0, 0, 0},
	{ GNOME_APP_UI_ITEM, "Test 8", "Run test 8 (local test)",
	  test_cb, GINT_TO_POINTER (8), NULL, 0, 0, 0, 0},
	{ GNOME_APP_UI_ITEM, "Test 9", "Run test 9 (Form Test)",
	  test_cb, GINT_TO_POINTER (9), NULL, 0, 0, 0, 0},
	{ GNOME_APP_UI_ITEM, "Test 10", "Run test 10 (Object Test)",
	  test_cb, GINT_TO_POINTER (10), NULL, 0, 0, 0, 0},
	{ GNOME_APP_UI_ITEM, "Test 11", "Run test 11 (Nowrap)",
	  test_cb, GINT_TO_POINTER (11), NULL, 0, 0, 0, 0},
	GNOMEUIINFO_END
};

static GnomeUIInfo debug_menu[] = {
	{ GNOME_APP_UI_ITEM, "Show bug list", "Show the layout bug list",
	  bug_cb, NULL, NULL, 0, 0, 0, 0},
	{ GNOME_APP_UI_ITEM, "Dump Object tree", "Dump Object tree to stdout",
	  dump_cb, NULL, NULL, 0, 0, 0, 0},
	{ GNOME_APP_UI_ITEM, "Dump Object tree (simple)", "Dump Simple Object tree to stdout",
	  dump_simple_cb, NULL, NULL, 0, 0, 0, 0},
	GNOMEUIINFO_TOGGLEITEM("Slow loading", "Load documents slowly", slow_cb, NULL),
	{ GNOME_APP_UI_ITEM, "Force resize", "Force a resize event",
	  resize_cb, NULL, NULL, 0, 0, 0 },
	{ GNOME_APP_UI_ITEM, "Force repaint", "Force a repaint event",
	  redraw_cb, NULL, NULL, 0, 0, 0 },
	GNOMEUIINFO_END
};

static GnomeUIInfo go_menu[] = {
	{ GNOME_APP_UI_ITEM, "Back", "Return to the previous page in history list",
	  back_cb, NULL, NULL, 0, 0, 0, 0},
	{ GNOME_APP_UI_ITEM, "Forward", "Go to the next page in history list", 
	  forward_cb, NULL, NULL, 0, 0, 0, 0},
	{ GNOME_APP_UI_ITEM, "Home", "Go to the homepage",
	  home_cb, NULL, NULL, 0, 0, 0 },
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_END
};

static GnomeUIInfo main_menu[] = {
	GNOMEUIINFO_MENU_FILE_TREE (file_menu),
	GNOMEUIINFO_SUBTREE (("_Tests"), test_menu),
	GNOMEUIINFO_SUBTREE (("_Debug"), debug_menu),
	GNOMEUIINFO_SUBTREE (("_Go"), go_menu),
	GNOMEUIINFO_END
};

static void
create_toolbars (GtkWidget *app)
{
	GtkWidget *dock;
	GtkWidget *hbox, *hbox2;
	GtkWidget *frame;
	GtkWidget *button;
	GtkWidget *toolbar;
	char *imgloc;

	dock = gnome_dock_item_new ("testgtkhtml-toolbar1",
				    (GNOME_DOCK_ITEM_BEH_EXCLUSIVE));
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (dock), hbox);
	gtk_container_border_width (GTK_CONTAINER (dock), 2);
	
	toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL,
				   GTK_TOOLBAR_ICONS);
	gtk_toolbar_set_style (GTK_TOOLBAR (toolbar),
			       GTK_TOOLBAR_ICONS);
	gtk_toolbar_set_space_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_SPACE_LINE);
	gtk_toolbar_set_button_relief (GTK_TOOLBAR (toolbar),
				       GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (hbox), toolbar, FALSE, FALSE, 0);

	toolbar_back = gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
				 NULL,
				 "Move back",
				 "Back",
				 gnome_stock_new_with_icon (GNOME_STOCK_PIXMAP_BACK),
				 back_cb, NULL);
	gtk_widget_set_sensitive(toolbar_back, FALSE);
	toolbar_forward = gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
				 NULL,
				 "Move forward",
				 "Forward",
				 gnome_stock_new_with_icon (GNOME_STOCK_PIXMAP_FORWARD),
				 forward_cb, NULL);
	gtk_widget_set_sensitive(toolbar_forward, FALSE);
	gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));
	gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
				 NULL,
				 "Stop loading",
				 "Stop",
				 gnome_stock_new_with_icon (GNOME_STOCK_PIXMAP_STOP),
				 stop_cb,
				 NULL);
	gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
				 NULL,
				 "Reload page",
				 "Reload",
				 gnome_stock_new_with_icon (GNOME_STOCK_PIXMAP_REFRESH),
				 reload_cb, NULL);
	gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
				 NULL,
				 "Home page",
				 "Home",
				 gnome_stock_new_with_icon (GNOME_STOCK_PIXMAP_HOME),
				 home_cb, NULL);
	gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

	animator = gnome_animator_new_with_size (32, 32);

	if (g_file_exists("32.png"))
	  imgloc = "32.png";
	else if (g_file_exists(SRCDIR "/32.png"))
	  imgloc = SRCDIR "/32.png";
	else
	  imgloc = "32.png";
	gnome_animator_append_frames_from_file_at_size (GNOME_ANIMATOR (animator),
							imgloc,
							0, 0,
							25,
							32, 
							32, 32);

	frame = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (frame), animator);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_box_pack_end (GTK_BOX (hbox), frame, FALSE, FALSE, 0);
	gnome_animator_set_loop_type (GNOME_ANIMATOR (animator), 
				      GNOME_ANIMATOR_LOOP_RESTART);
	gtk_widget_show_all (dock);
	gnome_dock_add_item (GNOME_DOCK (GNOME_APP (app)->dock),
			     GNOME_DOCK_ITEM (dock), GNOME_DOCK_TOP, 1, 0, 0, FALSE);

	/* Create the location bar */
	dock = gnome_dock_item_new ("testgtkhtml-toolbar2",
				    (GNOME_DOCK_ITEM_BEH_EXCLUSIVE));
	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (dock), hbox);
	gtk_container_border_width (GTK_CONTAINER (dock), 2);
	hbox2 = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox2), gtk_label_new ("Goto"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox2), gnome_stock_new_with_icon (GNOME_STOCK_PIXMAP_REDO), FALSE, FALSE, 0);
	button = gtk_button_new ();
	GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_container_add (GTK_CONTAINER (button), hbox2);
	gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox),
			    gtk_label_new ("Location:"), FALSE, FALSE, 0);
	entry = gtk_entry_new ();
	gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(entry_goto_url), NULL);
	gtk_box_pack_start (GTK_BOX (hbox),
			    entry, TRUE, TRUE, 0);
	gnome_dock_add_item (GNOME_DOCK (GNOME_APP (app)->dock),
			     GNOME_DOCK_ITEM (dock), GNOME_DOCK_TOP, 2, 0, 0, FALSE);

}

static gint page_num;
static GnomeFont *font;

static void
print_footer (GtkHTML *html, GnomePrintContext *context,
	      gdouble x, gdouble y, gdouble width, gdouble height, gpointer user_data)
{
	gchar *text = g_strdup_printf ("- %d -", page_num);
	gdouble tw = gnome_font_get_width_string (font, "text");

	if (font) {
		gnome_print_newpath     (context);
		gnome_print_setrgbcolor (context, .0, .0, .0);
		gnome_print_moveto      (context, x + (width - tw)/2, y - (height + gnome_font_get_ascender (font))/2);
		gnome_print_setfont     (context, font);
		gnome_print_show        (context, text);
	}

	g_free (text);
	page_num++;
}

static void
print_preview_cb (GtkWidget *widget,
		  gpointer data)
{
	GnomePrintMaster *print_master;
	GnomePrintContext *print_context;
	GtkWidget *preview;

	print_master = gnome_print_master_new ();
	gnome_print_master_set_paper (print_master, gnome_paper_with_name ("US-Letter"));
	gtk_html_print_set_master (html, print_master);

	print_context = gnome_print_master_get_context (print_master);

	page_num = 1;
	font = gnome_font_new_closest ("Helvetica", GNOME_FONT_BOOK, FALSE, 12);
	gtk_html_print_with_header_footer (html, print_context, .0, .03, NULL, print_footer, NULL);
	if (font) gtk_object_unref (GTK_OBJECT (font));

	preview = GTK_WIDGET (gnome_print_master_preview_new (print_master, "HTML Print Preview"));
	gtk_widget_show (preview);

	gtk_object_unref (GTK_OBJECT (print_master));
}

static void
dump_cb (GtkWidget *widget, gpointer data)
{
	g_print ("Object Tree\n");
	g_print ("-----------\n");

	gtk_html_debug_dump_tree (html->engine->clue, 0);
}

static void
dump_simple_cb (GtkWidget *widget, gpointer data)
{
	g_print ("Simple Object Tree\n");
	g_print ("-----------\n");

	gtk_html_debug_dump_tree_simple (html->engine->clue, 0);
}

static void
resize_cb (GtkWidget *widget, gpointer data)
{
	g_print ("forcing resize\n");
	html_engine_calc_size (html->engine, FALSE);
}

static void
redraw_cb (GtkWidget *widget, gpointer data)
{
	g_print ("forcing redraw\n");
	gtk_widget_draw (GTK_WIDGET (html), NULL);
}

static void
slow_cb (GtkWidget *widget, gpointer data)
{
	slow_loading = !slow_loading;
}

static void
title_changed_cb (GtkHTML *html, const gchar *title, gpointer data)
{
	gchar *s;

	s = g_strconcat ("GtkHTML: ", title, NULL);
	gtk_window_set_title (GTK_WINDOW (data), s);
	g_free (s);
}

static void
entry_goto_url(GtkWidget *widget, gpointer data)
{
	gchar *tmpurl;

	tmpurl = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));

	/* Add "http://" if no protocol is specified */
	if(strchr(tmpurl, ':'))
		goto_url (tmpurl, 0);
	else {
		gchar *url;

		url = g_strdup_printf("http://%s", tmpurl);
		on_set_base (NULL, url, NULL);
		goto_url (url, 0);
		g_free(url);
	}
	g_free (tmpurl);
}

static void
home_cb (GtkWidget *widget, gpointer data)
{
	goto_url("http://www.gnome.org", 0);
}

static void
back_cb (GtkWidget *widget, gpointer data)
{
	go_item *item;

	go_position++;

	if((item = g_list_nth_data(go_list, go_position))) {

		goto_url(item->url, 1);
		gtk_widget_set_sensitive(popup_menu_forward, TRUE);
		gtk_widget_set_sensitive(toolbar_forward, TRUE);
		gtk_widget_set_sensitive(go_menu[1].widget, TRUE);

		if(go_position == (g_list_length(go_list) - 1)) {

			gtk_widget_set_sensitive(popup_menu_back, FALSE);
			gtk_widget_set_sensitive(toolbar_back, FALSE);
			gtk_widget_set_sensitive(go_menu[0].widget, FALSE);
		}

	} else
		go_position--;
}

static void
forward_cb (GtkWidget *widget, gpointer data)
{
	go_item *item;

	go_position--;

	if((go_position >= 0) && (item = g_list_nth_data(go_list, go_position))) {

		goto_url(item->url, 1);

		gtk_widget_set_sensitive(popup_menu_back, TRUE);
		gtk_widget_set_sensitive(toolbar_back, TRUE);
		gtk_widget_set_sensitive(go_menu[0].widget, TRUE);

		if(go_position == 0) {
			gtk_widget_set_sensitive(popup_menu_forward, FALSE);
			gtk_widget_set_sensitive(toolbar_forward, FALSE);
			gtk_widget_set_sensitive(go_menu[1].widget, FALSE);
		}
	} else
		go_position++;
}

static void
reload_cb (GtkWidget *widget, gpointer data)
{
	go_item *item;

	if((item = g_list_nth_data(go_list, go_position))) {

		goto_url(item->url, 1);
	}
}

static void
stop_cb (GtkWidget *widget, gpointer data)
{
	/* Kill all requests */
	HTNet_killAll();
	html_stream_handle = NULL;
}

static void
load_done (GtkHTML *html)
{
	gnome_animator_stop (GNOME_ANIMATOR (animator));
	gnome_animator_goto_frame (GNOME_ANIMATOR (animator), 1);

	if (exit_when_done)
		gtk_main_quit();
}

static int
on_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	GtkMenu *menu;
	
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (event != NULL, FALSE);
	
	/* The "widget" is the menu that was supplied when 
	 * gtk_signal_connect_object was called.
	 */
	menu = GTK_MENU (popup_menu);
	
	if (event->type == GDK_BUTTON_PRESS) {

		if (event->button == 3) {
			gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 
					event->button, event->time);
			return TRUE;
		}
	}
		
	return FALSE;
}

static void
on_set_base (GtkHTML *html, const gchar *url, gpointer data)
{
	gtk_entry_set_text (GTK_ENTRY (entry), url);
	if (baseURL)
		html_url_destroy (baseURL);

	baseURL = html_url_new (url);
}

static gboolean
redirect_timer_event (gpointer data) {
	g_print("Redirecting to '%s' NOW\n", redirect_url);
	goto_url(redirect_url, 0);

	/*	OBS: redirect_url is freed in goto_url */

	return FALSE;
}

static void
on_redirect (GtkHTML *html, const gchar *url, int delay, gpointer data) {
	g_print("Redirecting to '%s' in %d seconds\n", url, delay);

	if(redirect_timerId == 0) {

		redirect_url = g_strdup(url);
		
		redirect_timerId = gtk_timeout_add (delay * 1000,(GtkFunction) redirect_timer_event, NULL);
	}
}

static void
on_submit (GtkHTML *html, const gchar *method, const gchar *action, const gchar *encoding, gpointer data) {
	GString *tmpstr = g_string_new (action);

	g_print("submitting '%s' to '%s' using method '%s'\n", encoding, action, method);

	if(strcasecmp(method, "GET") == 0) {

		tmpstr = g_string_append_c (tmpstr, '?');
		tmpstr = g_string_append (tmpstr, encoding);
		
		goto_url(tmpstr->str, 0);
		
		g_string_free (tmpstr, TRUE);
	} else {
		g_warning ("Unsupported submit method '%s'\n", method);
	}

}

static void
on_url (GtkHTML *html, const gchar *url, gpointer data)
{
	GnomeApp *app;

	app = GNOME_APP (data);

	if (url == NULL)
		gnome_appbar_set_status (GNOME_APPBAR (app->statusbar), "");
	else
		gnome_appbar_set_status (GNOME_APPBAR (app->statusbar), url);
}

static void
on_link_clicked (GtkHTML *html, const gchar *url, gpointer data)
{
	goto_url (url, 0);
}

/* Lame hack */
struct _HTStream {
	const HTStreamClass *	isa;
	GtkHTMLStream *handle;
};

static int
netin_stream_put_character (HTStream * me, char c)
{
	return netin_stream_write(me, &c, 1);
}

static int
netin_stream_put_string (HTStream * me, const char * s)
{
	return netin_stream_write(me, s, strlen(s));
}

static int
netin_stream_write (HTStream * me, const char * s, int l)
{
	printf ("*** %s (%d)\n",__FUNCTION__, l);
	gtk_html_write(html, me->handle, s, l);

	return HT_OK;
}

static int
netin_stream_flush (HTStream * me)
{
	return HT_OK;
}

static int
netin_stream_free (HTStream * me)
{
	gtk_html_end(html, me->handle, GTK_HTML_STREAM_OK);
	g_free(me);
	
	return HT_OK;
}

static int
netin_stream_abort (HTStream * me, HTList * e)
{
	return HT_OK;
}

static const HTStreamClass netin_stream_class =
{		
	"netin_stream",
	netin_stream_flush,
	netin_stream_free,
	netin_stream_abort,
	netin_stream_put_character,
	netin_stream_put_string,
	netin_stream_write
}; 

static HTStream *
netin_stream_new (GtkHTMLStream *handle, HTRequest *request)
{
	HTStream *retval;
	
	retval = g_new0(HTStream, 1);
	
	retval->isa = &netin_stream_class;
	retval->handle = handle;

	return retval;
}

/* simulate an async object isntantiation */
static int
object_timeout(GtkHTMLEmbedded *eb)
{
	GtkWidget *w;

	w = gtk_check_button_new();
	gtk_widget_show(w);

	printf("inserting custom widget after a delay ...\n");
	gtk_html_embedded_set_descent(eb, rand()%8);
	gtk_container_add (GTK_CONTAINER(eb), w);
	gtk_widget_unref (GTK_WIDGET (eb));

	return FALSE;
}

static gboolean
object_requested_cmd (GtkHTML *html, GtkHTMLEmbedded *eb, void *data)
{
	/* printf("object requested, wiaint a bit before creating it ...\n"); */

	if (strcmp (eb->classid, "mine:NULL") == 0)
		return FALSE;

	gtk_widget_ref (GTK_WIDGET (eb));
	gtk_timeout_add(rand() % 5000 + 1000, (GtkFunction) object_timeout, eb);
	/* object_timeout (eb); */

	return TRUE;
}

static void
url_requested (GtkHTML *html, const char *url, GtkHTMLStream *handle, gpointer data)
{
	gchar *full_url = NULL;
	HTRequest *newreq;
	BOOL status;

	newreq = HTRequest_new();
	g_assert(newreq);
	
 	/* Need to catch redirections */
 	/* Remove old filter if any */
 	HTNet_deleteAfter(redirectFilter);
 	/* Add new filters */
	full_url = parse_href (url);

	if (use_redirect_filter) {
		
		use_redirect_filter = FALSE;
		
		HTNet_addAfter(redirectFilter, full_url, NULL, HT_PERM_REDIRECT, HT_FILTER_FIRST);
		HTNet_addAfter(redirectFilter, full_url, NULL, HT_TEMP_REDIRECT, HT_FILTER_FIRST);
		HTNet_addAfter(redirectFilter, full_url, NULL, HT_SEE_OTHER, HT_FILTER_FIRST);
		HTNet_addAfter(redirectFilter, full_url, NULL, HT_FOUND, HT_FILTER_FIRST);
	}

	HTRequest_setOutputFormat(newreq, WWW_SOURCE);
	{
		HTStream *newstream = netin_stream_new(handle, newreq);
		status = HTLoadToStream(full_url, newstream, newreq);
		g_message("Loading URL %s to stream %p (status %d)", full_url, handle, status);
	}
	if (full_url)
		g_free (full_url);
	return;
}

static gchar *
parse_href (const gchar *s)
{
	gchar *retval;
	gchar *tmp;
	HTMLURL *tmpurl;

	if(s == NULL || *s == 0)
		return NULL;

	if (s[0] == '#') {
		tmpurl = html_url_dup (baseURL, HTML_URL_DUP_NOREFERENCE);
		html_url_set_reference (tmpurl, s + 1);

		tmp = html_url_to_string (tmpurl);
		html_url_destroy (tmpurl);

		return tmp;
	}

	tmpurl = html_url_new (s);
	if (html_url_get_protocol (tmpurl) == NULL) {
		if (s[0] == '/') {
			if (s[1] == '/') {
				gchar *t;

				/* Double slash at the beginning.  */

				/* FIXME?  This is a bit sucky.  */
				t = g_strconcat (html_url_get_protocol (baseURL),
						 ":", s, NULL);
				html_url_destroy (tmpurl);
				tmpurl = html_url_new (t);
				retval = html_url_to_string (tmpurl);
				html_url_destroy (tmpurl);
				g_free (t);
			} else {
				/* Single slash at the beginning.  */

				html_url_destroy (tmpurl);
				tmpurl = html_url_dup (baseURL,
						       HTML_URL_DUP_NOPATH);
				html_url_set_path (tmpurl, s);
				retval = html_url_to_string (tmpurl);
				html_url_destroy (tmpurl);
			}
		} else {
			html_url_destroy (tmpurl);
			tmpurl = html_url_append_path (baseURL, s);
			retval = html_url_to_string (tmpurl);
			html_url_destroy (tmpurl);
		}
	} else {
		retval = html_url_to_string (tmpurl);
		html_url_destroy (tmpurl);
	}

	return retval;
}

static void
go_list_cb (GtkWidget *widget, gpointer data)
{
	go_item *item;
	int num;
	/* Only if the item was selected, not deselected */
	if(GTK_CHECK_MENU_ITEM(widget)->active) {
		
		go_position = GPOINTER_TO_INT(data);
		
		if((item = g_list_nth_data(go_list, go_position))) {
			
			goto_url(item->url, 1);
			num = g_list_length(go_list);

			if(go_position == 0 || num < 2) {
				gtk_widget_set_sensitive(popup_menu_forward, FALSE);
				gtk_widget_set_sensitive(toolbar_forward, FALSE);
				gtk_widget_set_sensitive(go_menu[1].widget, FALSE);
			} else {
				gtk_widget_set_sensitive(popup_menu_forward, TRUE);
				gtk_widget_set_sensitive(toolbar_forward, TRUE);
				gtk_widget_set_sensitive(go_menu[1].widget, TRUE);
			}
			if(go_position == (num - 1) || num < 2) {
				gtk_widget_set_sensitive(popup_menu_back, FALSE);
				gtk_widget_set_sensitive(toolbar_back, FALSE);
				gtk_widget_set_sensitive(go_menu[0].widget, FALSE);
			} else {
				gtk_widget_set_sensitive(popup_menu_back, TRUE);
				gtk_widget_set_sensitive(toolbar_back, TRUE);
				gtk_widget_set_sensitive(go_menu[0].widget, TRUE);
			}
		}
	}
}

static void remove_go_list(gpointer data, gpointer user_data) {
	go_item *item = (go_item *)data;
	
	if(item->widget)
		gtk_widget_destroy(item->widget);
	
	item->widget = NULL;
}

static void
goto_url(const char *url, int back_or_forward)
{
	int tmp, i;
	go_item *item;
	GSList *group = NULL;
	gchar *full_url;

	/* Kill all requests */
	HTNet_killAll();

	/* Remove any pending redirection */
	if(redirect_timerId) {
		gtk_timeout_remove(redirect_timerId);

		redirect_timerId = 0;
	}

	gnome_animator_start (GNOME_ANIMATOR (animator));

	use_redirect_filter = TRUE;

	html_stream_handle = gtk_html_begin (html);

	/* Yuck yuck yuck.  Well this code is butt-ugly already anyway.  */
	url_requested (html, url, html_stream_handle, NULL);

	full_url = parse_href (url);
	on_set_base (NULL, full_url, NULL);

	if(!back_or_forward) {
		if(go_position) {
			/* Removes "Forward entries"*/
			tmp = go_position;
			while(tmp) {
				item = g_list_nth_data(go_list, --tmp);
				go_list = g_list_remove(go_list, item);
				if(item->url)
					g_free(item->url);
				if(item->title)
					g_free(item->title);
				if(item->url)
					gtk_widget_destroy(item->widget);
				g_free(item);
			}
			go_position = 0;
		}

		/* Removes old entries if the list is to big */
		tmp = g_list_length(go_list);
		while(tmp > MAX_GO_ENTRIES) {
			item = g_list_nth_data(go_list, MAX_GO_ENTRIES);

			if(item->url)
				g_free(item->url);
			if(item->title)
				g_free(item->title);
			if(item->url)
				gtk_widget_destroy(item->widget);
			g_free(item);

			go_list = g_list_remove(go_list, item);
			tmp--;
		}
		gtk_widget_set_sensitive(popup_menu_forward, FALSE);
		gtk_widget_set_sensitive(toolbar_forward, FALSE);
		gtk_widget_set_sensitive(go_menu[1].widget, FALSE);
		
		item = g_malloc0(sizeof(go_item));
		item->url = g_strdup(full_url);

		/* Remove old go list */
		g_list_foreach(go_list, remove_go_list, NULL);

		/* Add new url to go list */
		go_list = g_list_prepend(go_list, item);

		/* Create a new go list menu */
		tmp = g_list_length(go_list);
		group = NULL;

		for(i=0;i<tmp;i++) {
			item = g_list_nth_data(go_list, i);
			item->widget = gtk_radio_menu_item_new_with_label(group, item->url);

			gtk_signal_connect (GTK_OBJECT (item->widget), "activate",
					    GTK_SIGNAL_FUNC (go_list_cb), GINT_TO_POINTER (i));

			group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(item->widget));

			if(i == 0)
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item->widget), TRUE);

			gtk_menu_append(GTK_MENU(GTK_MENU_ITEM(main_menu[3].widget)->submenu), item->widget);
			gtk_widget_show(item->widget);
			
		}
		/* Enable the "Back" button if there are more then one url in the list */
		if(g_list_length(go_list) > 1) {

			gtk_widget_set_sensitive(popup_menu_back, TRUE);
			gtk_widget_set_sensitive(toolbar_back, TRUE);
			gtk_widget_set_sensitive(go_menu[0].widget, TRUE);
		}
	} else {
		/* Update current link in the go list */
		item = g_list_nth_data(go_list, go_position);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item->widget), TRUE);
	}

	if(redirect_url) {

		g_free(redirect_url);
		redirect_url = NULL;
	}
	g_free (full_url);
}

static void
bug_cb (GtkWidget *widget, gpointer data)
{
	gchar *filename;

	filename = g_strdup_printf ("%s/tests/bugs.html", HTGetCurrentDirectoryURL());
	goto_url (filename, 0);
	g_free (filename);
}

static void
test_cb (GtkWidget *widget, gpointer data)
{
	gchar *filename;
	
	filename = g_strdup_printf ("%s/tests/test%d.html", HTGetCurrentDirectoryURL(), GPOINTER_TO_INT (data));
	goto_url(filename, 0);
	g_free(filename);
}

static void
exit_cb (GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
}

static int request_terminater (HTRequest * request, HTResponse * response, void * param, int status) {
	if (status!=HT_LOADED) 
		g_print("Load couldn't be completed successfully (%p)\n", request);
	
	HTRequest_delete(request);
	return HT_OK;
}
static int redirectFilter(HTRequest *request, HTResponse *response,
		 void *param, int status)
{
	HTMethod method = HTRequest_method(request);
	HTAnchor *new_anchor = HTResponse_redirection(response);
	gchar *new_location;

	if (!new_anchor)
		return HT_OK;

	if (!HTMethod_isSafe(method))
		return HT_OK;

	HTRequest_deleteCredentialsAll(request);
	if (HTRequest_doRetry(request)) {
		HTRequest_setAnchor(request, new_anchor);
		HTLoad(request, NO);

		new_location = HTAnchor_address(new_anchor);

		g_print("******* Redirected to '%s' *****\n", new_location);

		/* Update BASE url */

		on_set_base (NULL, new_location, NULL);

		/* Update filters */
		HTNet_deleteAfter(redirectFilter);
		HTNet_addAfter(redirectFilter, new_location, NULL, HT_PERM_REDIRECT, HT_FILTER_FIRST);
		HTNet_addAfter(redirectFilter, new_location, NULL, HT_TEMP_REDIRECT, HT_FILTER_FIRST);
		HTNet_addAfter(redirectFilter, new_location, NULL, HT_FOUND, HT_FILTER_FIRST);
		HTNet_addAfter(redirectFilter, new_location, NULL, HT_SEE_OTHER, HT_FILTER_FIRST);
		
		free(new_location);

	} else {
		HTRequest_addError(request, ERR_FATAL, NO, HTERR_MAX_REDIRECT,
				   NULL, 0, "HTRedirectFilter");
		return HT_OK;
	}
	return HT_ERROR;
}

static struct poptOption options[] = {
  {"slow-loading", '\0', POPT_ARG_NONE, &slow_loading, 0, "Load the document as slowly as possible", NULL},
  {"exit-when-done", '\0', POPT_ARG_NONE, &exit_when_done, 0, "Exit the program as soon as the document is loaded", NULL},
  {NULL}
};

gint
main (gint argc, gchar *argv[])
{
	GtkWidget *app, *bar;
	GtkWidget *html_widget;
	GtkWidget *scrolled_window;
	/* GdkColor   bgColor = {0, 0xc7ff, 0xc7ff, 0xc7ff}; */
	poptContext ctx;
#ifdef GTKHTML_HAVE_GCONF
	GError  *gconf_error  = NULL;
#endif

#ifdef MEMDEBUG
	void *p = malloc (1024);	/* to make linker happy with ccmalloc */
#endif
	gnome_init_with_popt_table (PACKAGE, VERSION,
				    argc, argv, options, 0, &ctx);
	glibwww_init (PACKAGE, VERSION);
#ifdef GTKHTML_HAVE_GCONF
	if (!gconf_init (argc, argv, &gconf_error)) {
		g_assert (gconf_error != NULL);
		g_error ("GConf init failed:\n  %s", gconf_error->message);
		return FALSE;
	}
#endif

	HTNet_addAfter(request_terminater, NULL, NULL, HT_ALL, HT_FILTER_LAST);

	gdk_rgb_init ();
	
	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual (gdk_rgb_get_visual ());
	
	app = gnome_app_new ("testgtkhtml", "GtkHTML: testbed application");

	gtk_signal_connect (GTK_OBJECT (app), "delete_event",
			    GTK_SIGNAL_FUNC (exit_cb), NULL);

	create_toolbars (app);
	bar = gnome_appbar_new (FALSE, TRUE, GNOME_PREFERENCES_USER);
	gnome_app_set_statusbar (GNOME_APP (app), bar);
	gnome_app_create_menus (GNOME_APP (app), main_menu);

	/* Disable back and forward on the Go menu */
	gtk_widget_set_sensitive(go_menu[0].widget, FALSE);
	gtk_widget_set_sensitive(go_menu[1].widget, FALSE);

	gnome_app_install_menu_hints (GNOME_APP (app), main_menu);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	gnome_app_set_contents (GNOME_APP (app), scrolled_window);

	html_widget = gtk_html_new ();
	html = GTK_HTML (html_widget);
	gtk_html_load_empty (html);
	/* gtk_html_set_default_background_color (GTK_HTML (html_widget), &bgColor); */
	/* gtk_html_set_editable (GTK_HTML (html_widget), TRUE); */
	
	gtk_container_add (GTK_CONTAINER (scrolled_window), html_widget);

	/* Create a popup menu with disabled back and forward items */
	popup_menu = gtk_menu_new();

	popup_menu_back = gtk_menu_item_new_with_label("Back");
	gtk_widget_set_sensitive(popup_menu_back, FALSE);
	gtk_menu_append(GTK_MENU(popup_menu), popup_menu_back);
	gtk_widget_show(popup_menu_back);
	gtk_signal_connect (GTK_OBJECT (popup_menu_back), "activate",
			    GTK_SIGNAL_FUNC (back_cb), NULL);

	popup_menu_forward = gtk_menu_item_new_with_label("Forward");
	gtk_widget_set_sensitive(popup_menu_forward, FALSE);
	gtk_menu_append(GTK_MENU(popup_menu), popup_menu_forward);
	gtk_widget_show(popup_menu_forward);
	gtk_signal_connect (GTK_OBJECT (popup_menu_forward), "activate",
			    GTK_SIGNAL_FUNC (forward_cb), NULL);

	popup_menu_home = gtk_menu_item_new_with_label("Home");
	gtk_menu_append(GTK_MENU(popup_menu), popup_menu_home);
	gtk_widget_show(popup_menu_home);
	gtk_signal_connect (GTK_OBJECT (popup_menu_home), "activate",
			    GTK_SIGNAL_FUNC (home_cb), NULL);

	/* End of menu creation */

	gtk_widget_set_events (html_widget, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

	gtk_signal_connect (GTK_OBJECT (html), "title_changed",
			    GTK_SIGNAL_FUNC (title_changed_cb), (gpointer)app);
	gtk_signal_connect (GTK_OBJECT (html), "url_requested",
			    GTK_SIGNAL_FUNC (url_requested), (gpointer)app);
	gtk_signal_connect (GTK_OBJECT (html), "load_done",
			    GTK_SIGNAL_FUNC (load_done), (gpointer)app);
	gtk_signal_connect (GTK_OBJECT (html), "on_url",
			    GTK_SIGNAL_FUNC (on_url), (gpointer)app);
	gtk_signal_connect (GTK_OBJECT (html), "set_base",
			    GTK_SIGNAL_FUNC (on_set_base), (gpointer)app);
	gtk_signal_connect (GTK_OBJECT (html), "button_press_event",
			    GTK_SIGNAL_FUNC (on_button_press_event), popup_menu);
	gtk_signal_connect (GTK_OBJECT (html), "link_clicked",
			    GTK_SIGNAL_FUNC (on_link_clicked), NULL);
	gtk_signal_connect (GTK_OBJECT (html), "redirect",
			    GTK_SIGNAL_FUNC (on_redirect), NULL);
	gtk_signal_connect (GTK_OBJECT (html), "submit",
			    GTK_SIGNAL_FUNC (on_submit), NULL);
	gtk_signal_connect (GTK_OBJECT (html), "object_requested",
			    GTK_SIGNAL_FUNC (object_requested_cmd), NULL);

#if 0
	gtk_box_pack_start_defaults (GTK_BOX (hbox), GTK_WIDGET (html));
	vscrollbar = gtk_vscrollbar_new (GTK_LAYOUT (html)->vadjustment);
	gtk_box_pack_start (GTK_BOX (hbox), vscrollbar, FALSE, TRUE, 0);
	
#endif
	gtk_widget_realize (GTK_WIDGET (html));

	gtk_window_set_default_size (GTK_WINDOW (app), 540, 400);
	gtk_window_set_focus (GTK_WINDOW (app), GTK_WIDGET (html));

	gtk_widget_show_all (app);

	{
		const char **args;
		args = poptGetArgs(ctx);

		if (args && args[0])
			goto_url (args[0], 0);
	}

	gtk_main ();

#ifdef MEMDEBUG

	/* gtk_widget_unref (html_widget); */
	free (p);
#endif

	return 0;
}
