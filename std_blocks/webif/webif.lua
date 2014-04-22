local ffi=require"ffi"
local ubx=require"ubx"
local utils=require "utils"

local safe_ts = ubx.safe_tostr

-- Configuration
num_type_cols=4

--- define mongoose request info
ffi.cdef[[
struct mg_request_info {
  const char *request_method; // "GET", "POST", etc
  const char *uri;            // URL-decoded URI
  const char *http_version;   // E.g. "1.0", "1.1"
  const char *query_string;   // URL part after '?', not including '?', or NULL
  const char *remote_user;    // Authenticated user, or NULL if no auth used
  long remote_ip;             // Client's IP address
  int remote_port;            // Client's port
  int is_ssl;                 // 1 if SSL-ed, 0 if not
  void *user_data;            // User data pointer passed to mg_start()

  int num_headers;            // Number of HTTP headers
  struct mg_header {
    const char *name;         // HTTP header name
    const char *value;        // HTTP header value
  } http_headers[64];         // Maximum 64 headers
};
]]

--
-- Generic helpers
--

-- Decode an encode url or query string.
-- @param string encoded string
-- @return decoded plain Lua string
function url_decode(str)
   str = string.gsub(str, "+", " ")
   str = string.gsub(str, "%%(%x%x)", function(h) return string.char(tonumber(h,16)) end)
   str = string.gsub(str, "\r\n", "\n")
   return str
end

--- Convert a query string to a Lua table.
-- @param qs encoded query string
-- @return Lua table
function query_string_to_tab(qs)
   local res = {}
   local keyvals=utils.split(qs, "&")
   for _,kv in ipairs(keyvals) do
      local k,v = string.match(url_decode(kv), "^([^=]+)=(.+)$")
      res[k]=v
   end
   return res
end


---
--- HTML generation helpers.
---

function html(title, ...)
   return ([[
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN">
<html lang="en">
<head>
 <title>%s</title>
 <link rel="stylesheet" type="text/css" href="style.css">
</head>

<body>
%s
</body>
</html>
	   ]]):format(title, table.concat({...}, "\n"))
end

-- Header
function h(num, text) return ('<h%d>%s</h%d>'):format(num, text, num) end

-- Span
function span(text, attrs)
   local function concat_attrs(attrs)
      local res={}
      for k,v in pairs(attrs) do res[#res+1]=k..':'..v end
      return table.concat(res, "; ")
   end
   return('<span style="%s">%s</span>'):format(concat_attrs(attrs),text)
end

-- Colors
function color(color, text) return span(text, { color=color }) end
function bold(text) return span(text, { ['font-weight']='bold' }) end

-- script
function script(body, attrs)
   attrs = attrs or {}
   local attr_tab = utils.map(function (v,k)
				 return tostring(k)..'="'..tostring(v)..'"'
			      end, attrs)
   local res = utils.expand(
[[
<script $attrs>
$body
</script>
]], {body=body, attrs = table.concat(attr_tab, " ")})
   return res
end

-- hyperlink
function a(link, text, query_str_tab)
   local del = ""
   local qst={}
   if query_str_tab then del='?' else query_str_tab={} end
   for k,v in pairs(query_str_tab) do qst[#qst+1]=("%s=%s"):format(k,v) end
   return ('<a href="%s%s%s">%s</a>'):format(link, del, table.concat(qst, '&'), text)
end

function table_row(bdy)
   return ([[
<tr>
%s
</tr>
	   ]]):format(bdy)
end


function table_data(data) return ('<td>%s</td>'):format(data) end
function table_head(data) return ('<th>%s</th>'):format(data) end

function table_fill_row(data_tab, selected_rows)
   local out={}
   for _,rowname in ipairs(selected_rows) do
      out[#out+1]=table_data(data_tab[rowname])
   end
   return table_row(table.concat(out, "\n    "))
end

function table_fill_headline(selected_rows)
   local out={}
   for _,rowname in ipairs(selected_rows) do
      out[#out+1]=table_head(rowname)
   end
   return table_row(table.concat(out, "\n    "))
end

--- Convert http request info to a string.
function reqinf_tostr(ri)
   return ([[
request method: %s, uri: %s, http_version: %s, query_string: %s, remote_user: %s, generated on %s]]):format(
      safe_ts(ri.request_method),
      safe_ts(ri.uri),
      safe_ts(ri.http_version),
      safe_ts(ri.query_string),
      safe_ts(ri.remote_user),
      os.date("%A %B %d, %Y, %X"))
end

--- Generate html tables of the known ubx types.
-- @param ni node_info
-- @return string containing html
function typelist_tohtml(ni)
   local table_header, table_footer, elem_per_tab
   local entries={}
   local output={}

   if ubx.num_types(ni) <= 0 then return "" end

   output[#output+1]="<h2>Registered Types</h2>"

   table_header = [[
<table border="0" style="float:left; position:relative;" cellspacing="0" cellpadding="0">
  <tr>
   <th>name</th>
   <th>size [B]</th>
   <th>md5</th>
  </tr> ]]

   table_footer = '</table>'

   -- generate a single table entry and append it to entries
   local function gen_type_entry(t)
      entries[#entries+1]=
	 "<tr><td><tt>"..ffi.string(t.name).."</tt></td><td>"..tonumber(t.size).."</td><td>"..utils.str_to_hexstr(ffi.string(t.hash, 4)).."</td></tr>"
   end

   -- generate list of entries
   ubx.types_foreach(ni, gen_type_entry)

   -- write then out in num_type_cols tables
   elem_per_tab = math.ceil(#entries/num_type_cols)

   local cnt=0

   output[#output+1]=table_header

   while cnt+1 <= #entries do
      if cnt~=0 and cnt % elem_per_tab == 0 then
	 output[#output+1]="\n\n\n"
	 output[#output+1]=table_footer
	 output[#output+1]=table_header
      end
      output[#output+1]=entries[cnt+1]
      cnt=cnt+1
   end

   output[#output+1]=table_footer
   output[#output+1]='<div style="clear:left"></div>'
   return table.concat(output, "\n")
end


--- Generate html tables of the module+license
-- @param ni node_info
-- @return string containing html
function modlist_tohtml(ni)
   local output={}

   local mods =  {}
   ubx.modules_foreach(ni,
		       function (m)
			  mods[#mods+1] = { id=safe_ts(m.id),
				   spdx_license_id=safe_ts(m.spdx_license_id) }
		       end)

   if #mods == 0 then return "" end

   output[#output+1]="<h2>Loaded modules</h2>"

   output[#output+1] = [[
<table border="0" style="float:left; position:relative;" cellspacing="0" cellpadding="0">
  <tr>
   <th>module</th>
   <th>SPDX License ID</th>
  </tr> ]]

   -- generate a single table entry and append it to entries
   local function gen_mod_entry(t)
      output[#output+1] =
	 "<tr><td><tt>"..ffi.string(t.id).."</tt></td><td>"..t.spdx_license_id.."</td></tr>"
   end

   -- generate list of entries
   utils.foreach(gen_mod_entry, mods)

   output[#output+1]= '</table>'
   output[#output+1]= '<div style="clear:left"></div>'

   return table.concat(output, "\n")
end


function colorize_state(t)
   -- t.name=color("blue", t.name)
   if t.state=='preinit' then t.state=color("blue", t.state)
   elseif t.state=='inactive' then t.state=color("red", t.state)
   elseif t.state=='active' then t.state=color("green", t.state) end
   return t
end

--- Registered blocks
function blocklist_tohtml(blklst, header, table_fields)
   local table_header, table_footer, elem_per_tab
   local output={}

   if #blklst <= 0 then return "" end

   table_header = [[
<form method="POST" action="/">
  <table border="0" style="float:left; position:relative;" cellspacing="0" cellpadding="0">
      ]]

   table_footer = [[
</table>
</form>]]

   -- generate colors and state change links
   local function process_state(t)

      local function button(blockname, action)
	 return ('<input type="submit" name="%s" value="%s" class="%s">'):format(blockname, action, action..'button')
      end

      local function add_actions(t)
	 if t.state=='preinit' then t.actions=button(t.name, 'init')
	 elseif t.state=='inactive' then
	    if t.block_type=='cblock' then t.actions=button(t.name, 'steponce') end
	    t.actions=(t.actions or "")..button(t.name, 'start')..button(t.name, 'cleanup')
	 elseif t.state=='active' then
	    if t.block_type=='cblock' then t.actions=button(t.name, 'step') end
	    t.actions=(t.actions or "")..button(t.name, 'stop')
	 end
	 return t
      end

      local function add_block_info_href(t)
	 t.name=a("/block?name="..t.name, t.name)
      end


      add_actions(t)
      colorize_state(t)
      add_block_info_href(t)
      return t
   end
   -- generate a single table entry and append it to entries
   local function gen_block_tab_entry(t)
      output[#output+1]=table_fill_row(process_state(t), table_fields)
   end


   output[#output+1]=h(2, header)
   output[#output+1]=table_header
   output[#output+1]=table_fill_headline(table_fields)

   -- generate list of entries
   utils.foreach(gen_block_tab_entry, blklst)

   output[#output+1]=table_footer
   output[#output+1]='<div style="clear:left"></div>'
   output[#output+1]='<br>'
   return table.concat(output, "\n")
end

local function sysinfo()
   local res={}
   local width, endian, fpu, abi
   abi=""
   if ffi.abi('32bit') then width='32bit' else width='64bit' end
   if ffi.abi('le') then endian='little endian' else endian='big endian' end
   if ffi.abi('fpu') then
      if ffi.abi('softfp') then fpu='softfp'
      elseif ffi.abi('hardfp') then fpu='hardfp' end
   else fpu='no fpu' end
   if ffi.abi('eabi') then abi='eabi'
   elseif ffi.abi('win') then abi='win' end

   res[#res+1] = h(2, "System Information")
   res[#res+1] = ("%s %s %s %s %s %s"):format(ffi.os, ffi.arch, width, endian, fpu, abi)
   return table.concat(res, "\n")
end

--- Stylesheet for the ubx webinterface
stylesheet_str=[[
body{
   background-color: #CCE5F0;
   color: #000000;
   font-size: 14px;
   margin: 10px;
   padding: 0;
   text-align: left;
   font-family: arial, verdana, sans-serif;
   font-weight:normal;
}

h1{ font-size: 24px; color: #000000; font-weight: normal; text-decoration: none; }
h2{ font-size:18px; color:#000000; font-weight:normal; }
a{ }
a:hover{ }

table {
   margin-right: 20px;
   /* margin: auto; */
}

th { padding-right: 20px; }
td { /* border: 0px solid #000000; */ }
td { padding-right:10px; }

.startbutton { background-color:green; color:#fff; }
.stopbutton { background-color:yellow; color:#ffffffff; }
.cleanupbutton { background-color:red; color:white; }
.initbutton { background-color:blue; color:#fff; }
.steponcebutton { background-color:purple; color:#fff; }
.stepbutton { background-color:lawngreen; color:#ffffffff; }

]]

--- These block operations can be invoked via the webinterface.
local block_ops = {
   init=ubx.block_init,
   start=ubx.block_start,
   stop=ubx.block_stop,
   cleanup=ubx.block_cleanup,
   step=function(b)
	   if not ubx.is_cblock_instance(b) then return end
	   ubx.cblock_step(b)
	end,
   steponce=function(b)
	       if not ubx.is_cblock_instance(b) then return end
	       ubx.block_start(b)
	       ubx.cblock_step(b)
	       ubx.block_stop(b)
	    end
}

function handle_post(ni, pd)
   local name, op = string.match(url_decode(pd), "([^=]+)=(.+)")
   local b = ubx.block_get(ni, name)
   if b==nil then error("handle_post: unknown block "..safe_ts(b)) end
   block_ops[op](b)
end



function show_block_JS()
   return
   [[
       <script type="text/javascript">

       function remove_readonly(button_name)
       {
	  document.getElementById("config_"+button_name).removeAttribute("readonly");
	  document.getElementById("button_edit_"+button_name).style.display="none";
	  document.getElementById("button_save_"+button_name).style.display="inline";
       }

       function save()
       {
	  var xxxx=document.getElementById("wert").value;
	  document.getElementById("button-edit").style.display="inline";
	  document.getElementById("button-save").style.display="none";
	  alert('save ' + xxxx);
	  document.getElementById("wert").setAttribute("readonly", "readonly");
       }
     </script>
    ]]
end

--- Compute the dimensions of the
function str_get_dim_2d(s)
   local rows = utils.split(s, "\n")
   local num_rows = #rows
   local num_cols = 0
   utils.foreach(function (l)
		    if #l > num_cols then num_cols=#l end
		 end, rows)
   return num_cols, num_rows
end


--- Show information on a single block.
-- @param ri request info
-- @param ni node infp
function show_block(ri, ni)

   local nodename = safe_ts(ni.name)
   local query_string = safe_ts(ri.query_string)
   local qstab = query_string_to_tab(query_string)
   local blockname = qstab.name or " "
   local b = ubx.block_get(ni, blockname)

   if b==nil then
      local mes = "invalid blockname '"..blockname.."'"
      return html(mes, h(1,mes))
   end

   local bt=ubx.block_totab(b)

   -- gen_row_data
   local function gen_row_data(data, fields)
      local res=""
      for _,x in ipairs(data) do
	 res=res..table_fill_row(x, fields).."\n"
      end
      return res
   end

   -- conf_data_value_changeable_tostr
   local function conf_data_value_changeable_tostr(c)
      local val = utils.trim(utils.tab2str(c.value))
      local cols, rows = str_get_dim_2d(val)

      if cols==0 then cols=1 end
      if rows==0 then rows=1 end
      -- <input type="button" value="save" id="button-save" onclick="save();" style="display:none;">

      c.value = utils.expand(
	 [[
	     <form method="POST" action="/block?$query_string">
	       <input type="button" value="edit" id="button_edit_$confname" onclick='remove_readonly("$confname");'>
	       <input type="submit" id="button_save_$confname" value="save" style="display:none;">
	       <input type="hidden" name="blockname" value="$blockname" >
	       <input type="hidden" name="confname" value="$confname" >
	       <textarea id="config_$confname" name="confval" cols="$cols" rows="$rows" $readonly>$value</textarea>
	     </form> ]],
	 { blockname=blockname, confname=c.name, query_string=query_string, cols=cols, rows=rows, readonly='readonly', value=val}, true )
   end

   -- convert port in and out attrs to strings
   local function attrs_to_str(p)
      local res = {}
      if bit.band(p.attrs, ffi.C.PORT_DIR_IN) > 0 then res[#res+1]=ubx.port_attrs_tostr[ffi.C.PORT_DIR_IN] end
      if bit.band(p.attrs, ffi.C.PORT_DIR_OUT) > 0 then res[#res+1]=ubx.port_attrs_tostr[ffi.C.PORT_DIR_OUT] end
      p.attrs=table.concat(res, ", ")
   end

   -- unset
   local function rm_unused_data_len(p)
      if p.in_type_name=="" then p.in_data_len="" end
      if p.out_type_name=="" then p.out_data_len="" end
   end

   local function conf_data_value_tostr(c)
      c.value=utils.trim(utils.tab2str(c.value))
   end

   if bt.state == 'active' or bt.prototype=="<prototype>" then
      utils.foreach(conf_data_value_tostr, bt.configs)
   else
      utils.foreach(conf_data_value_changeable_tostr, bt.configs)
   end

   utils.foreach(attrs_to_str, bt.ports)
   utils.foreach(rm_unused_data_len, bt.ports)

   local port_fields={ 'name', 'attrs', 'in_type_name', 'in_data_len', 'out_type_name', 'out_data_len', 'doc' }
   local conf_fields={ 'name', 'type_name', 'value', 'doc' }

   colorize_state(bt)

   return html(
      "ubx node: "..nodename,
      show_block_JS(),
      h(1, "ubx_node: "..a("/", nodename)..":"..blockname),
      "state: "..bt.state,
      h(2, "Ports"),
      "<table>",
      table_fill_headline(port_fields),
      gen_row_data(bt.ports, port_fields),
      "</table>",
      -- ubx.ports_tostr(bt.ports),

      "<br>",
      h(2, "Configuration"),
      "<table>",
      table_fill_headline(conf_fields),
      gen_row_data(bt.configs, conf_fields),
      "</table>", "<br>",
      -- ubx.configs_tostr(bt.configs),
      reqinf_tostr(ri))
end

local protoblocks_table_fields = { 'name', 'block_type' }
local cblock_table_fields = { 'name', 'state', 'prototype', 'stat_num_steps', 'actions' }
local iblock_table_fields = { 'name', 'state', 'prototype', 'stat_num_reads', 'stat_num_writes', 'actions' }

--- master dispatch table.
dispatch_table = {
   -- Root node overview
   ["/"] = function(ri, ni, postdata)
	      if postdata then handle_post(ni, postdata) end
	      local nodename = safe_ts(ni.name)
	      local protoblocks = ubx.blocks_map(ni, ubx.block_totab, ubx.is_proto)
	      local cinst = ubx.blocks_map(ni, ubx.block_totab, ubx.is_cblock_instance)
	      local iinst = ubx.blocks_map(ni, ubx.block_totab, ubx.is_iblock_instance)

	      table.sort(protoblocks, function (one,two) return one.block_type<two.block_type end)
	      table.sort(cinst, function (one,two) return one.name<two.name end)
	      table.sort(iinst, function (one,two) return one.name<two.name end)

	      return html(
		 "ubx node: "..nodename,
		 a("/node", "node graph"),
		 h(1, "ubx_node: "..a("/", nodename)),
		 blocklist_tohtml(cinst, "Computational Blocks", cblock_table_fields),
		 blocklist_tohtml(iinst, "Interaction Blocks", iblock_table_fields),
		 blocklist_tohtml(protoblocks, "Prototype Blocks", protoblocks_table_fields),
		 typelist_tohtml(ni), "<br>",
		 modlist_tohtml(ni), "<br>",
		 sysinfo(), "<br><br>",
		 reqinf_tostr(ri))
	   end,
   -- Block overview
   ["/block"] = function(ri, ni, postdata)
      if postdata then
	 local t = query_string_to_tab(postdata)

	 local res, msg = pcall(ubx.set_config_str, ubx.block_get(ni, t.blockname), t.confname, t.confval)

	 if not res then
	    return html("Error",
			h(1, "Configuration error"),
			"Failed to apply config "..t.confname..", value: "..t.confval.."<br>"..msg)
	 end
      end
      return show_block(ri, ni)
   end,

   ["/node"] = function(ri, ni, postdata)
      local nodename = safe_ts(ni.name)
      local dotstr=ubx.node_todot(ni)
      local scrpt = [[
function src(id) {
   return document.getElementById(id).innerHTML;
}

document.body.innerHTML += Viz(src("dotstr"), "svg")
]]

      return html(
	 "Node graph: "..nodename,
	 h(1, "Node graph: "..a("/", nodename)),
	 script("", {src="https://cdn.rawgit.com/mdaines/viz.js/7a58912fa07cbe2a5f3cd8c2b4f5cf48b9dca7d8/viz.js"}),
	 script(dotstr, { type="text/vnd.graphviz", id="dotstr" }),
	 script(scrpt, { language="javascript", type="text/javascript"})
		 )
   end,

   ["/style.css"]=function() return stylesheet_str, "text/css" end,

   ["/jsonfile.json"]=function() return '{ "foo":1, "bar":"haugasd" }' end
}


function request_handler(node_info, request_info_lud, postdata)
   local reqinf = ffi.cast("struct mg_request_info*", request_info_lud)
   local ni = ffi.cast("struct ubx_node_info*", node_info)

   local uri = safe_ts(reqinf.uri)
   local handler = dispatch_table[uri]

   ubx.ffi_load_types(ni)

   -- print("requesting uri", uri)

   if handler then return handler(reqinf, ni, postdata) end

   print("no handler found for uri", uri)
   return "<h1>no handler found</h1>"..reqinf_tostr(reqinf)
end

print("loaded request_handler()")
