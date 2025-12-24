-- Progress Bar (Aslan snippet)
-- Usage: tweak `val` and run this file from your loader.
-- This is a *concept* snippet: Aslan's current Lua API doesn't expose drawing yet.
-- For now we emit shapes as canvas nodes via AOF commands.

local val = 240 -- pixels
local x, y = 40, 40
local h = 20

-- Background bar
aslan.execute(string.format("new_rect %d %d %d %d", x, y, 300, h))
-- Fill bar
aslan.execute(string.format("new_rect %d %d %d %d", x, y, val, h))
