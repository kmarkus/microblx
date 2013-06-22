--- u5C Lua interface
local ffi=require "ffi"
local u5c_utils = require("lua/u5c_utils")
local ts=tostring

local M={}
local setup_enums

-- call only after loading libu5c!
local function setup_enums()
   M.retval_tostr = {
      [0] ='OK',
      [-1] ='PORT_READ_NODATA',
      [-2] ='PORT_READ_NEWDATA',  
      [-3] ='EPORT_INVALID',
      [-4] ='EPORT_INVALID_TYPE',
      [-5] ='EINVALID_BLOCK_TYPE',
      [-6] ='ENOSUCHBLOCK',
      [-7] ='EALREADY_REGISTERED',
      [-8] ='EOUTOFMEM'
}

   M.block_type_tostr={
      [ffi.C.BLOCK_TYPE_COMPUTATION]="cblock",
      [ffi.C.BLOCK_TYPE_INTERACTION]="iblock",
      [ffi.C.BLOCK_TYPE_TRIGGER]="tblock" 
   }
   
   M.block_state_tostr={
      ffi.C.BLOCK_STATE_PREINIT,
      ffi.C.BLOCK_STATE_INACTIVE,
      ffi.C.BLOCK_STATE_ACTIVE
   }
end

-- load u5c_types and library
ffi.cdef(u5c_utils.read_file("src/uthash_ffi.h"))
ffi.cdef(u5c_utils.read_file("src/u5c_types.h"))
local u5c=ffi.load("src/libu5c.so")
M.u5c=u5c
setup_enums()

--- Create and initalize a new node_info struct
function M.init_node()
   local ni=ffi.new("u5c_node_info_t")
   print("u5c_node_init:", u5c.u5c_node_init(ni))
   return ni
end


function M.cblock_create(ni, type, name)
   local b=u5c.u5c_block_create(ni, ffi.C.BLOCK_TYPE_COMPUTATION, type, name)
   if b==nil then error("failed to create cblock "..ts(name).." of type "..ts(type)) end
   return b
end

function M.iblock_create(ni, type, name)
   local i=u5c.u5c_block_create(ni, ffi.C.BLOCK_TYPE_INTERACTION, type, name)
   if i==nil then error("failed to create iblock "..ts(name).." of type "..ts(type)) end
   return i
end

-- trampoline
function M.block_init(ni, b) return u5c.u5c_block_init(ni, b) end
function M.block_start(ni, b) return u5c.u5c_block_start(ni, b) end
function M.block_stop(ni, b) return u5c.u5c_block_stop(ni, b) end
function M.block_cleanup(ni, b) return u5c.u5c_block_cleanup(ni, b) end

function M.block_port_get(b, pname) return u5c.u5c_port_get(b, pname) end
function M.connect_one(port, interaction) u5c.u5c_connect_one(port, interaction) end

function M.block_rm(ni, block_type, name) u5c.u5c_block_rm(ni, block_type, name) end

--- print an u5c_data_t
function M.data2str(d)
   if d.type.type_class~=u5c.TYPE_CLASS_BASIC then
      return "can currently only print TYPE_CLASS_BASIC types"
   end
   ptrname = ffi.string(d.type.name).."*"
   dptr = ffi.new(ptrname, d.data)
   return string.format("0x%x", dptr[0]), "("..ts(dptr)..", "..ffi.string(d.type.name)..")"
end

-- function set_config
-- end

function M.interaction_read(ni, i)
   -- figure this out automatically.
   local rddat=u5c.u5c_alloc_data(ni, "unsigned int", 1)
   local res
   local res=i.read(i, rddat)
   if res <= 0 then res=M.retval_tostr[res] end
   return res, rddat
end

function M.interaction_write(i, val)
end


function M.u5c_type_pp(t)
   if t==nil then error("NULL type"); return end
   print("name: "..ffi.string(t.name).. ", class: "..ts(t.type_class)..", size: "..ts(t.size))
   if t.type_class==u5c.TYPE_CLASS_STRUCT and t.private_data ~= nil then
      print("struct spec:\n"..ts(ffi.string(t.private_data)))
   end
end

--- TODO: evolve into u5c_block_to_tab
function M.u5c_block_pp(b)
   print("    ", 
	 ffi.string(b.name).." ("..ts(M.block_type_tostr[b.type])..")"
      .." state: "..ts(M.block_state_tostr[b.block_state]))
end

--- Call a function on every known type.
-- @param ni u5c_node_info_t*
-- @param fun function to call on type.
function M.types_foreach(ni, fun)
   if not fun then error("types_foreach: missing/invalid fun argument") end
   if ni.types==nil then return end
   u5c_type_t_ptr = ffi.typeof("u5c_type_t*")
   typ=u5c_type_t_ptr(ni.types)
   while typ ~= nil do
      fun(typ)
      typ=ffi.cast(u5c_type_t_ptr, typ.hh.next)
   end
end


function M.blocks_foreach(blklist, fun)
   if blklist==nil then return end
   u5c_block_t_ptr = ffi.typeof("u5c_block_t*")
   typ=u5c_block_t_ptr(blklist)
   while typ ~= nil do
      fun(typ)
      typ=ffi.cast(u5c_block_t_ptr, typ.hh.next)
   end
end

function M.ffi_load_types(ni)
   function ffi_load_type(t)
      local ns_struct
      if t.type_class==u5c.TYPE_CLASS_STRUCT and t.private_data~=nil then
	 print("loading ffi type ", ffi.string(t.name))
	 ns_struct = u5c_utils.struct_add_ns("huha", ffi.string(t.private_data))
	 ffi.cdef(ns_struct)
      end
      return ns_struct or false
   end
   M.types_foreach(ni, ffi_load_type)
end

function M.print_types(ni)
   types_foreach(ni, u5c_type_pp)
end

function M.print_cblocks(ni) M.blocks_foreach(ni.cblocks, M.u5c_block_pp) end
function M.print_iblocks(ni) M.blocks_foreach(ni.iblocks, M.u5c_block_pp) end
function M.print_tblocks(ni) M.blocks_foreach(ni.tblocks, M.u5c_block_pp) end


u5c_modules = {}
function M.load_module(ni, libfile)
   local mod=ffi.load(libfile)
   if mod.__initialize_module(ni) ~= 0 then
      error("failed to init module "..libfile)
   end
   u5c_modules[#u5c_modules+1]=mod
end

function M.unload_modules()
   for _,mod in ipairs(u5c_modules) do mod.__cleanup_module(ni) end
end

function M.ni_stat(ni)
   print(string.rep('-',78))
   print("cblocks:"); M.print_cblocks(ni);
   print("iblocks:"); M.print_iblocks(ni);
   print("tblocks:"); M.print_tblocks(ni);
   print(tostring(u5c.u5c_num_cblocks(ni)).." cblocks, ",
	 tostring(u5c.u5c_num_iblocks(ni)).." iblocks, ",
	 tostring(u5c.u5c_num_tblocks(ni)).." tblocks",
	 tostring(u5c.u5c_num_types(ni)).." tblocks")
   print(string.rep('-',78))
end

return M