#ifndef ANNOTATION_TOOLS_SHELL_PRIVATE_H_
#define ANNOTATION_TOOLS_SHELL_PRIVATE_H_

#include <stdio.h>

#include "shell.h"
#include "da_panel.h"
#include "property-list.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct shell_private
{
	struct shell_context *shell;

	GtkWidget * window;
	GtkWidget * content_area;
	GtkWidget * header_bar;
	GtkWidget * statusbar;
	GtkWidget * combo;
	
	GtkWidget * filename_label;

	da_panel_t * panels[1];		// TODO: multi-viewport support
	property_list_t * properties;
	
	const char * app_path;
	char image_file[PATH_MAX];
	char label_file[PATH_MAX];

	guint timer_id;
	int ai_enabled;
	
	GtkWidget *classes_list;
}shell_private_t;



#ifdef __cplusplus
}
#endif
#endif
