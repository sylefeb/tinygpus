-- _____________________________________________________________________________
-- |                                                                           |
-- |  Silice preprocessor script to extract WAD data                           |
-- |  ==============================================                           |
-- |                                                                           |
-- | Prepares a texture pack and level structure header for DMC-1 demos        |
-- |                                                                           |
-- | Extracts a specific level data                                            |
-- |                                                                           |
-- | @sylefeb 2020-04-30  licence: GPL v3, see full text in repo               |
-- |___________________________________________________________________________|

-- -----------------------------------
-- data definitions

walkers_frames = {
  'A1','A2A8','A3A7','A4A6','A5', -- rest/walk
  'B1','B2B8','B3B7','B4B6','B5',
  'C1','C2C8','C3C7','C4C6','C5',
  'D1','D2D8','D3D7','D4D6','D5',
  'E1','E2E8','E3E7','E4E6','E5', -- fire
  'F1','F2F8','F3F7','F4F6','F5',
  'H0','I0','J0','K0','L0',       -- die
  }

walkers_frames_alt1 = {
  'A1','A2A8','A3A7','A4A6','A5', -- rest/walk
  'B1','B2B8','B3B7','B4B6','B5',
  'C1','C2C8','C3C7','C4C6','C5',
  'D1','D2D8','D3D7','D4D6','D5',
  'E1','E2E8','E3E7','E4E6','E5', -- fire
  'F1','F2F8','F3F7','F4F6','F5',
  'H1','I0','J0','K0','L0','M0'       -- die
  }

walkers_frames_alt2 = {
    'A1','A2A8','A3A7','A4A6','A5', -- rest/walk
    }

player_frames = {
  'A1','A2A8','A3A7','A4A6','A5', -- rest/walk
  'B1','B2B8','B3B7','B4B6','B5',
  'C1','C2C8','C3C7','C4C6','C5',
  'D1','D2D8','D3D7','D4D6','D5',
  'E1','E2E8','E3E7','E4E6','E5', -- fire
  'F1','F2F8','F3F7','F4F6','F5',
  'H0','I0','J0','K0','L0','M0','N0'   -- die
  }

four_frames = {'A0','B0','C0','D0'}

five_frames  = {'A0','B0','C0','D0','E0'}

barrel_frames  = {'A0','C0','D0','E0'}

two_frames  = {'A0','B0'}

one_frame   = {'A0'}

N_frame     = {'N0'}

known_things = {
  [9]    = {prefix='POSS',frames=walkers_frames,num_rest_frames=4*5,num_fire_frames=2*5,num_die_frames=5,monster=1,radius=20},
  [3001] = {prefix='TROO',frames=walkers_frames_alt1,num_rest_frames=4*5,num_fire_frames=2*5,num_die_frames=6,monster=1,radius=20},
  [3004] = {prefix='SPOS',frames=walkers_frames,num_rest_frames=4*5,num_fire_frames=2*5,num_die_frames=5,monster=1,radius=20},
  [3003] = {prefix='BOSS',frames=walkers_frames_alt2,num_rest_frames=4*5,num_fire_frames=2*5,num_die_frames=5,monster=1,radius=24},
  [48]   = {prefix='ELEC',frames=one_frame,num_rest_frames=1,num_fire_frames=0,num_die_frames=0,radius=20},
  [2035] = {prefix='BEXP',frames=barrel_frames,num_rest_frames=1,num_fire_frames=0,num_die_frames=3,radius=20},
  [24]   = {prefix='POL5',frames=one_frame,num_rest_frames=1,num_fire_frames=0,num_die_frames=0,radius=20},
  [2014] = {prefix='BON1',frames=four_frames,num_rest_frames=4,num_fire_frames=0,num_die_frames=0,radius=20},
  [2015] = {prefix='BON2',frames=four_frames,num_rest_frames=4,num_fire_frames=0,num_die_frames=0,radius=20},
  [2018] = {prefix='ARM1',frames=two_frames,num_rest_frames=2,num_fire_frames=0,num_die_frames=0,radius=20},
  [2019] = {prefix='ARM2',frames=two_frames,num_rest_frames=2,num_fire_frames=0,num_die_frames=0,radius=20},
  [2011] = {prefix='STIM',frames=one_frame,num_rest_frames=1,num_fire_frames=0,num_die_frames=0,radius=20},
  [2012] = {prefix='MEDI',frames=one_frame,num_rest_frames=1,num_fire_frames=0,num_die_frames=0,radius=20},
  [15]   = {prefix='PLAY',frames=N_frame,num_rest_frames=1,num_fire_frames=0,num_die_frames=0,radius=20},
  [2]    = {prefix='PLAY',frames=player_frames,num_rest_frames=4*5,num_fire_frames=2*5,num_die_frames=7,monster=1,radius=20},
}

-- -----------------------------------
-- locate a pos in the BSP (returns sub-sector)
function bspLocate(posx,posy)
  queue     = {}
  queue_ptr = 1
  queue[queue_ptr] = root
  queue_ptr = queue_ptr + 1
  while queue_ptr > 1 do
    n = queue[queue_ptr-1]
    queue_ptr = queue_ptr - 1
    if (n&(1<<15)) == 0 then
      lx  = bspNodes[1+n].x
      ly  = bspNodes[1+n].y
      ldx = bspNodes[1+n].dx
      ldy = bspNodes[1+n].dy
      r   = bspNodes[1+n].rchild
      l   = bspNodes[1+n].lchild
      -- which side are we on?
      dx     = posx - lx
      dy     = posy - ly
      csl    = dx * ldy
      csr    = dy * ldx
      if csr > csl then
        -- front
        queue[queue_ptr] = bspNodes[1+n].rchild;
        queue_ptr = queue_ptr + 1
        queue[queue_ptr] = bspNodes[1+n].lchild;
        queue_ptr = queue_ptr + 1
      else
        -- back
        queue[queue_ptr] = bspNodes[1+n].lchild;
        queue_ptr = queue_ptr + 1
        queue[queue_ptr] = bspNodes[1+n].rchild;
        queue_ptr = queue_ptr + 1
      end
    else
      ssid = (n&(~(1<<15)))
      ss   = ssectors[1+ssid]
      seg  = segs[1+ss.start_seg]
      ldef = lines[1+seg.ldf]
      if seg.dir == 0 then
        sidedef = sides[1+ldef.right]
      else
        sidedef = sides[1+ldef.left]
      end
      return ssid,sidedef.sec
    end
  end
end

-- -----------------------------------
-- locate a pos in the BSP (returns list of sub-sectors)
function bspLocateWithRadius(posx,posy,radius)
  subsecs   = {}
  queue     = {}
  queue_ptr = 1
  queue[queue_ptr] = root
  queue_ptr = queue_ptr + 1
  while queue_ptr > 1 do
    n = queue[queue_ptr-1]
    queue_ptr = queue_ptr - 1
    if (n&(1<<15)) == 0 then
      lx  = bspNodes[1+n].x
      ly  = bspNodes[1+n].y
      ldx = bspNodes[1+n].dx
      ldy = bspNodes[1+n].dy
      r   = bspNodes[1+n].rchild
      l   = bspNodes[1+n].lchild
      -- which side are we on?
      dx     = posx - lx
      dy     = posy - ly
      csl    = dx * ldy
      csr    = dy * ldx
      dist   = (csr - csl) / (math.sqrt( ldx*ldx + ldy*ldy ))
      if dist > -radius then
        -- front
        queue[queue_ptr] = bspNodes[1+n].rchild;
        queue_ptr = queue_ptr + 1
        queue[queue_ptr] = bspNodes[1+n].lchild;
        queue_ptr = queue_ptr + 1
      end
      if dist < radius then
        -- back
        queue[queue_ptr] = bspNodes[1+n].lchild;
        queue_ptr = queue_ptr + 1
        queue[queue_ptr] = bspNodes[1+n].rchild;
        queue_ptr = queue_ptr + 1
      end
    else
      ssid = (n&(~(1<<15)))
      subsecs[1+#subsecs] = ssid
    end
  end
  return subsecs
end

------------------------------------------
-- value from hex char
function char2value(c)
  if c >= string.byte('0') and c <= string.byte('9') then
    return c - string.byte('0')
  elseif c >= string.byte('a') and c <= string.byte('f') then
    return c - string.byte('a') + 10
  else
    error('char2value, not hex')
  end
end

------------------------------------------
-- extract a byte from hex string
function extractByte(dw)
  local v0  = char2value(dw:byte(dw:len()-0))
  local v1  = char2value(dw:byte(dw:len()-1))
  local sub = dw:sub(1,dw:len()-2)
  return sub,(v0 + v1 * 16)
end

------------------------------------------
-- write 128 bits to file
function write128bits(out,dw)
  local by
  -- print('writing 128 bits: ' .. dw)
  for i=1,16 do
    if dw == '' then
      -- pad with zeros
      out:write(string.pack('B', 0 ))
    else
      -- get next
      dw,by    = extractByte(dw)
      out:write(string.pack('B', by ))
    end
  end
end

-- -------------------------------------
-- rounding
function round(x)
  return math.floor(x+0.5)
end

-- -------------------------------------
-- read vertices
verts = {}
local in_verts = assert(io.open(findfile('lumps/' .. level .. '_' .. 'VERTEXES.lump'), 'rb'))
local sz = fsize(in_verts)
print('vertex file is ' .. sz .. ' bytes')
for i = 1,sz/4 do
  local x = string.unpack('h',in_verts:read(2))
  local y = string.unpack('h',in_verts:read(2))
  verts[i] = {x = x, y = y}
end

-- -------------------------------------
-- read sidedefs, also gather textures
sides = {}
local in_sides = assert(io.open(findfile('lumps/' .. level .. '_' .. 'SIDEDEFS.lump'), 'rb'))
local sz = fsize(in_sides)
print('sidedefs file is ' .. sz .. ' bytes')
for i = 1,sz/30 do
  local xoff = string.unpack('h',in_sides:read(2))
  local yoff = string.unpack('h',in_sides:read(2))
  local uprT = in_sides:read(8):match("[%_-%a%d]+")
  local lwrT = in_sides:read(8):match("[%_-%a%d]+")
  local midT = in_sides:read(8):match("[%_-%a%d]+")
  local sec  = string.unpack('H',in_sides:read(2))
  sides[i] = {xoff = xoff, yoff = yoff,uprT = uprT,lwrT = lwrT, midT = midT, sec=sec}
end

-- -------------------------------------
-- read sectors
sectors = {}
tag_2_sector = {}
local in_sectors = assert(io.open(findfile('lumps/' .. level .. '_' .. 'SECTORS.lump'), 'rb'))
local sz = fsize(in_sectors)
print('sectors file is ' .. sz .. ' bytes')
for i = 1,sz/26 do
  local floor    = string.unpack('h',in_sectors:read(2))
  local ceiling  = string.unpack('h',in_sectors:read(2))
  local floorT   = in_sectors:read(8):match("[%_-%a%d]+")
  local ceilingT = in_sectors:read(8):match("[%_-%a%d]+")
  local light    = string.unpack('H',in_sectors:read(2))
  local special  = string.unpack('H',in_sectors:read(2))
  local tag      = string.unpack('H',in_sectors:read(2))
  tag_2_sector[tag] = i-1
  sectors[i] = { floor=floor, ceiling=ceiling, floorT=floorT, ceilingT=ceilingT, light=light, special=special, tag=tag}
end
print('read ' .. #sectors)
if #sectors > 512 then
  error('cannot support more than 512 sectors')
end

-- -------------------------------------
-- read linedefs
lines = {}
local in_lines = assert(io.open(findfile('lumps/' .. level .. '_' .. 'LINEDEFS.lump'), 'rb'))
local sz = fsize(in_lines)
print('linedefs file is ' .. sz .. ' bytes')
for i = 1,sz/14 do
  local v0    = string.unpack('H',in_lines:read(2))
  local v1    = string.unpack('H',in_lines:read(2))
  local flags = string.unpack('H',in_lines:read(2))
  local types = string.unpack('H',in_lines:read(2))
  local tag   = string.unpack('H',in_lines:read(2))
  local right = string.unpack('H',in_lines:read(2)) -- sidedef
  local left  = string.unpack('H',in_lines:read(2)) -- sidedef
  lines[i] = {v0 = v0, v1 = v1,flags = flags,types = types, tag = tag,right =right, left = left}
end

-- -------------------------------------
-- read segs
segs = {}
local in_segs = assert(io.open(findfile('lumps/' .. level .. '_' .. 'SEGS.lump'), 'rb'))
local sz = fsize(in_segs)
local maxseglen = 0.0
print('segs file is ' .. sz .. ' bytes')
for i = 1,sz/12 do
  local v0  = string.unpack('H',in_segs:read(2))
  local v1  = string.unpack('H',in_segs:read(2))
  local agl = string.unpack('h',in_segs:read(2))
  local ldf = string.unpack('H',in_segs:read(2))
  local dir = string.unpack('h',in_segs:read(2))
  local off = string.unpack('h',in_segs:read(2))
  dx = verts[1+v1].x-verts[1+v0].x
  dy = verts[1+v1].y-verts[1+v0].y
  seglen = math.sqrt(dx*dx+dy*dy)
  segs[i] = {v0=v0,v1=v1,agl=agl,ldf=ldf,dir=dir,off=off,seglen=seglen}
  if (seglen > maxseglen) then
    maxseglen = seglen
  end
end
print('max seg len is ' .. maxseglen .. ' units.')
if (maxseglen*maxseglen / 32) > 65535 then
  print('[WARNING] squared segment length too large for 16 bits')
  maxseglen = 65535
end

-- -------------------------------------
-- read ssectors
ssectors = {}
local in_ssectors = assert(io.open(findfile('lumps/' .. level .. '_' .. 'SSECTORS.lump'), 'rb'))
local sz = fsize(in_ssectors)
print('ssectors file is ' .. sz .. ' bytes')
for i = 1,sz/4 do
  local num_segs  = string.unpack('H',in_ssectors:read(2))
  local start_seg = string.unpack('H',in_ssectors:read(2))
  ssectors[i] = {num_segs=num_segs,start_seg=start_seg}
end

-- -------------------------------------
-- read nodes
nodes = {}
local in_nodes = assert(io.open(findfile('lumps/' .. level .. '_' .. 'NODES.lump'), 'rb'))
local sz = fsize(in_nodes)
print('nodes file is ' .. sz .. ' bytes')
root = sz//28-1
for i = 1,sz/28 do
  local x  = string.unpack('h',in_nodes:read(2))
  local y  = string.unpack('h',in_nodes:read(2))
  local dx = string.unpack('h',in_nodes:read(2))
  local dy = string.unpack('h',in_nodes:read(2))
  local rby_hi = string.unpack('h',in_nodes:read(2))
  local rby_lw = string.unpack('h',in_nodes:read(2))
  local rbx_lw = string.unpack('h',in_nodes:read(2))
  local rbx_hi = string.unpack('h',in_nodes:read(2))
  local lby_hi = string.unpack('h',in_nodes:read(2))
  local lby_lw = string.unpack('h',in_nodes:read(2))
  local lbx_lw = string.unpack('h',in_nodes:read(2))
  local lbx_hi = string.unpack('h',in_nodes:read(2))
  local rchild = string.unpack('H',in_nodes:read(2))
  local lchild = string.unpack('H',in_nodes:read(2))
  nodes[i] = {x=x,y=y,dx=dx,dy=dy,
         rby_hi=rby_hi,rby_lw=rby_lw,rbx_lw=rbx_lw,rbx_hi=rbx_hi,
         lby_hi=lby_hi,lby_lw=lby_lw,lbx_lw=lbx_lw,lbx_hi=lbx_hi,
         rchild=rchild,lchild=lchild}
end

-- -------------------------------------
-- compute sectors heights
sectors_heights={}
for id,sect in ipairs(sectors) do
  sectors_heights[id-1] = {
    floor   = sect.floor,
    ceiling = sect.ceiling,
    lif     = sect.floor,
    hif     = sect.floor,
    lic     = sect.ceiling,
    hic     = sect.ceiling
  }
end
for _,ldef in pairs(lines) do
  if ldef.left < 65535 then
    othersidedef = sides[1+ldef.left]
    sidedef      = sides[1+ldef.right]
    -- one side
    othersec     = othersidedef.sec
    sec          = sidedef.sec
    sectors_heights[sec].lif = math.min(sectors_heights[sec].lif,sectors[1+othersec].floor)
    sectors_heights[sec].hif = math.max(sectors_heights[sec].hif,sectors[1+othersec].floor)
    sectors_heights[sec].lic = math.min(sectors_heights[sec].lic,sectors[1+othersec].ceiling)
    sectors_heights[sec].hic = math.max(sectors_heights[sec].hic,sectors[1+othersec].ceiling)
    if sectors_heights[sec].lef then
      sectors_heights[sec].lef = math.min(sectors_heights[sec].lef,sectors[1+othersec].floor)
      sectors_heights[sec].hef = math.max(sectors_heights[sec].hef,sectors[1+othersec].floor)
      sectors_heights[sec].lec = math.min(sectors_heights[sec].lec,sectors[1+othersec].ceiling)
    else
      sectors_heights[sec].lef = sectors[1+othersec].floor
      sectors_heights[sec].hef = sectors[1+othersec].floor
      sectors_heights[sec].lec = sectors[1+othersec].ceiling
    end
    -- other side
    othersec     = sidedef.sec
    sec          = othersidedef.sec
    sectors_heights[sec].lif = math.min(sectors_heights[sec].lif,sectors[1+othersec].floor)
    sectors_heights[sec].hif = math.max(sectors_heights[sec].hif,sectors[1+othersec].floor)
    sectors_heights[sec].lic = math.min(sectors_heights[sec].lic,sectors[1+othersec].ceiling)
    sectors_heights[sec].hic = math.max(sectors_heights[sec].hic,sectors[1+othersec].ceiling)
    if sectors_heights[sec].lef then
      sectors_heights[sec].lef = math.min(sectors_heights[sec].lef,sectors[1+othersec].floor)
      sectors_heights[sec].hef = math.max(sectors_heights[sec].hef,sectors[1+othersec].floor)
      sectors_heights[sec].lec = math.min(sectors_heights[sec].lec,sectors[1+othersec].ceiling)
    else
      sectors_heights[sec].lef = sectors[1+othersec].floor
      sectors_heights[sec].hef = sectors[1+othersec].floor
      sectors_heights[sec].lec = sectors[1+othersec].ceiling
    end
  end
end

-- -------------------------
-- find all movable triggers
movables = {}
id    = 1
for lid,ldef in ipairs(lines) do
  local ismovable,ismanual,floor_or_ceiling,end_level
  ismovable=false; ismanual=0; floor_or_ceiling=0; end_level=0
  -- manual doors
  if   ldef.types == 1
    or ldef.types == 26
    or ldef.types == 28
    or ldef.types == 27
    or ldef.types == 31
    or ldef.types == 32
    or ldef.types == 33
    or ldef.types == 34
    or ldef.types == 46
    or ldef.types == 117
    or ldef.types == 118
    then
    ismovable = true
    floor_or_ceiling = 0 -- ceiling
    ismanual = 1
  end
  -- remote doors
  if   ldef.types == 4
    or ldef.types == 29
    or ldef.types == 90
    or ldef.types == 63
    or ldef.types == 2
    or ldef.types == 103
    or ldef.types == 86
    or ldef.types == 61
    or ldef.types == 3
    or ldef.types == 50
    or ldef.types == 75
    or ldef.types == 42
    or ldef.types == 16
    or ldef.types == 76
    then
    ismovable = true
    floor_or_ceiling = 0 -- ceiling
    ismanual = 1
  end
  -- lifts
  if   ldef.types == 10
    or ldef.types == 21
    or ldef.types == 88
    or ldef.types == 62
    or ldef.types == 121
    or ldef.types == 122
    or ldef.types == 120
    or ldef.types == 123
    then
    ismovable = true
    floor_or_ceiling   = 1 -- floor
    ismanual = 1 -- for now
  end
  -- end switch
  if   ldef.types == 11
    or ldef.types == 51
    or ldef.types == 52
    or ldef.types == 124
    then
    ismovable = true
    end_level = 1
    ismanual  = 1
  end

  if ismovable then
    if ldef.left < 65535 or ldef.tag > 0 or end_level == 1 then
      if end_level == 1 then
          movedsec = 65535 -- no sector
      else
        if ldef.tag == 0 then -- TODO also check sector is not tagged
          sidedef    = sides[1+ldef.left]
          movedsec   = sidedef.sec
        else
          movedsec   = tag_2_sector[ldef.tag]
        end
      end
      if movedsec == 65535 then
        print('' .. id .. '] is an end of level switch')
      else
        print('' .. id .. '] sector ' .. movedsec .. ' is a moving (door/lift/ceiling) - tag:' .. ldef.tag)
      end
      movables[lid-1] = {
          id       = id,
          sec      = movedsec,
          starth   = starth,
          ismanual = ismanual,
          floor_or_ceiling = floor_or_ceiling,
          uph      = 0,
          downh    = 0,
      }
      if movedsec < 65535 then
        if floor_or_ceiling == 1 then
          movables[lid-1].downh = sectors_heights[movedsec].lif
          movables[lid-1].uph   = sectors_heights[movedsec].floor
          -- force open
          sectors[1+movedsec].floor = sectors_heights[movedsec].lif
        else
          movables[lid-1].downh = sectors_heights[movedsec].floor
          movables[lid-1].uph   = sectors_heights[movedsec].lec
          -- force open
          sectors[1+movedsec].ceiling = sectors_heights[movedsec].lec
        end
      end
      id = id + 1
    end
  end
end

-- -------------------------------------
-- find min light level of neighboring sectors
lowlights = {}
for _,ldef in pairs(lines) do
  sidedef      = sides[1+ldef.right]
  sec          = sidedef.sec
  if ldef.left < 65535 then
    othersidedef = sides[1+ldef.left]
    osec = othersidedef.sec;
    if not lowlights[sec] then
      lowlights[sec] = { level = sectors[1+osec].light }
    else
      lowlights[sec].level = math.min(lowlights[sec].level,sectors[1+osec].light)
    end
    if not lowlights[osec] then
      lowlights[osec] = { level = sectors[1+sec].light }
    else
      lowlights[osec].level = math.min(lowlights[osec].level,sectors[1+sec].light)
    end
  end
end

-- -------------------------------------
-- prepare custom data structures
bspNodes     = {}
bspSectors   = {}
bspSSectors  = {}
bspSegs      = {}
bspMovables  = {}

TERRAIN_ID   = 255 -- id, assumes no more that 254 walls and floors

for i,sc in ipairs(sectors) do
  -- textures
  local f_T
  if sc.floorT ~= '-' then
    if sc.floorT ~= 'terrain' then
      f_T = texture_ids[sc.floorT].id
    else
      f_T = TERRAIN_ID
    end
  else
    f_T = 0
  end
  local c_T
  if sc.ceilingT == 'F_SKY1' then
    c_T = 0
  else
    c_T = texture_ids[sc.ceilingT].id
  end
  local lowlight = 0
  if lowlights[i-1] then
    lowlight = lowlights[i-1].level
  end
  local seclight = sc.light
  bspSectors[i] = {
    f_h        = sc.floor,
    c_h        = sc.ceiling,
    f_T        = f_T,
    c_T        = c_T,
    light      = round(math.min(15,(seclight)/16)),
    lowlight   = round(math.min(15,(lowlight)/16)),
    special    = sc.special,
  }
end

maxmovableid = 0
for sec,m in pairs(movables) do
  bspMovables[m.id] = {
    sec    = m.sec,
    starth = m.starth,
    uph    = m.uph,
    downh  = m.downh,
    ismanual = m.ismanual,
    floor_or_ceiling = m.floor_or_ceiling
  }
  maxmovableid = math.max(maxmovableid,m.id)
end
print('' .. maxmovableid .. ' movables in level')

for i,n in ipairs(nodes) do
  bspNodes[i] = {
    x  = n.x,
    y  = n.y,
    dx = n.dx,
    dy = n.dy,
    rchild = n.rchild,
    lchild = n.lchild,
    lbx_hi = n.lbx_hi,
    lbx_lw = n.lbx_lw,
    lby_hi = n.lby_hi,
    lby_lw = n.lby_lw,
    rbx_hi = n.rbx_hi,
    rbx_lw = n.rbx_lw,
    rby_hi = n.rby_hi,
    rby_lw = n.rby_lw,
  }
end

for i,ss in ipairs(ssectors) do
  -- identify parent sector
  seg  = segs[1+ss.start_seg]
  ldef = lines[1+seg.ldf]
  if seg.dir == 0 then
    sidedef = sides[1+ldef.right]
  else
    sidedef = sides[1+ldef.left]
  end
  -- store
  bspSSectors[i] = {
    parentsec = sidedef.sec,
    num_segs  = ss.num_segs,
    start_seg = ss.start_seg,
  }
end

for i,sg in ipairs(segs) do
  ldef = lines[1+sg.ldf]
  other_sidedef = nil
  if sg.dir == 0 then
    sidedef = sides[1+ldef.right]
    if ldef.left < 65535 then
      other_sidedef = sides[1+ldef.left]
    end
  else
    sidedef = sides[1+ldef.left]
    other_sidedef = sides[1+ldef.right]
  end
  -- textures
  lwr = 0
  if sidedef.lwrT:sub(1, 1) ~= '-' then
    if sidedef.lwrT == 'terrain' then
      lwr = TERRAIN_ID -- terrain
    else
      lwr = texture_ids[sidedef.lwrT].id
    end
  end
  upr = 0
  if sidedef.uprT:sub(1, 1) ~= '-' then
    if sidedef.uprT == 'terrain' then
      upr = TERRAIN_ID -- terrain
    else
      upr = texture_ids[sidedef.uprT].id
    end
  end
  mid = 0
  if sidedef.midT:sub(1, 1) ~= '-' then
    if sidedef.midT == 'terrain' then
      mid = TERRAIN_ID -- terrain
    else
      mid = texture_ids[sidedef.midT].id
    end
  end
  -- adjust upr for terrain
  if upr ~= TERRAIN_ID then
    if other_sidedef then
      if     sectors[1+sidedef.sec].ceilingT == 'F_SKY1'
        and sectors[1+other_sidedef.sec].ceilingT == 'F_SKY1' then
        upr = 0
      end
    end
  end
  -- other sector
  other_sec = 65535
  if other_sidedef then
    other_sec = other_sidedef.sec
  end
  local xoff = sidedef.xoff + sg.off
  if (sg.dir == 1) then
    -- revert to be oriented as linedef
    v1x       = verts[1+sg.v0].x
    v1y       = verts[1+sg.v0].y
    v0x       = verts[1+sg.v1].x
    v0y       = verts[1+sg.v1].y
  else
    v0x       = verts[1+sg.v0].x
    v0y       = verts[1+sg.v0].y
    v1x       = verts[1+sg.v1].x
    v1y       = verts[1+sg.v1].y
  end
  lower_unpegged = 0
  if (ldef.flags & 16) ~= 0 then
    lower_unpegged = 1
  end
  upper_unpegged = 0
  if (ldef.flags & 8) ~= 0 then
    upper_unpegged = 1
  end
  local movableid = 0
  if movables[sg.ldf] then
    movableid = movables[sg.ldf].id
    if (movableid >= 255) then
      error('too many movables')
    end
  end
  bspSegs[i] = {
    v0x       = v0x,
    v0y       = v0y,
    v1x       = v1x,
    v1y       = v1y,
    upr       = upr,
    lwr       = lwr,
    mid       = mid,
    movableid = movableid,
    other_sec = other_sec,
    xoff      = xoff,
    yoff      = sidedef.yoff,
    seglen    = sg.seglen,
    segsqlen  = math.ceil(sg.seglen*sg.seglen/32),
    lower_unpegged = lower_unpegged,
    upper_unpegged = upper_unpegged
  }
end

-- -------------------------------------
-- things (player start, monsters)
-- requires data structures above
local in_things = assert(io.open(findfile('lumps/' .. level .. '_' .. 'THINGS.lump'), 'rb'))
local sz = fsize(in_things)
nthings = 0
thingsPerSec = {}
for i = 1,sz/10 do
  local x   = string.unpack('h',in_things:read(2))
  local y   = string.unpack('h',in_things:read(2))
  local a   = string.unpack('h',in_things:read(2))
  local ty  = string.unpack('H',in_things:read(2))
  local opt = string.unpack('H',in_things:read(2))
  if ty == 1 then
    print('Player start at ' .. x .. ',' .. y .. ' angle: ' .. a)
    player_start_x = x
    player_start_y = y
    _,player_sec   = bspLocate(x,y)
    player_start_z = bspSectors[1+player_sec].f_h + 40;
    player_start_a = a*1024//90;
  end

  -- if (opt&2) == 0 and (opt&4) == 0 then -- skill level filter
    if known_things[ty] then
      local _,in_sec = bspLocate(x,y)
      if not thingsPerSec[in_sec] then
        thingsPerSec[in_sec] = {}
      end
      thing = {
        x = x, y = y, a = a*32//90, ty = ty, opt = opt
      }
      print('Thing ' .. ty .. ' at ' .. x .. ',' .. y .. ' angle: ' .. a .. ' sector ' .. in_sec)
      table.insert(thingsPerSec[in_sec],thing)
      nthings = nthings + 1
    end
  -- end
end
print('level contains ' .. nthings .. ' things')
for s,ths in pairs(thingsPerSec) do
  local num = #ths
  print('   sector ' .. s .. ' contains ' .. num .. ' things')
end
-- produce consecutive array of things
allThings = {}
thingsPerSec_start = {}
nextThing = 1
for sec,ths in pairs(thingsPerSec) do
  thingsPerSec_start[sec] = nextThing-1
  for _,th in pairs(ths) do
    allThings[nextThing] = th
    nextThing = nextThing + 1
  end
end
if nextThing > 65535 then
  error('too many things (>65535)')
end
-- include info in sectors
for i,sc in ipairs(bspSectors) do
  local nth = 0
  local fth = 0
  if thingsPerSec[i-1] then
    nth = #thingsPerSec[i-1]
    fth = thingsPerSec_start[i-1]
  end
  sc.num_things  = nth
  sc.first_thing = fth
end

-- -------------------------------------
-- utility

function thing_to_sprite_num_die_frames(ty)
  if known_things[ty] then
    return known_things[ty].num_die_frames
  end
  error('I do not known a thing of type ' .. ty)
end

function thing_to_sprite_num_fire_frames(ty)
  if known_things[ty] then
    return known_things[ty].num_fire_frames
  end
  error('I do not known a thing of type ' .. ty)
end

function thing_to_sprite_num_rest_frames(ty)
  if known_things[ty] then
    return known_things[ty].num_rest_frames
  end
  error('I do not known a thing of type ' .. ty)
end

function thing_to_sprite_first_frame(ty)
  if known_things[ty] then
    return texture_ids[known_things[ty].prefix .. known_things[ty].frames[1]].id
  end
  error('I do not known a thing of type ' .. ty)
end

function thing_to_flags(ty)
  if known_things[ty] then
      if known_things[ty].monster then
        return 1
      else
        return 0
      end
  end
  error('I do not known a thing of type ' .. ty)
end

-- -------------------------------------
-- report
print('- ' .. #ssectors .. ' sub-sectors')
print('- ' .. #nodes .. ' nodes')
print('- ' .. #segs .. ' segs')
print('- ' .. (num_textures-1) .. ' textures')

-- -------------------------------------
