-- _____________________________________________________________________________
-- |                                                                           |
-- |  Silice preprocessor script to extract WAD data                           |
-- |  ==============================================                           |
-- |                                                                           |
-- | Prepares a texture pack and level structure header for DMC-1 demos        |
-- |                                                                           |
-- | Some utility functions                                                    |
-- |                                                                           |
-- | @sylefeb 2020-05-01  licence: GPL v3, see full text in repo               |
-- |___________________________________________________________________________|

-- detect current path
path,_1,_2 = string.match(findfile('pre_utils.lua'), "(.-)([^\\/]-%.?([^%.\\/]*))$")
print('PATH is ' .. path)

-- helper for file size
function fsize(file)
  local start = file:seek()
  local size  = file:seek("end")
  file:seek("set", start)
  return size
end
