
local ubx=require "ubx"

function init(block)
   print("init")
   for k=1,5 do
      ubx.config_add(block, "xaoo"..tostring(k), "meta inf here", "int32_t", k)
   end

   return true
end

function start(block)
   print("start")
   for k=1,10 do
      ubx.port_add(block, "hoop"..tostring(k), "meta", "int32_t", 5, nil, 0, 0)
   end
   return true
end

function step(block)
   print("step")
   -- local res, dat = ubx.port_read(ubx.port_get(block, "exec_str"))
end

function stop(block)
   print("stop")
   print("rm'ing", ubx.port_rm(block, "hoop10"))
   print("rm'ing", ubx.port_rm(block, "hoop9"))
   print("rm'ing", ubx.port_rm(block, "hoop8"))
   print("rm'ing", ubx.port_rm(block, "hoop7"))

   for k=1,6 do
      print("rm'ing", ubx.port_rm(block, "hoop"..tostring(k)))
   end
end
function cleanup(block) 
   print("cleanup") 
   print("rm'ing", ubx.config_rm(block, "xaoo1"))
   print("rm'ing", ubx.config_rm(block, "xaoo5"))

   for k=2,4 do
      ubx.config_rm(block, "xaoo"..tostring(k))
   end

end
