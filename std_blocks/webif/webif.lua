local ffi=require"ffi"
local u5c=require"u5c"
local utils=require"utils"

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


function safe_tostring(charptr)
   if charptr == nil then return "" end
   return ffi.string(charptr)
end

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
      safe_tostring(ri.request_method),
      safe_tostring(ri.uri),
      safe_tostring(ri.http_version),
      safe_tostring(ri.query_string),
      safe_tostring(ri.remote_user),
      os.date("%A %B %d, %Y, %X"))
end

function u5c_overview_tohtml(ni)
   local output={}
   output[#output+1]=
      [[
<h2>Registered types</h2>
<table border="0" cellspacing="0" cellpadding="0">
  <tr>
   <th>name</th>
   <th>size</th>
  </tr>
      ]]

   u5c.types_foreach(ni,
		     function(t)
			output[#output+1]=
			   "<tr><td><tt>"..ffi.string(t.name).."</tt></td><td>"..tostring(t.size).."</td></tr>"
		     end)

   output[#output+1]="</table><br><br>"

   
   
   return table.concat(output, "\n")
end

--h1 { color:red; font-size:48px; }
stylesheet_str=[[
body{
	background: #ffffff;
	color: #000000;
	font-size: 14px;
	margin: 0;
	padding: 0;
	text-align: left;
	font-family: arial, verdana, sans-serif;
	font-weight:normal;
}

h1{
	font-size: 32px;
	color: #000000;
	font-weight: normal;
	text-decoration: none;
}

h2{
	font-size:24px;
	color:#000000;
	font-weight:normal;
}

a{
}

a:hover{
}

table,td,th {
   /* border: 0px solid #000000; */
}

td{
   padding-right:10px;
}

]]

-- master dispatch table
dispatch_table = {
   ["/"] = function(ri, ni)
	      return response:format(u5c_overview_tohtml(ni)..reqinf_tostr(ri))
	   end,

   -- ["/reload"]=function() return "__reload__" end,
   ["/style.css"]=function() return stylesheet_str, "text/css" end,
}


function request_handler(node_info, request_info_lud)
   local reqinf = ffi.cast("struct mg_request_info*", request_info_lud)
   local ni = ffi.cast("struct u5c_node_info*", node_info)

   local uri = safe_tostring(reqinf.uri)
   local handler = dispatch_table[uri]

   print("requesting uri", uri)
   if handler then return handler(reqinf, ni) end

   print("no handler found for uri", uri)
   return "<h1>no handler found</h1>"..reqinf_tostr(reqinf)
end

print("loaded request_handler()")