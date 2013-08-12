


function init(block, ni)
   print("init")
   return true
end

function start(block, ni)
   print("start")
   return true
end

function step(block, ni) print("step") end
function stop(block, ni) print("stop") end
function cleanup(block, ni) print("cleanup") end
