-- _____________________________________________________________________________
-- |                                                                           |
-- |  Silice preprocessor script to extract WAD data                           |
-- |  ==============================================                           |
-- |                                                                           |
-- | Prepares a texture pack and level structure header for DMC-1 demos        |
-- |                                                                           |
-- | This file writes the level structure C header                             |
-- |                                                                           |
-- | @sylefeb 2020-11-02  licence: GPL v3, see full text in repo               |
-- |___________________________________________________________________________|

function struct_p2d()
  return [[
struct p2d {
  int16   x;
  int16   y;
};
]]
end
local sizeof_p2d = (16*2) // 8

function struct_bsp_bbox()
    return [[
  struct bsp_bbox {
    int16   y_hi;
    int16   y_lw;
    int16   x_hi;
    int16   x_lw;
  };
  ]]
end
local sizeof_bsp_bbox = (16*4) // 8

function struct_bsp_node()
  return [[
struct bsp_node {
  uint16   lchild;
  uint16   rchild;
  int16   dy;
  int16   dx;
  int16   y;
  int16   x;
  struct bsp_bbox lbb;
  struct bsp_bbox rbb;
};
]]
end
local sizeof_bsp_node = (16*6) // 8 + sizeof_bsp_bbox * 2

function record_bsp_node(node)
  local bin = '{'
        .. '.lchild = 0x' .. string.format("%04x",node.lchild ):sub(-4) .. ','
        .. '.rchild = 0x' .. string.format("%04x",node.rchild ):sub(-4) .. ','
        .. '.dy     = 0x' .. string.format("%04x",node.dy ):sub(-4) .. ','
        .. '.dx     = 0x' .. string.format("%04x",node.dx ):sub(-4) .. ','
        .. '.y      = 0x' .. string.format("%04x",node.y  ):sub(-4) .. ','
        .. '.x      = 0x' .. string.format("%04x",node.x  ):sub(-4) .. ','
        .. '.lbb    = {'
        .. '.y_hi = 0x' .. string.format("%04x",node.lby_hi):sub(-4) .. ','
        .. '.y_lw = 0x' .. string.format("%04x",node.lby_lw):sub(-4) .. ','
        .. '.x_hi = 0x' .. string.format("%04x",node.lbx_hi):sub(-4) .. ','
        .. '.x_lw = 0x' .. string.format("%04x",node.lbx_lw):sub(-4)
        .. '},'
        .. '.rbb    = {'
        .. '.y_hi = 0x' .. string.format("%04x",node.rby_hi):sub(-4) .. ','
        .. '.y_lw = 0x' .. string.format("%04x",node.rby_lw):sub(-4) .. ','
        .. '.x_hi = 0x' .. string.format("%04x",node.rbx_hi):sub(-4) .. ','
        .. '.x_lw = 0x' .. string.format("%04x",node.rbx_lw):sub(-4)
        .. '},'
        .. '}'
  return bin
end

------------------------------------------

function struct_bsp_ssec()
  return [[
struct bsp_ssec {
  uint16   parentsec;
  uint16   start_seg;
  uint8    num_segs;
};
]]
end
local sizeof_bsp_ssec = (16*2 + 8) // 8

function record_bsp_ssec(ssec)
  local bin = '{'
    .. '.parentsec = 0x' .. string.format("%04x",ssec.parentsec):sub(-4) .. ','
    .. '.start_seg = 0x' .. string.format("%04x",ssec.start_seg):sub(-4) .. ','
    .. '.num_segs  = 0x' .. string.format("%02x",ssec.num_segs ):sub(-2)
    .. '}'
  return bin
end

------------------------------------------

function struct_bsp_sec()
  return [[
struct bsp_sec {
  uint8    lowlight;
  uint8    special;
  uint8    light;
  uint8    c_T;
  uint8    f_T;
  int16    c_h;
  int16    f_h;
};
]]
end
local sizeof_bsp_sec = (16*2 + 8*5) // 8

function record_bsp_sec(sec)
  local bin = '{'
  .. '.lowlight    = 0x' .. string.format("%02x",sec.lowlight):sub(-2) .. ','
  .. '.special     = 0x' .. string.format("%02x",math.min(255,sec.special)):sub(-2) .. ','
  .. '.light       = 0x' .. string.format("%02x",sec.light):sub(-2) .. ','
  .. '.c_T         = 0x' .. string.format("%02x",sec.c_T  ):sub(-2) .. ','
  .. '.f_T         = 0x' .. string.format("%02x",sec.f_T  ):sub(-2) .. ','
  .. '.c_h         = 0x' .. string.format("%04x",sec.c_h):sub(-4) .. ','
  .. '.f_h         = 0x' .. string.format("%04x",sec.f_h):sub(-4)
  .. '}'
  return bin
end

------------------------------------------

function bsp_seg_flags(seg)
  local flags = 0
  -- check if mid texture is opaque
  local mid = seg.mid
  if mid ~= 0 then
    if textures_opacity[id_to_texture[mid]] == 0 then
      print(' texture ' .. mid .. ' is transparent')
      flags = flags | 1                   -- 1
    end
  end
  flags = flags | (seg.lower_unpegged<<1) -- 2
  flags = flags | (seg.upper_unpegged<<2) -- 4
  return flags
end

------------------------------------------

function struct_bsp_seg()
  return [[
struct bsp_seg {
  uint8    movableid;
  uint16   other_sec;
  uint8    upr;
  uint8    mid;
  uint8    lwr;
  uint8    flags; // bit 0: transparent
  struct p2d v1;
  struct p2d v0;
  uint16   seglen;
};
]]
end
local sizeof_bsp_seg = (16*2 + 8*5) // 8 + 2*sizeof_p2d

function record_bsp_seg(seg)
  local flags = bsp_seg_flags(seg)
  -- write record
  local bin = '{'
  .. '.movableid = 0x' .. string.format("%02x",seg.movableid):sub(-2) .. ','
  .. '.other_sec = 0x' .. string.format("%04x",seg.other_sec):sub(-4) .. ','
  .. '.upr       = 0x' .. string.format("%02x",seg.upr):sub(-2) .. ','
  .. '.mid       = 0x' .. string.format("%02x",seg.mid):sub(-2) .. ','
  .. '.lwr       = 0x' .. string.format("%02x",seg.lwr):sub(-2) .. ','
  .. '.flags     = 0x' .. string.format("%02x",flags):sub(-2) .. ','
  .. '.v1        = {.x = 0x' .. string.format("%04x",seg.v1x):sub(-4) .. ','
  .. '              .y = 0x' .. string.format("%04x",seg.v1y):sub(-4) .. '},'
  .. '.v0        = {.x = 0x' .. string.format("%04x",seg.v0x):sub(-4) .. ','
  .. '              .y = 0x' .. string.format("%04x",seg.v0y):sub(-4) .. '},'
  .. '.seglen               = 0x' .. string.format("%04x",round(seg.seglen)):sub(-4)
  .. '}'
  return bin
end

------------------------------------------

function struct_bsp_movable()
  return [[
struct bsp_movable {
  uint8    a_d_m_fc;
  uint16   sec;
  uint16   downh;
  uint16   uph;
};
]]
end
local sizeof_bsp_movable = (16*3) // 8

function record_bsp_movable(m)
  local bin = '{'
  .. '.a_d_m_fc = 0x' .. string.format("%02x",(0*8 + 1*4 + m.ismanual*2 + m.floor_or_ceiling*1)):sub(-2) .. ','
  .. '.sec      = 0x' .. string.format("%04x",m.sec):sub(-4) .. ','
  .. '.downh    = 0x' .. string.format("%04x",m.downh):sub(-4) .. ','
  .. '.uph      = 0x' .. string.format("%04x",m.uph):sub(-4)
  .. '}'
  return bin
end

------------------------------------------

function struct_thing()
  return [[
struct thing {
  uint8    a;
  int16    x;
  int16    y;
  uint8    r;
  uint8    last_update_time; // written
  uint8    status;           // written
  uint8    flags;
  uint8    num_frames;
  uint8    first_die_frame;
  uint8    first_fire_frame;
  uint8    current_frame;    // written
  uint16   first_frame;
};
]]
end
local sizeof_bsp_thing = (8*9 + 16*3) // 8

function record_thing(th)
  local nfo = texture_ids[known_things[th.ty].prefix .. known_things[th.ty].frames[1]]
  local h   = nfo.texh
  local bin = '{'
  .. '.last_update_time = 0x' .. string.format("%02x",255):sub(-2)  .. ',' -- last update time
  .. '.status           = 0x' .. string.format("%02x",1):sub(-2)  .. ',' -- 1 is alive, at rest
  .. '.flags            = 0x' .. string.format("%02x", thing_to_flags(th.ty)):sub(-2) .. ','
  .. '.num_frames       = 0x' .. string.format("%02x", thing_to_sprite_num_rest_frames(th.ty)
                         + thing_to_sprite_num_fire_frames(th.ty)
                         + thing_to_sprite_num_die_frames(th.ty)):sub(-2) .. ','
  .. '.first_die_frame  = 0x' .. string.format("%02x", thing_to_sprite_num_rest_frames(th.ty)
                         + thing_to_sprite_num_fire_frames(th.ty)):sub(-2) .. ','
  .. '.first_fire_frame = 0x' .. string.format("%02x",thing_to_sprite_num_rest_frames(th.ty)):sub(-2) .. ','
  .. '.current_frame    = 0x' .. string.format("%02x",0):sub(-2) .. ',' -- current frame is 0
  .. '.first_frame      = 0x' .. string.format("%04x",thing_to_sprite_first_frame(th.ty)):sub(-4) .. ','
  .. '.a                = 0x' .. string.format("%02x",th.a):sub(-2) .. ','
  .. '.y                = 0x' .. string.format("%04x",th.y):sub(-4) .. ','
  .. '.x                = 0x' .. string.format("%04x",th.x):sub(-4)
  .. '}'
  return bin
end

-- ------------------------------------------------------------------
-- open the image file to append level data

local out = assert(io.open(path .. '../build/level.h', "w"))
local bsp_node_bytes = sizeof_bsp_node * #bspNodes
local bsp_sec_bytes  = sizeof_bsp_sec  * #bspSectors
local bsp_ssec_bytes = sizeof_bsp_ssec * #bspSSectors
local bsp_seg_bytes  = sizeof_bsp_seg  * #bspSegs
local bsp_movable_bytes = sizeof_bsp_movable  * #bspMovables
local bsp_thing_bytes   = sizeof_bsp_thing    * #allThings

local level_data_bytes  = bsp_node_bytes
                        + bsp_sec_bytes
                        + bsp_ssec_bytes
                        + bsp_seg_bytes
                        + bsp_movable_bytes
                        + bsp_thing_bytes

print('level stats:')
print(' - ' .. #bspNodes .. ' nodes ' .. bsp_node_bytes .. ' bytes.')
print(' - ' .. #bspSectors .. ' sectors ' .. bsp_sec_bytes .. ' bytes.')
print(' - ' .. #bspSSectors .. ' sub-sectors ' .. bsp_ssec_bytes .. ' bytes.')
print(' - ' .. #bspSegs .. ' segments ' .. bsp_seg_bytes .. ' bytes.')
print(' - ' .. #bspMovables .. ' movables ' .. bsp_movable_bytes .. ' bytes.')
print(' - ' .. #allThings .. ' things ' .. bsp_thing_bytes .. ' bytes.')

print('bsp core data size ' .. level_data_bytes .. ' bytes')
-- all definitions
out:write('typedef short int16  ;\n')
out:write('typedef char  int8  ;\n')
out:write('typedef unsigned short uint16  ;\n')
out:write('typedef unsigned char  uint8  ;\n')
out:write(struct_p2d())
out:write(struct_bsp_bbox())
out:write(struct_bsp_node())
out:write(struct_bsp_ssec())
out:write(struct_bsp_sec())
out:write(struct_bsp_seg())
out:write(struct_bsp_movable())
out:write(struct_thing())
-- all sizes
out:write('const int n_bspNodes  = ' .. #bspNodes .. ';\n')
out:write('const int n_bspSectors  = ' .. #bspSectors .. ';\n')
out:write('#define N_BSP_SECTORS ' .. #bspSectors .. '\n')
out:write('const int n_bspSSectors = ' .. #bspSSectors .. ';\n')
out:write('const int n_bspSegs     = ' .. #bspSegs .. ';\n')
out:write('const int n_bspMovables = ' .. #bspMovables .. ';\n')
out:write('const int n_things   = ' .. #allThings .. ';\n')
-- nodes
out:write('const struct bsp_node bspNodes[] = {\n')
for i,n in ipairs(bspNodes) do
  out:write(record_bsp_node(n))
  if i < #bspNodes then out:write(',\n') else out:write('\n') end
end
out:write('};\n')
-- ssecs
out:write('const struct bsp_ssec bspSSectors[] = {\n')
for i,s in ipairs(bspSSectors) do
  out:write(record_bsp_ssec(s))
  if i < #bspSSectors then out:write(',\n') else out:write('\n') end
end
out:write('};\n')
-- secs
out:write('const struct bsp_sec bspSectors[] = {\n')
for i,s in ipairs(bspSectors) do
  out:write(record_bsp_sec(s))
  if i < #bspSectors then out:write(',\n') else out:write('\n') end
end
out:write('};\n')
-- segs
out:write('const struct bsp_seg bspSegs[] = {\n')
for i,s in ipairs(bspSegs) do
  out:write(record_bsp_seg(s))
  if i < #bspSegs then out:write(',\n') else out:write('\n') end
end
out:write('};\n')
-- movable
out:write('const struct bsp_movable bspMovables[] = {\n')
for i,m in ipairs(bspMovables) do
  out:write(record_bsp_movable(m))
  if i < #bspMovables then out:write(',\n') else out:write('\n') end
end
out:write('};\n')
-- things
out:write('const struct thing things[] = {\n')
for i,th in ipairs(allThings) do
  out:write(record_thing(th))
  if i < #allThings then out:write(',\n') else out:write('\n') end
end
out:write('};\n')
-- sprite texture dimensions
-- NOTE: this is used only for sprites, could save by
--       storing only for sprites (contiguous from first index)
first_sprite_index = -1
out:write('const unsigned char sprtdims[] = {\n')
for i = 1,terrain_texture_id-1 do
  if id_to_texture[i] then
    if first_sprite_index < 0 and texture_ids[id_to_texture[i]].type == 'sprite' then
      first_sprite_index = i
    end
    if first_sprite_index >= 0 then
      out:write('' .. texture_ids[id_to_texture[i]].truew .. ',' .. texture_ids[id_to_texture[i]].trueh .. ',')
    end
  else
    if first_sprite_index >= 0 then
      out:write('0,0,')
    end
  end
end
out:write('};\n')
out:write('const int first_sprite_index  = ' .. first_sprite_index .. ';\n')
-- misc
out:write('#define doomchip_width  ' .. doomchip_width  .. '\n')
out:write('#define doomchip_height ' .. doomchip_height .. '\n')
out:write('const int root            = ' .. root .. ';\n')
out:write('const int player_start_x  = ' .. player_start_x .. ';\n')
out:write('const int player_start_y  = ' .. player_start_y .. ';\n')
out:write('const int player_start_a  = ' .. player_start_a .. ';\n')
-- fixed point
FPm = 12
-- sin table
sin_tbl = {}
max_sin = ((2^FPm)-1)
for i=0,1023 do
   sin_tbl[i]               = round(max_sin*math.sin(2*math.pi*(i+0.5)/4096))
   sin_tbl[1024 + i]        = round(math.sqrt(max_sin*max_sin - sin_tbl[i]*sin_tbl[i]))
   sin_tbl[2048 + i]        = - sin_tbl[i]
   sin_tbl[2048 + 1024 + i] = - sin_tbl[1024 + i]
end
out:write('const short sin_m[]={\n')
for i=0,4095 do out:write('' .. sin_tbl[i] .. ',') end
out:write('};\n')
-- col_to_x
function col_to_x(i)
  return (doomchip_width/2-(i+0.5)) * (2/3) * (2/doomchip_width)
end
-- col_to_alpha
out:write('const short col_to_alpha[]={\n')
for i=0,doomchip_width do
  out:write('' .. round(math.atan(col_to_x(i)) * (2^12) / (2*math.pi)) .. ',')
end
out:write('};\n')
-- col_to_x table
out:write('const short col_to_x[]={\n')
for i=0,doomchip_width do
  local v = round(col_to_x(i)*max_sin)
  out:write('' .. v .. ',')
end
out:write('};\n')
-- inv_y table
out:write('const unsigned short inv_y[]={\n')
for i=0,2047 do
  if i > 0 then
    local v = round(((1<<16)-1)//i)
    out:write('' .. v .. ',')
  else
    out:write('65535,')
  end
end
out:write('};\n')
-- terrain texture
if terrain_texture_id then
  out:write('const int terrain_texture_id=' .. terrain_texture_id .. ';\n')
end

-- done
out:close()
