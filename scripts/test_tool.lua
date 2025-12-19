-- scripts/test_tool.lua
aslan.log("Hello from Lua! Initializing tool...")

-- Création de quelques objets via l'API C exposée
local id1 = aslan.create_rect("Header", 0, 0, 800, 100)
aslan.log("Created Header with ID: " .. id1)

local id2 = aslan.create_rect("Sidebar", 0, 100, 200, 500)
aslan.log("Created Sidebar with ID: " .. id2)

local id3 = aslan.create_rect("Content", 210, 110, 580, 480)
aslan.log("Created Content with ID: " .. id3)

aslan.log("Tool execution finished.")

-- Test du nouveau pont de commande
aslan.log("Testing command bridge...")
local success = aslan.execute("rect LuaRect 300 300 100 100")
if success then
    aslan.log("Command 'rect' executed successfully from Lua!")
else
    aslan.log("Command 'rect' failed from Lua.")
end
