W = 240
N = 12
B = W // N

-- contains[24][N * N]
local contains = {}
for i = 0, 23 do
  local t = {}
  for j = 0, N * N - 1 do t[j] = false end
  contains[i] = t
end

for i = 0, N * N - 1 do
  local x0 = (i % N) * B
  local y0 = (i // N) * B
  for dx = 0, B - 1 do
    for dy = 0, B - 1 do
      local x, y = x0 + dx, y0 + dy
      local xc, yc = x*2 + 1 - 240, y*2 + 1 - 240
      local dsq = xc ^ 2 + yc ^ 2
      if dsq >= 4 * 106 * 106 and dsq <= 4 * 116 * 116 then
        -- In the circle. Find the subdivision
        local subdiv = math.atan2(yc, xc) / (math.pi / 12)
        subdiv = (math.floor(subdiv) % 24 + 6) % 24
        contains[subdiv][i] = true
      end
    end
  end
end

for i = 0, 23 do
  io.write('{')
  local byte = ''
  local nonzero_cnt = 0
  for j = 0, N * N - 1 do
    byte = (contains[i][j] and '1' or '0') .. byte
    if j % 8 == 7 or j == N * N - 1 then
      if string.find(byte, '1', 1, true) then
        io.write('{', string.format('%2d', j // 8), ', 0b', byte, '}')
        nonzero_cnt = nonzero_cnt + 1
        if nonzero_cnt < 3 then io.write(', ') end
      end
      byte = ''
    end
  end
  if nonzero_cnt < 3 then io.write('{-1}') end
  io.write('},\n')
end
