#!/usr/bin/luajit

local ffi = require("ffi")
local u5c_utils = require("lua/u5c_utils")

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
   print(tostring(u5c.u5c_num_cblocks(ni)).." cblocks, ",
	 tostring(u5c.u5c_num_iblocks(ni)).." iblocks, ",
	 tostring(u5c.u5c_num_tblocks(ni)).." tblocks",
	 tostring(u5c.u5c_num_types(ni)).." tblocks")
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

ni_stat()

print("creating instance of 'random'")
random1=u5c.u5c_block_create(ni, ffi.C.BLOCK_TYPE_COMPUTATION, "random", "random1")

print("creating instance of 'hexdump'")
hexdump1=u5c.u5c_block_create(ni, ffi.C.BLOCK_TYPE_INTERACTION, "hexdump", "hexdump1")

ni_stat()

print("running random1 init", u5c.u5c_block_init(ni, random1))
print("running hexdump1 init", u5c.u5c_block_init(ni, hexdump1))

rand_port=u5c.u5c_port_get(random1, "rnd")

u5c.u5c_connect_one(rand_port, hexdump1)

for i=1,10 do
   random1.step(random1)
   -- os.execute("sleep 0.1")
end

print("cleaning up")
print("freeing random1", u5c.u5c_block_destroy(ni, ffi.C.BLOCK_TYPE_COMPUTATION, "random1"))
print("freeing hexdump1", u5c.u5c_block_destroy(ni, ffi.C.BLOCK_TYPE_INTERACTION, "hexdump1"))

unload_modules()
print(tostring(u5c.u5c_num_cblocks(ni)).." blocks loaded")

ni_stat()

