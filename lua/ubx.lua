--
-- microblx: embedded, real-time safe, reflective function blocks.
-- Copyright (C) 2013 Markus Klotzbuecher <markus.klotzbuecher@mech.kuleuven.be>
--
-- This program is free software: you can redistribute it and/or
-- modify it under the terms of the GNU General Public License as
-- published by the Free Software Foundation, either version 3 of the
-- License, or (at your option) any later version.
--
-- As a special exception, if other files instantiate templates or use
-- macros or inline functions from this file, or you compile this file
-- and link it with other works to produce a work based on this file,
-- this file does not by itself cause the resulting work to be covered
-- by the GNU General Public License. However the source code for this
-- file must still be made available in accordance with the GNU
-- General Public License.
--
-- This exception does not invalidate any other reasons why a work
-- based on this file might be covered by the GNU General Public
-- License.
--
-- This program is distributed in the hope that it will be useful, but
-- WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
-- General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program.  If not, see
-- <http://www.gnu.org/licenses/>.

---
--- Microblx Lua Interface
---

local ffi=require "ffi"
local cdata = require "cdata"
local utils= require "utils"
local ubx_env = require "ubx_env"
local ts=tostring
local ac=require "ansicolors"
local time=require"time"
--require "strict"

local M={}

--- set to false to disable terminal colors
M.color = true

local function red(str, bright) if M.color then str = ac.red(str); if bright then str=ac.bright(str) end end return str end
local function blue(str, bright) if M.color then str = ac.blue(str); if bright then str=ac.bright(str) end end return str end
local function cyan(str, bright) if M.color then str = ac.cyan(str); if bright then str=ac.bright(str) end end return str end
local function white(str, bright) if M.color then str = ac.white(str); if bright then str=ac.bright(str) end end return str end
local function green(str, bright) if M.color then str = ac.green(str); if bright then str=ac.bright(str) end end return str end
local function yellow(str, bright) if M.color then str = ac.yellow(str); if bright then str=ac.bright(str) end end return str end
local function magenta(str, bright) if M.color then str = ac.magenta(str); if bright then str=ac.bright(str) end end return str end

M.red=red; M.blue=blue; M.cyan=cyan; M.white=white; M.green=green; M.yellow=yellow; M.magenta=magenta;

local setup_enums

------------------------------------------------------------------------------
--                           helpers
------------------------------------------------------------------------------

--- Read the entire contents of a file.
-- @param file name of file
-- @return string contents
local function read_file(file)
   local f = assert(io.open(ubx_env.get_ubx_root()..file, "rb"))
   local data = f:read("*all")
   f:close()
   return data
end

--- Compute the MD5 checksum of the given string
-- @param str string to hash
-- @return hex md5
function M.md5(str)
   local res = ffi.new("unsigned char[16]")
   M.ubx.md5(str, #str, res)
   return utils.str_to_hexstr(ffi.string(res, 16))
end

--- Setup Enums
-- called internally after loading libubx
local function setup_enums()
   if M.ubx==nil then error("setup_enums called before loading libubx") end
   M.retval_tostr = {
      [0] ='OK',
      -- [ffi.C.PORT_READ_NODATA]	  ='PORT_READ_NODATA',
      -- [ffi.C.PORT_READ_NEWDATA]   ='PORT_READ_NEWDATA',
      [ffi.C.EPORT_INVALID]	  ='EPORT_INVALID',
      [ffi.C.EPORT_INVALID_TYPE]  ='EPORT_INVALID_TYPE',
      [ffi.C.EINVALID_BLOCK_TYPE] ='EINVALID_BLOCK_TYPE',
      [ffi.C.ENOSUCHBLOCK]	  ='ENOSUCHBLOCK',
      [ffi.C.EALREADY_REGISTERED] ='EALREADY_REGISTERED',
      [ffi.C.EOUTOFMEM]		  ='EOUTOFMEM',
      [ffi.C.EINVALID_CONFIG]	  ='EINVALID_CONFIG'
   }

   M.block_type_tostr={
      [ffi.C.BLOCK_TYPE_COMPUTATION]="cblock",
      [ffi.C.BLOCK_TYPE_INTERACTION]="iblock",
   }

   M.type_class_tostr={
      [ffi.C.TYPE_CLASS_BASIC]='basic',
      [ffi.C.TYPE_CLASS_STRUCT]='struct',
      [ffi.C.TYPE_CLASS_CUSTOM]='custom'
   }

   M.port_attrs_tostr={
      [ffi.C.PORT_DIR_IN]='in',
      [ffi.C.PORT_DIR_OUT]='out',
   }

   M.block_state_tostr={
      [ffi.C.BLOCK_STATE_PREINIT]='preinit',
      [ffi.C.BLOCK_STATE_INACTIVE]='inactive',
      [ffi.C.BLOCK_STATE_ACTIVE]='active'
   }
end

-- load ubx_types and library
ffi.cdef(read_file("src/uthash_ffi.h"))
ffi.cdef(read_file("src/ubx_types.h"))
ffi.cdef(read_file("src/ubx_proto.h"))
local ubx=ffi.load(ubx_env.get_ubx_root().."src/libubx.so")

setmetatable(M, { __index=function(t,k) return ubx["ubx_"..k] end })


ffi.cdef [[
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
]]

M.ubx=ubx
setup_enums()

--- Safely convert a char* to a lua string.
-- @param charptr C style string char* or char[]
-- @param return lua string
function M.safe_tostr(charptr)
   if charptr == nil then return "" end
   return ffi.string(charptr)
end

function M.is_proto(b) return b.prototype==nil end	 --- Is protoype block predicate.
function M.is_instance(b) return b.prototype~=nil end	 --- Is instance block predicate.
function M.is_cblock(b) return b.type==ffi.C.BLOCK_TYPE_COMPUTATION end  --- Is computational block predicate.
function M.is_iblock(b) return b.type==ffi.C.BLOCK_TYPE_INTERACTION end  --- Is interaction block predicate.
function M.is_cblock_instance(b) return M.is_cblock(b) and not M.is_proto(b) end  --- Is computational block instance predicate.
function M.is_iblock_instance(b) return M.is_iblock(b) and not M.is_proto(b) end  --- Is interaction block instance predicate.
function M.is_cblock_proto(b) return M.is_cblock(b) and M.is_proto(b) end --- Is computational block prototype predicate.
function M.is_iblock_proto(b) return M.is_iblock(b) and M.is_proto(b) end --- Is interaction block prototype predicate.

-- Port predicates
function M.is_outport(p) return p.out_type_name ~= nil end
function M.is_inport(p) return p.in_type_name ~= nil end
function M.is_inoutport(p) return M.is_outport(p) and M.is_inport(p) end


------------------------------------------------------------------------------
--                           OS API
------------------------------------------------------------------------------

--- Retrieve the current time using clock_gettime(CLOCK_MONOTONIC).
-- @param ts struct ubx_timespec out parameter (optional)
-- @return struct ubx_timespec with current time
function M.clock_mono_gettime(ts)
   ts = ts or ffi.new("struct ubx_timespec")
   ubx.ubx_clock_mono_gettime(ts)
   return ts
end

local function to_sec(sec, nsec)
   return tonumber(sec)+tonumber(nsec)/time.ns_per_s
end

local ubx_timespec_mt = {
   __tostring = function (ts) return tonumber(ubx.ubx_ts_to_double(ts)) end,
   __add = function (t1, t2) local s,ns = time.add(t1, t2); return to_sec(s,ns) end,
   __sub = function (t1, t2) local s,ns = time.sub(t1, t2); return to_sec(s,ns) end,
   __mul = function (t1, t2) local s,ns = time.mul(t1, t2); return to_sec(s,ns) end,
   __div = function (t1, d) local s,ns = time.div(t1, d); return to_sec(s,ns) end,
   __eq = function(op1, op2) return time.cmp(op1, op2)==0 end,
   __lt = function(op1, op2) return time.cmp(op1, op2)==-1 end,
   __le =
      function(op1, op2)
	 local res = time.cmp(op1, op2)
	 return res == 0 or res == -1
      end,

   __index = {
      normalize = time.normalize,
      tous = time.ts2us,
   },
}
ffi.metatype("struct ubx_timespec", ubx_timespec_mt)



------------------------------------------------------------------------------
--                           Node API
------------------------------------------------------------------------------

--- Create and initalize a new node_info struct
-- @param name name of node
-- @return ubx_node_info_t
function M.node_create(name)
   local ni=ffi.new("ubx_node_info_t")
   assert(ubx.ubx_node_init(ni, name)==0, "node_create failed")
   return ni
end

--- Load and initialize a ubx module.
-- @param ni node_info pointer into which to load module
-- @param libfile module file to load
function M.load_module(ni, libfile)
   local res = ubx.ubx_module_load(ni, libfile)
   if res ~= 0 then
      error(red("loading module ", true)..magenta(ts(libfile))..red(" failed", true))
   end
   M.ffi_load_types(ni)
end

--- Cleanup a node: cleanup and remove instances and unload modules.
-- @param ni node info
function M.node_cleanup(ni)
   ubx.ubx_node_cleanup(ni)
   collectgarbage("collect")
end

--- Create a new computational block.
-- @param ni node_info ptr
-- @param type of block to create
-- @param name name of block
-- @return new computational block
function M.block_create(ni, type, name, conf)
   local b=ubx.ubx_block_create(ni, type, name)
   if b==nil then error("failed to create block "..ts(name).." of type "..ts(type)) end
   if conf then M.set_config_tab(b, conf) end
   return b
end

function M.block_get(ni, bname)
   local b = ubx.ubx_block_get(ni, bname)
   if b==nil then error("block_get: no block with name '"..ts(bname).."'") end
   return b
end

--- Unload a block: bring it to state preinit and call ubx_block_rm
function M.block_unload(ni, name)
   local b=M.block_get(ni, name)
   if b==nil then error("no block "..tostring(name).." found") end
   if b.block_state==ffi.C.BLOCK_STATE_ACTIVE then M.block_stop(b) end
   if b.block_state==ffi.C.BLOCK_STATE_INACTIVE then M.block_cleanup(b) end
   if M.block_rm(ni, name) ~= 0 then error("block_unload: ubx_block_rm failed for '"..name.."'") end
end

--- Determine the number of blocks
function M.num_blocks(ni)
   local num_cb, num_ib, inv = 0,0,0
   M.blocks_map(ni,
		function (b)
		   if b.type==ffi.C.BLOCK_TYPE_COMPUTATION then num_cb=num_cb+1
		   elseif b.type==ffi.C.BLOCK_TYPE_INTERACTION then num_ib=num_ib+1
		   else inv=inv+1 end
		end)
   return num_cb, num_ib, inv
end

function M.num_types(ni) return ubx.ubx_num_types(ni) end


-- add Lua OO methods
local ubx_node_info_mt = {
   __tostring = function(ni)
      local num_cb, num_ib, inv = M.num_blocks(ni)
      local num_types = M.num_types(ni)

      return string.format("%s <node>: #blocks: %d (#cb: %d, #ib: %d), #types: %d",
			   green(M.safe_tostr(ni.name)), num_cb + num_ib, num_cb, num_ib, num_types)
   end,
   __index = {
      get_name = function (ni) return M.safe_tostr(ni.name) end,
      load_module = M.load_module,
      block_create = M.block_create,
      block_unload = M.block_unload,
      cleanup = M.node_cleanup,
      b = M.block_get,
      block_get = M.block_get,
   },
}
ffi.metatype("struct ubx_node_info", ubx_node_info_mt)


------------------------------------------------------------------------------
--                           Block API
------------------------------------------------------------------------------

function block_state_color(sstr)
   if sstr == 'preinit' then return blue(sstr, true)
   elseif sstr == 'inactive' then return red(sstr)
   elseif sstr == 'active' then return green(sstr, true)
   else
      error(red("unknown state"..ts(s)))
   end
end

--- Convert a block to a Lua table.
function M.block_totab(b)
   if b==nil then error("NULL block") end
   local res = {}
   res.name=M.safe_tostr(b.name)
   res.meta_data=M.safe_tostr(b.meta_data)
   res.block_type=M.block_type_tostr[b.type]
   res.state=M.block_state_tostr[b.block_state]
   if b.prototype~=nil then res.prototype=M.safe_tostr(b.prototype) else res.prototype="<prototype>" end

   if b.type==ffi.C.BLOCK_TYPE_COMPUTATION then
      res.stat_num_steps=tonumber(b.stat_num_steps)
   elseif b.type==ffi.C.BLOCK_TYPE_INTERACTION then
      res.stat_num_reads=tonumber(b.stat_num_reads)
      res.stat_num_writes=tonumber(b.stat_num_writes)
   end

   res.ports=M.ports_map(b, M.port_totab)
   res.configs=M.configs_map(b, M.config_totab)

   return res
end

--- Pretty print a block.
-- @param b block to convert to string.
-- @return string
function M.block_tostr(b)
   local bt=M.block_totab(b)
   local fmt="%s (type=%s, state=%s, prototype=%s)"
   local res=fmt:format(green(bt.name), bt.block_type, block_state_color(bt.state), blue(bt.prototype), bt.stat_num_steps)
   return res
end

function M.block_port_get (b, n)
   local res = ubx.ubx_port_get(b, n)
   if res==nil then error("port_get: no port with name '"..ts(n).."'") end
   return res
end
M.port_get = M.block_port_get

function M.block_config_get (b, n)
   local res = ubx.ubx_config_get(b, n)
   if res==nil then error("config_get: no config with name '"..ts(n).."'") end
   return res
end
M.config_get = M.block_config_get

-- add Lua OO methods
local ubx_block_mt = {
   __tostring = M.block_tostr,
   __index = {
      get_name = function (b) return M.safe_tostr(b.name) end,
      get_meta = function (b) return M.safe_tostr(b.meta) end,
      get_block_state = function (b) return M.block_state_tostr[b.block_state] end,
      get_block_type = function (b) return M.block_type_tostr[b.type] end,

      p = M.block_port_get,
      port_get = M.block_port_get,
      port_add = ubx.ubx_port_add,
      port_rm = ubx.ubx_port_rm,

      c = M.block_config_get,
      config_get = M.block_config_get,
      config_add = ubx.ubx_config_add,
      config_rm = ubx.ubx_config_rm,

      do_init = ubx.ubx_block_init,
      do_start = ubx.ubx_block_start,
      do_stop = ubx.ubx_block_stop,
      do_cleanup = ubx.ubx_block_cleanup,
      do_step = ubx.ubx_cblock_step,
   }
}
ffi.metatype("struct ubx_block", ubx_block_mt)

------------------------------------------------------------------------------
--                           Data type handling
------------------------------------------------------------------------------

--- Return the array size of a ubx_data_t
-- @param d ubx_data_t
-- @return array length
function M.data_size(d)
   return tonumber(ubx.data_size(d))
end

--- Get size of an instance of the given type.
-- @param type_name string name of type
-- @return size in bytes
function M.type_size(ni, type_name)
   local t = M.type_get(ni, type_name)
   if t==nil then error("unknown type "..tostring(type_name)) end
   return tonumber(t.size)
end

--- Allocate a new ubx_data with a given dimensionality.
-- This data will be automatically garbage collected.
-- @param ubx_type of data to allocate
-- @param num desired type array length
-- @return ubx_data_t
function M.__data_alloc(typ, num)
   num=num or 1
   local d = ubx.__ubx_data_alloc(typ, num)
   if d==nil then error(M.safe_tostr(ni.name)..": data_alloc: unkown type '"..type_name.."'") end
   ffi.gc(d, function(dat) ubx.ubx_data_free(ni, dat) end)
   return d
end

--- Allocate a new ubx_data with a given dimensionality.
-- This data will be automatically garbage collected.
-- @param ni node_info
-- @param name type of data to allocate
-- @param num dimensionality
-- @return ubx_data_t
function M.data_alloc(ni, type_name, num)
   local t = M.type_get(ni, type_name)
   return M.__data_alloc(t, num)
end

M.data_free = ubx.ubx_data_free
M.type_get = ubx.ubx_type_get

--- Load the registered C types into the luajit ffi.
-- @param ni node_info_t*
function M.ffi_load_types(ni)

   local function ffi_struct_type_is_loaded(t)
      return pcall(ffi.typeof, ffi.string(t.name))
   end

   local function ffi_load_no_ns(typref)
      local t = typref.type_ptr
      if t.type_class==ubx.TYPE_CLASS_STRUCT and t.private_data~=nil then
	 local struct_str = ffi.string(t.private_data)
	 local ret, err = pcall(ffi.cdef, struct_str)
	 if ret==false then
	    error("ffi_load_types: failed to load the following struct:\n"..struct_str.."\n"..err)
	 end
      end
   end

   local typref_list = {}
   M.types_foreach(ni,
		   function (typ, typref) typref_list[#typref_list+1] = typref end,
		   function(t) return
		      t.type_class==ffi.C.TYPE_CLASS_STRUCT and
			 (not ffi_struct_type_is_loaded(t))
		   end)

   table.sort(typref_list, function (tr1,tr2) return tr1.seqid<tr2.seqid end)
   utils.foreach(ffi_load_no_ns, typref_list)
end


--- Convert a ubx_data_t to a plain Lua representation.
-- requires cdata.tolua
-- @param d ubx_data_t type
-- @return Lua data
function M.data_tolua(d)
   if d==nil then error("ubx_data_t argument is nil") end
   if d.data==nil then return nil end

   if not(d.type.type_class==ubx.TYPE_CLASS_BASIC or
	  d.type.type_class==ubx.TYPE_CLASS_STRUCT) then
      error("can currently only print TYPE_CLASS_BASIC or TYPE_CLASS_STRUCT types")
   end

   local res
   local len=tonumber(d.len)

   -- detect char arrays
   if d.type.type_class==ubx.TYPE_CLASS_BASIC and len>1 and M.safe_tostr(d.type.name)=='char' then
      res=M.safe_tostr(d.data)
   else
      local ptrname
      -- TODO: simplify this and pcall ffi.new. If it fails, the type
      -- is probably unknown, and running ubx.ffi_load_types may help.

      if d.type.type_class==ubx.TYPE_CLASS_STRUCT then
	 ptrname = ffi.string(d.type.name).."*"
      else -- BASIC:
	 ptrname = ffi.string(d.type.name).."*"
      end
      local dptr = ffi.new(ptrname, d.data)

      if len>1 then
	 res = {}
	 for i=0,len-1 do res[i+1]=cdata.tolua(dptr[i]) end
      else res=cdata.tolua(dptr) end
   end
   return res
end

--- Convert a ubx_data_t to a simple string representation.
-- @param d ubx_data_t type
-- @return Lua string
function M.data_tostr(d)
   return utils.tab2str(M.data_tolua(d))
end

--- Convert an ubx_type_t to a FFI ctype object.
-- Only works for TYPE_CLASS_BASIC and TYPE_CLASS_STRUCT
-- @param ubx_type_t
-- @param ptr if true, then create pointer type
-- @param fixed_len number that specifies the array length (e.g. char (*)[10])
-- @return luajit FFI ctype
local function type_to_ctype_str(t, ptr, fixed_len)
   if ptr and fixed_len then ptr='(*)'
   elseif ptr then ptr='*'
   else ptr="" end

   if fixed_len then fixed_len='['..tostring(fixed_len)..']' else fixed_len="" end

   if t.type_class==ffi.C.TYPE_CLASS_BASIC or t.type_class==ffi.C.TYPE_CLASS_STRUCT then
      return ffi.string(t.name)..ptr..fixed_len
   end
   error("__type_to_ctype_str: unknown type_class")
end

-- memoize?
function M.type_to_ctype(t, ptr, fixed_len)
   local ctstr=type_to_ctype_str(t, ptr, fixed_len)
   return ffi.typeof(ctstr)
end

--- Transform an ubx_data_t* to a lua FFI ctype
-- @param d ubx_data_t pointer
-- @return ffi ctype
function M.data_to_ctype(d, uselen)
   if uselen then
      return M.type_to_ctype(d.type, true, tonumber(d.len))
   end
   return M.type_to_ctype(d.type, true)
end

--- Transform the value of a ubx_data_t* to a lua FFI cdata.
-- @param d ubx_data_t pointer
-- @return ffi cdata
function M.data_to_cdata(d, uselen)
   local ctp
   if uselen then
      ctp = M.type_to_ctype(d.type, true, tonumber(d.len))
   else
      ctp = M.type_to_ctype(d.type, true)
   end
   return ffi.cast(ctp, d.data)
end

function M.data_resize(d, newlen)
   -- print("changing len from", tonumber(d.len), " to ", newlen)
   if ubx.ubx_data_resize(d, newlen) == 0 then return true
   else return false end
end

--- Assign a value to a ubx_data
-- @param d ubx_data
-- @param value to assign (must follow the luajit FFI initialization rules)
-- @param resize if true, resize buffer and update data.len to fit val
-- @return cdata ptr of the contained type
function M.data_set(d, val, resize)

   -- find cdata of the target ubx_data
   local d_cdata = M.data_to_cdata(d)
   local val_type=type(val)

   if val_type=='table' then
      for k,v in pairs(val) do
	 if type(k)~='number' then
	    d_cdata[k]=v
	 else
	    local idx -- starting from zero
	    if val[0] == nil then idx=k-1 else idx=k end
	    if idx >= d.len and not resize then
	       error("data_set: attempt to index beyond bounds, index="..tostring(idx)..", len="..tostring(d.len))
	    elseif idx >= d.len and resize then
	       M.data_resize(d, idx+1)
	       d_cdata = M.data_to_cdata(d) -- pointer could have changed in realloc!
	    end
	    d_cdata[idx]=v
	 end
      end
   elseif val_type=='string' then
      if d.len<#val+1 then
	 M.data_resize(d, #val+1)
	 d_cdata = M.data_to_cdata(d) -- pointer could have changed in realloc!
      end
      --ffi.copy(d_cdata, val, #val)
      ffi.copy(d_cdata, val)
   elseif val_type=='number' then d_cdata[0]=val
   else
      error("data_set: don't know how to assign "..
	    tostring(val).." to ffi type "..tostring(d_cdata))
   end
   return d_cdata
end

-- add Lua OO methods
local ubx_data_mt = {
   __tostring = M.data_tostr,
   __len = function (d) return tonumber(d.len) end,
   __index = {
      tolua = M.data_tolua,
      size = M.data_size,
      ctype = M.data_to_ctype,
      cdata = M.data_to_cdata,
      resize = M.data_resize,
      set = M.data_set,
   },
}
ffi.metatype("struct ubx_data", ubx_data_mt)


--- Convert a ubx_type to a Lua table
function M.ubx_type_totab(t)
   if t==nil then error("NULL type"); return end
   local res = {}
   res.name=M.safe_tostr(t.name)
   res.class=M.type_class_tostr[t.type_class]
   res.size=tonumber(t.size)
   if t.type_class==ubx.TYPE_CLASS_STRUCT then res.model=M.safe_tostr(t.private_data) end
   return res
end

--- Print a ubx_type
function M.type_tostr(t, verb)
   local tt=M.ubx_type_totab(t)
   local res=("%s, sz=%d, %s"):format(tt.name, tt.size, tt.class)
   if verb then res=res.."\nmodel=\n"..tt.model end
   return res
end

-- add Lua OO methods
local ubx_type_mt = {
   __tostring = M.type_tostr,
   __index = {
      get_name = function (t) return M.safe_tostr(t.name) end,
      get_type = function (t) return M.safe_tostr(t.doc) end,
      totab = M.ubx_type_totab,
      size = function (t) return tonumber(t.size) end,
      ctype = M.type_to_ctype,
   },
}
ffi.metatype("struct ubx_type", ubx_type_mt)


------------------------------------------------------------------------------
--                           Config handling
------------------------------------------------------------------------------

function M.config_set(c, val)
   return M.data_set(c.value, val)
end

--- Set a configuration value
-- @deprecated
-- @param b block
-- @param name name of configuration value
-- @param val value to assign (must follow luajit FFI initialization rules)
function M.set_config(b, name, val)
   local d = ubx.ubx_config_get_data(b, name)
   if d == nil then error("set_config: unknown config '"..name.."'") end
   -- print("configuring ".. ffi.string(b.name).."."..name.." with value "..utils.tab2str(val))
   return M.data_set(d, val, true)
end

--- Configure a block with a table of configuration values.
-- @param b block
-- @param ctab table of configuration values
function M.set_config_tab(b, ctab)
   for n,v in pairs(ctab) do M.set_config(b, n, v) end
end

-- safely load a table from a string
-- @param str table string to load
-- @return true or false
-- @return table or error message
function load_tabstr(str)
   local tab, msg = load("return "..str, nil, 't', {})
   if not tab then return false, msg end
   return tab()
end

--- Load a configuration string.
-- This could be a table, a number or just a string.
-- @param str config string
-- @return true or false
-- @return value or error message
function load_confstr(str)
   local function getchr(s, i) return string.char(string.byte(s,i)) end

   str=utils.trim(str)

   if getchr(str,1) == '{' and getchr(str, #str)== '}' then
      return load_tabstr(str)
   end

   local x = tonumber(str)
   if type(x)=='number' then return x end

   -- last resort: just return the string:
   return str
end

--- Set a configuration value from a string.
-- @param block block pointer
-- @param name name of configuration
-- @param strval string value
function M.set_config_str(b, name, strval)
   local c = ubx.ubx_config_get(b, name)
   if c == nil then error("set_config_str: unknown config '"..name.."'") end

   if c.value.type.type_class==ubx.TYPE_CLASS_BASIC and M.safe_tostr(c.value.type.name)=='char' then
      return M.set_config(b, name, strval)
   end
   return M.set_config(b, name, load_confstr(strval))
end

function M.config_totab(c)
   if c==nil then return "NULL config" end
   local res = {}
   res.name = M.safe_tostr(c.name)
   res.doc = M.safe_tostr(c.doc)
   res.type_name = M.safe_tostr(c.type_name)
   res.value = M.data_tolua(c.value)
   return res
end

function M.config_tostr(c)
   local ctab = M.config_totab(c)
   local doc = ""
   local tstr = M.type_tostr(c.value.type)

   if ctab.doc then doc=red("// "..ctab.doc) end

   return blue(ctab.name, true).." <"..tstr.."> "..yellow(tostring(c.value))
end

-- add Lua OO methods
local ubx_config_mt = {
   __tostring = M.config_tostr,
   __index = {
      get_name = function (c) return M.safe_tostr(c.name) end,
      get_doc = function (c) return M.safe_tostr(c.doc) end,
      set = M.set_config,
      totab = M.config_totab,
      tolua = function (c) return M.data_tolua(c.value) end,
      data = function (c) return c.value end,
   },
}

ffi.metatype("struct ubx_config", ubx_config_mt)


------------------------------------------------------------------------------
--                              Interactions
------------------------------------------------------------------------------

--- TODO!
function M.interaction_read(i, rdat)
   if i.block_state ~= ffi.C.BLOCK_STATE_ACTIVE then
      error("interaction_read: interaction not readable in state "..M.block_state_tostr[i.block_state])
   end
   local res=i.read(i, rdat)
   if res < 0 then
      error("interaction_read failed: "..M.retval_tostr[res])
   elseif res==0 then
      return 0
   end
   return res
end

function M.interaction_write(i, wdat)
   if i.block_state ~= ffi.C.BLOCK_STATE_ACTIVE then
      error("interaction_read: interaction not readable in state "..M.block_state_tostr[i.block_state])
   end
   i.write(i, wdat)
end

------------------------------------------------------------------------------
--                   Port reading and writing
------------------------------------------------------------------------------

--- Allocate an ubx_data for port reading
-- @param port
-- @return ubx_data_t sample
function M.port_alloc_read_sample(p)
   return ubx.__ubx_data_alloc(p.in_type, p.in_data_len)
end

--- Allocate an ubx_data for port writing
-- @param port
-- @return ubx_data_t sample
function M.port_alloc_write_sample(p)
   return ubx.__ubx_data_alloc(p.out_type, p.out_data_len)
end

--- Read from a port.
--
function M.port_read(p, rval)
   if type(rval)~='cdata' then
      rval=M.port_alloc_read_sample(p)
   end
   return ubx.__port_read(p, rval), rval
end

function M.port_write(p, wval)
   if type(wval) == 'cdata' then
      ubx.__port_write(p, wval)
   else
      local sample = M.port_alloc_write_sample(p)
      sample:set(wval)
      ubx.__port_write(p, sample)
   end
end

function M.port_write_read(p, wdat, rdat)
   M.port_write(p, wdat)
   return M.port_read(p, rdat)
end

--- Read from port with timeout.
-- @param p port to read from
-- @param data data to store result of read
-- @param timeout duration to block in seconds
function M.port_read_timed(p, rval, timeout)
   local res
   local ts_start=ffi.new("struct ubx_timespec")
   local ts_cur=ffi.new("struct ubx_timespec")

   if type(rval)~='cdata' then
      rval=M.port_alloc_read_sample(p)
   end

   M.clock_mono_gettime(ts_start)
   M.clock_mono_gettime(ts_cur)

   while ts_cur - ts_start < timeout do
      res=ubx.__port_read(p, rval)
      if res>0 then return res, rval end
      M.clock_mono_gettime(ts_cur)
   end
   error("port_read_timed: timeout after reading "..M.safe_tostr(p.name).." for "..tostring(timeout).." seconds")
end

function M.port_out_size(p)
   if p==nil then error("port_out_size: port is nil") end
   if not M.is_outport(p) then error("port "..M.safe_tostr(p.name).." is not an outport") end
   return tonumber(p.out_type.size * p.out_data_len)
end

function M.port_in_size(p)
   if p==nil then error("port_in_size: port is nil") end
   if not M.is_inport(p) then error("port_in_size: port "..M.safe_tostr(p.name).." is not an inport") end
   return tonumber(p.in_type.size * p.in_data_len)
end


function M.port_conns_totab(p)
   local res = { incoming={}, outgoing={} }
   local i

   i = 0
   if p.in_interaction ~= nil then
      while p.in_interaction[i] ~= nil do
	 res.incoming[i+1] = M.safe_tostr(p.in_interaction[i].name)
	 i=i+1
      end
   end

   i = 0
   if p.out_interaction ~= nil then
      while p.out_interaction[i] ~= nil do
	 res.outgoing[i+1] = M.safe_tostr(p.out_interaction[i].name)
	 i=i+1
      end
   end
   return res
end


function M.port_totab(p)
   local ptab = {}
   ptab.name = M.safe_tostr(p.name)
   ptab.doc = M.safe_tostr(p.doc)
   ptab.attrs = tonumber(p.attrs)
   ptab.state = tonumber(p.state)
   if M.is_inport(p) then
      ptab.in_type_name = M.safe_tostr(p.in_type_name)
      ptab.in_data_len = tonumber(p.in_data_len)
   end
   if M.is_outport(p) then
      ptab.out_type_name = M.safe_tostr(p.out_type_name)
      ptab.out_data_len = tonumber(p.out_data_len)
   end
   ptab.connections = M.port_conns_totab(p)
   return ptab
end

function M.port_tostr(port)
   local in_str=""
   local out_str=""
   local doc=""

   local p = M.port_totab(port)

   -- print(utils.tab2str(p))

   if p.in_type_name then
      in_str = "in: "..p.in_type_name
      if p.in_data_len > 1 then in_str = in_str.."["..ts(p.in_data_len).."]" end
      in_str = in_str.." #conn: "..ts(#p.connections.incoming)
   end

   if p.out_type_name then
      out_str = "out: "..p.out_type_name
      if p.out_data_len > 1 then out_str = out_str.."["..ts(p.out_data_len).."]" end
      out_str = out_str.." #conn: "..ts(#p.connections.outgoing)
   end

   if p.doc then doc = red(" // "..p.doc) end

   return cyan(p.name, true) .. ": "..in_str..out_str..doc
end

-- add Lua OO methods
local ubx_port_mt = {
   __tostring = M.port_tostr,
   __len = function (p) return tonumber(p.in_data_len), tonumber(p.out_data_len) end,
   __index = {
      get_name = function (p) return M.safe_tostr(p.name) end,
      get_doc = function (p) return M.safe_tostr(p.doc) end,
      totab = M.port_totab,
      out_size = M.port_out_size,
      in_size = M.port_in_size,
      write = M.port_write,
      read = M.port_read,
      write_read = M.port_write_read,
      read_timed = M.port_read_timed,
   },
}
ffi.metatype("struct ubx_port", ubx_port_mt)


------------------------------------------------------------------------------
--                   Usefull stuff: foreach, pretty printing
------------------------------------------------------------------------------

--- Call a function on every known type.
-- @param ni ubx_node_info_t*
-- @param fun function to call on type.
function M.types_foreach(ni, fun, pred)
   if not fun then error("types_foreach: missing/invalid fun argument") end
   if ni.types==nil then return end
   pred = pred or function() return true end
   local ubx_type_ref_t_ptr = ffi.typeof("ubx_type_ref_t*")
   local typ=ni.types
   while typ ~= nil do
      if pred(typ.type_ptr, typ) then fun(typ.type_ptr, typ) end
      typ=ffi.cast(ubx_type_ref_t_ptr, typ.hh.next)
   end
end


--- Call function on all ports of a block and return the result in a table.
-- @param b block
-- @param fun function to call on port
-- @param pred optional predicate function. fun is only called if pred is true.
-- @return result table.
function M.ports_map(b, fun, pred)
   local res={}
   pred = pred or function() return true end
   local port_ptr=b.ports
   while port_ptr~=nil and port_ptr.name~= nil do
      if pred(port_ptr) then res[#res+1]=fun(port_ptr) end
      port_ptr=port_ptr+1
   end
   return res
end

--- Call function on all ports of a block and return the result in a table.
-- @param b block
-- @param fun function to call on port
-- @param pred optional predicate function. fun is only called if pred is true.
-- @return result table.
function M.ports_foreach(b, fun, pred)
   pred = pred or function() return true end
   local port_ptr=b.ports
   while port_ptr~=nil and port_ptr.name~= nil do
      if pred(port_ptr) then fun(port_ptr) end
      port_ptr=port_ptr+1
   end
   return res
end

--- Call function on all configs of a block and return the result in a table.
-- @param b block
-- @param fun function to call on config
-- @param pred optional predicate function. fun is only called if pred is true.
-- @return result table.
function M.configs_map(b, fun, pred)
   local res={}
   pred = pred or function() return true end
   local conf_ptr=b.configs
   while conf_ptr~=nil and conf_ptr.name~=nil do
      if pred(conf_ptr) then res[#res+1]=fun(conf_ptr) end
      conf_ptr=conf_ptr+1
   end
   return res
end

--- Call a function on every block of the given list.
-- @param ni node info
-- @param fun functionw
-- @param pred predicate function to filter
-- @param result table
function M.blocks_map(ni, fun, pred)
   local res = {}
   if ni==nil then return end
   pred = pred or function() return true end
   local ubx_block_t_ptr = ffi.typeof("ubx_block_t*")
   local b=ni.blocks
   while b ~= nil do
      if pred(b) then res[#res+1]=fun(b) end
      b=ffi.cast(ubx_block_t_ptr, b.hh.next)
   end
   return res
end


--- Apply a function to each module of a node.
-- @param ni node
-- @param fun function to apply to each module
-- @param pred predicate filter
function M.modules_foreach(ni, fun, pred)
   if ni==nil then return end
   pred = pred or function() return true end
   local ubx_module_t_ptr = ffi.typeof("ubx_module_t*")
   local m=ni.modules
   while m ~= nil do
      if pred(m) then fun(m) end
      m=ffi.cast(ubx_module_t_ptr, m.hh.next)
   end
end

--- Convert the current system to a dot-file
-- @param ni
-- @return graphviz dot string
function M.node_todot(ni)
   --- Generate a list of block nodes in graphviz dot syntax
   function gen_dot_nodes(blocks)
      local function block2color(b)
	 if b.state == 'preinit' then return "blue"
	 elseif b.state == 'inactive' then return "red"
	 elseif b.state == 'active' then return "green"
	 else return "pink" end
      end
      local res = {}
      local shape="box"
      local style=""
      for _,b in ipairs(blocks) do
	 if b.block_type=='cblock' then style="solid" else style="rounded,dashed" end
	 res[#res+1] = utils.expand('    "$name" [ shape=$shape, style="$style", color=$color, penwidth=3 ];',
				    { name=b.name, style=style, shape=shape, color=block2color(b) })
      end
      return table.concat(res, '\n')
   end

   --- Add trigger edges
   function gen_dot_trigger_edges(b, res)

      if not (b.prototype=="std_triggers/ptrig" or
	      b.prototype=="std_triggers/trig") then return end
      local trig_blocks_cfg

      for i,c in ipairs(b.configs) do
	 if c.name=='trig_blocks' then trig_blocks_cfg=c.value end
      end
      assert(trig_blocks_cfg~=nil, "gen_dot_trigger_edges: failed to find trig_blocks config")

      for i,trig in ipairs(trig_blocks_cfg or {}) do
	 res[#res+1]=utils.expand('    "$from" -> "$to" [label="$label",style=dashed,color=orange1 ];',
				  {from=b.name, to=trig.b, label=ts(i)})
      end
   end

   --- Generate edges
   function gen_dot_edges(blocks)
      local res = {}
      for _,b in ipairs(blocks) do
	 for _,p in ipairs(b.ports) do
	    for _,iblock_out in ipairs(p.connections.outgoing) do
	       res[#res+1]=utils.expand('    "$from" -> "$to" [label="$label", labeldistance=2];',
					{from=b.name, to=iblock_out, label=p.name})
	    end
	    for _,iblock_in in ipairs(p.connections.incoming) do
	       res[#res+1]=utils.expand('    "$from" -> "$to" [label="$label", labeldistance=2];',
					{from=iblock_in, to=b.name, label=p.name})
	    end
	 end
	 gen_dot_trigger_edges(b, res)
      end
      return table.concat(res, '\n')
   end

   local btab = M.blocks_map(ni, M.block_totab, function(b) return not M.is_proto(b) end)
   return utils.expand(
      [[
digraph "$node" {
    // global attributes
    graph [ rankdir=TD ];

    // nodes
$blocks

    // edges
$conns
}
]], {
   node=M.safe_tostr(ni.name),
   blocks=gen_dot_nodes(btab),
   conns=gen_dot_edges(btab),
    })
end


--- Create a new port connected to an existing port via an interaction.
-- @param bname block
-- @param pname name of port
-- @param buff_len1 desired buffer length (if port is in/out: length of out->in buffer)
-- @param buff_len2 only if port is in/out port: length of in->out buffer
-- @return the new port
function M.port_clone_conn(block, pname, buff_len1, buff_len2)
   local prot = M.port_get(block, pname)
   local p=ffi.new("ubx_port_t")
   local ts = ffi.new("struct ubx_timespec")

   -- print("port_clone_conn, block=", ubx.safe_tostr(block.name), ", port=", pname)

   M.clock_mono_gettime(ts)

   if M.clone_port_data(p, M.safe_tostr(prot.name)..'_inv', prot.doc,
			prot.out_type, prot.out_data_len,
			prot.in_type, prot.in_data_len, 0) ~= 0 then
      error("port_clone_conn: cloning port data failed")
   end

   buff_len1 = buff_len1 or 1
   if p.in_type and p.out_type then buff_len2 = buff_len2 or buff_len1 end

   -- New port is an out-port?
   local i_p_to_prot=nil
   if p.out_type~=nil then
      local iname = string.format("PCC->%s.%s_%x:%x",
				  M.safe_tostr(block.name), pname,
				  tonumber(ts.sec), tonumber(ts.nsec))

      -- print("creating interaction", iname, buff_len1, tonumber(p.out_type.size * p.out_data_len))

      i_p_to_prot = M.block_create(block.ni, "lfds_buffers/cyclic", iname,
				     { buffer_len=buff_len1,
				       type_name=ffi.string(p.out_type_name),
				       data_len=tonumber(p.out_data_len) })
      M.block_init(i_p_to_prot)

      if M.ports_connect_uni(p, prot, i_p_to_prot)~=0 then
	 M.port_free_data(p)
	 error("failed to connect port"..M.safe_tostr(p.name))
      end
      M.block_start(i_p_to_prot)
   end

   local i_prot_to_p = nil

   if p.in_type ~= nil then -- new port is an in-port?
      local iname = string.format("PCC<-%s.%s_%x:%x",
				  M.safe_tostr(block.name), pname,
				  tonumber(ts.sec), tonumber(ts.nsec))

      i_prot_to_p = M.block_create(block.ni, "lfds_buffers/cyclic", iname,
				     { buffer_len=buff_len2,
				       type_name=ffi.string(p.in_type_name),
				       data_len=tonumber(p.in_data_len) })

      M.block_init(i_prot_to_p)

      if M.ports_connect_uni(prot, p, i_prot_to_p)~=0 then
	 -- TODO disconnect if connected above.
	 M.port_free_data(p)
	 error("failed to connect port"..M.safe_tostr(p.name))
      end
      M.block_start(i_prot_to_p)
   end


   -- cleanup interaction and port once the reference is lost
   ffi.gc(p, function (p)
		-- print("cleaning up port PCC port "..M.safe_tostr(p.name))
		M.port_free_data(p)
	     end)
   return p
end

--- Connect two blocks' ports via a unidirectional connection.
-- @param b1 block1
-- @param pname1 name of a block1's out port to connect from.
-- @param b2 block2
-- @param pname2 name of a block2's in port to connect to
-- @param iblock_type type of interaction to use
-- @param configuration for interaction.
-- @param dont_start if true, then don't start the interaction (stays stopped)
function M.conn_uni(b1, pname1, b2, pname2, iblock_type, iblock_config, dont_start)
   local p1, p2, ni, bname1, bname2
   local ts = ffi.new("struct ubx_timespec")

   if b1==nil or b2==nil then error("parameter 1 or 3 (ubx_block_t) is nil") end

   bname1 = M.safe_tostr(b1.name)
   bname2 = M.safe_tostr(b2.name)

   ni = b1.ni

   p1 = M.port_get(b1, pname1)
   p2 = M.port_get(b2, pname2)

   if p1==nil then error("block "..bname1.." has no port '"..M.safe_tostr(pname1).."'") end
   if p2==nil then error("block "..bname2.." has no port '"..M.safe_tostr(pname2).."'") end

   if not M.is_outport(p1) then error("conn_uni: block "..bname1.."'s port "..pname1.." is not an outport") end
   if not M.is_inport(p2) then error("conn_uni: block "..bname2.."'s port "..pname2.." is not an inport") end

   -- get time stamp
   M.clock_mono_gettime(ts)

   -- local iblock_name = string.format("i_%s.%s->%s.%s_%x:%x", bname1, pname1, bname2, pname2, tonumber(ts.sec), tonumber(ts.nsec))
   local iblock_name = string.format("i_%x:%x", tonumber(ts.sec), tonumber(ts.nsec))

   -- create iblock, configure
   local ib=M.block_create(ni, iblock_type, iblock_name, iblock_config)

   M.block_init(ib)

   if M.ports_connect_uni(p1, p2, ib) ~= 0 then
      error("failed to connect "..bname1.."."..pname1.."->"..bname2.."."..pname2)
   end

   if not dont_start then M.block_start(ib) end

   return ib
end


--- Connect two ports with an interaction.
-- @param b1 block owning port1
-- @param pname1 name of port1
-- @param b2 block owning port2
-- @param pname2 name of port2
-- @param element_num number of elements
-- @param dont start if true, interaction will not be started
function M.conn_lfds_cyclic(b1, pname1, b2, pname2, element_num, dont_start)
   local p1, p2, len1, len2

   if b1==nil then error("conn_lfds_cyclic: block (arg 1) is nil") end
   if b2==nil then error("conn_lfds_cyclic: block (arg 3) is nil") end

   local bname1, bname2 = M.safe_tostr(b1.name), M.safe_tostr(b2.name)

   p1 = M.port_get(b1, pname1)
   p2 = M.port_get(b2, pname2)

   if p1==nil then error("block "..bname1.." has no port '"..M.safe_tostr(pname1).."'") end
   if p2==nil then error("block "..bname2.." has no port '"..M.safe_tostr(pname2).."'") end

   if not M.is_outport(p1) then
      error("conn_lfds_cyclic: block "..bname1.."'s port "..pname1.." is not an outport")
   end
   if not M.is_inport(p2) then
      error("conn_lfds_cyclic: block ".. bname2.."'s port "..pname2.." is not an inport")
   end

   len1, len2 = tonumber(p1.out_data_len), tonumber(p2.in_data_len)

   if len1 ~= len2 then print(yellow("WARNING: conn_lfds_cyclic "
					..bname1.."."..pname1.. " and "
					..bname2.."."..pname2.." have different array len ("
				        ..tostring(len1).." vs "..tostring(len2).."). Using minimum!"), true)
   end

   if type(element_num) ~= 'number' or element_num < 1 then
      error("conn_lfds_cyclic: invalid element_num array length param "..ts(element_num))
   end

   return M.conn_uni(b1, pname1, b2, pname2, "lfds_buffers/cyclic",
		     { buffer_len=element_num,
		       type_name=M.safe_tostr(p1.out_type_name),
		       data_len=utils.min(len1, len2) }, dont_start)
end

return M
