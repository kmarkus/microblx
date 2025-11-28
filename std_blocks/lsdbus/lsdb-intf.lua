
local ubx = require("ubx")
local ffi = require("ffi")
local lsdb = require("lsdbus")


local SERVICE =	"com.microblx.lsdb"

local bus, vt

local intf = {
   name = "com.microblx.lsdb",
   methods = {
      LoadModule = {
	 { direction='in', name='name', type='s' },
	 handler=function(vt, name) vt.nd:load_module(name) end
      },
      -- BlockCreate = {}
      -- BlockRemove = {}
      -- Connect
      -- GetConnections

   },
   properties = {
      NodeName = {
	 access = 'read',
	 type = 's',
	 get = function (vt) return vt.nd:get_name() end
      },
      CBlockTypes = {
	 access = 'read',
	 type = 'as',
	 get = function (vt)
	    return ubx.blocks_map(
	       vt.nd,
	       function (b) return ubx.safe_tostr(b.name) end,
	       ubx.is_cblock_proto)
	 end
      }
   }
}

function init(block)
   -- TODO config bus type
   -- TODO config endpoint
   return true
end

function start(block)
   block = ffi.cast("ubx_block_t*", block)
   bus = lsdb.open()
   bus:request_name(SERVICE)
   vt = lsdb.server.new(bus, "/", intf)
   vt.nd = block.nd
   vt:emitAllPropertiesChanged()
   return true
end

function step(block)
   bus:run(1)
end

function stop(block)
   print("stop")
   vt:unref()
   bus:close()
end

function cleanup(block)
end
