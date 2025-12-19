-- scripts/create_random.lua
local x = math.random(50, 600)
local y = math.random(50, 400)
local w = math.random(50, 150)
local h = math.random(50, 150)
local id = aslan.create_rect("RandomNode", x, y, w, h)
aslan.log("Created random rectangle via Lua! ID: " .. id)
