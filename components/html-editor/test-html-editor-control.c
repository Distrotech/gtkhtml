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

    Authors: Ettore Perazzoli <ettore@helixcode.com>
*/

#include <config.h>

#include <gnome.h>
#include <bonobo.h>

#include <Editor.h>
#include "editor-control-factory.h"

GtkWidget *control;
gint formatHTML = 1;


/* Saving/loading through PersistStream.  */

static void
load_through_persist_stream (const gchar *filename,
			     Bonobo_PersistStream pstream)
{
	BonoboStream *stream = NULL;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	/* FIX2 stream = bonobo_stream_open (BONOBO_IO_DRIVER_FS, filename,
	   Bonobo_Storage_READ, 0); */

	if (stream == NULL) {
		g_warning ("Couldn't load `%s'\n", filename);
	} else {
		BonoboObject *stream_object;
		Bonobo_Stream corba_stream;

		stream_object = BONOBO_OBJECT (stream);
		corba_stream = bonobo_object_corba_objref (stream_object);
		Bonobo_PersistStream_load (pstream, corba_stream,
					   "text/html", &ev);
	}

	Bonobo_Unknown_unref (pstream, &ev);
	CORBA_Object_release (pstream, &ev);

	CORBA_exception_free (&ev);
}

static void
save_through_persist_stream (const gchar *filename,
			     Bonobo_PersistStream pstream)
{
	BonoboStream *stream = NULL;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	/* FIX2 stream = bonobo_stream_open (BONOBO_IO_DRIVER_FS, filename,
				     Bonobo_Storage_WRITE |
				     Bonobo_Storage_CREATE |
				     Bonobo_Storage_FAILIFEXIST, 0); */

	if (stream == NULL) {
		g_warning ("Couldn't create `%s'\n", filename);
	} else {
		BonoboObject *stream_object;
		Bonobo_Stream corba_stream;

		stream_object = BONOBO_OBJECT (stream);
		corba_stream = bonobo_object_corba_objref (stream_object);
		Bonobo_PersistStream_save (pstream, corba_stream,
					   "text/html", &ev);
	}

	Bonobo_Unknown_unref (pstream, &ev);
	CORBA_Object_release (pstream, &ev);

	CORBA_exception_free (&ev);
}

static void
save_through_plain_persist_stream (const gchar *filename,
			     Bonobo_PersistStream pstream)
{
	BonoboStream *stream = NULL;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	/* FIX2 stream = bonobo_stream_open (BONOBO_IO_DRIVER_FS, filename,
				     Bonobo_Storage_WRITE |
				     Bonobo_Storage_CREATE |
				     Bonobo_Storage_FAILIFEXIST, 0); */

	if (stream == NULL) {
		g_warning ("Couldn't create `%s'\n", filename);
	} else {
		BonoboObject *stream_object;
		Bonobo_Stream corba_stream;

		stream_object = BONOBO_OBJECT (stream);
		corba_stream = bonobo_object_corba_objref (stream_object);
		Bonobo_PersistStream_save (pstream, corba_stream,
					   "text/plain", &ev);
	}

	Bonobo_Unknown_unref (pstream, &ev);
	CORBA_Object_release (pstream, &ev);

	CORBA_exception_free (&ev);
}


/* Loading/saving through PersistFile.  */

static void
load_through_persist_file (const gchar *filename,
			   Bonobo_PersistFile pfile)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	Bonobo_PersistFile_load (pfile, filename, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_warning ("Cannot load.");

	CORBA_exception_free (&ev);
}

static void
save_through_persist_file (const gchar *filename,
			   Bonobo_PersistFile pfile)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	Bonobo_PersistFile_save (pfile, filename, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_warning ("Cannot save.");

	CORBA_exception_free (&ev);
}


/* Common file selection widget.  We make sure there is only one open at a
   given time.  */

enum _FileSelectionOperation {
	OP_NONE,
	OP_SAVE_THROUGH_PERSIST_STREAM,
	OP_SAVE_THROUGH_PLAIN_PERSIST_STREAM,
	OP_LOAD_THROUGH_PERSIST_STREAM,
	OP_SAVE_THROUGH_PERSIST_FILE,
	OP_LOAD_THROUGH_PERSIST_FILE
};
typedef enum _FileSelectionOperation FileSelectionOperation;

struct _FileSelectionInfo {
	BonoboWidget *control;
	GtkWidget *widget;

	FileSelectionOperation operation;
};
typedef struct _FileSelectionInfo FileSelectionInfo;

static FileSelectionInfo file_selection_info = {
	NULL,
	NULL,
	OP_NONE
};

static void
file_selection_destroy_cb (GtkWidget *widget,
			   gpointer data)
{
	file_selection_info.widget = NULL;
}

static void
file_selection_ok_cb (GtkWidget *widget,
		      gpointer data)
{
	CORBA_Object interface;
	const gchar *interface_name;
	CORBA_Environment ev;

	if (file_selection_info.operation == OP_SAVE_THROUGH_PERSIST_FILE
	    || file_selection_info.operation == OP_LOAD_THROUGH_PERSIST_FILE)
		interface_name = "IDL:Bonobo/PersistFile:1.0";
	else
		interface_name = "IDL:Bonobo/PersistStream:1.0";

	CORBA_exception_init (&ev);
	interface = Bonobo_Unknown_queryInterface (bonobo_widget_get_objref (file_selection_info.control),
						   interface_name, &ev);
	CORBA_exception_free (&ev);

	if (interface == CORBA_OBJECT_NIL) {
		g_warning ("The Control does not seem to support `%s'.", interface_name);
	} else 	 {
		const gchar *fname;
	
		fname = gtk_file_selection_get_filename
			(GTK_FILE_SELECTION (file_selection_info.widget));

		switch (file_selection_info.operation) {
		case OP_LOAD_THROUGH_PERSIST_STREAM:
			load_through_persist_stream (fname, interface);
			break;
		case OP_SAVE_THROUGH_PERSIST_STREAM:
			save_through_persist_stream (fname, interface);
			break;
		case OP_SAVE_THROUGH_PLAIN_PERSIST_STREAM:
			save_through_plain_persist_stream (fname, interface);
			break;
		case OP_LOAD_THROUGH_PERSIST_FILE:
			load_through_persist_file (fname, interface);
			break;
		case OP_SAVE_THROUGH_PERSIST_FILE:
			save_through_persist_file (fname, interface);
			break;
		default:
			g_assert_not_reached ();
		}
	} 

	gtk_widget_destroy (file_selection_info.widget);
}


static void
open_or_save_as_dialog (BonoboWindow *app,
			FileSelectionOperation op)
{
	GtkWidget    *widget;
	BonoboWidget *control;

	control = BONOBO_WIDGET (bonobo_window_get_contents (app));

	if (file_selection_info.widget != NULL) {
		gdk_window_show (GTK_WIDGET (file_selection_info.widget)->window);
		return;
	}

	if (op == OP_LOAD_THROUGH_PERSIST_FILE || op == OP_LOAD_THROUGH_PERSIST_STREAM)
		widget = gtk_file_selection_new (_("Open file..."));
	else
		widget = gtk_file_selection_new (_("Save file as..."));

	gtk_window_set_transient_for (GTK_WINDOW (widget),
				      GTK_WINDOW (app));

	file_selection_info.widget = widget;
	file_selection_info.control = control;
	file_selection_info.operation = op;

	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (widget)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
				   GTK_OBJECT (widget));

	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (widget)->ok_button),
			    "clicked", GTK_SIGNAL_FUNC (file_selection_ok_cb),
			    NULL);

	gtk_signal_connect (GTK_OBJECT (file_selection_info.widget), "destroy",
			    GTK_SIGNAL_FUNC (file_selection_destroy_cb),
			    NULL);

	gtk_widget_show (file_selection_info.widget);
}

/* "Open through persist stream" dialog.  */
static void
open_through_persist_stream_cb (GtkWidget *widget,
				gpointer data)
{
	open_or_save_as_dialog (BONOBO_WINDOW (data), OP_LOAD_THROUGH_PERSIST_STREAM);
}

/* "Save through persist stream" dialog.  */
static void
save_through_persist_stream_cb (GtkWidget *widget,
				gpointer data)
{
	open_or_save_as_dialog (BONOBO_WINDOW (data), OP_SAVE_THROUGH_PERSIST_STREAM);
}

/* "Save through persist stream" dialog.  */
static void
save_through_plain_persist_stream_cb (GtkWidget *widget,
				gpointer data)
{
	open_or_save_as_dialog (BONOBO_WINDOW (data), OP_SAVE_THROUGH_PLAIN_PERSIST_STREAM);
}

/* "Open through persist file" dialog.  */
static void
open_through_persist_file_cb (GtkWidget *widget,
			      gpointer data)
{
	open_or_save_as_dialog (BONOBO_WINDOW (data), OP_LOAD_THROUGH_PERSIST_FILE);
}

/* "Save through persist file" dialog.  */
static void
save_through_persist_file_cb (GtkWidget *widget,
			      gpointer data)
{
	open_or_save_as_dialog (BONOBO_WINDOW (data), OP_SAVE_THROUGH_PERSIST_FILE);
}


static void
exit_cb (GtkWidget *widget,
	 gpointer data)
{
	gtk_widget_destroy (GTK_WIDGET (data));
}



static BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("OpenFile",   open_through_persist_file_cb),
	BONOBO_UI_UNSAFE_VERB ("SaveFile",   save_through_persist_file_cb),
	BONOBO_UI_UNSAFE_VERB ("OpenStream", open_through_persist_stream_cb),
	BONOBO_UI_UNSAFE_VERB ("SaveStream", save_through_persist_stream_cb),
	BONOBO_UI_UNSAFE_VERB ("SavePlainStream", save_through_plain_persist_stream_cb),

	BONOBO_UI_UNSAFE_VERB ("FileExit", exit_cb),

	BONOBO_UI_VERB_END
};

static void
menu_format_html_cb (BonoboUIComponent           *component,
		     const char                  *path,
		     Bonobo_UIComponent_EventType type,
		     const char                  *state,
		     gpointer                     user_data)

{
	BonoboArg *arg;
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	formatHTML = *state == '0' ? 0 : 1;

	arg = bonobo_arg_new_from (BONOBO_ARG_BOOLEAN, &formatHTML);
	bonobo_widget_set_property (BONOBO_WIDGET (user_data), "FormatHTML", arg, NULL);
	bonobo_arg_release (arg);

	arg = bonobo_arg_new_from (BONOBO_ARG_STRING, "testing");
	bonobo_widget_set_property (BONOBO_WIDGET (user_data), "HTMLTitle", arg, NULL);
	bonobo_arg_release (arg);
}

/* A dirty, non-translatable hack */
static char ui [] = 
"<Root>"
"	<commands>"
"	        <cmd name=\"FileExit\" _label=\"Exit\" _tip=\"Exit the program\""
"	         pixtype=\"stock\" pixname=\"Exit\" accel=\"*Control*q\"/>"
"	        <cmd name=\"FormatHTML\" _label=\"HTML mode\" type=\"toggle\" _tip=\"HTML Format switch\"/>"
"	</commands>"
"	<menu>"
"		<submenu name=\"File\" _label=\"_File\">"
"			<menuitem name=\"OpenFile\" verb=\"\" _label=\"Open (PersistFile)\" _tip=\"Open using the PersistFile interface\""
"			pixtype=\"stock\" pixname=\"Open\"/>"
"			<menuitem name=\"SaveFile\" verb=\"\" _label=\"Save (PersistFile)\" _tip=\"Save using the PersistFile interface\""
"			pixtype=\"stock\" pixname=\"Save\"/>"
"			<separator/>"
"			<menuitem name=\"OpenStream\" verb=\"\" _label=\"_Open (PersistStream)\" _tip=\"Open using the PersistStream interface\""
"			pixtype=\"stock\" pixname=\"Open\"/>"
"			<menuitem name=\"SaveStream\" verb=\"\" _label=\"_Save (PersistStream)\" _tip=\"Save using the PersistStream interface\""
"			pixtype=\"stock\" pixname=\"Save\"/>"
"			<menuitem name=\"SavePlainStream\" verb=\"\" _label=\"Save _plain(PersistStream)\" _tip=\"Save using the PersistStream interface\""
"			pixtype=\"stock\" pixname=\"Save\"/>"
"			<separator/>"
"			<menuitem name=\"FileExit\" verb=\"\" _label=\"E_xit\"/>"
"		</submenu>"
"		<placeholder name=\"Component\"/>"
"		<submenu name=\"Format\" _label=\"For_mat\">"
"			<menuitem name=\"FormatHTML\" verb=\"\"/>"
"                       <separator/>"
"                       <placeholder name=\"FormatParagraph\"/>"
"               </submenu>"
"	</menu>"
"	<dockitem name=\"Toolbar\" behavior=\"exclusive\">"
"	</dockitem>"
"</Root>";


static void
app_destroy_cb (GtkWidget *app, BonoboUIContainer *uic)
{
	bonobo_object_unref (BONOBO_OBJECT (uic));

	gtk_main_quit ();
}

static int
app_delete_cb (GtkWidget *widget, GdkEvent *event, gpointer dummy)
{
	gtk_widget_destroy (GTK_WIDGET (widget));

	return FALSE;
}

static guint
container_create (void)
{
	GtkWidget *win;
	GtkWindow *window;
	BonoboUIComponent *component;
	BonoboUIContainer *container;
	CORBA_Environment ev;
	GNOME_GtkHTML_Editor_Engine engine;
	BonoboArg *arg;

	win = bonobo_window_new ("test-editor",
				 "HTML Editor Control Test");
	window = GTK_WINDOW (win);

	container = bonobo_window_get_ui_container (BONOBO_WINDOW (win));

	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			    GTK_SIGNAL_FUNC (app_delete_cb), NULL);

	gtk_signal_connect (GTK_OBJECT (window), "destroy",
			    GTK_SIGNAL_FUNC (app_destroy_cb), container);

	gtk_window_set_default_size (window, 500, 440);
	gtk_window_set_policy (window, TRUE, TRUE, FALSE);

	component = bonobo_ui_component_new ("test-editor");

	bonobo_ui_component_set_container (component, BONOBO_OBJREF (container), NULL);
	bonobo_ui_component_add_verb_list_with_data (component, verbs, win);
	bonobo_ui_component_set_translate (component, "/", ui, NULL);

	control = bonobo_widget_new_control (CONTROL_ID, BONOBO_OBJREF (container));

	if (control == NULL)
		g_error ("Cannot get `%s'.", CONTROL_ID);

	arg = bonobo_arg_new_from (BONOBO_ARG_BOOLEAN, &formatHTML);
	bonobo_widget_set_property (BONOBO_WIDGET (control), "FormatHTML", arg, NULL);
	bonobo_arg_release (arg);
	bonobo_ui_component_set_prop (component, "/commands/FormatHTML", "state", formatHTML ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (component, "FormatHTML", menu_format_html_cb, control);

	bonobo_window_set_contents (BONOBO_WINDOW (win), control);

	gtk_widget_show_all (GTK_WIDGET (window));

	CORBA_exception_init (&ev);
	engine = (GNOME_GtkHTML_Editor_Engine) Bonobo_Unknown_queryInterface
		(bonobo_widget_get_objref (BONOBO_WIDGET (control)), "IDL:GNOME/GtkHTML/Editor/Engine:1.0", &ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (engine, "grab-focus", &ev);
	Bonobo_Unknown_unref (engine, &ev);
	CORBA_Object_release (engine, &ev);
	CORBA_exception_free (&ev);

	return FALSE;
}

static gint
load_file (const gchar *fname)
{
	CORBA_Object interface;
	CORBA_Environment ev;

	printf ("loading: %s\n", fname);
	CORBA_exception_init (&ev);
	interface = Bonobo_Unknown_queryInterface (bonobo_widget_get_objref (BONOBO_WIDGET (control)),
						   "IDL:Bonobo/PersistFile:1.0", &ev);
	CORBA_exception_free (&ev);
	load_through_persist_file (fname, interface);

	return FALSE;
}

int
main (int argc, char **argv)
{
	bindtextdomain(PACKAGE, GNOMELOCALEDIR);
	textdomain(PACKAGE);
	
	gnome_init ("test-gnome-gtkhtml-editor", "1.0", argc, argv);
	if (bonobo_init (&argc, argv) == FALSE)
		g_error ("Could not initialize Bonobo\n");
	bonobo_activate ();

	/* We can't make any CORBA calls unless we're in the main loop.  So we
	   delay creating the container here. */
	gtk_idle_add ((GtkFunction) container_create, NULL);
	if (argc > 1 && *argv [argc - 1] != '-')
		gtk_idle_add ((GtkFunction) load_file, argv [argc - 1]);

	bonobo_main ();

	return 0;
}
