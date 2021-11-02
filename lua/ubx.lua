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
local ac = require("ansicolors")
local time = require("time")
require "strict"

local ts = tostring
local concat = table.concat
local fmt = string.format

local M = {}

--- set to false to disable terminal colors
M.color = false

local function red(str, bright) if M.color then str = ac.red(str); if bright then str=ac.bright(str) end end return str end
local function blue(str, bright) if M.color then str = ac.blue(str); if bright then str=ac.bright(str) end end return str end
local function cyan(str, bright) if M.color then str = ac.cyan(str); if bright then str=ac.bright(str) end end return str end
local function white(str, bright) if M.color then str = ac.white(str); if bright then str=ac.bright(str) end end return str end
local function green(str, bright) if M.color then str = ac.green(str); if bright then str=ac.bright(str) end end return str end
local function yellow(str, bright) if M.color then str = ac.yellow(str); if bright then str=ac.bright(str) end end return str end
local function magenta(str, bright) if M.color then str = ac.magenta(str); if bright then str=ac.bright(str) end end return str end

M.red=red; M.blue=blue; M.cyan=cyan; M.white=white; M.green=green; M.yellow=yellow; M.magenta=magenta;

------------------------------------------------------------------------------
--                           helpers
------------------------------------------------------------------------------

--- preprocess a C string
-- currently this means stripping out all preprocessor directives
-- @param str string to process
-- @return string
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

--- Return ubx load prefix information
-- @return prefix from which the ubx_core was loaded
-- @return prefixes table of prefixes used to load blocks
function M.get_prefix() return core_prefix, prefixes end

--- Safely convert a char* to a lua string.
-- @param charptr C style string char* or char[]
-- @param return lua string
function M.safe_tostr(charptr)
   if charptr == nil then return "" end
   return ffi.string(charptr)
end

-- basic predicates
function M.is_node(x) return ffi.istype("ubx_node_t", x) end
function M.is_block(x) return ffi.istype("ubx_block_t", x) end
function M.is_config(x) return ffi.istype("ubx_config_t", x) end
function M.is_port(x) return ffi.istype("ubx_port_t", x) end
function M.is_data(x) return ffi.istype("ubx_data_t", x) end

function M.is_proto(b) assert(M.is_block(b)); return b.prototype == nil end
function M.is_instance(b) return not M.is_proto(b) end
function M.is_cblock(b) return M.is_block(b) and b.type==ffi.C.BLOCK_TYPE_COMPUTATION end
function M.is_iblock(b) return M.is_block(b) and b.type==ffi.C.BLOCK_TYPE_INTERACTION end
function M.is_cblock_instance(b) return M.is_cblock(b) and not M.is_proto(b) end
function M.is_iblock_instance(b) return M.is_iblock(b) and not M.is_proto(b) end
function M.is_cblock_proto(b) return M.is_cblock(b) and M.is_proto(b) end
function M.is_iblock_proto(b) return M.is_iblock(b) and M.is_proto(b) end

-- Port predicates
function M.is_outport(p) assert(M.is_port(p)); return p.out_type ~= nil end
function M.is_inport(p) assert(M.is_port(p)); return p.in_type ~= nil end
function M.is_inoutport(p) return M.is_outport(p) and M.is_inport(p) end

------------------------------------------------------------------------------
--                           LOGGING API
------------------------------------------------------------------------------

--- log a message via rtlog
-- @param level
-- @node node handle
-- @src source of log message
-- @str string to log
local function log(level, node, src, str)
   if level <= node.loglevel then
      ubx.__ubx_log(level, node, src, str)
   end
end

local function emerg(node, src, str) M.log(ffi.C.UBX_LOGLEVEL_EMERG, node, src, str) end
local function alert(node, src, str) M.log(ffi.C.UBX_LOGLEVEL_ALERT, node, src, str) end
local function crit(node, src, str) M.log(ffi.C.UBX_LOGLEVEL_CRIT, node, src, str) end
local function err(node, src, str) M.log(ffi.C.UBX_LOGLEVEL_ERR, node, src, str) end
local function warn(node, src, str) M.log(ffi.C.UBX_LOGLEVEL_WARN, node, src, str) end
local function notice(node, src, str) M.log(ffi.C.UBX_LOGLEVEL_NOTICE, node, src, str) end
local function info(node, src, str) M.log(ffi.C.UBX_LOGLEVEL_INFO, node, src, str) end
local function dbg(node, src, str) M.log(ffi.C.UBX_LOGLEVEL_DEBUG, node, src, str) end

M.log = log
M.emerg = emerg
M.alert = alert
M.crit = crit
M.err = err
M.warn = warn
M.notice = notice
M.info = info
M.debug = dbg


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

--- node_rm wrapper for manually removing node
-- Remove the gc finalizer and call ubx_node_rm
-- the ffi.new allocate ubx_node_t will still be automatically gc'ed
-- @param nd
function M.node_rm(nd)
   ffi.gc(nd, nil)
   ubx.ubx_node_rm(nd)
end

--- Create and initalize a new node_info struct
-- gc via ubx_node_rm and ffi.new set finalizer
-- @param name name of node
-- @param loglevel desired default loglevel
-- @param attrs node attributes
-- @return ubx_node_t
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
-- @param nd node_info pointer into which to load module
-- @param libfile module file to load
function M.load_module(nd, libfile)
   local ver = string.sub(M.safe_tostr(ubx.ubx_version()), 1, 3)
   local modfile = "/lib/ubx/"..ver.."/"..libfile

   for _,pf in ipairs(prefixes) do
      local modpath = pf..modfile
      if string.sub(modpath, -3) ~= '.so' then modpath = modpath .. ".so" end

      if utils.file_exists(modpath) then
	 local res = ubx.ubx_module_load(nd, modpath)
	 if res ~= 0 then
	    error(red("loading module ", true)..magenta(modpath)..red(" failed", true))
	 end
	 info(nd, "lua", "loaded module "..modpath)
	 M.ffi_load_types(nd)
	 return modpath
      end
   end
   error(red("no module ", true) .. magenta(modfile)..
	    red(" found under prefixes ", true) .. magenta(concat(prefixes, ', ')))
end

--- Node to tab
-- @param nd
-- @return table with node information
function M.node_totab(nd)
   local types = {}
   local blocks = {}

   M.types_foreach(nd,
		   function (_t)
		      local t = M.ubx_type_totab(_t)
		      types[t.name] = t end)

   M.blocks_map(nd,
		function (_b)
		   local b = M.block_totab(_b)
		   blocks[b.name] = b end)

   return { types=types, blocks=blocks }
end


--- Cleanup a node: cleanup and remove instances and unload modules.
-- @param nd node info
function M.node_cleanup(nd)
   ubx.ubx_node_cleanup(nd)
   collectgarbage("collect")
end

--- Create a new computational block.
-- @param nd node_info ptr
-- @param type of block to create
-- @param name name of block
-- @return new computational block
function M.block_create(nd, type, name, conf)
   local b=ubx.ubx_block_create(nd, type, name)
   if b==nil then error("failed to create block "..ts(name).." of type "..ts(type)) end
   if conf then M.set_config_tab(b, conf) end
   return b
end

-- OS stuff
function M.clock_mono_sleep(sec, nsec)
   local ts = ffi.new("struct ubx_timespec")
   ts.sec=sec
   ts.nsec=nsec or 0
   M.clock_mono_nanosleep(0, ts)
end

function M.block_get(nd, bname)
   local b = ubx.ubx_block_get(nd, bname)
   if b==nil then error("block_get: no block with name '"..ts(bname).."'") end
   return b
end

function M.block_prototype(b)
   if b.prototype == nil then return false end
   return M.safe_tostr(b.prototype.name)
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

--- Unload a block: bring it to state preinit and call ubx_block_rm
function M.block_unload(nd, name)
   local b = M.block_get(nd, name)
   M.block_tostate(b, 'preinit')
   if M.block_rm(nd, name) ~= 0 then error("block_unload: ubx_block_rm failed for '"..name.."'") end
end

--- Determine the number of blocks
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

function M.num_types(nd) return ubx.ubx_num_types(nd) end

--- Pretty print a node
-- @param nd node_info
function M.node_pp(nd)
   print(green(M.safe_tostr(nd.name), true))

   print("  modules:", true)
   M.modules_foreach(nd,
		     function (m)
			print("    "..magenta(M.safe_tostr(m.id))..
				 " ["..red(M.safe_tostr(m.spdx_license_id)).."]")
		     end
   )

   print("  types:")
   M.types_foreach(nd,
		   function (t)
		      print("    "..magenta(M.safe_tostr(t.name))..
			       " ["..yellow("size: "..tonumber(t.size))..", "..
			       red(utils.str_to_hexstr(ffi.string(t.hash, 4))).."] "..
			       red(M.safe_tostr(t.doc)))
		   end
   )

   print("  prototypes:")
   M.blocks_map(nd,
		function (b)
		   print("    "..M.block_tostr(b))
		end, M.is_proto)

   print("  iblocks:")
   M.blocks_map(nd,
		function (b)
		   print("    "..M.block_tostr(b))
		end, M.is_iblock_instance)

   print("  cblocks:")
   M.blocks_map(nd,
		function (b)
		   print("    "..M.block_tostr(b))
		end, M.is_cblock_instance)

end

-- add Lua OO methods
local ubx_node_mt = {
   __tostring = function(nd)
      local num_cb, num_ib, inv = M.num_blocks(nd)
      local num_types = M.num_types(nd)

      return fmt("%s <node>: #blocks: %d (#cb: %d, #ib: %d), #types: %d",
		 green(M.safe_tostr(nd.name)), num_cb + num_ib, num_cb, num_ib, num_types)
   end,
   __index = {
      get_name = function (nd) return M.safe_tostr(nd.name) end,
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


------------------------------------------------------------------------------
--                           Block API
------------------------------------------------------------------------------

local function block_state_color(sstr)
   if sstr == 'preinit' then return blue(sstr, true)
   elseif sstr == 'inactive' then return red(sstr)
   elseif sstr == 'active' then return green(sstr, true)
   else
      error(red("unknown state "..ts(sstr)))
   end
end

function M.block_hasattr(b, attr)
   if bit.band(b.attrs, attr) ~= 0 then return true end
   return false
end

function M.block_isactive(b)
   return M.block_hasattr(b, ffi.C.BLOCK_ATTR_ACTIVE)
end

function M.block_istrigger(b)
   return M.block_hasattr(b, ffi.C.BLOCK_ATTR_TRIGGER)
end

function M.block_isrealtime(b)
   return M.block_hasattr(b, ffi.C.BLOCK_ATTR_REALTIME)
end

local function block_attr_totab(b)
   local t = {}
   if M.block_istrigger(b) then t[#t+1] = "trigger" end
   if M.block_isactive(b) then t[#t+1] = "active" end
   return t
end

--- Convert block to lua table
-- @param b block to convert to table
-- @return table
function M.block_totab(b)
   if b==nil then error("NULL block") end

   local res = {}
   res.name = M.safe_tostr(b.name)
   res.attrs = block_attr_totab(b)
   res.meta_data = M.safe_tostr(b.meta_data)
   res.block_type=M.block_type_tostr[b.type]
   res.state = M.block_state_tostr[b.block_state]

   if b.prototype ~= nil then
      res.prototype = M.safe_tostr(b.prototype.name)
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

--- Pretty print a block.
-- @param b block to convert to string.
-- @return string
function M.block_pp(b)
   local res = {}
   local bt

   if M.is_block(b) then bt = M.block_totab(b) else bt = b end

   if bt.block_type == 'cblock' then
      local f = "%s [state: %s, steps: %u] (type: %s, prototype: %s, attrs: %s)"

      res[#res+1] = f:format(green(bt.name),
			     block_state_color(bt.state),
			     bt.stat_num_steps,
			     bt.block_type,
			     blue(bt.prototype),
			     table.concat(bt.attrs, ', '))
   elseif bt.block_type == 'iblock' then
      local f = "%s [state: %s, reads: %u, writes: %u] (type: %s, prototype: %s, attrs: %s)"
      res[#res+1] = f:format(green(bt.name),
			     block_state_color(bt.state),
			     bt.stat_num_reads,
			     bt.stat_num_writes,
			     bt.block_type,
			     blue(bt.prototype),
			     table.concat(bt.attrs, ', '))
   else
      error("unknown block type "..bt.block_type)
   end

   if #bt.configs > 0 then
      res[#res+1] = ("  configs:")
      utils.foreach(function(c) res[#res+1] = "    "..M.config_tabtostr(c) end, bt.configs)
   end

   if #bt.ports > 0 then
      res[#res+1] = ("  ports:")
      utils.foreach(function(p) res[#res+1] = "    "..M.port_tabtostr(p) end, bt.ports)
   end

   return table.concat(res, '\n')
end

--- Convert a block to a string (short form)
-- @param b ubx_block_t
function M.block_tostr(b)
   local bt

   if M.is_block(b) then
      bt = M.block_totab(b)
   else
      bt = b
   end

   return ("%s [%s]"):format(green(bt.name), bt.prototype or "proto")
end

function M.block_port_get (b, n)
   local res = ubx.ubx_port_get(b, n)
   if res==nil then error("port_get: no port with name '"..ts(n).."'") end
   return res
end
M.port_get = M.block_port_get

function M.block_config_get (b, n)
   return ubx.ubx_config_get(b, n)
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
function M.type_size(nd, type_name)
   local t = M.type_get(nd, type_name)
   if t==nil then error("unknown type "..tostring(type_name)) end
   return tonumber(t.size)
end

--- Allocate a new ubx_data with a given dimensionality.
-- This data will be automatically garbage collected.
-- @param ubx_type of data to allocate
-- @param num desired type array length
-- @return ubx_data_t
function M.__data_alloc(typ, num)
   num = num or 1
   local d = ubx.__ubx_data_alloc(typ, num)
   if d==nil then
      error("data_alloc: unknown type '"..M.safe_tostr(typ.name).."'")
   end
   ffi.gc(d, function(dat) ubx.ubx_data_free(dat) end)
   return d
end

--- Allocate a new ubx_data with a given dimensionality.
-- This data will be automatically garbage collected.
-- @param nd node_info
-- @param name type of data to allocate
-- @param num dimensionality
-- @return ubx_data_t
function M.data_alloc(nd, type_name, num)
   local t = M.type_get(nd, type_name)
   if t==nil then
      error(M.safe_tostr(nd.name)..": data_alloc: unknown type '"..M.safe_tostr(type_name).."'")
   end
   return M.__data_alloc(t, num)
end

--- Load the registered C types into the luajit ffi.
-- @param nd node_info_t*
function M.ffi_load_types(nd)

   local function ffi_struct_type_is_loaded(t)
      return pcall(ffi.typeof, ffi.string(t.name))
   end

   local function ffi_load_no_ns(t)
      if t.type_class==ubx.TYPE_CLASS_STRUCT and t.private_data~=nil then
	 local struct_str = preproc(ffi.string(t.private_data))
	 local ret, err = pcall(ffi.cdef, struct_str)
	 if ret==false then
	    error(fmt("loading type %s: %s", M.safe_tostr(t.name), err))
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


--- Convert a ubx_data_t to a plain Lua representation.
-- requires cdata.tolua
-- @param d ubx_data_t type
-- @return Lua data
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

--- Check if a ubx_data_t is null
-- @param d ubx_data_t
-- @return true or false
function M.data_isnull(d)
   assert(M.is_data(d))
   return d.len==0
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
      --ffi.copy(d_cdata, val, #val)
      ffi.copy(d_cdata, val)
   elseif val_type=='number' then
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


--- Convert a ubx_type to a Lua table
function M.ubx_type_totab(t)
   if t==nil then error("NULL type") end
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

--- Check if a configuration value is null
-- @param c config
-- @return true or false
function M.config_isnull(c)
   assert(M.is_config(c))
   return M.data_isnull(c.value)
end

--- Set a configuration value
-- @param c config
-- @param val value to assign (must follow luajit FFI initialization rules)
function M.config_set(c, val)
   return M.data_set(c.value, val, true)
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

--- Configure a block with a table of configuration values.
--
-- This version will initialize the block and configure dynamically
-- added configs that were skipped before in a second run.
-- @param b block
-- @param ctab configuration tab
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
      error(fmt("do_configure: failed to initalize %s", M.safe_tostr(b.name)))
   end

   for n,v in pairs(deferred) do
      if M.block_config_get(b, n) == nil then
	 error(fmt("do_configure: block %s has no config %s",
		   M.safe_tostr(b.name), n))
      end
      M.set_config(b, n, v)
   end
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
   if c == nil then return "NULL config" end
   local res = {}
   res.name = M.safe_tostr(c.name)

   res.doc = M.safe_tostr(c.doc)
   res.type_name = M.safe_tostr(c.type.name)
   if c.value ~= nil then
      res.value = M.data_tolua(c.value)
   end
   return res
end

function M.config_tabtostr(ctab)
   return blue(ctab.name, true)
      .." ["..magenta(ctab.type_name).."] "
      ..yellow(utils.tab2str(ctab.value))
      .." "..red("// "..ctab.doc)
end

function M.config_tostr(c)
   local ctab = M.config_totab(c)
   return M.config_tabtostr(ctab)
end

-- add Lua OO methods
local ubx_config_mt = {
   __tostring = M.config_tostr,
   __index = {
      get_name = function (c) return M.safe_tostr(c.name) end,
      get_doc = function (c) return M.safe_tostr(c.doc) end,
      set = M.config_set,
      totab = M.config_totab,
      tolua = function (c) return M.data_tolua(c.value) end,
      data = function (c) return c.value end,
      isnull = M.config_isnull,
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
   return M.__data_alloc(p.in_type, p.in_data_len)
end

--- Allocate an ubx_data for port writing
-- @param port
-- @return ubx_data_t sample
function M.port_alloc_write_sample(p)
   return M.__data_alloc(p.out_type, p.out_data_len)
end

--- Read from a port.
--
function M.port_read(p, rval)
   assert(p, "invalid port")
   if not M.is_data(rval) then
      rval = M.port_alloc_read_sample(p)
   end
   return ubx.__port_read(p, rval), rval
end

--- Read from port with timeout.
-- @param p port to read from
-- @param timeout duration to block in seconds
-- @param data optional data to store result of read
-- @return num or -1 if timeout
-- @return result or nil
function M.port_read_timed(p, timeout, data)
   timeout = timeout or 0
   local ts_start = ffi.new("struct ubx_timespec")
   local ts_cur = ffi.new("struct ubx_timespec")

   if not M.is_data(data) then
      data = M.port_alloc_read_sample(p)
   end

   M.clock_mono_gettime(ts_start)
   M.clock_mono_gettime(ts_cur)

   while ts_cur.sec - ts_start.sec < timeout do
      local len = ubx.__port_read(p, data)
      if len>0 then return len, data end
      M.clock_mono_sleep(0, 10*1000^2)
      M.clock_mono_gettime(ts_cur)
   end
   return -1
end

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

function M.port_write_read(p, wdat, rdat)
   M.port_write(p, wdat)
   return M.port_read(p, rdat)
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
   if M.is_inport(p) then
      ptab.in_type_name = M.safe_tostr(p.in_type.name)
      ptab.in_data_len = tonumber(p.in_data_len)
   end
   if M.is_outport(p) then
      ptab.out_type_name = M.safe_tostr(p.out_type.name)
      ptab.out_data_len = tonumber(p.out_data_len)
   end
   ptab.connections = M.port_conns_totab(p)
   return ptab
end

function M.port_tabtostr(pt)
   assert(type(pt) == 'table')

   local in_str=""
   local out_str=""
   local doc=""

   if pt.in_type_name then
      in_str = "in: "..magenta(pt.in_type_name)
      if pt.in_data_len > 1 then in_str = in_str.."["..ts(pt.in_data_len).."]" end
      in_str = in_str.." #conn: "..ts(#pt.connections.incoming)
   end

   if pt.out_type_name then
      out_str = "out: "..magenta(pt.out_type_name)
      if pt.out_data_len > 1 then out_str = out_str.."["..ts(pt.out_data_len).."]" end
      out_str = out_str.." #conn: "..ts(#pt.connections.outgoing)
   end

   if pt.doc then doc = red(" // "..pt.doc) end

   return cyan(pt.name, true).." ["..(in_str or "")..(out_str or "").."] "..doc
end

function M.port_tostr(port)
   local p = M.port_totab(port)
   return M.port_tabtostr(p)
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
--                   Useful stuff: foreach, pretty printing
------------------------------------------------------------------------------

--- Call a function on every known type.
-- @param nd ubx_node_t*
-- @param fun function to call on type.
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

--- Call function on all ports of a block and return the result in a table.
-- @param b block
-- @param fun function to call on port
-- @param pred optional predicate function. fun is only called if pred is true.
-- @return result table.
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

--- Call a function on every block of the given list.
-- @param nd node info
-- @param fun function
-- @param pred predicate function to filter
-- @param result table
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

function M.modules_map(nd, fun, pred)
   local res = {}
   local function mod_apply(m) res[#res+1] = fun(m) end
   M.modules_foreach(nd, mod_apply, pred)
   return res
end


--- Convert the current system to a dot-file
-- @param nd
-- @return graphviz dot string
function M.node_todot(nd)

   --- Generate a list of block nodes in graphviz dot syntax
   local function gen_dot_nodes(blocks)
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
   local function gen_dot_trigger_edges(b, res)

      if not (b.prototype ~= nil and
		 (b.prototype.name == "ubx/ptrig" or
		  b.prototype.name == "ubx/trig")) then return end

      local chain0_cfg

      for i,c in ipairs(b.configs) do
	 if c.name=='chain0' then chain0_cfg=c.value end
      end
      assert(chain0_cfg~=nil, "gen_dot_trigger_edges: failed to find chain0 config")

      for i,trig in ipairs(chain0_cfg or {}) do
	 res[#res+1]=utils.expand('    "$from" -> "$to" [label="$label",style=dashed,color=orange1 ];',
				  {from=b.name, to=trig.b, label=ts(i)})
      end
   end

   --- Generate edges
   local function gen_dot_edges(blocks)
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

   local btab = M.blocks_map(nd, M.block_totab, function(b) return not M.is_proto(b) end)
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
   node=M.safe_tostr(nd.name),
   blocks=gen_dot_nodes(btab),
   conns=gen_dot_edges(btab),
    })
end

local __pcc_cnt=0
local function pcc_cnt()
   __pcc_cnt=__pcc_cnt+1
   return __pcc_cnt
end

--
-- port_clone_conn - create a new port connected to an existing port
-- via an lfds_cyclic interaction. The returned port is garbage
-- collected.
--
-- @param bname block
-- @param pname name of port
-- @param buff_len1 buffer length in in->out direction (default 1)
-- @param buff_len2 buffer length in out->in direction (default buff_len1)
-- @param loglevel_overruns loglevel for buffer overruns
-- @param allow_partial allow partial flag (see lfds_cyclic)
-- @return the new, inverse, connected port
function M.port_clone_conn(block, pname, buff_len1, buff_len2, loglevel_overruns, allow_partial)

   local prot = M.port_get(block, pname)

   local p = ffi.C.malloc(ffi.sizeof("ubx_port_t"))
   if p == nil then error("failed to allocate port") end
   ffi.fill(p, ffi.sizeof("ubx_port_t"))
   p = ffi.cast("ubx_port_t*", p)
   ffi.gc(p, ubx.ubx_port_free)

   local pn = M.safe_tostr(prot.name)..'_inv'

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
      local iname = fmt("PCC%d->%s.%s", pcc_cnt(), M.safe_tostr(block.name), pname)

      i_p_to_prot = M.block_create(block.nd, "ubx/lfds_cyclic", iname,
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
	 error("failed to connect port "..M.safe_tostr(p.name))
      end
      M.block_start(i_p_to_prot)
      info(block.nd, "lua", fmt("port_clone_conn: %s, buffer_len: %d, data_len: %d",
				iname, buff_len1, tonumber(p.out_data_len)))
   end

   local i_prot_to_p

   if p.in_type ~= nil then -- new port is an in-port?
      local iname = fmt("PCC%d<-%s.%s", pcc_cnt(), M.safe_tostr(block.name), pname)

      i_prot_to_p = M.block_create(block.nd, "ubx/lfds_cyclic", iname,
				   { buffer_len = buff_len2,
				     type_name = ffi.string(p.in_type.name),
				     data_len = tonumber(p.in_data_len),
				     loglevel_overruns = loglevel_overruns,
				     allow_partial = allow_partial or 0,
				   }
      )

      M.block_init(i_prot_to_p)

      if M.ports_connect(prot, p, i_prot_to_p) ~= 0 then
	 -- TODO disconnect if connected above.
	 error("failed to connect port"..M.safe_tostr(p.name))
      end
      M.block_start(i_prot_to_p)
      info(block.nd, "lua", fmt("port_clone_conn: %s, buffer_len: %d, data_len: %d",
				iname, buff_len2, tonumber(p.in_data_len)))
   end

   return p
end

local block_uid_cnt = 0
local function gen_block_uid()
   block_uid_cnt = block_uid_cnt+1
   return fmt("i_%08x", block_uid_cnt)
end

function M.reset_block_uid()
   block_uid_cnt = 0
end

--- connect - universal connect function
--
-- create connections in different ways. The following cases are supported:
--
-- 1. cblock-cblock: src and tgt are existing cblocks: they will be
-- connected using a new iblock of ibtype configured with config.
--
-- 2. cblock-iblock: if one of src and tgt is a cblock and the other
-- an iblock, then these are connected. ibtype and config must be nil
-- (as the iblock was created elsewhere, it must be configured
-- elsewhere too).
--
-- 3. cblock-iblock: if one of src and tgt is a cblock, and the other
-- is nil, then a new iblock of ibtype will be created and configured
-- with `config` and src or tgt is connected to it.
--
-- Special cases:
--  - for 1:   if ibtype is unset, the it defaults to lfds_cyclic
--  - for 1+3: type_name, data_len and buffer_len are set automatically
--             unless overriden in config.
--  - for 3: if config.mq_id is unset, a default name based on the
--           peer port is chosen.
--
-- @param nd node
-- @param srcbn source block name
-- @param srcpn source (out-) port name
-- @param tgtbn target block name
-- @param tgtpn target (in-) port name
-- @param ibtype iblock type to use for connection
-- @param ibconfig iblock configuration table
-- @return true if OK, false otherwise
-- @return msg in case of error, error message.
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

   -- check: if port name is set, then block must be a cblock
   if srcpn and not M.is_cblock_instance(srcb) then
      return false, "src port name set but src block not a cblock"
   end

   if tgtpn and not M.is_cblock_instance(tgtb) then
      return false, "tgt port name set but tgt block not a cblock"
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
			   srcbn, srcpn, M.safe_tostr(srcp.out_type.name),
			   tgtbn, tgtpn, M.safe_tostr(tgtp.in_type.name))
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
	 append_ibconfig('type_name', M.safe_tostr(tgtp.in_type.name))
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
	 append_ibconfig('type_name', M.safe_tostr(srcp.out_type.name))
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
   if M.is_cblock_instance(srcb) and M.is_cblock_instance(tgtb) then
      -- cblock -> cblock
      ibtype = ibtype or "ubx/lfds_cyclic"
      ibconfig.data_len = ibconfig.data_len or tonumber(srcp.out_data_len)
      ibconfig.type_name = ibconfig.type_name or M.safe_tostr(srcp.out_type.name)

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

   elseif M.is_cblock_instance(srcb) and M.is_iblock_instance(tgtb) then
      -- cblock -> iblock
      if ubx.ubx_port_connect_out(srcp, tgtb) ~= 0 then
	 return false, fmt("failed to connect %s.p to iblock %s", srcbn, srcpn, tgtbn)
      end
      M.block_tostate(tgtb, 'active')
      info(nd, "connect", fmt("%s.%s -> %s [%s]",
			      srcbn, srcpn, tgtbn, M.block_prototype(tgtb)))
   elseif M.is_iblock_instance(srcb) and M.is_cblock_instance(tgtb) then
      -- iblock -> cblock
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

--- Build a table of connections
-- @param nd node info
-- @return connection table
function M.build_conntab(nd)
   local res = {}

   local function block_conns_totab(b)
      local function port_conns_totab(p)
	 return	{ [M.safe_tostr(p.name)] = M.port_conns_totab(p) }
      end
      res[M.safe_tostr(b.name)] = M.ports_map(b, port_conns_totab)
   end

   M.blocks_map(nd, block_conns_totab, M.is_cblock_instance)
   return res
end

return M
