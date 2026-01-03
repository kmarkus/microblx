
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

local function block_switch_state(vt, name, state)
   local b = ubx.block_get(vt.nd, name)
   ubx.block_tostate(b, state)
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
      SwitchBlockState = {
	 { direction='in', name='name', type='s' },
	 { direction='in', name='state', type='s' },
	 handler = block_switch_state,
      },

      GetNodeInfo = {
	 { direction='out', name='info', type='as' },
	 handler=function(vt)
	    local ok, res =  xpcall(ubx.node_totab, debug.traceback, vt.nd)
	    if not ok then
	       error(res)
	    else
	       return lsdb.tovariant2(res)
	    end
	 end
      },

      -- Connect
      -- GetConnections

   },
   properties = {
      NodeName =    { access='read', type='s',      get=get_node_name },
      CBlockTypes = { access='read', type='as',     get=get_cblock_types },
      IBlockTypes = { access='read', type='as',     get=get_iblock_types },
      CBlocks =     { access='read', type='a(sss)', get=get_cblocks },
   }
}

local bus, vt

function init(block)
   -- TODO config bus type
   -- TODO config endpoint
   return true
end

function start(block)
   block = ffi.cast("ubx_block_t*", block)
   bus = lsdb.open()
   local ndname = ubx.safe_tostr(block.nd.name)
   bus:request_name(fmt(SERVICE, ndname))
   vt = lsdb.server.new(bus, "/", intf)
   vt.nd = block.nd
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
