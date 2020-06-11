/*
 * A generic luajit based block.
 */

#undef UBX_DEBUG
#define COMPILE_IN_LOG_LUA_FILE

#include <lauxlib.h>
#include <lualib.h>
#include <lua.h>

#include <stdio.h>
#include <stdlib.h>

#include "ubx.h"

#ifdef COMPILE_IN_LOG_LUA_FILE
#include "file_logger.lua.hexarr"
#else
#define FILE_LOG_FILE "/home/mk/prog/c/microblx/std_blocks/logging/file_logger.lua"
#endif

char file_logger_meta[] =
	"{ doc='A reporting block that writes to a file',"
	"  realtime=false,"
	"}";

ubx_proto_config_t file_logger_conf[] = {
	{ .name = "report_conf", .type_name = "char" },
	{ .name = "filename", .type_name = "char" },
	{ .name = "separator", .type_name = "char"},
	{ .name = "timestamp", .type_name = "int"},
	{ 0 }
};

struct file_logger_info {
	struct lua_State *L;
};


/**
 * @brief: call a hook with fname.
 *
 * @param block (is passed on a first arg)
 * @param fname name of function to call
 * @param require_fun raise an error if function fname does not exist.
 * @param require_res if 1, require a boolean valued result.
 * @return -1 in case of error, 0 otherwise.
 */
int call_hook(ubx_block_t *b, const char *fname, int require_fun,
	      int require_res)
{
	int ret = 0;
	struct file_logger_info *inf = (struct file_logger_info *)b->private_data;
	int num_res = (require_res != 0) ? 1 : 0;

	lua_getglobal(inf->L, fname);

	if (lua_isnil(inf->L, -1)) {
		lua_pop(inf->L, 1);
		if (require_fun) {
			ubx_err(b, "%s: no (required) Lua function %s",
				b->name, fname);
		}
		goto out;
	}

	lua_pushlightuserdata(inf->L, (void *)b);

	if (lua_pcall(inf->L, 1, num_res, 0) != 0) {
		ubx_err(b, "%s: error calling function %s: %s", b->name, fname,
			lua_tostring(inf->L, -1));
		lua_pop(inf->L, 1); /* pop result */
		ret = -1;
		goto out;
	}

	if (require_res) {
		if (!lua_isboolean(inf->L, -1)) {
			ubx_err(b, "%s: %s must return a bool but returned a %s",
				b->name, fname, lua_typename(inf->L,
				lua_type(inf->L, -1)));
			ret = -1;
			goto out;
		}
		ret = !(lua_toboolean(inf->L, -1)); /* back in C! */
		lua_pop(inf->L, 1); /* pop result */
	}
 out:
	return ret;
}

/**
 * init_lua_state - initalize lua_State and execute lua_file.
 *
 * @param inf
 * @param lua_file
 *
 * @return 0 if Ok, -1 otherwise.
 */
static int init_lua_state(struct ubx_block *b)
{
	int ret = -1;

	struct file_logger_info *inf =
		(struct file_logger_info *)b->private_data;

	inf->L = luaL_newstate();
	if (inf->L == NULL) {
		ubx_err(b, "failed to alloc lua_State");
		goto out;
	}

	luaL_openlibs(inf->L);

#ifdef COMPILE_IN_LOG_LUA_FILE
	ret = luaL_dostring(inf->L, (const char *) &file_logger_lua);
#else
	ret = luaL_dofile(inf->L, FILE_LOG_FILE);
#endif

	if (ret) {
		ubx_err(b, "Failed to load file_logger.lua: %s",
			lua_tostring(inf->L, -1));
		goto out;
	}
	ret = 0;

 out:
	return ret;
}


static int file_logger_init(ubx_block_t *b)
{
	int ret = -EOUTOFMEM;
	struct file_logger_info *inf;

	inf = calloc(1, sizeof(struct file_logger_info));
	if (inf == NULL)
		goto out;

	b->private_data = inf;

	if (init_lua_state(b) != 0)
		goto out_free;

	ret = call_hook(b, "init", 0, 1);
	if (ret != 0)
		goto out_free;

	/* Ok! */
	ret = 0;
	goto out;

 out_free:
	free(inf);
 out:
	return ret;
}

static int file_logger_start(ubx_block_t *b)
{
	return call_hook(b, "start", 0, 1);
}

/**
 * file_logger_step - execute lua string and call step hook
 *
 * @param b
 */
static void file_logger_step(ubx_block_t *b)
{
	call_hook(b, "step", 0, 0);
}

static void file_logger_stop(ubx_block_t *b)
{
	call_hook(b, "stop", 0, 0);
}

static void file_logger_cleanup(ubx_block_t *b)
{
	struct file_logger_info *inf = (struct file_logger_info *)b->private_data;

	call_hook(b, "cleanup", 0, 0);
	lua_close(inf->L);
	free(b->private_data);
}


/* put everything together */
ubx_proto_block_t lua_comp = {
	.name = "logging/file_logger",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = file_logger_meta,
	.configs = file_logger_conf,
	/* .ports = lua_ports, */

	/* ops */
	.init = file_logger_init,
	.start = file_logger_start,
	.step = file_logger_step,
	.stop = file_logger_stop,
	.cleanup = file_logger_cleanup,
};

static int file_logger_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &lua_comp);
}

static void file_logger_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "logging/file_logger");
}

UBX_MODULE_INIT(file_logger_mod_init)
UBX_MODULE_CLEANUP(file_logger_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
