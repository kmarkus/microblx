--
-- Test blockdiagram system.merge functionality
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local utils = require("utils")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN
ubx.color = false

local ni

TestMerge = {}

function TestMerge:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

--- Test merging blocks: override=true replaces existing block
function TestMerge:TestMergeBlocksOverride()
   local base = bd.system {
      imports = { "stdtypes", "random" },
      blocks = {
	 { name = "rnd1", type = "ubx/random" },
      },
      configurations = {
	 { name = "rnd1", config = { min_max_config = { min = 1, max = 10 } } },
      },
   }

   local overlay = bd.system {
      blocks = {
	 { name = "rnd1", type = "ubx/random" },
      },
      configurations = {
	 { name = "rnd1", config = { min_max_config = { min = 100, max = 200 } } },
      },
   }

   bd.system.merge(base, overlay, true)

   ni = base:launch({ nodename = "TestMergeOverride", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)
   lu.assert_equals(ni:b("rnd1"):c("min_max_config"):tolua(), { min = 100, max = 200 })
end

--- Test merging blocks: override=false keeps existing block config
function TestMerge:TestMergeBlocksNoOverride()
   local base = bd.system {
      imports = { "stdtypes", "random" },
      blocks = {
	 { name = "rnd1", type = "ubx/random" },
      },
      configurations = {
	 { name = "rnd1", config = { min_max_config = { min = 1, max = 10 } } },
      },
   }

   local overlay = bd.system {
      blocks = {
	 { name = "rnd1", type = "ubx/random" },
      },
      configurations = {
	 { name = "rnd1", config = { min_max_config = { min = 100, max = 200 } } },
      },
   }

   bd.system.merge(base, overlay, false)

   ni = base:launch({ nodename = "TestMergeNoOverride", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)
   lu.assert_equals(ni:b("rnd1"):c("min_max_config"):tolua(), { min = 1, max = 10 })
end

--- Test merging appends new blocks
function TestMerge:TestMergeAppendsNewBlock()
   local base = bd.system {
      imports = { "stdtypes", "random" },
      blocks = {
	 { name = "rnd1", type = "ubx/random" },
      },
      configurations = {
	 { name = "rnd1", config = { min_max_config = { min = 1, max = 10 } } },
      },
   }

   local overlay = bd.system {
      blocks = {
	 { name = "rnd2", type = "ubx/random" },
      },
      configurations = {
	 { name = "rnd2", config = { min_max_config = { min = 50, max = 60 } } },
      },
   }

   bd.system.merge(base, overlay, true)

   ni = base:launch({ nodename = "TestMergeAppend", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)
   lu.assert_equals(ni:b("rnd1"):c("min_max_config"):tolua(), { min = 1, max = 10 })
   lu.assert_equals(ni:b("rnd2"):c("min_max_config"):tolua(), { min = 50, max = 60 })
end

--- Test merging connections: override=true replaces existing conn
function TestMerge:TestMergeConnectionsOverride()
   local base = bd.system {
      imports = { "stdtypes", "random", "lfds_cyclic" },
      blocks = {
	 { name = "rnd1", type = "ubx/random" },
	 { name = "rnd2", type = "ubx/random" },
	 { name = "rnd3", type = "ubx/random" },
      },
      configurations = {
	 { name = "rnd1", config = { min_max_config = { min = 1, max = 10 } } },
	 { name = "rnd2", config = { min_max_config = { min = 1, max = 10 } } },
	 { name = "rnd3", config = { min_max_config = { min = 1, max = 10 } } },
      },
      connections = {
	 { src = "rnd1.rnd", tgt = "rnd2.seed" },
      },
   }

   local overlay = bd.system {
      connections = {
	 { src = "rnd1.rnd", tgt = "rnd3.seed" },
      },
   }

   bd.system.merge(base, overlay, true)

   -- after merge, there should be 2 connections: the original was
   -- not replaced because src+tgt differ; the new one was appended
   lu.assert_equals(#base.connections, 2)
end

--- Test merging node_configurations: override=true
function TestMerge:TestMergeNodeConfigOverride()
   local base = bd.system {
      imports = { "stdtypes", "random" },
      blocks = {
	 { name = "rnd1", type = "ubx/random" },
      },
      node_configurations = {
	 rnd_conf = { type = "struct random_config", config = { min = 1, max = 10 } },
      },
      configurations = {
	 { name = "rnd1", config = { min_max_config = "&rnd_conf" } },
      },
   }

   local overlay = bd.system {
      node_configurations = {
	 rnd_conf = { type = "struct random_config", config = { min = 999, max = 1111 } },
      },
   }

   bd.system.merge(base, overlay, true)

   ni = base:launch({ nodename = "TestMergeNdcfg", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)
   lu.assert_equals(ni:b("rnd1"):c("min_max_config"):tolua(), { min = 999, max = 1111 })
end

--- Test merging node_configurations: override=false keeps original
function TestMerge:TestMergeNodeConfigNoOverride()
   local base = bd.system {
      imports = { "stdtypes", "random" },
      blocks = {
	 { name = "rnd1", type = "ubx/random" },
      },
      node_configurations = {
	 rnd_conf = { type = "struct random_config", config = { min = 1, max = 10 } },
      },
      configurations = {
	 { name = "rnd1", config = { min_max_config = "&rnd_conf" } },
      },
   }

   local overlay = bd.system {
      node_configurations = {
	 rnd_conf = { type = "struct random_config", config = { min = 999, max = 1111 } },
      },
   }

   bd.system.merge(base, overlay, false)

   ni = base:launch({ nodename = "TestMergeNdcfgNoOvr", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)
   lu.assert_equals(ni:b("rnd1"):c("min_max_config"):tolua(), { min = 1, max = 10 })
end

--- Test merging individual config fields with override
function TestMerge:TestMergePartialConfigOverride()
   local base = bd.system {
      imports = { "stdtypes", "random" },
      blocks = {
	 { name = "rnd1", type = "ubx/random" },
      },
      configurations = {
	 { name = "rnd1", config = { min_max_config = { min = 1, max = 10 }, loglevel = 6 } },
      },
   }

   local overlay = bd.system {
      configurations = {
	 { name = "rnd1", config = { loglevel = 4 } },
      },
   }

   bd.system.merge(base, overlay, true)

   ni = base:launch({ nodename = "TestPartialCfg", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)
   -- loglevel should be overridden
   lu.assert_equals(ni:b("rnd1"):c("loglevel"):tolua(), 4)
   -- min_max_config should be preserved
   lu.assert_equals(ni:b("rnd1"):c("min_max_config"):tolua(), { min = 1, max = 10 })
end

os.exit(lu.LuaUnit.run())
