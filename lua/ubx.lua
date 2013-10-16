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
local ubx_utils = require "ubx_utils"
local utils= require "utils"
local ts=tostring
local safe_ts=ubx_utils.safe_tostr
local ac=require "ansicolors"
-- require "strict"
local M={}
local setup_enums

------------------------------------------------------------------------------
--                           helpers
------------------------------------------------------------------------------

--- Read the entire contents of a file.
-- @param file name of file
-- @return string contents
local function read_file(file)
   local f = assert(io.open(file, "rb"))
   local data = f:read("*all")
   f:close()
   return data
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
local ubx=ffi.load("src/libubx.so")

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
--                           Node and block API
------------------------------------------------------------------------------

--- Dealing with modules. This could be moved to C.
ubx_modules = {}

--- Load and initialize a ubx module.
-- @param ni node_info pointer into which to load module
-- @param libfile module file to load
function M.load_module(ni, libfile)
   local nodename=M.safe_tostr(ni.name)

   if not utils.file_exists(libfile) then error("non-existing file "..tostring(libfile)) end

   if ubx_modules[nodename] and ubx_modules[nodename].loaded[libfile] then
      -- print(nodename..": library "..tostring(libfile).." already loaded, ignoring")
      goto out
   end


   local mod=ffi.load(libfile)
   if mod.__ubx_initialize_module(ni) ~= 0 then
      error("failed to init module "..libfile)
   end

   -- print(nodename..": library "..tostring(libfile).." successfully loaded")

   if not ubx_modules[nodename] then ubx_modules[nodename]={ loaded={}} end

   local mods = ubx_modules[nodename]
   mods[#mods+1]={ module=mod, libfile=libfile }
   mods.loaded[libfile]=true
   M.ffi_load_types(ni)

   ::out::
end

--- Unload all loaded modules
function M.unload_modules(ni)
   local nodename=M.safe_tostr(ni.name)

   local mods = ubx_modules[nodename] or {}
   for _,mod in ipairs(mods) do
      print(nodename..": unloading "..mod.libfile)
      mod.module.__ubx_cleanup_module(ni)
   end
   ubx_modules[nodename]={ loaded={}}
end


--- Create and initalize a new node_info struct
-- @param name name of node
-- @return ubx_node_info_t
function M.node_create(name)
   if ubx_modules[name] then error("a node named "..tostring(name).." already exists") end
   local ni=ffi.new("ubx_node_info_t")
   -- the following is bad, because nodes are shared among different Lua instances.
   -- ffi.gc(ni, M.node_cleanup)
   assert(ubx.ubx_node_init(ni, name)==0, "node_create failed")
   return ni
end

--- Cleanup a node: cleanup and remove instances and unload modules.
-- @param ni node info
function M.node_cleanup(ni)
   local nname = M.safe_tostr(ni.name)
   print(nname..": cleaning up node")
   print(nname..": unloading block instances:")
   M.blocks_foreach(ni, function (b)
			   local n=M.safe_tostr(b.name)
			   print("    unloading "..n)
			   M.block_unload(ni, n)
			end,
		    M.is_instance)
   print(nname..": unloading modules:")
   M.unload_modules(ni)
   print(nname..": cleaning up node info")
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

M.block_rm = ubx.ubx_block_rm
M.block_get = ubx.ubx_block_get

--- Life cycle handling
M.block_init = ubx.ubx_block_init
M.block_start = ubx.ubx_block_start
M.block_stop = ubx.ubx_block_stop
M.block_cleanup = ubx.ubx_block_cleanup

M.cblock_step = ubx.ubx_cblock_step

--- Port and Connections handling
M.port_get = ubx.ubx_port_get
M.port_add = ubx.ubx_port_add
M.port_rm = ubx.ubx_port_rm
M.connect_one = ubx.ubx_connect_one -- deprecated
M.clone_port_data = ubx.ubx_clone_port_data
M.port_free_data = ubx.ubx_port_free_data

M.port_connect_in=ubx.ubx_port_connect_in
M.port_connect_out=ubx.ubx_port_connect_out
M.ports_connect_uni=ubx.ubx_ports_connect_uni

M.port_disconnect_in=ubx.ubx_port_disconnect_in
M.port_disconnect_out=ubx.ubx_port_disconnect_out
M.ports_disconnect_uni=ubx.ubx_ports_disconnect_uni



--- Configuration

--- Add a ubx_config to a block.
-- @param b block
-- @param name name of configuration
-- @param meta configuration metadata
-- @param type_name desired type of configuration
-- @param len desired array multiplicity of contained data.
-- @return 0 in case of success, -1 otherwise.
M.config_add = ubx.ubx_config_add

M.config_rm = ubx.ubx_config_rm
M.config_get = ubx.ubx_config_get
M.config_get_data = ubx.ubx_config_get_data

-- OS stuff
M.clock_mono_gettime = ubx.ubx_clock_mono_gettime

--- Unload a block: bring it to state preinit and call ubx_block_rm
function M.block_unload(ni, name)
   local b=M.block_get(ni, name)
   if b==nil then error("no block "..tostring(name).." found") end
   if b.block_state==ffi.C.BLOCK_STATE_ACTIVE then M.block_stop(b) end
   if b.block_state==ffi.C.BLOCK_STATE_INACTIVE then M.block_cleanup(b) end
   if M.block_rm(ni, name) ~= 0 then error("block_unload: ubx_block_rm failed for '"..name.."'") end
end

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
-- @param ni node_info
-- @param name type of data to allocate
-- @param num dimensionality
-- @return ubx_data_t
function M.data_alloc(ni, type_name, num)
   num=num or 1
   local d = ubx.ubx_data_alloc(ni, type_name, num)
   if d==nil then error(M.safe_tostr(ni.name)..": data_alloc: unkown type '"..type_name.."'") end
   ffi.gc(d, function(dat)
		-- print("data_free: freeing data ", dat)
		ubx.ubx_data_free(ni, dat)
	     end)
   return d
end

M.data_free = ubx.ubx_data_free
M.type_get = ubx.ubx_type_get

--- Load the registered C types into the luajit ffi.
-- @param ni node_info_t*
function M.ffi_load_types(ni)

   local function ffi_struct_type_is_loaded(t)
      return pcall(ffi.typeof, ffi.string(t.name))
   end

   local function ffi_load_no_ns(t)
      if t.type_class==ubx.TYPE_CLASS_STRUCT and t.private_data~=nil then
	 local struct_str = ffi.string(t.private_data)
	 local ret, err = pcall(ffi.cdef, struct_str)
	 if ret==false then
	    error("ffi_load_types: failed to load the following struct:\n"..struct_str.."\n"..err)
	 end
      end
   end

   local type_list = {}
   M.types_foreach(ni, function (t) type_list[#type_list+1] = t end,
		   function(t)
		      return t.type_class==ffi.C.TYPE_CLASS_STRUCT and (not ffi_struct_type_is_loaded(t)) end
		)
   table.sort(type_list, function (t1,t2) return t1.seqid<t2.seqid end)
   utils.foreach(ffi_load_no_ns, type_list)
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
-- @return luajit FFI ctype
local function type_to_ctype_str(t, ptr)
   if ptr then ptr='*' else ptr="" end
   if t.type_class==ffi.C.TYPE_CLASS_BASIC or t.type_class==ffi.C.TYPE_CLASS_STRUCT then
      return ffi.string(t.name)..ptr
   end
   error("__type_to_ctype_str: unknown type_class")
end

-- memoize?
function M.type_to_ctype(t, ptr)
   local ctstr=type_to_ctype_str(t, ptr)
   return ffi.typeof(ctstr)
end

function M.data_to_ctype(d)
   return M.type_to_ctype(d.type, true)
end

--- Transform the value of a ubx_data_t* to a lua FFI cdata.
-- @param d ubx_data_t pointer
-- @return ffi cdata
function M.data_to_cdata(d)
   local ctp = M.type_to_ctype(d.type, true)
   return ffi.cast(ctp, d.data)
end

function M.data_resize(d, newlen)
   print("changing len from", tonumber(d.len), " to ", newlen)
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

--- Set a configuration value
-- @param block
-- @param name name of configuration value
-- @param confval value to assign (must follow luajit FFI initialization rules)
function M.set_config(b, name, val)
   local d = ubx.ubx_config_get_data(b, name)
   if d == nil then error("set_config: unknown config '"..name.."'") end
   -- print("configuring ".. ffi.string(b.name).."."..name.." with value "..utils.tab2str(val))
   return M.data_set(d, val, true)
end

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

function M.port_read(p, rdat)
   return ubx.__port_read(p, rdat)
end

function M.port_write(p, data)
   ubx.__port_write(p, data)
end

function M.port_write_read(p, wdat, rdat)
   M.port_write(p, wdat)
   return M.port_read(p, rdat)
end


------------------------------------------------------------------------------
--                   Usefull stuff: foreach, pretty printing
------------------------------------------------------------------------------

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
   local res=("ubx_type_t: name=%s, size=%d, type_class=%s"):format(tt.name, tt.size, tt.class)
   if verb then res=res.."\nmodel=\n"..tt.model end
   return res
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
   ptab.meta_data = M.safe_tostr(p.meta_data)
   ptab.attrs = tonumber(p.attrs)
   ptab.state = tonumber(p.state)
   ptab.in_type_name = M.safe_tostr(p.in_type_name)
   ptab.out_type_name = M.safe_tostr(p.out_type_name)
   ptab.in_data_len = tonumber(p.in_data_len)
   ptab.out_data_len = tonumber(p.out_data_len)
   ptab.connections = M.port_conns_totab(p)
   return ptab
end

function M.config_totab(c)
   if c==nil then return "NULL config" end
   local res = {}
   res.name = M.safe_tostr(c.name)
   res.type_name = M.safe_tostr(c.type_name)
   res.value = M.data_tolua(c.value)
   return res
end

--- Convert a block to a Lua table.
function M.block_totab(b)
   if b==nil then error("NULL block") end
   local res = {}
   res.name=M.safe_tostr(b.name)
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

--- Pretty print a port
-- @param p port to convert to string
-- @return string
function M.ports_tostr(ports)
   local res={}
   for i,p in ipairs(ports) do res[#res+1]="    port: "..ac.blue(p.name)..":   "..utils.tab2str(p) end
   return table.concat(res, '\n')
end

function M.configs_tostr(configs)
   local res={}
   for i,c in ipairs(configs) do res[#res+1]="    config: "..ac.blue(c.name)..": "..utils.tab2str(c) end
   return table.concat(res, '\n')
end

--- Pretty print a block.
-- @param b block to convert to string.
-- @return string
function M.block_tostr(b)
   local bt=M.block_totab(b)
   local fmt="ubx_block_t: name=%s, block_type=%s, state=%s, prototype=%s\n"
   local res=fmt:format(ac.blue(bt.name), bt.block_type, bt.state, bt.prototype, bt.stat_num_steps)
   res=res..M.ports_tostr(bt.ports)
   res=res..M.configs_tostr(bt.configs)
   return res
end

--- Call a function on every known type.
-- @param ni ubx_node_info_t*
-- @param fun function to call on type.
function M.types_foreach(ni, fun, pred)
   if not fun then error("types_foreach: missing/invalid fun argument") end
   if ni.types==nil then return end
   pred = pred or function() return true end
   local ubx_type_t_ptr = ffi.typeof("ubx_type_t*")
   local typ=ni.types
   while typ ~= nil do
      if pred(typ) then fun(typ) end
      typ=ffi.cast(ubx_type_t_ptr, typ.hh.next)
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
-- @param a block_list
-- @param function
function M.blocks_foreach(ni, fun, pred)
   if ni==nil then return end
   pred = pred or function() return true end
   local ubx_block_t_ptr = ffi.typeof("ubx_block_t*")
   local b=ni.blocks
   while b ~= nil do
      if pred(b) then fun(b) end
      b=ffi.cast(ubx_block_t_ptr, b.hh.next)
   end
end

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

function M.cblocks_foreach(ni, fun, pred)
   pred = pred or function() return true end
   M.blocks_foreach(ni, fun, function(b)
				if not M.is_cblock(b) then return end
				return pred(b)
			     end)
end

function M.iblocks_foreach(ni, fun, pred)
   pred = pred or function() return true end
   M.blocks_foreach(ni, fun, function(b)
				if not M.is_iblock(b) then return end
				return pred(b)
			     end)
end

--- Pretty print helpers
function M.print_types(ni) types_foreach(ni, ubx_type_pp) end
function M.print_cblocks(ni) M.cblocks_foreach(ni, function(b) print(M.block_tostr(b)) end) end
function M.print_iblocks(ni) M.iblocks_foreach(ni, function(b) print(M.block_tostr(b)) end) end

function M.num_blocks(ni)
   local num_cb, num_ib, inv = 0,0,0
   M.block_foreach(ni, function (b)
			  if b.block_type==ffi.C.BLOCK_TYPE_COMPUTATION then num_cb=num_cb+1
			  elseif b.block_type==ffi.C.BLOCK_TYPE_INTERACTION then num_ib=num_ib+1
			  else inv=inv+1 end
		       end)
   return num_cb, num_ib, inv
end

function M.num_types(ni) return ubx.ubx_num_types(ni) end

--- Print an overview of current node state.
function M.ni_stat(ni)
   print(string.rep('-',78))
   local num_cb, num_ib, num_inv
   print("cblocks:"); M.print_cblocks(ni);
   print("iblocks:"); M.print_iblocks(ni);
   print(tostring(num_cb).." cblocks, ",
	 tostring(num_ib).." iblocks, ",
	 tostring(ubx.ubx_num_types(ni)).." types")
   print(string.rep('-',78))
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
      local shape=nil
      for _,b in ipairs(blocks) do
	 if b.block_type=='cblock' then shape="box" else shape="oval" end
	 res[#res+1] = utils.expand('    "$name" [ shape=$shape, color=$color ];',
				    { name=b.name, shape=shape, color=block2color(b) })
      end
      return table.concat(res, '\n')
   end

   --- Generate edges
   function gen_dot_edges(blocks)
      local res = {}
      for _,b in ipairs(blocks) do
	 for _,p in ipairs(b.ports) do
	    for _,iblock_out in ipairs(p.connections.outgoing) do
	       res[#res+1]=utils.expand('    "$from" -> "$to" [taillabel="$taillabel"];',
					{from=b.name, to=iblock_out, taillabel=p.name})
	    end
	    for _,iblock_in in ipairs(p.connections.incoming) do
	       res[#res+1]=utils.expand('    "$from" -> "$to" [taillabel="$headlabel"];',
					{from=iblock_in, to=b.name, headlabel=p.name})
	    end
	 end
      end
      return table.concat(res, '\n')
   end

   local btab = M.blocks_map(ni, M.block_totab,
			     function(b)
				return not M.is_proto(b)
			     end)
   return utils.expand(
[[
digraph $node {
$blocks
$conns
}
]], {
   node=M.safe_tostr(ni.name),
   blocks=gen_dot_nodes(btab),
   conns=gen_dot_edges(btab),
    })



end




--- Create an inversly connected port
-- @param bname block
-- @param pname name of port
-- @param buff_len1 desired buffer length (if in_out port: length of out->in buffer)
-- @param buff_len2 if in_out port: length of in->out buffer
function M.port_clone_conn(block, pname, buff_len1, buff_len2)
   local prot = M.port_get(block, pname)
   local p=ffi.new("ubx_port_t")
   local ts = ffi.new("struct ubx_timespec")

   -- print("port_clone_conn, block=", ubx.safe_tostr(block.name), ", port=", pname)

   M.clock_mono_gettime(ts)

   if M.clone_port_data(p, M.safe_tostr(prot.name)..'_inv', prot.meta_data,
			  prot.out_type, prot.out_data_len,
			  prot.in_type, prot.in_data_len, 0) ~= 0 then
      error("port_clone_conn: cloning port data failed")
   end

   buff_len1=buff_len1 or 1
   if p.in_type and p.out_type then buff_len2 = buff_len2 or buff_len1 end

   -- New port is an out-port?
   local i_p_to_prot=nil
   if p.out_type~=nil then
      local iname = string.format("PCC->%s.%s_%x:%x",
				  M.safe_tostr(block.name), pname,
				  tonumber(ts.sec), tonumber(ts.nsec))

      -- print("creating interaction", iname, buff_len1, tonumber(p.out_type.size * p.out_data_len))

      i_p_to_prot = M.block_create(block.ni, "lfds_buffers/cyclic", iname,
				     { element_num=buff_len1,
				       element_size=tonumber(p.out_type.size * p.out_data_len) })

      M.block_init(i_p_to_prot)

      if M.ports_connect_uni(p, prot, i_p_to_prot)~=0 then
	 M.port_free_data(p)
	 error("failed to connect port"..M.safe_tostr(p.name))
      end
      M.block_start(i_p_to_prot)
   end

   -- New port is (also) an in-port?
   local i_prot_to_p=nil
   if p.in_type ~= nil then
      local iname = string.format("PCC<-%s.%s_%x:%x",
				  M.safe_tostr(block.name), pname,
				  tonumber(ts.sec), tonumber(ts.nsec))

      -- print("creating interaction", iname, buff_len2, tonumber(p.in_type.size * p.in_data_len))

      i_prot_to_p = M.block_create(block.ni, "lfds_buffers/cyclic", iname,
				     { element_num=buff_len2,
				       element_size=tonumber(p.in_type.size * p.in_data_len) })

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
		print("cleaning up port PCC port "..M.safe_tostr(p.name))

		if i_p_to_prot then
		   M.block_stop(i_p_to_prot)
		   M.ports_disconnect_uni(p, prot, i_p_to_prot)
		   M.block_unload(block.ni, i_p_to_prot.name)
		end

		if i_prot_to_p then
		   M.block_stop(i_prot_to_p)
		   M.ports_disconnect_uni(prot, p, i_prot_to_p)
		   M.block_unload(block.ni, i_prot_to_p.name)
		end
		M.port_free_data(p)
	     end)
   return p
end

--- Read from port with timeout.
-- @param p port to read from
-- @param data data to store result of read
-- @param sec duration to block in seconds
function M.port_read_timed(p, data, sec)
   local res
   local ts_start=ffi.new("struct ubx_timespec")
   local ts_cur=ffi.new("struct ubx_timespec")

   M.clock_mono_gettime(ts_start)
   M.clock_mono_gettime(ts_cur)

   while ts_cur.sec - ts_start.sec < sec do
      res=M.port_read(p, data)
      if res>0 then return res end
      M.clock_mono_gettime(ts_cur)
   end
   error("port_read_timed: timeout after reading "..M.safe_tostr(p.name).." for "..tostring(sec).." seconds")
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

   local iblock_name = string.format("I_%s.%s->%s.%s_%x:%x",
				     bname1, pname1, bname2, pname2,
				     tonumber(ts.sec), tonumber(ts.nsec))

   -- create iblock, configure
   local ib=M.block_create(ni, iblock_type, iblock_name, iblock_config)

   M.block_init(ib)

   if M.ports_connect_uni(p1, p2, ib) ~= 0 then
      error("failed to connect "..bname1.."."..pname1.."->"..bname2.."."..pname2)
   end

   if not dont_start then M.block_start(ib) end

   return ib
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

function M.conn_lfds_cyclic(b1, pname1, b2, pname2, element_num, dont_start)
   local function max(x1, x2) if x1>x2 then return x1; else return x2; end end

   local p1, p2, len

   p1 = M.port_get(b1, pname1)
   p2 = M.port_get(b2, pname2)

   if p1==nil then error("block "..bname1.." has no port '"..M.safe_tostr(pname1).."'") end
   if p2==nil then error("block "..bname2.." has no port '"..M.safe_tostr(pname2).."'") end

   if not M.is_outport(p1) then error("conn_uni: block "..bname1.."'s port "..pname1.." is not an outport") end
   if not M.is_inport(p2) then error("conn_uni: block "..bname2.."'s port "..pname2.." is not an inport") end

   size = max(M.port_out_size(p1), M.port_in_size(p2))

   return M.conn_uni(b1, pname1, b2, pname2, "lfds_buffers/cyclic",
		     {element_num=element_num, element_size=size}, dont_start)
end

return M
