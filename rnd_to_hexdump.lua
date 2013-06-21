#!/usr/bin/luajit

local ffi = require("ffi")
local u5c_utils = require("lua/u5c_utils")
local ts=tostring
local u5c, retvals

local err2str = {
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


--- print an u5c_data_t
function data2str(d)
   if d.type.type_class~=u5c.TYPE_CLASS_BASIC then
      return "can currently only print TYPE_CLASS_BASIC types"
   end
   ptrname = ffi.string(d.type.name).."*"
   dptr = ffi.new(ptrname, d.data)
   return string.format("0x%x", dptr[0]), "("..ts(dptr)..", "..ffi.string(d.type.name)..")"
end

-- function set_config
-- end

function interaction_read(i)
   -- figure this out automatically.
   local rddat=u5c.u5c_alloc_data(ni, "unsigned int", 1)
   local res
   local res=i.read(i, rddat)
   if res <= 0 then res=err2str[res] end
   return res, rddat
end

function interaction_write(i, val)
end


function u5c_type_pp(t)
   if t==nil then error("NULL type"); return end
   print("name: "..ffi.string(t.name).. ", class: "..ts(t.type_class)..", size: "..ts(t.size))
   if t.type_class==u5c.TYPE_CLASS_STRUCT and t.private_data ~= nil then
      print("struct spec:\n"..ts(ffi.string(t.private_data)))
   end
end

local block_type  = { [1]='computation', [2]='interaction', [3]='TRIGGER' }
local block_state = { [0]='unknown', [1]='preinit', [2]='inactive', [3]='active' }

function u5c_block_pp(b)
   print("    ", ffi.string(b.name).." ("..block_type[b.type]..")".." state: "..block_state[b.block_state])
end

function types_foreach(ni, fun)
   if ni.types==nil then return end
   u5c_type_t_ptr = ffi.typeof("u5c_type_t*")
   typ=u5c_type_t_ptr(ni.types)
   while typ ~= nil do
      fun(typ)
      typ=ffi.cast(u5c_type_t_ptr, typ.hh.next)
   end
end

function blocks_foreach(blklist, fun)
   if blklist==nil then return end
   u5c_block_t_ptr = ffi.typeof("u5c_block_t*")
   typ=u5c_block_t_ptr(blklist)
   while typ ~= nil do
      fun(typ)
      typ=ffi.cast(u5c_block_t_ptr, typ.hh.next)
   end
end

function ffi_load_types(ni)
   function ffi_load_type(t)
      local ns_struct
      if t.type_class==u5c.TYPE_CLASS_STRUCT and t.private_data~=nil then
	 print("loading ffi type ", ffi.string(t.name))
	 ns_struct = u5c_utils.struct_add_ns("huha", ffi.string(t.private_data))
	 ffi.cdef(ns_struct)
      end
      return ns_struct or false
   end
   types_foreach(ni, ffi_load_type)
end

function print_types(ni)
   types_foreach(ni, u5c_type_pp)
end

function print_cblocks(ni) blocks_foreach(ni.cblocks, u5c_block_pp) end
function print_iblocks(ni) blocks_foreach(ni.iblocks, u5c_block_pp) end
function print_tblocks(ni) blocks_foreach(ni.tblocks, u5c_block_pp) end

function init_node()
   -- load u5c_types and library
   ffi.cdef(u5c_utils.read_file("src/uthash_ffi.h"))
   ffi.cdef(u5c_utils.read_file("src/u5c_types.h"))
   u5c=ffi.load("src/libu5c.so")

   -- create a node_info struct
   ni=ffi.new("u5c_node_info_t")

   -- initalize the node
   print("u5c_node_init:", u5c.u5c_node_init(ni))
   return ni
end

modules = {}
function load_module(ni, libfile)
   m=ffi.load(libfile)
   if m.__initialize_module(ni) ~= 0 then
      error("failed to init module "..libfile)
   end
   modules[#modules+1]=m
end

function unload_modules()
   for _,m in ipairs(modules) do m.__cleanup_module(ni) end
end

function ni_stat()
   print(string.rep('-',78))
   print("cblocks:"); print_cblocks(ni)
   print("iblocks:"); print_iblocks(ni)
   print("tblocks:"); print_tblocks(ni)
   print(tostring(u5c.u5c_num_cblocks(ni)).." cblocks, ",
	 tostring(u5c.u5c_num_iblocks(ni)).." iblocks, ",
	 tostring(u5c.u5c_num_tblocks(ni)).." tblocks",
	 tostring(u5c.u5c_num_types(ni)).." tblocks")
   print(string.rep('-',78))
end

-- prog starts here.

ni=init_node()

block_type_to_name={
   [ffi.C.BLOCK_TYPE_COMPUTATION]="cblock",
   [ffi.C.BLOCK_TYPE_INTERACTION]="iblock",
   [ffi.C.BLOCK_TYPE_TRIGGER]="tblock",
}

load_module(ni, "std_types/stdtypes/stdtypes.so")
load_module(ni, "std_blocks/random/random.so")
load_module(ni, "std_blocks/hexdump/hexdump.so")
load_module(ni, "std_blocks/lfds_buffers/lfds_cyclic.so")
load_module(ni, "std_blocks/webif/webif.so")

ni_stat()

print("creating instance of 'webif'")
webif1=u5c.u5c_block_create(ni, ffi.C.BLOCK_TYPE_COMPUTATION, "webif", "webif1")

print("creating instance of 'random'")
random1=u5c.u5c_block_create(ni, ffi.C.BLOCK_TYPE_COMPUTATION, "random", "random1")

u5c_type_pp(u5c.u5c_type_get(ni, "random/struct random_config"))
-- print_types(ni)
ffi_load_types(ni)

print("creating instance of 'hexdump'")
hexdump1=u5c.u5c_block_create(ni, ffi.C.BLOCK_TYPE_INTERACTION, "hexdump", "hexdump1")

print("creating instance of 'fifo'")
fifo1=u5c.u5c_block_create(ni, ffi.C.BLOCK_TYPE_INTERACTION, "lfds_cyclic", "fifo1")

ni_stat()

print("running webif init", u5c.u5c_block_init(ni, webif1))
print("running webif start", u5c.u5c_block_start(ni, webif1))

print("running random1 init", u5c.u5c_block_init(ni, random1))
print("running hexdump1 init", u5c.u5c_block_init(ni, hexdump1))
print("running fifo1 init", u5c.u5c_block_init(ni, fifo1))

rand_port=u5c.u5c_port_get(random1, "rnd")

u5c.u5c_connect_one(rand_port, hexdump1)
u5c.u5c_connect_one(rand_port, fifo1)

local res, dat
for i=1,8 do
   random1.step(random1)
   --res, dat = interaction_read(fifo1)
   --print("fifo1 read", res, data2str(dat))
   -- os.execute("sleep 0.1")
end

for i=1,8 do
   res, dat = interaction_read(fifo1)
   print("fifo1 read", res, data2str(dat))
   -- os.execute("sleep 0.1")
end

io.read()

print("cleaning up blocks --------------------------------------------------------")
print("webif1 cleanup", u5c.u5c_block_rm(ni, ffi.C.BLOCK_TYPE_COMPUTATION, "webif1"))

print("fifo1 cleanup", u5c.u5c_block_cleanup(ni, fifo1))
print("random1 cleanup", u5c.u5c_block_rm(ni, ffi.C.BLOCK_TYPE_COMPUTATION, "random1"))
print("hexdump1 cleanup", u5c.u5c_block_rm(ni, ffi.C.BLOCK_TYPE_INTERACTION, "hexdump1"))
print("fifo1 cleanup", u5c.u5c_block_rm(ni, ffi.C.BLOCK_TYPE_INTERACTION, "fifo1"))


print(tostring(u5c.u5c_num_cblocks(ni)).." blocks loaded")

ni_stat()

-- l1=u5c.u5c_alloc_data(ni, "unsigned long", 1)
-- if l1~=nil then print_data(l1) end

unload_modules()
ni_stat()
os.exit(1)

