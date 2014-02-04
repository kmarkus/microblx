--
-- Microblx generic logger
--
-- SPDX-License-Identifier: BSD-3-Clause
--

local ubx=require("ubx")
local utils = require("utils")
local cdata = require("cdata")
local ffi = require("ffi")
local time = require("time")
local ts = tostring

-- color handling via ubx
red=ubx.red; blue=ubx.blue; cyan=ubx.cyan; white=ubx.cyan; green=ubx.green; yellow=ubx.yellow; magenta=ubx.magenta

-- global state
filename=nil
rconf=nil
separator=nil
fd=nil
timestamp=nil

-- sample_conf={
--    { blockname='blockA', portname="port1", buff_len=10, },
--    { blockname='blockB', portname=true, buff_len=10 }, -- report all (not supported yet)␘
--    { blockname='iblock' }, -- will connect directly to the out channel of the given iblock.
--}

local ts1=ffi.new("struct ubx_timespec")
local ns_per_s = 1000000000

function get_time()
   ubx.clock_mono_gettime(ts1)
   return tonumber(ts1.sec) + tonumber(ts1.nsec) / ns_per_s
end

--- convert the report_conf string to a table.
-- @param report_conf str
-- @param ni node_info
-- @return rconf table with inv. conn. ports
local function report_conf_to_portlist(rc, this)
   local ni = this.ni

   local succ, res = utils.eval_sandbox("return "..rc)
   if not succ then error(red("file_logger: failed to load report_conf:\n"..res, true)) end

   for i,conf in ipairs(res) do
      local bname, pname = conf.blockname, conf.portname

      local b = ni:b(bname)

      if b==nil then
	 print(red("file_logger error: no block ",true)..green(bname, true)..red(" found", true))
	 return false
      end

      -- are we directly connecting to an iblock??
      if pname==nil and ubx.is_iblock(b) then
	 print("file_logger: reporting iblock ".. ubx.safe_tostr(b.name))
	 local p_rep_name='r'..ts(i)
	 local type_name = ubx.data_tolua(ubx.config_get_data(b, "type_name"))
	 local data_len = ubx.data_tolua(ubx.config_get_data(b, "data_len"))

	 ubx.port_add(this, p_rep_name, "reporting iblock "..bname, type_name, data_len, nil, 0, 0)
	 local p = ubx.port_get(this, p_rep_name)
	 ubx.port_connect_in(p, b)

	 conf.type = 'iblock'
	 conf.bname = bname
	 conf.pname = pname
	 conf.p_rep_name = p_rep_name
	 conf.sample = ubx.data_alloc(ni, p.in_type_name, p.in_data_len)
	 conf.sample_cdata = ubx.data_to_cdata(conf.sample, true)
	 conf.serfun=cdata.gen_logfun(ubx.data_to_ctype(conf.sample, true), bname)

      else -- normal connection to cblock
	 local p = ubx.port_get(b, pname)
	 if p==nil then
	    print(red("file_logger error: block ", true)..green(bname, true)..red(" has no port ", true)..cyan(pname, true))
	    return false
	 end

	 if conf.buff_len==nil or conf.buff_len <=0 then conf.buff_len=1 end

	 if p.out_type~=nil then
	    local blockport = bname.."."..pname
	    local p_rep_name='r'..ts(i)
	    local iblock
	    print("file_logger: reporting "..blockport.." as "..p_rep_name)
	    ubx.port_add(this, p_rep_name, "reporting "..blockport, p.out_type_name, p.out_data_len, nil, 0, 0)
	    iblock = ubx.conn_lfds_cyclic(b, pname, this, p_rep_name, conf.buff_len)

	    conf.type = 'port'
	    conf.iblock_name = iblock:get_name()
	    conf.bname = bname
	    conf.pname = pname
	    conf.p_rep_name = p_rep_name
	    conf.sample=ubx.port_alloc_write_sample(p)
	    conf.sample_cdata = ubx.data_to_cdata(conf.sample, true)
	    conf.serfun=cdata.gen_logfun(ubx.data_to_ctype(conf.sample, true), blockport)
	 else
	    print(red("ERR: file_logger: refusing to report in-port ", bname.."."..pname), true)
	 end
      end
   end

   -- cache port ptr's (only *after* adding has finished (realloc!)
   for _,conf in ipairs(res) do conf.pinv=ubx.port_get(this, conf.p_rep_name) end

   return res
end

--- init: parse config and create port and connections.
function init(b)
   b=ffi.cast("ubx_block_t*", b)
   ubx.ffi_load_types(b.ni)

   local rconf_str = ubx.data_tolua(ubx.config_get_data(b, "report_conf"))

   if not rconf_str or rconf_str == 0 then
      print(ubx.safe_tostr(b.name)..": invalid/nonexisting report_conf")
      return false
   end

   filename = ubx.data_tolua(ubx.config_get_data(b, "filename"))
   separator = ubx.data_tolua(ubx.config_get_data(b, "separator"))
   timestamp = ubx.data_tolua(ubx.config_get_data(b, "timestamp"))

   -- print(('file_logger.init: reporting to file="%s", sep="%s", conf=%s'):format(filename, separator, rconf_str))
   print(('file_logger: reporting to file="%s", sep="%s"'):format(filename, separator))

   rconf = report_conf_to_portlist(rconf_str, b)

   fd=io.open(filename, 'w+') -- trunc
   fd:setvbuf("line")
   return true
end

--- start: write header
function start(b)
   b=ffi.cast("ubx_block_t*", b)
   ubx.ffi_load_types(b.ni)

   if timestamp~=0 then
      fd:write(("time, "):format(get_time()))
   end

   for i=1,#rconf do
      rconf[i].serfun("header", fd)
      if i<#rconf then fd:write(", ") end
   end

   fd:write("\n")
   return true
end

--- step: read ports and write values
function step(b)
   b=ffi.cast("ubx_block_t*", b)

   if timestamp~=0 then
      fd:write(("%f, "):format(get_time()))
   end

   for i=1,#rconf do
      if ubx.port_read(rconf[i].pinv, rconf[i].sample) < 0 then
	 print("file_logger error: failed to read "..rconf.blockname.."."..rconf.portname)
      else
	 rconf[i].serfun(rconf[i].sample_cdata, fd)
	 if i<#rconf then fd:write(", ") end
      end
   end
   fd:write("\n")
end

--- cleanup
function cleanup(b)
   b=ffi.cast("ubx_block_t*", b)
   local ni = b.ni

   -- cleanup connections and remove ports
   for i,c in ipairs(rconf) do
      if c.type == 'iblock' then
	 ubx.port_disconnect_in(b:p(c.p_rep_name), ni:b(c.bname))
	 b:port_rm(c.p_rep_name)
      else
	 -- disconnect reporting iblock from reported port:
	 ubx.port_disconnect_out(ni:b(c.bname):p(c.pname), ni:b(c.iblock_name))
	 -- disconnect local port from iblock and remove it
	 ubx.port_disconnect_in(b:p(c.p_rep_name), ni:b(c.iblock_name))
	 b:port_rm(c.p_rep_name)
	 -- unload iblock
	 ni:block_unload(c.iblock_name)
      end
   end

   io.close(fd)
   fd=nil
   filename=nil
   rconf=nil
   separator=nil
end
