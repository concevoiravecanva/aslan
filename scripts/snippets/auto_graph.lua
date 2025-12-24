-- Auto-Graph (Aslan snippet)
-- Concept: graph a list of numbers.
-- This version approximates a bar chart using rectangles (since we don't have line primitives yet).

local data = { 12, 40, 22, 55, 30, 10 }
local x0, y0 = 60, 200
local bar_w = 18
local gap = 6

for i, v in ipairs(data) do
  local x = x0 + (i - 1) * (bar_w + gap)
  local y = y0 - v
  aslan.execute(string.format("new_rect %d %d %d %d", x, y, bar_w, v))
end
