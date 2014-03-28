--
-- Useful code snips
-- to parse ubx environment
--

local os = os

module('ubx_env')

--- Extract the UBX_ROOT environment variable path
-- @return string UBX_ROOT_path
function get_ubx_root()
  local rootpath = os.getenv("UBX_ROOT")
  if rootpath == nil then
    return ""
  end

  return rootpath.."/"
end

--- Extract the UBX_MODULES environment variable path
-- @return string UBX_MODULES path
function get_ubx_modules()
  local path = os.getenv("UBX_MODULES")
  if path == nil then
    return ""
  end

  return path.."/"
end