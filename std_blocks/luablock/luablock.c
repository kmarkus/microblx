/*
 * A generic luajit based block.
 */

#define DEBUG

#include <lauxlib.h>
#include <lualib.h>
#include <lua.h>

#include <stdio.h>
#include <stdlib.h>

#include "ubx.h"

#define EXEC_STR_BUFF_SIZE	(16 * 1024 * 1024)

ubx_port_t lua_ports[] = {
	{ .name = "exec_str", .in_type_name = "char", .out_type_name = "int", .in_data_len = 16777216 },
	{ 0 }
};

def_write_fun(write_int, int)

ubx_config_t lua_conf[] = {
	{ .name = "lua_file", .type_name = "char" },
	{ .name = "lua_str", .type_name = "char" },
	{ 0 }
};

char luablock_meta[] =
	"{ doc='A generic luajit based block',"
	"  realtime=false,"
	"}";

struct luablock_info {
	struct ubx_node_info *ni;
	struct lua_State *L;
	ubx_data_t *exec_str_buff;
};

const char *predef_hooks =
	"function init(b) return true end\n"
	"function start(b) return true end\n"
	"function step(b) end\n"
	"function stop(b) end\n"
	"function cleanup(b) end\n";

/**
 * @brief: call a hook with fname.
 *
 * @param block (is passed on a first arg)
 * @param fname name of function to call
 * @param require_fun raise an error if function fname does not exist.
 * @param require_res if 1, require a boolean valued result.
 * @return -1 in case of error, 0 otherwise.
 */
int call_hook(ubx_block_t *b, const char *fname, int require_fun, int require_res)
{
	int ret = 0;
	struct luablock_info *inf = (struct luablock_info *)b->private_data;
	int num_res = (require_res != 0) ? 1 : 0;

	lua_getglobal(inf->L, fname);

	if (lua_isnil(inf->L, -1)) {
		lua_pop(inf->L, 1);
		if (require_fun)
			ubx_err(b, "%s: no (required) Lua function %s", b->name, fname);
		else
			goto out;
	}

	lua_pushlightuserdata(inf->L, (void *)b);

	if (lua_pcall(inf->L, 1, num_res, 0) != 0) {
		ubx_err(b, "calling fun %s: %s", fname, lua_tostring(inf->L, -1));
		ret = -1;
		goto out;
	}

	if (require_res) {
		if (!lua_isboolean(inf->L, -1)) {
			ubx_err(b, "%s: %s must return a bool but returned a %s",
				b->name, fname, lua_typename(inf->L, lua_type(inf->L, -1)));
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
static int init_lua_state(struct ubx_block *b,
			  const char *lua_file,
			  const char *lua_str)
{
	int ret = -1;
	struct luablock_info *inf = (struct luablock_info *)b->private_data;

	inf->L = luaL_newstate();
	if (inf->L == NULL) {
		ubx_err(b, "failed to alloc lua_State");
		goto out;
	}

	luaL_openlibs(inf->L);

	ret = luaL_dostring(inf->L, predef_hooks);
	if (ret != 0) {
		ubx_err(b, "failed to predefine hooks");
		goto out;
	}

	if (lua_file) {
		ret = luaL_dofile(inf->L, lua_file);
		if (ret != 0) {
			ubx_err(b, "Failed to load lua_file '%s': %s\n", lua_file, lua_tostring(inf->L, -1));
			goto out;
		}
	}

	if (lua_str) {
		ret = luaL_dostring(inf->L, lua_str);
		if (ret != 0) {
			ubx_err(b, "Failed to load lua_str '%s': %s\n", lua_str, lua_tostring(inf->L, -1));
			goto out;
		}
	}

	ret = 0;
 out:
	return ret;
}


static int luablock_init(ubx_block_t *b)
{
	const char *lua_file = NULL;
	const char *lua_str = NULL;
	long len;

	int ret = -EOUTOFMEM;
	struct luablock_info *inf;

	inf = calloc(1, sizeof(struct luablock_info));
	if (inf == NULL)
		goto out;

	b->private_data = inf;

	len = cfg_getptr_char(b, "lua_file", &lua_file);
	if (len < 0)
		goto out_free1;

	lua_file = (len > 0) ? lua_file : NULL;

	len = cfg_getptr_char(b, "lua_str", &lua_str);
	if (len < 0)
		goto out_free1;

	lua_str = (len > 0) ? lua_str : NULL;

	inf->exec_str_buff = ubx_data_alloc(b->ni, "char", EXEC_STR_BUFF_SIZE);
	if (inf->exec_str_buff == NULL) {
		ubx_err(b, "failed to allocate exec_str buffer");
		goto out_free1;
	}

	if (init_lua_state(b, lua_file, lua_str) != 0)
		goto out_free2;

	ret = call_hook(b, "init", 0, 1);
	if (ret != 0)
		goto out_free2;

	/* Ok! */
	ret = 0;
	goto out;

 out_free2:
	free(inf->exec_str_buff);
 out_free1:
	free(inf);
 out:
	return ret;
}

static int luablock_start(ubx_block_t *b)
{
	return call_hook(b, "start", 0, 1);
}

/**
 * luablock_step - execute lua string and call step hook
 *
 * @param b
 */
static void luablock_step(ubx_block_t *b)
{
	int len = 0, ret;
	struct luablock_info *inf = (struct luablock_info *)b->private_data;

	/* any lua code to execute */
	ubx_port_t *p_exec_str = ubx_port_get(b, "exec_str");

	len = __port_read(p_exec_str, inf->exec_str_buff);
	if (len > 0) {
		ret = luaL_dostring(inf->L, inf->exec_str_buff->data);
		if (ret != 0) {
			ubx_err(b, "Failed to exec_str: %s", lua_tostring(inf->L, -1));
			goto out;
		}
	}
	call_hook(b, "step", 0, 0);
 out:
	/* TODO: fix this. realloc could have changed port addr */
	if (len > 0) {
		p_exec_str = ubx_port_get(b, "exec_str");
		write_int(p_exec_str, &ret);
	}
}

static void luablock_stop(ubx_block_t *b)
{
	call_hook(b, "stop", 0, 0);
}

static void luablock_cleanup(ubx_block_t *b)
{
	struct luablock_info *inf = (struct luablock_info *)b->private_data;

	call_hook(b, "cleanup", 0, 0);
	lua_close(inf->L);
	ubx_data_free(inf->exec_str_buff);
	free(b->private_data);
}


/* put everything together */
ubx_block_t lua_comp = {
	.name = "lua/luablock",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = luablock_meta,
	.configs = lua_conf,
	.ports = lua_ports,

	/* ops */
	.init = luablock_init,
	.start = luablock_start,
	.step = luablock_step,
	.stop = luablock_stop,
	.cleanup = luablock_cleanup,
};

static int lua_init(ubx_node_info_t *ni)
{
	return ubx_block_register(ni, &lua_comp);
}

static void lua_cleanup(ubx_node_info_t *ni)
{
	ubx_block_unregister(ni, "lua/luablock");
}

UBX_MODULE_INIT(lua_init)
UBX_MODULE_CLEANUP(lua_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
