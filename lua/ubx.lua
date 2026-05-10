-- -*- mode: lua; -*-
--
-- microblx: embedded, realtime safe, reflective function blocks.
--
-- Copyright (C) 2013 Markus Klotzbuecher <markus.klotzbuecher@mech.kuleuven.be>
-- Copyright (C) 2014-2020 Markus Klotzbuecher <mk@mkio.de>
--
-- SPDX-License-Identifier: BSD-3-Clause
--

local ffi = require("ffi")
local bit = require("bit")
local cdata = require("cdata")
local utils = require("utils")
local time = require("time")
local pt = require("prettytable")

local ts = tostring
local concat = table.concat
local fmt = string.format

--- LuaJIT FFI binding and Lua API for microblx.
--
-- Provides node, block, port, config, and data management for microblx.
-- C structs are exposed via LuaJIT FFI and augmented with Lua metatypes
-- for OO-style access (e.g. `b:pp()`, `p:read()`, `c:set(...)`).
--
-- @module ubx
-- @license BSD-3-Clause
local M = {}


--                           helpers

-- preprocess a C string
-- currently this means stripping out all preprocessor directives
local function preproc(str)
   local res = {}
   for l in str:gmatch("[^\n]+") do
      if not (string.match(l, "^%s*#%s*")) then res[#res+1] = l end
   end
   return table.concat(res, "\n")
end

--- Read the entire contents of a file.
-- @param file name of file
-- @return string contents
local function read_file(file)
   local f = assert(io.open(file, "rb"))
   local data = f:read("*all")
   f:close()
   return data
end

--- Compute the MD5 checksum of the given string.
-- @param str string to hash
-- @return hex-encoded MD5 string
function M.md5(str)
   local res = ffi.new("unsigned char[16]")
   M.ubx.md5(str, #str, res)
   return utils.str_to_hexstr(ffi.string(res, 16))
end


--
-- ffi based lfs.dir replacement
--
ffi.cdef[[
    typedef struct DIR DIR;
    struct dirent {
        unsigned long d_ino;        /* Inode number */
        unsigned long d_off;        /* Not an offset; see below */
        unsigned short d_reclen;    /* Length of this record */
        unsigned char d_type;        /* Type of file */
        char d_name[256];           /* Null-terminated filename */
    };

    DIR* opendir(const char* name);
    struct dirent* readdir(DIR* dirp);
    int closedir(DIR* dirp);
]]

--- Directory iterator (ffi-based `lfs.dir` replacement).
-- @param path directory path string
-- @return iterator yielding filenames (including `.` and `..`)
function M.dir(path)
   local dirp = ffi.C.opendir(path)

   if dirp == nil then
      error("failed to open directory: " .. path)
   end

   local function iterator()
      local entry = ffi.C.readdir(dirp)
      if entry ~= nil then
	 return ffi.string(entry.d_name)
      else
	 ffi.C.closedir(dirp)
	 return nil
      end
   end

   return iterator
end

--- Setup Enums
-- called internally after loading libubx
local function setup_enums()
   if M.ubx==nil then error("setup_enums called before loading libubx") end
   M.retval_tostr = {
      [0] ='OK',
      [ffi.C.EINVALID_BLOCK]		='EINVALID_BLOCK',
      [ffi.C.EINVALID_PORT]		='EINVALID_PORT',
      [ffi.C.EINVALID_CONFIG]		='EINVALID_CONFIG',
      [ffi.C.EINVALID_TYPE]		='EINVALID_TYPE',

      [ffi.C.EINVALID_BLOCK_TYPE]	='EINVALID_BLOCK_TYPE',
      [ffi.C.EINVALID_PORT_TYPE]	='EINVALID_PORT_TYPE',
      [ffi.C.EINVALID_CONFIG_TYPE]	='EINVALID_CONFIG_TYPE',

      [ffi.C.EINVALID_CONFIG_LEN]	='EINVALID_CONFIG_LEN',

      [ffi.C.EINVALID_PORT_DIR]		='EINVALID_PORT_DIR',

      [ffi.C.EINVALID_ARG]		='EINVALID_ARG',
      [ffi.C.EWRONG_STATE]		='EWRONG_STATE',
      [ffi.C.ENOSUCHENT]		='ENOSUCHENT',
      [ffi.C.EENTEXISTS]		='EENTEXISTS',
      [ffi.C.EALREADY_REGISTERED]	='EALREADY_REGISTERED',
      [ffi.C.ETYPE_MISMATCH]		='ETYPE_MISMATCH',
      [ffi.C.EOUTOFMEM]			='EOUTOFMEM',
   }

   M.block_type_tostr={
      [ffi.C.BLOCK_TYPE_COMPUTATION]="cblock",
      [ffi.C.BLOCK_TYPE_INTERACTION]="iblock",
   }

   M.type_class_tostr={
      [ffi.C.TYPE_CLASS_BASIC]='basic',
      [ffi.C.TYPE_CLASS_STRUCT]='struct',
   }

   M.block_state_tostr={
      [ffi.C.BLOCK_STATE_PREINIT]='preinit',
      [ffi.C.BLOCK_STATE_INACTIVE]='inactive',
      [ffi.C.BLOCK_STATE_ACTIVE]='active'
   }

   M.block_attrs_tostr={
      [ffi.C.BLOCK_ATTR_TRIGGER]='trigger',
      [ffi.C.BLOCK_ATTR_ACTIVE]='active',
      [ffi.C.BLOCK_ATTR_REALTIME]='realtime',
   }
end

local ubx_ffi_headers = {
   "include/ubx/ubx_uthash_ffi.h",
   "include/ubx/ubx_types.h",
   "include/ubx/ubx_core.h",
   "include/ubx/ubx_time.h",
   "include/ubx/md5.h",
   "include/ubx/ubx_utils.h",
}

local ubx_ffi_lib = nil
local ubx_ffi_libs = { "lib/libubx.so", "lib/libubx.so.0" }

local ubx = nil
local prefixes = { "/usr", "/usr/local" }
local core_prefix = nil


--- Load ubx into the luajit ffi
local function load_ubx_ffi()
   local function find_core_prefix(pfxs)
      for _,pf in ipairs(pfxs) do
	 local match=true
	 utils.foreach(
	    function(f) match = match and utils.file_exists(pf.."/"..f) end, ubx_ffi_headers)

	 local libubx
	 utils.foreach(
	    function(l)
	       if utils.file_exists(pf.."/"..l) then libubx = l end
	    end, ubx_ffi_libs)

	 if libubx and match == true then return pf, libubx end
      end
      utils.stderr("failed to load ubx core under the prefixes\n"..
		   table.concat(prefixes, '\n'))
      os.exit(1)
   end

   -- override prefixes?
   local ubx_path = os.getenv("UBX_PATH")
   if ubx_path then prefixes = utils.split(ubx_path, ':') end

   core_prefix, ubx_ffi_lib = find_core_prefix(prefixes)

   -- declare std functions
   ffi.cdef [[
   void *malloc(size_t size);
   void free(void *ptr);
   void *calloc(size_t nmemb, size_t size);
   void *realloc(void *ptr, size_t size);
   ]]

   -- load headers and ubx lib
   for _,h in ipairs(ubx_ffi_headers) do
      ffi.cdef(preproc(read_file(core_prefix.."/"..h)))
   end

   ubx = ffi.load(core_prefix.."/"..ubx_ffi_lib)

   setmetatable(M, { __index=function(t,k) return ubx["ubx_"..k] end })
   M.ubx = ubx
   setup_enums()
end

load_ubx_ffi()

--- Return ubx load prefix information.
-- @return `core_prefix` string — prefix from which `libubx` was loaded
-- @return `prefixes` table — search prefixes used to load modules
function M.get_prefix() return core_prefix, prefixes end

--- Safely convert a C string to a Lua string, returning `""` for nil.
--
-- Use this for pointer fields that may be nil: `ubx_type_t.name`,
-- `ubx_type_t.doc`, `ubx_port_t.doc`, `ubx_config_t.doc`,
-- `ubx_block_t.meta_data`, `ubx_module_t.id` / `.spdx_license_id`,
-- `ubx_type_t.private_data`, and any return value from a C function.
--
-- Use `ffi.string()` directly for fixed char-array fields that are
-- never nil: `ubx_node_t.name`, `ubx_block_t.name`, `ubx_port_t.name`,
-- `ubx_config_t.name`.
-- @param charptr `const char*` that may be nil
-- @return Lua string, `""` for nil
function M.safe_tostr(charptr)
   if charptr == nil then return "" end
   return ffi.string(charptr)
end

local safe_tostr = M.safe_tostr

--- Return the git-describe version string of the microblx library.
-- @return version string, e.g. `"v0.9.2-154-gabcdef"` or `"unknown"`
function M.git_version()
   return ffi.string(ubx.ubx_git_version())
end

--- Return the module directory version string (MAJOR.MINOR).
-- @return module version string, e.g. `"0.9"`
function M.mod_version()
   return ffi.string(ubx.ubx_mod_version())
end

--- Predicates
-- @section Predicates

--- Check if x is a `ubx_node_t`.
-- @param x value to test
-- @return boolean
function M.is_node(x) return ffi.istype("ubx_node_t", x) end

--- Check if x is a `ubx_block_t`.
-- @param x value to test
-- @return boolean
function M.is_block(x) return ffi.istype("ubx_block_t", x) end

--- Check if x is a `ubx_config_t`.
-- @param x value to test
-- @return boolean
function M.is_config(x) return ffi.istype("ubx_config_t", x) end

--- Check if x is a `ubx_port_t`.
-- @param x value to test
-- @return boolean
function M.is_port(x) return ffi.istype("ubx_port_t", x) end

--- Check if x is a `ubx_data_t`.
-- @param x value to test
-- @return boolean
function M.is_data(x) return ffi.istype("ubx_data_t", x) end

--- Check if b is a prototype (not an instance).
-- @param b `ubx_block_t`
-- @return boolean
function M.is_proto(b) assert(M.is_block(b)); return b.prototype == nil end

--- Check if b is an instance (not a prototype).
-- @param b `ubx_block_t`
-- @return boolean
function M.is_instance(b) return not M.is_proto(b) end

--- Check if b is a computation block.
-- @param b `ubx_block_t`
-- @return boolean
function M.is_cblock(b) return M.is_block(b) and b.type==ffi.C.BLOCK_TYPE_COMPUTATION end

--- Check if b is an interaction block.
-- @param b `ubx_block_t`
-- @return boolean
function M.is_iblock(b) return M.is_block(b) and b.type==ffi.C.BLOCK_TYPE_INTERACTION end

--- Check if b is a cblock instance.
-- @param b `ubx_block_t`
-- @return boolean
function M.is_cblock_instance(b) return M.is_cblock(b) and not M.is_proto(b) end

--- Check if b is an iblock instance.
-- @param b `ubx_block_t`
-- @return boolean
function M.is_iblock_instance(b) return M.is_iblock(b) and not M.is_proto(b) end

--- Check if b is a cblock prototype.
-- @param b `ubx_block_t`
-- @return boolean
function M.is_cblock_proto(b) return M.is_cblock(b) and M.is_proto(b) end

--- Check if b is an iblock prototype.
-- @param b `ubx_block_t`
-- @return boolean
function M.is_iblock_proto(b) return M.is_iblock(b) and M.is_proto(b) end

--- Check if p has an output type (is an outport).
-- @param p `ubx_port_t`
-- @return boolean
function M.is_outport(p) assert(M.is_port(p)); return p.out_type ~= nil end

--- Check if p has an input type (is an inport).
-- @param p `ubx_port_t`
-- @return boolean
function M.is_inport(p) assert(M.is_port(p)); return p.in_type ~= nil end

--- Check if p is both an inport and an outport.
-- @param p `ubx_port_t`
-- @return boolean
function M.is_inoutport(p) return M.is_outport(p) and M.is_inport(p) end

--                           LOGGING API

-- @section Logging

--- Log a message via rtlog.
-- Only emitted when `level <= node.loglevel`.
-- @param level `UBX_LOGLEVEL_*` constant
-- @param node `ubx_node_t` handle
-- @param src source identifier string
-- @param str `string.format`-style format string
-- @param ... optional format arguments
local function log(level, node, src, str, ...)
   if level <= node.loglevel then
      ubx.__ubx_log(level, node, src, fmt(str, ...))
   end
end

local function emerg(node, src, str, ...) M.log(ffi.C.UBX_LOGLEVEL_EMERG,  node, src, str, ...) end
local function alert(node, src, str, ...)  M.log(ffi.C.UBX_LOGLEVEL_ALERT,  node, src, str, ...) end
local function crit(node, src, str, ...)   M.log(ffi.C.UBX_LOGLEVEL_CRIT,   node, src, str, ...) end
local function err(node, src, str, ...)    M.log(ffi.C.UBX_LOGLEVEL_ERR,    node, src, str, ...) end
local function warn(node, src, str, ...)   M.log(ffi.C.UBX_LOGLEVEL_WARN,   node, src, str, ...) end
local function notice(node, src, str, ...) M.log(ffi.C.UBX_LOGLEVEL_NOTICE, node, src, str, ...) end
local function info(node, src, str, ...)   M.log(ffi.C.UBX_LOGLEVEL_INFO,   node, src, str, ...) end
local function dbg(node, src, str, ...)    M.log(ffi.C.UBX_LOGLEVEL_DEBUG,  node, src, str, ...) end

M.log = log
M.emerg = emerg
M.alert = alert
M.crit = crit
M.err = err
M.warn = warn
M.notice = notice
M.info = info
M.debug = dbg


--                           OS API

-- @section OS

--- Retrieve the current time via `ubx_gettime`.
-- The underlying clock source is selected at build time: TSC, CNTVCT
-- (aarch64), or POSIX `CLOCK_MONOTONIC` (default).
-- @param ts *optional* `struct ubx_timespec` to fill in-place
-- @return `struct ubx_timespec` with current time
function M.gettime(ts)
   ts = ts or ffi.new("struct ubx_timespec")
   ubx.ubx_gettime(ts)
   return ts
end

--- Sleep for a relative duration (yields CPU).
-- Not real-time safe. For RT contexts use `nanowait`.
-- @param ts_or_sec `struct ubx_timespec` *or* seconds as number
-- @param nsec *optional* nanoseconds (only when first arg is a number, default `0`)
function M.nanosleep(ts_or_sec, nsec)
   local ts
   if ffi.istype("struct ubx_timespec", ts_or_sec) then
      ts = ts_or_sec
   else
      ts = ffi.new("struct ubx_timespec")
      ts.sec = ts_or_sec
      ts.nsec = nsec or 0
   end
   ubx.ubx_nanosleep(ts)
end

--- Busy-wait for a relative duration (RT-safe, no CPU yield).
-- @param ts_or_sec `struct ubx_timespec` *or* seconds as number
-- @param nsec *optional* nanoseconds (only when first arg is a number, default `0`)
function M.nanowait(ts_or_sec, nsec)
   local ts
   if ffi.istype("struct ubx_timespec", ts_or_sec) then
      ts = ts_or_sec
   else
      ts = ffi.new("struct ubx_timespec")
      ts.sec = ts_or_sec
      ts.nsec = nsec or 0
   end
   ubx.ubx_nanowait(ts)
end

-- deprecated aliases
M.clock_mono_gettime = M.gettime
M.clock_mono_sleep = M.nanosleep

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


--                           Node API

-- @section Node

--- Remove a node manually.
-- Clears the gc finalizer and calls `ubx_node_rm`.
-- The `ffi.new`-allocated `ubx_node_t` itself is still gc'ed normally.
-- @param nd `ubx_node_t`
function M.node_rm(nd)
   ffi.gc(nd, nil)
   ubx.ubx_node_rm(nd)
end

--- Create and initialize a new node.
-- Finalizer calls `ubx_node_rm` on gc.
-- @param name node name string
-- @param params *optional* table with keys: `mlockall` (bool), `dumpable` (bool), `loglevel` (number)
-- @return `ubx_node_t`
function M.node_create(name, params)
   local nd = ffi.gc(ffi.new("ubx_node_t"), ubx.ubx_node_cleanup)
   params = params or {}
   local attrs=0
   if params.mlockall then attrs = bit.bor(attrs, ffi.C.ND_MLOCK_ALL) end
   if params.dumpable then attrs = bit.bor(attrs, ffi.C.ND_DUMPABLE) end
   if params.loglevel then nd.loglevel = params.loglevel end
   assert(ubx.ubx_node_init(nd, name, attrs)==0, "node_create failed")
   return nd
end

--- Load and initialize a ubx module.
-- Searches `UBX_PATH` prefixes for `lib/ubx/<ver>/<libfile>.so`.
-- No-ops (with a notice log) if the module is already loaded.
-- @param nd `ubx_node_t` to load into
-- @param libfile module name or path (`.so` appended if absent)
-- @return full resolved module path string
function M.load_module(nd, libfile)
   local ver = safe_tostr(ubx.ubx_mod_version())
   local modfile = "/lib/ubx/"..ver.."/"..libfile

   for _,pf in ipairs(prefixes) do
      local modpath = pf..modfile
      if string.sub(modpath, -3) ~= '.so' then modpath = modpath .. ".so" end

      if utils.file_exists(modpath) then
	 local res = ubx.ubx_module_load(nd, modpath)
	 if res == ffi.C.EENTEXISTS then
	    notice(nd, "lua", "module "..modpath.." already loaded")
	    return modpath
	 elseif res ~= 0 then
	    error("loading module "..modpath.." failed")
	 end
	 info(nd, "lua", "loaded module "..modpath)
	 M.ffi_load_types(nd)
	 return modpath
      end
   end
   error("no module "..modfile.." found under prefixes "..concat(prefixes, ', '))
end

--- Convert a node to a Lua table.
-- @param nd `ubx_node_t`
-- @return table `{ modules={...}, types={[name]=tab,...}, blocks={[name]=tab,...} }`
function M.node_totab(nd)
   local modules, types, blocks = {}, {}, {}

   M.modules_foreach(nd, function(m)
      modules[#modules+1] = { id=safe_tostr(m.id), license=safe_tostr(m.spdx_license_id) }
   end)

   M.types_foreach(nd, function(_t)
      local t = M.ubx_type_totab(_t)
      types[t.name] = t
   end)

   M.blocks_map(nd, function(_b)
      local b = M.block_totab(_b)
      blocks[b.name] = b
   end)

   return { modules=modules, types=types, blocks=blocks }
end


--- Cleanup a node: cleanup and remove instances and unload modules.
-- @param nd node info
function M.node_cleanup(nd)
   ffi.gc(nd, nil)
   ubx.ubx_node_cleanup(nd)
   collectgarbage("collect")
end

--- Create a new block instance.
-- @param nd `ubx_node_t`
-- @param type **prototype** name string (e.g. `"ubx/trig"`)
-- @param name **instance** name string
-- @param conf *optional* config table `{key=val,...}`
-- @return `ubx_block_t` in `preinit` state
function M.block_create(nd, type, name, conf)
   local b=ubx.ubx_block_create(nd, type, name)
   if b==nil then error("failed to create block "..ts(name).." of type "..ts(type)) end
   if conf then M.set_config_tab(b, conf) end
   return b
end

--- Return the default iblock prototype name (`ubx/lfds_cyclic` or `ubx/lfrb`).
-- Prefers `lfds_cyclic`; falls back to `lfrb`; errors if neither is loaded.
-- @param nd `ubx_node_t`
-- @return prototype name string
function M.get_default_iblock(nd)
   if ubx.ubx_block_get(nd, "ubx/lfds_cyclic") ~= nil then
      return "ubx/lfds_cyclic"
   elseif ubx.ubx_block_get(nd, "ubx/lfrb") ~= nil then
      return "ubx/lfrb"
   else
      error("neither lfds_cyclic or lfrb iblocks found")
   end
end

--- Get a block by name; **errors** if not found.
-- @param nd `ubx_node_t`
-- @param bname block name string
-- @return `ubx_block_t`
function M.block_get(nd, bname)
   local b = ubx.ubx_block_get(nd, bname)
   if b==nil then error("block_get: no block with name '"..ts(bname).."'") end
   return b
end

--- Unload a block: transition to `preinit` then call `ubx_block_rm`.
-- @param nd `ubx_node_t`
-- @param name block name string
function M.block_unload(nd, name)
   local b = M.block_get(nd, name)
   M.block_tostate(b, 'preinit')
   if M.block_rm(nd, name) ~= 0 then error("block_unload: ubx_block_rm failed for '"..name.."'") end
end

--- Count blocks in a node by type.
-- @param nd `ubx_node_t`
-- @return `#cblocks`, `#iblocks`, `#invalid`
function M.num_blocks(nd)
   local num_cb, num_ib, inv = 0,0,0
   M.blocks_map(nd,
		function (b)
		   if b.type==ffi.C.BLOCK_TYPE_COMPUTATION then num_cb=num_cb+1
		   elseif b.type==ffi.C.BLOCK_TYPE_INTERACTION then num_ib=num_ib+1
		   else inv=inv+1 end
		end)
   return num_cb, num_ib, inv
end

--- Return the number of registered types.
-- @param nd `ubx_node_t`
-- @return number of types
function M.num_types(nd) return ubx.ubx_num_types(nd) end

--- Pretty print a node
-- @param nd node_info
function M.node_pp(nd) print(pt.tostr(M.node_totab(nd))) end

-- add Lua OO methods
local ubx_node_mt = {
   __tostring = function(nd)
      local num_cb, num_ib, inv = M.num_blocks(nd)
      local num_types = M.num_types(nd)

      return fmt("%s <node>: #blocks: %d (#cb: %d, #ib: %d), #types: %d",
		 ffi.string(nd.name), num_cb + num_ib, num_cb, num_ib, num_types)
   end,
   __index = {
      get_name = function (nd) return ffi.string(nd.name) end,
      load_module = M.load_module,
      block_create = M.block_create,
      block_unload = M.block_unload,
      cleanup = M.node_cleanup,
      b = M.block_get,
      block_get = M.block_get,
      pp = M.node_pp,
   },
}
ffi.metatype("struct ubx_node", ubx_node_mt)


--                           Block API

-- @section Block

--- Check if a block has a given attribute flag set.
-- @param b `ubx_block_t`
-- @param attr `BLOCK_ATTR_*` constant
-- @return boolean
function M.block_hasattr(b, attr)
   if bit.band(b.attrs, attr) ~= 0 then return true end
   return false
end

--- Check if a block has the `ACTIVE` attribute.
-- @param b `ubx_block_t`
-- @return boolean
function M.block_isactive(b)
   return M.block_hasattr(b, ffi.C.BLOCK_ATTR_ACTIVE)
end

--- Check if a block has the `TRIGGER` attribute.
-- @param b `ubx_block_t`
-- @return boolean
function M.block_istrigger(b)
   return M.block_hasattr(b, ffi.C.BLOCK_ATTR_TRIGGER)
end

--- Check if a block has the `REALTIME` attribute.
-- @param b `ubx_block_t`
-- @return boolean
function M.block_isrealtime(b)
   return M.block_hasattr(b, ffi.C.BLOCK_ATTR_REALTIME)
end

--- Return the prototype name of a block instance.
-- @param b `ubx_block_t`
-- @return prototype name string, or `false` if `b` is itself a prototype
function M.block_prototype(b)
   if b.prototype == nil then return false end
   return ffi.string(b.prototype.name)
end

--- Bring a block to the given state
-- @param b block
-- @param tgtstate desired state ('active', 'inactive', 'preinit')
-- @return 0 if OK, nonzero otherwise
function M.block_tostate(b, tgtstate)
   local ret

   if b.block_state == tgtstate then return 0 end

   -- starting it up
   if (b.block_state == ffi.C.BLOCK_STATE_PREINIT and
       (tgtstate == 'inactive' or tgtstate == 'active')) then
      ret = M.block_init(b)
      if ret ~= 0 then return ret end
   end

   if (b.block_state == ffi.C.BLOCK_STATE_INACTIVE and tgtstate == 'active') then
      ret = M.block_start(b)
      if ret ~= 0 then return ret end
   end

   -- shutting it down
   if (b.block_state == ffi.C.BLOCK_STATE_ACTIVE and
       (tgtstate == 'inactive' or tgtstate == 'preinit')) then
      ret = M.block_stop(b)
      if ret ~= 0 then return ret end
   end

   if (b.block_state == ffi.C.BLOCK_STATE_INACTIVE and tgtstate == 'preinit') then
      ret = M.block_cleanup(b)
      if ret ~= 0 then return ret end
   end

   return 0
end

local function block_attr_totab(b)
   local t = {}
   if M.block_istrigger(b) then t[#t+1] = "trigger" end
   if M.block_isactive(b) then t[#t+1] = "active" end
   return t
end

--- Convert a block to a Lua table.
-- @param b `ubx_block_t`
-- @return table with `name`, `state`, `block_type`, `prototype`, `attrs`, `ports`, `configs`, and stat fields
function M.block_totab(b)
   if b==nil then error("NULL block") end

   local res = {}
   res.name = ffi.string(b.name)
   res.attrs = block_attr_totab(b)
   res.meta_data = safe_tostr(b.meta_data)
   res.block_type=M.block_type_tostr[b.type]
   res.state = M.block_state_tostr[b.block_state]

   if b.prototype ~= nil then
      res.prototype = ffi.string(b.prototype.name)
   else
      res.prototype = false
   end

   res.ports = M.ports_map(b, M.port_totab)
   res.configs = M.configs_map(b, M.config_totab)

   if M.is_cblock(b) then
      res.stat_num_steps = tonumber(b.stat_num_steps)
   elseif M.is_iblock(b) then
      res.stat_num_reads = tonumber(b.stat_num_reads)
      res.stat_num_writes = tonumber(b.stat_num_writes)
   end

   return res
end

--- Pretty-print a block.
-- @param b `ubx_block_t`
function M.block_pp(b) print(pt.tostr(M.block_totab(b))) end

--- Convert a block to a short one-line string.
-- @param b `ubx_block_t` or block table
-- @return `"name [prototype]"` string
function M.block_tostr(b)
   local bt

   if M.is_block(b) then
      bt = M.block_totab(b)
   else
      bt = b
   end

   return ("%s [%s]"):format(bt.name, bt.prototype or "proto")
end

--- Get a port by name; **errors** if not found.
-- @param b `ubx_block_t`
-- @param n port name string
-- @return `ubx_port_t`
function M.block_port_get (b, n)
   local res = ubx.ubx_port_get(b, n)
   if res==nil then error("port_get: no port with name '"..ts(n).."'") end
   return res
end
M.port_get = M.block_port_get

--- Get a config by name; returns `nil` if not found.
-- @param b `ubx_block_t`
-- @param n config name string
-- @return `ubx_config_t` or `nil`
function M.block_config_get (b, n)
   return ubx.ubx_config_get(b, n)
end
M.config_get = M.block_config_get

--- Call function on all ports of a block and return the result in a table.
-- @param b block
-- @param fun function to call on port
-- @param pred optional predicate function. fun is only called if pred is true.
-- @return result table.
function M.ports_map(b, fun, pred)
   local res={}
   pred = pred or function() return true end
   local port_ptr=b.ports
   while port_ptr~=nil do
      if pred(port_ptr) then res[#res+1]=fun(port_ptr) end
      port_ptr=port_ptr.next
   end
   return res
end

--- Call a function on all ports of a block (no return value).
-- @param b ubx_block_t
-- @param fun function to call on each ubx_port_t
-- @param pred optional predicate to filter ports
function M.ports_foreach(b, fun, pred)
   pred = pred or function() return true end
   local port_ptr=b.ports
   while port_ptr~=nil do
      if pred(port_ptr) then fun(port_ptr) end
      port_ptr=port_ptr.next
   end
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
   while conf_ptr~=nil do
      if pred(conf_ptr) then res[#res+1]=fun(conf_ptr) end
      conf_ptr=conf_ptr.next
   end
   return res
end

--- Set a configuration value by name. **Deprecated** — prefer `config_set`.
-- @param b `ubx_block_t`
-- @param name config name string
-- @param val value to assign (LuaJIT FFI init rules apply)
function M.set_config(b, name, val)
   local d = ubx.ubx_config_get_data(b, name)
   if d == nil then error("set_config: unknown config '"..name.."'") end
   return M.data_set(d, val, true)
end

--- Configure a block with a table of `{name=value}` pairs.
-- @param b `ubx_block_t`
-- @param ctab table of configuration values
function M.set_config_tab(b, ctab)
   for n,v in pairs(ctab) do M.set_config(b, n, v) end
end

--- Configure a block, handling **dynamically added configs**.
-- Applies known configs, calls `block_init`, then applies any configs
-- that only exist after init (e.g. added in the block's `init` hook).
-- **Requires** block to be in `preinit` state.
-- @param b `ubx_block_t`
-- @param ctab `{name=value}` configuration table
function M.do_configure(b, ctab)
   local deferred = {}

   local state = b:get_block_state()

   if state ~= 'preinit' then
      error("do_configure: block not in state preinit but "..state)
   end

   for n,v in pairs(ctab) do
      if M.block_config_get(b, n) == nil then
	 deferred[n] = v
      else
	 M.set_config(b, n, v)
      end
   end

   local ret = M.block_init(b)
   if ret ~= 0 then
      error(fmt("do_configure: failed to initalize %s", ffi.string(b.name)))
   end

   for n,v in pairs(deferred) do
      if M.block_config_get(b, n) == nil then
	 error(fmt("do_configure: block %s has no config %s",
		   ffi.string(b.name), n))
      end
      M.set_config(b, n, v)
   end
end

-- safely load a table from a string
-- @param str table string to load
-- @return true or false
-- @return table or error message
local function load_tabstr(str)
   local tab, msg = load("return "..str, nil, 't', {})
   if not tab then return false, msg end
   return tab()
end

--- Load a configuration string.
-- This could be a table, a number or just a string.
-- @param str config string
-- @return true or false
-- @return value or error message
local function load_confstr(str)
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
-- Parses `strval` as a table (`{...}`), number, or plain string.
-- For `char`-typed configs the string is assigned directly.
-- @param b `ubx_block_t`
-- @param name config name string
-- @param strval string value
function M.set_config_str(b, name, strval)
   local c = ubx.ubx_config_get(b, name)
   if c == nil then error("set_config_str: unknown config '"..name.."'") end

   if c.value.type.type_class==ubx.TYPE_CLASS_BASIC and safe_tostr(c.value.type.name)=='char' then
      return M.set_config(b, name, strval)
   end
   return M.set_config(b, name, load_confstr(strval))
end

-- add Lua OO methods
local ubx_block_mt = {
   __tostring = M.block_tostr,
   __index = {
      get_name = function (b) return ffi.string(b.name) end,
      get_meta = function (b) return safe_tostr(b.meta_data) end,
      get_prototype = function (b) return M.block_prototype(b) end,
      get_block_state = function (b) return M.block_state_tostr[b.block_state] end,
      get_block_type = function (b) return M.block_type_tostr[b.type] end,

      pp = M.block_pp,
      p = M.block_port_get,
      port_get = M.block_port_get,
      port_add = ubx.ubx_port_add,
      inport_add = ubx.ubx_inport_add,
      outport_add = ubx.ubx_outport_add,
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

      is_proto = M.is_proto,
      is_instance = M.is_instance,
      is_cblock = M.is_cblock,
      is_iblock = M.is_iblock,
      is_cblock_instance = M.is_cblock_instance,
      is_iblock_instance = M.is_iblock_instance,
      is_cblock_proto = M.is_cblock_proto,
      is_iblock_proto = M.is_iblock_proto,
      has_attr = M.block_hasattr,
      is_active = M.block_isactive,
      is_trigger = M.block_istrigger,
      is_realtime = M.block_isrealtime,
   }
}
ffi.metatype("struct ubx_block", ubx_block_mt)

--                           Data type handling

-- @section Data

--- Return the total byte size of a `ubx_data_t` (`d.len * type.size`).
-- @param d `ubx_data_t`
-- @return total size in bytes
function M.data_size(d)
   return tonumber(ubx.data_size(d))
end

--- Get the size in bytes of a single instance of the given type.
-- @param nd `ubx_node_t`
-- @param type_name type name string (e.g. `"struct my_type"`)
-- @return size in bytes
function M.type_size(nd, type_name)
   local t = M.type_get(nd, type_name)
   if t==nil then error("unknown type "..tostring(type_name)) end
   return tonumber(t.size)
end

--- Allocate a new `ubx_data_t` (automatically gc'ed).
-- @param typ `ubx_type_t` of data to allocate
-- @param num *optional* array length (default `1`)
-- @return `ubx_data_t`
function M.__data_alloc(typ, num)
   num = num or 1
   local d = ubx.__ubx_data_alloc(typ, num)
   if d==nil then
      error("data_alloc: unknown type '"..safe_tostr(typ.name).."'")
   end
   ffi.gc(d, function(dat) ubx.ubx_data_free(dat) end)
   return d
end

--- Allocate a new `ubx_data_t` by type name (automatically gc'ed).
-- @param nd `ubx_node_t`
-- @param type_name type name string
-- @param num *optional* array length (default `1`)
-- @return `ubx_data_t`
function M.data_alloc(nd, type_name, num)
   local t = M.type_get(nd, type_name)
   if t==nil then
      error(ffi.string(nd.name)..": data_alloc: unknown type '"..safe_tostr(type_name).."'")
   end
   return M.__data_alloc(t, num)
end

--- Load the registered C types into the luajit ffi.
-- @param nd node_info_t*
function M.ffi_load_types(nd)

   local function ffi_struct_type_is_loaded(t)
      return pcall(ffi.typeof, ffi.string(t.name))
   end

   local loaded_hexarrs = {}

   local function ffi_load_no_ns(t)
      if t.type_class==ubx.TYPE_CLASS_STRUCT and t.private_data~=nil then
	 -- Dedup by hexarr pointer: multiple types may share one header/hexarr.
	 -- Using a name-based check here would silently skip a conflicting type
	 -- from a different module with the same struct name; pointer-based dedup
	 -- only suppresses the exact same hexarr and lets ffi.cdef raise loudly
	 -- on any genuine name collision from a different source.
	 local ptr = tostring(ffi.cast("uintptr_t", t.private_data))
	 if loaded_hexarrs[ptr] then return end
	 loaded_hexarrs[ptr] = true
	 local struct_str = preproc(ffi.string(t.private_data))
	 local ret, err = pcall(ffi.cdef, struct_str)
	 if ret==false then
	    error(fmt("loading type %s: %s", safe_tostr(t.name), err))
	 end
      end
   end

   local typ_list = {}
   M.types_foreach(nd,
		   function (typ) typ_list[#typ_list+1] = typ end,
		   function(t) return
			 t.type_class==ffi.C.TYPE_CLASS_STRUCT and
			 (not ffi_struct_type_is_loaded(t))
		   end)

   table.sort(typ_list, function (tr1,tr2) return tr1.seqid<tr2.seqid end)
   utils.foreach(ffi_load_no_ns, typ_list)
end


--- Convert a `ubx_data_t` to a plain Lua value.
-- Returns `nil` for null data. Char arrays are returned as strings.
-- @param d `ubx_data_t`
-- @return Lua value (number, string, or table)
function M.data_tolua(d)
   if d==nil then error("ubx_data_t argument is nil") end
   if M.data_isnull(d) then return nil end

   if not(d.type.type_class==ubx.TYPE_CLASS_BASIC or
	  d.type.type_class==ubx.TYPE_CLASS_STRUCT) then
      error("can currently only print TYPE_CLASS_BASIC or TYPE_CLASS_STRUCT types")
   end

   local res
   local len=tonumber(d.len)

   -- detect char arrays
   if d.type.type_class==ubx.TYPE_CLASS_BASIC and len>1 and safe_tostr(d.type.name)=='char' then
      res=safe_tostr(d.data)
   else
      local ptrname = ffi.string(d.type.name).."*"
      local dptr = ffi.new(ptrname, d.data)

      if len>1 then
	 res = {}
	 for i=0,len-1 do res[i+1]=cdata.tolua(dptr[i]) end
      else res=cdata.tolua(dptr) end
   end
   return res
end

--- Convert a `ubx_data_t` to a string representation.
-- @param d `ubx_data_t`
-- @return string
function M.data_tostr(d)
   return utils.tab2str(M.data_tolua(d))
end

--- Check if a ubx_data_t is null
-- @param d ubx_data_t
-- @return true or false
function M.data_isnull(d)
   assert(M.is_data(d))
   return d.len==0
end

-- Convert an ubx_type_t to a FFI ctype string.
-- Only works for TYPE_CLASS_BASIC and TYPE_CLASS_STRUCT
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

--- Convert a `ubx_type_t` to a LuaJIT FFI ctype.
-- Only supports `TYPE_CLASS_BASIC` and `TYPE_CLASS_STRUCT`.
-- @param t `ubx_type_t`
-- @param ptr *optional* if `true`, create a pointer type
-- @param fixed_len *optional* array length (e.g. for `char (*)[10]`)
-- @return ffi ctype
function M.type_to_ctype(t, ptr, fixed_len)
   local ctstr=type_to_ctype_str(t, ptr, fixed_len)
   return ffi.typeof(ctstr)
end

--- Derive a LuaJIT FFI ctype from a `ubx_data_t`.
-- @param d `ubx_data_t`
-- @param uselen *optional* if `true`, incorporate `d.len` as array dimension
-- @return ffi ctype
function M.data_to_ctype(d, uselen)
   if uselen then
      return M.type_to_ctype(d.type, true, tonumber(d.len))
   end
   return M.type_to_ctype(d.type, true)
end

--- Cast a `ubx_data_t` value pointer to a typed LuaJIT FFI cdata.
-- @param d `ubx_data_t`
-- @param uselen *optional* if `true`, incorporate `d.len` as array dimension
-- @return ffi cdata pointer to the underlying data
function M.data_to_cdata(d, uselen)
   local ctp
   if uselen then
      ctp = M.type_to_ctype(d.type, true, tonumber(d.len))
   else
      ctp = M.type_to_ctype(d.type, true)
   end
   return ffi.cast(ctp, d.data)
end

--- Resize a `ubx_data_t` to a new array length.
-- **Note:** any cdata pointer obtained before this call may be invalidated.
-- @param d `ubx_data_t`
-- @param newlen new array length
-- @return `true` on success, `false` otherwise
function M.data_resize(d, newlen)
   if ubx.ubx_data_resize(d, newlen) == 0 then return true
   else return false end
end

--- Assign a value to a `ubx_data_t`.
-- Accepts Lua tables, strings, and numbers; follows LuaJIT FFI init rules.
-- @param d `ubx_data_t`
-- @param val value to assign
-- @param resize *optional* if `true`, resize the buffer to fit `val`
-- @return cdata pointer to the (possibly reallocated) buffer
function M.data_set(d, val, resize)

   -- find cdata of the target ubx_data
   local d_cdata = M.data_to_cdata(d)
   local val_type=type(val)

   if val_type=='table' then
      for k,v in pairs(val) do
	 if type(k)~='number' then
	    if d.len < 1 then
	       if resize then
		  M.data_resize(d, 1)
		  d_cdata = M.data_to_cdata(d)
	       else
		  error("data_set: can't assign to null ubx_data_t")
	       end
	    end
	    d_cdata[k]=v
	 else
	    local idx -- starting from zero
	    if val[0] == nil then idx=k-1 else idx=k end
	    if idx >= d.len and not resize then
	       error("data_set: attempt to index beyond bounds, index="..tostring(idx)..", len="..tostring(d.len)..". use resize=true?")
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
      ffi.copy(d_cdata, val)
   elseif val_type == 'number' then
      if d.len ~= 1 then
	 if resize then
	    M.data_resize(d, 1)
	    d_cdata = M.data_to_cdata(d)
	 else
	    error("data_set: can't assign scalar number to array of len "..
		     tostring(d.len).. ". set resize flag?"..tostring(resize))
	 end
      end
      d_cdata[0]=val
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
      isnull = M.data_isnull,
   },
}
ffi.metatype("struct ubx_data", ubx_data_mt)


--- Convert a `ubx_type_t` to a Lua table.
-- @param t `ubx_type_t`
-- @return table `{ name, class, size [, model] }`
function M.ubx_type_totab(t)
   if t==nil then error("NULL type") end
   local res = {}
   res.name=safe_tostr(t.name)
   res.class=M.type_class_tostr[t.type_class]
   res.size=tonumber(t.size)
   if t.type_class==ubx.TYPE_CLASS_STRUCT then res.model=safe_tostr(t.private_data) end
   return res
end

--- Convert a `ubx_type_t` to a human-readable string.
-- @param t `ubx_type_t`
-- @param verb *optional* if `true`, append the full model definition
-- @return string
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
      get_name = function (t) return safe_tostr(t.name) end,
      get_type = function (t) return safe_tostr(t.doc) end,
      totab = M.ubx_type_totab,
      size = function (t) return tonumber(t.size) end,
      ctype = M.type_to_ctype,
   },
}
ffi.metatype("struct ubx_type", ubx_type_mt)


--                           Config handling

-- @section Config

--- Check if a configuration value is null (zero-length `ubx_data_t`).
-- @param c `ubx_config_t`
-- @return boolean
function M.config_isnull(c)
   assert(M.is_config(c))
   return M.data_isnull(c.value)
end

--- Set a configuration value (resizes buffer as needed).
-- @param c `ubx_config_t`
-- @param val value to assign (LuaJIT FFI init rules apply)
function M.config_set(c, val)
   return M.data_set(c.value, val, true)
end

--- Convert a `ubx_config_t` to a Lua table.
-- @param c `ubx_config_t`
-- @return table `{ name, doc, type_name, value }`
function M.config_totab(c)
   if c == nil then return "NULL config" end
   local res = {}
   res.name = ffi.string(c.name)

   res.doc = safe_tostr(c.doc)
   res.type_name = safe_tostr(c.type.name)
   if c.value ~= nil then
      res.value = M.data_tolua(c.value)
   end
   return res
end

--- Convert a config to a human-readable string.
-- @param c ubx_config_t
-- @return string
function M.config_tostr(c) return pt.tostr(M.config_totab(c)) end

-- add Lua OO methods
local ubx_config_mt = {
   __tostring = M.config_tostr,
   __index = {
      get_name = function (c) return ffi.string(c.name) end,
      get_doc = function (c) return safe_tostr(c.doc) end,
      set = M.config_set,
      totab = M.config_totab,
      tolua = function (c) return M.data_tolua(c.value) end,
      data = function (c) return c.value end,
      isnull = M.config_isnull,
   },
}

ffi.metatype("struct ubx_config", ubx_config_mt)


--                              Interactions

-- @section Interaction

--- Read from an interaction block.
-- **Requires** iblock to be in `active` state.
-- @param i `ubx_block_t` (iblock)
-- @param rdat `ubx_data_t` to store the result
-- @return number of items read (0 = no data available)
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

--- Write to an interaction block.
-- **Requires** iblock to be in `active` state.
-- @param i `ubx_block_t` (iblock)
-- @param wdat `ubx_data_t` to write
function M.interaction_write(i, wdat)
   if i.block_state ~= ffi.C.BLOCK_STATE_ACTIVE then
      error("interaction_read: interaction not readable in state "..M.block_state_tostr[i.block_state])
   end
   i.write(i, wdat)
end

--                   Port reading and writing

-- @section Port

--- Allocate a `ubx_data_t` sized for reading from a port.
-- @param p `ubx_port_t` (must be an inport)
-- @return `ubx_data_t`
function M.port_alloc_read_sample(p)
   return M.__data_alloc(p.in_type, p.in_data_len)
end

--- Allocate a `ubx_data_t` sized for writing to a port.
-- @param p `ubx_port_t` (must be an outport)
-- @return `ubx_data_t`
function M.port_alloc_write_sample(p)
   return M.__data_alloc(p.out_type, p.out_data_len)
end

--- Read from a port.
-- @param p `ubx_port_t`
-- @param rval *optional* `ubx_data_t` to store result (allocated if `nil`)
-- @return number of items read (0 = no data, negative = error)
-- @return `ubx_data_t` containing the result
function M.port_read(p, rval)
   assert(p, "invalid port")
   if not M.is_data(rval) then
      rval = M.port_alloc_read_sample(p)
   end
   return ubx.__port_read(p, rval), rval
end

--- Read from a port, blocking up to `timeout` seconds.
-- Polls every 10 ms until data arrives or the timeout expires.
-- @param p `ubx_port_t`
-- @param timeout maximum wait in seconds
-- @param data *optional* `ubx_data_t` to store result (allocated if `nil`)
-- @return number of items read, or **-1** on timeout
-- @return `ubx_data_t` or `nil`
function M.port_read_timed(p, timeout, data)
   timeout = timeout or 0
   local ts_start = ffi.new("struct ubx_timespec")
   local ts_cur = ffi.new("struct ubx_timespec")

   if not M.is_data(data) then
      data = M.port_alloc_read_sample(p)
   end

   M.gettime(ts_start)
   M.gettime(ts_cur)

   while ts_cur.sec - ts_start.sec < timeout do
      local len = ubx.__port_read(p, data)
      if len>0 then return len, data end
      M.nanosleep(0, 10*1000^2)
      M.gettime(ts_cur)
   end
   return -1
end

--- Write to a port.
-- Accepts a `ubx_data_t` directly, or a plain Lua value that is
-- auto-converted via `port_alloc_write_sample` + `data_set`.
-- @param p `ubx_port_t`
-- @param wval `ubx_data_t`, or a Lua table/number/string
function M.port_write(p, wval)
   assert(p, "invalid port")
   if M.is_data(wval) then
      ubx.__port_write(p, wval)
   elseif type(wval) == 'cdata' then
      error("port_write: invalid cdata. expected ubx_data, got "..ffi.typeof(wval))
   else
      local sample = M.port_alloc_write_sample(p)
      sample:set(wval)
      ubx.__port_write(p, sample)
   end
end

--- Write to and then immediately read from a port.
-- @param p `ubx_port_t`
-- @param wdat `ubx_data_t` to write
-- @param rdat *optional* `ubx_data_t` for read result
-- @return number of items read
-- @return `ubx_data_t` containing the result
function M.port_write_read(p, wdat, rdat)
   M.port_write(p, wdat)
   return M.port_read(p, rdat)
end

--- Return the output buffer size in bytes (`out_type.size * out_data_len`).
-- @param p `ubx_port_t` (**must** be an outport)
-- @return size in bytes
function M.port_out_size(p)
   if p==nil then error("port_out_size: port is nil") end
   if not M.is_outport(p) then error("port "..ffi.string(p.name).." is not an outport") end
   return tonumber(p.out_type.size * p.out_data_len)
end

--- Return the input buffer size in bytes (`in_type.size * in_data_len`).
-- @param p `ubx_port_t` (**must** be an inport)
-- @return size in bytes
function M.port_in_size(p)
   if p==nil then error("port_in_size: port is nil") end
   if not M.is_inport(p) then error("port_in_size: port "..ffi.string(p.name).." is not an inport") end
   return tonumber(p.in_type.size * p.in_data_len)
end


--- Return connection names for a port.
-- @param p `ubx_port_t`
-- @return table `{ incoming={...}, outgoing={...} }` with iblock name arrays
function M.port_conns_totab(p)
   local res = { incoming={}, outgoing={} }
   local i

   i = 0
   if p.in_interaction ~= nil then
      while p.in_interaction[i] ~= nil do
	 res.incoming[i+1] = ffi.string(p.in_interaction[i].name)
	 i=i+1
      end
   end

   i = 0
   if p.out_interaction ~= nil then
      while p.out_interaction[i] ~= nil do
	 res.outgoing[i+1] = ffi.string(p.out_interaction[i].name)
	 i=i+1
      end
   end
   return res
end


--- Convert a port to a Lua table.
-- @param p ubx_port_t
-- @return table with name, doc, attrs, type info, and connections
function M.port_totab(p)
   local ptab = {}
   ptab.name = ffi.string(p.name)
   ptab.doc = safe_tostr(p.doc)
   ptab.attrs = tonumber(p.attrs)
   if M.is_inport(p) then
      ptab.in_type_name = safe_tostr(p.in_type.name)
      ptab.in_data_len = tonumber(p.in_data_len)
   end
   if M.is_outport(p) then
      ptab.out_type_name = safe_tostr(p.out_type.name)
      ptab.out_data_len = tonumber(p.out_data_len)
   end
   ptab.connections = M.port_conns_totab(p)
   return ptab
end

--- Convert a port to a human-readable string.
-- @param port ubx_port_t
-- @return string
function M.port_tostr(port) return pt.tostr(M.port_totab(port)) end

-- add Lua OO methods
local ubx_port_mt = {
   __tostring = M.port_tostr,
   __len = function (p) return tonumber(p.in_data_len), tonumber(p.out_data_len) end,
   __index = {
      get_name = function (p) return ffi.string(p.name) end,
      get_doc = function (p) return safe_tostr(p.doc) end,
      totab = M.port_totab,
      out_size = M.port_out_size,
      in_size = M.port_in_size,
      write = M.port_write,
      read = M.port_read,
      write_read = M.port_write_read,
      read_timed = M.port_read_timed,
      is_inport = M.is_inport,
      is_outport = M.is_outport,
      is_inoutport = M.is_inoutport,
   },
}
ffi.metatype("struct ubx_port", ubx_port_mt)


--                   Useful stuff: foreach, pretty printing

-- @section Iterators

--- Call a function on every known type.
-- @param nd ubx_node_t
-- @param fun function to call on each ubx_type_t
-- @param pred optional predicate to filter types
function M.types_foreach(nd, fun, pred)
   if not fun then error("types_foreach: missing/invalid fun argument") end
   if nd.types==nil then return end
   pred = pred or function() return true end
   local ubx_type_t_ptr = ffi.typeof("ubx_type_t*")
   local typ=nd.types
   while typ ~= nil do
      if pred(typ) then fun(typ) end
      typ=ffi.cast(ubx_type_t_ptr, typ.hh.next)
   end
end


--- Call a function on every block and collect results.
-- @param nd ubx_node_t
-- @param fun function to call on each ubx_block_t
-- @param pred optional predicate to filter blocks
-- @return table of results
function M.blocks_map(nd, fun, pred)
   local res = {}
   if nd==nil then return end
   pred = pred or function() return true end
   local ubx_block_t_ptr = ffi.typeof("ubx_block_t*")
   local b=nd.blocks
   while b ~= nil do
      if pred(b) then res[#res+1]=fun(b) end
      b=ffi.cast(ubx_block_t_ptr, b.hh.next)
   end
   return res
end

--- Apply a function to each module of a node.
-- @param nd node
-- @param fun function to apply to each module
-- @param pred predicate filter
function M.modules_foreach(nd, fun, pred)
   if nd==nil then return end
   pred = pred or function() return true end
   local ubx_module_t_ptr = ffi.typeof("ubx_module_t*")
   local m=nd.modules
   while m ~= nil do
      if pred(m) then fun(m) end
      m=ffi.cast(ubx_module_t_ptr, m.hh.next)
   end
end

--- Call a function on every module and collect results.
-- @param nd ubx_node_t
-- @param fun function to call on each module
-- @param pred optional predicate to filter modules
-- @return table of results
function M.modules_map(nd, fun, pred)
   local res = {}
   M.modules_foreach(nd, function(m) res[#res+1]=fun(m) end, pred)
   return res
end


--- Misc
-- @section Misc

local block_uid_cnt = 0

--- Reset the internal block UID counter to zero.
function M.reset_block_uid()
   block_uid_cnt = 0
end

local __pcc_cnt=0
local function pcc_cnt()
   __pcc_cnt=__pcc_cnt+1
   return __pcc_cnt
end

--- Connection
-- @section Connection

--- Create an inverse clone of a port, connected via a new iblock.
-- The cloned port has swapped in/out types relative to the original.
-- The returned port is **garbage collected** (the iblock(s) are not).
-- @param block `ubx_block_t` owning the port to clone
-- @param pname name of port to clone
-- @param buff_len1 *optional* buffer length in→out direction (default `1`)
-- @param buff_len2 *optional* buffer length out→in direction (default `buff_len1`)
-- @param loglevel_overruns *optional* log level for overrun warnings
-- @param allow_partial *optional* allow-partial flag for the iblock
-- @return new inverse `ubx_port_t`
function M.port_clone_conn(block, pname, buff_len1, buff_len2, loglevel_overruns, allow_partial)

   local ibtype = M.get_default_iblock(block.nd)

   local prot = M.port_get(block, pname)

   local p = ffi.C.malloc(ffi.sizeof("ubx_port_t"))
   if p == nil then error("failed to allocate port") end
   ffi.fill(p, ffi.sizeof("ubx_port_t"))
   p = ffi.cast("ubx_port_t*", p)
   ffi.gc(p, ubx.ubx_port_free)

   local pn = ffi.string(prot.name)..'_inv'

   ffi.copy(ffi.cast("char*", p.name), pn, #pn + 1)

   p.out_type = prot.in_type
   p.in_type = prot.out_type

   p.in_data_len = prot.out_data_len
   p.out_data_len = prot.in_data_len

   if M.is_inport(p) then
      if p.in_data_len == 0 then p.in_data_len = 1 end
   end

   if M.is_outport(p) then
      if p.out_data_len == 0 then p.out_data_len = 1 end
   end

   buff_len1 = buff_len1 or 1

   if p.in_type and p.out_type then
      buff_len2 = buff_len2 or buff_len1
   end

   loglevel_overruns = loglevel_overruns or ffi.C.UBX_LOGLEVEL_INFO

   -- New port is an out-port?
   local i_p_to_prot
   if p.out_type~=nil then
      local iname = fmt("PCC%d->%s.%s", pcc_cnt(), ffi.string(block.name), pname)

      i_p_to_prot = M.block_create(block.nd, ibtype, iname,
				   {
				      buffer_len = buff_len1,
				      type_name = ffi.string(p.out_type.name),
				      data_len = tonumber(p.out_data_len),
				      loglevel_overruns = loglevel_overruns,
				      allow_partial = allow_partial or 0,
				   }
      )

      M.block_init(i_p_to_prot)

      if M.ports_connect(p, prot, i_p_to_prot) ~= 0 then
	 error("failed to connect port "..ffi.string(p.name))
      end
      M.block_start(i_p_to_prot)
      info(block.nd, "lua", fmt("port_clone_conn: %s, buffer_len: %d, data_len: %d",
				iname, buff_len1, tonumber(p.out_data_len)))
   end

   local i_prot_to_p

   if p.in_type ~= nil then -- new port is an in-port?
      local iname = fmt("PCC%d<-%s.%s", pcc_cnt(), ffi.string(block.name), pname)

      i_prot_to_p = M.block_create(block.nd, ibtype, iname,
				   { buffer_len = buff_len2,
				     type_name = ffi.string(p.in_type.name),
				     data_len = tonumber(p.in_data_len),
				     loglevel_overruns = loglevel_overruns,
				     allow_partial = allow_partial or 0,
				   }
      )

      M.block_init(i_prot_to_p)

      if M.ports_connect(prot, p, i_prot_to_p) ~= 0 then
	 if i_p_to_prot then
	    M.ports_disconnect(p, prot, i_p_to_prot)
	    M.block_unload(block.nd, ffi.string(i_p_to_prot.name))
	 end
	 error("failed to connect port "..ffi.string(p.name))
      end
      M.block_start(i_prot_to_p)
      info(block.nd, "lua", fmt("port_clone_conn: %s, buffer_len: %d, data_len: %d",
				iname, buff_len2, tonumber(p.in_data_len)))
   end

   return p
end

local function gen_block_uid()
   block_uid_cnt = block_uid_cnt+1
   return fmt("i_%08x", block_uid_cnt)
end

--- Universal connect function — three connection modes:
--
-- **1. port → port:** both `srcbn`/`srcpn` and `tgtbn`/`tgtpn` name existing
-- blocks and ports; a new iblock of `ibtype` is created and configured.
--
-- **2. port ↔ existing iblock:** one side names a block.port, the other
-- names an existing iblock. `ibtype` and `ibconfig` must be `nil`.
--
-- **3. port → new iblock (or new iblock → port):** one side is `nil`;
-- a new iblock of `ibtype` is created and connected to the named port.
--
-- *Special cases:* for modes 1 and 3, `type_name`, `data_len`, and
-- `buffer_len` are inferred from the port unless overridden in `ibconfig`.
-- For mode 3, `mq_id` defaults to a name derived from the peer port.
--
-- @param nd `ubx_node_t`
-- @param srcbn source block name (or `nil` for a new src iblock)
-- @param srcpn source **out**-port name (or `nil`)
-- @param tgtbn target block name (or `nil` for a new tgt iblock)
-- @param tgtpn target **in**-port name (or `nil`)
-- @param ibtype *optional* iblock prototype name (defaults to `lfds_cyclic`/`lfrb`)
-- @param ibconfig *optional* iblock configuration table
-- @return `true` on success, `false` on failure
-- @return error message string on failure
--
function M.connect(nd, srcbn, srcpn, tgtbn, tgtpn, ibtype, ibconfig)
   local tgtb, srcb
   local tgtp, srcp
   local ibproto

   -- check: invalid if block name is nil but port isn't
   if srcbn == nil and srcpn ~= nil then
      return false, "src block is nil but src port is not"
   end

   if tgtbn == nil and tgtpn ~= nil then
      return false, "tgt block is nil but tgt port is not"
   end

   -- check case non-existing iblock src/tgt
   if srcbn == nil or tgtbn == nil then
      if not ibtype then
	 return false, "empty src or tgt requires type"
      end

      -- check: ibtype must be a prototype
      ibproto = ubx.ubx_block_get(nd, ibtype)
      if ibproto ~= nil and not M.is_iblock_proto(ibproto) then
	 return false, fmt("type %s is not an iblock prototype", ibtype)
      end
   end

   -- get blocks and check they exist
   if srcbn then
      srcb = ubx.ubx_block_get(nd, srcbn)
      if srcb == nil then return false, fmt("no src block %s") end
   end

   if tgtbn then
      tgtb = ubx.ubx_block_get(nd, tgtbn)
      if tgtb == nil then return false, fmt("no tgt block %s") end
   end

   -- check: one of src and target must exist
   if srcb == nil and tgtb == nil then
      return false, fmt("both src %s and tgt %s blocks don't exist", srcbn, tgtbn)
   end

   -- get src port and check that src port exists and is an outport
   if srcb ~= nil and srcpn then
      srcp = ubx.ubx_port_get(srcb, srcpn)
      if srcp == nil then
	 return false, fmt("block %s has no port %s", srcbn, srcpn)
      end
      if not M.is_outport(srcp) then
	 return false, fmt("block %s port %s is not an output port", srcbn, srcpn)
      end
   end

   -- get tgt port and check that tgt port exists and is an inport
   if tgtb ~= nil and tgtpn then
      tgtp = ubx.ubx_port_get(tgtb, tgtpn)
      if tgtp == nil then
	 return false, fmt("block %s has no port %s", tgtbn, tgtpn)
      end
      if not M.is_inport(tgtp) then
	 return false, fmt("block %s port %s is not an input port", tgtbn, tgtpn)
      end
   end

   -- check: warn about a config table when it's not used
   if M.is_iblock_instance(srcb) then
      if ibconfig then
	 warn(nd, "connect", fmt("%s -> %s.%s: ignoring config %s",
				 srcbn, tgtbn, tgtpn, utils.tab2str(ibconfig)))
      end
      if ibtype then
	 warn(nd, "connect", fmt("%s -> %s.%s: ignoring type %s",
				 srcbn, tgtbn, tgtpn, ibtype))
      end
   end

   if M.is_iblock_instance(tgtb) then
      if ibconfig then
	 warn(nd, "connect", fmt("%s.%s -> %s: ignoring config %s",
				 srcbn, srcpn, tgtbn, utils.tab2str(ibconfig)))
      end
      if ibtype then
	 warn(nd, "connect", fmt("%s -> %s.%s: ignoring type %s",
				 srcbn, tgtpn, tgtbn, ibtype))
      end
   end

   -- check: for port-port connections, types and dimensions must match
   if tgtp ~= nil and srcp ~= nil then
      if srcp.out_type ~= tgtp.in_type then
	 return false, fmt("port type mismatch:	%s.%s is %s, %s.%s is %s",
			   srcbn, srcpn, safe_tostr(srcp.out_type.name),
			   tgtbn, tgtpn, safe_tostr(tgtp.in_type.name))
      end

      if srcp.out_data_len ~= tgtp.in_data_len then
	 return false, fmt("port length mismatch: %s.%s is %u, %s.%s is %u",
			   srcbn, srcpn, tonumber(srcp.out_data_len),
			   tgtbn, tgtpn, tonumber(tgtp.in_data_len))
      end
   end

   ibconfig = ibconfig or {}

   -- creating src iblock
   if srcb == nil or tgtb == nil then
      -- extend the iblock config with the given value if a) the
      -- config exists and b) the config is not set by the user
      local function append_ibconfig(cfg, val)
	 if ubx.ubx_config_get(ibproto, cfg) ~= nil then
	    ibconfig[cfg] = ibconfig[cfg] or val
	 end
      end
      local function make_mqname(bn, pn)
	 return string.gsub(fmt("%s.%s", bn, pn), "%/", "_")
      end

      if srcb == nil then
	 srcbn = gen_block_uid()
	 append_ibconfig('type_name', safe_tostr(tgtp.in_type.name))
	 append_ibconfig('data_len', tonumber(tgtp.in_data_len))
	 append_ibconfig('buffer_len', 8)
	 append_ibconfig('mq_id', make_mqname(tgtbn, tgtpn))

	 info(nd, "connect", fmt("creating connection src %s [%s]: %s",
				 tgtbn, ibtype, utils.tab2str(ibconfig)))

	 srcb = M.block_create(nd, ibtype, srcbn, ibconfig)
	 M.block_init(srcb)
      end

      -- create tgt iblock
      if tgtb == nil then
	 tgtbn = gen_block_uid()
	 append_ibconfig('type_name', safe_tostr(srcp.out_type.name))
	 append_ibconfig('data_len', tonumber(srcp.out_data_len))
	 append_ibconfig('buffer_len', 8)
	 append_ibconfig('mq_id', make_mqname(srcbn, srcpn))

	 info(nd, "connect", fmt("creating connection tgt %s [%s]: %s",
				 tgtbn, ibtype, utils.tab2str(ibconfig)))
	 tgtb = M.block_create(nd, ibtype, tgtbn, ibconfig)
	 M.block_init(tgtb)
      end
   end

   -- connect!
   if srcp and tgtp then
      -- block.port -> block.port
      ibtype = ibtype or M.get_default_iblock(nd)
      ibconfig.data_len = ibconfig.data_len or tonumber(srcp.out_data_len)
      ibconfig.type_name = ibconfig.type_name or safe_tostr(srcp.out_type.name)

      local ibname = gen_block_uid()
      local ib = M.block_create(nd, ibtype, ibname, ibconfig)
      M.block_init(ib)

      if ubx.ubx_ports_connect(srcp, tgtp, ib) ~= 0 then
	 return false, fmt("failed to connect %s.%s -> %s.%s",
			   srcbn, srcpn, tgtbn, tgtpn)
      end

      M.block_tostate(ib, 'active')

      info(nd, "connect", fmt("%s.%s -[%s,%s,%d]-> %s.%s [%s]",
			      srcbn, srcpn,
			      ibname, ibconfig.type_name, ibconfig.data_len,
			      tgtbn, tgtpn, ibtype))

   elseif srcp and not tgtp then
      -- block.port -> iblock
      if ubx.ubx_port_connect_out(srcp, tgtb) ~= 0 then
	 return false, fmt("failed to connect %s.p to iblock %s", srcbn, srcpn, tgtbn)
      end
      M.block_tostate(tgtb, 'active')
      info(nd, "connect", fmt("%s.%s -> %s [%s]",
			      srcbn, srcpn, tgtbn, M.block_prototype(tgtb)))
   elseif not srcp and tgtp then
      -- iblock -> block.port
      if ubx.ubx_port_connect_in(tgtp, srcb) ~= 0 then
	 return false, fmt("failed to connect iblock %s to %s.%s", srcbn, tgtbn, tgtpn)
      end
      M.block_tostate(srcb, 'active')
      info(nd, "connect", fmt("%s [%s] -> %s.%s",
			      srcbn, M.block_prototype(srcb), tgtbn, tgtpn))
   else
      return false, fmt("connect: invalid args: %s.%s -> %s.%s, ibtype %s, config %s",
			srcbn, srcpn, tgtbn, tgtpn, ibtype, utils.tab2str(ibconfig))
   end

   return true
end

--- Build a table of all cblock connections.
-- @param nd `ubx_node_t`
-- @return table `{ [bname]={ [pname]={ incoming={...}, outgoing={...} } } }`
function M.build_conntab(nd)
   local res = {}

   local function block_conns_totab(b)
      local function port_conns_totab(p)
	 return	{ [ffi.string(p.name)] = M.port_conns_totab(p) }
      end
      res[ffi.string(b.name)] = M.ports_map(b, port_conns_totab)
   end

   M.blocks_map(nd, block_conns_totab, M.is_cblock_instance)
   return res
end

return M
