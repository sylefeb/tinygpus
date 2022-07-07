-- _____________________________________________________________________________
-- |                                                                           |
-- |  Silice preprocessor script to extract WAD data                           |
-- |  ==============================================                           |
-- |                                                                           |
-- | Prepares a texture pack and level structure header for DMC-1 demos        |
-- |                                                                           |
-- | This file implements a WAD lump extractor                                 |
-- |                                                                           |
-- | @sylefeb 2020-05-01  licence: GPL v3, see full text in repo               |
-- |___________________________________________________________________________|

-- -------------------------------------
-- helper for file size
function fsize(file)
  local start = file:seek()
  local size  = file:seek("end")
  file:seek("set", start)
  return size
end

-- -------------------------------------
-- decode a Doom sound into a wave table
function decode_sound_lump(name)
  -- open lump file
  local in_lump  = assert(io.open(findfile(name), 'rb'))
  local three    = string.unpack('H',in_lump:read(2))
  local freq     = string.unpack('H',in_lump:read(2))
  local nsmpls   = string.unpack('H',in_lump:read(2))
  local zero     = string.unpack('H',in_lump:read(2))
  print('loading sound ' .. name .. ' frequency is ' .. freq .. ' #samples: ' .. nsmpls)
  local wave     = {}
  for i=1,nsmpls do
    wave[i] = string.unpack('B',in_lump:read(1))
  end
  return wave
end

-- -------------------------------------
-- load a raw 8 bit PCM
function load_raw_pcm_8bits(name,maxsz)
  -- open lump file
  local in_pcm   = assert(io.open(findfile(name), 'rb'))
  local sz       = fsize(in_pcm)
  if maxsz then
    sz = math.min(sz,maxsz)
  end
  print('loading PCM 8 bits ' .. name .. ' ' .. sz .. ' bytes')
  local wave     = {}
  for i=1,sz do
    wave[i] = string.unpack('b',in_pcm:read(1))
  end
  return wave
end

-- -------------------------------------
-- decode a Doom patch into an image table
function decode_patch_lump(name)
  -- open lump file
  local in_patch = assert(io.open(findfile(name), 'rb'))
  local pw = string.unpack('H',in_patch:read(2))
  local ph = string.unpack('H',in_patch:read(2))
  img={}
  for j = 1,ph do
    img[j]={}
    for i = 1,pw do
      img[j][i] = -1 -- transparent
    end
  end
  local lo = string.unpack('h',in_patch:read(2))  -- left offset
  local to = string.unpack('h',in_patch:read(2))  -- top offset
  local colptrs={}
  for i=1,pw do
    colptrs[i] = string.unpack('I4',in_patch:read(4))
  end
  -- read posts
  local opaque = 1
  for i=1,pw do
    local last = 0
    while true do
      local rstart = string.unpack('B',in_patch:read(1))
      if rstart == 255 then
        break
      end
      if rstart > last + 1 then
        opaque = 0
      end
      local num    = string.unpack('B',in_patch:read(1))
      in_patch:read(1) -- skip
      for n=1,num do
        img[1+rstart][i] = string.unpack('B',in_patch:read(1))
        rstart = rstart + 1
      end
      in_patch:read(1) -- skip
    end
  end
  in_patch:close()
  return img,opaque,lo,to
end

-- -------------------------------------
-- decode a Doom flat into an image table
function decode_flat_lump(name)
  local pw = 64
  local ph = 64
  print('decode_flat_lump ' .. name)
  local in_flat = assert(io.open(findfile(name), 'rb'))
  img={}
  for j = 1,ph do
    img[j]={}
    for i = 1,pw do
      img[j][i] = string.unpack('B',in_flat:read(1))
    end
  end
  in_flat:close()
  return img
end

lump_level = {
LINEDEFS='LINEDEFS',
NODES='NODES',
SECTORS='SECTORS',
SEGS='SEGS',
SIDEDEFS='SIDEDEFS',
SSECTORS='SSECTORS',
THINGS='THINGS',
VERTEXES='VERTEXES',
}

lump_misc = {
'COLORMAP',
'PLAYPAL',
'PNAMES',
'TEXTURE1',
}

lump_opt = {
'TEXTURE2',
}

-- -------------------------------------
-- extracts a lump
function extract_lump(name,dir,optional)
  if not dir then
    dir = ''
  end
  name = name:upper()
  -- open wad
  local in_wad = assert(io.open(findfile(wad), 'rb'))
  -- print('extracting lump ' .. name)
  if lumps[name] then
    in_wad:seek("set", lumps[name].start)
    local data = in_wad:read(lumps[name].size)
    local out_lump = assert(io.open(path .. '/lumps/' .. dir .. name .. '.lump', 'wb'))
    out_lump:write(data)
    out_lump:close()
  else
    if optional then
      print('optional lump ' .. name .. ' not found')
    else
      error('lump ' .. name .. ' not found!')
    end
  end
  in_wad:close()
end

-- -------------------------------------

local in_wad = assert(io.open(findfile(wad), 'rb'))
local sz_wad = fsize(in_wad)

local id = string.unpack('c4',in_wad:read(4))
if id ~= 'IWAD' and  id ~= 'PWAD' then
  error('not a WAD file')
end

local nlumps = string.unpack('I4',in_wad:read(4))
print('WAD file contains ' .. nlumps .. ' lumps')

local diroffs = string.unpack('I4',in_wad:read(4))
-- read directory
in_wad:seek("set", diroffs)
local level_prefix = ''
local in_flats = 0
local in_patches = 0
local in_sprites = 0
lumps={}
lumps_flats={}
lumps_patches={}
lumps_sprites={}
for l=1,nlumps do
  local start = string.unpack('I4',in_wad:read(4))
  local size  = string.unpack('I4',in_wad:read(4))
  local name  = string.unpack('c8',in_wad:read(8)):match("[%_-%a%d]+")
  if string.match(name,'E%dM%d') then
    print('level ' .. name)
    level_prefix = name
  end
  if lump_level[name] then
    name = level_prefix .. '_' .. name
  end
  lumps[name] = { start=start, size=size }
  -- print(' - lump "' .. name .. '" [' .. start .. '] ' .. size)
  -- track sections end tokens
  if string.match(name,'F_END') then
    in_flats = 0
  end
  if string.match(name,'F1_END') then
    in_flats = 0
  end
  if string.match(name,'P1_END') then
    in_patches = 0
  end
  if string.match(name,'P2_END') then
    in_patches = 0
  end
  if string.match(name,'S_END') then
    in_sprites = 0
    in_patches = 0 -- freedoom uses some sprites as patches ...
  end
  -- record flats
  if in_flats == 1 then
    lumps_flats[1+#lumps_flats] = { name=name, start=start, size=size }
    extract_lump(name,'flats/')
  end
  -- record patches
  if in_patches == 1 then
    lumps_patches[1+#lumps_patches] = { name=name, start=start, size=size }
    extract_lump(name,'patches/')
  end
  -- record sprites
  if in_sprites == 1 then
    lumps_sprites[1+#lumps_sprites] = { name=name, start=start, size=size }
    extract_lump(name,'sprites/')
  end
  -- sounds
  if string.match(name,'DS.*') then
    -- lumps_patches[1+#lumps_patches] = { name=name, start=start, size=size }
    -- extract_lump(name,'sounds/')
  end
  -- font
  if string.match(name,'STCFN.*') then
    -- lumps_patches[1+#lumps_patches] = { name=name, start=start, size=size }
    -- extract_lump(name,'font/')
  end
  -- track sections start tokens
  if string.match(name,'F_START') then
    in_flats = 0
  end
  if string.match(name,'F1_START') then
    in_flats = 1
  end
  if string.match(name,'P1_START') then
    in_patches = 1
  end
  if string.match(name,'P2_START') then
    in_patches = 1
  end
  if string.match(name,'S_START') then
    in_sprites = 1
    in_patches = 1 -- freedoom uses some sprites as patches ...
  end
end

in_wad:close()

-- -------------------------------------
-- extract level lump
for _,lmp in pairs(lump_level) do
  extract_lump(level .. '_' .. lmp)
end

-- -------------------------------------
-- colormap
local in_clrmap = assert(io.open(findfile('lumps/COLORMAP.lump'), 'rb'))
local sz = fsize(in_clrmap)
print('colormap file is ' .. sz .. ' bytes')
colormaps={}
for i=1,32 do
  local map = {}
  for c=1,256 do
    local palidx = string.unpack('B',in_clrmap:read(1))
    map[c] = palidx
  end
  colormaps[i] = map
end

-- -------------------------------------
-- palette
local in_pal = assert(io.open(findfile('lumps/PLAYPAL.lump'), 'rb'))
local sz = fsize(in_pal)
print('palette file is ' .. sz .. ' bytes')
palette={}
inv_palette={}
palette_666={}
for c=1,256 do
  local r    = string.unpack('B',in_pal:read(1))
  local g    = string.unpack('B',in_pal:read(1))
  local b    = string.unpack('B',in_pal:read(1))
  local rgb     = r + (g*256) + (b*256*256)
  local rgb_666 = (b>>2) + (g>>2)*64 + (r>>2)*64*64
  palette_666[c] = rgb_666
  palette[c] = rgb
  inv_palette[rgb] = c
end
in_pal:close()

-- write palette for design

local out = assert(io.open(path .. '../build/palette666.si', "w"))
out:write('$$palette666 = \'')
for c=1,256 do
  out:write(palette_666[c] .. ',')
end
out:write('\'\n')
out:close()
