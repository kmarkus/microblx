/*
 * A system monitoring web-interface function block.
 */

#undef DEBUG
#define WEBIF_RELOAD			1
#define COMPILE_IN_WEBIF_LUA_FILE	1

#define MONGOOSE_NR_THREADS		"1"	/* don't change. */
#define WEBIF_DEFAULT_PORT		"8080"

#include <luajit-2.0/lauxlib.h>
#include <luajit-2.0/lualib.h>
#include <luajit-2.0/lua.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "mongoose.h"
#include "ubx.h"

UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)

#ifdef COMPILE_IN_WEBIF_LUA_FILE
#include "webif.lua.hexarr"
#else
#define WEBIF_FILE "/home/mk/prog/c/microblx/std_blocks/webif/webif.lua"
#endif

/* make this configuration */
static struct ubx_node_info *global_ni;

ubx_config_t webif_conf[] = {
	{ .name="port", .type_name="char", .value = { .len=10 },
	  .doc="Port to listen on (default: " WEBIF_DEFAULT_PORT ")" },
	{ NULL }
};

char wi_meta[] =
	"{ doc='A microblx webinterface',"
	"  license='MIT',"
	"  real-time=false,"
	"}";

struct webif_info {
	struct ubx_node_info* ni;
	struct mg_context *ctx;
	struct mg_callbacks callbacks;
	struct lua_State* L;
	pthread_mutex_t mutex;
};


static int init_lua(struct webif_info* inf);

/* This function will be called by mongoose on every new request. */
static int begin_request_handler(struct mg_connection *conn)
{
	int len;
	const char *res, *mime_type;

	const struct mg_request_info *request_info = mg_get_request_info(conn);
	struct webif_info *inf = (struct webif_info*) request_info->user_data;

	if(pthread_mutex_lock(&inf->mutex) != 0) {
		ERR("failed to aquire mutex");
		goto out;
	}

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
		ERR("%s, calling Lua request_handler failed: %s",
		    "webif", lua_tostring(inf->L, -1));
		goto out_unlock;
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
 out_unlock:
	pthread_mutex_unlock(&inf->mutex);
 out:
	return 1;
}

static int init_lua(struct webif_info* inf)
{
	int ret=-1;

	if(pthread_mutex_init(&inf->mutex, NULL) != 0) {
		ERR("failed to init mutex");
		goto out;
	}

	if((inf->L=luaL_newstate())==NULL) {
		ERR("failed to alloc lua_State");
		goto out;
	}

	luaL_openlibs(inf->L);

#ifdef COMPILE_IN_WEBIF_LUA_FILE
	ret = luaL_dostring(inf->L, (const char*) &webif_lua);
#else
	ret = luaL_dofile(inf->L, WEBIF_FILE);
#endif
	if (ret) {
		ERR("Failed to load ubx_webif.lua: %s\n", lua_tostring(inf->L, -1));
		goto out;
	}
	ret=0;
 out:
	return ret;
}


static int wi_init(ubx_block_t *c)
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

static void wi_cleanup(ubx_block_t *c)
{
	DBG(" ");
	struct webif_info* inf = (struct webif_info*) c->private_data;
	lua_close(inf->L);
	pthread_mutex_destroy(&inf->mutex);
	free(c->private_data);
}

static int wi_start(ubx_block_t *c)
{
	char *port_num;
	unsigned int port_num_len;
	struct webif_info *inf;

	DBG("in");

	inf=(struct webif_info*) c->private_data;

	/* read port config and set default if undefined */
	port_num = (char *) ubx_config_get_data_ptr(c, "port", &port_num_len);

	if(port_num_len == 0 || strlen(port_num)==0)
		port_num = WEBIF_DEFAULT_PORT;

	DBG("starting mongoose on port %s using %s thread(s)", port_num, MONGOOSE_NR_THREADS);

	/* List of options. Last element must be NULL. */
	const char *options[] = {"listening_ports", port_num, "num_threads", MONGOOSE_NR_THREADS, NULL};

	if((inf->ctx = mg_start(&inf->callbacks, inf, options))==NULL) {
		ERR("failed to start mongoose on port %s", port_num);
		goto out_err;
	}

	return 0; /* Ok */
 out_err:
	return -1;
}

static void wi_stop(ubx_block_t *c)
{
	struct webif_info *inf;
	DBG("in");
	inf=(struct webif_info*) c->private_data;
	mg_stop(inf->ctx);
}

/* put everything together */
ubx_block_t webif_comp = {
	.name = "webif/webif",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = wi_meta,
	.configs = webif_conf,

	/* ops */
	.init = wi_init,
	.start = wi_start,
	.stop = wi_stop,
	.cleanup = wi_cleanup,
};

static int webif_init(ubx_node_info_t* ni)
{
	DBG(" ");
	global_ni=ni;
	return ubx_block_register(ni, &webif_comp);
}

static void webif_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	ubx_block_unregister(ni, "webif/webif");
}

UBX_MODULE_INIT(webif_init)
UBX_MODULE_CLEANUP(webif_cleanup)
