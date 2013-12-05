-- strict.lua from the metalua project.
--
-- Permission is hereby granted, free of charge, to any person
-- obtaining a copy of this software and associated documentation
-- files (the "Software"), to deal in the Software without
-- restriction, including without limitation the rights to use, copy,
-- modify, merge, publish, distribute, sublicense, and/or sell copies
-- of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be
-- included in all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
-- EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
-- MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
-- NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
-- LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
-- OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
-- WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

--
-- checks uses of undeclared global variables
-- All global variables must be 'declared' through a regular assignment
-- (even assigning nil will do) in a main chunk before being used
-- anywhere or assigned to inside a function.
--

local mt = getmetatable(_G)
if mt == nil then
   mt = {}
   setmetatable(_G, mt)
end

__STRICT = true
mt.__declared = {}

mt.__newindex = function (t, n, v)
		   if __STRICT and not mt.__declared[n] then
		      local w = debug.getinfo(2, "S").what
		      if w ~= "main" and w ~= "C" then
			 error("assign to undeclared variable '"..tostring(n).."'", 2)
		      end
		      mt.__declared[n] = true
		   end
		   rawset(t, n, v)
		end

mt.__index = function (t, n)
		if not mt.__declared[n] and debug.getinfo(2, "S").what ~= "C" then
		   error("variable '"..tostring(n).."' is not declared", 2)
		end
		return rawget(t, n)
	     end

function global(...)
   for _, v in ipairs{...} do mt.__declared[v] = true end
end


