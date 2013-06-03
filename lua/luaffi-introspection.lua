--- This one is a hack!

local ffi = require 'ffi'

local defs = [[
struct A {
   int16_t a;
   int16_t b;
   char c[7];
} __attribute__((packed));

struct B {
   char a[6];
   int16_t b;
   int16_t c;
   int8_t d;
} __attribute__((packed));
]]

ffi.cdef(defs)

local member_candidates = {}
for w in string.gmatch(defs, '%w+') do
   member_candidates[w] = true
end

local memorized_members = {}

local function members(obj, ct)
   ct = ct or ffi.typeof(obj)

   local f = memorized_members[ct]
   if not f then
      local om = {} -- {{offset, "member"}, ...}
      for m,_ in pairs(member_candidates) do
	 local o = ffi.offsetof(ct, m)
	 if o then table.insert(om, {o, m}) end
      end

      table.sort(om, function(a, b) return a[1] < b[1] end)

      local mms = {}
      for i = 1, #om do
	 mms[i] = om[i][2]
      end

      f = function(obj)
	     local i = 0
	     return function()
		       i = i + 1
		       if i <= #mms then
			  local m = mms[i]
			  return m, obj[m]
		       end
		    end
	  end

      memorized_members[ct] = f
   end

   return f(obj)
end

local pa = ffi.cast('struct A *', 'abracadabra')
local pb = ffi.cast('struct B *', 'abracadabra')

print('struct A:')
for k,v in members(pa[0]) do
   print(k,v)
end

print('struct B:')
for k,v in members(pb[0]) do
   print(k,v)
end