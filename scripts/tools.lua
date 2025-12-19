-- scripts/tools.lua
aslan.log("Tools script loaded (Command-Log version)!")

local dragged_node_id = nil
local drag_mode = "none" -- "move" or "resize"
local offset_x = 0
local offset_y = 0
local start_w = 0
local start_h = 0

function on_mouse_down(x, y)
    dragged_node_id = aslan.get_node_at(x, y)
    if dragged_node_id then
        -- On informe le moteur C de la sélection pour l'affichage des gizmos
        aslan.execute(string.format("select %d", dragged_node_id))

        local nx, ny, nw, nh = aslan.get_node_rect(dragged_node_id)
        
        -- Détection si on clique sur le coin inférieur droit (zone de 20x20 pixels)
        local threshold = 20
        if x > (nx + nw - threshold) and y > (ny + nh - threshold) then
            drag_mode = "resize"
            aslan.log("Lua resizing node: " .. dragged_node_id)
        else
            drag_mode = "move"
            offset_x = x - nx
            offset_y = y - ny
            aslan.log("Lua dragging node: " .. dragged_node_id)
        end
    end
end

function on_mouse_move(x, y)
    if dragged_node_id then
        if drag_mode == "move" then
            local cmd = string.format("move %d %f %f", dragged_node_id, x - offset_x, y - offset_y)
            aslan.execute(cmd)
        elseif drag_mode == "resize" then
            local nx, ny, nw, nh = aslan.get_node_rect(dragged_node_id)
            local new_w = math.max(10, x - nx)
            local new_h = math.max(10, y - ny)
            local cmd = string.format("resize %d %f %f", dragged_node_id, new_w, new_h)
            aslan.execute(cmd)
        end
    end
end

function on_mouse_up(x, y)
    dragged_node_id = nil
    drag_mode = "none"
end
