-- scripts/tools.lua
aslan.log("Tools script loaded (Command-Log version)!")

local dragged_node_id = nil
local offset_x = 0
local offset_y = 0

function on_mouse_down(x, y)
    dragged_node_id = aslan.get_node_at(x, y)
    if dragged_node_id then
        -- On pourrait récupérer la position actuelle pour un offset précis, 
        -- mais pour le proto on simplifie.
        aslan.log("Lua dragging node: " .. dragged_node_id)
    end
end

function on_mouse_move(x, y)
    if dragged_node_id then
        -- Émission d'une commande texte au lieu de manipuler la mémoire directement
        local cmd = string.format("move %d %f %f", dragged_node_id, x - 50, y - 25)
        aslan.execute(cmd)
    end
end

function on_mouse_up(x, y)
    dragged_node_id = nil
end
