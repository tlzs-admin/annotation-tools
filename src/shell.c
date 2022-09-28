/*
 * shell.c
 * 
 * Copyright 2020 chehw <htc.chehw@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "shell.h"
#include <gtk/gtk.h>

#include "common.h"
#include "da_panel.h"
#include "property-list.h"
#include "utils.h"

#include "ai-client.h"

#include "shell.h"
#include "shell_private.h"

#define get_widget(builder, name) GTK_WIDGET(gtk_builder_get_object(builder, name))

const char * get_app_path(void);
int show_error_message(struct shell_context *priv, const char *fmt, ...);

int auto_parse_image(struct ai_client *ai, const char *image_file, const char *annotation_file)
{
	if(NULL == ai) return -1;
	
	int rc = 0;
	unsigned char *image_data = NULL;
	ssize_t cb_image = load_binary_data(image_file, &image_data);
	if(cb_image <= 0 || NULL == image_data) return -1;
	
	json_object *jresult = NULL;
	json_object *jdetections = NULL;
	
	rc = ai->predict(ai, image_data, cb_image, &jresult);
	if(rc) {
		if(jresult) json_object_put(jresult);
		return -1;
	}
	json_bool ok = json_object_object_get_ex(jresult, "detections", &jdetections);
	
	rc = -1;
	FILE *fp = NULL;
	int num_detections = 0;
	if(ok && jdetections)
	{
		num_detections = json_object_array_length(jdetections);
		fp = fopen(annotation_file, "w+");
	}
	if(fp) {
		for(int i = 0; i < num_detections; ++i) {
			json_object *jdet = json_object_array_get_idx(jdetections, i);
			if(NULL == jdet) continue;
			
			int class_index = json_get_value(jdet, int, class_index);
			double left = json_get_value(jdet, double, left);
			double top = json_get_value(jdet, double, top);
			double width = json_get_value(jdet, double, width);
			double height = json_get_value(jdet, double, height);
			
			double center_x = left + width / 2.0;
			double center_y = top + height / 2.0;
			 
			fprintf(fp, "%d %g %g %g %g\n", class_index, 
				center_x, center_y, width, height);
		}
		fclose(fp);
		rc = 0;
	}
	json_object_put(jresult);
	return rc;
}

static GtkFileFilter *create_image_files_filter()
{
	GtkFileFilter * filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "image files (jpeg/png)");
	gtk_file_filter_add_mime_type(filter, "image/jpeg");
	gtk_file_filter_add_mime_type(filter, "image/png");
	return filter;
}


static int shell_load_image(const char *path_name, struct shell_context *shell)
{
	int rc = -1;
	assert(shell && shell->priv);
	struct shell_private *priv = shell->priv;
	
	strncpy(priv->image_file, path_name, sizeof(priv->image_file));
	
	char annotation_file[PATH_MAX] = "";
	strncpy(annotation_file, path_name, sizeof(annotation_file));

	char * p_ext = strrchr(annotation_file, '.');
	if(p_ext) strcpy(p_ext, ".txt");
	else strcat(annotation_file, ".txt");
	
	strncpy(priv->label_file, annotation_file, sizeof(priv->label_file));
	
	rc = check_file(annotation_file);
	if(rc || priv->ai_enabled) {
		global_params_t *params = shell->user_data;
		assert(params);
		rc = auto_parse_image(params->ai, priv->image_file, annotation_file);
	}
	
	da_panel_t * panel = priv->panels[0];
	assert(panel->load_image);
	if(panel->load_image)
	{
		annotation_list_t * list = priv->properties->annotations;
		assert(list);
		annotation_list_reset(list);
		
		if(0 == rc) {
			debug_printf("annotation_file: '%s'", priv->label_file);
			ssize_t count = list->load(list, priv->label_file);
			assert(count >= 0);
			annotation_list_dump(list);
		}
		panel->load_image(panel, path_name);
	}
	
	return 0;
}

void statusbar_set_info(GtkWidget *statusbar, const char *fmt, ...)
{
	char msg_text[PATH_MAX] = "";
	va_list ap;
	va_start(ap, fmt);
	int cb = vsnprintf(msg_text, sizeof(msg_text) - 1, fmt, ap);
	va_end(ap);
	
	if(cb > 0) {
		guint msgid = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "info");
		gtk_statusbar_push(GTK_STATUSBAR(statusbar), msgid, msg_text);
	}
	
	return;
}

static void on_image_file_changed(GtkFileChooserButton *file_chooser, struct shell_context *shell)
{
	assert(shell && shell->priv);
	struct shell_private *priv = shell->priv;
	
	char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
	if(NULL == filename) return;
	
	shell_load_image(filename, shell);
	
	statusbar_set_info(priv->statusbar, "%s", filename);
	free(filename);	
	return;
}

static void on_load_image(GtkButton * button, struct shell_context *shell)
{
	assert(shell && shell->priv);
	struct shell_private *priv = shell->priv;
	
	static GtkWidget *dlg = NULL;
	if(NULL == dlg) {
		dlg = gtk_file_chooser_dialog_new(_("load image"), 
			GTK_WINDOW(priv->window),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			_("open"), GTK_RESPONSE_APPLY,
			_("cancel"), GTK_RESPONSE_CANCEL,
			NULL
			);
		assert(dlg);
	}
	GtkFileFilter * filter = create_image_files_filter();
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dlg), filter);
	
	assert(priv->app_path);
	if(priv->image_file[0] == '\0') gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), priv->app_path);
	else gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), priv->image_file);
	
	gtk_widget_show_all(dlg);
	
	int response = gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_hide(dlg);
	
	switch(response)
	{
	case GTK_RESPONSE_APPLY: break;
	default:	
		return;
	}

	guint msgid = gtk_statusbar_get_context_id(GTK_STATUSBAR(priv->statusbar), "info");
	const char * path_name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
	gtk_statusbar_push(GTK_STATUSBAR(priv->statusbar), msgid, path_name);
	int rc = 0;
	
	const char * p_filename = strrchr(path_name, '/');
	if(NULL == p_filename) p_filename = path_name;
	else ++p_filename;
	gtk_label_set_text(GTK_LABEL(priv->filename_label), p_filename);
	
	priv->image_file[0] = '\0';
	priv->label_file[0] = '\0';

	rc = check_file(path_name);
	if(rc) {
		show_error_message(shell, "invalid image path_name: %s\n", path_name);
		return;
	}
	
	strncpy(priv->image_file, path_name, sizeof(priv->image_file));
	
	char annotation_file[PATH_MAX] = "";
	strncpy(annotation_file, path_name, sizeof(annotation_file));

	char * p_ext = strrchr(annotation_file, '.');
	if(p_ext) strcpy(p_ext, ".txt");
	else strcat(annotation_file, ".txt");
	
	strncpy(priv->label_file, annotation_file, sizeof(priv->label_file));	
	rc = check_file(annotation_file);
	if(rc || priv->ai_enabled) {
		global_params_t *params = shell->user_data;
		assert(params);
		rc = auto_parse_image(params->ai, priv->image_file, annotation_file);
	}
	
	
	da_panel_t * panel = priv->panels[0];
	assert(panel->load_image);
	if(panel->load_image)
	{
		annotation_list_t * list = priv->properties->annotations;
		assert(list);
		annotation_list_reset(list);

		if(0 == rc)
		{
			printf("annotation_file: '%s'", priv->label_file);
			ssize_t count = list->load(list, priv->label_file);
			assert(count >= 0);
			annotation_list_dump(list);
		}
		panel->load_image(panel, path_name);
	}
	
	return;
}

static void on_save_annotation(GtkWidget * button, struct shell_context *shell)
{
	assert(shell && shell->priv);
	struct shell_private *priv = shell->priv;
	
	annotation_list_t * list = priv->properties->annotations;
	assert(list);

	int rc = list->save(list, priv->label_file);
	if(rc) {
		show_error_message(shell, 
			"<b>save annotation failed.</b>\nlabel_file: %s",
			priv->label_file);
	}
	return;
}

static void on_selchanged_labels(GtkComboBox * combo, struct shell_context *shell)
{
	assert(shell && shell->priv);
	struct shell_private *priv = shell->priv;
	
	gint id = gtk_combo_box_get_active(combo);
	char msg[200] = "";

	GtkHeaderBar * header_bar = GTK_HEADER_BAR(priv->header_bar);
	assert(header_bar);

	snprintf(msg, sizeof(msg), "active id: %d", (int)id);
	gtk_header_bar_set_subtitle(header_bar, msg);


	da_panel_t * panel = priv->panels[0];
	assert(panel);

	da_panel_set_annotation(panel, id);
	return;
}

static void on_tree_labels_selchanged(GtkTreeSelection *selection, struct shell_context *shell)
{
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	
	gboolean ok = gtk_tree_selection_get_selected(selection, &model, &iter);
	if(!ok) return;
	
	gint id = -1;
	gtk_tree_model_get(model, &iter, 0, &id, -1);
	
	gtk_combo_box_set_active(GTK_COMBO_BOX(shell->priv->combo), id);
	return;
	
}


/******************************************************************************
 * struct shell_context : virtual functions
******************************************************************************/
static int init_windows_with_ui_file(struct shell_context *shell, const char *ui_file);
static int init_windows(struct shell_context *shell);

static int shell_init(struct shell_context *shell, json_object *jconfig)
{
	static const char *default_ui_file = "app.ui";
	shell_private_t * priv = (shell_private_t *)shell;
	assert(priv);
	
	
	const char *ui_file = NULL;
	if(jconfig) ui_file = json_get_value(jconfig, string, ui_file);
	if(NULL == ui_file) ui_file = default_ui_file;
	
	json_object * jshell = NULL;
	json_bool ok = FALSE;
	if(jconfig) ok = json_object_object_get_ex(jconfig, "shell", &jshell);
	if(ok) shell->jconfig = jshell;
	
	return init_windows_with_ui_file(shell, ui_file);
}

static int shell_run(struct shell_context *shell)
{
	assert(shell && shell->priv);
	struct shell_private *priv = shell->priv;
	shell->quit = 0;


	gtk_widget_show_all(priv->window);
	
	gtk_main();
	shell->quit = 1;
	return 0;
}

static int shell_stop(struct shell_context *shell)
{
	if(NULL == shell) return -1;
	if(!shell->quit) {
		shell->quit = 1;
		gtk_main_quit();
	}
	return 0;
}


/******************************************************************************
 * struct shell_context : init ui
******************************************************************************/
static GtkListStore *create_classes_list_store(global_params_t *params)
{
	if(NULL == params) params = global_params_get_default();
	assert(params->num_labels > 0 && params->labels);
	
	GtkListStore * store = gtk_list_store_new(2, G_TYPE_INT, G_TYPE_STRING);
	assert(store);
	for(int i = 0; i < params->num_labels; ++i)
	{
		GtkTreeIter iter;
		printf("add %d: %s(%s)\n", i, params->labels[i], _(params->labels[i]));
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
			0, i,
			1, _(params->labels[i]),
			-1);
	}
	return store;
}

static int init_classes_combo(GtkWidget *combo, GtkListStore * store, struct shell_context *shell)
{
	//~ g_object_ref(store);
	if(NULL == store) store = create_classes_list_store(NULL);
	else g_object_ref(store);
	
	gtk_combo_box_set_model(GTK_COMBO_BOX(combo), GTK_TREE_MODEL(store));
	GtkCellRenderer * cr = NULL;

	cr = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), cr, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), cr, "text", 0, NULL);
	
	//~ cr = gtk_cell_renderer_text_new();
	//~ gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), cr, TRUE);
	//~ gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), cr, "text", 1, NULL);

	gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(combo), 1);
	gtk_combo_box_set_id_column(GTK_COMBO_BOX(combo), 0);
	g_signal_connect(combo, "changed", G_CALLBACK(on_selchanged_labels), shell);
	return 0;
}

static int init_classes_tree(GtkWidget *listview, GtkListStore *store, struct shell_context *shell)
{
	GtkCellRenderer * cr = NULL;
	GtkTreeViewColumn * col = NULL;

	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Id"), cr, "text", 0, NULL);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(listview), col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Name"), cr, "text", 1, NULL);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(listview), col);
	
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(listview));
	g_signal_connect(selection, "changed", G_CALLBACK(on_tree_labels_selchanged), shell);
	
	gtk_tree_view_set_model(GTK_TREE_VIEW(listview), GTK_TREE_MODEL(store));
	
	
	
	return 0;
	
}

static int init_windows(struct shell_context *shell)
{
	assert(shell && shell->priv);
	struct shell_private *priv = shell->priv;
	
	GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget * header_bar = gtk_header_bar_new();
	GtkWidget * grid = gtk_grid_new();
	GtkWidget * uri_entry = gtk_entry_new();
	GtkWidget * nav_button = gtk_button_new_from_icon_name("go-jump", GTK_ICON_SIZE_BUTTON);
	GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget * statusbar = gtk_statusbar_new();

	GtkWidget * hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);

	property_list_t * list = property_list_new(NULL, NULL, shell);
	assert(list);
	priv->properties = list;

	da_panel_t * panel = da_panel_new(640, 480, shell);
	assert(panel);
	priv->panels[0] = panel;

	gtk_grid_attach(GTK_GRID(grid), list->scrolled_win, 0, 0, 1, 1);
	
	gtk_paned_add1(GTK_PANED(hpaned), panel->frame);
	gtk_paned_add2(GTK_PANED(hpaned), grid);
	gtk_paned_set_position(GTK_PANED(hpaned), 640);

	gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
	gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), _("Annotation Tools"));

	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), statusbar, FALSE, TRUE, 0);
	
	GtkWidget * load_btn = gtk_button_new_from_icon_name("document-open", GTK_ICON_SIZE_BUTTON);
	gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), load_btn);
	g_signal_connect(load_btn, "clicked", G_CALLBACK(on_load_image), shell);
	
	GtkWidget * filename_label = gtk_label_new("");
	gtk_widget_set_size_request(filename_label, 180, -1);
	priv->filename_label = filename_label;
	gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), filename_label);
	

	GtkWidget * save_btn = gtk_button_new_from_icon_name("document-save", GTK_ICON_SIZE_BUTTON);
	assert(save_btn);
	g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_annotation), shell);
	gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), save_btn);

	GtkListStore *store = create_classes_list_store(NULL);
	GtkWidget * combo = gtk_combo_box_new_with_entry();
	init_classes_combo(combo, store, shell);
	GtkWidget * label = gtk_label_new(_("Classes"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 1);
	gtk_box_pack_start(GTK_BOX(hbox), combo, FALSE, TRUE, 1);
	priv->combo = combo;
	
	
	gtk_box_pack_start(GTK_BOX(hbox), uri_entry, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), nav_button, FALSE, TRUE, 0);
	gtk_widget_set_hexpand(uri_entry, TRUE);
	

	priv->window = window;
	priv->header_bar = header_bar;
	priv->content_area = vbox;
	priv->statusbar = statusbar;
	
	gtk_window_set_default_size(GTK_WINDOW(window), 1280, 800);
	g_signal_connect_swapped(window, "destroy", G_CALLBACK(shell_stop), shell);
	gtk_window_set_role(GTK_WINDOW(window), "tools");
	return 0;
}
static gboolean on_ai_flags_state_changed(GtkSwitch *widget, gboolean state, struct shell_context *shell)
{
	assert(shell && shell->priv);
	struct shell_private *priv = shell->priv;
	
	priv->ai_enabled = state;
	return FALSE;
}

static int init_windows_with_ui_file(struct shell_context *shell, const char *ui_file)
{
	assert(shell && shell->priv);
	struct shell_private *priv = shell->priv;
	
	if(NULL == ui_file || check_file(ui_file) != 0) return init_windows(shell);
	
	GtkBuilder *builder = gtk_builder_new_from_file(ui_file);
	if(NULL == builder) return init_windows(shell);
	
	da_panel_t * panel = da_panel_new(640, 480, shell);
	assert(panel);
	priv->panels[0] = panel;
	
	
	GtkWidget *window = get_widget(builder, "main_window");
	GtkWidget *header_bar = gtk_header_bar_new();
	gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
	gtk_header_bar_set_has_subtitle(GTK_HEADER_BAR(header_bar), TRUE);
	
	GtkWidget *statusbar = get_widget(builder, "statusbar1");
	GtkWidget *classes_list = get_widget(builder, "classes_list");
	
	GtkWidget *classes_combo = get_widget(builder, "classes_combo");
	GtkWidget *da_frame = get_widget(builder, "da_frame");
	GtkWidget *properties_list = get_widget(builder, "properties_list");
	

	if(da_frame) {
		g_object_ref(panel->da);
		gtk_container_remove(GTK_CONTAINER(panel->frame), panel->da);
		gtk_container_add(GTK_CONTAINER(da_frame), panel->da);
		g_object_unref(panel->da);
		
		gtk_widget_destroy(panel->frame);
		panel->frame = da_frame;
		gtk_widget_show_all(panel->frame);
		
	}
	
	GtkWidget *file_chooser = gtk_file_chooser_button_new(_("Open File"), GTK_FILE_CHOOSER_ACTION_OPEN);
	GtkFileFilter *filter = create_image_files_filter();
	assert(filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(file_chooser), filter);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_chooser), priv->app_path);
	g_signal_connect(file_chooser, "file-set", G_CALLBACK(on_image_file_changed), shell);
	gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), file_chooser);

	priv->window = window;
	priv->header_bar = header_bar;
	priv->statusbar = statusbar;
	priv->combo = classes_combo;
	priv->classes_list = classes_list;
	
	property_list_t * list = property_list_new(properties_list, NULL, shell);
	assert(list);
	priv->properties = list;
	
	GtkListStore *store = create_classes_list_store(NULL);
	init_classes_combo(priv->combo, store, shell);
	init_classes_tree(priv->classes_list, store, shell);
	
	
	GtkWidget *ai_flags_switcher = get_widget(builder, "ai_flags");
	assert(ai_flags_switcher);
	g_signal_connect(ai_flags_switcher, "state-set", G_CALLBACK(on_ai_flags_state_changed), shell);
	
	
	// ...
	gtk_window_set_default_size(GTK_WINDOW(window), 1280, 800);
	g_signal_connect_swapped(window, "destroy", G_CALLBACK(shell_stop), shell);
	gtk_window_set_role(GTK_WINDOW(window), "tools");
	g_object_unref(builder);
	return 0;
}

/******************************************************************************
 * struct shell_context : public functions
******************************************************************************/
static struct shell_context g_shell[1] = {{
	.init = shell_init,
	.run = shell_run,
	.stop = shell_stop,
}};

static struct shell_private *shell_private_new(struct shell_context *shell)
{
	struct shell_private *priv = calloc(1, sizeof(*priv));
	priv->shell = shell;
	priv->app_path = get_app_path();
	
	return priv;
}

struct shell_context * shell_context_init(struct shell_context *shell, void * user_data)
{
	if(NULL == shell) shell = g_shell;
	else *shell = g_shell[0];
	
	shell->user_data = user_data;
	shell->priv = shell_private_new(shell);
	
	return shell;
}

void shell_context_cleanup(struct shell_context * shell)
{
	return;
}


/******************************************************************************
 * struct shell_context : member functions
******************************************************************************/
annotation_list_t * shell_get_annotations(struct shell_context * shell)
{
	assert(shell && shell->priv);
	shell_private_t * priv = shell->priv;
	property_list_t * props = priv->properties;
	if(props) return props->annotations;
	return NULL;
}

void shell_redraw(struct shell_context * shell)
{
	assert(shell && shell->priv);
	shell_private_t * priv = shell->priv;
	property_list_t * props = priv->properties;
	property_list_redraw(props);
	
	// auto save 
	on_save_annotation(NULL, shell);
	return;
}

void shell_set_current_label(struct shell_context * shell, int klass)
{
	shell_private_t * priv = shell->priv;
	global_params_t	* params = global_params_get_default();
	assert(klass >= 0 && klass <= params->num_labels);

	GtkComboBox * combo = GTK_COMBO_BOX(priv->combo);
	gtk_combo_box_set_active(combo, klass);
	return;
}

int show_error_message(struct shell_context *shell, const char *fmt, ...)
{
	assert(shell && shell->priv);
	static GtkWidget *dlg = NULL;
	
	struct shell_private *priv = shell->priv;
	char msg[PATH_MAX] = "";
	int cb_msg = 0;
	va_list ap;
	va_start(ap, fmt);
	cb_msg = vsnprintf(msg, sizeof(msg) - 1, fmt, ap);
	va_end(ap);
	debug_printf("cb_msg: %d, err_msg: %s\n", cb_msg, msg);
	
	if(NULL == dlg) {
		dlg = gtk_message_dialog_new(GTK_WINDOW(priv->window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, 
			GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
			NULL);
		assert(dlg);
	}
	
	gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(dlg), msg);
	gtk_widget_show_all(dlg);
	gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_hide(dlg);
	return 0;
}
