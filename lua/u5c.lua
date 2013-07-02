--- u5C Lua interface
local ffi=require "ffi"
local reflect = require "lua/reflect"
local u5c_utils = require "lua/u5c_utils"
local utils= require "utils"
local ts=tostring
local safe_ts=u5c_utils.safe_tostr
local strict=require "strict"

local M={}
local setup_enums

------------------------------------------------------------------------------
--                           helpers
------------------------------------------------------------------------------

--- Read the entire contents of a file.
-- @param file name of file
-- @return string contents
local function read_file(file)
   local f = io.open(file, "rb")
   local data = f:read("*all")
   f:close()
   return data
end

-- call only after loading libu5c!
local function setup_enums()
   if M.u5c==nil then error("setup_enums called before loading libu5c") end
   M.retval_tostr = {
      [0] ='OK',
      [ffi.C.PORT_READ_NODATA] 	  ='PORT_READ_NODATA',
      [ffi.C.PORT_READ_NEWDATA]   ='PORT_READ_NEWDATA',
      [ffi.C.EPORT_INVALID] 	  ='EPORT_INVALID',
      [ffi.C.EPORT_INVALID_TYPE]  ='EPORT_INVALID_TYPE',
      [ffi.C.EINVALID_BLOCK_TYPE] ='EINVALID_BLOCK_TYPE',
      [ffi.C.ENOSUCHBLOCK] 	  ='ENOSUCHBLOCK',
      [ffi.C.EALREADY_REGISTERED] ='EALREADY_REGISTERED',
      [ffi.C.EOUTOFMEM] 	  ='EOUTOFMEM',
      [ffi.C.EINVALID_CONFIG] 	  ='EINVALID_CONFIG'
   }

   M.block_type_tostr={
      [ffi.C.BLOCK_TYPE_COMPUTATION]="cblock",
      [ffi.C.BLOCK_TYPE_INTERACTION]="iblock",
      [ffi.C.BLOCK_TYPE_TRIGGER]="tblock"
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

-- load u5c_types and library
ffi.cdef(read_file("src/uthash_ffi.h"))
ffi.cdef(read_file("src/u5c_types.h"))
ffi.cdef(read_file("src/u5c_proto.h"))
local u5c=ffi.load("src/libu5c.so")

ffi.cdef[[
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
]]

M.u5c=u5c
setup_enums()

--- Safely convert a char* to a lua string.
function M.safe_tostr(charptr)
   if charptr == nil then return "" end
   return ffi.string(charptr)
end

--- Useful predicates
function M.is_proto(b) return b.prototype==nil end
function M.is_instance(b) return b.prototype~=nil end
function M.is_cblock(b) return b.type==ffi.C.BLOCK_TYPE_COMPUTATION end
function M.is_iblock(b) return b.type==ffi.C.BLOCK_TYPE_INTERACTION end
function M.is_tblock(b) return b.type==ffi.C.BLOCK_TYPE_TRIGGER end
function M.is_cblock_instance(b) return M.is_cblock(b) and not M.is_proto(b) end
function M.is_iblock_instance(b) return M.is_iblock(b) and not M.is_proto(b) end
function M.is_tblock_instance(b) return M.is_tblock(b) and not M.is_proto(b) end
function M.is_cblock_proto(b) return M.is_cblock(b) and M.is_proto(b) end
function M.is_iblock_proto(b) return M.is_iblock(b) and M.is_proto(b) end
function M.is_tblock_proto(b) return M.is_tblock(b) and M.is_proto(b) end

------------------------------------------------------------------------------
--                           Node and block API
------------------------------------------------------------------------------

--- Dealing with modules. This should be moved to C eventually.
u5c_modules = {}
function M.load_module(ni, libfile)
   local mod=ffi.load(libfile)
   if mod.__initialize_module(ni) ~= 0 then
      error("failed to init module "..libfile)
   end
   u5c_modules[#u5c_modules+1]=mod
end

function M.unload_modules(ni)
   for _,mod in ipairs(u5c_modules) do mod.__cleanup_module(ni) end
   u5c_modules={}
end


--- Create and initalize a new node_info struct
function M.node_create(name)
   local ni=ffi.new("u5c_node_info_t")
   print("u5c_node_init:", u5c.u5c_node_init(ni, name))
   return ni
end

function M.node_cleanup(ni)
   M.blocks_foreach(ni, function (b) M.block_unload(ni, b) end)
   M.unload_modules(ni)
end

--- Create a new computational block.
-- @param ni node_info ptr
-- @param type of block to create
-- @param name name of block
-- @return new computational block
function M.block_create(ni, type, name, conf)
   local b=u5c.u5c_block_create(ni, type, name)
   if b==nil then error("failed to create block "..ts(name).." of type "..ts(type)) end
   if conf then M.set_config_tab(b, conf) end
   return b
end

M.block_rm = u5c.u5c_block_rm
M.block_get = u5c.u5c_block_get

--- Life cycle handling
M.block_init = u5c.u5c_block_init
M.block_start = u5c.u5c_block_start
M.block_stop = u5c.u5c_block_stop
M.block_cleanup = u5c.u5c_block_cleanup

M.cblock_step = u5c.u5c_cblock_step

--- Port and Connections handling
M.block_port_get = u5c.u5c_port_get
M.connect_one = u5c.u5c_connect_one

M.block_get = u5c.u5c_block_get

--- Unload a block: bring it to state preinit and call u5c_block_rm
function M.block_unload(ni, name)
   local b=M.block_get(ni, name)
   if b.block_state==ffi.C.BLOCK_STATE_ACTIVE then M.block_stop(ni, b) end
   if b.block_state==ffi.C.BLOCK_STATE_INACTIVE then M.block_cleanup(ni, b) end
   if M.block_rm(ni, name) ~= 0 then error("block_unload: u5c_block_rm failed for '"..name.."'") end
end

------------------------------------------------------------------------------
--                           Data type handling
------------------------------------------------------------------------------

function M.data_len(d) return tonumber(u5c.u5c_data_len(d)) end

function M.data_alloc(ni, name, num)
   num=num or 1
   local d = u5c.u5c_data_alloc(ni, name, num)
   if d==nil then error("data_alloc: unkown type '"..name.."'") end
   return d
end

M.data_free=u5c.u5c_data_free
M.type_get = u5c.u5c_type_get

local def_sep = '___'

--- Make a namespace'd version of the given struct
-- @param ns namespace to add
-- @param name stuct name or typedef'ed name
-- @param optional boolean flag. if true the prefix with 'struct'
-- @return namespaced version [struct] pack..del..name
local function make_ns_name(ns, name, struct)
   if struct then struct='struct' else struct="" end
   return ("struct %s%s%s"):format(ns,def_sep,name)
end

-- package/struct foo -> package, struct, foo
local pack_struct_patt = "%s*([%w_]+)%s*/%s*(struct)%s([%w_]+)"

-- package/foo_t -> package, foo_t
local pack_typedef_patt = "%s*([%w_]+)%s*/%s*([%w_]+)"

--- Split a package/struct foo string int package, [struct], name
-- @param name string
-- @return package name
-- @return struct (empty string if typedef'ed)
-- @return name of type
local function parse_structname(n)
   local pack, name, struct

   pack, struct, name = string.match(n, pack_struct_patt)
   if pack==nil then
      -- package/foo_t
      pack, name = string.match(n, pack_typedef_patt)
   end
   if pack==nil then error("failed to parse struct name '"..n.."'") end
   return pack, struct or "", name
end

local typedef_struct_patt = "%s*typedef%s+struct%s+([%w_]*)%s*(%b{})%s*([%w_]+)%s*;"
local struct_def_patt = "%s*struct%s+([%w_]+)%s*(%b{});"

--- Extend a struct definition with a namespace
-- namespace is prepended to identifiers separated by three '_'
-- @param ns string namespace to add
-- @param struct struct as string
-- @return rewritten struct
-- @return name1 u5c namespaced' version of struct name
-- @return name2 u5c namespaced' version of typedef'ed alias
local function struct_add_ns(ns, struct_str)
   local name, body, alias, newstruct

   -- try matching a typedef struct [name] { ... } name_t;
   -- name could be empty
   name, body, alias = string.match(struct_str, typedef_struct_patt)

   if body and alias then
      if name ~= "" then name = make_ns_name(ns, name) end
      alias = make_ns_name(ns, alias)
      newstruct = ("typedef struct %s %s %s;"):format(name, body, alias);
      return newstruct, name, alias
   end

   -- try matching a simple struct def
   name, body = string.match(struct_str, struct_def_patt)
   if name and body then
      name = make_ns_name(ns, name, true)
      newstruct = name .." "..body..";"
      return newstruct, name
   end

   return false
end

--- Convert an u5c 'package/struct foo' to the namespaced version.
-- @param n u5c type name
-- @return 'struct package___foo'
-- @return package
local function structname_to_ns(n)
   local pack, struct, name = parse_structname(n)
   return make_ns_name(pack, name, struct), pack
end

--- Load the registered C types into the luajit ffi.
-- @param ni node_info_t*
function M.ffi_load_types(ni)
   local function ffi_load_no_ns(t)
      if t.type_class==u5c.TYPE_CLASS_STRUCT and t.private_data~=nil then
	 local struct_str = ffi.string(t.private_data)
	 local ret, err = pcall(ffi.cdef, struct_str)
	 if ret==false then
	    error("ffi_load_types: failed to load the following struct:\n"..struct_str.."\n"..err)
	 end
      end
   end

   local function ffi_load_type(t)
      local ns_struct, n1, n2, n3
      if t.type_class==u5c.TYPE_CLASS_STRUCT and t.private_data~=nil then
	 local n1, pack = structname_to_ns(ffi.string(t.name))

	 -- extract pack, struct, name from t.name
	 -- TODO: warn in struct name extraced via parse_structname is
	 -- not the same as in body definition (enforce, allow, load both?)
	 -- call struct_add_ns with CORRECT ns
	 -- print("loading ffi type ", ffi.string(t.name))
	 ns_struct, n2, n3 = struct_add_ns(pack, ffi.string(t.private_data))

	 if n1~=n2 and n1~=n3 then
	    error(("ffi_load_types: mismatch between C struct name (%s, %s) and type name %s"):format(n2, n3, n1))
	 end
	 local ret, err = pcall(ffi.cdef, ns_struct)
	 if ret==false then error("ffi_load_types: failed to load the following struct:\n"..ns_struct.."\n"..err) end
      end
      return ns_struct or false
   end
   local type_list = {}
   M.types_foreach(ni, function (t) type_list[#type_list+1] = t end)
   table.sort(type_list, function (t1,t2) return t1.seqid<t2.seqid end)
   utils.foreach(ffi_load_no_ns, type_list)
end


--- print an u5c_data_t
-- @deprecated
-- requires reflect
-- TODO: using reflect make a struct2tab function.
function M.data2str(d)
   if d.type.type_class~=u5c.TYPE_CLASS_BASIC then
      return "can currently only print TYPE_CLASS_BASIC types"
   end
   local ptrname = ffi.string(d.type.name).."*"
   local dptr = ffi.new(ptrname, d.data)
   return string.format("0x%x", dptr[0]), "("..ts(dptr)..", "..ffi.string(d.type.name)..")"
end

--- Convert an u5c_type_t to a FFI ctype object.
-- Only works for TYPE_CLASS_BASIC and TYPE_CLASS_STRUCT
-- @param u5c_type_t
-- @return luajit FFI ctype
local function __type_to_ctype_str(t, ptr)
   if ptr then ptr='*' else ptr="" end
   if t.type_class==ffi.C.TYPE_CLASS_BASIC then
      return ffi.string(t.name)..ptr
   elseif t.type_class==ffi.C.TYPE_CLASS_STRUCT then
      return structname_to_ns(ffi.string(t.name))..ptr
   end
end

--- No NS variant:
-- Only works for TYPE_CLASS_BASIC and TYPE_CLASS_STRUCT
-- @param u5c_type_t
-- @return luajit FFI ctype
local function type_to_ctype_str(t, ptr)
   if ptr then ptr='*' else ptr="" end
   if t.type_class==ffi.C.TYPE_CLASS_BASIC then
      return ffi.string(t.name)..ptr
   elseif t.type_class==ffi.C.TYPE_CLASS_STRUCT then
      local pack, struct, name = parse_structname(ffi.string(t.name))
      return struct.." "..name..ptr
   end
end

-- memoize?
local function type_to_ctype(t, ptr)
   local ctstr=type_to_ctype_str(t, ptr)
   return ffi.typeof(ctstr)
end

--- Transform the value of a u5c_data_t* to a lua FFI cdata.
-- @param u5c_data_t
-- @return
function data_to_cdata(d)
   local ctp = type_to_ctype(d.type, true)
   return ffi.cast(ctp, d.data)
end

function M.data_resize(d, newlen)
   print("changing len from", tonumber(d.len), " to ", newlen)
   if u5c.u5c_data_resize(d, newlen) == 0 then
      return true
   else
      return false
   end
end

--- Assign a value to a u5c_data
-- @param d u5c_data
-- @param value to assign (must follow the luajit FFI initialization rules)
-- @param resize if true, resize buffer and update data.len to fit val
-- @return cdata ptr of the contained type
function M.data_set(d, val, resize)
   local d_cdata = data_to_cdata(d)

   -- find cdata of the target u5c_data
   local val_type=type(val)
   if val_type=='table' then
      for k,v in pairs(val) do
	 if type(k)~='number' then
	    d_cdata[k]=v
	 else
	    local idx -- starting from zero
	    if val[0] == nil then idx=k-1 else idx=k end
	    if idx >= d.len and not resize then
	       error("data_set: attempt to index beyond bounds, index=", idx, "len=", d.len)
	    elseif idx >= d.len and resize then
	       M.data_resize(d, idx+1)
	    end
	    d_cdata[idx]=v
	 end
      end
   elseif val_type=='string' then ffi.copy(d_cdata, val, #val)
   elseif val_type=='number' then d_cdata[0]=val
   else
      error("set_config: don't know how to assign "..
	    tostring(val).." to ffi type "..tostring(d_cdata))
   end
   return d_cdata
end

--- Set a configuration value
-- @param block
-- @param conf_name name of configuration value
-- @param confval value to assign (must follow luajit FFI initialization rules)
function M.set_config(b, name, val)
   local d = u5c.u5c_config_get_data(b, name)
   if d == nil then error("set_config: unknown config '"..name.."'") end
   print("configuring ", ffi.string(b.name), name)
   return M.data_set(d, val, true)
end

function M.set_config_tab(b, ctab)
   for n,v in pairs(ctab) do M.set_config(b, n, v) end
end

------------------------------------------------------------------------------
--                              Interactions
------------------------------------------------------------------------------

--- TODO!
function M.interaction_read(ni, i)
   if i.block_state ~= ffi.C.BLOCK_STATE_ACTIVE then
      error("interaction_read: interaction not readable in state "..M.block_state_tostr[i.block_state])
   end
   -- figure this out automatically.
   local rddat=M.data_alloc(ni, "unsigned int", 1)
   local res
   local res=i.read(i, rddat)
   if res <= 0 then res=M.retval_tostr[res] end
   return res, rddat
end

--- TODO!
function M.interaction_write(i, val)
end

------------------------------------------------------------------------------
--                   Usefull stuff: foreach, pretty printing
------------------------------------------------------------------------------

--- Convert a u5c_type to a Lua table
function M.u5c_type_totab(t)
   if t==nil then error("NULL type"); return end
   local res = {}
   res.name=M.safe_tostr(t.name)
   res.class=M.type_class_tostr[t.type_class]
   res.size=tonumber(t.size)
   if t.type_class==u5c.TYPE_CLASS_STRUCT then res.model=M.safe_tostr(t.private_data) end
   return res
end

--- Print a u5c_type
function M.type_tostr(t, verb)
   local tt=M.u5c_type_totab(t)
   local res=("u5c_type_t: name=%s, size=%d, type_class=%s"):format(tt.name, tt.size, tt.class)
   if verb then res=res.."\nmodel=\n"..tt.model end
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
   -- TODO ports, configs
   return res
end

--- Convert a block to a Lua string.
function M.block_tostr(b)
   local bt=M.block_totab(b)
   local fmt="u5c_block_t: name=%s, block_type=%s, state=%s, prototype=%s"
   local res=fmt:format(bt.name, bt.block_type, bt.state, bt.prototype, bt.stat_num_steps)
   -- TODO ports, conf if verbose
   return res
end

--- Call a function on every known type.
-- @param ni u5c_node_info_t*
-- @param fun function to call on type.
function M.types_foreach(ni, fun)
   if not fun then error("types_foreach: missing/invalid fun argument") end
   if ni.types==nil then return end
   local u5c_type_t_ptr = ffi.typeof("u5c_type_t*")
   local typ=ni.types
   while typ ~= nil do
      fun(typ)
      typ=ffi.cast(u5c_type_t_ptr, typ.hh.next)
   end
end

--- Call a function on every block of the given list.
-- @param a block_list
-- @param function
function M.blocks_foreach(ni, fun, pred)
   if ni==nil then return end
   pred = pred or function() return true end
   local u5c_block_t_ptr = ffi.typeof("u5c_block_t*")
   local b=ni.blocks
   while b ~= nil do
      if pred(b) then fun(b) end
      b=ffi.cast(u5c_block_t_ptr, b.hh.next)
   end
end

function M.blocks_map(ni, fun, pred)
   local res = {}
   if ni==nil then return end
   pred = pred or function() return true end
   local u5c_block_t_ptr = ffi.typeof("u5c_block_t*")
   local b=ni.blocks
   while b ~= nil do
      if pred(b) then res[#res+1]=fun(b) end
      b=ffi.cast(u5c_block_t_ptr, b.hh.next)
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

function M.tblocks_foreach(ni, fun, pred)
   pred = pred or function() return true end
   M.blocks_foreach(ni, fun, function(b)
				if not M.is_tblock(b) then return end
				return pred(b)
			     end)
end

--- Pretty print helpers
function M.print_types(ni) types_foreach(ni, u5c_type_pp) end
function M.print_cblocks(ni) M.cblocks_foreach(ni, function(b) print(M.block_tostr(b)) end) end
function M.print_iblocks(ni) M.iblocks_foreach(ni, function(b) print(M.block_tostr(b)) end) end
function M.print_tblocks(ni) M.tblocks_foreach(ni, function(b) print(M.block_tostr(b)) end) end

function M.num_blocks(ni)
   local num_cb, num_ib, num_tb, inv = 0,0,0,0
   M.block_foreach(ni, function (b)
			  if b.block_type==ffi.C.BLOCK_TYPE_COMPUTATION then num_cb=num_cb+1
			  elseif b.block_type==ffi.C.BLOCK_TYPE_INTERACTION then num_ib=num_ib+1
			  elseif b.block_type==ffi.C.BLOCK_TYPE_TRIGGER then num_tb=num_tb+1
			  else inv=inv+1 end
		       end)
   return num_cb, num_ib, num_tb, inv
end

function M.num_types(ni) return u5c.u5c_num_types(ni) end

--- Print an overview of current node state.
function M.ni_stat(ni)
   print(string.rep('-',78))
   local num_cb, num_ib, num_tb, num_inv
   print("cblocks:"); M.print_cblocks(ni);
   print("iblocks:"); M.print_iblocks(ni);
   print("tblocks:"); M.print_tblocks(ni);
   print(tostring(num_cb).." cblocks, ",
	 tostring(num_ib).." iblocks, ",
	 tostring(num_tb).." tblocks",
	 tostring(u5c.u5c_num_types(ni)).." types")
   print(string.rep('-',78))
end



return M