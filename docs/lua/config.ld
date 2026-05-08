-- ldoc configuration for microblx Lua API docs

title = "microblx Lua API"
project = "microblx"
description = "LuaJIT FFI binding and Lua API for microblx"

-- source files to document
file = {
   "../../lua/ubx.lua",
   "../../lua/blockdiagram.lua",
}

-- only document exported (module-level) symbols
all = false

-- use @section to group related functions
use_markdown_titles = true

format = 'discount'
