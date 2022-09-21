/*
 * ai-client.c
 * 
 * Copyright 2022 chehw <hongwei.che@gmail.com>
 * 
 * The MIT License (MIT)
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the "Software"), to deal 
 * in the Software without restriction, including without limitation the rights 
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 * copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all 
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
 * IN THE SOFTWARE.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libsoup/soup.h>
#include <json-c/json.h>

#include "ai-client.h"

static int ai_client_predict(struct ai_client *client, const void *image_data, size_t cb_image, json_object **p_jresult)
{
	assert(client->session);
	
	int rc = -1;
	if(NULL == image_data || cb_image <= 0) return -1;
	
	SoupSession *session = client->session;
	char *content_type = NULL;
	gboolean uncertain = TRUE;
	content_type = g_content_type_guess(NULL, image_data, cb_image, &uncertain);
	if(NULL == content_type || uncertain) {
		fprintf(stderr, "[ERROR]: %s(): unknown image type.\n", __FUNCTION__);
		if(content_type) g_free(content_type);
		return -1;
	}

	SoupMessage *msg = soup_message_new("POST", client->ai_server_url);
	if(NULL == msg) goto label_cleanup;
	
	SoupMessageHeaders *request_headers = msg->request_headers;
	soup_message_headers_append(request_headers, "Content-Type", content_type);
	soup_message_body_append(msg->request_body, SOUP_MEMORY_COPY, image_data, cb_image);
	
	guint response_code = soup_session_send_message(session, msg);
	if(response_code < 200 || response_code >= 300) goto label_cleanup;
	
	if(msg->response_body && msg->response_body->length > 0 && msg->response_body->data)
	{
		json_tokener *jtok = json_tokener_new();
		assert(jtok);
		
		json_object *jresult = json_tokener_parse_ex(jtok, 
			(const char *)msg->response_body->data,
			msg->response_body->length);
		enum json_tokener_error jerr = json_tokener_get_error(jtok);
		if(jerr == json_tokener_success) {
			if(p_jresult) *p_jresult = jresult;
			rc = 0;
		}else {
			if(jresult) json_object_put(jresult);
		}
		json_tokener_free(jtok);
	}
label_cleanup:	
	if(msg) g_object_unref(msg);
	if(content_type) g_free(content_type);
	return rc;
}


static int ai_client_set_url(struct ai_client *client, const char *server_url)
{
	if(client->ai_server_url) {
		free(client->ai_server_url);
		client->ai_server_url = NULL;
	}
	
	if(server_url) client->ai_server_url = strdup(server_url);
	return 0;
}

struct ai_client *ai_client_init(struct ai_client *client, void *user_data)
{
	if(NULL == client) client = calloc(1, sizeof(*client));
	assert(client);
	
	client->user_data = user_data;
	client->set_url = ai_client_set_url;
	client->predict = ai_client_predict;
	
	client->session = soup_session_new_with_options(SOUP_SESSION_USER_AGENT, "soup/2.4 Mozilla/5.0", NULL);
	assert(client->session);
	
	return client;
}
void ai_client_cleanup(struct ai_client *client)
{
	if(NULL == client) return;
	
	if(client->ai_server_url) {
		free(client->ai_server_url);
		client->ai_server_url = NULL;
	}
	
	if(client->session) {
		g_object_unref(client->session);
		client->session = NULL;
	}
}


#if defined(TEST_AI_CLIENT_) && defined(_STAND_ALONE)
#include "utils.h"
int main(int argc, char **argv)
{
	const char *server_url = "http://127.0.0.1:9090/ai";
	if(argc > 1) server_url = argv[1];
	
	struct ai_client *client = ai_client_init(NULL, NULL);
	client->set_url(client, server_url);

	const char *jpeg_file = "test.jpg";
	const char *png_file = "test.png";
	
	int rc = -1;
	json_object *jresult = NULL;
	unsigned char *image_data = NULL;
	ssize_t cb_image = 0;
	
	cb_image = load_binary_data(jpeg_file, &image_data);
	assert(image_data && cb_image > 0);
	rc = client->predict(client, image_data, cb_image, &jresult);
	assert(0 == rc);
	printf("== image_file: %s, result:\n%s\n",
		jpeg_file, 
		json_object_to_json_string_ext(jresult, JSON_C_TO_STRING_PRETTY));
	
	json_object_put(jresult);
	jresult = NULL;
	free(image_data);
	image_data = NULL;
	cb_image = 0;
	
	
	cb_image = load_binary_data(png_file, &image_data);
	assert(image_data && cb_image > 0);
	rc = client->predict(client, image_data, cb_image, &jresult);
	assert(0 == rc);
	printf("== image_file: %s, result:\n%s\n",
		png_file, 
		json_object_to_json_string_ext(jresult, JSON_C_TO_STRING_PRETTY));
	
	
	json_object_put(jresult);
	jresult = NULL;
	free(image_data);
	image_data = NULL;
	cb_image = 0;
	
	return 0;
}
#endif
