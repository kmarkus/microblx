
local ubx=require "ubx"
local ubx_utils = require("lua/ubx_utils")

function init(block, ni)
   print("init")
   for k=1,100 do
      ubx.port_add(block, "hoop"..tostring(k), "meta", "int32_t", 5, nil, 0, 0)
   end
   return true
end

function start(block, ni)
   print("start")
   return true
end

function step(block, ni) print("step") end
function stop(block, ni) print("stop") end
function cleanup(block, ni) print("cleanup") end
