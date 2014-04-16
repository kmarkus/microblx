--
-- Useful code snips to parse ubx environment
-- Copyright (C) 2014 Enea Scioni <enea.scioni@unife.it>
--                             <enea.scioni@kuleuven.be>

local os = os
local utils = require "utils"

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

--- Extract the UBX_MODELS environment variable path
-- @return string UBX_MODELS path
function get_ubx_models()
  local path = os.getenv("UBX_MODELS")
  if path == nil then
    return ""
  end

  return path.."/"
end

--- fetch a model file, given the name and the type
-- @param bm model name
-- @param ext model type/extension
-- @return string to model file
local function fetch_model(bm,ext)
  -- looking into: relative path, models folder (if pkg),
  -- UBX_MODELS
  if utils.file_exists(bm.."."..ext) then
    return bm.."."..ext
  elseif utils.file_exists("models/"..bm.."."..ext) then
    return "models/"..bm.."."..ext
  elseif get_ubx_models() and utils.file_exists(get_ubx_models()..bm.."."..ext) then
    return get_ubx_models()..bm.."."..ext
  end
  return false;
end

--- fetch a block model file
-- @param blx block model name
-- @return string to model file
function fetch_block_model(blx)
  return fetch_model(blx,"blx")
end

--- fetch a block model file
-- @param pkg package model name
-- @return string to model file
function fetch_pkg_model(pkg)
  return fetch_model(pkg,"pkg")
end
