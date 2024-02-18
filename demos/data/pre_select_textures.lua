-- _____________________________________________________________________________
-- |                                                                           |
-- |  Silice preprocessor script to extract WAD data                           |
-- |  ==============================================                           |
-- |                                                                           |
-- | Prepares a texture pack and level structure header for DMC-1 demos        |
-- |                                                                           |
-- | Selects the textures to be part of the texture pack                       |
-- |                                                                           |
-- | @sylefeb 2020-05-01  licence: GPL v3, see full text in repo               |
-- |___________________________________________________________________________|

num_textures     = 0
textures_opacity = {}
textures_names   = {}
texture_ids      = {}
switch_off_ids   = {}
switch_on_ids    = {}

parse_textures('TEXTURE1.lump')

-- add all wall textures
for _,name in ipairs(textures_names) do
  num_textures   = num_textures + 1
  texture_ids[name] = {id=num_textures,type='wall'}
end

-- add all flat textures
for _,flat in ipairs(lumps_flats) do
  num_textures   = num_textures + 1
  texture_ids[flat.name] = {id=num_textures,type='flat'}
end

-- add all sprites as textures
for _,sprt in pairs(lumps_sprites) do
  local sprite_lump      = 'lumps/sprites/' .. sprt.name .. '.lump'
  local img,opaque,lo,to = decode_patch_lump(sprite_lump)
  save_table_as_image_with_palette(img,palette,path .. 'textures/sprites/' .. sprt.name .. '.tga')
  num_textures      = num_textures + 1
  texture_ids[sprt.name] = {id=num_textures,type='sprite'}
end

print('num walls  : ' .. #textures_names)
print('num flats  : ' .. #lumps_flats)
print('num sprites: ' .. #lumps_sprites)


--[[

-- -------------------------------------
-- add textures for the terrain
-- remap colors
if not file_exists(path .. 'recolored.tga') then
  print('remapping palette for terrain texture (slow!)')
  local img = get_image_as_table(path .. 'colored.tga')
  local pal = get_palette_as_table(path .. 'colored.tga')
  local w   = #img[1]
  local h   = #img
  for j = 1,w do
    for i = 1,h do
      local pidx = img[j][i]
      local rgb  = pal[1+pidx]
      img[j][i] = find_closest(rgb)
    end
  end
  save_table_as_image_with_palette(img,palette,path .. 'recolored.tga')
end
-- combine with height
local trnt = get_image_as_table(path .. 'recolored.tga')
local hght = get_image_as_table(path .. 'height.tga')
local w   = #hght[1]
local h   = #hght
local lrgr = {}
for j = 1,h*2 do
  lrgr[j] = {}
  for i = 1,w do
    if j <= h then
      lrgr[j][i] = hght[i][j]
    else
      lrgr[j][i] = trnt[i][j-h]
    end
  end
end
save_table_as_image_with_palette(lrgr,palette,path .. 'terrain.tga')
-- add texture
local t = '../../terrain'
num_textures       = num_textures + 1
texture_ids[t]     = {id=num_textures,type='terrain',used=1}
terrain_texture_id = num_textures

]]