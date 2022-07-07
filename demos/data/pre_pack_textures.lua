-- _____________________________________________________________________________
-- |                                                                           |
-- |  Silice preprocessor script to extract WAD data                           |
-- |  ==============================================                           |
-- |                                                                           |
-- | Prepares a texture pack and level structure header for DMC-1 demos        |
-- |                                                                           |
-- | Writes the texture pack to disk                                           |
-- |                                                                           |
-- | IMPORTANT: texture_data_offset has to be set to the texture pack          |
-- |            resident address in memory (avoids a 24 bits adder in hw)      |
-- |                                                                           |
-- | @sylefeb 2020-05-01  licence: GPL v3, see full text in repo               |
-- |___________________________________________________________________________|

print('preparing texture package')

-- -------------------------------------
-- add texture sizes to datastructure
for tex,_ in pairs(texture_ids) do
  local texdata,truew,trueh = gettex(tex)
  texture_ids[tex].texw  = #texdata[1]
  texture_ids[tex].texh  = #texdata
  texture_ids[tex].truew = truew
  texture_ids[tex].trueh = trueh
end

-- -------------------------------------
-- reverse texture index
id_to_texture = {}
for tex,nfo in pairs(texture_ids) do
  if tex ~= 'F_SKY1' then -- skip sky entirely
    id_to_texture[nfo.id] = tex
  end
end

-- -------------------------------------
print('generating raw texture file')

-- build texture start address table
-- note: addresses are 24 bits
texture_start_addr = texture_data_offset    -- start offset in memory
                   + 8 * (num_textures + 1) -- address after indexing table
texture_start_addr_table = {}
for tex,_ in pairs(texture_ids) do
  if tex ~= 'F_SKY1' then -- skip sky entirely
    -- load texture
    local texw = texture_ids[tex].texw
    local texh = texture_ids[tex].texh
    texture_start_addr_table[tex] = texture_start_addr
    texture_start_addr            = texture_start_addr + texw * texh
  end
end

-- create raw image
local out = assert(io.open(path .. '/../build/textures.img', "wb"))
-- there is not texid 0
local addr_check = texture_data_offset
-- pad to 8 bytes
for b=1,8 do
  out:write(string.pack('B', 0 ))
  addr_check = addr_check + 1
end

-- dump texture index table, 64 bits per texture, (8 bytes)
for i = 1,num_textures do
  -- texture info
  -- 3 bytes address
  -- 1 byte  hp2<<4 | wp2 (height and width power of two, packed)
  -- 1 byte  0 (reserved)
  -- 1 byte  0 (reserved)
  -- 1 byte  0 (reserved)
  -- 1 byte  flags
  if id_to_texture[i] then
    local addr = texture_start_addr_table[id_to_texture[i]]
    print('index ' .. i .. ' texture ' .. id_to_texture[i] .. '@' ..  string.format("%06x",addr ) .. ' ' .. texture_ids[id_to_texture[i]].texw .. 'x' .. texture_ids[id_to_texture[i]].texh)
    -- save for preview
    -- save_table_as_image_with_palette(gettex(id_to_texture[i]),palette,path .. 'textures/' .. id_to_texture[i] .. '.tga')
    -- address
    out:write(string.pack('B',  addr     &255 ))
    out:write(string.pack('B', (addr>> 8)&255 ))
    out:write(string.pack('B', (addr>>16)&255 ))
    addr_check = addr_check + 3
    -- width and height (log)
    local wp2,ok = texture_dim_pow2(texture_ids[id_to_texture[i]].texw)
    if not ok then
        error('texture width is not power of 2! ' .. id_to_texture[i]);
    end
    if wp2 < 2 then
        error('texture width is too small! (4 min) ' .. id_to_texture[i]);
    end
    if wp2 >= 16 then
      error('texture width is too large! ' .. id_to_texture[i]);
    end
    local hp2,ok = texture_dim_pow2(texture_ids[id_to_texture[i]].texh)
    if not ok then
        error('texture height is not power of 2! ' .. id_to_texture[i]);
    end
    if hp2 < 2 then
        error('texture height is too small! (4 min) ' .. id_to_texture[i]);
    end
    if hp2 >= 16 then
      error('texture height is too large! ' .. id_to_texture[i]);
    end
    out:write(string.pack('B',(hp2<<4)|wp2)) -- now packed together
    out:write(string.pack('B',0))
    addr_check = addr_check + 2
    -- padding
    out:write(string.pack('B', 0 ))
    addr_check = addr_check + 1
    out:write(string.pack('B', 0 ))
    addr_check = addr_check + 1
    -- flags
    out:write(string.pack('B', texture_flags(id_to_texture[i]) ))
    addr_check = addr_check + 1
  else
    print('skipping index ' .. i)
    for b=1,8 do
      out:write(string.pack('B', 0 ))
      addr_check = addr_check + 1
    end
  end
end
-- dump texture data
for tex,_ in pairs(texture_ids) do
  if tex ~= 'F_SKY1' then -- skip sky entirely
    local addr = texture_start_addr_table[tex]
    if (addr ~= addr_check) then
      error('addressing is incorrect')
    end
    -- load texture
    local texdata = gettex(tex)
    local texw = #texdata[1]
    local texh = #texdata
    -- data
    for j=1,texh do
      for i=1,texw do
        out:write(string.pack('B',texdata[j][i]))
        addr_check = addr_check + 1
      end
    end
  end
end

out:close()

print('stored ' .. addr_check .. ' texture bytes\n')
print('total: ' .. num_textures .. ' textures\n\n')
