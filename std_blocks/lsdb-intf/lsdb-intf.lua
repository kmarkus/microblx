
local ubx = require("ubx")
local ffi = require("ffi")
local lsdb = require("lsdbus")
local utils = require("utils")
local fmt = string.format

local BUS_RUN_TIMEOUT_USEC = 200000

local SERVICE =	"org.ubx.%s"

--- Methods
local function load_module(vt, module)
   vt.nd:load_module(module)
   vt:emitPropertiesChanged("CBlockTypes", "IBlockTypes")
end

local function create_block(vt,	type, name, conf)
   ubx.block_create(vt.nd, type, name, conf)
end

local function remove_block(vt, name)
   ubx.block_unload(vt.nd, name)
end

local function switch_state(vt, name, state)
   local b = ubx.block_get(vt.nd, name)
   ubx.block_tostate(b, state)
end

local function trigger_blocks(vt, blocks)
   for _,name in ipairs(blocks) do
      local b = ubx.block_get(vt.nd, name)
      b:do_step()
   end
end

local function connect(vt, srcbn, srcpn, tgtbn, tgtpn, ibtype, ibconfig)
   srcbn = srcbn ~= "" and srcbn or nil
   srcpn = srcpn ~= "" and srcpn or nil
   tgtbn = tgtbn ~= "" and tgtbn or nil
   tgtpn = tgtpn ~= "" and tgtpn or nil
   ibtype = ibtype ~= "" and ibtype or nil
   if ibconfig == "" or ibconfig == false then ibconfig = nil end

   assert(ubx.connect(vt.nd, srcbn, srcpn, tgtbn, tgtpn, ibtype, ibconfig))
   vt:emitPropertiesChanged("Connections")
end

-- like ubx_node_clear, but with filters
-- TODO: crude exclusion of lsdb blocks by wildcard
local function clear_node(vt)
   local function filter(b)
      return ubx.is_instance(b) and string.match(b:get_name(), "lsdb") == nil
   end

   ubx.blocks_map(vt.nd, function(b) ubx.block_tostate(b, 'inactive') end, filter)
   ubx.blocks_map(vt.nd, function(b) ubx.block_tostate(b, 'preinit') end, filter)
   ubx.blocks_map(vt.nd, function(b) ubx.block_rm(vt.nd, b:get_name()) end, filter)
end

-- a list of port_clone_conn ports
local wpccs = {}

local function write(vt, bn, pn, val)
   local pcc

   if wpccs[bn] and wpccs[bn][pn] then
      pcc = wpccs[bn][pn]
   else
      local b = ubx.ubx_block_get(vt.nd, bn)
      local p = ubx.block_port_get(b, pn)

      pcc = ubx.port_clone_conn(p)
      wpccs[bn] = wpccs[bn] or {}
      wpccs[bn][pn] = pcc
   end

   ubx.port_write(pcc, val)
end

local rppcs = {}

local function read(vt, bn, pn)
   local pcc

   if rppcs[bn] and rppcs[bn][pn] then
      pcc = rppcs[bn][pn]
   else
      local b = ubx.ubx_block_get(vt.nd, bn)
      local p = ubx.block_port_get(b, pn)

      pcc = ubx.port_clone_conn(p)
      rppcs[bn] = rppcs[bn] or {}
      rppcs[bn][pn] = pcc
   end

   local v =ubx.port_read(pcc)
   return lsdb.tovariant2(v)
end

-- Property getters/setters

local function get_node_name(vt)
   return vt.nd:get_name()
end

local function get_cblock_types (vt)
   return ubx.blocks_map(
      vt.nd,
      function (b) return ubx.safe_tostr(b.name) end,
      ubx.is_cblock_proto)
end

local function get_iblock_types (vt)
   return ubx.blocks_map(
      vt.nd,
      function (b) return ubx.safe_tostr(b.name) end,
      ubx.is_iblock_proto)
end

-- get all cblocks including prototype and state
local function get_cblocks (vt)
   local function get_cblock_info(b)
      return {
	 b:get_name(),
	 b:get_prototype(),
	 b:get_block_state(),
      }
   end
   return ubx.blocks_map(vt.nd, get_cblock_info, ubx.is_cblock_instance)
end

-- get_block_info - get detailed information on a block
local function get_block_info(vt, name)
   local b = ubx.block_get(vt.nd, name)
   return lsdb.tovariant2(ubx.block_totab(b))
end

-- get_modules
local function get_modules(vt)
   local function get_mod_info(m)
      return {
	 ubx.safe_tostr(m.id),
	 ubx.safe_tostr(m.spdx_license_id),
      }
   end
   return ubx.modules_map(vt.nd, get_mod_info)
end

-- get_connections
local function get_connections(vt)
   local blocks = ubx.blocks_map(vt.nd, ubx.block_totab, ubx.is_instance)

   local r = {}
   for _,b in ipairs(blocks) do
      for _,p in ipairs(b.ports) do
	 for _,ib_out in ipairs(p.connections.outgoing) do
	    r[#r+1] = { from={ b.name, p.name }, to=ib_out }
	 end
	 for _,ib_in in ipairs(p.connections.incoming) do
	    r[#r+1] = { from=ib_in, to={ b.name, p.name } }
	 end
      end
   end
   return lsdb.tovariant2(r)
end

local intf = {
   name = "org.ubx.node",
   methods = {
      LoadModule = {
	 { direction='in', name='name', type='s' },
	 handler = load_module
      },
      CreateBlock = {
	 { direction='in', name='type', type='s' },
	 { direction='in', name='name', type='s' },
	 { direction='in', name='config', type='a{sv}' },
	 handler = create_block,
      },
      RemoveBlock = {
	 { direction='in', name='name', type='s' },
	 handler = remove_block,
      },
      SwitchState = {
	 { direction='in', name='name', type='s' },
	 { direction='in', name='state', type='s' },
	 handler = switch_state,
      },
      Trigger = {
	 { direction='in', name='blocks', type='as' },
	 handler = trigger_blocks,
      },
      GetBlockInfo = {
	 { direction='in', name='name', type='s' },
	 { direction='out', name='info', type='a{sv}' },
	 handler = get_block_info
      },
      Connect = {
	 { direction='in', name='srcbn', type='s' },
	 { direction='in', name='srcpn', type='s' },
	 { direction='in', name='tgtbn', type='s' },
	 { direction='in', name='tgtpn', type='s' },
	 { direction='in', name='ibtype', type='s' },
	 { direction='in', name='ibconfig', type='v' },
	 handler = connect,
      },
      Write = {
	 { direction='in', name='tgtbn', type='s' },
	 { direction='in', name='tgtpn', type='s' },
	 { direction='in', name='value', type='v' },
	 handler = write,
      },
      Read = {
	 { direction='in', name='tgtbn', type='s' },
	 { direction='in', name='tgtpn', type='s' },
	 { direction='out', name='value', type='v' },
	 handler = read,
      },

      -- LoadUSC = { }

      ClearNode = {
	 handler = clear_node,
      }
   },

   properties = {
      Node =        { access='read', type='s',      get=get_node_name },
      CBlockTypes = { access='read', type='as',     get=get_cblock_types },
      IBlockTypes = { access='read', type='as',     get=get_iblock_types },
      CBlocks =     { access='read', type='a(sss)', get=get_cblocks },
      Modules =     { access='read', type='a(ss)',  get=get_modules },
      Connections = { access='read', type='av',     get=get_connections },
   }
}

local bus, vt

function init(block)
   -- add config bus type ?
   return true
end

function start(block)
   block = ffi.cast("ubx_block_t*", block)
   local nd = block.nd
   local ndname = ubx.safe_tostr(nd.name)
   ubx.ffi_load_types(nd)

   bus = lsdb.open()
   bus:request_name(fmt(SERVICE, ndname))
   vt = lsdb.server.new(bus, "/", intf)
   vt.nd = nd
   vt:emitAllPropertiesChanged()

   return true
end

function step(block)
   while bus:run(BUS_RUN_TIMEOUT_USEC) > 0 do end
end

function stop(block)
   vt:unref()
end

function cleanup(block)
end
