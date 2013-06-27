/*
 * A system monitoring web-interface function block.
 */

#define DEBUG 1
#define WEBIF_RELOAD 1

#include <luajit-2.0/lauxlib.h>
#include <luajit-2.0/lualib.h>
#include <luajit-2.0/lua.h>

#include <stdio.h>
#include <stdlib.h>

#include "mongoose.h"
#include "u5c.h"

#define WEBIF_FILE "/home/mk/prog/c/u5c/std_blocks/webif/webif.lua"

/* make this configuration */
static struct u5c_node_info *global_ni;

u5c_config_t webif_conf[] = {
	{ .name="port", .type_name="char", .value = { .len=10 } }, /* char[10] */
	{ NULL }
};

char wi_meta[] =
	"{ doc='The u5C webinterface',"
	"  license='MIT',"
	"  real-time=false,"
	"}";

struct webif_info {
	struct u5c_node_info* ni;
	struct mg_context *ctx;
	struct mg_callbacks callbacks;
	struct lua_State* L;
};

static int init_lua(struct webif_info* inf);

/* This function will be called by mongoose on every new request. */
static int begin_request_handler(struct mg_connection *conn)
{
	int len;
	const char *res, *mime_type;
	const struct mg_request_info *request_info = mg_get_request_info(conn);
	struct webif_info *inf = (struct webif_info*) request_info->user_data;

	/* check post */
	char post_data[1024];
	int post_data_len = mg_read(conn, post_data, sizeof(post_data));

#ifdef WEBIF_RELOAD
	if(strcmp(request_info->uri, "/reload")==0) {
		fprintf(stderr, "reloading webif.lua\n");
		lua_close(inf->L);
		init_lua(inf);
		res="reloaded, <a href=\"./\">continue</a>";
		len=strlen(res);
		mime_type="text/html";
		goto respond;
	}
#endif
	/* call lua */
	lua_getfield(inf->L, LUA_GLOBALSINDEX, "request_handler");
	lua_pushlightuserdata(inf->L, (void*) inf->ni);
	lua_pushlightuserdata(inf->L, (void*) request_info);

	if(post_data_len>0)
		lua_pushlstring(inf->L, post_data, post_data_len);
	else
		lua_pushnil(inf->L);


	if(lua_pcall(inf->L, 3, 2, 0)!=0) {
		ERR("Lua: %s", lua_tostring(inf->L, -1));
		goto out;
	}

	res=luaL_checkstring(inf->L, 1);
	mime_type=luaL_optstring(inf->L, 2, "text/html");
	len=strlen(res);
	lua_pop(inf->L, 2);

 respond:
	/* Send HTTP reply to the client */
	mg_printf(conn,
		  "HTTP/1.1 200 OK\r\n"
		  "Content-Type: %s\r\n"
		  "Content-Length: %d\r\n"        /* Always set Content-Length */
		  "\r\n"
		  "%s", mime_type, len, res);

	 /* Returning non-zero tells mongoose that our function has
	  * replied to the client, and mongoose should not send client
	  * any more data. */
 out:
	return 1;
}

static int init_lua(struct webif_info* inf)
{
	int ret=-1;

	if((inf->L=luaL_newstate())==NULL) {
		ERR("failed to alloc lua_State");
		goto out;
	}

	luaL_openlibs(inf->L);

	ret = luaL_dofile(inf->L, WEBIF_FILE);

	if (ret) {
		ERR("Failed to load u5c_webif.lua: %s\n", lua_tostring(inf->L, -1));
		goto out;
	}
	ret=0;
 out:
	return ret;
}


static int wi_init(u5c_block_t *c)
{
	int ret = -EOUTOFMEM;
	struct webif_info* inf;

	if((inf = calloc(1, sizeof(struct webif_info)))==NULL)
		goto out;

	c->private_data = inf;

	/* make configurable ?*/
	inf->ni=global_ni;

	if(init_lua(inf) != 0)
		goto out_free;

	inf->callbacks.begin_request = begin_request_handler;

	/* Ok! */
	ret = 0;
	goto out;

 out_free:
	free(inf);
 out:
	return ret;
}

static void wi_cleanup(u5c_block_t *c)
{
	DBG(" ");
	struct webif_info* inf = (struct webif_info*) c->private_data;
	lua_close(inf->L);
	free(c->private_data);
}

static int wi_start(u5c_block_t *c)
{
	struct webif_info *inf;
	char *port_num;
	unsigned int port_num_len;

	DBG("in");

	/* read port config and set default if undefined */
	port_num = (char *) u5c_config_get_data_ptr(c, "port", &port_num_len);
	port_num = (port_num_len==0) ?  "8080" : port_num;

	DBG("starting mongoose on port %s", port_num);

	/* List of options. Last element must be NULL.
	   TODO: read from config.
	 */
	const char *options[] = {"listening_ports", port_num, NULL};

	inf=(struct webif_info*) c->private_data;
	inf->ctx = mg_start(&inf->callbacks, inf, options);

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
	.configs = webif_conf,

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
