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


--- Generate a Makefile
-- @param data
-- @param fd file to write to (optional, default: io.stdout)
function generate_type(data, fd)
   fd = fd or io.stdout
   local res, str = utils.preproc(
[[
/* example type definition */
struct $(block_name) {
	int count;
	char name[100];
};

]], { table=table, block_name=data.block_name } )

   if not res then error(str) end
   fd:write(str)
end


--- Generate a Makefile
-- @param data
-- @param fd file to write to (optional, default: io.stdout)
function generate_makefile(data, fd)
   fd = fd or io.stdout
   local str = utils.expand(
[[
ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

$block_name.so: $block_name.o $(INCLUDE_DIR)/libubx.so
	${CC} $(CFLAGS_SHARED) -o $block_name.so $block_name.o $(INCLUDE_DIR)/libubx.so

$block_name.o: $block_name.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) $block_name.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
]], { block_name=data.block_name })

   fd:write(str)
end


--- Generate a ubx block.
-- @param data input data
-- @param fd open file to write to (optional, default: io.stdout)
function generate_block(data, fd)
   fd = fd or io.stdout
   local res, str = utils.preproc(
[[
/*
 * $(block_name) microblx function block
 */

#include "ubx.h"

/* Register a dummy type "struct $(block_name)" */
#include "types/$(block_name).h"
#include "types/$(block_name).h.hexarr"

ubx_type_t types[] = {
	def_struct_type(struct $(block_name), &$(block_name)_h),
	{ NULL },
};

/* block meta information */
char $(block_name)_meta[] =
	" { doc='',"
	"   license='',"
	"   real-time=true,"
	"}";

/* declaration of block configuration */
ubx_config_t $(block_name)_config[] = {
	{ .name="config", .type_name = "struct $(block_name)" },
	{ .name="speed", .type_name = "double" },
	{ NULL },
};

/* declaration port block ports */
ubx_port_t $(block_name)_ports[] = {
	{ .name="foo", .in_type_name="unsigned int" },
	{ .name="bar", .out_type_name="int" },
	{ NULL },
};

/* define a structure that contains the block state. By assigning an
 * instance of this struct to the block private_data pointer, this
 * struct is available the hook functions. (see init)
 */
struct $(block_name)_info
{
	int dummy_state;
};

/* declare convenience functions to read/write from the ports */
def_read_fun(read_uint, unsigned int)
def_write_fun(write_int, int)

/* init */
static int $(block_name)_init(ubx_block_t *b)
{
	int ret = -1;

	/* allocate memory for the block state */
	if ((b->private_data = calloc(1, sizeof(struct $(block_name)_info)))==NULL) {
		ERR("$(block_name): failed to alloc memory");
		ret=EOUTOFMEM;
		goto out;
	}
	ret=0;
out:
	return ret;
}

/* start */
static int $(block_name)_start(ubx_block_t *b)
{
	int ret = 0;
	return ret;
}

/* stop */
static void $(block_name)_stop(ubx_block_t *b)
{
}

/* cleanup */
static void $(block_name)_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}

/* step */
static void $(block_name)_step(ubx_block_t *b)
{
	unsigned int x;
	int y;

	ubx_port_t* foo = ubx_port_get(b, "foo");
	ubx_port_t* bar = ubx_port_get(b, "bar");

	/* read from a port */
	read_uint(foo, &x);
	y = x * -2;
	write_int(bar, &y);
}


/* put everything together */
ubx_block_t $(block_name)_block = {
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

	if(ubx_block_register(ni, &$(block_name)_block) != 0)
		goto out;

	ret=0;
out:
	return ret;
}

static void $(block_name)_mod_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	const ubx_type_t *tptr;

	for(tptr=types; tptr->name!=NULL; tptr++)
		ubx_type_unregister(ni, tptr->name);

	ubx_block_unregister(ni, "$(block_name)");
}

/* declare module init and cleanup functions, so that the ubx core can
 * find these when the module is loaded/unloaded */
UBX_MODULE_INIT($(block_name)_mod_init)
UBX_MODULE_CLEANUP($(block_name)_mod_cleanup)
]], { table=table, block_name=data.block_name } )

   if not res then error(str) end
   fd:write(str)
end


--
-- Program enters here
--

local opttab=utils.proc_args(arg)
local data = {}

if #arg==1 or opttab['-h'] then usage(); os.exit(1) end

if not (opttab['-n'] and opttab['-n'][1]) then
   print("missing name (-n)"); os.exit(1)
end

data.block_name = opttab['-n'][1]

if not (opttab['-d'] and opttab['-d'][1]) then
   print("missing output directory (-d)"); os.exit(1)
end

data.outdir=opttab['-d'][1]
if not utils.file_exists(data.outdir) then
   if os.execute("mkdir -p "..data.outdir) ~= 0 then
      print("creating dir "..data.outdir.." failed")
      os.exit(1)
   end
end

if not utils.file_exists(data.outdir.."/types") then
   if os.execute("mkdir -p "..data.outdir.."/types") ~= 0 then
      print("creating dir "..data.outdir.."/types ".." failed")
      os.exit(1)
   end
end


local code_gen_table = {
   { fun=generate_type, file="types/"..data.block_name..".h" },
   { fun=generate_makefile, file="Makefile" },
   { fun=generate_block, file=data.block_name..".c" },
}

utils.foreach(function(e)
		 local file = data.outdir.."/"..e.file
		 print("generating", file)
		 local fd=assert(io.open(file, "w"))
		 e.fun(data, fd)
		 fd:close()
	      end, code_gen_table)
