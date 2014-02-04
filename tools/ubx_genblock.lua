#!/usr/bin/env luajit

utils=require "utils"
umf=require "umf"
local ts = tostring

---
--- ubx block meta-model
---
AnySpec=umf.AnySpec
NumberSpec=umf.NumberSpec
BoolSpec=umf.BoolSpec
StringSpec=umf.StringSpec
TableSpec=umf.TableSpec
ObjectSpec=umf.ObjectSpec
EnumSpec=umf.EnumSpec

block=umf.class("block")

-- types
types_spec=TableSpec{
   name="types",
   array = {
      TableSpec {
	 name="type",
	 dict={ name=StringSpec{}, class=EnumSpec{ "struct" }, doc=StringSpec{} },
	 sealed='both',
	 optional={ 'doc' },
      },
   },
   sealed='both'
}

-- configurations
configurations_spec=TableSpec{
   name="configurations",
   array = {
      TableSpec {
	 name="configuration",
	 dict={ name=StringSpec{}, type_name=StringSpec{}, len=NumberSpec{min=1}, doc=StringSpec{} },
	 sealed='both',
	 optional={ 'len', 'doc' },
      },
   },
   sealed='both'
}

-- configurations
ports_spec=TableSpec{
   name="ports",
   array = {
      TableSpec {
	 name="port",
	 dict={
	    name=StringSpec{},
	    in_type_name=StringSpec{},
	    in_data_len=NumberSpec{ min=1 },
	    out_type_name=StringSpec{},
	    out_data_len=NumberSpec{ min=1 },
	    doc=StringSpec{}
	 },
	 sealed='both',
	 optional={ 'in_type_name', 'in_data_len', 'out_type_name', 'out_data_len', 'doc' }
      },
   },
   sealed='both'
}

block_spec = ObjectSpec {
   name="block",
   type=block,
   sealed="both",
   dict={
      name=StringSpec{},
      meta_data=StringSpec{},
      cpp=BoolSpec{},
      port_cache=BoolSpec{},
      types=types_spec,
      configurations=configurations_spec,
      ports=ports_spec,
      operations=TableSpec{
	 name='operations',
	 dict={
	    start=BoolSpec{},
	    stop=BoolSpec{},
	    step=BoolSpec{},
	 },
	 sealed='both',
	 optional={ "start", "stop", "step" },
      },
   },
   optional={ 'meta_data', 'cpp', 'types', 'configurations', 'ports' },
}

--- Validate a block model.
function block:validate(verbose)
   return umf.check(self, block_spec, verbose)
end

function usage()
   print( [[
ubx_genblock: generate the code for an empty microblx block.

Usage: genblock [OPTIONS]
   -c           block specification file
   -check       only check block specification, don't generate
   -force       force regeneration of all files
		and the type header files
   -d		output directory (will be created)
   -h           show this.
]])
end

---
--- Code generation functions and templates
---

--- Generate a struct type stub
-- @param data
-- @param fd file to write to (optional, default: io.stdout)
function generate_struct_type(fd, typ)
   fd = fd or io.stdout
   local res, str = utils.preproc(
[[
/* generated type stub, extend this struct with real information */

struct $(type_name) {
	/* TODO: fill in body */
};
]], { table=table, type_name=typ.name } )

   if not res then error(str) end
   fd:write(str)
end

--- Generate type read/write helpers
-- @param block_model
-- @return string
function generate_rd_wr_helpers(bm)
   -- extract a suitable name from the type
   local function find_name(n)
      local nm = string.match(n, "%s*struct%s+([%w_]+)")
      if nm then return nm end
      return utils.trim(n)
   end

   -- remove duplicates
   local function filter_dupes(lst)
      local res = {}
      local track = {}
      for _,v in ipairs(lst) do
	 if not track[v] then res[#res+1] = v; track[v]=true; end
      end
      return res
   end

   local res = {}
   for _,p in ipairs(bm.ports or {}) do
      if p.in_type_name then
	 if not p.in_data_len or p.in_data_len == 1 then
	    res[#res+1] = utils.expand("def_read_fun(read_$name, $type_name)",
				       { name=find_name(p.name), type_name=p.in_type_name })
	 else -- in_data_len > 1
	    res[#res+1] = utils.expand("def_read_arr_fun(read_$name_$len, $type_name, $len)",
				       { name=find_name(p.name), type_name=p.in_type_name, len=p.in_data_len })
	 end
      elseif p.out_type_name then
	 if not p.out_data_len or p.out_data_len == 1 then
	    res[#res+1] = utils.expand("def_write_fun(write_$name, $type_name)",
				       { name=find_name(p.name), type_name=p.out_type_name })
	 else -- ou_data_len > 1
	    res[#res+1] = utils.expand("def_write_arr_fun(write_$name_$len, $type_name, $len)",
				       { name=find_name(p.name), type_name=p.out_type_name, len=p.out_data_len })
	 end
      end
   end

   return table.concat(filter_dupes(res), "\n")
end


--- Generate a Makefile
-- @param bm block model
-- @param fd file to write to (optional, default: io.stdout)
function generate_makefile(fd, bm, c_ext, h_ext)
   fd = fd or io.stdout
   local cc
   if bm.cpp then cc = "CPP" else cc = "CC" end
   local str = utils.expand(
      [[
ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

# silence warnings due to unused rd/write helpers: remove once used!
CFLAGS:=$(CFLAGS) -Wno-unused-function

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

$block_name.so: $block_name.o $(INCLUDE_DIR)/libubx.so
	${$cc} $(CFLAGS_SHARED) -o $block_name.so $block_name.o $(INCLUDE_DIR)/libubx.so

$block_name.o: $block_name$h_ext $block_name$c_ext $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${$cc} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) $block_name$c_ext

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
]], { block_name=bm.name, cc=cc, c_ext=c_ext, h_ext=h_ext })
   fd:write(str)
end

--- Generate an entry in a port declaration.
-- Moved out due to improve readability of the conditional
-- @param t type entry
-- @return designated C initializer string
function gen_port_decl(t)
   t.in_data_len = t.in_data_len or 1
   t.out_data_len = t.out_data_len or 1
   t.doc = t.doc or ''

   if t.in_type_name and t.out_type_name then
      return utils.expand('{ .name="$name", .out_type_name="$out_type_name", .out_data_len=$out_data_len, .in_type_name="$in_type_name", .in_data_len=$in_data_len, .doc="$doc" },', t)
   elseif t.in_type_name then
      return utils.expand('{ .name="$name", .in_type_name="$in_type_name", .in_data_len=$in_data_len, .doc="$doc"  },', t)
   elseif t.out_type_name then
      return utils.expand('{ .name="$name", .out_type_name="$out_type_name", .out_data_len=$out_data_len, .doc="$doc"  },', t)
   end
end

--- Generate the interface of an ubx block.
-- @param bm block model
-- @param fd open file to write to (optional, default: io.stdout)
function generate_block_if(fd, bm)
   fd = fd or io.stdout
   local res, str = utils.preproc(
[[
/*
 * $(bm.name) microblx function block (autogenerated, don't edit)
 */

#include "ubx.h"

/* includes types and type metadata */
@ for _,t in ipairs(bm.types or {}) do
#include "types/$(t.name).h"
#include "types/$(t.name).h.hexarr"
@ end

ubx_type_t types[] = {
@ for _,t in ipairs(bm.types or {}) do
	def_struct_type(struct $(t.name), &$(t.name)_h),
@ end
	{ NULL },
};

/* block meta information */
char $(bm.name)_meta[] =
	" { doc='',"
	"   real-time=true,"
	"}";

/* declaration of block configuration */
ubx_config_t $(bm.name)_config[] = {
@ for _,c in ipairs(bm.configurations or {}) do
@       c.doc=c.doc or ""
	{ .name="$(c.name)", .type_name = "$(c.type_name)", .doc="$(c.doc)" },
@ end
	{ NULL },
};

/* declaration port block ports */
ubx_port_t $(bm.name)_ports[] = {
@ for _,p in ipairs(bm.ports or {}) do
	$(gen_port_decl(p))
@ end
	{ NULL },
};

/* declare a struct port_cache */
struct $(bm.name)_port_cache {
@ for _,t in ipairs(bm.ports or {}) do
	ubx_port_t* $(t.name);
@ end
};

/* declare a helper function to update the port cache this is necessary
 * because the port ptrs can change if ports are dynamically added or
 * removed. This function should hence be called after all
 * initialization is done, i.e. typically in 'start'
 */
static void update_port_cache(ubx_block_t *b, struct $(bm.name)_port_cache *pc)
{
@ for _,t in ipairs(bm.ports or {}) do
	pc->$(t.name) = ubx_port_get(b, "$(t.name)");
@ end
}


/* for each port type, declare convenience functions to read/write from ports */
$(generate_rd_wr_helpers(bm))

/* block operation forward declarations */
int $(bm.name)_init(ubx_block_t *b);
@ if bm.operations.start then
int $(bm.name)_start(ubx_block_t *b);
@ end
@ if bm.operations.stop then
void $(bm.name)_stop(ubx_block_t *b);
@ end
void $(bm.name)_cleanup(ubx_block_t *b);
@ if bm.operations.step then
void $(bm.name)_step(ubx_block_t *b);
@ end


/* put everything together */
ubx_block_t $(bm.name)_block = {
	.name = "$(bm.name)",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = $(bm.name)_meta,
	.configs = $(bm.name)_config,
	.ports = $(bm.name)_ports,

	/* ops */
	.init = $(bm.name)_init,
@ if bm.operations.start then
	.start = $(bm.name)_start,
@ end
@ if bm.operations.stop then
	.stop = $(bm.name)_stop,
@ end
	.cleanup = $(bm.name)_cleanup,
@ if bm.operations.step then
	.step = $(bm.name)_step,
@ end
};


/* $(bm.name) module init and cleanup functions */
int $(bm.name)_mod_init(ubx_node_info_t* ni)
{
	DBG(" ");
	int ret = -1;
	ubx_type_t *tptr;

	for(tptr=types; tptr->name!=NULL; tptr++) {
		if(ubx_type_register(ni, tptr) != 0) {
			goto out;
		}
	}

	if(ubx_block_register(ni, &$(bm.name)_block) != 0)
		goto out;

	ret=0;
out:
	return ret;
}

void $(bm.name)_mod_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	const ubx_type_t *tptr;

	for(tptr=types; tptr->name!=NULL; tptr++)
		ubx_type_unregister(ni, tptr->name);

	ubx_block_unregister(ni, "$(bm.name)");
}

/* declare module init and cleanup functions, so that the ubx core can
 * find these when the module is loaded/unloaded */
UBX_MODULE_INIT($(bm.name)_mod_init)
UBX_MODULE_CLEANUP($(bm.name)_mod_cleanup)
]], { gen_port_decl=gen_port_decl, ipairs=ipairs, table=table,
      bm=bm, generate_rd_wr_helpers=generate_rd_wr_helpers } )

   if not res then error(str) end
   fd:write(str)
end


--- Generate the interface of an ubx block.
-- @param bm block model
-- @param fd open file to write to (optional, default: io.stdout)
function generate_block_body(fd, bm)
   fd = fd or io.stdout
   local res, str = utils.preproc(
[[

@ if bm.cpp then
#include "$(bm.name).hpp"
@ else
#include "$(bm.name).h"
@ end

/* edit and uncomment this:
 * UBX_MODULE_LICENSE_SPDX(GPL-2.0+)
 */

/* define a structure for holding the block local state. By assigning an
 * instance of this struct to the block private_data pointer (see init), this
 * information becomes accessible within the hook functions.
 */
struct $(bm.name)_info
{
	/* add custom block local data here */

@ if bm.port_cache then
	/* this is to have fast access to ports for reading and writing, without
	 * needing a hash table lookup */
	struct $(bm.name)_port_cache ports;
@ end
};

/* init */
int $(bm.name)_init(ubx_block_t *b)
{
	int ret = -1;
	struct $(bm.name)_info *inf;

	/* allocate memory for the block local state */
	if ((inf = (struct $(bm.name)_info*)calloc(1, sizeof(struct $(bm.name)_info)))==NULL) {
		ERR("$(bm.name): failed to alloc memory");
		ret=EOUTOFMEM;
		goto out;
	}
	b->private_data=inf;
	update_port_cache(b, &inf->ports);
	ret=0;
out:
	return ret;
}

@ if bm.operations.start then
/* start */
int $(bm.name)_start(ubx_block_t *b)
{
	/* struct $(bm.name)_info *inf = (struct $(bm.name)_info*) b->private_data; */
	int ret = 0;
	return ret;
}
@ end

@ if bm.operations.stop then
/* stop */
void $(bm.name)_stop(ubx_block_t *b)
{
	/* struct $(bm.name)_info *inf = (struct $(bm.name)_info*) b->private_data; */
}
@ end

/* cleanup */
void $(bm.name)_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}

@ if bm.operations.step then
/* step */
void $(bm.name)_step(ubx_block_t *b)
{
	/*
	struct $(bm.name)_info *inf = (struct $(bm.name)_info*) b->private_data;
	*/
}
@ end

]], { table=table, bm=bm } )

   if not res then error(str) end
   fd:write(str)
end

--- Generate a simple blockdiagram interface of an ubx block.
-- @param bm block model
-- @param fd open file to write to (optional, default: io.stdout)
function generate_bd_system(fd, bm, outdir)
   fd = fd or io.stdout
   local res, str = utils.preproc(
[[
-- a minimal blockdiagram to start the block

return bd.system
{
   imports = {
      "std_types/stdtypes/stdtypes.so",
      "std_blocks/ptrig/ptrig.so",
      "std_blocks/lfds_buffers/lfds_cyclic.so",
      "std_blocks/logging/file_logger.so",
      "$(outdir)/$(bm.name).so",
   },

   blocks = {
      { name="$(bm.name)_1", type="$(bm.name)" },
   },
}

]], { table=table, bm=bm, outdir=outdir } )

   if not res then error(str) end
   fd:write(str)
end


--- Generate code according to the given code generation table.
-- For each entry in code_gen_table, open file and call fun passing
-- block_model as first and the file handle as second arg
-- @param cgt code generate table
-- @param outdir directory prefix
-- @param force_overwrite ignore overwrite flag on individual entries
function generate(cgt, outdir, force_overwrite)

   local function file_open(fn, overwrite)
      if not overwrite and utils.file_exists(fn) then return false end
      return assert(io.open(fn, "w"))
   end

   utils.foreach(function(e)
		    local file = outdir.."/"..e.file
		    local fd = file_open(file, e.overwrite or force_overwrite)
		    if fd then
		       print("    generating ".. file)
		       e.fun(fd, unpack(e.funargs))
		       fd:close()
		    else
		       print("    skipping existing "..file)
		    end
		 end, cgt)
end


--
-- Program enters here
--

local opttab=utils.proc_args(arg)

local block_model_file
local force_overwrite
local c_ext = '.c'
local h_ext = '.h'

if #arg==1 or opttab['-h'] then usage(); os.exit(1) end

-- load and check config file
if not (opttab['-c'] and opttab['-c'][1]) then
   print("missing config file option (-c)"); os.exit(1)
end

block_model_file=opttab['-c'][1]

local suc, block_model = pcall(dofile, block_model_file)

if not suc then
   print(block_model)
   print("ubx_genblock failed to load block config file "..ts(block_model_file))
   os.exit(1)
end

if block_model:validate(false) > 0 then
   block_model:validate(true)
   os.exit(1)
end

-- only check, don't generate
if opttab['-check'] then
   print("Ok!")
   os.exit(1)
end

if opttab['-force'] then
   force_overwrite=true
end

-- handle output directory
if not (opttab['-d'] and opttab['-d'][1]) then
   print("missing output directory (-d)"); os.exit(1)
end

local outdir=opttab['-d'][1]

if not utils.file_exists(outdir) then
   if os.execute("mkdir -p "..outdir) ~= 0 then
      print("creating dir "..outdir.." failed")
      os.exit(1)
   end
end

if not utils.file_exists(outdir.."/types") then
   if os.execute("mkdir -p "..outdir.."/types") ~= 0 then
      print("creating dir "..outdir.."/types ".." failed")
      os.exit(1)
   end
end

if block_model.cpp then c_ext = '.cpp' end
if block_model.cpp then h_ext = '.hpp' end

-- static part
local codegen_tab = {
   { fun=generate_makefile, funargs={ block_model, c_ext, h_ext }, file="Makefile", overwrite=false },
   { fun=generate_block_if, funargs={ block_model } , file=block_model.name..h_ext, overwrite=true },
   { fun=generate_block_body, funargs={ block_model }, file=block_model.name..c_ext, overwrite=false },
   { fun=generate_bd_system, funargs={ block_model, outdir }, file=block_model.name..".usc", overwrite=false },
}

-- add types
for i,t in ipairs(block_model.types or {}) do
   if t.class=='struct' then
      codegen_tab[#codegen_tab+1] =
	 { fun=generate_struct_type, funargs={ t }, file="types/"..t.name..".h", overwrite=false }
   elseif t.type=='enum' then
      print("   ERROR: enum not yet supported")
   end
end

generate(codegen_tab, outdir, force_overwrite)
