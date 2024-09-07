W = 240
N = 12
B = W // N

for i = 0, N * N - 1 do
  local contains = false
  local x0 = (i % N) * B
  local y0 = (i // N) * B
  for dx = 0, B - 1 do
    for dy = 0, B - 1 do
      local x, y = x0 + dx, y0 + dy
      local dsq = (x*2 + 1 - 240) ^ 2 + (y*2 + 1 - 240) ^ 2
      if dsq >= 4 * 106 * 106 and dsq <= 4 * 116 * 116 then
        contains = true
        goto fin
      end
    end
  end
::fin::
  if contains then print(i) end
end
