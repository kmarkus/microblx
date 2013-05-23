
-- Lua API for u5c.
-- should sink to c eventually.

local ffi = require "ffi"

ffi.cdef[[

]]

---
--- Low level plumbing API
--- 

--- Node initalization
-- @param name
-- @return node_info
function init_node(name)
end

function cleanup_node(name)
end


--- Loading components, interactions and type definitions.

function load_comp_def(filename)
end

function load_inter_def(filename)
end

function load_type_def(filename)
end

--- Load all components, types and interactions
-- @param directory
function load_package(directory)
end

---
--- Creating components, interactions, and type defintion
---

--- Create a new component instance.
-- @param type
-- @param name
-- @return u5c_component
function comp_create(type, name)
end

function interaction_new

function port_connect(pname1, pname2, iname)

end
