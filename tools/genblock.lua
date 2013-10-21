utils=require "utils"

function usage()
   print( [=[
genblock: generate the code for an empty microblx block.

usage genblock OPTIONS <conf file>
   -n           name of block
   -d		output directory (must exist)
   -h           show this.
]=])
end

local opttab=utils.proc_args(arg)

if #arg==1 or opttab['-h'] then usage(); os.exit(1) end

if not (opttab['-n'] and opttab['-n'][1]) then
   print("missing name (-n)"); os.exit(1)
end
local block_name = opttab['-n'][1]

local outdir="."
if opttab['-d'] then
   outdir=opttab['-d'][1]
   if not utils.file_exists(outdir) then
      if os.system("mkdir -p "..outdir) ~= 0 then
	 print("creating dir "..outdir.." failed")
	 os.exit(1)
      end
   end
end



local res, str = utils.preproc(
[[
/*
 * $(block_name)
 */

#include "ubx.h"

/* Adding types example:
*
* #include "types/$(block_name)_config.h"
* #include "types/$(block_name).h.hexarr"
*
* ubx_type_t types[] = {
*	def_struct_type(struct $(block_name)_config, &$(block_name)_config_h);
*	{ NULL },
* };
*/

/* block meta information *.
char $(block_name)_meta[] =
 " { doc='',"
 "   license='',"
 "   real-time=true,
 "}";

ubx_config_t $(block_name)_config[] = {
	{ .name="config", .type_name = "struct $(block_name)_config" },
	{ NULL },
};

ubx_port_t rnd_ports[] = {
	{ .name="foo", .in_type_name="unsigned int" },
	{ .name="bar", .out_type_name="int" },
	{ NULL },
};

/* convenience functions to read/write from the ports */
def_read_fun(read_uint, unsigned int)
def_write_fun(write_int, int)


static int $(block_name)_init(ubx_block_t *b)
{
	int ret = 0;
	return ret;
}

static int $(block_name)_start(ubx_block_t *b)
{
	int ret = 0;
	return ret;
}

static void $(block_name)_stop(ubx_block_t *b)
{
}

static void $(block_name)_cleanup(ubx_block_t *b)
{
}

static void $(block_name)_step(ubx_block_t *b)
{
}

/* put everything together */
ubx_block_t random_comp = {
	.name = "$(block_name)",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = $(block_name)_meta,
	.configs = $(block_name)_config,
	.ports = $(block_name)_ports,

	/* ops */
	.init = $(block_name)_init,
	.start = $(block_name)_start,
	.stop = $(block_name)_stop,
	.cleanup = $(block_name)_cleanup,
	.step = $(block_name)_step,
};


/* $(block_name) module init and cleanup functions */

static int $(block_name)_mod_init(ubx_node_info_t* ni)
{
	DBG(" ");
	int ret = -1;
	ubx_type_t *tptr;

	for(tptr=types; tptr->name!=NULL; tptr++) {
		if(ubx_type_register(ni, tptr) != 0) {
			goto out;
		}
	}

	if(ubx_type_register(ni, &$(block_name)_config_type) != 0)
		goto out;

	ret=0;
out:
	return ret;
}

static void $(block_name)_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	const ubx_type_t *tptr;

	for(tptr=types; tptr->name!=NULL; tptr++)
		ubx_type_unregister(ni, tptr->name);

	ubx_block_unregister(ni, "$(block_name)");
}



UBX_MODULE_INIT($(block_name)_mod_init)
UBX_MODULE_CLEANUP($(block_name)_mod_cleanup)

]], { table=table, block_name=block_name } )

print(str)
