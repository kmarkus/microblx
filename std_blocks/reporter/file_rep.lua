local ubx=require("ubx")
local ubx_utils = require("ubx_utils")
local utils = require("utils")
local ffi = require("ffi")
local time = require("time")
local ts = tostring

-- global state
filename=nil
rconf=nil
separator=nil
fd=nil

-- sample_conf={
--    { blockname='blockA', portname="port1", buff_len=10, },
--    { blockname='blockB', portname=true, buff_len=10 }, -- report all
--}

local ts1=ffi.new("struct ubx_timespec")
local ns_per_s = 1000000000

function get_time()
   ubx.clock_mono_gettime(ts1)
   return tonumber(ts1.sec) + tonumber(ts1.nsec) / ns_per_s
end

--- safely convert a Lua configuration table string to a table.
-- @param str
-- @return lua table or false, msg
function load_conf(str)
   local tab, msg = load("return "..str, nil, 't', {})
   if not tab then return false, msg end
   return pcall(tab)
end

--- For the given port, create a ubx_data to hold the result of a read.
-- @param port
-- @return ubx_data_t sample
function create_read_sample(p, ni)
   return ubx.data_alloc(ni, p.out_type_name, p.out_data_len)
end

--- convert the report_conf string to a table.
-- @param report_conf str
-- @param ni node_info
-- @return rconf table with inv. conn. ports
local function report_conf_to_portlist(rc, ni)
   local succ, res = load_conf(rc)
   if not succ then error("file_rep: failed to load report_conf: "..res) end

   for _,conf in ipairs(res) do
      local bname, pname = ts(conf.blockname), ts(conf.portname)

      local b = ubx.block_get(ni, bname)
      if b==nil then
	 print("file_rep error: no block "..bname.." found")
	 return false
      end
      local p = ubx.port_get(b, pname)
      if p==nil then
	 print("file_rep error: block "..bname.." has no port "..pname)
	 return false
      end

      if conf.buff_len==nil or conf.buff_len <=0 then
	 conf.buff_len=1
      end

      if p.out_type~=nil then
	 print("file_rep: reporting ", bname.."."..pname)
	 local pinv = ubx.port_clone_conn(b, pname, conf.buff_len)
	 conf.pinv=pinv
	 conf.sample=create_read_sample(p, ni)
      else
	 print("file_rep: refusing to report in-port ", bname.."."..pname)
      end
   end
   return res
end

function init(b)
   b=ffi.cast("ubx_block_t*", b)

   local rconf_str = ubx.data_tolua(ubx.config_get_data(b, "report_conf"))
   filename = ubx.data_tolua(ubx.config_get_data(b, "filename"))
   separator = ubx.data_tolua(ubx.config_get_data(b, "separator"))
   print(('file_reporter.init: reporting to file="%s", sep="%s", conf=%s'):format(filename, separator, rconf_str))

   rconf = report_conf_to_portlist(rconf_str, b.ni)

   fd=io.open(filename, 'w+') -- trunc
   fd:setvbuf("line")
   return true
end

function start(b)
   b=ffi.cast("ubx_block_t*", b)
   ubx.ffi_load_types(b.ni)
   return true
end

function step(b)
   b=ffi.cast("ubx_block_t*", b)
   local cur_t = get_time()
   local wtab = {}

   wtab[#wtab+1] = ("time=%f"):format(cur_t)

   for _,c in ipairs(rconf) do
      local bpn=c.blockname.."."..c.portname
      -- print("reading ", bpn)
      if ubx.port_read(c.pinv, c.sample) < 0 then
	 print("file_rep error: failed to read "..c.blockname.."."..c.portname)
      else
	 wtab[#wtab+1] = ('["%s"]=%s'):format(bpn, ubx.data_tostr(c.sample))
      end
   end
   fd:write("{ "..table.concat(wtab, ', ').." }\n")
end

function cleanup(b)
   io.close(fd)
   fd=nil
   filename=nil
   report_conf=nil
   separator=nil
   plist=nil
end
