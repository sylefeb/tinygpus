-- _____________________________________________________________________________
-- |                                                                           |
-- |  Silice preprocessor script to extract WAD data                           |
-- |  ==============================================                           |
-- |                                                                           |
-- | Prepares a texture pack and level structure header for DMC-1 demos        |
-- |                                                                           |
-- | Texture processing functions                                              |
-- |                                                                           |
-- | @sylefeb 2020-05-01  licence: GPL v3, see full text in repo               |
-- |___________________________________________________________________________|

-- -------------------------------------
-- finds closest color in Doom palette
function find_closest(rgb)
  local r = rgb&255
  local g = (rgb>>8)&255
  local b = (rgb>>16)&255
  local dist = 999999999
  local best = 0
  for i,cmp in ipairs(palette) do
    local cmp_r = cmp&255
    local cmp_g = (cmp>>8)&255
    local cmp_b = (cmp>>16)&255
    local d = math.max(
                math.max((cmp_r-r)^2,
                         (cmp_g-g)^2),
                (cmp_b-b)^2
              )
    if d < dist then
      dist = d
      best = i
    end
  end
  return best-1
end

-- https://stackoverflow.com/questions/4990990/check-if-a-file-exists-with-lua
function file_exists(name)
  local f=io.open(name,"r")
  if f~=nil then io.close(f) return true else return false end
end

-- -------------------------------------
-- helper functions
function texture_dim_pow2(dim)
  local pow2=0
  local tmp = dim
  while tmp > 1 do
    pow2 = pow2 + 1
    tmp  = (tmp>>1)
  end
  if dim > (1<<pow2) then
    pow2 = pow2 + 1
  end
  return pow2,(dim == (1<<pow2))
end

function transpose_tex(img)
  local w = #img[1]
  local h = #img
  local trsp = {}
  for j = 1,w do
    trsp[j] = {}
    for i = 1,h do
      trsp[j][i] = img[i][j]
    end
  end
  return trsp
end

function tex_vflip(img)
  local w = #img[1]
  local h = #img
  local flip = {}
  for j = 1,h do
    flip[j] = {}
    for i = 1,w do
      flip[j][i] = img[h-j+1][i]
    end
  end
  return flip
end

function cache_encode_flat(img)
  local w = #img[1]
  local h = #img
  local bsz = 4 -- block siwe
  tmp = {}
  for j = 0 , (h//bsz) - 1 do
    for i = 0, (w//bsz) - 1 do
      for bj = 0 , bsz - 1 do
        for bi = 0 , bsz - 1 do
          oi = i*bsz + bi
          oj = j*bsz + bj
          tmp[1+#tmp] = img[1+oj][1+oi]
        end
      end
    end
  end
  local enc = {}
  for j = 0,h-1 do
    enc[1+j] = {}
    for i = 0,w-1 do
      enc[1+j][1+i] = tmp[1 + i + j * w]
    end
  end
  return enc
end

function pad_tex(img,transparent)
  local w = #img[1]
  local h = #img
  local wp2,_ = texture_dim_pow2(w)
  local hp2,_ = texture_dim_pow2(h)
  -- print('padding ' .. w .. 'x' .. h .. ' as ' .. (1<<wp2) .. 'x' .. (1<<hp2))
  local padded = {}
  for j = 1,(1<<hp2) do
    padded[j] = {}
    for i = 1,(1<<wp2) do
      if i <= w and j <= h then
        padded[j][i] = img[j][i]
      else
        if transparent then
          padded[j][i] = 255
        else
          padded[j][i] = 0
        end
      end
    end
  end
  return padded
end

function gettex(tex)
  local nfo = texture_ids[tex]
  local td,truew,trueh
  if nfo.type == 'wall' then
    td = get_image_as_table(path .. 'textures/assembled/' .. tex:upper() .. '.tga')
    truew = #td[1]
    trueh = #td
    if not standalone_pack then
      td = tex_vflip(td)
    end
    td = pad_tex(td)
  elseif nfo.type == 'flat' then
    td = decode_flat_lump(path .. 'lumps/flats/' .. tex .. '.lump')
    truew = #td[1]
    trueh = #td
  elseif nfo.type == 'sprite' then
    td = get_image_as_table(path .. 'textures/sprites/' .. tex .. '.tga')
    truew = #td[1]
    trueh = #td
    if not standalone_pack then
      td = tex_vflip(td)
    end
    td = pad_tex(td,1)
  elseif nfo.type == 'weapon' then
    td = get_image_as_table(path .. 'textures/weapons/' .. tex .. '.tga')
    truew = #td[1]
    trueh = #td
  elseif nfo.type == 'terrain' then
    td = get_image_as_table(path .. 'textures/assembled/' .. tex .. '.tga')
    truew = #td[1]
    trueh = #td
  end
  if not td then
    error('could not load ' .. tex)
  end
  return td,truew,trueh
end

-- -------------------------------------
-- parse pnames
extract_lump('PNAMES')
local in_pnames = assert(io.open(findfile('lumps/PNAMES.lump'), 'rb'))
local num_pnames = string.unpack('I4',in_pnames:read(4))
pnames={}
for p=1,num_pnames do
  local name = in_pnames:read(8):match("[%_-%a%d]+")
  pnames[p-1] = name
end
in_pnames:close()

-- -------------------------------------
-- parse texture defs
function parse_textures(texdeflump)
  local in_texdefs = io.open(findfile('lumps/' .. texdeflump), 'rb')
  if in_texdefs == nil then
    return -- not all WADs contain TEXTURE2, for instance
  end
  local imgcur = nil
  local imgcur_w = 0
  local imgcur_h = 0
  local sz_read = 0
  local num_texdefs = string.unpack('I4',in_texdefs:read(4))
  local texdefs_seek={}
  for i=1,num_texdefs do
    texdefs_seek[i] = string.unpack('I4',in_texdefs:read(4))
  end

  for i=1,num_texdefs do
    local name = in_texdefs:read(8):match("[%_-%a%d]+")
    in_texdefs:read(2) -- skip
    in_texdefs:read(2) -- skip
    local w = string.unpack('H',in_texdefs:read(2))
    local h = string.unpack('H',in_texdefs:read(2))
    in_texdefs:read(2) -- skip
    in_texdefs:read(2) -- skip
    -- start new
    print('wall texture ' .. name .. ' ' .. w .. 'x' .. h)
    imgcur = {}
    for j=1,h do
      imgcur[j] = {}
      for i=1,w do
        imgcur[j][i] = -1
      end
    end
    -- tex is opaque?
    local npatches = string.unpack('H',in_texdefs:read(2))
    -- copy patches
    for p=1,npatches do
      local x   = string.unpack('h',in_texdefs:read(2))
      local y   = string.unpack('h',in_texdefs:read(2))
      local pid = string.unpack('H',in_texdefs:read(2))
      pname = nil
      if pnames[pid] then
        pname = pnames[pid]
        print('   patch "' .. pname .. '" id=' .. pid)
        print('     x:  ' .. x)
        print('     y:  ' .. y)
      end
      in_texdefs:read(2) -- skip
      in_texdefs:read(2) -- skip
      if pname then
        print('   loading patch ' .. pname)
        local pimg = decode_patch_lump(path .. 'lumps/patches/' .. pname:upper() .. '.lump')
        local ph = #pimg
        local pw = #pimg[1]
        print('   patch is ' .. pw .. 'x' .. ph)
        for j=1,ph do
          for i=1,pw do
             if ((j+y) <= #imgcur) and ((i+x) <= #imgcur[1]) and (j+y) > 0 and (i+x) > 0 then
               if pimg[j][i] > -1 then -- -1 is transparent
                 imgcur[math.floor(j+y)][math.floor(i+x)] = pimg[j][i]
               end
             end
          end
        end
        print('   copied.')
      else
        error('cannot find patch ' .. pid)
      end
    end
    local opaque = 1
    local uses_255 = 0
    for j=1,h do
      for i=1,w do
        if imgcur[j][i] == 255 then
          uses_255 = 1
        end
        if imgcur[j][i] == -1 then
          opaque = 0
          imgcur[j][i] = 255 -- replace by transparent marker
        end
      end
    end
    textures_names[1 + #textures_names] = name
    textures_opacity[name] = opaque
    -- save
    print('saving ' .. name .. ' ...')

    save_table_as_image_with_palette(imgcur,palette,path .. 'textures/assembled/' .. name:upper() .. '.tga')
    print('         ... done.')

    if opaque == 0 and uses_255 == 1 then
      print('WARNING: transparent marker used in texture')
    end
  end
end

-- -------------------------------------
-- returns texture flags (8 bits)

function texture_flags(tex)
  local flags = 0
  local nfo   = texture_ids[tex]
  local wp2,_ = texture_dim_pow2(nfo.texw)
  local hp2,_ = texture_dim_pow2(nfo.texh)
  -- NOTE: 32 is used for frontmost (eg weapons)
  if nfo.type == 'flat' then
    flags = flags | 64 -- floor/celing encoding (cache)
  end
  if (1<<wp2) == nfo.texw then
    flags = flags | 16 -- pow2 in width
  end
  if (1<<hp2) == nfo.texh then
    flags = flags | 8  -- pow2 in height
  end
  if switch_off_ids[nfo.id] then
    flags = flags | 4
  end
  if switch_on_ids[nfo.id] then
    flags = flags | 2
  end
  opaque = 1
  if textures_opacity[tex] then
    if textures_opacity[tex] == 0 then
      opaque = 0
    end
  end
  flags = flags | opaque
  return flags
end

-- -------------------------------------
-- parse pnames
local in_pnames = assert(io.open(findfile('lumps/PNAMES.lump'), 'rb'))
local num_pnames = string.unpack('I4',in_pnames:read(4))
pnames={}
for p=1,num_pnames do
  local name = in_pnames:read(8):match("[%_-%a%d]+")
  pnames[p-1] = name
end
in_pnames:close()
