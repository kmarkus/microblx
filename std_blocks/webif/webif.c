/*
 * A system monitoring web-interfaace function block.
 */

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>

#include "mongoose.h"
#include "u5c.h"

/* make this configuration */
static struct u5c_node_info *global_ni;

char wi_meta[] =
	"{ doc='The u5C webinterface',"
	"  license='MIT',"
	"  real-time=false,"
	"}";

struct webif_info {
	struct node_info* ni;
	struct mg_context *ctx;
	struct mg_callbacks callbacks;
};


/* This function will be called by mongoose on every new request. */
static int begin_request_handler(struct mg_connection *conn) {
	const struct mg_request_info *request_info = mg_get_request_info(conn);
	char content[500];

	/* Prepare the message we're going to send */
	int content_length = snprintf(content, sizeof(content),
				      "<!DOCTYPE html>\n"
				      "<html lang=\"en\">\n"
				      "<html>\n"
				      "<head><title>u5C webinterface</title></head>\n"
				      "<body>\n"
				      "<h1>u5C webinterface!</h1>\n"
				      "remote:%d\n"
				      "</body>\n"
				      "</html>\n"
				      ,
				      request_info->remote_port);

	/* Send HTTP reply to the client */
	mg_printf(conn,
		  "HTTP/1.1 200 OK\r\n"
		  "Content-Type: text/html\r\n"
		  "Content-Length: %d\r\n"        /* Always set Content-Length */
		  "\r\n"
		  "%s",
		  content_length, content);

	 /* Returning non-zero tells mongoose that our function has
	  * replied to the client, and mongoose should not send client
	  * any more data. */
	return 1;
}

static int wi_init(u5c_block_t *c)
{
	int ret = -EOUTOFMEM;
	struct webif_info* inf;

	DBG(" ");

	if((inf = calloc(1, sizeof(struct webif_info)))==NULL)
		goto out;

	inf->callbacks.begin_request = begin_request_handler;

	c->private_data = inf;

	/* Ok! */
	ret = 0;
 out:
	return ret;
}

static void wi_cleanup(u5c_block_t *c)
{
	DBG(" "); 
	free(c->private_data);
}

static int wi_start(u5c_block_t *c)
{
	struct webif_info *inf;
	DBG("in");

	/* List of options. Last element must be NULL.
	   TODO: read from config.
	 */
	const char *options[] = {"listening_ports", "8080", NULL};

	inf=(struct webif_info*) c->private_data;

	inf->ctx = mg_start(&inf->callbacks, NULL, options);

	return 0; /* Ok */
}

static void wi_stop(u5c_block_t *c)
{
	struct webif_info *inf;
	DBG("in");
	inf=(struct webif_info*) c->private_data;
	mg_stop(inf->ctx);
}

/* put everything together */
u5c_block_t webif_comp = {
	.name = "webif",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = wi_meta,
	
	/* ops */
	.init = wi_init,
	.start = wi_start,
	.stop = wi_stop,
	.cleanup = wi_cleanup,
};

static int webif_init(u5c_node_info_t* ni)
{
	DBG(" ");	
	global_ni=ni;
	return u5c_block_register(ni, &webif_comp);
}

static void webif_cleanup(u5c_node_info_t *ni)
{
	DBG(" ");
	u5c_block_unregister(ni, BLOCK_TYPE_COMPUTATION, "webif");
}

module_init(webif_init)
module_cleanup(webif_cleanup)
