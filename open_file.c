/*
 *
 *  Copyright (C) 2016  Leif Persson <leifmariposa@hotmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <glib.h>
#include <glib/gprintf.h>
#include <geanyplugin.h>
#include <libgen.h>

#ifdef WIN32
#	include <windows.h>
#	include <sys/types.h>
#	include <dirent.h>
#	define PATH_SEPARATOR '\\'
#	define DEFAULT_PATTERN "*.*"
#else
#	include <fnmatch.h>
#	include <wordexp.h>
#	define DEFAULT_PATTERN "*"
#	define PATH_SEPARATOR '/'
#endif


#define D(x) /*x*/


/**********************************************************************/
static const char *PLUGIN_NAME = "Open File";
static const char *PLUGIN_CONF_DIRECORY = "open_file";
static const char *PLUGIN_CONF_FILE_NAME = "open_file.conf";
static const char *PLUGIN_DESCRIPTION = "Open a file from preconfigured locations";
static const char *PLUGIN_VERSION = "0.1";
static const char *PLUGIN_AUTHOR = "Leif Persson <leifmariposa@hotmail.com>";
static const char *PLUGIN_KEY_NAME = "open_file";
static const int   WINDOW_WIDTH = 650;
static const int   WINDOW_HEIGHT = 500;
static const char *LOCATIONS = "locations";
static const char *PATHS = "paths";
static const char *PATTERNS = "patterns";


/**********************************************************************/
GeanyPlugin *geany_plugin;

static GtkListStore *list_store;

/**********************************************************************/
enum
{
	KB_GOTO_OPEN_FILE,
	KB_COUNT
};


/**********************************************************************/
enum
{
	COLUMN_OPEN_FILE_SHORT_NAME = 0,
	COLUMN_OPEN_FILE_PATH = 1,
	OPEN_FILE_COLUMN_COUNT
};

/**********************************************************************/
typedef enum
{
	COLUMN_CONFIG_PATH = 0,
	COLUMN_CONFIG_PATTERN,
	CONFIG_COLUMN_COUNT
} Column;

/**********************************************************************/
struct PLUGIN_DATA
{
	GtkWidget           *main_window;
	GtkWidget           *text_entry;
	GtkWidget           *tree_view;
	GtkTreeSelection    *selection;
	GtkTreeModel        *model;
	GtkTreeModel        *filter;
	GtkTreeModel        *sorted;
	const gchar         *text_value;
	GtkWidget           *cancel_button;
	GtkWidget           *open_button;
} PLUGIN_DATA;

/**********************************************************************/
typedef struct
{
	gchar* path;
	gchar* pattern;
} Location;


static GtkWidget *configure(GeanyPlugin *plugin, GtkDialog *parent, gpointer pdata);
static GSList* load_configuration(void);
static void clear_configuration(GSList* locations);

/**********************************************************************/
D(static void log_debug(const gchar* s, ...)
{
	gchar* format = g_strconcat("[CTR DEBUG] : ", s, "\n", NULL);
	va_list l;
	va_start(l, s);
	g_vprintf(format, l);
	g_free(format);
	va_end(l);
})

#if defined (WIN32)

/**********************************************************************/
static void list_files_in_dir(GtkListStore *store, const char *path, const char *pattern)
{
	WIN32_FIND_DATA ff;

	D(log_debug("%s:%s - path: %s, pattern: %s", __FILE__, __FUNCTION__, path, pattern));

	gchar *full_path = g_build_filename(path, pattern, NULL);

	HANDLE findhandle = FindFirstFile(full_path, &ff);
	if(findhandle != INVALID_HANDLE_VALUE)
	{
		do
		{
			if(!(ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				GtkTreeIter tree_iter;
				gtk_list_store_append(store, &tree_iter);
				gchar *file_name = g_locale_to_utf8(ff.cFileName, -1, NULL, NULL, NULL);
				gchar *path_name = g_locale_to_utf8(path, -1, NULL, NULL, NULL);
				gtk_list_store_set(store, &tree_iter, COLUMN_OPEN_FILE_SHORT_NAME, file_name, -1);
				gtk_list_store_set(store, &tree_iter, COLUMN_OPEN_FILE_PATH, path_name, -1);
				g_free(file_name);
				g_free(path_name);
			}

		}while(FindNextFile(findhandle, &ff));

		FindClose(findhandle);
	}
	g_free(full_path);
}

/**********************************************************************/
static void list_directory(GtkListStore *store, const char *path, const char *pattern)
{
	WIN32_FIND_DATA ff;

	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	list_files_in_dir(store, path, pattern);

	gchar *full_path = g_build_filename(path, "*.*", NULL);
	HANDLE findhandle = FindFirstFile(full_path, &ff);
	if(findhandle != INVALID_HANDLE_VALUE)
	{
		do
		{
			if((ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && g_strcmp0(ff.cFileName, ".") != 0 && g_strcmp0(ff.cFileName, "..") != 0)
			{
				gchar *new_path = g_build_filename(path, ff.cFileName, NULL);
				list_directory(store, new_path, pattern);
				g_free(new_path);
			}

		}while(FindNextFile(findhandle, &ff));

		FindClose(findhandle);
	}

	g_free(full_path);
}

#else

/**********************************************************************/
static void list_directory(GtkListStore *store, const char *path, const char *pattern)
{
	DIR *dir;
	struct dirent *entry;

	if(!(dir = opendir(path)))
		return;

	if(!(entry = readdir(dir)))
		return;

	do
	{
		if(entry->d_type == DT_DIR)
		{
			if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
				continue;

			gchar *new_path = g_build_filename(path, entry->d_name, NULL);
			list_directory(store, new_path, pattern);
			g_free(new_path);
		}
		else
		{
			if((fnmatch(pattern, entry->d_name, 0)) == 0)
			{
				GtkTreeIter tree_iter;
				gtk_list_store_append(store, &tree_iter);
				gtk_list_store_set(store, &tree_iter, COLUMN_OPEN_FILE_SHORT_NAME, entry->d_name, -1);
				gtk_list_store_set(store, &tree_iter, COLUMN_OPEN_FILE_PATH, path, -1);
			}
		}
	} while((entry = readdir(dir)));
	closedir(dir);
}
#endif

/**********************************************************************/
static GtkTreeModel* get_files()
{
	GtkListStore *store = gtk_list_store_new(OPEN_FILE_COLUMN_COUNT,
																					 G_TYPE_STRING,
																					 G_TYPE_STRING);

	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	GSList *iter;
	GSList *locations = load_configuration();
	for(iter = locations; iter != NULL; iter = iter->next)
	{
		Location *location = (Location*)iter->data;
#ifdef WIN32
		list_directory(store, location->path, location->pattern);
#else
		wordexp_t expanded_path;
		wordexp(location->path, &expanded_path, 0);
		list_directory(store, expanded_path.we_wordv[0], location->pattern);
		wordfree(&expanded_path);
#endif
	}
    clear_configuration(locations);

	return GTK_TREE_MODEL(store);
}


/**********************************************************************/
static gboolean count(G_GNUC_UNUSED GtkTreeModel *model,
											G_GNUC_UNUSED GtkTreePath *path,
											G_GNUC_UNUSED GtkTreeIter *iter,
											gint *no_rows )
{
	(*no_rows)++;

	return FALSE;
}


/**********************************************************************/
void select_first_row(struct PLUGIN_DATA *plugin_data)
{
	GtkTreePath *path = gtk_tree_path_new_from_indices(0, -1);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(plugin_data->tree_view), path, NULL, FALSE);
	gtk_tree_path_free(path);
}


/**********************************************************************/
static int on_update_visibilty_elements(G_GNUC_UNUSED GtkWidget *widget, struct PLUGIN_DATA *plugin_data)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	plugin_data->text_value = gtk_entry_get_text(GTK_ENTRY(plugin_data->text_entry));

	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(plugin_data->filter));

	gint total_rows = 0;
	gint filtered_rows = 0;
	gtk_tree_model_foreach(plugin_data->model, (GtkTreeModelForeachFunc)count, &total_rows);
	gtk_tree_model_foreach(plugin_data->filter, (GtkTreeModelForeachFunc)count, &filtered_rows);
	gchar buf[20];
	g_sprintf(buf, "%s %d/%d", PLUGIN_NAME, filtered_rows, total_rows);
	gtk_window_set_title(GTK_WINDOW(plugin_data->main_window), buf);

	select_first_row(plugin_data);

	gtk_widget_set_sensitive(plugin_data->open_button, filtered_rows > 0);

	return 0;
}


/**********************************************************************/
static gboolean row_visible(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	struct PLUGIN_DATA *plugin_data = data;
	gchar *short_name;
	gboolean visible = FALSE;

	gtk_tree_model_get(model, iter, COLUMN_OPEN_FILE_SHORT_NAME, &short_name, -1);
	const gchar *text_value = plugin_data->text_value;

	if (!text_value || g_strcmp0(text_value, "") == 0 || (short_name && g_str_match_string(text_value, short_name, TRUE)))
		visible = TRUE;

	g_free(short_name);

	return visible;
}


/**********************************************************************/
void activate_selected_file_and_quit(struct PLUGIN_DATA *plugin_data)
{
	GtkTreePath *tree_path = NULL;

	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	gtk_tree_view_get_cursor(GTK_TREE_VIEW(plugin_data->tree_view), &tree_path, NULL);
	if (tree_path)
	{
		GtkTreeIter iter;
		GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(plugin_data->tree_view));
		if (gtk_tree_model_get_iter(model, &iter, tree_path))
		{
			gchar *short_name = NULL;
			gchar *path = NULL;
			gtk_tree_model_get(model, &iter,
				COLUMN_OPEN_FILE_SHORT_NAME, &short_name,
				COLUMN_OPEN_FILE_PATH, &path,
				-1);

			if(short_name != NULL && path != NULL)
			{
				gchar *full_path = g_build_filename(path, short_name, NULL);
				document_open_file(full_path, FALSE, NULL, NULL);
				g_free(full_path);
			}
			g_free(short_name);
			g_free(path);
		}
		gtk_tree_path_free(tree_path);
	}

	gtk_widget_destroy(plugin_data->main_window);
	g_free(plugin_data);
}


/**********************************************************************/
void view_on_row_activated(G_GNUC_UNUSED GtkTreeView *treeview,
													 G_GNUC_UNUSED GtkTreePath *path,
													 G_GNUC_UNUSED GtkTreeViewColumn *col,
													 gpointer data)
{
	struct PLUGIN_DATA *plugin_data = data;

	activate_selected_file_and_quit(plugin_data);
}


/**********************************************************************/
static void create_tree_view(struct PLUGIN_DATA *plugin_data)
{
	GtkTreeViewColumn *filename_column;
	GtkTreeViewColumn *path_column;

	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	plugin_data->model = get_files();

	plugin_data->filter = gtk_tree_model_filter_new(plugin_data->model, NULL);
	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(plugin_data->filter), row_visible, plugin_data, NULL);

	plugin_data->sorted = gtk_tree_model_sort_new_with_model(plugin_data->filter);

	plugin_data->tree_view = gtk_tree_view_new_with_model(plugin_data->sorted);
	g_signal_connect(plugin_data->tree_view, "row-activated", (GCallback) view_on_row_activated, plugin_data);

	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(plugin_data->tree_view ), -1, "File name", renderer, "text", COLUMN_OPEN_FILE_SHORT_NAME, NULL);
	filename_column = gtk_tree_view_get_column(GTK_TREE_VIEW(plugin_data->tree_view ), COLUMN_OPEN_FILE_SHORT_NAME);
	gtk_tree_view_column_set_sort_column_id(filename_column, COLUMN_OPEN_FILE_SHORT_NAME);
	gtk_tree_view_column_set_max_width(filename_column, WINDOW_WIDTH * 2 / 3);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_MIDDLE, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(plugin_data->tree_view ), -1, "Path", renderer, "text", COLUMN_OPEN_FILE_PATH, NULL);
	path_column = gtk_tree_view_get_column(GTK_TREE_VIEW(plugin_data->tree_view ), COLUMN_OPEN_FILE_PATH);
	gtk_tree_view_column_set_sort_column_id(path_column, COLUMN_OPEN_FILE_PATH);
	gtk_tree_view_column_set_max_width(path_column, WINDOW_WIDTH * 2 / 3);

	/* Trigger a sort */
	gtk_tree_view_column_clicked(filename_column);
}


/**********************************************************************/
static void close_plugin(struct PLUGIN_DATA *plugin_data)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	gtk_widget_destroy(plugin_data->main_window);
	g_free(plugin_data);
}


/**********************************************************************/
static gboolean on_key_press(G_GNUC_UNUSED GtkWidget *widget,
														 GdkEventKey *event,
														 struct PLUGIN_DATA *plugin_data)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	switch(event->keyval)
	{
	case 0xff0d: /* GDK_Return */
		activate_selected_file_and_quit(plugin_data);
		break;
	case 65307: /* Escape */
		close_plugin(plugin_data);
		break;
	case 0xff54: /* GDK_Down */
		gtk_widget_grab_focus(plugin_data->tree_view);
		break;
	default:
		return FALSE;
	}

	return FALSE;
}


/**********************************************************************/
static void on_cancel_button(G_GNUC_UNUSED GtkButton *button, struct PLUGIN_DATA *plugin_data)
{
	close_plugin(plugin_data);
}


/**********************************************************************/
static void on_open_button(G_GNUC_UNUSED GtkButton *button, struct PLUGIN_DATA *plugin_data)
{
	activate_selected_file_and_quit(plugin_data);
}


/**********************************************************************/
static gboolean on_quit(G_GNUC_UNUSED GtkWidget *widget,
											  G_GNUC_UNUSED GdkEvent *event,
											  G_GNUC_UNUSED gpointer   data)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	return FALSE;
}


/**********************************************************************/
int launch_widget(void)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	struct PLUGIN_DATA *plugin_data =  g_malloc(sizeof(PLUGIN_DATA));
	memset(plugin_data, 0, sizeof(PLUGIN_DATA));

	plugin_data->main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_modal(GTK_WINDOW(plugin_data->main_window), TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(plugin_data->main_window), 5);

	create_tree_view(plugin_data);

	GtkWidget *main_grid = gtk_table_new(2, 1, FALSE);

	gtk_table_set_row_spacings(GTK_TABLE(main_grid), 8);
	gtk_table_set_col_spacings(GTK_TABLE(main_grid), 0);

	plugin_data->text_entry = gtk_entry_new();
	g_signal_connect(plugin_data->text_entry, "changed", G_CALLBACK(on_update_visibilty_elements), plugin_data);
	gtk_table_attach(GTK_TABLE(main_grid), plugin_data->text_entry, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);

	GtkWidget *scrolled_file_list_window = gtk_scrolled_window_new(NULL,NULL);
	gtk_container_add(GTK_CONTAINER(scrolled_file_list_window), plugin_data->tree_view );
	gtk_table_attach_defaults(GTK_TABLE(main_grid), scrolled_file_list_window, 0, 1, 1, 2);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_file_list_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_window_set_title(GTK_WINDOW(plugin_data->main_window), PLUGIN_NAME);
	gtk_widget_set_size_request(plugin_data->main_window, WINDOW_WIDTH, WINDOW_HEIGHT);
	gtk_window_set_position(GTK_WINDOW(plugin_data->main_window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(plugin_data->main_window), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(plugin_data->main_window), GTK_WINDOW (geany_plugin->geany_data->main_widgets->window));
	g_signal_connect(plugin_data->main_window, "delete_event", G_CALLBACK(on_quit), plugin_data);
	g_signal_connect(plugin_data->main_window, "key-press-event", G_CALLBACK(on_key_press), plugin_data);

	/* Buttons */
	GtkWidget *bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);

	plugin_data->cancel_button = gtk_button_new_with_mnemonic(_("_Cancel"));
	gtk_container_add(GTK_CONTAINER(bbox), plugin_data->cancel_button);
	g_signal_connect(plugin_data->cancel_button, "clicked", G_CALLBACK(on_cancel_button), plugin_data);

	plugin_data->open_button = gtk_button_new_with_mnemonic(_("_Open"));
	gtk_container_add(GTK_CONTAINER(bbox), plugin_data->open_button);
	g_signal_connect(plugin_data->open_button, "clicked", G_CALLBACK(on_open_button), plugin_data);

	gtk_table_attach(GTK_TABLE(main_grid), bbox, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);

	gtk_container_add(GTK_CONTAINER(plugin_data->main_window), main_grid);
	gtk_widget_show_all(plugin_data->main_window);

	select_first_row(plugin_data);
	on_update_visibilty_elements(plugin_data->main_window, plugin_data);

	return 0;
}


/**********************************************************************/
static void item_activate_cb(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	launch_widget();
}


/**********************************************************************/
static void kb_activate(G_GNUC_UNUSED guint key_id)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	launch_widget();
}


/**********************************************************************/
static gboolean init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	GtkWidget* edit_menu = ui_lookup_widget(plugin->geany_data->main_widgets->window, "edit1_menu");

	GtkWidget *main_menu_item;
	/* Create a new menu item and show it */
	main_menu_item = gtk_menu_item_new_with_mnemonic(PLUGIN_NAME);
	gtk_widget_show(main_menu_item);
	gtk_container_add(GTK_CONTAINER(edit_menu), main_menu_item);

	GeanyKeyGroup *key_group = plugin_set_key_group(plugin, PLUGIN_KEY_NAME, KB_COUNT, NULL);
	keybindings_set_item(key_group, KB_GOTO_OPEN_FILE, kb_activate, 0, 0, PLUGIN_KEY_NAME, PLUGIN_NAME, main_menu_item);

	g_signal_connect(main_menu_item, "activate", G_CALLBACK(item_activate_cb), NULL);
	geany_plugin_set_data(plugin, main_menu_item, NULL);

	return TRUE;
}


/**********************************************************************/
static void cleanup(G_GNUC_UNUSED GeanyPlugin *plugin, gpointer pdata)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	GtkWidget *main_menu_item = (GtkWidget*)pdata;
	gtk_widget_destroy(main_menu_item);
}


/**********************************************************************/
G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	geany_plugin = plugin;
	plugin->info->name = PLUGIN_NAME;
	plugin->info->description = PLUGIN_DESCRIPTION;
	plugin->info->version = PLUGIN_VERSION;
	plugin->info->author = PLUGIN_AUTHOR;
	plugin->funcs->init = init;
	plugin->funcs->cleanup = cleanup;
	plugin->funcs->configure = configure;
	GEANY_PLUGIN_REGISTER(plugin, 225);
}

/**********************************************************************/
static GSList* load_configuration(void)
{
	GKeyFile *config = NULL;
	gchar *config_filename = NULL;
	gchar **path_list  = NULL;
	gchar **pattern_list  = NULL;
	gsize path_list_len;
	gsize pattern_list_len;
	gsize i;
	GSList* locations = NULL;

	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	config = g_key_file_new();
	config_filename = g_strconcat(geany_plugin->geany_data->app->configdir,
															  G_DIR_SEPARATOR_S,
															  "plugins",
															  G_DIR_SEPARATOR_S,
															  PLUGIN_CONF_DIRECORY,
															  G_DIR_SEPARATOR_S,
															  PLUGIN_CONF_FILE_NAME,
															  NULL);
	if(g_key_file_load_from_file(config, config_filename, G_KEY_FILE_NONE, NULL))
	{
		path_list = g_key_file_get_string_list(config, LOCATIONS, PATHS, &path_list_len, NULL);
		pattern_list = g_key_file_get_string_list(config, LOCATIONS, PATTERNS, &pattern_list_len, NULL);

		if(pattern_list_len != path_list_len)
		{
			dialogs_show_msgbox(GTK_MESSAGE_WARNING, _("Open File configuration file invalid!"));
		}
		else
		{
			for(i = 0; i < path_list_len; ++i)
			{
				if(strlen(path_list[i]) == 0)
					continue;

				Location *location = g_malloc0(sizeof(Location));
				location->path = g_strdup(path_list[i]);
				location->pattern = g_strdup(pattern_list[i]);
				locations = g_slist_append(locations, location);
			}
		}
	}

	g_key_file_free(config);
	g_free(config_filename);

	if(path_list != NULL)
	{
		for(i = 0; i < path_list_len; ++i)
			g_free(path_list[i]);
		g_free(path_list);
	}
	if(pattern_list != NULL)
	{
		for(i = 0; i < pattern_list_len; ++i)
			g_free(pattern_list[i]);
		g_free(pattern_list);
	}

	return locations;
}

/**********************************************************************/
static void clear_configuration(GSList* locations)
{
	GSList *iter;
	for(iter = locations; iter != NULL; iter = iter->next)
	{
		Location *location = (Location*)iter->data;
		g_free(location->path);
		g_free(location->pattern);
	}
	g_slist_foreach(locations, (GFunc)g_free, NULL);
	g_slist_free(locations);
}

/**********************************************************************/
static void on_configure_cell_edited(G_GNUC_UNUSED GtkCellRendererText* renderer, gchar* path, gchar* text, gpointer data)
{
	GtkTreeIter iter;
	gint i = 0;
	Column col = (Column)(GPOINTER_TO_INT(data));

	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	while(text[i] != '\0')
	{
		++i;
	}

	if(i == 0)
	{
		D(log_debug("%s:%s - Not-valid char", __FILE__, __FUNCTION__));
		return;
	}

	// Replace old text with new
	gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(list_store), &iter, path);
	gtk_list_store_set(list_store, &iter, col, text, -1);

}

/**********************************************************************/
static void on_configure_add_language(G_GNUC_UNUSED GtkWidget* button, gpointer data)
{
	GtkWidget* tree_view = (GtkWidget*)data;
	GtkTreeIter tree_iter;
	GtkTreePath *path;
	GtkTreeViewColumn* column = NULL;
	gint nb_lines;

	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	/* Add a line */
	gtk_list_store_append(list_store, &tree_iter);
	gtk_list_store_set(list_store, &tree_iter, COLUMN_CONFIG_PATH, "", COLUMN_CONFIG_PATTERN, DEFAULT_PATTERN, -1);

	/* And give the focus to it */
	nb_lines = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list_store), NULL);
	path = gtk_tree_path_new_from_indices(nb_lines-1, -1);
	column = gtk_tree_view_get_column(GTK_TREE_VIEW(tree_view), 0);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(tree_view), path, column, TRUE);

	gtk_tree_path_free(path);
}

/**********************************************************************/
static void on_configure_remove_language(G_GNUC_UNUSED GtkWidget* button, gpointer data)
{
	GtkTreeView* tree_view = (GtkTreeView*)data;
	GtkTreeSelection *selection;
	GtkTreeIter tree_iter;

	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	selection = gtk_tree_view_get_selection (tree_view);
	if(!gtk_tree_selection_get_selected(selection, NULL, &tree_iter))
	{
		D(log_debug("%s:%s - Delete without selection!", __FILE__, __FUNCTION__));
		return;
	}
	/* Remove the element */
	gtk_list_store_remove(list_store, &tree_iter);
}

/**********************************************************************/
GtkWidget* config_widget(void)
{
	GtkWidget *help_label;
	GtkWidget *frame, *vbox, *tree_view;
	GtkWidget *hbox_buttons, *add_button, *remove_button;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;

	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	/* Frame, which is the returned widget */
	frame = gtk_frame_new(_("Open file"));

	/* Main VBox */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	/* Help label */
	help_label = gtk_label_new(_("Directories to scan for files."));
	gtk_box_pack_start(GTK_BOX(vbox), help_label, FALSE, FALSE, 6);

	/* ======= Extensions list ======= */

	/* Add a list containing the extensions for each language (headers / implementations) */
	/* - create the GtkListStore */
	list_store = gtk_list_store_new(CONFIG_COLUMN_COUNT, G_TYPE_STRING, G_TYPE_STRING);

	GSList *iter;
	GSList *locations = load_configuration();
	for(iter = locations; iter != NULL; iter = iter->next)
	{
		GtkTreeIter tree_iter;
		Location *location = (Location*)iter->data;
		gtk_list_store_append(list_store, &tree_iter);
		gtk_list_store_set(list_store, &tree_iter, COLUMN_CONFIG_PATH, location->path, COLUMN_CONFIG_PATTERN, location->pattern, -1);
	}
    clear_configuration(locations);

	tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));

	cell_renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(cell_renderer), "editable", TRUE, NULL);
	g_signal_connect_after(G_OBJECT(cell_renderer), "edited", G_CALLBACK(on_configure_cell_edited), GINT_TO_POINTER(COLUMN_CONFIG_PATH));
	column = gtk_tree_view_column_new_with_attributes(  _("Path"), cell_renderer, "text", COLUMN_CONFIG_PATH, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

	cell_renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(cell_renderer), "editable", TRUE, NULL);
	g_signal_connect_after(G_OBJECT(cell_renderer), "edited", G_CALLBACK(on_configure_cell_edited), GINT_TO_POINTER(COLUMN_CONFIG_PATTERN));
	column = gtk_tree_view_column_new_with_attributes(  _("Pattern"), cell_renderer, "text", COLUMN_CONFIG_PATTERN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

	/* - finally add the GtkTreeView to the frame's vbox */
	gtk_box_pack_start(GTK_BOX(vbox), tree_view, TRUE, TRUE, 6);


	/* ========= Buttons ======== */

	/* HBox */
	hbox_buttons = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_buttons, FALSE, FALSE, 0);

	/* Add the "add" button to the frame's hbox */
	add_button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	g_signal_connect(G_OBJECT(add_button), "clicked", G_CALLBACK(on_configure_add_language), tree_view);
	gtk_box_pack_start(GTK_BOX(hbox_buttons), add_button, FALSE, FALSE, 0);

	/* Add the "remove" button to the frame's hbox */
	remove_button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	g_signal_connect(G_OBJECT(remove_button), "clicked", G_CALLBACK(on_configure_remove_language), tree_view);
	gtk_box_pack_start(GTK_BOX(hbox_buttons), remove_button, FALSE, FALSE, 0);

	gtk_widget_grab_focus(tree_view);

	return frame;
}

/**********************************************************************/
static void on_configure_response(G_GNUC_UNUSED GtkDialog* dialog, gint response, G_GNUC_UNUSED gpointer user_data)
{
	gsize i=0;

	GKeyFile *config = NULL;
	gchar *config_filename = NULL;
	gchar *config_dir = NULL;
	gchar *data;

	gsize list_len;
	gchar** path_list = NULL;
	gchar** pattern_list = NULL;

	GtkTreeIter iter;

	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	if(response != GTK_RESPONSE_OK && response != GTK_RESPONSE_APPLY)
		return;

	config = g_key_file_new();
	config_filename = g_strconcat(geany_plugin->geany_data->app->configdir,
															  G_DIR_SEPARATOR_S,
															  "plugins",
															  G_DIR_SEPARATOR_S,
															  PLUGIN_CONF_DIRECORY,
															  G_DIR_SEPARATOR_S,
															  PLUGIN_CONF_FILE_NAME,
															  NULL);

	config_dir = g_path_get_dirname(config_filename);

	/* Allocate the list */
	list_len = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list_store), NULL);
	path_list = g_malloc0( sizeof(gchar**) * list_len);
	pattern_list = g_malloc0( sizeof(gchar**) * list_len);

	if(list_len > 0)
	{
		gtk_tree_model_iter_children (GTK_TREE_MODEL(list_store),&iter,NULL);

		do
		{
			gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter, COLUMN_CONFIG_PATH, &path_list[i], -1);
			gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter, COLUMN_CONFIG_PATTERN, &pattern_list[i], -1);
			++i;
		} while(gtk_tree_model_iter_next(GTK_TREE_MODEL(list_store), &iter));
	}

	g_key_file_set_string_list(config, LOCATIONS, PATHS, (const gchar * const*)path_list, list_len);
	g_key_file_set_string_list(config, LOCATIONS, PATTERNS, (const gchar * const*)pattern_list, list_len);

	if(!g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(config_dir, TRUE) != 0)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Plugin configuration directory could not be created."));
	}
	else
	{
		data = g_key_file_to_data(config, NULL, NULL);
		utils_write_file(config_filename, data);
		g_free(data);
	}

	for(i = 0; i < list_len; ++i)
	{
		g_free(path_list[i]);
		g_free(pattern_list[i]);
	}
	g_free(path_list);

	g_free(config_dir);
	g_free(config_filename);
	g_key_file_free(config);
}

/**********************************************************************/
static GtkWidget *configure(G_GNUC_UNUSED GeanyPlugin *plugin, GtkDialog *dialog, G_GNUC_UNUSED gpointer pdata)
{
	GtkWidget *vbox;

	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), config_widget(), TRUE, TRUE, 0);
	gtk_widget_show_all(vbox);
	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);

	return vbox;
}
