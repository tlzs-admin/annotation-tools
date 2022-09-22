#ifndef _SHELL_H_
#define _SHELL_H_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "common.h"		// annotation_data_t

#include <json-c/json.h>

struct shell_private;
struct shell_context
{
	void * user_data;
	struct shell_private *priv;
	json_object *jconfig;
	int quit;
	
	// virtual functions
	int (* init)(struct shell_context * shell, json_object *jsettings);
	int (* run)(struct shell_context * shell);
	int (* stop)(struct shell_context *shell);
	
	// callbacks
	int (* on_update)(struct shell_context * shell);
};

struct shell_context * shell_context_init(struct shell_context *shell, void * user_data);
void shell_context_cleanup(struct shell_context *shell);

int shell_add_annotation(struct shell_context * shell, int index, const annotation_data_t * data);
annotation_list_t * shell_get_annotations(struct shell_context * shell);
void shell_redraw(struct shell_context * shell);
void shell_set_current_label(struct shell_context * shell, int klass);

#ifdef __cplusplus
}
#endif
#endif
