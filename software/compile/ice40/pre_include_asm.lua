-- include ASM code as a BROM
-- MIT license, see LICENSE_MIT in Silice repo root

if not path then
  path,_1,_2 = string.match(findfile('Makefile'), "(.-)([^\\/]-%.?([^%.\\/]*))$")
  if path == '' then
    path = '.'
  end
  print('********************* firmware written to     ' .. path .. '/build/code.img')
  print('********************* compiled code read from ' .. path .. '/build/code.hex')
end

all_data_hex   = {}
all_data_bram = {}
word = ''
init_data_bytes = 0
local first = 1
local addr_offs = 0

local out = assert(io.open(path .. '/build/code.img', "wb"))
local in_asm = io.open(findfile('/build/code.hex'), 'r')
if not in_asm then
  error('please compile code first using the compile_asm.sh / compile_c.sh scripts')
end
local code = in_asm:read("*all")
in_asm:close()
for str in string.gmatch(code, "([^ \r\n]+)") do
  if string.sub(str,1,1) == '@' then
    addr = tonumber(string.sub(str,2), 16)
    if first == 1 then
      addr_offs = addr;
    end
    print('addr      ' .. addr)
    print('addr_offs ' .. addr_offs)
    print('bytes     ' .. init_data_bytes)
    delta = addr - addr_offs - init_data_bytes
    print('padding ' .. delta)
    for i=1,delta do
      -- pad with zeros
      word     = '00' .. word;
      if #word == 8 then
        all_data_bram[1+#all_data_bram] = '32h' .. word .. ','
        word = ''
      end
      all_data_hex[1+#all_data_hex] = '8h' .. 0 .. ','
      out:write(string.pack('B', 0 ))
      init_data_bytes = init_data_bytes + 1
    end
  else
    word     = str .. word;
    if #word == 8 then
      all_data_bram[1+#all_data_bram] = '32h' .. word .. ','
      word = ''
    end
    all_data_hex[1+#all_data_hex] = '8h' .. str .. ','
    out:write(string.pack('B', tonumber(str,16) ))
    init_data_bytes = init_data_bytes + 1
  end
  first = 0
end

out:close()

data_hex  = table.concat(all_data_hex)
data_bram = table.concat(all_data_bram)

print('image is ' .. init_data_bytes .. ' bytes.')
