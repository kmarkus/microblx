--
-- Test ubx Lua API: predicates, introspection, iterators, type system
--

local lu = require("luaunit")
local ffi = require("ffi")
local ubx = require("ubx")

local ae  = lu.assert_equals
local at  = lu.assert_true
local af  = lu.assert_false
local ann = lu.assert_not_nil
local an  = lu.assert_nil

-- shared node and blocks
local nd, b_rand, b_trig, b_lfrb

local function setup()
   nd = ubx.node_create("test_api")
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "random")
   ubx.load_module(nd, "trig")
   ubx.load_module(nd, "lfrb")
   ubx.ffi_load_types(nd)
   b_rand = ubx.block_create(nd, "ubx/random", "myrand",
			     { min_max_config = { min = 0, max = 100 } })
   b_trig = ubx.block_create(nd, "ubx/trig", "mytrig")
   b_lfrb = ubx.block_create(nd, "ubx/lfrb", "mylfrb",
			     { type_name = "double", buffer_len = 4 })
end

local function teardown()
   if nd then ubx.node_rm(nd) end
   nd, b_rand, b_trig, b_lfrb = nil, nil, nil, nil
end

------------------------------------------------------------------------------
-- Type predicates
------------------------------------------------------------------------------

TestTypePredicates = {}
TestTypePredicates.setupClass  = setup
TestTypePredicates.teardownClass = teardown

function TestTypePredicates:test_is_node()
   at(ubx.is_node(nd))
   af(ubx.is_node(b_rand))
   af(ubx.is_node(nil))
   af(ubx.is_node(42))
end

function TestTypePredicates:test_is_block()
   at(ubx.is_block(b_rand))
   at(ubx.is_block(b_trig))
   af(ubx.is_block(nd))
   af(ubx.is_block(nil))
end

function TestTypePredicates:test_is_config()
   local c = ubx.block_config_get(b_rand, "min_max_config")
   at(ubx.is_config(c))
   af(ubx.is_config(nd))
   af(ubx.is_config(nil))
end

function TestTypePredicates:test_is_port()
   local p = ubx.block_port_get(b_trig, "tstats")
   at(ubx.is_port(p))
   af(ubx.is_port(nd))
   af(ubx.is_port(nil))
end

function TestTypePredicates:test_is_data()
   local d = ubx.data_alloc(nd, "double")
   at(ubx.is_data(d))
   af(ubx.is_data(nd))
   af(ubx.is_data(nil))
end

------------------------------------------------------------------------------
-- Block type predicates (is_cblock / is_iblock / is_proto / is_instance)
------------------------------------------------------------------------------

TestBlockPredicates = {}
TestBlockPredicates.setupClass  = setup
TestBlockPredicates.teardownClass = teardown

function TestBlockPredicates:test_is_cblock_iblock()
   at(ubx.is_cblock(b_rand))
   af(ubx.is_iblock(b_rand))
   at(ubx.is_iblock(b_lfrb))
   af(ubx.is_cblock(b_lfrb))
end

function TestBlockPredicates:test_is_instance()
   -- block_create returns an instance
   af(ubx.is_proto(b_rand))
   at(ubx.is_instance(b_rand))
end

function TestBlockPredicates:test_is_proto()
   -- the registered prototype is still accessible by its type name
   local proto = ubx.block_get(nd, "ubx/random")
   at(ubx.is_proto(proto))
   af(ubx.is_instance(proto))
end

function TestBlockPredicates:test_combined_predicates()
   at(ubx.is_cblock_instance(b_rand))
   af(ubx.is_iblock_instance(b_rand))
   at(ubx.is_iblock_instance(b_lfrb))
   af(ubx.is_cblock_instance(b_lfrb))

   local cproto = ubx.block_get(nd, "ubx/random")
   at(ubx.is_cblock_proto(cproto))
   af(ubx.is_iblock_proto(cproto))

   local iproto = ubx.block_get(nd, "ubx/lfrb")
   at(ubx.is_iblock_proto(iproto))
   af(ubx.is_cblock_proto(iproto))
end

------------------------------------------------------------------------------
-- Block attribute predicates + method style
------------------------------------------------------------------------------

TestBlockAttrs = {}
TestBlockAttrs.setupClass  = setup
TestBlockAttrs.teardownClass = teardown

function TestBlockAttrs:test_random_no_attrs()
   af(ubx.block_isactive(b_rand))
   af(ubx.block_istrigger(b_rand))
end

function TestBlockAttrs:test_trig_is_trigger_not_active()
   at(ubx.block_istrigger(b_trig))
   af(ubx.block_isactive(b_trig))
end

function TestBlockAttrs:test_block_hasattr()
   at(ubx.block_hasattr(b_trig, ffi.C.BLOCK_ATTR_TRIGGER))
   af(ubx.block_hasattr(b_trig, ffi.C.BLOCK_ATTR_ACTIVE))
   af(ubx.block_hasattr(b_rand, ffi.C.BLOCK_ATTR_TRIGGER))
end

function TestBlockAttrs:test_block_method_predicates()
   at(b_rand:is_cblock())
   af(b_rand:is_iblock())
   af(b_rand:is_trigger())
   af(b_rand:is_active())
   at(b_trig:is_trigger())
   af(b_trig:is_active())
   at(b_lfrb:is_iblock())
   af(b_lfrb:is_cblock())
   at(b_rand:is_instance())
   af(b_rand:is_proto())
   at(b_rand:is_cblock_instance())
   at(b_lfrb:is_iblock_instance())
end

------------------------------------------------------------------------------
-- Port predicates + method style
------------------------------------------------------------------------------

TestPortPredicates = {}
TestPortPredicates.setupClass  = setup
TestPortPredicates.teardownClass = teardown

function TestPortPredicates:test_port_direction()
   -- ubx/random: seed is inport, rnd is outport
   local p_seed = ubx.block_port_get(b_rand, "seed")
   local p_rnd  = ubx.block_port_get(b_rand, "rnd")
   at(ubx.is_inport(p_seed))
   af(ubx.is_outport(p_seed))
   af(ubx.is_inoutport(p_seed))
   at(ubx.is_outport(p_rnd))
   af(ubx.is_inport(p_rnd))
   af(ubx.is_inoutport(p_rnd))
end

function TestPortPredicates:test_port_method_direction()
   local p_seed = ubx.block_port_get(b_rand, "seed")
   local p_rnd  = ubx.block_port_get(b_rand, "rnd")
   at(p_seed:is_inport())
   af(p_seed:is_outport())
   af(p_seed:is_inoutport())
   at(p_rnd:is_outport())
   af(p_rnd:is_inport())
   af(p_rnd:is_inoutport())
end

------------------------------------------------------------------------------
-- Node introspection
------------------------------------------------------------------------------

TestNodeIntrospection = {}
TestNodeIntrospection.setupClass  = setup
TestNodeIntrospection.teardownClass = teardown

function TestNodeIntrospection:test_num_blocks()
   local nc, ni, inv = ubx.num_blocks(nd)
   -- random, trig are cblocks; lfrb is iblock; plus prototypes
   at(nc >= 2, "expected at least 2 cblocks, got "..nc)
   at(ni >= 1, "expected at least 1 iblock, got "..ni)
   ae(inv, 0)
end

function TestNodeIntrospection:test_num_types()
   local n = ubx.num_types(nd)
   at(n > 0, "expected types to be registered")
end

function TestNodeIntrospection:test_node_totab()
   local t = ubx.node_totab(nd)
   ann(t)
   ae(type(t), "table")
   ann(t.blocks)
   ann(t.types)
   -- blocks and types are keyed by name
   ann(t.blocks["myrand"])
   at(t.blocks["myrand"].block_type == "cblock")
end

function TestNodeIntrospection:test_node_totab_modules()
   local t = ubx.node_totab(nd)
   ann(t.modules)
   at(#t.modules >= 2, "expected at least stdtypes and random modules")
   ann(t.modules[1].id)
   ann(t.modules[1].license)
end

function TestNodeIntrospection:test_node_pp()
   local out = {}
   local orig = print
   print = function(s) out[#out+1] = s end
   ubx.node_pp(nd)
   print = orig
   ae(#out, 1)
   at(out[1]:find("myrand") ~= nil, "node_pp output should contain block name")
end

------------------------------------------------------------------------------
-- Block introspection
------------------------------------------------------------------------------

TestBlockIntrospection = {}
TestBlockIntrospection.setupClass  = setup
TestBlockIntrospection.teardownClass = teardown

function TestBlockIntrospection:test_block_tostr()
   local s = ubx.block_tostr(b_rand)
   ae(type(s), "string")
   at(#s > 0)
end

function TestBlockIntrospection:test_block_totab()
   local t = ubx.block_totab(b_rand)
   ae(type(t), "table")
   ae(t.name, "myrand")
   ae(t.block_type, "cblock")
   ann(t.state)
   ann(t.ports)
   ann(t.configs)
end

function TestBlockIntrospection:test_block_totab_prototype_field()
   local t_inst = ubx.block_totab(b_rand)
   ae(t_inst.prototype, "ubx/random")

   local proto = ubx.block_get(nd, "ubx/random")
   local t_proto = ubx.block_totab(proto)
   ae(t_proto.prototype, false)
end

function TestBlockIntrospection:test_block_pp()
   local out = {}
   local orig = print
   print = function(s) out[#out+1] = s end
   ubx.block_pp(b_rand)
   print = orig
   ae(#out, 1)
   at(out[1]:find("myrand") ~= nil, "block_pp output should contain block name")
end

------------------------------------------------------------------------------
-- Port introspection
------------------------------------------------------------------------------

TestPortIntrospection = {}
TestPortIntrospection.setupClass  = setup
TestPortIntrospection.teardownClass = teardown

function TestPortIntrospection:test_port_sizes()
   local p_seed = ubx.block_port_get(b_rand, "seed")
   local p_rnd  = ubx.block_port_get(b_rand, "rnd")
   at(ubx.port_in_size(p_seed) > 0)
   at(ubx.port_out_size(p_rnd) > 0)
end

function TestPortIntrospection:test_port_tostr()
   local p = ubx.block_port_get(b_rand, "rnd")
   local s = ubx.port_tostr(p)
   ae(type(s), "string")
   at(s:find("rnd") ~= nil, "port_tostr output should contain port name")
   at(s:find("out_type_name") ~= nil, "port_tostr output should contain type name field")
end

function TestPortIntrospection:test_port_totab()
   local p = ubx.block_port_get(b_rand, "rnd")
   local t = ubx.port_totab(p)
   ae(type(t), "table")
   ae(t.name, "rnd")
   ann(t.out_type_name)
end

function TestPortIntrospection:test_port_conns_totab()
   local p = ubx.block_port_get(b_rand, "rnd")
   local t = ubx.port_conns_totab(p)
   ae(type(t), "table")
end

------------------------------------------------------------------------------
-- Config introspection
------------------------------------------------------------------------------

TestConfigIntrospection = {}
TestConfigIntrospection.setupClass  = setup
TestConfigIntrospection.teardownClass = teardown

function TestConfigIntrospection:test_config_isnull()
   -- min_max_config was set during block_create
   local c = ubx.block_config_get(b_rand, "min_max_config")
   af(ubx.config_isnull(c))
   -- loglevel was not set → null
   local cl = ubx.block_config_get(b_rand, "loglevel")
   at(ubx.config_isnull(cl))
end

function TestConfigIntrospection:test_config_tostr()
   local c = ubx.block_config_get(b_rand, "min_max_config")
   local s = ubx.config_tostr(c)
   ae(type(s), "string")
   at(s:find("min_max_config") ~= nil, "config_tostr output should contain config name")
end

function TestConfigIntrospection:test_config_totab()
   local c = ubx.block_config_get(b_rand, "min_max_config")
   local t = ubx.config_totab(c)
   ae(type(t), "table")
   ae(t.name, "min_max_config")
end

function TestConfigIntrospection:test_set_config_str()
   -- set loglevel via string
   ubx.set_config_str(b_rand, "loglevel", "2")
   local c = ubx.block_config_get(b_rand, "loglevel")
   af(ubx.config_isnull(c))
end

------------------------------------------------------------------------------
-- Data introspection
------------------------------------------------------------------------------

TestDataIntrospection = {}
TestDataIntrospection.setupClass  = setup
TestDataIntrospection.teardownClass = teardown

function TestDataIntrospection:test_data_size()
   -- data_size returns total bytes: len * type.size
   local d = ubx.data_alloc(nd, "double", 3)
   ae(ubx.data_size(d), 3 * 8)
end

function TestDataIntrospection:test_data_tostr()
   local d = ubx.data_alloc(nd, "double")
   ubx.data_set(d, 3.14)
   local s = ubx.data_tostr(d)
   ae(type(s), "string")
   at(#s > 0)
end

function TestDataIntrospection:test_data_to_ctype()
   local d = ubx.data_alloc(nd, "double")
   local ct = ubx.data_to_ctype(d)
   ann(ct)
   -- ffi.typeof returns a cdata object in LuaJIT; verify it's usable
   at(ffi.istype(ct, ffi.typeof("double*")))
end

------------------------------------------------------------------------------
-- Type system
------------------------------------------------------------------------------

TestTypeSystem = {}
TestTypeSystem.setupClass  = setup
TestTypeSystem.teardownClass = teardown

function TestTypeSystem:test_type_size()
   ae(ubx.type_size(nd, "double"), 8)
   ae(ubx.type_size(nd, "uint32_t"), 4)
end

function TestTypeSystem:test_type_to_ctype()
   local t = ubx.type_get(nd, "double")
   ann(t)
   local ct = ubx.type_to_ctype(t, true)
   ann(ct)
   -- ffi.typeof returns a cdata object in LuaJIT; verify it's usable
   at(ffi.istype(ct, ffi.typeof("double*")))
end

function TestTypeSystem:test_ubx_type_totab()
   local t = ubx.type_get(nd, "double")
   local tab = ubx.ubx_type_totab(t)
   ae(type(tab), "table")
   ae(tab.name, "double")
   ann(tab.size)
end

function TestTypeSystem:test_type_tostr()
   local t = ubx.type_get(nd, "double")
   local s = ubx.type_tostr(t)
   ae(type(s), "string")
   at(#s > 0)
end

------------------------------------------------------------------------------
-- Iterators
------------------------------------------------------------------------------

TestIterators = {}
TestIterators.setupClass  = setup
TestIterators.teardownClass = teardown

function TestIterators:test_types_foreach()
   local count = 0
   ubx.types_foreach(nd, function() count = count + 1 end)
   at(count > 0, "expected at least one type")
end

function TestIterators:test_modules_foreach()
   local names = {}
   ubx.modules_foreach(nd, function(m)
      names[#names+1] = ubx.safe_tostr(m.id)
   end)
   at(#names >= 2, "expected at least stdtypes and random modules")
end

function TestIterators:test_modules_map()
   local res = ubx.modules_map(nd, function(m)
      return ubx.safe_tostr(m.id)
   end)
   at(#res >= 2)
   ae(type(res[1]), "string")
end

function TestIterators:test_blocks_map()
   local names = ubx.blocks_map(nd, function(b)
      return ubx.safe_tostr(b.name)
   end)
   at(#names >= 3, "expected myrand, mytrig, mylfrb at minimum")
end

function TestIterators:test_ports_foreach()
   local count = 0
   ubx.ports_foreach(b_rand, function() count = count + 1 end)
   at(count >= 2, "random has at least seed and rnd ports")
end

function TestIterators:test_ports_map()
   local names = ubx.ports_map(b_rand, function(p)
      return ubx.safe_tostr(p.name)
   end)
   at(#names >= 2)
   ae(type(names[1]), "string")
end

function TestIterators:test_configs_map()
   local names = ubx.configs_map(b_rand, function(c)
      return ubx.safe_tostr(c.name)
   end)
   at(#names >= 1, "random has at least min_max_config")
end

function TestIterators:test_ports_foreach_with_pred()
   local count = 0
   ubx.ports_foreach(b_rand, function() count = count + 1 end, ubx.is_outport)
   ae(count, 1)  -- only "rnd" is an outport
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
