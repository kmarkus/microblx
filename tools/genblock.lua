utils=require "utils"

function usage()
   print( [=[
genblock: generate the code for an empty microblx block.

usage genblock OPTIONS <conf file>
   -n           name of block
   -d		output directory (must exist)
   -h           show this.
]=])
end

local opttab=utils.proc_args(arg)

if #arg==1 or opttab['-h'] then usage(); os.exit(1) end

if not (opttab['-n'] and opttab['-n'][1]) then
   print("missing name (-n)"); os.exit(1) 
end
local blockname = opttab['-n'][1]

local outdir="."
if opttab['-d'] then
   outdir=opttab['-d'][1]
   if not utils.file_exists(outdir) then
      if os.system("mkdir -p "..outdir) ~= 0 then
	 print("creating dir "..outdir.." failed")
	 os.exit(1)
      end
   end
end



local res, str = utils.preproc(
[[
/*
 * $(blockname)
 */



]], { table=table, blockname=blockname } )

print(str)