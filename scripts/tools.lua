-- scripts/tools.lua
aslan.log("Tools script loaded!")

local selected_node_id = nil
local offset_x = 0
local offset_y = 0

-- Cette fonction sera appelée par le C lors d'un clic
function on_mouse_down(x, y)
    selected_node_id = aslan.get_node_at(x, y)
    if selected_node_id then
        aslan.log("Lua selected node: " .. selected_node_id)
    end
end

-- Cette fonction sera appelée par le C lors du mouvement
function on_mouse_move(x, y)
    if selected_node_id then
        -- Pour l'instant on centre le bloc sur la souris (simplifié)
        aslan.set_node_pos(selected_node_id, x - 50, y - 25)
    end
end

-- Cette fonction sera appelée par le C lors du relâchement
function on_mouse_up(x, y)
    selected_node_id = nil
end
