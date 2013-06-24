local ffi=require"ffi"
local u5c=require"u5c"

local safe_ts = u5c.safe_tostr

-- Configuration
num_type_cols=3

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

--- Standard response template
response=[[
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN">
<html lang="en">
<head>
 <title>u5C webinterface</title>
 <link rel="stylesheet" type="text/css" href="style.css">
</head>

<body>
<h1>u5C webinterface</h1>
%s
</body>
</html>
]]

function html(...) return response:format(table.concat({...}, "\n")) end

function reqinf_tostr(ri)
   return ([[
request method: %s <br>
uri:            %s <br>
http_version:   %s <br>
query_string:   %s <br>
remote_user:    %s <br>
<br>
<br>
generated on %s
   ]]):format(
      safe_ts(ri.request_method),
      safe_ts(ri.uri),
      safe_ts(ri.http_version),
      safe_ts(ri.query_string),
      safe_ts(ri.remote_user),
      os.date("%A %B %d, %Y, %X"))
end

--- Generate html tables of the known u5c types.
-- @param ni node_info
-- @return string containing html
function typelist_tohtml(ni)
   local table_header, table_footer, elem_per_tab
   local entries={}
   local output={}

   output[#output+1]="<h2>Registered types</h2>"

   table_header = [[
<table border="0" style="float:left; position:relative;" cellspacing="0" cellpadding="0">
  <tr>
   <th>name</th>
   <th>size</th>
  </tr> ]]
   
   table_footer = '</table>'

   -- generate a single table entry and append it to entries
   local function gen_type_entry(t)
      entries[#entries+1]=
	 "<tr><td><tt>"..ffi.string(t.name).."</tt></td><td>"..tostring(t.size).."</td></tr>"
   end

   -- generate list of entries
   u5c.types_foreach(ni, gen_type_entry)

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


--- Registered blocks
function blocklist_tohtml(ni)
   
end


--- Stylesheet for the u5c webinterface
stylesheet_str=[[
body{
   background-color: #CCE5F0;
   color: #000000;
   font-size: 14px;
   margin: 0;
   padding: 0;
   text-align: left;
   font-family: arial, verdana, sans-serif;
   font-weight:normal;
}

h1{ font-size: 32px; color: #000000; font-weight: normal; text-decoration: none; }
h2{ font-size:24px; color:#000000; font-weight:normal; }
a{ }
a:hover{ }

table {
   margin: 20px;
}

td,th { /* border: 0px solid #000000; */ }
td{ padding-right:10px; }

]]

--- master dispatch table.
-- This 
dispatch_table = {
   ["/"] = function(ri, ni)
	      return html(typelist_tohtml(ni), reqinf_tostr(ri))
	   end,

   ["/style.css"]=function() return stylesheet_str, "text/css" end,
}


function request_handler(node_info, request_info_lud)
   local reqinf = ffi.cast("struct mg_request_info*", request_info_lud)
   local ni = ffi.cast("struct u5c_node_info*", node_info)

   local uri = safe_ts(reqinf.uri)
   local handler = dispatch_table[uri]

   print("requesting uri", uri)
   if handler then return handler(reqinf, ni) end

   print("no handler found for uri", uri)
   return "<h1>no handler found</h1>"..reqinf_tostr(reqinf)
end

print("loaded request_handler()")