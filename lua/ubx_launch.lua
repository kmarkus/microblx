#!/usr/bin/env luajit

local ffi = require("ffi")
local ubx = require "ubx"
local ubx_utils = require("ubx_utils")

local bd = require("blockdiagram")
local ts = tostring

function usage()
   print( [=[
microblx function blocks system launcher

usage ubx_launch OPTIONS -c <conf file>
   -c           configuration file to launch
   -nodename    name to give to node
   -validate    dont run, just validate configuration file
   -webif	create and start a webinterface block
   -h           show this.
]=])
end

local opttab=utils.proc_args(arg)
local nodename="node-"..os.date("%Y%m%d_%H%M%S")

if #arg==1 or opttab['-h'] then
   usage(); os.exit(1)
end

if not (opttab['-c'] and opttab['-c'][1]) then
   print("no configuration file given (-c option)")
   os.exit(1)
else
   conf_file = opttab['-c'][1]
end

local suc, model = pcall(dofile, conf_file)

if not suc then
   print(model)
   print("ubx_launch failed to load file "..ts(conf_file))
   os.exit(1)
end

if opttab['-nodename'] then
   if opttab['-nodename'][1] then
      print("-nodename option requires a node name argument)")
      os.exit(1)
   else
      nodename = opttab['-nodename'][1]
   end
end


if opttab['-validate'] then
   model:validate(true)
   os.exit(1)
end

model:launch(nodename)