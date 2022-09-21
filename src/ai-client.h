#ifndef ANNOTATION_TOOLS_AI_CLIENT_
#define ANNOTATION_TOOLS_AI_CLIENT_

#include <stdio.h>
#include <libsoup/soup.h>
#include <json-c/json.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ai_client
{
	void *user_data;
	char *ai_server_url;
	SoupSession *session;
	
	int (*set_url)(struct ai_client *client, const char *server_url);
	int (*predict)(struct ai_client *client, const void *image_data, size_t cb_image, json_object **p_jresult);
};
struct ai_client *ai_client_init(struct ai_client *client, void *user_data);
void ai_client_cleanup(struct ai_client *client);



#ifdef __cplusplus
}
#endif
#endif
